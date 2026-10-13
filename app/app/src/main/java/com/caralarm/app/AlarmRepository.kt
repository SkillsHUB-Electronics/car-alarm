package com.caralarm.app

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * In-memory app state shared by the service (writer) and the activity (reader).
 * All mutation and listener callbacks happen on the main thread.
 */
object AlarmRepository {
    enum class Conn { DISCONNECTED, CONNECTING, CONNECTED }
    enum class Level { NORMAL, WARNING, ALARM }

    // Shock/brake alarms stay visible this long, matching the firmware auto-clear (no reset button).
    private const val MOTION_HOLD_MS = 10_000L
    private const val MAX_LOG = 100

    private val main = Handler(Looper.getMainLooper())
    private val listeners = mutableListOf<() -> Unit>()
    private val logLines = ArrayDeque<String>()
    private val timeFormat = SimpleDateFormat("HH:mm:ss", Locale.getDefault())

    var conn = Conn.DISCONNECTED
        private set
    var serviceActive = false
        private set
    var deviceName: String? = null
        private set
    var distanceCm: Int? = null
        private set

    /** Latest edge-AI driving-state class and its confidence in percent (informational), or null. */
    var driveClass: Int? = null
        private set
    var drivePercent: Int = 0
        private set

    /** Latest accelerometer reading in mg (X, Y, Z) from DBG lines, or null; and the peak magnitude in g. */
    var accelMg: IntArray? = null
        private set
    var peakG: Double = 0.0
        private set

    private var distanceLevel = Level.NORMAL
    private var motionSource: MsgSource? = null
    private var motionUntil = 0L
    private val refreshRunnable = Runnable { notifyChanged() }

    /** Shock/brake source while its hold window is open, otherwise null. */
    val activeMotion: MsgSource?
        get() = if (SystemClock.elapsedRealtime() < motionUntil) motionSource else null

    val level: Level
        get() = if (activeMotion != null) Level.ALARM else distanceLevel

    fun logSnapshot(): List<String> = logLines.toList()

    fun addListener(l: () -> Unit) {
        listeners.add(l)
    }

    fun removeListener(l: () -> Unit) {
        listeners.remove(l)
    }

    fun setServiceActive(active: Boolean) = onMain {
        serviceActive = active
        if (!active) resetLive(Conn.DISCONNECTED)
        notifyChanged()
    }

    fun setDevice(name: String?) = onMain {
        deviceName = name
        notifyChanged()
    }

    fun setConn(newConn: Conn) = onMain {
        if (newConn != Conn.CONNECTED) {
            resetLive(newConn)
        } else {
            conn = newConn
        }
        notifyChanged()
    }

    fun addLog(text: String) = onMain {
        logLines.addFirst("${timeFormat.format(Date())}  $text")
        while (logLines.size > MAX_LOG) logLines.removeLast()
        notifyChanged()
    }

    fun onMessage(ctx: Context, msg: AlarmMessage) = onMain {
        when (msg.source) {
            MsgSource.DIST -> applyDistance(ctx, msg)
            MsgSource.SHOCK, MsgSource.BRAKE -> applyMotion(ctx, msg)
            MsgSource.BT -> if (msg.type == MsgType.TEST) addLogLine(ctx.getString(R.string.log_test))
        }
        notifyChanged()
    }

    fun onClass(id: Int, percent: Int) = onMain {
        driveClass = id
        drivePercent = percent
        notifyChanged()
    }

    fun onAccel(mg: IntArray) = onMain {
        accelMg = mg
        val g = Math.sqrt(mg.sumOf { it.toDouble() * it } ) / 1000.0
        if (g > peakG) peakG = g
        notifyChanged()
    }

    fun resetPeak() = onMain {
        peakG = 0.0
        notifyChanged()
    }

    private fun applyDistance(ctx: Context, msg: AlarmMessage) {
        val newLevel = when (msg.type) {
            MsgType.OK -> Level.NORMAL
            MsgType.WARN -> Level.WARNING
            MsgType.ALARM -> Level.ALARM
            MsgType.TEST -> return
        }
        // No value means the firmware has no valid reading: show "--", never the stale distance.
        distanceCm = msg.value?.toInt()
        if (newLevel != distanceLevel) {
            distanceLevel = newLevel
            val cm = distanceCm ?: 0
            addLogLine(
                when (newLevel) {
                    Level.NORMAL -> ctx.getString(R.string.log_dist_ok)
                    Level.WARNING -> ctx.getString(R.string.log_dist_warn, cm)
                    Level.ALARM -> ctx.getString(R.string.log_dist_alarm, cm)
                }
            )
        }
    }

    private fun applyMotion(ctx: Context, msg: AlarmMessage) {
        if (msg.type != MsgType.ALARM) return
        motionSource = msg.source
        motionUntil = SystemClock.elapsedRealtime() + MOTION_HOLD_MS
        main.removeCallbacks(refreshRunnable)
        main.postDelayed(refreshRunnable, MOTION_HOLD_MS + 50)

        val g = msg.value?.let { String.format(Locale.US, "%.1f", it) } ?: "?"
        addLogLine(
            if (msg.source == MsgSource.SHOCK) ctx.getString(R.string.log_shock, g)
            else ctx.getString(R.string.log_brake, g)
        )
    }

    private fun resetLive(newConn: Conn) {
        conn = newConn
        distanceCm = null
        driveClass = null
        accelMg = null
        distanceLevel = Level.NORMAL
        motionUntil = 0L
    }

    // Variant of addLog for callers already on the main thread (avoids a redundant notify).
    private fun addLogLine(text: String) {
        logLines.addFirst("${timeFormat.format(Date())}  $text")
        while (logLines.size > MAX_LOG) logLines.removeLast()
    }

    private fun notifyChanged() {
        listeners.toList().forEach { it() }
    }

    private inline fun onMain(crossinline block: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) block() else main.post { block() }
    }
}
