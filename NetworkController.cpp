#include "NetworkController.h"
#include "PortalUi.h"
#include <ctype.h>

NetworkController* NetworkController::_instance = nullptr;
bool NetworkController::ethConectado = false;

NetworkController::NetworkController(OledDisplay& oledRef, StorageManager& storageRef)
  : _server(80), _oled(oledRef), _storage(storageRef), _metrics(storageRef), _dadosProntosParaSalvar(false), _rotasOk(false) {
  _instance = this;
}

void NetworkController::inicializar()
 {
  _networkEvents = xQueueCreate(16, sizeof(arduino_event_id_t));
  WiFi.onEvent(onNetworkEvent);
  _metrics.begin();
}

void NetworkController::onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (!_instance) return;
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) _instance->_metrics.wifiDisconnected();
  if (event == ARDUINO_EVENT_ETH_START) ETH.setHostname("esp32-p4-node");
  // I2C and String history belong to the main loop, never to the event task.
  if (_instance->_networkEvents) xQueueSend(_instance->_networkEvents, &event, 0);
}

static String esc(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else if (c == '\'') o += "&#39;";
    else o += c;
  }
  return o;
}

void NetworkController::carregarRedes(String ssids[NUM_REDES], String senhas[NUM_REDES], int& idx) {
  _prefs.begin("wifi", false);
  String antigo = _prefs.getString("ssid", "");
  if (antigo != "" && _prefs.getString("s0", "") == "") {
    _prefs.putString("s0", antigo);
    _prefs.putString("p0", _prefs.getString("senha", ""));
  }
  if (antigo != "") { _prefs.remove("ssid"); _prefs.remove("senha"); }
  for (int i = 0; i < NUM_REDES; i++) {
    ssids[i]  = _prefs.getString(("s" + String(i)).c_str(), "");
    senhas[i] = _prefs.getString(("p" + String(i)).c_str(), "");
  }
  idx = _prefs.getInt("idx", -1);
  _prefs.end();
}

bool NetworkController::tentarRede(const String& ssid, const String& senha) {
  _oled.adicionarLinha("Conectando a:");
  _oled.adicionarLinha(ssid);
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), senha.c_str());

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 24) {
    delay(500);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    tentativas++;
  }
  digitalWrite(LED_PIN, LOW);
  return WiFi.status() == WL_CONNECTED;
}

bool NetworkController::conectarWifi() {
  String ssids[NUM_REDES], senhas[NUM_REDES];
  int idx;
  carregarRedes(ssids, senhas, idx);

  bool algumaSalva = false;
  for (int i = 0; i < NUM_REDES; i++) if (ssids[i] != "") algumaSalva = true;
  if (!algumaSalva) return false;

  WiFi.mode(WIFI_STA);
  _oled.adicionarLinha("Buscando redes...");
  int n = WiFi.scanNetworks();

  for (int k = 1; k <= NUM_REDES; k++) {
    int i = ((idx + k) % NUM_REDES + NUM_REDES) % NUM_REDES;
    if (ssids[i] == "") continue;

    bool visivel = false;
    for (int j = 0; j < n; j++) {
      if (WiFi.SSID(j) == ssids[i]) { visivel = true; break; }
    }
    if (!visivel) continue;

    if (tentarRede(ssids[i], senhas[i])) {
      _prefs.begin("wifi", false);
      _prefs.putInt("idx", i);
      _prefs.end();
      WiFi.scanDelete();
      _udp.begin(UDP_PORTA);
      configurarRotas();
      return true;
    }
  }
  WiFi.scanDelete();
  return false;
}

