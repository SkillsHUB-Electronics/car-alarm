package com.caralarm.app

import android.os.Handler
import android.os.Looper

/**
 * Command channel to the firmware (PLAN.md D12). [BluetoothService] installs [writer] while the link is up
 * and feeds every received line to [onLine]; screens use [request] to send a command and get the reply.
 * Everything runs on the main thread.
 */
object CommandBus {
    private class Pending(
        val cmd: String,
        val prefix: String,
        val timeoutMs: Long,
        val onDone: (reply: String?, error: String?) -> Unit,
    )

    private val main = Handler(Looper.getMainLooper())
    private val queue = ArrayDeque<Pending>()
    private var current: Pending? = null
    private val timeoutRunnable = Runnable { finish(null, "No reply (timeout)") }
    private val listeners = mutableListOf<(String) -> Unit>()

    /** Installed by the service while connected; takes the full text (with newline), false if it cannot be sent. */
    var writer: ((String) -> Boolean)? = null

    val ready: Boolean get() = writer != null

    fun addLineListener(l: (String) -> Unit) {
        listeners.add(l)
    }

    fun removeLineListener(l: (String) -> Unit) {
        listeners.remove(l)
    }

    /** Fire and forget. */
    fun send(cmd: String): Boolean = writer?.invoke(cmd + "\n") ?: false

    /**
     * Sends [cmd] and calls [onDone] once: with the first reply line starting with [prefix], or with an error
     * for `ERR,<command>`, a failed write, or a timeout. Requests run one at a time.
     */
    fun request(cmd: String, prefix: String, timeoutMs: Long = 3_000L, onDone: (String?, String?) -> Unit) {
        queue.addLast(Pending(cmd, prefix, timeoutMs, onDone))
        if (current == null) startNext()
    }

    fun onLine(line: String) {
        val p = current
        if (p != null) {
            val name = p.cmd.substringBefore(',')
            if (line.startsWith(p.prefix)) finish(line, null)
            else if (line.startsWith("ERR,$name")) finish(null, line)
        }
        listeners.toList().forEach { it(line) }
    }

    /** Link went down: fail everything that is waiting. */
    fun reset() {
        writer = null
        main.removeCallbacks(timeoutRunnable)
        val waiting = ArrayList<Pending>()
        current?.let { waiting.add(it) }
        waiting.addAll(queue)
        current = null
        queue.clear()
        waiting.forEach { it.onDone(null, "Not connected") }
    }

    private fun startNext() {
        val p = queue.removeFirstOrNull() ?: return
        current = p
        val w = writer
        if (w == null || !w(p.cmd + "\n")) {
            finish(null, "Not connected")
            return
        }
        main.postDelayed(timeoutRunnable, p.timeoutMs)
    }

    private fun finish(reply: String?, error: String?) {
        main.removeCallbacks(timeoutRunnable)
        val p = current ?: return
        current = null
        p.onDone(reply, error)
        startNext()
    }
}
