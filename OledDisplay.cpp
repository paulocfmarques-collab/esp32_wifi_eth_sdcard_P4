#include "OledDisplay.h"
#include "TimeManager.h"
#include <Wire.h>
#include "NetworkController.h"
#include "StorageManager.h"

OledDisplay::OledDisplay()
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1), _inicializado(false), _totalLinhas(0),
      _ultimoRelogioRefresh(0), _ultimaHoraExibida("--:--:--"), _progressoAtual(0),
      _exibirProgresso(false)
{
}

void OledDisplay::inicializar()
{
    Wire.begin(PIN_SDA, PIN_SCL);
    _ultimaAtividade = millis();
    if (_display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        _inicializado = true;
        _display.clearDisplay();
        _display.display();
        adicionarLinha("OLED Pronto!");
    }
}

void OledDisplay::adicionarLinha(const String& novoTexto)
{
    Serial.println("[OLED Log] " + novoTexto);
    if (!_inicializado)
        return;

    if (_totalLinhas >= MAX_LINHAS_TERMINAL)
    {
        for (int i = 0; i < MAX_LINHAS_TERMINAL - 1; i++)
        {
            _historico[i] = _historico[i + 1];
        }
        _historico[MAX_LINHAS_TERMINAL - 1] = novoTexto;
    }
    else
    {
        _historico[_totalLinhas] = novoTexto;
        _totalLinhas++;
    }

    _consoleAte = millis() + 5000;
    redesenharTelaCompleta(_ultimaHoraExibida);
}

void OledDisplay::configurarProgresso(bool ativar, int porcentagem)
{
    if (ativar) registrarAtividade();
    _exibirProgresso = ativar;
    _progressoAtual = constrain(porcentagem, 0, 100);
    if (_inicializado)
    {
        _consoleAte = millis() + 5000;
        redesenharTelaCompleta(_ultimaHoraExibida);
    }
}

void OledDisplay::atualizarRelogioSuperior(TimeManager& timeRef)
{
    if (!_inicializado || _protetorAtivo)
        return;
    unsigned long agora = millis();
    if (agora - _ultimoRelogioRefresh >= 1000)
    {
        _ultimoRelogioRefresh = agora;
        String horaAtual = timeRef.obterApenasHora();
        if (horaAtual != _ultimaHoraExibida)
        {
            _ultimaHoraExibida = horaAtual;
            redesenharTelaCompleta(_ultimaHoraExibida);
        }
    }
}

void OledDisplay::redesenharTelaCompleta(const String& horaAtual)
{
    if (!_inicializado || _protetorAtivo)
        return;
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    // 1. Relógio no topo
    _display.setCursor(40, 1);
    _display.print(horaAtual);

    // 2. Linha física horizontal superior
    _display.drawFastHLine(0, 11, SCREEN_WIDTH, SSD1306_WHITE);

    // 3. Histórico de comandos e respostas do terminal
    for (int i = 0; i < _totalLinhas; i++)
    {
        _display.setCursor(0, 14 + (i * 8));
        _display.print(_historico[i]);
    }

    // 4. NOVO: Desenho Geométrico da Barra de Progresso se ativa
    if (_exibirProgresso)
    {
        int barraLarguraTotal = SCREEN_WIDTH - 20; // 108 pixels de largura externa
        int xBarra = 10;
        int yBarra = 54;
        int hBarra = 8;

        // Desenha o contorno da caixa (Borda)
        _display.drawRoundRect(xBarra, yBarra, barraLarguraTotal, hBarra, 2, SSD1306_WHITE);

        // Calcula a largura de preenchimento proporcional à porcentagem
        int larguraPreenchimento = (barraLarguraTotal - 4) * _progressoAtual / 100;

        if (larguraPreenchimento > 0)
        {
            // Preeche a barra internamente com respiro de 2 pixels das bordas
            _display.fillRoundRect(xBarra + 2, yBarra + 2, larguraPreenchimento, hBarra - 4, 1,
                                   SSD1306_WHITE);
        }
    }

    _display.display();
}

void OledDisplay::selecionarPagina(uint8_t pagina)
{
    if (pagina >= 6) return;
    bool mudou = pagina != _pagina || _protetorAtivo;
    registrarAtividade();
    _pagina = pagina;
    _autoPaginas = false;
    _consoleAte = 0;
    _ultimoPainel = millis() - 1000;
    _ultimaPagina = millis();
    if (mudou && _aoTrocarPagina) _aoTrocarPagina();
}