void NetworkController::configurarRotas() {
  if (_rotasOk) return;
  _server.addMiddleware([this](WebServer& server, Middleware::Callback next) {
    uint64_t started = esp_timer_get_time();
    bool result = next();
    _metrics.recordRequest(started, server.responseCode() >= 400);
    return result;
  });
  const char* headers[] = {"X-ESP32-Command", "X-ESP32-Firmware"};
  _server.collectHeaders(headers, 2);
  _server.on("/", HTTP_GET, handleRootCallback);
  _server.on("/info", HTTP_GET, handleInfoCallback);
  _server.on("/wifi", HTTP_GET, handleWifiCallback);
  _server.on("/salvar", HTTP_POST, handleSaveCallback);
  _server.on("/api/dashboard", HTTP_GET, [this]() { handleInfo(); });
  _server.on("/api/command", HTTP_POST, [this]() { handleCommand(); });
  _server.on("/logs", HTTP_GET, [this]() { handleLogs(); });
  _server.on("/api/logs", HTTP_GET, [this]() { handleLogData(); });
  _server.on("/prometheus", HTTP_GET, [this]() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "text/html; charset=utf-8", _metrics.page());
  });
  _server.on("/api/prometheus", HTTP_POST, [this]() { handlePrometheusSave(); });
  _server.on("/api/prometheus/config", HTTP_POST, [this]() { handlePrometheusSave(); });
  _server.on("/api/prometheus/config", HTTP_GET, [this]() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json; charset=utf-8", _metrics.configurationJson());
  });
  _server.on("/metrics", HTTP_GET, [this]() { _metrics.serveOnWeb(_server); });
  _server.on("/prometheus.yml", HTTP_GET, [this]() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("Content-Disposition", "attachment; filename=prometheus-esp32-p4.yml");
    _server.send(200, "text/plain; charset=utf-8", _metrics.scrapeConfiguration());
  });
  _firmware.begin(_server, _oled);
  _server.begin();
  _rotasOk = true;
}

void NetworkController::iniciarPortal() 
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_P4_CONFIG");
  _udp.begin(UDP_PORTA);
  _oled.adicionarLinha("Portal Ativo!");
  _oled.adicionarLinha("IP: 192.168.4.1");
  configurarRotas();
}

static String statusBadge(bool ok, const char* yes, const char* no) {
 return String("<span class='badge ") + (ok ? "ok" : "warn") + "'>" + (ok ? yes : no) + "</span>";
}
static String infoRow(const char* title, const String& value) {
 return "<div class='row'><span>" + String(title) + "</span><strong>" + esc(value) + "</strong></div>";
}
static String metric(const char* title, const String& value, const char* hint) {
 return "<article class='card'><div class='label'>" + String(title) + "</div><div class='value'>" + esc(value) + "</div><small>" + String(hint) + "</small></article>";
}

void NetworkController::handleLogs() {
 String h = portalStart("Log do dispositivo", false, true);
 h += FPSTR(PORTAL_LOGS);
 h += portalEnd();
 _server.sendHeader("Cache-Control", "no-store");
 _server.send(200, "text/html; charset=utf-8", h);
}

static String logJson(const String& value) {
 String out; out.reserve(value.length() + 64); out += '"';
 const char hex[] = "0123456789abcdef";
 for (size_t i = 0; i < value.length(); ++i) {
   unsigned char c = value[i];
   if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
   else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
   else out += char(c);
 }
 out += '"'; return out;
}

