// SPDX-License-Identifier: Apache-2.0
#include "PageAudio.h"
#include "SystemLog.h"
#include "DeviceConfig.h"
#include <Wire.h>
#include <math.h>

PageAudio pageAudio;
static constexpr uint32_t sampleRate = 48000;

bool PageAudio::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(AUDIO_CODEC_ADDRESS);
    Wire.write(reg); Wire.write(value);
    if (Wire.endTransmission() == 0) return true;
    Serial.printf("[AUDIO] Falha no registrador ES8311 0x%02x\n", reg);
    return false;
}

bool PageAudio::readRegister(uint8_t reg, uint8_t& value) {
    // Called only by setup / command execution on the main application task.
    Wire.beginTransmission(AUDIO_CODEC_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(uint8_t(AUDIO_CODEC_ADDRESS), size_t(1)) != 1)
        return false;
    value = uint8_t(Wire.read());
    return true;
}

void PageAudio::fail(esp_err_t error) {
    systemLog.record("AUDIO", "falha codigo=%d", int(error));
    _error.store(error); _ready.store(false);
    digitalWrite(AUDIO_AMP_ENABLE, LOW);
    Serial.printf("[AUDIO] Indisponivel: %s\n", esp_err_to_name(error));
}

void PageAudio::begin() {
    pinMode(AUDIO_AMP_ENABLE, OUTPUT);
    digitalWrite(AUDIO_AMP_ENABLE, LOW);
    // Reuse OLED's Wire on GPIO7/8. No second bus initialization or worker I2C access.
    Wire.beginTransmission(AUDIO_CODEC_ADDRESS);
    if (Wire.endTransmission() != 0) { fail(ESP_ERR_NOT_FOUND); return; }
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    channel.dma_desc_num = 4; channel.dma_frame_num = 128;
    channel.auto_clear = true;
    esp_err_t error = i2s_new_channel(&channel, &_tx, nullptr);
    if (error != ESP_OK) { fail(error); return; }
    i2s_std_config_t config = {};
    config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
    // Match the Module-DEV-KIT ESP-IDF example; send the tone in both slots.
    config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    config.gpio_cfg.mclk = gpio_num_t(AUDIO_MCLK);
    config.gpio_cfg.bclk = gpio_num_t(AUDIO_BCLK);
    config.gpio_cfg.ws = gpio_num_t(AUDIO_LRCK);
    config.gpio_cfg.dout = gpio_num_t(AUDIO_DOUT);
    config.gpio_cfg.din = I2S_GPIO_UNUSED;
    error = i2s_channel_init_std_mode(_tx, &config);
    if (error == ESP_OK) error = i2s_channel_enable(_tx);
    if (error != ESP_OK) { i2s_del_channel(_tx); _tx = nullptr; fail(error); return; }
    // Reset the codec as well as the MCU; its power is retained across MCU resets.
    bool resetOk = writeRegister(0x00, 0x1F);
    delay(20); // Setup only, before the web/application task starts.
    resetOk = resetOk && writeRegister(0x00, 0x00) && writeRegister(0x00, 0x80);
    if (!resetOk) {
        i2s_channel_disable(_tx); i2s_del_channel(_tx); _tx = nullptr;
        fail(ESP_FAIL); return;
    }
    // 48 kHz, MCLK=256*Fs, 16-bit I2S. Register setup follows Waveshare's
    // ES8311 Arduino example: examples/Arduino/i2s/ES8311.cpp (see README).
    const uint8_t registers[][2] = {
        {0x0D,0xFA},{0x44,0x08},{0x44,0x08},{0x01,0x30},{0x02,0x00},
        {0x03,0x10},{0x16,0x24},{0x04,0x10},{0x05,0x00},{0x0B,0x00},
        {0x0C,0x00},{0x10,0x1F},{0x11,0x7F},{0x00,0x80},{0x01,0x3F},
        {0x06,0x00},{0x13,0x10},{0x1B,0x0A},{0x1C,0x6A},{0x44,0x08},
        {0x02,0x00},{0x03,0x10},{0x04,0x10},{0x05,0x00},{0x06,0x03},
        {0x07,0x00},{0x08,0xFF},{0x09,0x0C},{0x0A,0x0C},
        {0x17,0xBF},{0x0E,0x02},{0x12,0x00},{0x14,0x1A},
        {0x0D,0x01},{0x15,0x40},{0x37,0x08},{0x45,0x00},
        {0x32,0xE8}
    };
    for (const auto& entry : registers) {
        if (!writeRegister(entry[0], entry[1])) {
            i2s_channel_disable(_tx); i2s_del_channel(_tx); _tx = nullptr;
            fail(ESP_FAIL); return;
        }
    }
    uint8_t mute = 0, format = 0, dacPower = 0;
    if (!readRegister(0x31, mute) || !writeRegister(0x31, mute & 0x9F) ||
        !readRegister(0x09, format) || (format & 0x1F) != 0x0C ||
        !readRegister(0x12, dacPower) || dacPower != 0x00) {
        i2s_channel_disable(_tx); i2s_del_channel(_tx); _tx = nullptr;
        fail(ESP_FAIL); return;
    }
    if (xTaskCreatePinnedToCore(run, "page_audio", 4096, this, 1, &_task, 0) != pdPASS) {
        i2s_channel_disable(_tx); i2s_del_channel(_tx); _tx = nullptr;
        fail(ESP_ERR_NO_MEM); return;
    }
    _ready.store(true);
    Serial.println("[AUDIO] ES8311 pronto: 48 kHz; som de navegacao habilitado.");
}

