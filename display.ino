// Função para escrever linha por linha dinamicamente com rolagem automática
void adicionarLinha(String novoTexto) {
  Serial.println("[OLED] " + novoTexto); // Também espelha no Monitor Serial
  
  if (!oledInicializado) return; // Proteção contra ponteiro nulo se o display falhar

  // Move o histórico para cima se a tela estiver cheia
  if (totalLinhas >= MAX_LINHAS) {
    for (int i = 0; i < MAX_LINHAS - 1; i++) {
      historicoLinhas[i] = historicoLinhas[i + 1];
    }
    historicoLinhas[MAX_LINHAS - 1] = novoTexto;
  } else {
    historicoLinhas[totalLinhas] = novoTexto;
    totalLinhas++;
  }

  // Renderiza o histórico atualizado na tela
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  for (int i = 0; i < totalLinhas; i++) {
    display.setCursor(0, i * 8); 
    display.println(historicoLinhas[i]);
  }
  display.display();
}
