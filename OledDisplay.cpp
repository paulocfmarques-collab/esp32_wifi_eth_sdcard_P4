#include "OledDisplay.h"
#include "TimeManager.h"
#include <Wire.h>

OledDisplay::OledDisplay() 
  : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1), 
    _inicializado(false), _totalLinhas(0), _ultimoRelogioRefresh(0), 
    _ultimaHoraExibida("--:--:--"), _progressoAtual(0), _exibirProgresso(false) {}

void OledDisplay::inicializar() {
  Wire.begin(PIN_SDA, PIN_SCL); 
  if (_display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    _inicializado = true;
    _display.clearDisplay();
    _display.display();
    adicionarLinha("OLED Pronto!");
  }
}

void OledDisplay::adicionarLinha(const String& novoTexto) {
  Serial.println("[OLED Log] " + novoTexto);
  if (!_inicializado) return;

  if (_totalLinhas >= MAX_LINHAS_TERMINAL) {
    for (int i = 0; i < MAX_LINHAS_TERMINAL - 1; i++) {
      _historico[i] = _historico[i + 1];
    }
    _historico[MAX_LINHAS_TERMINAL - 1] = novoTexto;
  } else {
    _historico[_totalLinhas] = novoTexto;
    _totalLinhas++;
  }

  redesenharTelaCompleta(_ultimaHoraExibida);
}

void OledDisplay::configurarProgresso(bool ativar, int porcentagem) {
  _exibirProgresso = ativar;
  _progressoAtual = constrain(porcentagem, 0, 100);
  if (_inicializado) {
    redesenharTelaCompleta(_ultimaHoraExibida);
  }
}

void OledDisplay::atualizarRelogioSuperior(TimeManager& timeRef) {
  if (!_inicializado) return;
  unsigned long agora = millis();
  if (agora - _ultimoRelogioRefresh >= 1000) {
    _ultimoRelogioRefresh = agora;
    String horaAtual = timeRef.obterApenasHora();
    if (horaAtual != _ultimaHoraExibida) {
      _ultimaHoraExibida = horaAtual;
      redesenharTelaCompleta(_ultimaHoraExibida);
    }
  }
}

void OledDisplay::redesenharTelaCompleta(const String& horaAtual) {
  _display.clearDisplay();
  _display.setTextSize(1);
  _display.setTextColor(SSD1306_WHITE);
  
  // 1. Relógio no topo
  _display.setCursor(40, 1); 
  _display.print(horaAtual);
  
  // 2. Linha física horizontal superior
  _display.drawFastHLine(0, 11, SCREEN_WIDTH, SSD1306_WHITE);
  
  // 3. Histórico de comandos e respostas do terminal
  for (int i = 0; i < _totalLinhas; i++) {
    _display.setCursor(0, 14 + (i * 8)); 
    _display.print(_historico[i]);
  }
  
  // 4. NOVO: Desenho Geométrico da Barra de Progresso se ativa
  if (_exibirProgresso) {
    int barraLarguraTotal = SCREEN_WIDTH - 20; // 108 pixels de largura externa
    int xBarra = 10;
    int yBarra = 54;
    int hBarra = 8;
    
    // Desenha o contorno da caixa (Borda)
    _display.drawRoundRect(xBarra, yBarra, barraLarguraTotal, hBarra, 2, SSD1306_WHITE);
    
    // Calcula a largura de preenchimento proporcional à porcentagem
    int larguraPreenchimento = (barraLarguraTotal - 4) * _progressoAtual / 100;
    
    if (larguraPreenchimento > 0) {
      // Preeche a barra internamente com respiro de 2 pixels das bordas
      _display.fillRoundRect(xBarra + 2, yBarra + 2, larguraPreenchimento, hBarra - 4, 1, SSD1306_WHITE);
    }
  }
  
  _display.display();
}
