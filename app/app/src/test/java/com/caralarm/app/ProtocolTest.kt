package com.caralarm.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class ProtocolTest {
    @Test
    fun parsesDistanceAlarm() {
        val msg = Protocol.parse("ALARM,DIST,42")
        assertNotNull(msg)
        assertEquals(MsgType.ALARM, msg!!.type)
        assertEquals(MsgSource.DIST, msg.source)
        assertEquals(42f, msg.value!!, 0.001f)
    }

    @Test
    fun parsesDecimalAndTrimsWhitespace() {
        val msg = Protocol.parse("  alarm,shock,0.9\r\n")
        assertEquals(MsgSource.SHOCK, msg!!.source)
        assertEquals(0.9f, msg.value!!, 0.001f)
    }

    @Test
    fun parsesWithoutValue() {
        val msg = Protocol.parse("OK,DIST")
        assertEquals(MsgType.OK, msg!!.type)
        assertNull(msg.value)
    }

    @Test
    fun rejectsMalformedLines() {
        assertNull(Protocol.parse(""))
        assertNull(Protocol.parse("garbage"))
        assertNull(Protocol.parse("ALARM"))
        assertNull(Protocol.parse("ALARM,DIS"))
        assertNull(Protocol.parse("ALARM,DIST,abc"))
        assertNull(Protocol.parse("ALARM,DIST,1,2"))
        assertNull(Protocol.parse("FOO,DIST,1"))
    }
}