void NetworkController::handleLogData() {
 _server.sendHeader("Cache-Control", "no-store");
 auto error = [this](int code, const char* message) {
   _server.send(code, "application/json; charset=utf-8", "{\"error\":" + logJson(message) + "}");
 };
 if (_firmware.busy() || _dadosProntosParaSalvar) { error(503, "Atualização ou reinício em andamento. Aguarde."); return; }
 if (!_storage.isAtivo()) { error(503, "Cartão SD indisponível."); return; }
 int wanted = 100;
 if (_server.hasArg("lines")) {
   const String arg = _server.arg("lines");
   if (arg != "25" && arg != "100" && arg != "250") { error(400, "Escolha 25, 100 ou 250 linhas."); return; }
   wanted = arg.toInt();
 }
 File file = SD_MMC.open("/log.txt", FILE_READ);
 if (!file) { error(404, "O arquivo /log.txt ainda não existe ou não pode ser aberto."); return; }
 if (file.isDirectory()) { file.close(); error(400, "/log.txt não é um arquivo."); return; }
 const uint32_t total = file.size();
 const uint32_t offset = total > 8192 ? total - 8192 : 0;
 if (!file.seek(offset)) { file.close(); error(500, "Não foi possível posicionar a leitura do log."); return; }
 String text;
 if (!text.reserve(total - offset + 1)) { file.close(); error(500, "Memória insuficiente para ler o log."); return; }
 uint8_t buffer[256]; uint32_t remaining = total - offset;
 while (remaining) {
   int n = file.read(buffer, min(remaining, uint32_t(sizeof(buffer))));
   if (n <= 0) { file.close(); error(500, "Leitura do cartão SD interrompida."); return; }
   if (!text.concat(reinterpret_cast<const char*>(buffer), n)) { file.close(); error(500, "Memória insuficiente para o log."); return; }
   remaining -= n;
 }
 file.close();
 if (offset) {
   int newline = text.indexOf('\n');
   if (newline >= 0 && newline + 1 < int(text.length())) text.remove(0, newline + 1);
   else while (text.length() && (uint8_t(text[0]) & 0xc0) == 0x80) text.remove(0, 1);
 }
 int start = 0, found = 0;
 for (int i = int(text.length()) - 1; i >= 0; --i) {
   if (text[i] == '\n' && i != int(text.length()) - 1 && ++found == wanted) { start = i + 1; break; }
 }
 if (start) text.remove(0, start);
 unsigned lines = 0;
 for (size_t i = 0; i < text.length(); ++i) if (text[i] == '\n') ++lines;
 if (text.length() && !text.endsWith("\n")) ++lines;
 String json = "{\"text\":" + logJson(text) + ",\"size\":" + String(total) +
   ",\"lines\":" + String(lines) + ",\"limited\":" + String(offset || start ? "true" : "false") + "}";
 _server.send(200, "application/json; charset=utf-8", json);
}
void NetworkController::handleWifi() {
 String ssids[NUM_REDES], senhas[NUM_REDES]; int idx;
 carregarRedes(ssids, senhas, idx);
 String h = portalStart("Redes Wi-Fi", true);
 h += F("<div class='top'><div><div class='eyebrow'>Conectividade</div><h1>Suas redes Wi-Fi</h1><p class='muted'>Até cinco conexões salvas para o seu dispositivo.</p></div><a class='button secondary' href='/info'>Ver informações</a></div>");
 h += F("<div class='notice'>Deixe a senha vazia para manter a senha de uma rede com o mesmo SSID. Para remover uma rede, apague o nome. Marque <b>Rede aberta</b> para salvar sem senha.</div><form action='/salvar' method='post' id='networks'><div class='grid'>");
 for(int i=0;i<NUM_REDES;i++) {
  String n=String(i); bool active=WiFi.status()==WL_CONNECTED && ssids[i].length() && ssids[i]==WiFi.SSID();
  h += "<article class='card'><div class='net-title'><div><div class='slot'>PERFIL 0" + String(i+1) + "</div><h2>Rede " + String(i+1) + "</h2></div>";
  h += active ? statusBadge(true,"Conectada","") : ssids[i].length() ? String("<span class='badge'>Salva</span>") : String("<span class='badge'>Disponível</span>");
  h += "</div><label class='field' for='ssid"+n+"'>Nome da rede (SSID)</label><input id='ssid"+n+"' name='ssid"+n+"' type='text' maxlength='32' autocomplete='off' placeholder='Ex.: Minha rede' value='"+esc(ssids[i])+"'>";
  h += "<label class='field' for='senha"+n+"'>Senha</label><div class='password'><input id='senha"+n+"' name='senha"+n+"' type='password' maxlength='64' autocomplete='new-password' placeholder='"+String(ssids[i].length()?"Vazia mantém a senha atual":"8 a 63 caracteres ou chave hexadecimal")+"'><button type='button' aria-controls='senha"+n+"' aria-pressed='false' onclick='togglePassword(this)'>Mostrar</button></div>";
  h += "<label class='check'><input type='checkbox' name='aberto"+n+"' value='1' onchange='toggleOpen(this)'"+String(ssids[i].length() && senhas[i].isEmpty()?" checked":"")+">Rede aberta (sem senha)</label>";
  h += "<p class='hint'>"+String(i==idx?"Última rede utilizada pelo dispositivo.":"A senha salva nunca é enviada para esta página.")+"</p></article>";
 }
 h += F("</div><div class='actions'><button type='submit' id='save'>Salvar redes e reiniciar</button><p>As configurações são verificadas antes do reinício.</p></div><p id='form-status' role='status' class='muted'></p></form>");
 h += F(R"JS(<script>
function togglePassword(b){const i=document.getElementById(b.getAttribute('aria-controls'));const show=i.type==='password';i.type=show?'text':'password';b.textContent=show?'Ocultar':'Mostrar';b.setAttribute('aria-pressed',String(show))}
function toggleOpen(c){const i=c.closest('article').querySelector('input[name^="senha"]');i.disabled=c.checked;if(c.checked)i.value=''}
document.querySelectorAll('input[name^="aberto"]').forEach(toggleOpen);
document.getElementById('networks').addEventListener('submit',function(e){let error='';this.querySelectorAll('input[name^="ssid"]').forEach(i=>{if(new TextEncoder().encode(i.value.trim()).length>32)error='O nome de cada rede deve ter até 32 bytes.'});if(error){e.preventDefault();document.getElementById('form-status').textContent=error;return}document.getElementById('save').disabled=true;document.getElementById('form-status').textContent='Gravando e verificando suas redes…'});
</script>)JS");
 h += portalEnd(); _server.send(200,"text/html; charset=utf-8",h);
}
void NetworkController::handleInfo() {
 String ssids[NUM_REDES], senhas[NUM_REDES]; int idx;
 carregarRedes(ssids,senhas,idx);
 bool wifi=WiFi.status()==WL_CONNECTED; uint32_t seg=millis()/1000;
 String uptime=String(seg/3600)+"h "+String((seg/60)%60)+"m";
 const bool fragment = _server.uri() == "/api/dashboard";
 String h=fragment ? String() : portalStart("Informações",false);
 h += F("<section id='dashboard'>");
 h += F("<div class='top'><div><div class='eyebrow'>Visão geral</div><h1>Seu dispositivo, ao vivo.</h1><p class='muted'>Conexões, desempenho e histórico em um só lugar.</p></div><a class='button' href='/wifi'>Configurar redes</a></div><div class='grid metrics'>");
 h += metric("Wi-Fi",wifi?String("Conectado"):String("Offline"),"Rede sem fio");
 h += metric("Ethernet",ethConectado?String("Conectado"):String("Offline"),"Conexão por cabo");
 h += metric("Memória livre",String(ESP.getFreeHeap()/1024)+" KB","Heap disponível");
 h += metric("Tempo ligado",uptime,"Desde a inicialização"); h += F("</div><div class='grid'>");
 for(int i=0;i<2;i++) {
  bool online=i==0?wifi:ethConectado;const auto& l=_monitor.links[i];
  h += String("<article class='card'><div class='net-title'><h2>")+(i==0?"Wi-Fi":"Ethernet")+"</h2>"+statusBadge(online,"Online","Offline")+"</div>";
  if(i==0) h+=infoRow("Rede",wifi?WiFi.SSID():WiFi.getMode()==WIFI_AP?String("AP ESP32_P4_CONFIG"):String("Não conectada"));
  h += infoRow("Endereço IP",online?(i==0?WiFi.localIP():ETH.localIP()).toString():String("—"));
  h += infoRow("Gateway",online?(i==0?WiFi.gatewayIP():ETH.gatewayIP()).toString():String("—"));
  if(i==0) {h+=infoRow("Sinal",wifi?String(WiFi.RSSI())+" dBm":String("—"));h+=infoRow("MAC",WiFi.macAddress());}
  String ping=!online?String("Sem rede"):!l.result?String("Aguardando medição"):l.replied?String(l.rtt)+" ms":String("Sem resposta");
  h += infoRow("Resposta ao gateway",ping);
  if(l.received) h+=infoRow("RTT mín. / média / máx.",String(l.minimum)+" / "+String((unsigned long)(l.totalRtt/l.received))+" / "+String(l.maximum)+" ms");
  if(l.result) h+=infoRow("Última medição","Há "+String(uint32_t(millis()-l.measuredAt)/1000)+" s");
  h += infoRow("Quedas de conexão",String(l.drops));h+=infoRow("Tempo offline",String((unsigned long)(l.offlineMs/1000))+" s");
  h+=infoRow("Pings sem resposta",String(l.lost)+" de "+String(l.sent));h+="</article>";
 }
 h+=F("<article class='card'><h2>Dispositivo</h2>");
 h+=infoRow("Processador",String(ESP.getChipModel())+" · rev "+String(ESP.getChipRevision()));
 h+=infoRow("Frequência",String(ESP.getCpuFreqMHz())+" MHz");h+=infoRow("Menor heap livre",String(ESP.getMinFreeHeap()/1024)+" KB");
 h+=infoRow("Core do painel",String(xPortGetCoreID())+" · consulte cores no terminal");
 h+=infoRow("Cartão SD",_storage.isAtivo()?String("Disponível"):String("Indisponível"));
 h+=infoRow("Console UDP",String(UDP_PORTA));h+="</article><article class='card'><h2>Redes salvas</h2>";
 bool any=false;
 for(int i=0;i<NUM_REDES;i++) if(ssids[i].length()) {any=true;h+=infoRow(("Perfil "+String(i+1)).c_str(),ssids[i]+(i==idx?" · última utilizada":""));}
 if(!any)h+=F("<p class='muted'>Nenhuma rede salva. Configure sua primeira conexão.</p>");
 h+=F("</article><article class='card full'><h2>Histórico de conexão</h2>");
 bool events=false;for(const auto& e:_monitor.events)if(e.length()){events=true;h+="<div class='event'>"+esc(e)+"</div>";}
 if(!events)h+=F("<p class='muted'>Aguardando eventos de rede.</p>");
 h+=F("<p class='hint'>Tempos desde o boot. O histórico reinicia com a placa. Um ping sem resposta pode indicar bloqueio de ICMP.</p></article></div></section>");
 if (!fragment) { h += FPSTR(PORTAL_CONSOLE); h += portalEnd(); }
 _server.sendHeader("Cache-Control", "no-store");
 _server.send(200,"text/html; charset=utf-8",h);
}