void OledDisplay::avancarPagina()
{
    selecionarPagina((_pagina + 1) % 6);
    paginasAutomaticas();
}

void OledDisplay::paginasAutomaticas()
{
    registrarAtividade();
    _autoPaginas = true;
    _ultimaPagina = millis();
    _consoleAte = 0;
    _ultimoPainel = millis() - 1000;
}

void OledDisplay::registrarAtividade()
{
    _ultimaAtividade = millis();
    if (_protetorAtivo) {
        _protetorAtivo = false;
        _consoleAte = 0;
        _ultimaPagina = millis();
        _ultimoPainel = millis() - 1000;
    }
}

void OledDisplay::iniciarProtetor()
{
    if (!_inicializado || _exibirProgresso || _protetorAtivo) return;
    _protetorAtivo = true; _frameRobo = 0; _ultimoRobo = millis() - 120;
}

void OledDisplay::configurarProtetor(bool automatico)
{
    _protetorAutomatico = automatico;
    registrarAtividade();
}

String OledDisplay::statusProtetor() const
{
    return String("Protetor robo: ") + (_protetorAtivo ? "ativo" : "inativo") +
        "\nAutomatico: " + (_protetorAutomatico ? "sim" : "nao") +
        "\nInatividade: " + String(_tempoProtetor / 1000) + " s\n";
}

void OledDisplay::desenharRobo(uint32_t now)
{
    if (now - _ultimoRobo < 120) return;
    _ultimoRobo = now;
    uint32_t frame = _frameRobo++;
    uint8_t expressao = (frame / 40) % 4;
    bool piscar = frame % 32 < 2;
    int dx = int((frame / 8) % 7) - 3;
    int dy = int((frame / 16) % 3) - 1;
    _display.clearDisplay();
    // Entire 128x64 canvas: no clock, terminal or fixed header over the robot.
    _display.drawRoundRect(1 + (dx + 3) / 3, 1 + (dy + 1), 123, 60, 9, SSD1306_WHITE);
    for (int x : {19 + dx, 76 + dx}) {
        if (piscar) _display.fillRoundRect(x, 28 + dy, 32, 3, 1, SSD1306_WHITE);
        else if (expressao == 1) {
            _display.fillRoundRect(x, 18 + dy, 32, 22, 8, SSD1306_WHITE);
            _display.fillRoundRect(x - 1, 25 + dy, 34, 18, 7, SSD1306_BLACK);
        } else if (expressao == 2) {
            _display.fillRoundRect(x, 29 + dy, 32, 8, 3, SSD1306_WHITE);
        } else {
            int h = expressao == 3 ? 30 : 24;
            _display.fillRoundRect(x, 14 + dy, 32, h, 8, SSD1306_WHITE);
            _display.fillRoundRect(x + 11 + dx, 20 + dy, 10, h - 10, 3, SSD1306_BLACK);
        }
    }
    if (expressao == 3) _display.drawRoundRect(58 + dx, 46 + dy, 12, 11, 5, SSD1306_WHITE);
    else {
        _display.drawFastHLine(50 + dx, 51 + dy, 28, SSD1306_WHITE);
        if (expressao == 1) {
            _display.drawLine(46 + dx, 47 + dy, 50 + dx, 51 + dy, SSD1306_WHITE);
            _display.drawLine(78 + dx, 51 + dy, 82 + dx, 47 + dy, SSD1306_WHITE);
        }
    }
    _display.display();
}

