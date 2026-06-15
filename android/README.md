# ha_kvm Remote (Android)

A native Android client that does everything the iPad `pc_remote.html` does, in one app:

- **Video** — go2rtc stream shown in a WebView (auto-negotiates WebRTC, falls back to MSE).
- **Mouse** — Android **Pointer Capture** (`AXIS_RELATIVE_X/Y`) → raw relative deltas, no
  acceleration/deadzone. Sent as the existing 6-byte report.
- **Keyboard** — every physical key via `dispatchKeyEvent` → existing 2-byte report, with a
  user-editable remap layer. Held keys are tracked and force-released on focus loss, so the
  "stuck key / continuous input" failure mode can't happen.

Because this one app sends **both** keyboard and mouse, it is a single WebSocket client and the
**M5AtomS3 receiver runs unchanged** (`src/receiver/receiver_main.cpp`). No `+38` gamepad smuggling,
no BLE-mouse→gamepad ESP32 in the loop.

## 1. Configure

Edit [`app/src/main/java/com/hakvm/remote/Config.kt`](app/src/main/java/com/hakvm/remote/Config.kt):

```kotlin
const val GO2RTC_STREAM_URL = "http://<go2rtc-host>:1984/stream.html?src=capture_pc"
const val RECEIVER_WS_URL   = "ws://192.168.1.150:81"
```

Use Tailscale IPs (e.g. `http://100.x.y.z:1984/...`) when remote — the Pixel is a full tailnet
node, so the app reaches home with zero public exposure.

Remaps live in [`HidKeymap.kt`](app/src/main/java/com/hakvm/remote/HidKeymap.kt) → `REMAP`
(e.g. CapsLock → Left-Ctrl).

## 2. Build

Open the `android/` folder in **Android Studio** (Hedgehog or newer) and let it sync; it will set
up the Gradle wrapper. Then Run on the Pixel (USB debugging on).

CLI alternative once the wrapper jar exists:

```
./gradlew installDebug
```

> Built with: AGP 8.7, Gradle 8.9, Kotlin 2.0, compileSdk 35, minSdk 26 (Pointer Capture needs
> Android 8.0+). Only third-party dependency: OkHttp.

## 3. Use

1. Pair a **Bluetooth mouse** and connect a **keyboard** to the Pixel.
2. Launch the app — video fills the screen, status bar top-left shows `WS: connected`.
3. **Tap the screen** (or the *Capture* button) to grab the mouse — the system cursor disappears
   and motion/keys are forwarded to the PC.
4. Press **BACK** (gesture or key) to release capture and return the devices to Android.

## Status / scope

- This is an MVP prototype. It has **not** been compiled or run here (no Android SDK/device in the
  build environment) — first build is on your machine.
- Video uses WebView WebRTC for speed of implementation. If latency/compat disappoints, the video
  layer can be swapped for a native low-latency player without touching the input path.
- Running this app **alongside** the iPad page at the same time needs the receiver to accept 2
  concurrent WebSocket clients (it currently keeps one). For "Pixel does everything", no change.

## Known things to verify on-device

- Pointer Capture re-acquisition after notifications/multi-window.
- iPad Personal Hotspot / Pixel cellular path latency when fully mobile.
- `systemUiVisibility` is deprecated on API 30+ (still works); migrate to `WindowInsetsController`
  if it misbehaves on your Android version.