void NetworkController::handlePrometheusSave() {
  _server.sendHeader("Cache-Control", "no-store");
  if (_server.header("X-ESP32-Command") != "1" || !_server.hasArg("host") ||
      !_server.hasArg("port") || !_server.hasArg("interval")) {
    _server.send(400, "text/plain; charset=utf-8", "Formulário incompleto ou inválido."); return;
  }
  if (_dadosProntosParaSalvar || _firmware.busy()) {
    _server.send(409, "text/plain; charset=utf-8", "Aguarde a atualização ou reinício."); return;
  }
  String error;
  int status;
  if (!_metrics.save(_server.arg("host"), _server.arg("port"), _server.arg("interval"), error, status)) {
    _server.send(status, "text/plain; charset=utf-8", error); return;
  }
  _server.send(200, "application/json; charset=utf-8", _metrics.configurationJson());
}

void NetworkController::handleCommand() {
  _server.sendHeader("Cache-Control", "no-store");
  // A custom header prevents cross-origin HTML forms from invoking commands.
  if (!_server.hasArg("cmd") || _server.header("X-ESP32-Command") != "1") {
    _server.send(400, "text/plain; charset=utf-8", "Requisicao de comando invalida\n"); return;
  }
  if (_dadosProntosParaSalvar || _firmware.busy()) {
    _server.send(409, "text/plain; charset=utf-8", "Reinicio em andamento\n"); return;
  }
  String cmd = _server.arg("cmd");
  if (cmd.isEmpty() || cmd.length() > 254 || cmd.indexOf('\r') >= 0 || cmd.indexOf('\n') >= 0) {
    _server.send(400, "text/plain; charset=utf-8", "Envie um comando de ate 254 bytes, sem quebras de linha\n"); return;
  }
  if (!_commandHandler) { _server.send(503, "text/plain", "Console indisponivel\n"); return; }
  _httpResponse = "";
  _httpTruncated = false;
  _commandHandler(cmd);
  if (_httpTruncated) _httpResponse += "\n[Resposta limitada a 8 KB. Use sd_tail para arquivos grandes.]\n";
  _server.send(200, "text/plain; charset=utf-8", _httpResponse);
  _httpResponse = "";
}

