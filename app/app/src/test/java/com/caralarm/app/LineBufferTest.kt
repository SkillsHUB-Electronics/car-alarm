package com.caralarm.app

import org.junit.Assert.assertEquals
import org.junit.Test

class LineBufferTest {
    private fun collect(buf: LineBuffer, vararg chunks: String): List<String> {
        val out = mutableListOf<String>()
        for (c in chunks) buf.feed(c.toByteArray(Charsets.US_ASCII)) { out.add(it) }
        return out
    }

    @Test
    fun joinsChunksIntoOneLine() {
        val lines = collect(LineBuffer(), "ALARM,SH", "OCK,0.9", "\n")
        assertEquals(listOf("ALARM,SHOCK,0.9"), lines)
    }

    @Test
    fun splitsSeveralLinesInOneChunk() {
        val lines = collect(LineBuffer(), "OK,DIST,150\r\nWARN,DIST,80\r\n")
        assertEquals(listOf("OK,DIST,150", "WARN,DIST,80"), lines)
    }

    @Test
    fun keepsPartialLineUntilNewline() {
        val buf = LineBuffer()
        assertEquals(emptyList<String>(), collect(buf, "ALARM,DI"))
        assertEquals(listOf("ALARM,DIST,42"), collect(buf, "ST,42\n"))
    }

    @Test
    fun ignoresEmptyLines() {
        assertEquals(listOf("OK,DIST,100"), collect(LineBuffer(), "\n\r\nOK,DIST,100\n\n"))
    }

    @Test
    fun dropsRunawayLineWithoutNewline() {
        val buf = LineBuffer(maxLineLength = 10)
        val lines = collect(buf, "X".repeat(25), "\nOK,DIST,1\n")
        // The overlong garbage is cut; the next valid line still comes through intact.
        assertEquals("OK,DIST,1", lines.last())
    }

    @Test
    fun clearDiscardsPartialLine() {
        val buf = LineBuffer()
        collect(buf, "ALARM,SH")
        buf.clear()
        assertEquals(listOf("OK,DIST,5"), collect(buf, "OK,DIST,5\n"))
    }
}
