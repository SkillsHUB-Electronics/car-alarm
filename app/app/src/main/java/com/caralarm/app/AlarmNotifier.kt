package com.caralarm.app

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.SystemClock

/** Builds the foreground-service notification and the heads-up alarm notifications. */
class AlarmNotifier(private val ctx: Context) {
    companion object {
        const val ID_ONGOING = 1
        private const val ID_ALARM_BASE = 100
        private const val CH_STATUS = "status"
        private const val CH_ALARM = "alarm_silent"
        private const val ALERT_COOLDOWN_MS = 5_000L
    }

    private val nm = ctx.getSystemService(NotificationManager::class.java)
    private val lastAlert = HashMap<MsgSource, Long>()
    private val sound = AlarmSound(ctx)

    init {
        nm.createNotificationChannel(
            NotificationChannel(CH_STATUS, ctx.getString(R.string.channel_status), NotificationManager.IMPORTANCE_LOW)
        )
        val alarmChannel = NotificationChannel(
            CH_ALARM, ctx.getString(R.string.channel_alarm), NotificationManager.IMPORTANCE_HIGH
        ).apply {
            enableVibration(true)
            vibrationPattern = longArrayOf(0, 400, 200, 400, 200, 800)
            setSound(null, null) // tone is played by AlarmSound (chosen in Settings)
        }
        nm.deleteNotificationChannel("alarm") // old channel with a fixed system sound
        nm.createNotificationChannel(alarmChannel)
    }

    fun ongoing(text: String): Notification =
        Notification.Builder(ctx, CH_STATUS)
            .setSmallIcon(R.drawable.ic_stat_alarm)
            .setContentTitle(ctx.getString(R.string.app_name))
            .setContentText(text)
            .setOngoing(true)
            .setContentIntent(openApp())
            .build()

    fun updateOngoing(text: String) = nm.notify(ID_ONGOING, ongoing(text))

    /** Posts a heads-up alarm for ALARM messages only, at most once per cooldown per source. */
    fun alarm(msg: AlarmMessage) {
        if (msg.type != MsgType.ALARM || msg.source !in setOf(MsgSource.DIST, MsgSource.SHOCK, MsgSource.BRAKE)) return

        val now = SystemClock.elapsedRealtime()
        val last = lastAlert[msg.source]
        if (last != null && now - last < ALERT_COOLDOWN_MS) return
        lastAlert[msg.source] = now

        val text = when (msg.source) {
            MsgSource.DIST -> ctx.getString(R.string.notif_alarm_dist, msg.value?.toInt() ?: 0)
            MsgSource.SHOCK -> ctx.getString(R.string.notif_alarm_shock)
            else -> ctx.getString(R.string.notif_alarm_brake)
        }
        val notification = Notification.Builder(ctx, CH_ALARM)
            .setSmallIcon(R.drawable.ic_stat_alarm)
            .setContentTitle(ctx.getString(R.string.notif_alarm_title))
            .setContentText(text)
            .setCategory(Notification.CATEGORY_ALARM)
            .setAutoCancel(true)
            .setContentIntent(openApp())
            .build()
        nm.notify(ID_ALARM_BASE + msg.source.ordinal, notification)
        sound.play()
    }

    private fun openApp(): PendingIntent =
        PendingIntent.getActivity(
            ctx, 0, Intent(ctx, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
}
