package com.hakvm.remote

import android.content.Context
import android.graphics.Color
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs

/**
 * Full-area input surface, overlaid over the video in both orientations:
 *   - touch        = trackpad (drag = move, tap = left click, 2-finger tap = right, 2-finger drag = scroll)
 *   - physical mouse = Pointer Capture (engaged via the toolbar Capture button)
 * On-screen L/M/R buttons set [heldButtons] so a held button + drag performs a drag.
 */
class CaptureView(context: Context) : View(context) {

    var onMouse: ((Int, Int, Int, Int) -> Unit)? = null
    var onCaptureChanged: ((Boolean) -> Unit)? = null
    var heldButtons = 0

    // --- physical mouse / pointer capture ---
    private var wantCapture = false
    private var captureTries = 0
    private val captureRunnable = Runnable { tryCapture() }
    private var accX = 0f
    private var accY = 0f
    private var lastCapButtons = 0

    // --- touch trackpad ---
    private val density = context.resources.displayMetrics.density
    private val tapSlop = 12f * density
    private val scrollStep = 16f * density
    private var lastX = 0f
    private var lastY = 0f
    private var downX = 0f
    private var downY = 0f
    private var downTime = 0L
    private var maxPointers = 0
    private var movedFar = false
    private var scrollAccum = 0f
    private var lastScrollY = 0f

    init {
        isFocusable = true
        isFocusableInTouchMode = true
        isClickable = true
        setBackgroundColor(Color.TRANSPARENT)
    }

    /** Called by the on-screen L/M/R buttons. */
    fun setHeld(mask: Int, down: Boolean) {
        heldButtons = if (down) heldButtons or mask else heldButtons and mask.inv()
        onMouse?.invoke(heldButtons, 0, 0, 0)
    }

    // ---------------- physical mouse: Pointer Capture ----------------

    fun capture() {
        wantCapture = true
        captureTries = 0
        removeCallbacks(captureRunnable)
        tryCapture()
    }

    private fun tryCapture() {
        if (!wantCapture || hasPointerCapture()) return
        if (hasWindowFocus()) {
            requestFocus()
            requestPointerCapture()
        }
        if (captureTries++ < 12) postDelayed(captureRunnable, 120)
    }

    fun release() {
        wantCapture = false
        removeCallbacks(captureRunnable)
        releasePointerCapture()
    }

    override fun onWindowFocusChanged(hasWindowFocus: Boolean) {
        super.onWindowFocusChanged(hasWindowFocus)
        if (hasWindowFocus && wantCapture && !hasPointerCapture()) {
            requestFocus()
            requestPointerCapture()
        }
    }

    override fun onPointerCaptureChange(hasCapture: Boolean) {
        super.onPointerCaptureChange(hasCapture)
        if (!hasCapture) lastCapButtons = 0
        onCaptureChanged?.invoke(hasCapture)
    }

    override fun onCapturedPointerEvent(event: MotionEvent): Boolean {
        accX += event.getAxisValue(MotionEvent.AXIS_RELATIVE_X)
        accY += event.getAxisValue(MotionEvent.AXIS_RELATIVE_Y)
        val dx = accX.toInt(); accX -= dx
        val dy = accY.toInt(); accY -= dy

        val vscroll = event.getAxisValue(MotionEvent.AXIS_VSCROLL)
        val wheel = if (vscroll > 0f) 1 else if (vscroll < 0f) -1 else 0

        var buttons = heldButtons
        val b = event.buttonState
        if (b and MotionEvent.BUTTON_PRIMARY != 0) buttons = buttons or 0x01
        if (b and MotionEvent.BUTTON_SECONDARY != 0) buttons = buttons or 0x02
        if (b and MotionEvent.BUTTON_TERTIARY != 0) buttons = buttons or 0x04
        if (b and MotionEvent.BUTTON_BACK != 0) buttons = buttons or 0x08
        if (b and MotionEvent.BUTTON_FORWARD != 0) buttons = buttons or 0x10

        if (dx != 0 || dy != 0 || wheel != 0 || buttons != lastCapButtons) {
            lastCapButtons = buttons
            onMouse?.invoke(buttons, dx, dy, wheel * 2)
        }
        return true
    }

    // ---------------- touch: trackpad ----------------

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                downX = event.x; downY = event.y
                lastX = event.x; lastY = event.y
                downTime = SystemClock.uptimeMillis()
                maxPointers = 1
                movedFar = false
                lastScrollY = event.getY(0)
                scrollAccum = 0f
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                maxPointers = maxOf(maxPointers, event.pointerCount)
                lastScrollY = event.getY(0)
                scrollAccum = 0f
            }
            MotionEvent.ACTION_MOVE -> {
                if (event.pointerCount >= 2) {
                    val y = event.getY(0)
                    scrollAccum += y - lastScrollY
                    lastScrollY = y
                    if (abs(scrollAccum) >= scrollStep) {
                        val notches = (scrollAccum / scrollStep).toInt()
                        onMouse?.invoke(heldButtons, 0, 0, -notches)
                        scrollAccum -= notches * scrollStep
                        movedFar = true
                    }
                } else {
                    val dx = ((event.x - lastX) * SENS).toInt()
                    val dy = ((event.y - lastY) * SENS).toInt()
                    if (dx != 0 || dy != 0) {
                        onMouse?.invoke(heldButtons, dx, dy, 0)
                        lastX = event.x; lastY = event.y
                    }
                    if (abs(event.x - downX) > tapSlop || abs(event.y - downY) > tapSlop) movedFar = true
                }
            }
            MotionEvent.ACTION_UP -> {
                val dt = SystemClock.uptimeMillis() - downTime
                if (!movedFar && dt < TAP_MS) {
                    val btn = if (maxPointers >= 2) 0x02 else 0x01
                    onMouse?.invoke(heldButtons or btn, 0, 0, 0)
                    postDelayed({ onMouse?.invoke(heldButtons, 0, 0, 0) }, 50)
                }
            }
        }
        return true
    }

    companion object {
        private const val SENS = 1.6f
        private const val TAP_MS = 250L
    }
}
