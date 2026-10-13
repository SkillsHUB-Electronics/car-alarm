package com.caralarm.app

/** One learning window from the firmware: `FEAT,<label>,f0..f7` (milli-g). */
class FeatRow(val label: Int, val f: IntArray)

/** Parsers for the non-alarm lines of the protocol (PLAN.md D12/D15). Tolerant: anything odd gives null. */
object DeviceLines {
    const val FEAT_COUNT = 8

    fun parseFeat(line: String): FeatRow? {
        val p = line.trim().split(',')
        if (p.size != 2 + FEAT_COUNT || p[0] != "FEAT") return null
        val label = p[1].trim().toIntOrNull() ?: return null
        if (label < 0 || label >= Learn.CLASSES.size) return null
        val f = IntArray(FEAT_COUNT)
        for (i in 0 until FEAT_COUNT) f[i] = p[2 + i].trim().toIntOrNull() ?: return null
        return FeatRow(label, f)
    }

    /** `DBG,status,cm,shockDev,fwd,pkShock,pkBrake,pkAccel,ax,ay,az` (firmware 2.1) -> accel in mg, or null. */
    fun parseDbg(line: String): IntArray? {
        val p = line.trim().split(',')
        if (p.size < 11 || p[0] != "DBG") return null
        val a = IntArray(3)
        for (i in 0..2) a[i] = p[8 + i].trim().toIntOrNull() ?: return null
        return a
    }

    /** `RSP,HW,<lcdAddr>,<lcdOn>,<backlight>,<charger>,<ina>` -> the five numbers, or null. */
    fun parseHw(line: String): IntArray? {
        val p = line.trim().split(',')
        if (p.size != 7 || p[0] != "RSP" || p[1] != "HW") return null
        val v = IntArray(5)
        for (i in 0..4) v[i] = p[2 + i].trim().toIntOrNull() ?: return null
        if (v[0] < 8 || v[0] > 119) return null
        return v
    }

    /** `RSP,I2C,<count>[,<addr>...]` -> addresses (decimal), or null. */
    fun parseI2c(line: String): List<Int>? {
        val p = line.trim().split(',')
        if (p.size < 3 || p[0] != "RSP" || p[1] != "I2C") return null
        val n = p[2].trim().toIntOrNull() ?: return null
        if (p.size != 3 + n) return null
        return p.drop(3).map { it.trim().toIntOrNull() ?: return null }
    }

    /** Accepts `0x27`, `27h`-less hex with prefix, or plain decimal; null if unparsable. */
    fun parseAddr(text: String): Int? {
        val t = text.trim()
        return if (t.startsWith("0x", ignoreCase = true)) t.substring(2).toIntOrNull(16) else t.toIntOrNull()
    }

    /** `CLS,<class>,<percent>` -> (class id, percent). */
    fun parseCls(line: String): Pair<Int, Int>? {
        val p = line.trim().split(',')
        if (p.size != 3 || p[0] != "CLS") return null
        val id = p[1].trim().toIntOrNull() ?: return null
        val pct = p[2].trim().toIntOrNull() ?: return null
        if (id < 0 || id >= Learn.CLASSES.size || pct < 0 || pct > 100) return null
        return id to pct
    }
}

/** Class list and CSV format shared with the web console (console/ml.js). Do not renumber the ids. */
object Learn {
    val CLASSES = listOf(
        "Idle", "Speed 40 km/h", "Speed 80 km/h", "Speed 100 km/h", "Speed 120 km/h", "Speed 140 km/h", "Speed 160 km/h",
        "Brake slow", "Brake medium", "Brake hard", "Bumpy road", "Turn slow", "Turn medium", "Turn fast",
    )
    const val CSV_COMMENT = "# car-alarm-learn v1 (fitur dalam milli-g)"
    const val CSV_HEADER = "session,label_id,label,f0,f1,f2,f3,f4,f5,f6,f7"

    fun csvLine(session: String, label: Int, f: IntArray): String {
        val s = session.replace(Regex("[,\\r\\n]"), " ").trim().ifEmpty { "app" }
        return s + "," + label + "," + CLASSES[label] + "," + f.joinToString(",")
    }
}

/** Splits outgoing text into BLE-sized pieces (default ATT MTU 23 leaves 20 bytes per write). */
object Chunker {
    fun chunks(text: String, size: Int = 20): List<ByteArray> {
        val bytes = text.toByteArray(Charsets.US_ASCII)
        val out = ArrayList<ByteArray>()
        var i = 0
        while (i < bytes.size) {
            out.add(bytes.copyOfRange(i, minOf(bytes.size, i + size)))
            i += size
        }
        return out
    }
}

/** Distance calibration: corrected = gain * raw + offset (PLAN.md D14). */
object CalMath {
    class Fit(val gain: Double, val offset: Double)

    /** [points] are (true cm, raw cm). Regression needs two points with different raw values; otherwise offset only. */
    fun fit(points: List<Pair<Double, Double>>, regression: Boolean): Fit? {
        val n = points.size
        if (n == 0) return null
        if (!regression || n < 2) {
            return Fit(1.0, points.sumOf { it.first - it.second } / n)
        }
        val sx = points.sumOf { it.second }
        val sy = points.sumOf { it.first }
        val sxx = points.sumOf { it.second * it.second }
        val sxy = points.sumOf { it.second * it.first }
        val den = n * sxx - sx * sx
        if (Math.abs(den) < 1e-9) return null
        val gain = (n * sxy - sx * sy) / den
        return Fit(gain, (sy - gain * sx) / n)
    }

    fun rmse(errors: List<Double>): Double =
        if (errors.isEmpty()) 0.0 else Math.sqrt(errors.sumOf { it * it } / errors.size)
}