bool PageAudio::play(bool diagnostic) {
    if (!_ready.load() || _suspended.load() || !_task) return false;
    if (diagnostic) _diagnostic.store(true);
    _requested.fetch_add(1);
    xTaskNotifyGive(_task);
    return true;
}

void PageAudio::run(void* context) {
    auto& audio = *static_cast<PageAudio*>(context);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Coalesce rapid presses; never queue an unbounded tune.
        bool diagnostic = audio._diagnostic.exchange(false);
        if (audio._ready.load() && !audio._suspended.load()) audio.tone(diagnostic);
    }
}

bool PageAudio::writeSamples(const int16_t* samples, size_t bytes) {
    size_t offset = 0;
    uint32_t started = millis();
    while (offset < bytes) {
        if (_suspended.load()) return false;
        size_t written = 0;
        esp_err_t error = i2s_channel_write(_tx, reinterpret_cast<const uint8_t*>(samples) + offset,
                                           bytes - offset, &written, 50);
        offset += written;
        _lastBytes.fetch_add(written);
        if (error != ESP_OK && error != ESP_ERR_TIMEOUT) {
            _error.store(error); _failures.fetch_add(1); return false;
        }
        if (offset == bytes) return true;
        if (uint32_t(millis() - started) >= 250) {
            _error.store(ESP_ERR_TIMEOUT); _failures.fetch_add(1); return false;
        }
        vTaskDelay(1);
    }
    return true;
}

void PageAudio::tone(bool diagnostic) {
    const size_t count = sampleRate * (diagnostic ? 500 : 80) / 1000;
    const float frequency = diagnostic ? 880.0f : 1200.0f;
    int16_t samples[128 * 2] = {};
    _lastBytes.store(0);
    _error.store(ESP_OK);
    digitalWrite(AUDIO_AMP_ENABLE, HIGH);
    bool ok = true;
    // Clocked silence lets the NS4150B settle BEFORE a short tone starts.
    for (size_t n = 0; n < sampleRate / 10; n += 128) {
        if (!writeSamples(samples, sizeof(samples))) { ok = false; break; }
    }
    for (size_t offset = 0; ok && offset < count; offset += 128) {
        if (_suspended.load()) break;
        for (size_t j = 0; j < 128; ++j) {
            size_t n = offset + j;
            float envelope = n < count ? min(1.0f, min(n / 480.0f, (count - n) / 480.0f)) : 0;
            int16_t value = int16_t(4096 * envelope * sinf(2 * PI * frequency * n / sampleRate));
            samples[j * 2] = value; samples[j * 2 + 1] = value;
        }
        ok = writeSamples(samples, sizeof(samples));
    }
    memset(samples, 0, sizeof(samples));
    for (int n = 0; ok && n < 12; ++n) ok = writeSamples(samples, sizeof(samples));
    vTaskDelay(pdMS_TO_TICKS(20)); // Drain the final DMA silence, only in the audio task.
    digitalWrite(AUDIO_AMP_ENABLE, LOW);
    if (ok && !_suspended.load()) _completed.fetch_add(1);
    else if (_error.load() != ESP_OK) {
        Serial.printf("[AUDIO] Falha de reproducao: %s; proxima solicitacao pode tentar novamente.\n",
                      esp_err_to_name(esp_err_t(_error.load())));
    }
}

String PageAudio::status() {
    String result = String("Audio: ") + (_ready.load() ? "inicializado" : "indisponivel") +
        "\nCodec: ES8311 I2C 0x18, SDA/SCL " + String(PIN_SDA) + "/" + String(PIN_SCL) +
        "\nMCLK/BCLK/LRCK/DOUT: " + String(AUDIO_MCLK) + "/" + String(AUDIO_BCLK) + "/" +
        String(AUDIO_LRCK) + "/" + String(AUDIO_DOUT) + "\nI2S: 48000 Hz, 16 bits, slots L+R\nAmplificador GPIO" +
        String(AUDIO_AMP_ENABLE) + ": " + String(digitalRead(AUDIO_AMP_ENABLE)) +
        "\nErro: " + esp_err_to_name(esp_err_t(_error.load())) +
        "\nSolicitacoes: " + String(_requested.load()) + "\nEnvios completos: " + String(_completed.load()) +
        "\nFalhas de escrita: " + String(_failures.load()) + "\nBytes no ultimo envio: " + String(_lastBytes.load()) + "\n";
    for (uint8_t reg : {0x00, 0x01, 0x09, 0x12, 0x13, 0x31, 0x32, 0xFD, 0xFE}) {
        uint8_t value = 0;
        char line[28];
        if (readRegister(reg, value)) snprintf(line, sizeof(line), "Reg 0x%02X = 0x%02X\n", reg, value);
        else snprintf(line, sizeof(line), "Reg 0x%02X: falha I2C\n", reg);
        result += line;
    }
    return result;
}
