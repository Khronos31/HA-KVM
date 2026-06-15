package com.hakvm.remote

/**
 * All endpoints are fixed (LAN / Tailscale). Edit these two lines for your setup.
 */
object Config {

    /**
     * go2rtc stream page. The built-in page auto-negotiates WebRTC (lowest latency)
     * and falls back to MSE. `src` must match your go2rtc stream name
     * (the HA card used `capture_pc`).
     *
     * Example LAN:      http://192.168.1.10:1984/stream.html?src=capture_pc
     * Example Tailscale: http://100.x.y.z:1984/stream.html?src=capture_pc
     */
    const val GO2RTC_STREAM_URL = "http://192.168.1.130:1984/stream.html?src=capture_pc"

    /** The M5AtomS3 receiver (USB-HID injector) WebSocket. */
    const val RECEIVER_WS_URL = "ws://192.168.1.150:81"
}
