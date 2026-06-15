package com.hakvm.remote

import android.view.KeyEvent

/**
 * Android keyCode -> USB HID usage, matching the receiver's protocol:
 *   - normal keys: raw HID usage code (ESP32 `pressRaw`)
 *   - modifiers:   0x80..0x87  (ESP32 `press`, KEY_LEFT_CTRL.. etc.)
 *
 * This mirrors HID_MAP in dashboard/pc_remote.html so receiver_main.cpp is unchanged.
 */
object HidKeymap {

    /**
     * User-editable remap layer: Android keyCode -> Android keyCode.
     * Applied before the HID lookup. Add your own rules here.
     */
    val REMAP: Map<Int, Int> = mapOf(
        // Example — send Left-Ctrl when CapsLock is pressed:
        // KeyEvent.KEYCODE_CAPS_LOCK to KeyEvent.KEYCODE_CTRL_LEFT,
    )

    private val BASE: Map<Int, Int> = buildMap {
        // Letters A..Z -> 0x04..0x1D
        for (kc in KeyEvent.KEYCODE_A..KeyEvent.KEYCODE_Z) put(kc, 0x04 + (kc - KeyEvent.KEYCODE_A))
        // Digits 1..9 -> 0x1E..0x26, 0 -> 0x27
        for (kc in KeyEvent.KEYCODE_1..KeyEvent.KEYCODE_9) put(kc, 0x1E + (kc - KeyEvent.KEYCODE_1))
        put(KeyEvent.KEYCODE_0, 0x27)
        // F1..F12 -> 0x3A..0x45
        for (kc in KeyEvent.KEYCODE_F1..KeyEvent.KEYCODE_F12) put(kc, 0x3A + (kc - KeyEvent.KEYCODE_F1))

        put(KeyEvent.KEYCODE_ENTER, 0x28)
        put(KeyEvent.KEYCODE_ESCAPE, 0x29)
        put(KeyEvent.KEYCODE_DEL, 0x2A)              // Backspace
        put(KeyEvent.KEYCODE_TAB, 0x2B)
        put(KeyEvent.KEYCODE_SPACE, 0x2C)
        put(KeyEvent.KEYCODE_MINUS, 0x2D)
        put(KeyEvent.KEYCODE_EQUALS, 0x2E)
        put(KeyEvent.KEYCODE_LEFT_BRACKET, 0x2F)
        put(KeyEvent.KEYCODE_RIGHT_BRACKET, 0x30)
        put(KeyEvent.KEYCODE_BACKSLASH, 0x31)
        put(KeyEvent.KEYCODE_SEMICOLON, 0x33)
        put(KeyEvent.KEYCODE_APOSTROPHE, 0x34)
        put(KeyEvent.KEYCODE_GRAVE, 0x35)
        put(KeyEvent.KEYCODE_COMMA, 0x36)
        put(KeyEvent.KEYCODE_PERIOD, 0x37)
        put(KeyEvent.KEYCODE_SLASH, 0x38)
        put(KeyEvent.KEYCODE_CAPS_LOCK, 0x39)

        // Navigation / editing
        put(KeyEvent.KEYCODE_DPAD_RIGHT, 0x4F)
        put(KeyEvent.KEYCODE_DPAD_LEFT, 0x50)
        put(KeyEvent.KEYCODE_DPAD_DOWN, 0x51)
        put(KeyEvent.KEYCODE_DPAD_UP, 0x52)
        put(KeyEvent.KEYCODE_INSERT, 0x49)
        put(KeyEvent.KEYCODE_MOVE_HOME, 0x4A)
        put(KeyEvent.KEYCODE_PAGE_UP, 0x4B)
        put(KeyEvent.KEYCODE_FORWARD_DEL, 0x4C)      // Delete
        put(KeyEvent.KEYCODE_MOVE_END, 0x4D)
        put(KeyEvent.KEYCODE_PAGE_DOWN, 0x4E)

        // Modifiers (receiver expects 0x80..0x87)
        put(KeyEvent.KEYCODE_CTRL_LEFT, 0x80)
        put(KeyEvent.KEYCODE_SHIFT_LEFT, 0x81)
        put(KeyEvent.KEYCODE_ALT_LEFT, 0x82)
        put(KeyEvent.KEYCODE_META_LEFT, 0x83)
        put(KeyEvent.KEYCODE_CTRL_RIGHT, 0x84)
        put(KeyEvent.KEYCODE_SHIFT_RIGHT, 0x85)
        put(KeyEvent.KEYCODE_ALT_RIGHT, 0x86)
        put(KeyEvent.KEYCODE_META_RIGHT, 0x87)
    }

    /** Returns the HID byte to send, or null if this key is not mapped. */
    fun toHid(keyCode: Int): Int? {
        val mapped = REMAP[keyCode] ?: keyCode
        return BASE[mapped]
    }
}
