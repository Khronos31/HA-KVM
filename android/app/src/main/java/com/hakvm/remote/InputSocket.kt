package com.hakvm.remote

import android.os.Handler
import android.os.Looper
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString
import java.util.concurrent.TimeUnit

/**
 * Persistent binary WebSocket to the M5AtomS3 receiver.
 * Protocol (matches receiver_main.cpp):
 *   keyboard: 2 bytes  [hid, down?1:0]
 *   mouse:    6 bytes  [buttons, x_lo, x_hi, y_lo, y_hi, wheel]   (x/y = int16 LE)
 */
class InputSocket(private val url: String) {

    private val client = OkHttpClient.Builder()
        .pingInterval(20, TimeUnit.SECONDS)
        .build()

    @Volatile private var ws: WebSocket? = null
    private val main = Handler(Looper.getMainLooper())
    private var closed = false

    /** Called on the main thread with the current connection state. */
    var onStatus: ((Boolean) -> Unit)? = null

    fun connect() {
        closed = false
        val req = Request.Builder().url(url).build()
        client.newWebSocket(req, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                ws = webSocket
                main.post { onStatus?.invoke(true) }
            }
            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                ws = null
                main.post { onStatus?.invoke(false) }
                scheduleReconnect()
            }
            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                ws = null
                main.post { onStatus?.invoke(false) }
                scheduleReconnect()
            }
        })
    }

    private fun scheduleReconnect() {
        if (closed) return
        main.postDelayed({ if (!closed) connect() }, 2000)
    }

    fun close() {
        closed = true
        ws?.close(1000, null)
        ws = null
    }

    fun sendKey(hid: Int, down: Boolean) {
        ws?.send(ByteString.of(hid.toByte(), (if (down) 1 else 0).toByte()))
    }

    fun sendMouse(buttons: Int, dx: Int, dy: Int, wheel: Int) {
        ws?.send(
            ByteString.of(
                buttons.toByte(),
                (dx and 0xFF).toByte(), ((dx shr 8) and 0xFF).toByte(),
                (dy and 0xFF).toByte(), ((dy shr 8) and 0xFF).toByte(),
                (wheel and 0xFF).toByte()
            )
        )
    }
}
