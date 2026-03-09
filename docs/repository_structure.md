# Repository Structure Guide

Guia para quem esta entrando no projeto e quer localizar rapido cada responsabilidade.

## Visao geral

- Projeto: firmware SenseGrid para ESP32-C3
- Sensores radar suportados: ME73MS01 e LD2410C
- Arquitetura: Core desacoplado de endpoints (HTTP/MQTT/Serial)
- Linguagens: C/C++ (Arduino build)

Fluxo principal:

`UART radar -> driver/parser -> core/pipeline -> adapters/services -> HTTP/MQTT/Serial`

## Arvore de pastas (alto nivel)

```text
components/
  adapters_http/
  adapters_mqtt/
  cli/
  common/
  config/
  core/
  drivers/
  hal/
  net/
  pipeline/
  radar/
  serializer/
  util/

sketch/SenseGrid/
  SenseGrid.ino
  http_service.*
  mqtt_service.*
  net_service.*
  telemetry_ctx.*
  sg_adapters_logger.*
  glue/*

toolchain/
  arduino-cli.yaml
  build-arduino.ps1
  upload-arduino.ps1
  monitor-arduino.ps1
  include-dirs.txt
  *.ps1 de bootstrap/install

tools/
  serial_logger.py
  ab_capture.py
  prune_toolchain.ps1

docs/
  pipeline_v1.md
  observabilidade.md
  calibracao.md
  contrato_de_dados.md
  ...
```

## Responsabilidade por modulo

### `components/core/`

- `sg_core.*`: orquestra estado do sistema.
- Recebe medida parseada (`RadarParsed`), aplica pipeline/gating/range, gera snapshot e KPI.
- Nao depende de HTTP/MQTT/Serial.

### `components/pipeline/`

- `sg_pipe.*`: filtros e state machine (`empty/presence/motion`).
- `sg_calib.*`: assistente de calibracao (opcional).

### `components/radar/` e `components/drivers/`

- `sg_radar.h` / `sg_radar_types.h`: contrato neutro de radar (DIP).
- Drivers implementados:
- `drv_radar_me73.*`
- `drv_radar_ld2410c.*`

### `components/hal/`

- `hal_uart.*`: camada UART para isolar IO de hardware.

### `components/config/`

- Persistencia de parametros e perfis (NVS/Preferences).

### `components/cli/`

- Parser e dispatch dos comandos de serial CLI.
- Handlers reais sao injetados no sketch.

### `components/adapters_http/` e `components/adapters_mqtt/`

- Envelope/serializacao de transporte.

### `components/net/`

- Provisionamento e estado AP/STA.

### `components/serializer/`

- Helpers para payloads JSON.

### `components/common/` e `components/util/`

- Utilitarios compartilhados (ex.: ring buffer, logger).

## Papel do `sketch/SenseGrid/`

### `SenseGrid.ino`

- Entrypoint Arduino (`setup/loop`).
- Inicializa servicos e liga modulos.
- Faz deteccao de sensor em runtime (LD2410C/ME73).
- Fluxo de loop: ler radar -> `sg_core_step` -> publicar/streamar.

### `http_service.*`

- Endpoints REST e pagina `/diagnostics`.

### `mqtt_service.*`

- Publicacao MQTT (telemetria/KPI/eventos) e comandos.

### `net_service.*`

- Integracao de rede com `sg_net`.

### `sg_adapters_logger.*`

- Logger estruturado `.jsonl` e export HTTP.

### `telemetry_ctx.*`

- Contexto compartilhado para envelope HTTP/MQTT.

### `glue/*`

- Pontes de compilacao para integrar componentes no sketch.

## Ordem recomendada de leitura

1. `README.md`
2. `AGENTS.md`
3. `sketch/SenseGrid/SenseGrid.ino`
4. `components/radar/sg_radar.h`
5. `components/drivers/drv_radar_me73.*` e `drv_radar_ld2410c.*`
6. `components/core/sg_core.*`
7. `components/pipeline/sg_pipe.*`
8. `sketch/SenseGrid/http_service.cpp` e `mqtt_service.cpp`
9. `docs/pipeline_v1.md` e `docs/observabilidade.md`

## Onde mexer por tipo de demanda

- Ajuste de deteccao/estado: `components/pipeline/` e `components/core/`
- Novo endpoint HTTP: `sketch/SenseGrid/http_service.cpp`
- Novo comando/topico MQTT: `sketch/SenseGrid/mqtt_service.cpp`
- Novo comando serial: `components/cli/sg_cli.cpp` + handler no `SenseGrid.ino`
- Novo sensor radar: adicionar driver em `components/drivers/` e registrar no fluxo do sketch
- Persistencia de parametro: `components/config/`

## Fronteiras que nao devem ser quebradas

- Nao colocar `WiFi.*`, `WebServer`, `MQTT` dentro de `components/core/`.
- Nao acoplar parser/driver a endpoint.
- Nao espalhar gravacao NVS fora de `components/config/`.

## Build e toolchain

- Fluxo oficial em `.vscode/tasks.json`.
- Build: `toolchain/build-arduino.ps1`.
- Upload/monitor com auto-detect de porta:
- `toolchain/upload-arduino.ps1`
- `toolchain/monitor-arduino.ps1`

## Dica pratica de exploracao

Siga uma medida real ponta a ponta:

1. Frame bruto no driver (`components/drivers/*`)  
2. `RadarParsed` entrando em `sg_core_step`  
3. `SgCoreSnapshot` saindo para HTTP/MQTT/logger
