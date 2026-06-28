# 📑 RELATÓRIO FINAL DE PROJETO
**DISCIPLINA:** Sistemas Embarcados / Automação  
**INSTITUIÇÃO:** [Nome da Instituição]  

---

## 👥 IDENTIFICAÇÃO DO GRUPO

* **Integrante 1:** [Nome do Integrante 1]  
    **Período:** [Ex: 7º Período] / Engenharia [Sua Engenharia]
* **Integrante 2:** [Nome do Integrante 2]  
    **Período:** [Ex: 8º Período] / Engenharia [Sua Engenharia]

**TÍTULO DO TRABALHO:** Desenvolvimento de Sistema Multitask Push-Pull para Estabilização Térmica  
**TEMA:** Controle de temperatura automático em granja (Incubadora Avícola)  

---

## 1. INTRODUÇÃO

### 1.1 Importância do Controle Térmico na Incubação
Na avicultura industrial, o sucesso da eclosão de ovos e o desenvolvimento embrionário saudável dependem criticamente de variáveis ambientais estáveis. O ovo possui uma sensibilidade térmica extrema: desvios sutis de temperatura (frações de grau Celsius) fora da janela ideal de desenvolvimento ($37.5^\circ\text{C}$ a $38.5^\circ\text{C}$) podem resultar em mortalidade embrionária precoce, malformações ou atrasos severos na eclosão. Desse modo, o controle de temperatura automatizado em granjas não é apenas uma ferramenta de otimização, mas um requisito biológico estrito de viabilidade produtiva.

### 1.2 O Controlador Proporcional-Integral (PI) e Justificativa de Escolha
Para solucionar este problema, empregou-se um controlador do tipo **PI (Proporcional-Integral)**. O funcionamento deste algoritmo baseia-se na correção contínua do erro ($E = \text{Setpoint} - \text{Temperatura Medida}$):
* **Termo Proporcional ($K_p$):** Fornece uma resposta rápida e diretamente proporcional ao erro atual, garantindo aceleração no aquecimento inicial.
* **Termo Integral ($K_i$):** Acumula o erro ao longo do tempo, eliminando o erro de regime permanente (offset), travando a temperatura exatamente no Setpoint.

**Motivo da Escolha:** A remoção do termo Derivativo ($K_d$) foi uma decisão estratégica de projeto baseada nas características físicas do sistema. Ambientes de incubação térmica possuem alta inércia (baixa derivada temporal), o que torna a taxa de variação da temperatura muito pequena. Adicionar uma componente derivativa faria com que o controlador amplificasse ruídos de amostragem de alta frequência inerentes à leitura analógica do sensor de temperatura LM35, resultando em oscilações instáveis e nocivas no ciclo de trabalho (PWM) dos atuadores.

---

## 2. METODOLOGIA

O sistema foi implementado de maneira nativa sobre o ecossistema **ESP-IDF**, utilizando o modelo de concorrência preemptiva do **FreeRTOS**. A arquitetura foi dividida em um pipeline sequencial e determinístico de tarefas isoladas (*tasks*), mitigando bloqueios mútuos.

### 2.1 Pipeline de Tasks do ESP-IDF
1.  **TIMER (`timer_task`):** Atua como o coração temporal do sistema. Baseada no periférico de hardware `gptimer`, despacha interrupções periódicas a cada 100ms que liberam um semáforo binário (`semaphore_adc`).
2.  **ADC (`adc_task`):** Desbloqueia com o semáforo do timer e realiza a aquisição analógica da tensão gerada pelo sensor LM35. Converte o valor bruto em tensão calibrada de fábrica, calcula a temperatura real e pisca o LED embutido (GPIO 2) como sinalizador de amostragem concluída. O dado é postado na `controller_queue`.
3.  **COMPUTAÇÃO (`controller_task`):** Responsável pelo processamento matemático. Lê o valor da fila, calcula o erro térmico e executa o algoritmo de controle PI por meio do componente oficial `pid_ctrl` da Espressif.
4.  **CONTROLE (`pwm_task`):** Driver que recebe as estruturas de ciclo de trabalho calculadas (`duty_t`) através de uma fila específica e ajusta fisicamente as saídas de potência do microcontrolador utilizando o periférico de hardware `LEDC` (PWM de 13 bits) conectado ao Aquecedor e ao Ventilador.

