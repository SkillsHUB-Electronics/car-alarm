package com.caralarm.app

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.app.AlertDialog
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.ListView
import android.widget.TextView
import android.widget.Toast
import com.caralarm.app.AlarmRepository.Conn
import com.caralarm.app.AlarmRepository.Level

class MainActivity : Activity() {
    companion object {
        private const val REQ_PERMISSIONS = 1
        private const val PREFS = "caralarm"
        private const val KEY_ADDRESS = "address"
        private const val KEY_NAME = "name"
    }

    private lateinit var statusCard: android.view.View
    private lateinit var statusText: TextView
    private lateinit var distanceText: TextView
    private lateinit var deviceText: TextView
    private lateinit var connectButton: Button
    private lateinit var classText: TextView
    private lateinit var imuText: TextView
    private lateinit var aiBar: android.widget.ProgressBar
    private var dbgOn = false
    private lateinit var logAdapter: ArrayAdapter<String>

    private var pendingAfterPermission: (() -> Unit)? = null
    private val repoListener: () -> Unit = { render() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusCard = findViewById(R.id.statusCard)
        statusText = findViewById(R.id.statusText)
        distanceText = findViewById(R.id.distanceText)
        deviceText = findViewById(R.id.deviceText)
        connectButton = findViewById(R.id.connectButton)
        logAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, mutableListOf<String>())
        findViewById<ListView>(R.id.logList).adapter = logAdapter

        findViewById<Button>(R.id.pickButton).setOnClickListener { ensurePermissions { pickDevice() } }
        connectButton.setOnClickListener { onConnectClicked() }
        classText = findViewById(R.id.classText)
        imuText = findViewById(R.id.imuText)
        aiBar = findViewById(R.id.aiBar)
        imuText.setOnClickListener { AlarmRepository.resetPeak() }
        findViewById<Button>(R.id.ioButton).setOnClickListener { startActivity(Intent(this, IoTestActivity::class.java)) }
        findViewById<Button>(R.id.calButton).setOnClickListener { startActivity(Intent(this, CalibrationActivity::class.java)) }
        findViewById<Button>(R.id.learnButton).setOnClickListener { startActivity(Intent(this, LearningActivity::class.java)) }
        findViewById<Button>(R.id.settingsButton).setOnClickListener { startActivity(Intent(this, SettingsActivity::class.java)) }

