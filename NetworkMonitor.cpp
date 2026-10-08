#include "NetworkMonitor.h"
#include <WiFi.h>
#include <ETH.h>
#include "lwip/ip_addr.h"
void NetworkMonitor::event(const String& text) {
 for(int i=3;i>0;i--) events[i]=events[i-1];
 events[0]=String(millis()/1000)+"s "+text;
}
void NetworkMonitor::received(esp_ping_handle_t h,void* arg) {
 auto* m=static_cast<NetworkMonitor*>(arg); uint32_t ms=0;
 esp_ping_get_profile(h,ESP_PING_PROF_TIMEGAP,&ms,sizeof(ms));
 portENTER_CRITICAL(&m->mux); m->elapsed=ms; m->success=true; portEXIT_CRITICAL(&m->mux);
}
void NetworkMonitor::ended(esp_ping_handle_t,void* arg) {
 auto* m=static_cast<NetworkMonitor*>(arg);
 portENTER_CRITICAL(&m->mux); m->done=true; portEXIT_CRITICAL(&m->mux);
}
void NetworkMonitor::update(bool wifi,bool eth) {
 uint32_t now=millis(); bool states[2]={wifi,eth};
 if(!initialized) {
  for(int i=0;i<2;i++) {
   for(int &v:links[i].samples) v=-2;
   links[i].online=states[i];
   event(String(i==0 ? "WiFi " : "ETH ")+(states[i] ? "online" : "offline"));
  }
  tick=now; initialized=true;
 }
 for(int i=0;i<2;i++) {
  if(!links[i].online) links[i].offlineMs+=uint32_t(now-tick);
  if(states[i]!=links[i].online) {
   if(!states[i]) links[i].drops++;
   links[i].online=states[i]; links[i].result=false; epoch[i]++;
   event(String(i==0 ? "WiFi " : "ETH ")+(states[i] ? "online" : "caiu"));
  }
 }
 tick=now;
 bool finished, ok; uint32_t ms;
 portENTER_CRITICAL(&mux);finished=done;ok=success;ms=elapsed;portEXIT_CRITICAL(&mux);
 if(session && finished) {
  esp_ping_delete_session(session);session=nullptr;
  IPAddress current=active==0 ? WiFi.gatewayIP() : ETH.gatewayIP();
  if(links[active].online && epoch[active]==sessionEpoch && current==target) {
   auto& l=links[active];l.result=true;l.replied=ok;l.rtt=ms;l.measuredAt=now;l.sent++;
   if(!ok) l.lost++;
   else {
    if(l.received==0 || ms<l.minimum) l.minimum=ms;
    if(l.received==0 || ms>l.maximum) l.maximum=ms;
    l.received++; l.totalRtt+=ms;
   }
   l.samples[l.head]=ok ? int(ms) : -1;l.head=(l.head+1)%24;
  }
 }
 if(session || int32_t(now-next)<0) return;
 next=now+5000;
 active=turn;turn=1-turn;
 if(!links[active].online) active=1-active;
 if(!links[active].online) return;
 target=active==0 ? WiFi.gatewayIP() : ETH.gatewayIP();
 auto* net=esp_netif_get_handle_from_ifkey(active==0 ? "WIFI_STA_DEF" : "ETH_DEF");
 if(!net || target==IPAddress(0,0,0,0)) return;
 int index=esp_netif_get_netif_impl_index(net);if(index<=0)return;
 esp_ping_config_t cfg=ESP_PING_DEFAULT_CONFIG();cfg.count=1;cfg.timeout_ms=1000;cfg.interface=index;
 IP_ADDR4(&cfg.target_addr,target[0],target[1],target[2],target[3]);
 esp_ping_callbacks_t cb={};cb.cb_args=this;cb.on_ping_success=received;cb.on_ping_end=ended;
 portENTER_CRITICAL(&mux);done=false;success=false;elapsed=0;portEXIT_CRITICAL(&mux);
 sessionEpoch=epoch[active];
 if(esp_ping_new_session(&cfg,&cb,&session)!=ESP_OK){session=nullptr;return;}
 if(esp_ping_start(session)!=ESP_OK){esp_ping_delete_session(session);session=nullptr;}
}
String NetworkMonitor::report() const {
 String s="=== MONITOR DE REDE ===\n";
 for(int i=0;i<2;i++) {
  const auto& l=links[i];s+=i==0 ? "WiFi: " : "ETH: ";s+=l.online ? "ONLINE" : "OFFLINE";
  s+="\nQuedas: "+String(l.drops)+" | Offline: "+String((unsigned long)(l.offlineMs/1000))+" s\n";
  s+="Ping gateway: ";
  s+=!l.online ? String("sem rede") : !l.result ? String("aguardando") : l.replied ? String(l.rtt)+" ms" : String("sem resposta");
  if(l.result) s+=" | ha "+String(uint32_t(millis()-l.measuredAt)/1000)+" s";
  if(l.received) s+="\nRTT min/medio/max: "+String(l.minimum)+"/"+
    String((unsigned long)(l.totalRtt/l.received))+"/"+String(l.maximum)+" ms";
  s+="\nEnviados: "+String(l.sent)+" | Sem resposta: "+String(l.lost)+"\n";
 }
 for(const auto& e:events) if(e.length())s+=e+"\n";
 return s;
}
