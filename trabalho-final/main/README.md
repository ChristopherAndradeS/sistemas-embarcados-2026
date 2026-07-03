# 📑 RELATÓRIO FINAL DE PROJETO
**DISCIPLINA:** Sistemas Embarcados  
---

* **Aluno 1:** Christopher Andrade
* **Aluno 2:** Max Muller

**TÍTULO:** Desenvolvimento de Sistema de controle PI para temperatura
**OBJETIVO:** Controlar temperatura, automaticamente, em granja (Incubadora Avícola)  
---

## 1. INTRODUÇÃO

### 1.1 Importância do Controle Térmico na Incubação
Na avicultura industrial, o sucesso para eclosão de ovos e o desenvolvimento embrionário dependem criticamente da variável temperatura. O ovo possui uma sensibilidade térmica extrema, desvios sutis de temperatura por um longo tempo podem resultar na morte do embrão, malformações ou atrasos severos na eclosão. Desse modo, o controle de temperatura automatizado em granjas é crucial para controle e qualidade. A janela de temperatura ideal de desenvolvimento é de $37.5^\circ\text{C}$ a $38.5^\circ\text{C}$

### 1.2 O Controlador Proporcional-Integral (PI) e Justificativa de Escolha
Para solucionar este problema, utilizou-se um controlador do tipo **PI (Proporcional-Integral)**. O funcionamento do algoritmo está na correção contínua do erro ($E = \text{Setpoint} - \text{Temperatura Medida}$):
* **Termo Proporcional ($K_p$):** Fornece uma resposta rápida e diretamente proporcional ao erro atual, garantindo aceleração no aquecimento inicial.
* **Termo Integral ($K_i$):** Acumula o erro ao longo do tempo, eliminando o erro de regime permanente (offset), travando a temperatura exatamente no Setpoint.

**Motivo da Escolha:** A remoção do termo Derivativo ($K_d$) foi uma decisão de projeto baseada nas características físicas do sistema. Ambientes de incubação térmica possuem alta inércia (baixa derivada temporal), o que torna a taxa de variação da temperatura muito pequena. Adicionar uma componente derivativa faria com que o controlador amplificasse ruídos de amostragem de alta frequência inerentes à leitura analógica do sensor de temperatura LM35, resultando em oscilações instáveis e nocivas no ciclo de trabalho (PWM) dos atuadores.

---

## 2. METODOLOGIA

O sistema foi implementado de maneira nativa sobre o ecossistema **ESP-IDF**, utilizando o modelo de concorrência preemptiva do **FreeRTOS**. A arquitetura foi dividida em um pipeline sequencial e determinístico de tarefas isoladas (*tasks*), mitigando bloqueios mútuos.

### 2.1 Pipeline de Tasks do ESP-IDF
1.  **TIMER (`timer_task`):** Atua como o coração temporal do sistema. Baseada no periférico de hardware `gptimer`, despacha interrupções periódicas a cada 100ms que liberam um semáforo binário (`semaphore_adc`).
2.  **ADC (`adc_task`):** Desbloqueia com o semáforo do timer e realiza a aquisição analógica da tensão gerada pelo sensor LM35. Converte o valor bruto em tensão calibrada, calcula a temperatura real e pisca o LED embutido (GPIO 2) como sinalizador de amostragem concluída. O dado é postado na `controller_queue`.
3.  **COMPUTAÇÃO (`controller_task`):** Responsável pelo processamento matemático. Lê o valor da fila, calcula o erro térmico e executa o algoritmo de controle PI por meio do componente oficial `pid_ctrl` da Espressif.
4.  **CONTROLE (`pwm_task`):** Driver que recebe as estruturas de ciclo de trabalho calculadas (`duty_t`) através de uma fila específica e ajusta fisicamente as saídas de potência do microcontrolador utilizando o periférico de hardware `LEDC` (PWM de 13 bits) conectado ao Aquecedor e ao Cooler.

### 2.3 Estratégia de Atuação Complementar Push-Pull
Para mitigar os "trancos" causados pela zona morta de ativação do cooler mecânico, foi aplicada uma lógica de atuação contrária e complementar ao redor do ponto de ajuste:
* **Erro > 0.5°C:** Aquecimento estrito (Heater em 100% e Cooler em 0%).
* **Erro < -0.5°C:** Resfriamento estrito (Heater em 0% e Cooler em 100%).
* **Dentro da Banda (-0.5°C a 0.5°C):** Regime estável complementar. O cooler opera com a potência inversa à requisitada pelo aquecedor ($Cooler\% = 100\% - Heater\%$) atunuada em 84.5%. Esse "cabo de guerra" controlado equilibra perfeitamente as taxas de troca de calor.

---

## 3. RESULTADOS

Os testes na prática em laboratório apresentaram resultados altamente favoráveis e em total conformidade com os requisitos iniciais do projeto:

---

## 4. CONCLUSÃO

O projeto cumpriu integralmente seus objetivos didáticos e técnicos. A utilização do microcontrolador ESP32 junto do framework nativo do ESP, ESP-IDF, e o sistema operacional FreeRTOS possibilitou o desenvolvimento de uma aplicação robusta de automação industrial embarcada, estruturada sob a Teoria de Controle. A lógica implementada solucionou os desafios mecânicos e elétricos dos atuadores sem a necessidade de componentes adicionais de hardware, garantindo estabilidade e a segurança biológica exigida por cenários críticos como a incubação avícola.
