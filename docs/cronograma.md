# Cronograma do Projeto — SenseGrid

**Status atual:** iniciando **Atividade 2 — Bring-up de hardware & drivers** (28/10–10/11).

---

## Atividade 1 — Planejamento técnico & contrato de dados
**Semanas 1–2 (14/10–27/10) — 2 semanas**  
Etapa dedicada a consolidar requisitos e indicadores de sucesso, como **latência < 1 s** e **falsos positivos/negativos < 5%**. Define-se a arquitetura do sistema (integração do radar com o microcontrolador, processamento para estados de presença/movimento e exposição por API/MQTT). Também se estabelece o **contrato de dados em JSON**, com exemplos de mensagens de medições, eventos e status.  
**Entregáveis:** Documento de arquitetura (v0.1) e contrato de payload JSON (v0.1) com exemplos.

## Atividade 2 — Bring-up de hardware & drivers
**Semanas 3–4 (28/10–10/11) — 2 semanas**  
Realiza-se a integração elétrica e de software do radar, configuração da porta serial e parsing das mensagens (distância, velocidade, força de sinal etc.). Implementam-se drivers para sensores ambientais (temperatura/umidade/lux) e uma ferramenta de diagnóstico em linha de comando para visualização em tempo real. O objetivo é obter **leituras estáveis e consistentes**.  
**Entregáveis:** Firmware básico (“hello-radar”) operacional e **CLI de diagnóstico** com leituras consistentes.

## Atividade 3 — Pipeline v1 (detecção básica)
**Semanas 5–7 (11/11–01/12) — 3 semanas**  
Os dados brutos são convertidos em estados operacionais: “vazio”, “presença estática” e “movimento”. Aplicam-se filtros (redução de ruído), cria-se baseline de ambiente vazio e utiliza-se histerese para estabilidade (evitando flicker). O **tempo de sustentação do estado** permanece configurável. A validação é feita em bancada com cenários reais.  
**Entregáveis:** Detecção estável de presença estática por **≥ 10 minutos** sem desligamentos indevidos.

## Atividade 4 — Assistente de calibração + API/MQTT (primeira versão)
**Semanas 8–9 (02/12–15/12) — 2 semanas**  
Disponibiliza-se um **assistente de calibração** para instalação simples: medição do ambiente vazio, ajuste de sensibilidade e distâncias úteis, com persistência em memória. Em paralelo, a **API local (HTTP/WebSocket)** e a publicação em **MQTT** são configuradas com tópicos padronizados (telemetria, eventos, status), facilitando a integração por terceiros. Testes são realizados com Mosquitto.  
**Entregáveis:** Calibração guiada funcional e **payloads MQTT** publicados corretamente em broker de teste.

## Atividade 5 — Sensor-fusion v1 (temperatura/umidade/lux)
**Semanas 10–11 (16/12–29/12) — 2 semanas**  
As leituras ambientais são utilizadas para qualificar decisões do radar. Em cenários com ventilação/cortina ou baixa iluminação, os limiares de presença são ajustados para **reduzir falsos positivos**, preservando a sensibilidade a pessoas.  
**Entregáveis:** Evidência de **redução de falsos positivos** em cenários com influência ambiental.

## Atividade 6 — Zoneamento avançado (máscaras/ganhos por setor)
**Semanas 12–13 (30/12–12/01) — 2 semanas**  
O ambiente é dividido em zonas (p. ex., grade 3×2), permitindo **ganho/limiar por setor** e **máscaras** para áreas problemáticas (portas finas, vidros, áreas adjacentes). Uma visualização tipo **heatmap** indica a intensidade por região. A etapa considera o período de fim de ano, com foco no essencial.  
**Entregáveis:** Zoneamento com efeito em tempo real e **persistente após reinicialização**.

## Atividade 7 — Observabilidade (KPIs & heatmap)
**Semanas 14–15 (13/01–26/01) — 2 semanas**  
Entrega-se uma tela de **Diagnóstico** com KPIs principais (latência, FP/FN, SNR médio) e heatmap simples. Ativa-se **log estruturado** e exportação de sessões para análise offline em formato **.jsonl**.  
**Entregáveis:** Página “Diagnóstico” com heatmap e **exportação .jsonl** de logs/telemetria.

## Atividade 8 — Testes controlados multiambiente (A/B com PIR)
**Semanas 16–18 (27/01–09/02) — 2 semanas**  
Executam-se testes A/B com **PIR de referência** em múltiplos cenários (quarto, sala, corredor, sala de reunião), coletando dados comparáveis (tempo de acionamento/desligamento, falsos acionamentos, perdas). O objetivo é comprovar ganhos do radar com **métricas objetivas**.  
**Entregáveis:** Relatório parcial com **≥ 30% de redução de falsos** em comparação ao PIR.

## Atividade 9 — Catálogo de Casos de Uso
**Semanas 18–19 (10/02–23/02) — 2 semanas**  
Consolidação dos resultados de campo para transformar a configuração genérica em um **catálogo de casos de uso** voltados a contextos típicos de ocupação (ex.: sala de reunião, corredor de passagem, estações de trabalho/home-office). Cada caso de uso define **parâmetros operacionais** do classificador (sensibilidade por faixa, histerese, limites de distância e velocidade, regras de zoneamento e fusão com luminosidade) e **semântica de eventos/telemetria**. O objetivo é maximizar a **acurácia de contagem** e **estabilidade de rastreamento** em cada contexto, reduzindo o tempo de comissionamento por ambiente e padronizando a interpretação dos dados pela API.  
**Entregáveis:** Catálogo versionado com **3 casos de uso** e **guia de seleção** explicando “quando usar” cada caso e os efeitos esperados nos dados.

## Atividade 10 — Testes de campo & estabilização
**Semanas 20–21 (24/02–09/03) — 2 semanas**  
Um **piloto com usuários reais** é conduzido para validações finais, aplicação de correções e revisão de documentação (calibração, operação e troubleshooting). O objetivo é finalizar uma **build candidata a release**, estável e aderente às metas definidas.  
**Entregáveis:** **Versão candidata a lançamento (RC)** e notas de versão consolidadas.

## Atividade 11 — Pacote de integração & documentação
**Semana 22 (10/03–16/03) — 1 semana**  
Consolida-se o **OpenAPI final**, exemplos de **pub/sub MQTT**, **scripts de replay** e o **guia de comissionamento** passo a passo, com meta de configuração de ambiente em ≤ 10 minutos.  
**Entregáveis:** “**Integrator Kit**” v1.0 com especificações, exemplos e scripts.

## Atividade 12 — Fechamento & entrega final
**Semanas 23–24 (17/03–31/03) — 2 semanas**  
Elabora-se o **relatório final** (métricas, gráficos, conclusões), grava-se **vídeo demonstrativo** e compila-se **checklist de critérios de aceitação**: detecção estável, redução de falsos vs. PIR, zoneamento persistente e API/MQTT operantes. **Conclui-se o SenseGrid v1** pronto para integração no ecossistema Dyona.
