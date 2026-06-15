package com.hakvm.remote

import android.app.Activity
import android.content.res.Configuration
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.Surface
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.webkit.PermissionRequest
import android.webkit.WebChromeClient
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Button
import android.widget.FrameLayout
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.LinearLayout

class MainActivity : Activity() {

    private lateinit var socket: InputSocket
    private lateinit var input: CaptureView
    private lateinit var web: WebView
    private lateinit var wifiIcon: ImageView
    private lateinit var toggle: ImageButton
    private lateinit var videoBtn: ImageButton
    private lateinit var buttonRow: LinearLayout
    private lateinit var toolbar: LinearLayout
    private lateinit var root: FrameLayout

    private var videoOn = true
    private val pressed = HashSet<Int>()
    private var wsUp = false

    companion object {
        // Toolbar thickness, matched to the 16:9 letterbox black bar (measured 176 px on Pixel 9a).
        // Same in both orientations: vertical strip width (landscape) / top band height (portrait).
        private const val TOOLBAR_PX = 176
        private const val ICON_PX = 132
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        hideSystemBars()

        socket = InputSocket(Config.RECEIVER_WS_URL).apply {
            onStatus = { up -> wsUp = up; runOnUiThread { updateStatus() } }
        }

        web = WebView(this).apply {
            isFocusable = false
            isFocusableInTouchMode = false
            settings.javaScriptEnabled = true
            settings.domStorageEnabled = true
            settings.mediaPlaybackRequiresUserGesture = false
            webViewClient = WebViewClient()
            webChromeClient = object : WebChromeClient() {
                override fun onPermissionRequest(request: PermissionRequest) {
                    request.grant(request.resources)
                }
            }
            loadUrl(Config.GO2RTC_STREAM_URL)
        }

        input = CaptureView(this).apply {
            onMouse = { b, dx, dy, w -> socket.sendMouse(b, dx, dy, w) }
            onCaptureChanged = { cap ->
                runOnUiThread {
                    if (!cap) releaseAllKeys()
                    toggle.setColorFilter(if (cap) Color.GREEN else Color.WHITE)
                    updateStatus()
                }
            }
        }

        buildToolbar()
        buildButtonRow()

        root = FrameLayout(this).apply { setBackgroundColor(Color.BLACK) }
        setContentView(root)

        buildLayout()
        socket.connect()
    }

    private fun iconButton(resId: Int, onClick: () -> Unit) = ImageButton(this).apply {
        setImageResource(resId)
        scaleType = ImageView.ScaleType.FIT_CENTER
        setColorFilter(Color.WHITE)
        setBackgroundColor(Color.TRANSPARENT)
        setPadding(10, 10, 10, 10)
        isFocusable = false
        isFocusableInTouchMode = false
        setOnClickListener { onClick() }
    }

    private fun buildToolbar() {
        wifiIcon = ImageView(this).apply {
            setImageResource(R.drawable.ic_wifi)
            scaleType = ImageView.ScaleType.FIT_CENTER
            setColorFilter(Color.GRAY)
            setPadding(10, 10, 10, 10)
        }
        videoBtn = iconButton(R.drawable.ic_videocam) { setVideo(!videoOn) }
        toggle = iconButton(R.drawable.ic_mouse) {
            if (input.hasPointerCapture()) input.release() else input.capture()
        }
        toolbar = LinearLayout(this).apply { setBackgroundColor(0x99000000.toInt()) }
    }

    /**
     * Re-flow the toolbar for the current orientation. A weighted spacer pins the Wi-Fi icon to the
     * leading edge (top in landscape strip / left in portrait band) and video + mouse to the trailing edge.
     */
    private fun layoutToolbar(portrait: Boolean) {
        toolbar.removeAllViews()
        listOf<View>(wifiIcon, videoBtn, toggle).forEach { (it.parent as? ViewGroup)?.removeView(it) }
        val sz = ICON_PX
        val spacer = View(this)
        if (portrait) {
            toolbar.orientation = LinearLayout.HORIZONTAL
            toolbar.gravity = Gravity.CENTER_VERTICAL
            toolbar.setPadding(16, 0, 16, 0)
            toolbar.addView(wifiIcon, LinearLayout.LayoutParams(sz, sz))
            toolbar.addView(spacer, LinearLayout.LayoutParams(0, 1, 1f))
            toolbar.addView(videoBtn, LinearLayout.LayoutParams(sz, sz))
            toolbar.addView(toggle, LinearLayout.LayoutParams(sz, sz))
        } else {
            toolbar.orientation = LinearLayout.VERTICAL
            toolbar.gravity = Gravity.CENTER_HORIZONTAL
            toolbar.setPadding(0, 16, 0, 16)
            toolbar.addView(wifiIcon, LinearLayout.LayoutParams(sz, sz))
            toolbar.addView(spacer, LinearLayout.LayoutParams(1, 0, 1f))
            toolbar.addView(videoBtn, LinearLayout.LayoutParams(sz, sz))
            toolbar.addView(toggle, LinearLayout.LayoutParams(sz, sz))
        }
    }

