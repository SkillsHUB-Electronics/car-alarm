package com.caralarm.app

import android.app.Activity
import android.os.Bundle
import android.text.InputFilter
import android.text.InputType
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

/**
 * Base for the tool screens (IO test, calibration, learning, settings). Views are built in code with plain
 * platform widgets to keep the APK dependency-free. Commands go through [CommandBus] over the BLE link that
 * the main screen started.
 */
abstract class ToolActivity : Activity() {
    protected lateinit var root: LinearLayout
    private lateinit var connText: TextView
    private lateinit var messageText: TextView
    private val repoListener: () -> Unit = { updateConn() }

    protected abstract val screenTitle: String
    protected abstract fun buildUi()
    protected open fun onLinkChanged(connected: Boolean) {}

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        title = screenTitle
        val scroll = ScrollView(this)
        root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(16), dp(16), dp(24))
        }
        scroll.addView(root)
        setContentView(scroll)

        label(screenTitle).apply { textSize = 22f }
        connText = note("")
        messageText = note("")
        buildUi()
    }

    override fun onStart() {
        super.onStart()
        AlarmRepository.addListener(repoListener)
        updateConn()
    }

    override fun onStop() {
        AlarmRepository.removeListener(repoListener)
        super.onStop()
    }

    private var lastReady: Boolean? = null

    private fun updateConn() {
        val ready = CommandBus.ready
        connText.text = if (ready) "Connected to ${AlarmRepository.deviceName ?: "device"}"
        else "Not connected. Select the device and connect on the main screen first."
        if (lastReady != ready) {
            lastReady = ready
            onLinkChanged(ready)
        }
    }

    // region view helpers

    protected fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()

    protected fun label(text: String): TextView = TextView(this).also {
        it.text = text
        it.textSize = 16f
        it.setPadding(0, dp(12), 0, dp(4))
        root.addView(it)
    }

    protected fun note(text: String): TextView = TextView(this).also {
        it.text = text
        it.textSize = 13f
        it.setPadding(0, dp(2), 0, dp(2))
        root.addView(it)
    }

    protected fun mono(text: String): TextView = TextView(this).also {
        it.text = text
        it.typeface = android.graphics.Typeface.MONOSPACE
        it.textSize = 12f
        root.addView(it)
    }

    protected fun button(text: String, onClick: () -> Unit): Button = Button(this).also {
        it.text = text
        it.setOnClickListener { onClick() }
    }

    protected fun check(text: String, onChange: (Boolean) -> Unit = {}): CheckBox = CheckBox(this).also {
        it.text = text
        it.setOnCheckedChangeListener { _, checked -> onChange(checked) }
    }

    protected fun field(hint: String, initial: String = "", numeric: Boolean = false, maxLen: Int = 0): EditText =
        EditText(this).also {
            it.hint = hint
            it.setText(initial)
            it.setSingleLine(true)
            if (numeric) it.inputType = InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL or InputType.TYPE_NUMBER_FLAG_SIGNED
            if (maxLen > 0) it.filters = arrayOf(InputFilter.LengthFilter(maxLen))
        }

    /** Adds [views] side by side with equal width. */
    protected fun row(vararg views: View): LinearLayout = LinearLayout(this).also { r ->
        r.orientation = LinearLayout.HORIZONTAL
        for (v in views) r.addView(v, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        root.addView(r)
    }

    protected fun add(v: View) {
        root.addView(v)
    }

    protected fun say(text: String) {
        messageText.text = text
    }

    // endregion

    /** Sends [command], waits for a reply starting with [prefix], and calls [onOk] with it (or shows the error). */
    protected fun cmd(command: String, prefix: String, timeoutMs: Long = 3_000L, onOk: (String) -> Unit) {
        CommandBus.request(command, prefix, timeoutMs) { reply, error ->
            if (isFinishing || isDestroyed) return@request
            if (reply != null) onOk(reply) else say("Failed: ${command.substringBefore(',')} ($error)")
        }
    }
}
