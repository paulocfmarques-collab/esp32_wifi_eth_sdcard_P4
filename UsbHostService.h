#pragma once
#include <Arduino.h>
#include <atomic>
#include <freertos/semphr.h>

class UsbHostService {
public:
    void begin();
    String status();
    String files();
    void rescan() { _rescan = true; }
    bool ready() const { return _state == 1; }
    uint32_t reports() const { return _keyboardReports + _mouseReports; }
private:
    TaskHandle_t _task = nullptr;
    SemaphoreHandle_t _mutex = nullptr;
    std::atomic<unsigned> _state{0};
    std::atomic<bool> _keyboard{false}, _mouse{false}, _drive{false}, _mounted{false}, _rescan{false};
    std::atomic<uint32_t> _keyboardReports{0}, _mouseReports{0}, _errors{0}, _blocks{0}, _blockSize{0};
    String _files;
    static void run(void* context);
    static void keyboardReport(uint8_t modifiers, const uint8_t keys[6], void* context);
    static void mouseReport(int16_t x, int16_t y, uint8_t buttons, int8_t wheel, void* context);
};
extern UsbHostService usbHost;
