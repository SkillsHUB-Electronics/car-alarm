package com.caralarm.app

import android.content.Context
import android.widget.CheckBox
import android.widget.EditText
import android.widget.TextView
import java.util.Locale

/**
 * Distance sensor calibration: the user enters the true distance, the device returns its raw mean reading,
 * and a linear correction (corrected = gain * raw + offset) is fitted and stored in the firmware EEPROM.
 */
class CalibrationActivity : ToolActivity() {
    override val screenTitle = "Distance calibration"

    private val points = ArrayList<Pair<Double, Double>>()  // (true cm, raw cm)
    private lateinit var trueField: EditText
    private lateinit var regression: CheckBox
    private lateinit var table: TextView
    private lateinit var stored: TextView

    override fun buildUi() {
        loadPoints()
        note("Place a flat obstacle at a known distance (measured from the sensor face), enter it, then tap Measure. The device averages 10 raw readings. Take at least 4 spread points, e.g. 20, 50, 100, 200 cm.")
        trueField = field("True distance (cm)", "50", numeric = true)
        add(trueField)
        add(button("Measure") { measure() })

        label("Points")
        table = mono("")
        add(button("Clear points") {
            points.clear()
            savePoints()
            render()
        })

        label("Fit")
        regression = check("Linear regression (gain + offset); off = offset only") { render() }
        regression.isChecked = true
        add(regression)
        add(button("Apply to device (EEPROM)") { apply() })
        add(button("Reset device calibration (gain 1, offset 0)") {
            cmd("CAL,RESET", "RSP,CAL,") {
                say("Calibration reset")
                readStored()
            }
        })
        stored = note("Stored on device: ?")
        add(button("Read stored values") { readStored() })
        render()
    }

    override fun onLinkChanged(connected: Boolean) {
        if (connected) readStored()
    }

    private fun measure() {
        val t = trueField.text.toString().toDoubleOrNull()
        if (t == null || t < 2.0 || t > 400.0) {
            say("True distance must be 2 to 400 cm")
            return
        }
        say("Measuring…")
        cmd("CAL,RAW,10", "RSP,CAL,RAW", 6_000L) { reply ->
            val raw = reply.split(',').getOrNull(3)?.toDoubleOrNull()
            if (raw == null) {
                say("Bad reply: $reply")
            } else {
                points.add(t to raw / 10.0)
                savePoints()
                say(String.format(Locale.US, "Point %d: raw %.1f cm", points.size, raw / 10.0))
                render()
            }
        }
    }

    private fun apply() {
        val fit = CalMath.fit(points, regression.isChecked)
        if (fit == null) {
            say("Not enough points for this fit")
            return
        }
        val command = String.format(Locale.US, "CAL,SET,%.4f,%.2f", fit.gain, fit.offset)
        cmd(command, "RSP,CAL,") {
            say("Saved: $it")
            readStored()
        }
    }

    private fun readStored() {
        CommandBus.request("CAL,GET", "RSP,CAL,", 3_000L) { reply, _ ->
            val p = reply?.split(',')
            stored.text = if (p != null && p.size >= 4) "Stored on device: gain ${p[2]}, offset ${p[3]} cm" else "Stored on device: ?"
        }
    }

    private fun render() {
        val fit = CalMath.fit(points, regression.isChecked)
        val sb = StringBuilder("  #   true    raw   err0   err1\n")
        val before = ArrayList<Double>()
        val after = ArrayList<Double>()
        points.forEachIndexed { i, (t, r) ->
            val e0 = r - t
            val e1 = if (fit != null) r * fit.gain + fit.offset - t else Double.NaN
            before.add(e0)
            if (fit != null) after.add(e1)
            sb.append(String.format(Locale.US, "%3d %7.1f %6.1f %6.1f %6.1f\n", i + 1, t, r, e0, e1))
        }
        if (fit != null) {
            sb.append(String.format(Locale.US, "\ngain %.4f  offset %.2f cm\n", fit.gain, fit.offset))
            sb.append(String.format(Locale.US, "RMSE %.2f -> %.2f cm", CalMath.rmse(before), CalMath.rmse(after)))
        }
        table.text = sb.toString()
    }

    private fun prefs() = getSharedPreferences("caralarm", Context.MODE_PRIVATE)

    private fun savePoints() {
        prefs().edit().putString("cal_points", points.joinToString(";") { "${it.first}:${it.second}" }).apply()
    }

    private fun loadPoints() {
        points.clear()
        val s = prefs().getString("cal_points", "") ?: ""
        for (item in s.split(';')) {
            val p = item.split(':')
            val t = p.getOrNull(0)?.toDoubleOrNull()
            val r = p.getOrNull(1)?.toDoubleOrNull()
            if (t != null && r != null) points.add(t to r)
        }
    }
}
