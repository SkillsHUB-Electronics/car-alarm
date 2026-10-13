package com.caralarm.app

/**
 * Reassembles text lines from BLE notification chunks (typically at most 20 bytes each).
 * Pure Kotlin so it can be unit tested without Android.
 */
class LineBuffer(private val maxLineLength: Int = 120) {
    private val sb = StringBuilder()

    /** Feeds [bytes] and calls [onLine] for every completed, non-empty line (CR/LF stripped). */
    fun feed(bytes: ByteArray, onLine: (String) -> Unit) {
        for (b in bytes) {
            val c = (b.toInt() and 0xFF).toChar()
            when (c) {
                '\n' -> {
                    if (sb.isNotEmpty()) onLine(sb.toString())
                    sb.setLength(0)
                }
                '\r' -> Unit
                else -> {
                    // Runaway data without a newline: drop it instead of growing without bound.
                    if (sb.length >= maxLineLength) sb.setLength(0)
                    sb.append(c)
                }
            }
        }
    }

    fun clear() = sb.setLength(0)
}