    private fun buildButtonRow() {
        fun makeBtn(label: String, mask: Int): Button = Button(this).apply {
            text = label
            textSize = 18f
            isFocusable = false
            isFocusableInTouchMode = false
            setOnTouchListener { _, e ->
                when (e.actionMasked) {
                    MotionEvent.ACTION_DOWN -> { input.setHeld(mask, true); true }
                    MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> { input.setHeld(mask, false); true }
                    else -> false
                }
            }
        }
        buttonRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setBackgroundColor(0xAA000000.toInt())
            addView(makeBtn("L", 0x01), LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1f))
            addView(makeBtn("M", 0x04), LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1f))
            addView(makeBtn("R", 0x02), LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1f))
        }
    }

    private fun buildLayout() {
        root.removeAllViews()
        listOf<View>(web, input, buttonRow, toolbar).forEach { (it.parent as? ViewGroup)?.removeView(it) }

        val portrait = resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT
        val dm = resources.displayMetrics
        val MP = ViewGroup.LayoutParams.MATCH_PARENT

        layoutToolbar(portrait)

        if (portrait) {
            // Toolbar as a top band (over the punch-hole), video 16:9 below it, then trackpad + L/M/R.
            val content = FrameLayout(this)
            val videoH = dm.widthPixels * 9 / 16
            content.addView(web, FrameLayout.LayoutParams(MP, videoH).apply { gravity = Gravity.TOP })
            content.addView(input, FrameLayout.LayoutParams(MP, MP))
            val rowH = (64 * dm.density).toInt()
            content.addView(buttonRow, FrameLayout.LayoutParams(MP, rowH).apply { gravity = Gravity.BOTTOM })

            val shell = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
            shell.addView(toolbar, LinearLayout.LayoutParams(MP, TOOLBAR_PX))
            shell.addView(content, LinearLayout.LayoutParams(MP, 0, 1f))
            root.addView(shell, FrameLayout.LayoutParams(MP, MP))
        } else {
            // Video stays FULLSCREEN so go2rtc letterboxes it into the safe (non-clipped) area
            // (measured x=328..2247 on Pixel 9a). The toolbar floats over the black bar, same
            // thickness as the right bar (176 px) so it never shadows the video.
            root.addView(web, FrameLayout.LayoutParams(MP, MP))
            root.addView(input, FrameLayout.LayoutParams(MP, MP))

            @Suppress("DEPRECATION")
            val onLeft = windowManager.defaultDisplay.rotation != Surface.ROTATION_270
            root.addView(toolbar, FrameLayout.LayoutParams(TOOLBAR_PX, MP).apply {
                gravity = (if (onLeft) Gravity.START else Gravity.END) or Gravity.CENTER_VERTICAL
            })
        }

        web.visibility = if (videoOn) View.VISIBLE else View.GONE
        updateStatus()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        buildLayout()
        hideSystemBars()
    }

    private fun updateStatus() {
        wifiIcon.setColorFilter(if (wsUp) Color.GREEN else Color.GRAY)
    }

    private fun setVideo(on: Boolean) {
        videoOn = on
        videoBtn.setImageResource(if (on) R.drawable.ic_videocam else R.drawable.ic_videocam_off)
        if (on) {
            web.visibility = View.VISIBLE
            web.loadUrl(Config.GO2RTC_STREAM_URL)
        } else {
            web.loadUrl("about:blank")
            web.visibility = View.GONE
        }
    }

    private fun releaseAllKeys() {
        for (hid in pressed) socket.sendKey(hid, false)
        pressed.clear()
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val kc = event.keyCode

        if (kc == KeyEvent.KEYCODE_BACK) {
            if (input.hasPointerCapture()) { input.release(); return true }
            return super.dispatchKeyEvent(event)
        }

        val hid = HidKeymap.toHid(kc)
        if (hid != null && input.hasPointerCapture()) {
            when (event.action) {
                KeyEvent.ACTION_DOWN -> if (event.repeatCount == 0 && pressed.add(hid)) socket.sendKey(hid, true)
                KeyEvent.ACTION_UP -> if (pressed.remove(hid)) socket.sendKey(hid, false)
            }
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    override fun onPause() {
        super.onPause()
        releaseAllKeys()
        if (input.hasPointerCapture()) input.release()
    }

    override fun onDestroy() {
        super.onDestroy()
        socket.close()
    }

    @Suppress("DEPRECATION")
    private fun hideSystemBars() {
        window.decorView.systemUiVisibility = (
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            )
    }
}
