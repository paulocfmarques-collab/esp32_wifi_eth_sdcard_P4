# CameraDmaFix

Project-provided helper for the Arduino ESP32 4.0.0 ESP32-P4 CSI driver. Install with the repository's `tools/install-camera-dma-fix.ps1` script and restart Arduino IDE after updating it.

The Arduino builder reads library `ldflags` through its precompiled-library path. `precompiled=true` keeps the C++ source compiled while enabling that path. `src/esp32p4/libCameraDmaLink.a` is a valid empty GNU archive (the eight-byte archive signature); it supplies no machine code. Its presence enables discovery of the architecture folder and application of `--wrap=heap_caps_calloc`. Keep this file when copying or packaging the library. See the [Arduino builder implementation](https://github.com/arduino/arduino-cli/blob/master/internal/arduino/builder/libraries.go).

The wrapper adds `MALLOC_CAP_INTERNAL` to exactly-default capability allocations only in the task inside `CameraDmaInternalScope`, outside interrupt context. All other calls retain their requested capabilities. The scope must not be nested or used by multiple callers simultaneously; the camera startup is its sole caller.

The scope makes a small probe allocation through the public allocator and checks that the wrapper ran and returned internal memory. `ready()` reports that check. `allocations()` counts intercepted calls after the probe; it is not a count of image buffers or successful captures. Camera status exposes both diagnostics. The SDK is not modified.