void NetworkController::handleRoot() {
  if (WiFi.getMode() == WIFI_AP) handleWifi();
  else handleInfo();
}

void NetworkController::handleSave() {
 auto message=[&](int code,const char* title,const String& text,bool success){
  String h=portalStart(title,true);
  h+=String("<div class='card message ")+(success?"success":"error")+"'><div class='eyebrow'>Configuração de rede</div><h1>"+String(title)+"</h1><p class='muted'>"+esc(text)+"</p>";
  h+=success?String("<p>Após o reinício, acesse o novo IP do dispositivo. Se nenhuma rede conectar, use o AP ESP32_P4_CONFIG.</p>"):String("<a class='button' href='/wifi'>Voltar às redes</a>");
  h+="</div>"+portalEnd();_server.send(code,"text/html; charset=utf-8",h);
 };
 if(_dadosProntosParaSalvar || _firmware.busy()){message(409,"Atualização ou reinício em andamento","Aguarde o dispositivo reiniciar.",false);return;}
 String oldS[NUM_REDES],oldP[NUM_REDES];int oldIdx;
 carregarRedes(oldS,oldP,oldIdx);
 for(int i=0;i<NUM_REDES;i++) {
  String n=String(i);
  bool open=_server.hasArg("aberto"+n);
  if(!_server.hasArg("ssid"+n) || (!open && !_server.hasArg("senha"+n))){message(400,"Formulário incompleto","Nenhuma configuração foi alterada. Envie todos os cinco perfis.",false);return;}
  String ssid=_server.arg("ssid"+n),pass=_server.arg("senha"+n);ssid.trim();
  if(ssid.length()>32){message(400,"Nome de rede inválido","O SSID deve ter até 32 bytes.",false);return;}
  if(ssid.isEmpty()){_tmpS[i]="";_tmpP[i]="";continue;}
  if(open)pass="";else if(pass.isEmpty() && ssid==oldS[i])pass=oldP[i];
  bool hex=pass.length()==64;
  if(hex)for(size_t j=0;j<pass.length();j++)if(!isxdigit((unsigned char)pass[j])){hex=false;break;}
  if(!pass.isEmpty() && !(pass.length()>=8 && pass.length()<=63) && !hex){message(400,"Senha inválida","Use de 8 a 63 bytes ou uma chave hexadecimal de 64 caracteres.",false);return;}
  if(pass.isEmpty() && !open && (ssid!=oldS[i] || !oldP[i].isEmpty())){message(400,"Informe a senha","Para uma rede sem senha, marque Rede aberta.",false);return;}
  _tmpS[i]=ssid;_tmpP[i]=pass;
 }
 if(!_prefs.begin("wifi",false)){message(500,"Falha ao salvar","Não foi possível abrir as configurações. Tente novamente.",false);return;}
 bool ok=true;
 for(int i=0;i<NUM_REDES;i++) {
  String sk="s"+String(i),pk="p"+String(i);
  _prefs.putString(sk.c_str(),_tmpS[i]);_prefs.putString(pk.c_str(),_tmpP[i]);
  ok=(_prefs.isKey(sk.c_str()) && _prefs.isKey(pk.c_str()) && _prefs.getString(sk.c_str(),"!missing!")==_tmpS[i] && _prefs.getString(pk.c_str(),"!missing!")==_tmpP[i]) && ok;
 }
 _prefs.putInt("idx",-1);ok=(_prefs.getInt("idx",-2)==-1) && ok;
 if(!ok) {
  bool restored=true;
  for(int i=0;i<NUM_REDES;i++) {
   String sk="s"+String(i),pk="p"+String(i);
   _prefs.putString(sk.c_str(),oldS[i]);_prefs.putString(pk.c_str(),oldP[i]);
   restored=(_prefs.isKey(sk.c_str()) && _prefs.isKey(pk.c_str()) && _prefs.getString(sk.c_str(),"!missing!")==oldS[i] && _prefs.getString(pk.c_str(),"!missing!")==oldP[i]) && restored;
  }
  _prefs.putInt("idx",oldIdx);restored=(_prefs.getInt("idx",-2)==oldIdx) && restored;_prefs.end();
  message(500,"Falha na gravação",restored?String("As configurações anteriores foram restauradas. Tente novamente."):String("Não foi possível restaurar todos os dados. Revise os perfis antes de reiniciar."),false);return;
 }
 _prefs.end();
 message(200,"Redes salvas!","Os cinco perfis foram gravados e verificados. O dispositivo vai reiniciar em instantes.",true);
 _restartAt=millis()+1500;_dadosProntosParaSalvar=true;
}

