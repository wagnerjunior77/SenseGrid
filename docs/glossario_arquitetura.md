# Glossário do diagrama

## Hardware

### Radar 24GHz
Módulo de radar (ME73MS01). Emite micro-ondas e mede ecos; entrega targets com distância, velocidade, “direction cosine”, pitch e força do sinal via UART.

### Env sensors T UR Lux
Sensores ambientais (temperatura, umidade relativa, luminosidade). Servem para “contexto” e filtros (ex.: reduzir sensibilidade se o ventilador estiver ligado ou se o ambiente estiver muito escuro).

---

## Drivers HAL

### RadarDriver UART
Driver de porta serial 3V3: configura baudrate, lê bytes do radar e expõe frames crus pro parser. Também envia comandos de configuração ao sensor (se aplicável).

### EnvDriver I2C
Driver I²C para ler T/UR/Lux em intervalos fixos, com retry e debounce. Entrega leituras normalizadas (ex.: °C, %UR, lux).

---

## Parser

### RadarParser bytes to Meas
Traduz os bytes dos frames do radar para uma estrutura **Meas** (medição) com campos tipados: `id`, `distance_cm`, `speed_cms`, `dircos_deg`, `pitch_deg`, `signal`, e snapshot de `T`/`RH`/`lux` quando disponível.

---

## Buses (linhas “compartilhadas” do diagrama)

### Meas bus
“Barramento lógico” por onde as medições **Meas** são distribuídas para os blocos do Core sem cruzamento visual de setas.

### Output bus
Canal lógico para publicar saídas (eventos/telemetria) rumo aos adapters (MQTT/HTTP).

### Cmd bus
Canal lógico para comandos vindos de fora (ex.: aplicar preset, iniciar calibração) até o Core via interfaces.

---

## Core Dominio

### Classifier
Converte sequências de **Meas** em estados de presença: `empty`, `presence_still`, `presence_move`. Usa filtros, baseline e histerese.

### Tracker
Mantém IDs técnicos de alvos ao longo do tempo (associa “o mesmo alvo” entre frames). Ajuda na contagem estável e evita “pulos” de ID.

### GateCounter
“Porta virtual” para entradas/saídas. Observa movimento através de um gate e incrementa/decrementa contagem.

### ZoneMasker
Aplica máscaras/ganhos por setor (grade 3×2, por exemplo) para ignorar zonas (porta/corredor) ou ajustar sensibilidade local.

### FusionLux
Regras simples de sensor-fusion com luminosidade (e opcionalmente T/UR) para ajustar confiança/thresholds e reduzir falsos positivos.

### UseCaseEngine
Carrega e aplica presets de “casos de uso” (sala de reunião, corredor, home-office): define thresholds, histerese, TTL de track, máscaras e parâmetros do GateCounter.

---

## Servicos

### OccupancyService
Agrega resultados do Core e gera eventos de ocupação: `count.changed`, `state.changed`, `track.started`/`ended`, `zone.activity`, além de telemetria periódica.

### KpiService
Calcula e publica KPIs: latência, FP/FN estimado, MAE da contagem, SNR médio, trocas de ID, etc.

### UseCaseService
Orquestra a aplicação de presets/casos de uso (carregar JSON, validar, aplicar no UseCaseEngine) e expõe comandos de calibração.

---

## Portas Interfaces (contratos que o Core enxerga)

### IOutput (eventos/telemetria)
Interface de saída do Core:
- `publicarEvento(e)`
- `publicarMeas(m)`
- `publicarStatus(s)`

> Não sabe onde vai (MQTT/HTTP); só chama o contrato.

### ICommand (comandos)
Interface de entrada de comandos externos:
- `subscribe(handler)`
- `onCommand(cmd)`

> Adapters chamam isso quando chega um comando.

### IConfigRepo
Repositório de configuração/presets:
- `load`
- `save`
- `list`

> Implementação real grava em flash/NVS.

### ILogger
Log estruturado (ex.: `.jsonl`):
- `info(evento, payload)`
- `warn(evento, payload)`
- `error(evento, payload)`

> Útil pra diagnóstico e export.

### IClock
Fonte de tempo:
- `timestamp`
- `monotonic`

> Facilita teste/simulação e evita dependência direta do `esp_timer`.

---

## Adapters

### MqttPublisher
Implementa **IOutput** publicando em tópicos (`…/meas`, `…/events`, `…/status`, `…/kpi`) e **ICommand** assinando `…/cmd`. Cuida de QoS/retain.

### HttpApi Rest Ws
Servidor HTTP local (e opcional WebSocket):
- `GET /v1/occupancy`
- `GET /v1/tracks`
- `POST /v1/cmd`
- `GET /v1/health`

Converte chamadas REST/WS em **ICommand** e escreve saídas via **IOutput**.

### FlashConfigRepo
Implementação de **IConfigRepo** usando NVS/Flash: guarda presets, máscaras e parâmetros de calibração de forma persistente.

### JsonlLogger
Implementação de **ILogger** que grava linhas JSON (`.jsonl`) em partição de dados, com opção de export pelo endpoint de diagnóstico.

---

## Estruturas de dado citadas

### Meas
Medição instantânea do radar (e contexto):
- `id`, `distance_cm`, `speed_cms`, `dircos_deg`, `pitch_deg`, `signal`, `T`, `RH`, `lux`, `t_sample`.

### Event
Saída semântica do sistema (ex.: `state.changed`, `count.changed`, `track.started`/`ended`, `zone.activity`, `kpi.tick`) com `t_publish` e `confidence`.
