package com.caralarm.app

import android.os.Handler
import android.os.Looper
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.TextView

/** Manual output test and live sensor read-out (IO,ON / IO,LED / IO,BUZ / IO,LCD / IO,READ). */
class IoTestActivity : ToolActivity() {
    override val screenTitle = "IO test"

    private val handler = Handler(Looper.getMainLooper())
    private var ioOn = false
    private var reading = false
    private var ticks = 0
    private val on = BooleanArray(4)  // red, yellow, green, buzzer
    private val names = arrayOf("Red LED (D5)", "Yellow LED (D7)", "Green LED (D6)", "Buzzer (D8)")
    private val codes = arrayOf("R", "Y", "G", "")

    private lateinit var toggle: Button
    private lateinit var outputs: List<Button>
    private lateinit var lcd0: EditText
    private lateinit var lcd1: EditText
    private lateinit var lcdSend: Button
    private lateinit var backlightBtn: Button
    private var backlightOn = true
    private lateinit var btText: EditText
    private lateinit var btSend: Button
    private lateinit var allOff: Button
    private lateinit var readBtn: Button
    private lateinit var auto: CheckBox
    private lateinit var readout: TextView

    private val loop = object : Runnable {
        override fun run() {
            if (!ioOn) return
            ticks++
            if (auto.isChecked) readSensors()
            // Any IO command restarts the firmware 30 s timeout; repeat the red LED state as a keep-alive.
            else if (ticks % 10 == 0) CommandBus.send("IO,LED,R," + (if (on[0]) 1 else 0))
            handler.postDelayed(this, 1_000L)
        }
    }

    override fun buildUi() {
        note("Outputs are driven by hand; the automatic alarm is paused while this is active and resumes by itself after 30 s without commands or when you leave.")
        toggle = button("Start IO test") { if (ioOn) stopIo() else startIo() }
        add(toggle)

        val btns = ArrayList<Button>()
        for (i in 0..3) {
            val b = button(names[i] + ": OFF") {
                on[i] = !on[i]
                apply(i)
            }
            btns.add(b)
            add(b)
        }
        outputs = btns
        add(button("Beep 200 ms") {
            if (!ioOn) return@button
            on[3] = true
            apply(3)
            handler.postDelayed({
                on[3] = false
                apply(3)
            }, 200L)
        })

        label("LCD")
        lcd0 = field("Line 1", "Car Alarm Test", maxLen = 16)
        lcd1 = field("Line 2", "LCD OK", maxLen = 16)
        add(lcd0)
        add(lcd1)
        lcdSend = button("Send to LCD") {
            val text = (lcd0.text.toString() + "|" + lcd1.text.toString()).replace(',', ' ')
            cmd("IO,LCD,$text", "RSP,IO") { say("LCD updated") }
        }
        add(lcdSend)
        backlightBtn = button("LCD backlight: ON") {
            backlightOn = !backlightOn
            backlightBtn.text = "LCD backlight: " + (if (backlightOn) "ON" else "OFF")
            cmd("IO,BL," + (if (backlightOn) 1 else 0), "RSP,IO") { }
        }
        add(backlightBtn)

        label("Bluetooth TX")
        btText = field("Text", "BT_TEST_1", maxLen = 24)
        add(btText)
        btSend = button("Send over BLE") {
            val t = btText.text.toString().replace('\n', ' ')
            cmd("IO,BT,$t", "RSP,IO") { say("Sent") }
        }
        add(btSend)
        allOff = button("Turn all outputs off") {
            for (i in on.indices) on[i] = false
            for (i in on.indices) paint(i)
            cmd("IO,LED,R,0", "RSP,IO") {}
            cmd("IO,LED,Y,0", "RSP,IO") {}
            cmd("IO,LED,G,0", "RSP,IO") {}
            cmd("IO,BUZ,0", "RSP,IO") {}
        }
        add(allOff)

        label("Inputs")
        readBtn = button("Read sensors") { readSensors() }
        add(readBtn)
        auto = check("Read every second")
        add(auto)
        readout = mono("-")
        setControlsEnabled(false)
    }

    override fun onLinkChanged(connected: Boolean) {
        if (!connected) {
            ioOn = false
            setControlsEnabled(false)
        }
        toggle.isEnabled = connected
        toggle.text = if (ioOn) "Stop IO test" else "Start IO test"
    }

    private fun setControlsEnabled(enabled: Boolean) {
        for (b in outputs) b.isEnabled = enabled
        lcdSend.isEnabled = enabled
        backlightBtn.isEnabled = enabled
        btSend.isEnabled = enabled
        allOff.isEnabled = enabled
        readBtn.isEnabled = enabled
        auto.isEnabled = enabled
    }

    private fun startIo() {
        cmd("IO,ON", "RSP,IO,ON") {
            ioOn = true
            ticks = 0
            for (i in on.indices) on[i] = false
            for (i in on.indices) paint(i)
            setControlsEnabled(true)
            toggle.text = "Stop IO test"
            handler.postDelayed(loop, 1_000L)
        }
    }

    private fun stopIo() {
        CommandBus.send("IO,OFF")
        ioOn = false
        handler.removeCallbacks(loop)
        for (i in on.indices) on[i] = false
        for (i in on.indices) paint(i)
        setControlsEnabled(false)
        toggle.text = "Start IO test"
    }

    private fun paint(i: Int) {
        outputs[i].text = names[i] + (if (on[i]) ": ON" else ": OFF")
    }

    private fun apply(i: Int) {
        paint(i)
        val command = if (i < 3) "IO,LED,${codes[i]},${if (on[i]) 1 else 0}" else "IO,BUZ,${if (on[3]) 1 else 0}"
        cmd(command, "RSP,IO") { }
    }

    private fun readSensors() {
        if (reading) return
        reading = true
        CommandBus.request("IO,READ", "RSP,IO,READ", 4_000L) { reply, error ->
            reading = false
            if (isFinishing || isDestroyed) return@request
            val p = reply?.split(',')
            if (p == null || p.size < 9) {
                say("Read failed ($error)")
                return@request
            }
            val dist = p[3].toIntOrNull()
            readout.text = buildString {
                append("Raw distance : ").append(if (dist == null || dist < 0) "no echo" else "%.1f cm".format(dist / 10.0)).append('\n')
                append("Accel X/Y/Z  : ").append(p[4]).append(" / ").append(p[5]).append(" / ").append(p[6]).append(" mg\n")
                append("MPU6050      : ").append(if (p[7] == "1") "OK" else "NOT FOUND").append('\n')
                append("LCD          : ").append(if (p[8] == "1") "OK" else "NOT FOUND")
            }
        }
    }

    override fun onStop() {
        if (ioOn) stopIo()  // never leave the outputs under manual control
        super.onStop()
    }
}