void NetworkController::processarPortalERedes() 
{
  arduino_event_id_t event;
  while (_networkEvents && xQueueReceive(_networkEvents, &event, 0) == pdTRUE) {
    switch (event) {
      case ARDUINO_EVENT_ETH_START: _oled.adicionarLinha("ETH inicializada"); break;
      case ARDUINO_EVENT_ETH_CONNECTED: _oled.adicionarLinha("Cabo ETH conectado"); break;
      case ARDUINO_EVENT_ETH_GOT_IP:
        ethConectado = true; _udp.begin(UDP_PORTA);
        _oled.adicionarLinha("ETH: " + ETH.localIP().toString()); break;
      case ARDUINO_EVENT_ETH_DISCONNECTED:
      case ARDUINO_EVENT_ETH_STOP:
      case ARDUINO_EVENT_ETH_LOST_IP:
        ethConectado = false; _oled.adicionarLinha("ETH sem rede"); break;
      default: break;
    }
  }
  // Poll actual status as well: a full event queue must not leave stale telemetry.
  ethConectado = ETH.linkUp() && ETH.localIP() != IPAddress(0,0,0,0);
  configurarPrioridadeDeRede();
  _monitor.update(WiFi.status() == WL_CONNECTED, ethConectado);
  _server.handleClient();
  if (!_firmware.busy()) _storage.sampleTelemetry();
  _metrics.handle(_firmware.busy());
  if (!_dadosProntosParaSalvar) _firmware.handle();
  if (_dadosProntosParaSalvar && int32_t(millis()-_restartAt)>=0) ESP.restart();
}

