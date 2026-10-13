package com.caralarm.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class DeviceLinesTest {
    @Test
    fun parsesFeatureWindow() {
        val r = DeviceLines.parseFeat("FEAT,3,30,20,20,-5,5,90,-40,50")
        assertNotNull(r)
        assertEquals(3, r!!.label)
        assertArrayEquals(intArrayOf(30, 20, 20, -5, 5, 90, -40, 50), r.f)
    }

    @Test
    fun rejectsBadFeatureLines() {
        assertNull(DeviceLines.parseFeat("FEAT,3,30,20"))
        assertNull(DeviceLines.parseFeat("FEAT,99,1,2,3,4,5,6,7,8"))
        assertNull(DeviceLines.parseFeat("FEAT,3,1,2,3,4,5,6,7,x"))
        assertNull(DeviceLines.parseFeat("OK,DIST,10"))
    }

    @Test
    fun parsesClassification() {
        assertEquals(3 to 87, DeviceLines.parseCls("CLS,3,87"))
        assertNull(DeviceLines.parseCls("CLS,3,101"))
        assertNull(DeviceLines.parseCls("CLS,20,50"))
        assertNull(DeviceLines.parseCls("CLS,3"))
    }

    @Test
    fun csvLineMatchesWebFormat() {
        assertEquals("s1,3,Speed 100 km/h,1,2,3,4,5,6,7,8", Learn.csvLine("s1", 3, intArrayOf(1, 2, 3, 4, 5, 6, 7, 8)))
        assertEquals("app,0,Idle,0,0,0,0,0,0,0,0", Learn.csvLine(" ", 0, IntArray(8)))
    }

    @Test
    fun chunksAtTwentyBytes() {
        val c = Chunker.chunks("CFG,SET,100,50,60,40,150,10,1\n")
        assertEquals(2, c.size)
        assertEquals(20, c[0].size)
        assertEquals("CFG,SET,100,50,60,40,150,10,1\n", c.joinToString("") { String(it, Charsets.US_ASCII) })
    }

    @Test
    fun regressionRecoversLinearError() {
        // sensor reads 3% long plus 2 cm: raw = 1.03 * true + 2
        val pts = listOf(20.0, 50.0, 100.0, 200.0).map { it to (1.03 * it + 2.0) }
        val fit = CalMath.fit(pts, true)!!
        for ((t, r) in pts) assertEquals(t, fit.gain * r + fit.offset, 0.01)
    }

    @Test
    fun offsetOnlyUsesMeanDifference() {
        val fit = CalMath.fit(listOf(50.0 to 53.0, 100.0 to 105.0), false)!!
        assertEquals(1.0, fit.gain, 1e-9)
        assertEquals(-4.0, fit.offset, 1e-9)
    }

    @Test
    fun regressionNeedsSpreadingRawValues() {
        assertNull(CalMath.fit(listOf(50.0 to 52.0, 60.0 to 52.0), true))
    }

    @Test
    fun parsesDebugAccel() {
        val a = DeviceLines.parseDbg("DBG,0,125,0.02,0.01,0.10,0.00,0.00,12,-30,1030")
        assertArrayEquals(intArrayOf(12, -30, 1030), a)
        assertNull(DeviceLines.parseDbg("DBG,0,125,0.02,0.01,0.10,0.00,0.00"))  // firmware 2.0 line
        assertNull(DeviceLines.parseDbg("DBG,0,125,0.02,0.01,0.10,0.00,0.00,1,2,x"))
    }

    @Test
    fun parsesHardwareReplies() {
        assertArrayEquals(intArrayOf(39, 1, 1, 0, 0), DeviceLines.parseHw("RSP,HW,39,1,1,0,0"))
        assertNull(DeviceLines.parseHw("RSP,HW,3,1,1,0,0"))
        assertNull(DeviceLines.parseHw("RSP,HW,39,1,1,0"))
        assertEquals(listOf(39, 104), DeviceLines.parseI2c("RSP,I2C,2,39,104"))
        assertEquals(emptyList<Int>(), DeviceLines.parseI2c("RSP,I2C,0"))
        assertNull(DeviceLines.parseI2c("RSP,I2C,2,39"))
        assertEquals(63, DeviceLines.parseAddr("0x3F"))
        assertEquals(39, DeviceLines.parseAddr("39"))
        assertNull(DeviceLines.parseAddr("zz"))
    }
}
