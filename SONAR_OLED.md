# Tela sonar — ESP32-P4 v1.25.0

Revisão: 06/11/2026. OLED SSD1306 monocromático de 128×64 pixels. Firmware não compilado nem gravado; aparência final e navegação precisam de validação na placa.

## Abrir e sair

A tela sonar é a **oitava tela**, depois da câmera. Pressione o botão GPIO27 para percorrer as telas ou use `tela:7` pela Serial, UDP ou terminal web. Há atalho **Sonar no OLED** no portal e no dispositivo MQTT do Home Assistant.

Permanece selecionada até apertar o botão ou enviar um comando explícito de navegação. Não entra no protetor automático nem é substituída por mensagens do terminal. OTA tem prioridade e comandos explícitos de protetor/tela continuam disponíveis. O botão sai para a primeira tela, relógio. A câmera permanece na sétima posição (`tela:6`), também sem protetor automático. As seis telas de informações mantêm o protetor por inatividade.

Abrir a tela não liga o servo nem altera o rastreamento. Use `ultrasonic:on` e `servo:on` para iniciar a varredura física; `servo:off` remove os pulsos do motor. Veja [HC_SR04.md](HC_SR04.md) para ligação e [RADAR_TRACKING.md](RADAR_TRACKING.md) para acompanhamento.

## Desenho

- Semicírculo com quatro anéis, representando **1, 2, 3 e 4 metros**.
- Feixe na direção atual **comandada** ao servo; só aparece com o servo habilitado. Não há animação de direção inventada.
- Ecos válidos associados ao ângulo da aquisição, agrupados em 35 setores de 5°. Ecos novos ficam maiores por 1,5 s e depois viram pontos até desaparecerem em 6 s. Um setor guarda apenas seu eco mais recente.
- Ecos são registrados também enquanto outra página está aberta; não se trata de mapa persistente ou armazenamento de todas as aquisições.
- Alvo confirmado recebe um círculo e a indicação **ALVO**. Os demais estados indicam **BUSCA** ou **PARADO**.
- Rodapé mostra ângulo atual comandado, distância mais recente em cm e indicação **BOTAO**. Distância do rodapé e feixe podem corresponder a instantes diferentes; cada ponto do gráfico preserva o ângulo da própria aquisição.
- Sem leitura válida, mostra **S/ECO**; com medições desabilitadas, **OFF**. Pontos antigos continuam apenas até o prazo de persistência, sem criar novos ecos.

O desenho é atualizado no máximo a cada 150 ms e usa apenas snapshots já medidos pelo HC-SR04. Não dispara novas medições nem espera por ecos para desenhar. O envio do framebuffer ao OLED usa a interface I²C existente. Câmera no OLED mantém atualização de uma foto por segundo.

## Validação após atualizar

1. Confira a distância com `ultrasonic:status` em posição fixa.
2. Abra `tela:7`: devem aparecer os anéis e a identificação 8/8.
3. Inicie `servo:on` e observe o feixe acompanhando o servo e os pontos surgindo onde houver eco válido.
4. Retire o objeto: confirme S/ECO quando não houver outro eco e o desaparecimento dos pontos antigos em até 6 s.
5. Aguarde mais de 60 s: o sonar deve permanecer. Pressione GPIO27: deve voltar ao relógio e emitir o som habitual de troca de tela.