void NetworkController::configurarPrioridadeDeRede() 
{
  esp_netif_t* netif_wifi = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_t* netif_eth  = esp_netif_get_handle_from_ifkey("ETH_DEF");
  if (netif_wifi != NULL && netif_eth != NULL) {
    esp_netif_set_route_prio(netif_wifi, 50);
    esp_netif_set_route_prio(netif_eth, 10);
  }
}

void NetworkController::responderUDP(const String& msg) 
{
  if (commandChannel == CommandChannel::ConsoleSerial) { Serial.print(msg); return; }
  if (commandChannel == CommandChannel::HTTP) {
    const size_t remaining = 8192 - _httpResponse.length();
    if (msg.length() > remaining) _httpTruncated = true;
    _httpResponse += msg.substring(0, remaining);
    return;
  }
  if (_udp.remoteIP()) {
    _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
    _udp.print(msg);
    _udp.endPacket();
  }
}

void NetworkController::zerarConfiguracoes() 
{
  _oled.adicionarLinha("Limpando Memoria...");
  _prefs.begin("wifi", false); _prefs.clear(); _prefs.end();
  responderUDP("WiFi zerado. Reiniciando...\n");
  agendarReinicio();
}

void NetworkController::ativarEthernetFisica() 
{
  _oled.adicionarLinha("Iniciando ETH...");
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
  // Keep the existing routing setup; measurement binds each ping to its interface.
}

