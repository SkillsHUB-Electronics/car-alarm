package com.caralarm.app

enum class MsgType { OK, WARN, ALARM, TEST }

enum class MsgSource { DIST, SHOCK, BRAKE, BT }

data class AlarmMessage(val type: MsgType, val source: MsgSource, val value: Float?)

/**
 * Parser for the line protocol sent by the Arduino: `TYPE,SOURCE[,VALUE]\n`.
 * Examples: `OK,DIST,150`, `WARN,DIST,80`, `ALARM,DIST,42`, `ALARM,SHOCK,0.9`, `ALARM,BRAKE,0.5`.
 * Returns null for anything malformed so noisy or truncated lines are ignored.
 */
object Protocol {
    fun parse(line: String): AlarmMessage? {
        val parts = line.trim().split(',')
        if (parts.size < 2 || parts.size > 3) return null

        val type = MsgType.values().firstOrNull { it.name == parts[0].trim().uppercase() } ?: return null
        val source = MsgSource.values().firstOrNull { it.name == parts[1].trim().uppercase() } ?: return null

        val rawValue = parts.getOrNull(2)?.trim()
        val value = if (rawValue.isNullOrEmpty()) null else rawValue.toFloatOrNull() ?: return null

        return AlarmMessage(type, source, value)
    }
}
