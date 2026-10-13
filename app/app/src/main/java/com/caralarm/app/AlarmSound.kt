package com.caralarm.app

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.media.Ringtone
import android.media.RingtoneManager
import android.net.Uri
import android.os.Build
import android.os.Handler
import android.os.Looper
import kotlin.math.PI
import kotlin.math.sin

/**
 * Plays the alarm tone on the alarm audio stream: either a generated siren (default) or a tone picked from the
 * phone. The choice is stored in SharedPreferences and edited in the Settings screen.
 */
class AlarmSound(private val ctx: Context) {
    companion object {
        private const val PREFS = "alarm_sound"
        private const val KEY_URI = "uri"
        private const val PLAY_MS = 8_000L
        private const val RATE = 22_050

        /** Empty string = built-in siren. */
        fun savedUri(ctx: Context): String =
            ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getString(KEY_URI, "") ?: ""

        fun save(ctx: Context, uri: String) {
            ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putString(KEY_URI, uri).apply()
        }

        fun title(ctx: Context): String {
            val s = savedUri(ctx)
            if (s.isEmpty()) return "Built-in siren"
            return try {
                RingtoneManager.getRingtone(ctx, Uri.parse(s))?.getTitle(ctx) ?: "Phone tone"
            } catch (e: Exception) {
                "Phone tone"
            }
        }

        /** Two-tone siren (1000/1400 Hz, 0.25 s each, three cycles, then a short gap). 16-bit mono PCM. */
        fun sirenPcm(): ShortArray {
            val out = ArrayList<Short>()
            fun tone(freq: Double, ms: Int) {
                val n = RATE * ms / 1000
                for (i in 0 until n) {
                    val env = minOf(1.0, minOf(i, n - 1 - i) / (RATE * 0.01)) // 10 ms fade avoids clicks
                    out.add((sin(2 * PI * freq * i / RATE) * env * 28_000).toInt().toShort())
                }
            }
            repeat(3) { tone(1_000.0, 250); tone(1_400.0, 250) }
            repeat(RATE * 300 / 1000) { out.add(0) }
            return out.toShortArray()
        }
    }

    private val handler = Handler(Looper.getMainLooper())
    private var track: AudioTrack? = null
    private var ringtone: Ringtone? = null
    private val stopper = Runnable { stop() }

    private val attrs = AudioAttributes.Builder()
        .setUsage(AudioAttributes.USAGE_ALARM)
        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
        .build()

    fun play() {
        stop()
        try {
            val s = savedUri(ctx)
            if (s.isEmpty()) playSiren() else playRingtone(Uri.parse(s))
        } catch (e: Exception) {
            stop()
            try { playSiren() } catch (_: Exception) {}
        }
        handler.postDelayed(stopper, PLAY_MS)
    }

    private fun playSiren() {
        val pcm = sirenPcm()
        val t = AudioTrack.Builder()
            .setAudioAttributes(attrs)
            .setAudioFormat(
                AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(RATE).setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build()
            )
            .setTransferMode(AudioTrack.MODE_STATIC)
            .setBufferSizeInBytes(pcm.size * 2)
            .build()
        t.write(pcm, 0, pcm.size)
        t.setLoopPoints(0, pcm.size, -1)
        t.play()
        track = t
    }

    private fun playRingtone(uri: Uri) {
        val r = RingtoneManager.getRingtone(ctx, uri) ?: throw IllegalStateException("no ringtone")
        r.audioAttributes = attrs
        if (Build.VERSION.SDK_INT >= 28) r.isLooping = true
        r.play()
        ringtone = r
    }

    fun stop() {
        handler.removeCallbacks(stopper)
        track?.let { try { it.stop() } catch (_: Exception) {}; it.release() }
        track = null
        ringtone?.let { try { it.stop() } catch (_: Exception) {} }
        ringtone = null
    }
}