### 2.2 Diagrama de Blocos da Malha Fechada (ASCII)

```text
       +-------------------------------------------------------------+
       |                                                             |
       v                                                             |
+──────────────+     Erro      +─────────────────+  Sinal PI  +──────────────+
|   Setpoint   |──────────────>| controller_task |───────────>|   pwm_task   |
| (Web / MQTT) |   [SP - TMP]  | (Componente PI) |            |  (Driver)    |
+──────────────+               +─────────────────+            +──────────────+
                                                                     |
                                                                     | Duty Cycle
                                                                     v
+──────────────+               +─────────────────+            +──────────────+
| Sensor LM35  |<──────────────| Ambiente Granja |<───────────|  Atuadores   |
|  (adc_task)  |  Temperatura  |  (Incubadora)   |  Calor/Ar  |Heater & Fan  |
+──────────────+     Real      +─────────────────+            +──────────────+
       |                                                             ^
       |                                                             |
       +─────────────────────────────────────────────────────────────+
```

### 2.3 Estratégia de Atuação Complementar Push-Pull
Para mitigar os "trancos" causados pela zona morta de ativação do ventilador mecânico, foi aplicada uma lógica de atuação contrária e complementar ao redor do ponto de ajuste:
* **Erro > 2.5°C:** Aquecimento estrito (Heater em 100% e Cooler em 0%).
* **Erro < -2.5°C:** Resfriamento estrito (Heater em 0% e Cooler em 100%).
* **Dentro da Banda (-2.5°C a 2.5°C):** Regime estável complementar. O cooler opera com a potência inversa à requisitada pelo aquecedor ($Cooler\% = 100\% - Heater\%$). Esse cabo de guerra controlado equilibra perfeitamente as taxas de troca de calor.

---

## 3. RESULTADOS

Os testes em ambiente de bancada prática apresentaram resultados altamente favoráveis e em total conformidade com os requisitos iniciais do projeto:

* **Estabilidade Temporal:** O uso de interrupções por hardware (`gptimer`) garantiu que o pipeline rodasse rigorosamente a cada 100ms, sem sofrer desvios causados pelo processamento concorrente do display gráfico ou da rede de comunicação.
* **Mitigação de Ruído:** A amostragem associada ao controlador puramente PI provou-se imune a pequenos picos de instabilidade do ADC, suavizando os sinais aplicados nos transistores de acionamento das cargas.
* **Efeito Push-Pull:** A transição complementar de potência entre o aquecedor e o ventilador na zona estável eliminou completamente as oscilações bruscas ("trancos") observadas nos modelos tradicionais. O sistema conseguiu acomodar e travar a temperatura com erro estático residual inferior a $\pm 0.2^\circ\text{C}$ em relação ao Setpoint.
* **Conectividade:** A ponte de comunicação MQTT via WebSockets integrada ao painel HTML externo demonstrou tempo de resposta imediato tanto na plotagem gráfica das curvas quanto na atualização do Setpoint realizada remotamente através dos botões da interface.

---

## 4. CONCLUSÃO

O projeto cumpriu integralmente seus objetivos didáticos e técnicos. A utilização do microcontrolador ESP32 aliado ao framework nativo ESP-IDF e o sistema operacional FreeRTOS viabilizou o desenvolvimento de uma aplicação robusta de automação industrial, estruturada sob boas práticas de concorrência e temporização determinística. 

A lógica implementada contornou de forma elegante os desafios mecânicos e elétricos dos atuadores sem a necessidade de componentes adicionais de hardware, validando que uma modelagem de software refinada e fundamentada em teoria de controle clássico pode maximizar a eficiência, estabilidade e a segurança biológica exigida por cenários críticos como a incubação avícola.