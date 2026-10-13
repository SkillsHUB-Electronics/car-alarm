package com.caralarm.app

import android.app.Activity
import android.content.Intent
import android.media.RingtoneManager
import android.net.Uri
import android.widget.CheckBox
import android.widget.EditText
import java.util.Locale

/** Firmware parameters kept in EEPROM (CFG,GET / CFG,SET / CFG,RESET). */
class SettingsActivity : ToolActivity() {
    override val screenTitle = "Device settings"

    private lateinit var warn: EditText
    private lateinit var danger: EditText
    private lateinit var shock: EditText
    private lateinit var brake: EditText
    private lateinit var brakeMs: EditText
    private lateinit var hold: EditText
    private lateinit var buzzer: CheckBox
    private lateinit var lcdAddr: EditText
    private lateinit var lcdOn: CheckBox
    private lateinit var backlight: CheckBox
    private lateinit var charger: CheckBox
    private lateinit var ina: CheckBox
    private lateinit var scanOut: android.widget.TextView

    private val pickRequest = 42
    private lateinit var toneName: android.widget.TextView
    private val preview by lazy { AlarmSound(this) }

    override fun onStop() {
        preview.stop()
        super.onStop()
    }

    @Deprecated("Deprecated in Java")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != pickRequest || resultCode != Activity.RESULT_OK) return
        @Suppress("DEPRECATION")
        val uri = data?.getParcelableExtra<Uri>(RingtoneManager.EXTRA_RINGTONE_PICKED_URI)
        if (uri != null) {
            AlarmSound.save(this, uri.toString())
            toneName.text = "Current: ${AlarmSound.title(this)}"
        }
    }

    override fun buildUi() {
        label("App alarm tone")
        note("Played on the phone when the car alarm triggers (distance, shock, braking), on the alarm volume.")
        toneName = note("Current: ${AlarmSound.title(this)}")
        add(button("Use built-in siren") {
            AlarmSound.save(this, "")
            toneName.text = "Current: ${AlarmSound.title(this)}"
        })
        add(button("Choose tone from phone") {
            val i = Intent(RingtoneManager.ACTION_RINGTONE_PICKER).apply {
                putExtra(RingtoneManager.EXTRA_RINGTONE_TYPE, RingtoneManager.TYPE_ALARM or RingtoneManager.TYPE_RINGTONE)
                putExtra(RingtoneManager.EXTRA_RINGTONE_SHOW_DEFAULT, true)
                putExtra(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT, false)
                putExtra(RingtoneManager.EXTRA_RINGTONE_TITLE, "Alarm tone")
                val cur = AlarmSound.savedUri(this@SettingsActivity)
                if (cur.isNotEmpty()) putExtra(RingtoneManager.EXTRA_RINGTONE_EXISTING_URI, Uri.parse(cur))
            }
            startActivityForResult(i, pickRequest)
        })
        row(button("Test tone") { preview.play() }, button("Stop") { preview.stop() })

        label("Device settings")
        note("Rules: danger + 10 <= warning. Values are stored in the device EEPROM. The distance calibration is not touched by Reset.")
        label("Warning distance <= (cm)")
        warn = field("20-300", numeric = true)
        add(warn)
        label("Danger distance <= (cm)")
        danger = field("10-290", numeric = true)
        add(danger)
        label("Shock threshold (g)")
        shock = field("0.10-3.00", numeric = true)
        add(shock)
        label("Hard-braking threshold (g)")
        brake = field("0.05-2.00", numeric = true)
        add(brake)
        label("Braking must last at least (ms)")
        brakeMs = field("50-2000", numeric = true)
        add(brakeMs)
        label("Hold shock/brake alarm (s)")
        hold = field("1-60", numeric = true)
        add(hold)
        buzzer = check("Buzzer enabled")
        buzzer.isChecked = true
        add(buzzer)

        add(button("Read from device") { read() })
        add(button("Save to device") { save() })
        add(button("Restore defaults") {
            cmd("CFG,RESET", "RSP,CFG") {
                show(it)
                say("Defaults restored")
            }
        })
        note("Shock and braking thresholds were only tested on the bench, not on a road.")

        label("Hardware (EEPROM)")
        note("LCD address is usually 0x27 or 0x3F; the MPU6050 is 0x68. Changes apply immediately.")
        lcdAddr = field("0x27", "0x27", maxLen = 5)
        add(lcdAddr)
        lcdOn = check("LCD enabled")
        backlight = check("LCD backlight on")
        charger = check("Battery charger enabled (reserved: stored only, no function yet)")
        ina = check("INA current sensor enabled (reserved: stored only, no function yet)")
        for (c in listOf(lcdOn, backlight, charger, ina)) add(c)
        scanOut = note("")
        add(button("Scan I2C bus") { scan() })
        add(button("Read hardware settings") { readHw() })
        add(button("Save hardware settings") { saveHw() })
        add(button("Restore hardware defaults") {
            cmd("HW,RESET", "RSP,HW") {
                showHw(it)
                say("Hardware defaults restored")
            }
        })
    }

    private fun hex(a: Int) = "0x" + a.toString(16).uppercase().padStart(2, '0')

    private fun showHw(reply: String) {
        val v = DeviceLines.parseHw(reply) ?: return
        lcdAddr.setText(hex(v[0]))
        lcdOn.isChecked = v[1] == 1
        backlight.isChecked = v[2] == 1
        charger.isChecked = v[3] == 1
        ina.isChecked = v[4] == 1
    }

    private fun readHw() {
        cmd("HW,GET", "RSP,HW") {
            showHw(it)
            say("Hardware settings read from device")
        }
    }

    private fun saveHw() {
        val a = DeviceLines.parseAddr(lcdAddr.text.toString())
        if (a == null || a < 8 || a > 119) {
            say("LCD address must be 0x08-0x77 (e.g. 0x27)")
            return
        }
        fun b(c: CheckBox) = if (c.isChecked) 1 else 0
        cmd("HW,SET,$a,${b(lcdOn)},${b(backlight)},${b(charger)},${b(ina)}", "RSP,HW") {
            showHw(it)
            say("Saved (charger and INA have no function yet)")
        }
    }

    private fun scan() {
        scanOut.text = "Scanning..."
        cmd("I2C,SCAN", "RSP,I2C", 5_000L) { reply ->
            val found = DeviceLines.parseI2c(reply)
            scanOut.text = when {
                found == null -> "Scan reply not understood"
                found.isEmpty() -> "No I2C device found (check SDA/SCL wiring)"
                else -> "Found: " + found.joinToString(", ") {
                    hex(it) + when (it) { 0x68 -> " (MPU6050)"; 0x27, 0x3F -> " (LCD)"; else -> "" }
                }
            }
        }
    }

    override fun onLinkChanged(connected: Boolean) {
        if (connected) {
            read()
            readHw()
        }
    }

    private fun read() {
        cmd("CFG,GET", "RSP,CFG") {
            show(it)
            say("Read from device")
        }
    }

    private fun show(reply: String) {
        val p = reply.split(',').drop(2).map { it.trim().toIntOrNull() ?: return }
        if (p.size < 7) return
        warn.setText(p[0].toString())
        danger.setText(p[1].toString())
        shock.setText(String.format(Locale.US, "%.2f", p[2] / 100.0))
        brake.setText(String.format(Locale.US, "%.2f", p[3] / 100.0))
        brakeMs.setText(p[4].toString())
        hold.setText(p[5].toString())
        buzzer.isChecked = p[6] == 1
    }

    private fun save() {
        val w = warn.text.toString().toIntOrNull()
        val d = danger.text.toString().toIntOrNull()
        val s = shock.text.toString().toDoubleOrNull()
        val b = brake.text.toString().toDoubleOrNull()
        val bm = brakeMs.text.toString().toIntOrNull()
        val h = hold.text.toString().toIntOrNull()
        if (w == null || d == null || s == null || b == null || bm == null || h == null) {
            say("Fill in every field with numbers")
            return
        }
        val command = "CFG,SET,$w,$d,${Math.round(s * 100)},${Math.round(b * 100)},$bm,$h,${if (buzzer.isChecked) 1 else 0}"
        cmd(command, "RSP,CFG") {
            show(it)
            say("Saved to device EEPROM")
        }
    }
}