void OledDisplay::atualizarPainel(TimeManager& hora, NetworkController& rede, StorageManager& sd)
{
    if (!_inicializado)
        return;
    uint32_t now = millis();
    if (!_exibirProgresso && _protetorAutomatico && now - _ultimaAtividade >= _tempoProtetor)
        iniciarProtetor();
    if (_protetorAtivo) { desenharRobo(now); return; }
    if (_exibirProgresso || int32_t(_consoleAte - now) > 0)
        return;
    if (now - _ultimoPainel < 1000)
        return;
    _ultimoPainel = now;
    if (_autoPaginas && now - _ultimaPagina >= 8000)
    {
        _pagina = (_pagina + 1) % 6;
        _ultimaPagina = now;
        if (_aoTrocarPagina) _aoTrocarPagina();
    }
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextWrap(false);
    _display.setTextColor(SSD1306_WHITE);
    const char* titles[] = {"RELOGIO",      "WI-FI",        "ETHERNET",
                            "SISTEMA / SD", "PING GATEWAY", "HISTORICO REDE"};
    _display.setCursor(0, 0);
    _display.print(titles[_pagina]);
    _display.setCursor(110, 0);
    _display.printf("%u/6", _pagina + 1);
    _display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    auto line = [&](int row, const String& text)
    {
        _display.setCursor(0, 14 + row * 10);
        _display.print(text.substring(0, 21));
    };
    const auto& m = rede.monitor();
    if (_pagina == 0)
    {
        String dt = hora.obterStringDataHora();
        _display.setTextSize(2);
        _display.setCursor(15, 18);
        _display.print(dt == "Sem Hora Sinc." ? "--:--:--" : hora.obterApenasHora());
        _display.setTextSize(1);
        line(3, dt == "Sem Hora Sinc." ? dt : dt.substring(0, 10));
        line(4, String("WiFi:") + (m.links[0].online ? "OK" : "--") +
                    " ETH:" + (m.links[1].online ? "OK" : "--"));
    }
    else if (_pagina == 1)
    {
        bool ok = m.links[0].online;
        line(0, ok                          ? WiFi.SSID()
                : WiFi.getMode() == WIFI_AP ? String("AP ESP32_P4_CONFIG")
                                            : String("WiFi desconectado"));
        line(1, WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
        line(2, "GW: " + WiFi.gatewayIP().toString());
        line(3, ok ? "RSSI: " + String(WiFi.RSSI()) + " dBm" : "RSSI: --");
        line(4, "Canal: " + String(WiFi.channel()));
        int bars = ok ? constrain((WiFi.RSSI() + 100) / 10, 0, 4) : 0;
        for (int i = 0; i < 4; i++)
        {
            int h = 3 + i * 2, x = 104 + i * 6;
            _display.drawRect(x, 52 - h, 4, h, SSD1306_WHITE);
            if (i < bars)
                _display.fillRect(x, 52 - h, 4, h, SSD1306_WHITE);
        }
    }
    else if (_pagina == 2)
    {
        line(0, m.links[1].online ? "ETH conectado" : "ETH desconectado");
        line(1, "IP: " + ETH.localIP().toString());
        line(2, "GW: " + ETH.gatewayIP().toString());
        line(3, "Quedas: " + String(m.links[1].drops));
        line(4, "Offline: " + String((unsigned long)(m.links[1].offlineMs / 1000)) + " s");
    }
    else if (_pagina == 3)
    {
        line(0, String(ESP.getChipModel()) + " " + String(ESP.getCpuFreqMHz()) + "MHz");
        line(1, "RAM: " + String(ESP.getFreeHeap() / 1024) + " KB");
        line(2, "Min RAM: " + String(ESP.getMinFreeHeap() / 1024) + " KB");
        line(3, sd.isAtivo() ? "SD: OK" : "SD: indisponivel");
        line(4, "Ligado: " + String(now / 1000) + " s");
    }
    else if (_pagina == 4)
    {
        for (int i = 0; i < 2; i++)
        {
            const auto& l = m.links[i];
            String value = !l.online   ? String("sem rede")
                           : !l.result ? String("aguardando")
                           : l.replied ? String(l.rtt) + " ms"
                                       : String("timeout");
            line(i, String(i == 0 ? "WiFi: " : "ETH: ") + value);
        }
        uint8_t i = (now / 8000) % 2;
        const auto& l = m.links[i];
        line(2, String(i == 0 ? "WiFi " : "ETH ") + "falha: " +
                    String(l.sent ? uint32_t(uint64_t(l.lost) * 100 / l.sent) : 0) + "%");
        if (l.result && (now / 4000) % 2)
        {
            _display.fillRect(0, 34, 128, 8, SSD1306_BLACK);
            line(2, String(i == 0 ? "WiFi " : "ETH ") + "amostra ha " +
                        String(uint32_t(now - l.measuredAt) / 1000) + "s");
        }
        int peak = 10;
        for (int v : l.samples)
            if (v > peak)
                peak = v;
        for (int j = 0; j < 24; j++)
        {
            int v = l.samples[(l.head + j) % 24], x = 4 + j * 5;
            if (v == -1)
            {
                _display.drawLine(x, 48, x + 3, 51, SSD1306_WHITE);
                _display.drawLine(x + 3, 48, x, 51, SSD1306_WHITE);
            }
            else if (v >= 0)
            {
                int h = max(1, v * 15 / peak);
                _display.drawFastVLine(x, 63 - h, h, SSD1306_WHITE);
            }
        }
    }
    else
    {
        for (int i = 0; i < 4; i++)
            line(i, m.events[i]);
        line(4, "Desde o boot (RAM)");
    }
    _display.display();
}