String NetworkController::listarSSIDs() {
 String ssids[NUM_REDES],senhas[NUM_REDES];int idx;
 carregarRedes(ssids,senhas,idx);String result;
 for(int i=0;i<NUM_REDES;i++)if(ssids[i].length())result+=ssids[i]+"\n";
 return result.length()?result:String("\n");
}
bool NetworkController::adicionarRede(const String& ssid,const String& senha,String& erro) {
 if(_dadosProntosParaSalvar){erro="Reinicio em andamento";return false;}
 if(ssid.isEmpty() || ssid.length()>32 || ssid.indexOf('\n')>=0 || ssid.indexOf('\r')>=0){erro="SSID deve conter de 1 a 32 bytes";return false;}
 bool hex=senha.length()==64;
 if(hex)for(size_t i=0;i<senha.length();i++)if(!isxdigit((unsigned char)senha[i])){hex=false;break;}
 if(!senha.isEmpty() && !(senha.length()>=8 && senha.length()<=63) && !hex){erro="Senha: 8 a 63 bytes ou 64 caracteres hexadecimais";return false;}
 String ssids[NUM_REDES],senhas[NUM_REDES];int idx;
 carregarRedes(ssids,senhas,idx);
 if(!_prefs.begin("wifi",false)){erro="Falha ao abrir configuracoes";return false;}
 uint8_t next=_prefs.getUChar("next",0);if(next>=NUM_REDES)next=0;
 int slot=-1;for(int i=0;i<NUM_REDES;i++)if(ssids[i]==ssid){slot=i;break;}
 bool existing=slot>=0;if(!existing)slot=next;
 String sk="s"+String(slot),pk="p"+String(slot);
 uint8_t newNext=existing?next:uint8_t((slot+1)%NUM_REDES);
 _prefs.putString(sk.c_str(),ssid);_prefs.putString(pk.c_str(),senha);_prefs.putUChar("next",newNext);
 bool ok=_prefs.isKey(sk.c_str()) && _prefs.isKey(pk.c_str()) &&
   _prefs.getString(sk.c_str())==ssid && _prefs.getString(pk.c_str())==senha && _prefs.getUChar("next",255)==newNext;
 if(!ok){
  _prefs.putString(sk.c_str(),ssids[slot]);_prefs.putString(pk.c_str(),senhas[slot]);_prefs.putUChar("next",next);
  bool restored=_prefs.isKey(sk.c_str()) && _prefs.isKey(pk.c_str()) && _prefs.getString(sk.c_str())==ssids[slot] && _prefs.getString(pk.c_str())==senhas[slot] && _prefs.getUChar("next",255)==next;
  erro=restored?"Falha ao gravar; perfil anterior restaurado":"Falha ao gravar e restaurar; revise os perfis";
 }
 _prefs.end();return ok;
}
