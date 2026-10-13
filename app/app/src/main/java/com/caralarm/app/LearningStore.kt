package com.caralarm.app

import android.content.Context
import android.net.Uri

/**
 * Learning windows recorded from the firmware, appended to `learn.csv` in the app's private storage as they
 * arrive (so recording continues while the screen is off). Export writes the file to a user-chosen location;
 * the web console imports it for training.
 */
object LearningStore {
    private const val FILE = "learn.csv"

    var recording = false
        private set
    var label = 0
        private set
    var session = ""
        private set
    var sessionCount = 0
        private set

    /** Called on the main thread whenever counts or the recording state change. */
    var onChange: (() -> Unit)? = null

    private var counts: IntArray? = null

    fun start(ctx: Context, label: Int, session: String) {
        ensureFile(ctx)
        this.label = label
        this.session = session
        sessionCount = 0
        recording = true
        onChange?.invoke()
    }

    fun stop() {
        recording = false
        onChange?.invoke()
    }

    fun onFeat(ctx: Context, row: FeatRow) {
        if (!recording || row.label != label) return
        ensureFile(ctx)
        ctx.openFileOutput(FILE, Context.MODE_APPEND).use {
            it.write((Learn.csvLine(session, row.label, row.f) + "\n").toByteArray(Charsets.UTF_8))
        }
        val c = loadCounts(ctx)
        c[row.label]++
        sessionCount++
        onChange?.invoke()
    }

    fun counts(ctx: Context): IntArray = loadCounts(ctx).copyOf()

    fun clear(ctx: Context) {
        ctx.deleteFile(FILE)
        counts = IntArray(Learn.CLASSES.size)
        ensureFile(ctx)
        onChange?.invoke()
    }

    /** Copies the CSV to [uri] (from the system file picker). Returns the number of bytes written. */
    fun exportTo(ctx: Context, uri: Uri): Long {
        ensureFile(ctx)
        var total = 0L
        ctx.contentResolver.openOutputStream(uri)?.use { out ->
            ctx.openFileInput(FILE).use { input -> total = input.copyTo(out) }
        }
        return total
    }

    private fun ensureFile(ctx: Context) {
        if (ctx.getFileStreamPath(FILE).exists()) return
        ctx.openFileOutput(FILE, Context.MODE_PRIVATE).use {
            it.write((Learn.CSV_COMMENT + "\n" + Learn.CSV_HEADER + "\n").toByteArray(Charsets.UTF_8))
        }
    }

    private fun loadCounts(ctx: Context): IntArray {
        counts?.let { return it }
        val c = IntArray(Learn.CLASSES.size)
        ensureFile(ctx)
        ctx.openFileInput(FILE).bufferedReader().useLines { lines ->
            for (line in lines) {
                if (line.isEmpty() || line[0] == '#' || line.startsWith("session,")) continue
                val label = line.split(',').getOrNull(1)?.toIntOrNull() ?: continue
                if (label in c.indices) c[label]++
            }
        }
        counts = c
        return c
    }
}
