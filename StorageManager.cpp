#include "StorageManager.h"
#include "time.h"
#include "SystemLog.h"

StorageManager::StorageManager() : _sdcardOk(false)
{
}

bool StorageManager::salvarCrash(const String& relatorio, String& caminho, String& erro)
{
    ++_telemetry.crashErrors;
    caminho = "";
    erro = "";
    if (!_sdcardOk)
    {
        erro = "SD indisponivel";
        return false;
    }
    struct tm hora;
    char data[32];
    if (getLocalTime(&hora, 0))
        strftime(data, sizeof(data), "%Y%m%d_%H%M%S", &hora);
    else
        snprintf(data, sizeof(data), "SEM_NTP_%lu", (unsigned long)millis());
    for (unsigned i = 0; i < 1000; ++i)
    {
        caminho = "/crash_" + String(data) + (i ? "_" + String(i) : String()) + ".txt";
        if (!SD_MMC.exists(caminho))
            break;
        caminho = "";
    }
    if (caminho.isEmpty())
    {
        erro = "Nomes de arquivo esgotados";
        return false;
    }
    File arquivo = SD_MMC.open(caminho, FILE_WRITE);
    if (!arquivo || arquivo.isDirectory())
    {
        erro = "Falha ao criar " + caminho;
        return false;
    }
    String cabecalho = "Capturado em: " + String(data) + " | uptime=" + String(millis()) + "ms\n";
    const size_t esperado = cabecalho.length() + relatorio.length();
    size_t gravado = arquivo.print(cabecalho);
    gravado += arquivo.print(relatorio);
    arquivo.flush();
    arquivo.close();
    if (gravado != esperado)
    {
        erro = "Gravacao incompleta: " + caminho;
        return false;
    }
    // Match the central: verify all bytes from a reopened file before erasing flash.
    arquivo = SD_MMC.open(caminho, FILE_READ);
    bool ok = arquivo && !arquivo.isDirectory() && arquivo.size() == esperado;
    size_t posicao = 0;
    uint8_t bloco[128];
    while (ok && posicao < esperado)
    {
        size_t lidos = arquivo.read(bloco, min(sizeof(bloco), esperado - posicao));
        if (!lidos)
        {
            ok = false;
            break;
        }
        for (size_t i = 0; i < lidos; ++i, ++posicao)
        {
            char original = posicao < cabecalho.length() ? cabecalho[posicao]
                                                         : relatorio[posicao - cabecalho.length()];
            if (bloco[i] != uint8_t(original))
            {
                ok = false;
                break;
            }
        }
    }
    if (arquivo)
        arquivo.close();
    if (!ok)
    {
        erro = "Falha ao conferir " + caminho;
        return false;
    }
    --_telemetry.crashErrors;
    ++_telemetry.crashes;
    _nextSample = 0;
    return true;
}

void StorageManager::sampleTelemetry()
{
    uint64_t now = esp_timer_get_time();
    if (now < _nextSample) return;
    _nextSample = now + 30000000;
    _telemetry.sampledAt = now;
    _telemetry.mounted = false;
    _telemetry.logOk = false;
    if (!_sdcardOk) return;
    File root = SD_MMC.open("/", FILE_READ);
    bool readable = root && root.isDirectory();
    if (root) root.close();
    if (!readable) return;
    _telemetry.capacity = SD_MMC.cardSize();
    _telemetry.total = SD_MMC.totalBytes();
    _telemetry.used = SD_MMC.usedBytes();
    _telemetry.mounted = _telemetry.total > 0 && _telemetry.used <= _telemetry.total;
    if (!_telemetry.mounted) return;
    _telemetry.logExists = SD_MMC.exists("/log.txt");
    _telemetry.logSize = 0;
    _telemetry.logOk = !_telemetry.logExists;
    if (_telemetry.logExists) {
        File file = SD_MMC.open("/log.txt", FILE_READ);
        _telemetry.logOk = file && !file.isDirectory();
        if (_telemetry.logOk) _telemetry.logSize = file.size();
        if (file) file.close();
    }
}

bool StorageManager::inicializar()
{
    if (!SD_MMC.begin("/sdcard", false))
    {
        Serial.println(F("[SD] Falha ao montar o Cartao SDMMC!"));
        _sdcardOk = false;
    }
    else
    {
        uint8_t cardType = SD_MMC.cardType();
        if (cardType == CARD_NONE)
        {
            Serial.println(F("[SD] Nenhum tipo de cartao reconhecido."));
            _sdcardOk = false;
        }
        else
        {
            _sdcardOk = true;
        }
    }
    return _sdcardOk;
}

void StorageManager::gravarLog(const String& mensagem)
{
    if (!_sdcardOk) {
        ++_telemetry.logErrors;
        return;
    }

    struct tm timeinfo;
    char bufferCarimbo[25];

    if (getLocalTime(&timeinfo, 10))
    {
        sprintf(bufferCarimbo, "[%02d/%02d/%04d %02d:%02d:%02d] ", timeinfo.tm_mday,
                timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min,
                timeinfo.tm_sec);
    }
    else
    {
        sprintf(bufferCarimbo, "[Sem Hora Sinc.] ");
    }

    String safe = SystemLog::sanitize(mensagem);
    if (!rotateLog(strlen(bufferCarimbo) + safe.length() + 2)) { ++_telemetry.logErrors; return; }
    File arquivoLog = SD_MMC.open("/log.txt", FILE_APPEND);
    if (arquivoLog)
    {
        size_t written = arquivoLog.print(bufferCarimbo);
        written += arquivoLog.println(safe);
        if (written == strlen(bufferCarimbo) + safe.length() + 2)
            ++_telemetry.logWrites;
        else ++_telemetry.logErrors;
        arquivoLog.close();

        Serial.print(F("[SD LOG] "));
        Serial.print(bufferCarimbo);
        Serial.println(safe);
    } else ++_telemetry.logErrors;
}

bool StorageManager::rotateLog(size_t incoming) {
    File current = SD_MMC.open("/log.txt", FILE_READ);
    if (!current) return !SD_MMC.exists("/log.txt");
    size_t size = current.size(); current.close();
    if (size + incoming <= 1048576) return true;
    if (SD_MMC.exists("/log.4.txt") && !SD_MMC.remove("/log.4.txt")) return false;
    for (int i = 3; i >= 1; --i) {
        String from = "/log." + String(i) + ".txt", to = "/log." + String(i+1) + ".txt";
        if (SD_MMC.exists(from) && !SD_MMC.rename(from, to)) return false;
    }
    if (!SD_MMC.rename("/log.txt", "/log.1.txt")) return false;
    ++_telemetry.logRotations; return true;
}
