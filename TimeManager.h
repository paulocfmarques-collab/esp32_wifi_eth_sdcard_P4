#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <Preferences.h> 
#include "OledDisplay.h"

class TimeManager {
public:
    TimeManager(OledDisplay& oledRef);
    void inicializarETestarNTP();
    String obterApenasHora();
    String obterStringDataHora();
    void imprimirDataHora();

    void carregarEConfigurarHorario();
    void atualizarFuso(long novoGmtOffset);
    void atualizarDST(int novoDaylightOffset);

private:
    OledDisplay& _oled;
};

#endif
