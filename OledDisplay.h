#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Adafruit_SSD1306.h>
#include "DeviceConfig.h"

class TimeManager;
class NetworkController;
class StorageManager;

class OledDisplay
{
  private:
    Adafruit_SSD1306 _display;
    bool _inicializado;
    uint8_t _pagina = 0;
    bool _autoPaginas = true;
    bool _imagemCamera = false;
    uint32_t _ultimaPagina = 0, _ultimoPainel = 0, _consoleAte = 0;
    uint32_t _ultimaAtividade = 0, _ultimoRobo = 0, _frameRobo = 0;
    uint32_t _tempoProtetor = 60000;
    bool _protetorAtivo = false, _protetorAutomatico = true;
    void (*_aoTrocarPagina)() = nullptr;
    void desenharRobo(uint32_t now);

    // Reduzido para 5 linhas móveis para sobrar espaço para a barra de progresso na base
    static const int MAX_LINHAS_TERMINAL = 5;
    String _historico[MAX_LINHAS_TERMINAL];
    int _totalLinhas;

    unsigned long _ultimoRelogioRefresh;
    String _ultimaHoraExibida;

    // Variáveis internas para controle do progresso
    int _progressoAtual; // 0 a 100
    bool _exibirProgresso;

  public:
    OledDisplay();
    void inicializar();
    void selecionarPagina(uint8_t pagina);
    void avancarPagina();
    void paginasAutomaticas();
    void registrarAtividade();
    void iniciarProtetor();
    void configurarProtetor(bool automatico);
    void tempoProtetor(uint32_t segundos) { _tempoProtetor = segundos * 1000; registrarAtividade(); }
    String statusProtetor() const;
    void aoTrocarPagina(void (*callback)()) { _aoTrocarPagina = callback; }
    void atualizarPainel(TimeManager& hora, NetworkController& rede, StorageManager& sd);

    void adicionarLinha(const String& novoTexto);
    void atualizarRelogioSuperior(TimeManager& timeRef);

    // Novo: Ativa ou desativa a barra gráfica inferior
    void configurarProgresso(bool ativar, int porcentagem = 0);
    void redesenharTelaCompleta(const String& horaAtual);

    bool isPronto() const
    {
        return _inicializado;
    }
    bool mostrarImagemCamera(const uint8_t* pixels, size_t size);
};

#endif
