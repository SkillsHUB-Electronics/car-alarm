package com.caralarm.app

import android.annotation.SuppressLint
import android.app.Service
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import java.util.UUID
import kotlin.math.min

/**
 * Foreground service that keeps a BLE GATT connection to the alarm module alive. The module
 * exposes a transparent UART characteristic (default FFE0/FFE1, see [findUartCharacteristic]);
 * notifications are reassembled into lines and forwarded to [AlarmRepository].
 * Reconnects with backoff when the link drops.
 */
@SuppressLint("MissingPermission") // Permissions are requested in MainActivity before the service starts.
class BluetoothService : Service() {
    companion object {
        private const val ACTION_STOP = "com.caralarm.app.STOP"
        private const val EXTRA_ADDRESS = "address"
        private const val EXTRA_NAME = "name"

        // Common transparent-UART layout of HM-10 style BLE modules. Replace after reading the real
        // UUIDs with nRF Connect if the module uses something else (the fallback below usually copes).
        private val UART_SERVICE: UUID = UUID.fromString("0000ffe0-0000-1000-8000-00805f9b34fb")
        private val UART_CHAR: UUID = UUID.fromString("0000ffe1-0000-1000-8000-00805f9b34fb")
        private val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private val GENERIC_ACCESS: UUID = UUID.fromString("00001800-0000-1000-8000-00805f9b34fb")
        private val GENERIC_ATTRIBUTE: UUID = UUID.fromString("00001801-0000-1000-8000-00805f9b34fb")

        fun start(ctx: Context, address: String, name: String) {
            val intent = Intent(ctx, BluetoothService::class.java)
                .putExtra(EXTRA_ADDRESS, address)
                .putExtra(EXTRA_NAME, name)
            ctx.startForegroundService(intent)
        }

        fun stop(ctx: Context) {
            // Plain startService is fine here: the service is already running in the foreground.
            ctx.startService(Intent(ctx, BluetoothService::class.java).setAction(ACTION_STOP))
        }

        /** Preferred FFE0/FFE1, else the first notifiable characteristic outside the generic GATT services. */
        fun findUartCharacteristic(gatt: BluetoothGatt): BluetoothGattCharacteristic? {
            gatt.getService(UART_SERVICE)?.getCharacteristic(UART_CHAR)?.let { return it }
            for (service in gatt.services) {
                if (service.uuid == GENERIC_ACCESS || service.uuid == GENERIC_ATTRIBUTE) continue
                for (c in service.characteristics) {
                    val p = c.properties
                    if (p and (BluetoothGattCharacteristic.PROPERTY_NOTIFY or BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0) return c
                }
            }
            return null
        }
    }

    // All state below is touched on the main thread only (GATT callbacks hop over via mainHandler).
    private val mainHandler = Handler(Looper.getMainLooper())
    private lateinit var notifier: AlarmNotifier
    private val lineBuffer = LineBuffer()
    private var deviceName = ""
    private var address = ""

    private var running = false
    // Bumped on every (re)start so callbacks from a previous connection are ignored.
    private var generation = 0
    private var gatt: BluetoothGatt? = null
    private var backoffMs = 1_000L
    private var wasConnected = false

    // Outgoing command bytes (PLAN.md D12): queued in 20-byte pieces and written one at a time.
    private var uartChar: BluetoothGattCharacteristic? = null
    private val writeQueue = ArrayDeque<ByteArray>()
    private var writing = false
    private val writeWatchdog = Runnable {
        writing = false
        writeQueue.clear()
    }

    // The repository notifies on every distance message (~5 Hz); only touch the notification when its text changes,
    // otherwise Android rate-limits it and the updates waste battery.
    private var lastStatus = ""
    private val repoListener: () -> Unit = {
        val text = statusText()
        if (text != lastStatus) {
            lastStatus = text
            notifier.updateOngoing(text)
        }
    }

    override fun onCreate() {
        super.onCreate()
        notifier = AlarmNotifier(this)
        AlarmRepository.addListener(repoListener)
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopWork()
            AlarmRepository.addLog(getString(R.string.log_stopped))
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf()
            return START_NOT_STICKY
        }

        val addr = intent?.getStringExtra(EXTRA_ADDRESS)
        if (addr == null) {
            stopSelf()
            return START_NOT_STICKY
        }
        address = addr
        deviceName = intent.getStringExtra(EXTRA_NAME) ?: addr

        val notification = notifier.ongoing(getString(R.string.notif_connecting, deviceName))
        if (Build.VERSION.SDK_INT >= 29) {
            startForeground(AlarmNotifier.ID_ONGOING, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        } else {
            startForeground(AlarmNotifier.ID_ONGOING, notification)
        }

        AlarmRepository.setDevice(deviceName)
        AlarmRepository.setServiceActive(true)

        stopWork()
        running = true
        backoffMs = 1_000L
        wasConnected = false
        connectGatt(++generation)
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        stopWork()
        AlarmRepository.removeListener(repoListener)
        AlarmRepository.setServiceActive(false)
        super.onDestroy()
    }

    private fun stopWork() {
        running = false
        generation++
        mainHandler.removeCallbacksAndMessages(null)
        closeGatt()
        lineBuffer.clear()
        lastStatus = ""
    }

    private fun closeGatt() {
        uartChar = null
        writeQueue.clear()
        writing = false
        CommandBus.reset()
        val g = gatt
        gatt = null
        if (g != null) {
            try {
                g.disconnect()
            } catch (_: Exception) {
            }
            try {
                g.close()
            } catch (_: Exception) {
            }
        }
    }

    private fun statusText(): String =
        if (AlarmRepository.conn == AlarmRepository.Conn.CONNECTED)
            getString(R.string.notif_connected, deviceName)
        else getString(R.string.notif_connecting, deviceName)

    private fun isCurrent(gen: Int) = running && gen == generation

    private fun connectGatt(gen: Int) {
        if (!isCurrent(gen)) return
        AlarmRepository.setConn(AlarmRepository.Conn.CONNECTING)
        lineBuffer.clear()
        try {
            val adapter = getSystemService(BluetoothManager::class.java).adapter
            val device = adapter.getRemoteDevice(address)
            closeGatt()
            gatt = device.connectGatt(this, false, GattCallback(gen), BluetoothDevice.TRANSPORT_LE)
        } catch (e: SecurityException) {
            AlarmRepository.addLog(getString(R.string.log_no_permission))
            stopSelf()
        } catch (e: IllegalArgumentException) {
            // Invalid MAC address: nothing to retry.
            stopSelf()
        }
    }

    /** Link lost or failed: release the GATT object and retry after the backoff delay. */
    private fun onLinkDown(gen: Int) {
        if (!isCurrent(gen)) return
        closeGatt()
        if (wasConnected) AlarmRepository.addLog(getString(R.string.log_disconnected))
        wasConnected = false
        AlarmRepository.setConn(AlarmRepository.Conn.CONNECTING)
        mainHandler.postDelayed({ connectGatt(gen) }, backoffMs)
        backoffMs = min(backoffMs * 2, 10_000L)
    }

    private fun enqueueWrite(text: String): Boolean {
        if (gatt == null || uartChar == null) return false
        for (c in Chunker.chunks(text)) writeQueue.addLast(c)
        pumpWrites()
        return true
    }

    @Suppress("DEPRECATION")
    private fun pumpWrites() {
        if (writing) return
        val g = gatt ?: return
        val ch = uartChar ?: return
        val chunk = writeQueue.removeFirstOrNull() ?: return
        val withResponse = ch.properties and BluetoothGattCharacteristic.PROPERTY_WRITE != 0
        ch.writeType = if (withResponse) BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        else BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        ch.value = chunk
        writing = true
        if (!g.writeCharacteristic(ch)) {
            writing = false
            writeQueue.clear()
            return
        }
        if (withResponse) {
            mainHandler.postDelayed(writeWatchdog, 2_000L)  // cleared by onCharacteristicWrite
        } else {
            // No callback for write-without-response: pace the pieces so the module UART keeps up.
            mainHandler.postDelayed({
                writing = false
                pumpWrites()
            }, 30L)
        }
    }

    private fun onLinkReady(gen: Int, ch: BluetoothGattCharacteristic) {
        if (!isCurrent(gen)) return
        uartChar = ch
        CommandBus.writer = { text -> enqueueWrite(text) }
        val characteristicId = ch.uuid.toString()
        wasConnected = true
        backoffMs = 1_000L
        AlarmRepository.setConn(AlarmRepository.Conn.CONNECTED)
        AlarmRepository.addLog(getString(R.string.log_connected, deviceName))
        AlarmRepository.addLog(getString(R.string.log_uart, characteristicId))
    }

    private fun onBytes(gen: Int, bytes: ByteArray) {
        if (!isCurrent(gen)) return
        lineBuffer.feed(bytes) { line ->
            CommandBus.onLine(line)  // command replies and any screen listening
            DeviceLines.parseFeat(line)?.let {
                LearningStore.onFeat(applicationContext, it)
                return@feed
            }
            DeviceLines.parseDbg(line)?.let {
                AlarmRepository.onAccel(it)
                return@feed
            }
            DeviceLines.parseCls(line)?.let {
                AlarmRepository.onClass(it.first, it.second)
                return@feed
            }
            val msg = Protocol.parse(line) ?: return@feed
            AlarmRepository.onMessage(applicationContext, msg)
            notifier.alarm(msg)
        }
    }

    // Value get/set on descriptors and characteristics is deprecated on API 33 but is the only form on older APIs.
    @Suppress("DEPRECATION")
    private inner class GattCallback(private val gen: Int) : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            mainHandler.post {
                if (!isCurrent(gen) || g !== gatt) {
                    // Stale connection from before a restart: just release it.
                    if (g !== gatt) try { g.close() } catch (_: Exception) {}
                    return@post
                }
                if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
                    if (!g.discoverServices()) onLinkDown(gen)
                } else {
                    onLinkDown(gen)
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            mainHandler.post {
                if (!isCurrent(gen) || g !== gatt) return@post
                val ch = if (status == BluetoothGatt.GATT_SUCCESS) findUartCharacteristic(g) else null
                if (ch == null) {
                    AlarmRepository.addLog(getString(R.string.log_no_uart))
                    onLinkDown(gen)
                    return@post
                }
                g.setCharacteristicNotification(ch, true)
                val cccd = ch.getDescriptor(CCCD)
                if (cccd == null) {
                    onLinkReady(gen, ch)
                    return@post
                }
                val indicate = ch.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY == 0
                cccd.value = if (indicate) BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
                else BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                if (!g.writeDescriptor(cccd)) onLinkDown(gen)
            }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            mainHandler.post {
                if (!isCurrent(gen) || g !== gatt) return@post
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    onLinkReady(gen, descriptor.characteristic)
                } else {
                    onLinkDown(gen)
                }
            }
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            mainHandler.post {
                if (!isCurrent(gen) || g !== gatt) return@post
                mainHandler.removeCallbacks(writeWatchdog)
                writing = false
                if (status == BluetoothGatt.GATT_SUCCESS) pumpWrites() else writeQueue.clear()
            }
        }

        // The two-argument form is the one that exists on every API level; on Android 13+ the framework's
        // default three-argument callback forwards to it, so overriding only this one avoids duplicates.
        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            val bytes = characteristic.value ?: return
            val copy = bytes.copyOf()
            mainHandler.post { if (g === gatt) onBytes(gen, copy) }
        }
    }
}