        // Restore the previously chosen device so the label is correct before the service starts.
        if (AlarmRepository.deviceName == null) {
            AlarmRepository.setDevice(prefs().getString(KEY_NAME, null))
        }
    }

    override fun onStart() {
        super.onStart()
        AlarmRepository.addListener(repoListener)
        render()
    }

    override fun onStop() {
        AlarmRepository.removeListener(repoListener)
        // Live IMU lines are only wanted on the dashboard; keep the BLE link free for the tool screens.
        if (dbgOn) CommandBus.send("DBG,0")
        dbgOn = false
        super.onStop()
    }

    private fun prefs() = getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    // region Permissions

    private fun requiredPermissions(): List<String> = buildList {
        if (Build.VERSION.SDK_INT >= 31) {
            add(Manifest.permission.BLUETOOTH_SCAN)
            add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            add(Manifest.permission.ACCESS_FINE_LOCATION)  // needed for BLE scan results on Android 6-11
        }
        if (Build.VERSION.SDK_INT >= 33) add(Manifest.permission.POST_NOTIFICATIONS)
    }

    private fun hasBluetoothPermission(): Boolean =
        requiredPermissions().filter { it != Manifest.permission.POST_NOTIFICATIONS }
            .all { checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }

    private fun ensurePermissions(then: () -> Unit) {
        val missing = requiredPermissions().filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isEmpty()) {
            then()
        } else {
            pendingAfterPermission = then
            requestPermissions(missing.toTypedArray(), REQ_PERMISSIONS)
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode != REQ_PERMISSIONS) return
        val action = pendingAfterPermission
        pendingAfterPermission = null
        // Notification permission may be denied (alarms then only show in-app); Bluetooth is mandatory.
        if (hasBluetoothPermission()) action?.invoke()
        else Toast.makeText(this, R.string.toast_no_permission, Toast.LENGTH_LONG).show()
    }

    // endregion

    // region Actions

    private class Found(val address: String, var name: String, var rssi: Int)

    private fun locationSwitchOn(): Boolean {
        // Android 6-11 return no BLE scan results while the system location switch is off.
        if (Build.VERSION.SDK_INT >= 31) return true
        val lm = getSystemService(LocationManager::class.java)
        return lm.isProviderEnabled(LocationManager.GPS_PROVIDER) ||
            lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
    }

    /** Scans for BLE devices and lets the user pick one (devices without a name are hidden). */
    @SuppressLint("MissingPermission")
    private fun pickDevice() {
        val adapter = getSystemService(BluetoothManager::class.java).adapter
        if (adapter == null || !adapter.isEnabled) {
            Toast.makeText(this, R.string.toast_bt_off, Toast.LENGTH_LONG).show()
            return
        }
        if (!locationSwitchOn()) {
            Toast.makeText(this, R.string.toast_location_off, Toast.LENGTH_LONG).show()
            return
        }
        val scanner = adapter.bluetoothLeScanner
        if (scanner == null) {
            Toast.makeText(this, R.string.toast_bt_off, Toast.LENGTH_LONG).show()
            return
        }

        val found = LinkedHashMap<String, Found>()
        val shown = mutableListOf<Found>()
        val labels = mutableListOf<String>()
        val listAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, labels)
        val handler = Handler(Looper.getMainLooper())

        fun refresh() {
            shown.clear()
            shown.addAll(found.values.sortedByDescending { it.rssi })
            labels.clear()
            labels.addAll(shown.map { "${it.name}\n${it.address}  (${it.rssi} dBm)" })
            listAdapter.notifyDataSetChanged()
        }

        val callback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val name = result.scanRecord?.deviceName ?: result.device.name ?: return
                val addr = result.device.address
                val entry = found.getOrPut(addr) { Found(addr, name, result.rssi) }
                entry.name = name
                entry.rssi = result.rssi
                refresh()
            }
        }

        val dialog = AlertDialog.Builder(this)
            .setTitle(R.string.dialog_scan_title)
            .setAdapter(listAdapter) { _, which ->
                val d = shown[which]
                prefs().edit().putString(KEY_ADDRESS, d.address).putString(KEY_NAME, d.name).apply()
                AlarmRepository.setDevice(d.name)
            }
            .setNegativeButton(R.string.dialog_cancel, null)
            .create()
        dialog.setOnDismissListener {
            handler.removeCallbacksAndMessages(null)
            try {
                scanner.stopScan(callback)
            } catch (_: Exception) {
            }
        }
        dialog.show()
        scanner.startScan(callback)
        // Do not scan forever (battery); the user can reopen the dialog to rescan.
        handler.postDelayed({
            try {
                scanner.stopScan(callback)
            } catch (_: Exception) {
            }
        }, 30_000L)
    }

    private fun onConnectClicked() {
        if (AlarmRepository.serviceActive) {
            BluetoothService.stop(this)
            return
        }
        val address = prefs().getString(KEY_ADDRESS, null)
        if (address == null) {
            Toast.makeText(this, R.string.toast_pick_first, Toast.LENGTH_LONG).show()
            return
        }
        ensurePermissions {
            BluetoothService.start(this, address, prefs().getString(KEY_NAME, address) ?: address)
        }
    }

    // endregion

    private fun render() {
        val repo = AlarmRepository
        val connected = repo.conn == Conn.CONNECTED

        val (textRes, color) = when {
            !repo.serviceActive -> R.string.state_disconnected to R.color.status_idle
            !connected -> R.string.state_connecting to R.color.status_idle
            repo.activeMotion == MsgSource.SHOCK -> R.string.state_shock to R.color.status_alarm
            repo.activeMotion == MsgSource.BRAKE -> R.string.state_brake to R.color.status_alarm
            repo.level == Level.ALARM -> R.string.state_danger to R.color.status_alarm
            repo.level == Level.WARNING -> R.string.state_warning to R.color.status_warning
            else -> R.string.state_ok to R.color.status_normal
        }
        statusText.setText(textRes)
        statusCard.setBackgroundColor(getColor(color))
        // Dark text on the amber card for contrast; white elsewhere.
        val fg = if (color == R.color.status_warning) Color.BLACK else Color.WHITE
        statusText.setTextColor(fg)
        distanceText.setTextColor(fg)

        val cm = repo.distanceCm
        distanceText.text = if (connected && cm != null) getString(R.string.distance_fmt, cm)
        else getString(R.string.distance_none)

        val cls = repo.driveClass
        classText.text = if (connected && cls != null) getString(R.string.drive_fmt, Learn.CLASSES[cls], repo.drivePercent) else ""

        aiBar.progress = if (connected && cls != null) repo.drivePercent else 0
        val mg = repo.accelMg
        if (connected && mg != null) {
            val g = DoubleArray(3) { mg[it] / 1000.0 }
            val mag = Math.sqrt(g.sumOf { it * it })
            val dev = Math.abs(mag - 1.0)
            val state = getString(if (dev < 0.15) R.string.imu_still else if (dev < 0.6) R.string.imu_moving else R.string.imu_shock)
            imuText.text = getString(R.string.imu_fmt, g[0], g[1], g[2], mag, state, repo.peakG)
        } else {
            imuText.setText(R.string.imu_none)
        }
        // Ask the firmware for live IMU values (DBG lines, firmware 2.1) once per connection.
        if (connected && !dbgOn) dbgOn = CommandBus.send("DBG,1") else if (!connected) dbgOn = false

        deviceText.text = repo.deviceName?.let { getString(R.string.device_fmt, it) }
            ?: getString(R.string.device_none)
        connectButton.setText(if (repo.serviceActive) R.string.btn_disconnect else R.string.btn_connect)

        logAdapter.clear()
        logAdapter.addAll(repo.logSnapshot())
    }
}
