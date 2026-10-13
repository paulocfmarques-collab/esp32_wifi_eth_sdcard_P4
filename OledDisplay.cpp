#include "OledDisplay.h"
#include "TimeManager.h"
#include <Wire.h>
#include "NetworkController.h"
#include "StorageManager.h"

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

void OledDisplay::adicionarLinha(const String& novoTexto, bool interacao) {
  Serial.println("[OLED Log] " + novoTexto);
  if (interacao) acordar();
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

  _consoleAte = millis() + 5000;
  if (!_protecaoAtiva && !_telaApagada) redesenharTelaCompleta(_ultimaHoraExibida);
}

void OledDisplay::configurarProgresso(bool ativar, int porcentagem) {
  acordar();
  _exibirProgresso = ativar;
  _progressoAtual = constrain(porcentagem, 0, 100);
  if (_inicializado) {
    _consoleAte = millis() + 5000;
    redesenharTelaCompleta(_ultimaHoraExibida);
  }
}

void OledDisplay::atualizarRelogioSuperior(TimeManager& timeRef) {
  if (!_inicializado || _protecaoAtiva || _telaApagada) return;
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
  if (!_inicializado || _protecaoAtiva || _telaApagada) return;
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

void OledDisplay::selecionarPagina(uint8_t pagina) {
 acordar();
 _pagina=pagina;_autoPaginas=false;_consoleAte=0;_ultimoPainel=millis()-1000;
}
void OledDisplay::avancarPagina(){selecionarPagina((_pagina+1)%6);}
void OledDisplay::paginasAutomaticas(){acordar();_autoPaginas=true;_ultimaPagina=millis();_consoleAte=0;_ultimoPainel=millis()-1000;}
void OledDisplay::atualizarPainel(TimeManager& hora,NetworkController& rede,StorageManager& sd) {
 if(!_inicializado)return;
 uint32_t now=millis();
 if(atualizarProtecao(hora, now))return;
 if(_exibirProgresso || int32_t(_consoleAte-now)>0)return;
 if(now-_ultimoPainel<1000)return;_ultimoPainel=now;
 if(_autoPaginas && now-_ultimaPagina>=8000){_pagina=(_pagina+1)%6;_ultimaPagina=now;}
 _display.clearDisplay();_display.setTextSize(1);_display.setTextWrap(false);_display.setTextColor(SSD1306_WHITE);
 const char* titles[]={"RELOGIO","WI-FI","ETHERNET","SISTEMA / SD","PING GATEWAY","HISTORICO REDE"};
 _display.setCursor(0,0);_display.print(titles[_pagina]);_display.setCursor(110,0);_display.printf("%u/6",_pagina+1);
 _display.drawFastHLine(0,10,128,SSD1306_WHITE);
 auto line=[&](int row,const String& text){_display.setCursor(0,14+row*10);_display.print(text.substring(0,21));};
 const auto& m=rede.monitor();
 if(_pagina==0){
  String dt=hora.obterStringDataHora();
  _display.setTextSize(2);_display.setCursor(15,18);_display.print(dt=="Sem Hora Sinc." ? "--:--:--" : hora.obterApenasHora());
  _display.setTextSize(1);line(3,dt=="Sem Hora Sinc." ? dt : dt.substring(0,10));
  line(4,String("WiFi:")+(m.links[0].online?"OK":"--")+" ETH:"+(m.links[1].online?"OK":"--"));
 } else if(_pagina==1){
  bool ok=m.links[0].online;
  line(0,ok?WiFi.SSID():WiFi.getMode()==WIFI_AP?String("AP ESP32_P4_CONFIG"):String("WiFi desconectado"));
  line(1,WiFi.getMode()==WIFI_AP?WiFi.softAPIP().toString():WiFi.localIP().toString());
  line(2,"GW: "+WiFi.gatewayIP().toString());line(3,ok?"RSSI: "+String(WiFi.RSSI())+" dBm":"RSSI: --");
  line(4,"Canal: "+String(WiFi.channel()));
  int bars=ok?constrain((WiFi.RSSI()+100)/10,0,4):0;
  for(int i=0;i<4;i++){int h=3+i*2,x=104+i*6;_display.drawRect(x,52-h,4,h,SSD1306_WHITE);if(i<bars)_display.fillRect(x,52-h,4,h,SSD1306_WHITE);}
 } else if(_pagina==2){
  line(0,m.links[1].online?"ETH conectado":"ETH desconectado");line(1,"IP: "+ETH.localIP().toString());
  line(2,"GW: "+ETH.gatewayIP().toString());line(3,"Quedas: "+String(m.links[1].drops));
  line(4,"Offline: "+String((unsigned long)(m.links[1].offlineMs/1000))+" s");
 } else if(_pagina==3){
  line(0,String(ESP.getChipModel())+" "+String(ESP.getCpuFreqMHz())+"MHz");
  line(1,"RAM: "+String(ESP.getFreeHeap()/1024)+" KB");line(2,"Min RAM: "+String(ESP.getMinFreeHeap()/1024)+" KB");
  line(3,sd.isAtivo()?"SD: OK":"SD: indisponivel");line(4,"Ligado: "+String(now/1000)+" s");
 } else if(_pagina==4){
  for(int i=0;i<2;i++){
   const auto& l=m.links[i];String value=!l.online?String("sem rede"):!l.result?String("aguardando"):l.replied?String(l.rtt)+" ms":String("timeout");
   line(i,String(i==0?"WiFi: ":"ETH: ")+value);
  }
  uint8_t i=(now/8000)%2;const auto& l=m.links[i];
  line(2,String(i==0?"WiFi ":"ETH ")+"falha: "+String(l.sent?uint32_t(uint64_t(l.lost)*100/l.sent):0)+"%");
  if(l.result && (now/4000)%2) {
   _display.fillRect(0,34,128,8,SSD1306_BLACK);
   line(2,String(i==0?"WiFi ":"ETH ")+"amostra ha "+String(uint32_t(now-l.measuredAt)/1000)+"s");
  }
  int peak=10;for(int v:l.samples)if(v>peak)peak=v;
  for(int j=0;j<24;j++){int v=l.samples[(l.head+j)%24],x=4+j*5;
   if(v==-1){_display.drawLine(x,48,x+3,51,SSD1306_WHITE);_display.drawLine(x+3,48,x,51,SSD1306_WHITE);}
   else if(v>=0){int h=max(1,v*15/peak);_display.drawFastVLine(x,63-h,h,SSD1306_WHITE);}
  }
 } else {for(int i=0;i<4;i++)line(i,m.events[i]);line(4,"Desde o boot (RAM)");}
 _display.display();
}

bool OledDisplay::acordar() {
  const bool estavaProtegida = _protecaoAtiva || _telaApagada;
  _ultimaInteracao = millis();
  if (_inicializado && estavaProtegida) {
    _display.ssd1306_command(SSD1306_DISPLAYON);
    _display.dim(false);
  }
  _protecaoAtiva = _telaApagada = false;
  if (estavaProtegida) {
    _consoleAte = 0;
    _ultimoPainel = millis() - 1000;
    _ultimaPagina = millis();
  }
  return estavaProtegida;
}

bool OledDisplay::iniciarProtecao() {
  if (!_inicializado || _exibirProgresso) return false;
  acordar();
  // Backdate activity so the same timers govern manual and automatic protection.
  _ultimaInteracao = millis() - _tempoProtecao;
  _protecaoAtiva = true;
  _consoleAte = 0;
  _ultimoQuadroProtecao = millis() - 1000;
  _display.dim(true);
  return true;
}

void OledDisplay::habilitarProtecao(bool habilitada) {
  _protecaoHabilitada = habilitada;
  acordar();
}

void OledDisplay::configurarTempoProtecao(uint32_t segundos) {
  _tempoProtecao = segundos * 1000;
  _protecaoHabilitada = true;
  acordar();
}

String OledDisplay::statusProtecao() const {
  return "Protecao automatica: " + String(_protecaoHabilitada ? "ON" : "OFF") +
    "\nEstado: " + String(!_inicializado ? "OLED indisponivel" : _telaApagada ? "OLED apagado" : _protecaoAtiva ? "relogio em movimento" : "painel") +
    "\nInatividade: " + String(_tempoProtecao / 1000) + " s\nOLED apaga 240 s depois de iniciar a protecao\n";
}

bool OledDisplay::atualizarProtecao(TimeManager& hora, uint32_t now) {
  if (_exibirProgresso) return false;
  const uint32_t parado = uint32_t(now - _ultimaInteracao);
  if (!_protecaoAtiva && _protecaoHabilitada && parado >= _tempoProtecao) {
    _protecaoAtiva = true;
    _consoleAte = 0;
    _ultimoQuadroProtecao = now - 1000;
    _display.dim(true);
  }
  if (!_protecaoAtiva) return false;
  if (!_telaApagada && parado >= _tempoProtecao + 240000UL) {
    _display.clearDisplay();
    _display.display();
    _display.ssd1306_command(SSD1306_DISPLAYOFF);
    _telaApagada = true;
  }
  if (!_telaApagada && uint32_t(now - _ultimoQuadroProtecao) >= 1000) desenharProtecao(hora, now);
  return true;
}

void OledDisplay::desenharProtecao(TimeManager& hora, uint32_t now) {
  _ultimoQuadroProtecao = now;
  // Eight characters in the built-in 6x8 font; keep the entire clock on screen.
  _protecaoX += _protecaoDx;
  _protecaoY += _protecaoDy;
  if (_protecaoX <= 0 || _protecaoX >= SCREEN_WIDTH - 48) _protecaoDx = -_protecaoDx;
  if (_protecaoY <= 0 || _protecaoY >= SCREEN_HEIGHT - 8) _protecaoDy = -_protecaoDy;
  _display.clearDisplay();
  _display.setTextSize(1);
  _display.setTextWrap(false);
  _display.setTextColor(SSD1306_WHITE);
  _display.setCursor(_protecaoX, _protecaoY);
  _display.print(hora.obterApenasHora());
  _display.display();
}
