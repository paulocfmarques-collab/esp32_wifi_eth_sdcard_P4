#include "UsbHostService.h"
#include "SystemLog.h"
#include <ArduinoJson.h>
#include <USBHost.h>
#include <USBHostHIDKeyboard.h>
#include <USBHostHIDMouse.h>
#include <USBHostMSC.h>
#include <USBMSCFS.h>

UsbHostService usbHost;
#if CFG_TUH_ENABLED && CFG_TUH_HID && CFG_TUH_MSC
namespace {
USBHostHIDKeyboard keyboard;
USBHostHIDMouse mouse;
USBMSCFSClass volume;
}
#endif

void UsbHostService::begin() {
    _mutex = xSemaphoreCreateMutex();
    if (!_mutex || xTaskCreatePinnedToCore(run, "usb_devices", 8192, this, 1, &_task, 0) != pdPASS) {
        _state = 2; ++_errors; systemLog.record("USB", "falha ao criar tarefa/mutex");
    }
}

void UsbHostService::keyboardReport(uint8_t, const uint8_t[6], void* context) {
    ++static_cast<UsbHostService*>(context)->_keyboardReports;
    // Do not store key codes/text: a keyboard may also be used to enter secrets.
}

void UsbHostService::mouseReport(int16_t, int16_t, uint8_t, int8_t, void* context) {
    ++static_cast<UsbHostService*>(context)->_mouseReports;
}

void UsbHostService::run(void* context) {
    auto& self = *static_cast<UsbHostService*>(context);
#if CFG_TUH_ENABLED && CFG_TUH_HID && CFG_TUH_MSC
#if ARDUINO_USB_MODE == 0 && (ARDUINO_USB_CDC_ON_BOOT || ARDUINO_USB_MSC_ON_BOOT || ARDUINO_USB_DFU_ON_BOOT)
    self._state = 4; ++self._errors;
    systemLog.record("USB", "conflito USB device: selecionar Hardware CDC/JTAG e desativar funcoes OTG device no boot");
    vTaskDelete(nullptr); return;
#endif
    keyboard.registerWithHost(); keyboard.setNotifyOnChangeOnly(true);
    keyboard.setReportCallback(keyboardReport, &self);
    mouse.registerWithHost(); mouse.setReportCallback(mouseReport, &self);
    USBHost.setCore(0);
    if (!USBHost.begin()) {
        self._state = 5; ++self._errors; systemLog.record("USB", "USBHost.begin falhou");
        vTaskDelete(nullptr); return;
    }
    self._state = 1;
    systemLog.record("USB", "host HS iniciado; HID teclado/mouse e MSC; jumper DEVICE=hub, HOST=porta direta");
    bool mountAttempted = false;
    uint32_t summaryAt = millis() + 10000, previousKeys = 0, previousMouse = 0;
    for (;;) {
        USBHost.task(); // HID callbacks are dispatched here, never performing SD or OLED operations.
        bool keyPresent = keyboard.mounted(), mousePresent = mouse.mounted(), drivePresent = USBHostMSC.mounted();
        if (keyPresent != self._keyboard) { self._keyboard = keyPresent; systemLog.record("USB", "teclado %s", keyPresent ? "conectado" : "removido"); }
        if (mousePresent != self._mouse) { self._mouse = mousePresent; systemLog.record("USB", "mouse %s", mousePresent ? "conectado" : "removido"); }
        if (drivePresent != self._drive) {
            self._drive = drivePresent;
            systemLog.record("USB", "pendrive %s", drivePresent ? "conectado" : "removido");
            if (!drivePresent) {
                if (self._mounted.exchange(false)) volume.end();
                self._blocks = 0; self._blockSize = 0; mountAttempted = false;
                if (xSemaphoreTake(self._mutex, pdMS_TO_TICKS(10)) == pdTRUE) { self._files = "Pendrive ausente\n"; xSemaphoreGive(self._mutex); }
            }
        }
        if (drivePresent && !mountAttempted) {
            mountAttempted = true;
            self._blocks = USBHostMSC.blockCount(); self._blockSize = USBHostMSC.blockSize();
            bool ok = volume.begin("/usb", 4, false); // Never format a user's drive.
            self._mounted = ok;
            systemLog.record("USB", "FAT mount=%s blocos=%lu tamanho_bloco=%lu", ok ? "ok" : "falha", (unsigned long)self._blocks.load(), (unsigned long)self._blockSize.load());
            if (ok) self._rescan = true; else ++self._errors;
        }
        if (self._mounted && self._rescan.exchange(false)) {
            String listing; listing.reserve(4096);
            File root = volume.open("/"); unsigned count = 0;
            if (!root || !root.isDirectory()) { listing = "Falha ao ler raiz\n"; ++self._errors; }
            else {
                for (File file = root.openNextFile(); file && count < 32; file = root.openNextFile()) {
                    String name = file.name(); name = name.substring(0, 80);
                    listing += String(file.isDirectory() ? "DIR  " : "FILE ") + name + " " + String((unsigned long)file.size()) + " B\n";
                    ++count; file.close();
                }
                if (count == 32) listing += "Limite de 32 entradas; listagem pode estar truncada\n";
                if (!count) listing = "Raiz vazia\n";
            }
            root.close();
            if (xSemaphoreTake(self._mutex, pdMS_TO_TICKS(10)) == pdTRUE) { self._files = listing; xSemaphoreGive(self._mutex); }
            systemLog.record("USB", "leitura da raiz: %u entradas (maximo 32)", count);
        }
        if (int32_t(millis() - summaryAt) >= 0) {
            summaryAt = millis() + 10000;
            uint32_t keys = self._keyboardReports, mice = self._mouseReports;
            if (keys != previousKeys || mice != previousMouse)
                systemLog.record("USB", "atividade HID em 10s: teclado=%lu mouse=%lu (sem conteudo)", (unsigned long)(keys-previousKeys), (unsigned long)(mice-previousMouse));
            previousKeys = keys; previousMouse = mice;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
#else
    self._state = 3; ++self._errors;
    systemLog.record("USB", "pacote sem TinyUSB host/HID/MSC; use Arduino ESP32 3.3.12 configurado para P4");
    vTaskDelete(nullptr);
#endif
}

String UsbHostService::files() {
    if (!_mounted) return "Pendrive FAT nao montado; consulte usb_status\n";
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return "USB ocupado\n";
    String result = _files; xSemaphoreGive(_mutex); return result;
}

String UsbHostService::status() {
    JsonDocument doc;
    doc["state"] = _state.load(); doc["ready"] = ready();
    doc["keyboard"] = _keyboard.load(); doc["mouse"] = _mouse.load();
    doc["drive_connected"] = _drive.load(); doc["fat_mounted"] = _mounted.load();
    doc["keyboard_reports"] = _keyboardReports.load(); doc["mouse_reports"] = _mouseReports.load(); doc["errors"] = _errors.load();
    doc["drive_bytes"] = uint64_t(_blocks.load()) * _blockSize.load();
    doc["jumper"] = "DEVICE=portas 2-4 via hub; HOST=porta 1 direta";
    String json; serializeJson(doc, json); return json;
}
