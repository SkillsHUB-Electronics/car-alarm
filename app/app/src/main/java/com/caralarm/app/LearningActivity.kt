package com.caralarm.app

import android.app.AlertDialog
import android.content.Intent
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.Spinner
import android.widget.TextView
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Records labelled IMU feature windows from the firmware (REC,START / REC,STOP) into a CSV file and exports it
 * for training in the web console (PLAN.md D15/D16). Recording keeps running while the screen is off.
 */
class LearningActivity : ToolActivity() {
    companion object {
        private const val REQ_EXPORT = 7
    }

    override val screenTitle = "Learning data"

    private lateinit var classSpinner: Spinner
    private lateinit var session: EditText
    private lateinit var recButton: Button
    private lateinit var live: TextView
    private lateinit var counts: TextView

    override fun buildUi() {
        note("Mount the unit in the car with the MPU6050 +Y axis facing the front. Pick a class, tap Record, perform the maneuver, then Stop. One window is saved per second. Training happens in the web console: export the CSV and import it there.")
        note("Safety: do not operate this screen while driving. Have a passenger do it, or start recording before you set off and stop it after you park.")

        label("Class")
        classSpinner = Spinner(this)
        classSpinner.adapter = ArrayAdapter(this, android.R.layout.simple_spinner_dropdown_item, Learn.CLASSES.mapIndexed { i, n -> "$i  $n" })
        add(classSpinner)

        label("Session name")
        session = field("session", "app-" + SimpleDateFormat("yyyyMMdd-HHmm", Locale.US).format(Date()))
        add(session)

        recButton = button("Record") { toggleRecording() }
        add(recButton)
        live = note("")

        label("Collected windows")
        counts = mono("")

        add(button("Export CSV…") {
            val intent = Intent(Intent.ACTION_CREATE_DOCUMENT)
                .addCategory(Intent.CATEGORY_OPENABLE)
                .setType("text/csv")
                .putExtra(Intent.EXTRA_TITLE, "learn-" + SimpleDateFormat("yyyyMMdd", Locale.US).format(Date()) + ".csv")
            startActivityForResult(intent, REQ_EXPORT)
        })
        add(button("Delete all collected data") {
            AlertDialog.Builder(this)
                .setMessage("Delete all learning data stored in the app?")
                .setPositiveButton("Delete") { _, _ -> LearningStore.clear(this) }
                .setNegativeButton("Cancel", null)
                .show()
        })
        refresh()
    }

    override fun onStart() {
        super.onStart()
        LearningStore.onChange = { refresh() }
        refresh()
    }

    override fun onStop() {
        LearningStore.onChange = null
        super.onStop()
    }

    override fun onLinkChanged(connected: Boolean) {
        recButton.isEnabled = connected || LearningStore.recording
    }

    private fun toggleRecording() {
        if (!LearningStore.recording) {
            val id = classSpinner.selectedItemPosition
            val name = session.text.toString()
            cmd("REC,START,$id", "RSP,REC,1") {
                LearningStore.start(this, id, name)
                say("Recording ${Learn.CLASSES[id]}")
            }
        } else {
            // Stop locally even if the link is gone, so the app never claims to record when it cannot.
            CommandBus.request("REC,STOP", "RSP,REC,0", 3_000L) { _, _ ->
                LearningStore.stop()
                say("Stopped")
            }
        }
    }

    private fun refresh() {
        recButton.text = if (LearningStore.recording) "Stop recording" else "Record"
        recButton.isEnabled = CommandBus.ready || LearningStore.recording
        live.text = if (LearningStore.recording) {
            "Recording ${Learn.CLASSES[LearningStore.label]}: ${LearningStore.sessionCount} windows this session"
        } else ""
        val c = LearningStore.counts(this)
        counts.text = Learn.CLASSES.indices.joinToString("\n") { String.format(Locale.US, "%2d %-16s %5d", it, Learn.CLASSES[it], c[it]) } +
            "\n" + String.format(Locale.US, "   %-16s %5d", "Total", c.sum())
    }

    @Deprecated("Deprecated in Java")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQ_EXPORT || resultCode != RESULT_OK) return
        val uri = data?.data ?: return
        try {
            val bytes = LearningStore.exportTo(this, uri)
            say("Exported $bytes bytes")
        } catch (e: Exception) {
            say("Export failed: ${e.message}")
        }
    }
}
