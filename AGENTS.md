# SenseGrid — AGENTS.md (for automations & new contributors)

> **Purpose.** Passo-a-passo objetivo para agentes (e humanos) trabalharem neste repo **sem quebrar a arquitetura**.  
> **Pilares:** SOLID (com foco em DIP), **endpoint-agnostic** (Core desacoplado), build **Arduino CLI portable**.  
> **Fase atual:** **Atividade 2 — Bring-up de hardware & drivers**.

---

## 0) Visão rápida

- **Alvo**: ESP32-C3 DevKitC (`esp32:esp32:esp32c3`).  
- **Sensor principal**: radar 24 GHz (ME73MS01) via **UART**.  
- **Arquitetura**: `components/` (C puro) → **Core** orquestra; **Drivers** (radar/env) só capturam; **Parser** traduz bytes; **Adapters** (HTTP/MQTT/Logger/FlashRepo) expõem para o “mundo”.  
- **Regra de ouro**: **Core não conhece endpoints**. Qualquer side-effect externo (MQTT/HTTP/Flash) passa por **Adapters**.

---

## 1) Build oficial (Arduino CLI portable)

> Não use IDF direto aqui. As Tasks do VS Code deixam tudo portátil, sem depender de Python/instalações globais.

1. **Baixar CLI (1x):**  
   VS Code → **Run Task** → `Arduino (portable): Bootstrap CLI (1x)`

2. **Ajustar timeout (1x):**  
   VS Code → **Run Task** → `Arduino (portable): Configurar timeout (1x)`

3. **Instalar cores/libs (1x):**  
   VS Code → **Run Task** → `Arduino (portable): Instalar core esp32 + deps (1x)`

4. **Build + export bin/elf:**  
   VS Code → **Run Task** → `Arduino (portable): Build + Export (bin/elf)`

   Isso chama:  
   `toolchain/build-arduino.ps1 -SketchPath .\sketch\SenseGrid -YamlPath .\toolchain\arduino-cli.yaml -CliPath .\toolchain\arduino-cli.exe -BuildPath .\toolchain\build\SenseGrid -Fqbn (settings.json) -ExportBinaries`

- **Config padrão** (já versionada em `.vscode/settings.json`):  
  - `sensegrid.fqbn = esp32:esp32:esp32c3`  
  - `sensegrid.coreVersion = 3.3.2`

---

## 2) Upload e Monitor

- **Upload (a partir dos binários exportados):**  
  VS Code → **Run Task** → `Arduino (portable): Upload (from exported binaries)`

- **Monitor Serial** (115200):  
  VS Code → **Terminal** →  
  `toolchain\arduino-cli.exe monitor --config-file toolchain\arduino-cli.yaml -p COM5 -c baudrate=115200`

> **Dica:** se a sua porta for diferente, troque `COM5` (o repo fixa COM5 em `.vscode/settings.json`).

---

## 3) Regras de arquitetura (fiscais do SOLID)

- **DIP**: `core/` (C) só fala com **interfaces** (headers em `components/*/include`).  
- **Drivers**: apenas acesso a HW/barramentos (UART/I2C/GPIO). **Sem** lógica de negócios.  
- **Parser**: traduz bytes do radar → `Meas`.  
- **Core**: agrega `Meas`, aplica zona/preset/calibração. **Não** publica MQTT/HTTP.  
- **Adapters**: HTTP/MQTT/Logger/FlashRepo/Ports. Qualquer IO externo **sai por aqui**.  
- **Proibido no Core**: `WiFi.begin`, `HTTPServer`, `MQTTClient`, `Serial.printf` (use Adapter Logger).

---

## 4) Layout do repo (mínimo que você precisa saber)

```
components/
  core/              # Regras de negócio (C) – sem dependências externas
  drivers_radar/     # UART → frames brutos
  parser_radar/      # bytes → Meas
  drivers_env/       # I2C → T/RH/Lux
  adapters_{http,mqtt,logger,flashrepo}/
  common/, ports/    # tipos, abstrações, contratos

sketch/SenseGrid/    # Ponte Arduino (C++) chamando os símbolos C de components/
toolchain/           # arduino-cli portable, scripts e build export
.vscode/             # tasks + settings (FQBN/porta/coreVersion)
docs/                # arquitetura, contrato de dados, cronograma...
```

---

## 5) Objetivos desta fase (Atividade 2)

- UART do radar inicializada; “heartbeat” no Logger a cada 1 s.  
- Se o radar estiver plugado: **parse** e “dump” de `{distance, speed, signal}` como JSON no Logger.  
- Stubs de env (T/RH/Lux) compilando com fallback.  
- CLI mínima via Logger: `help`, `version`, `radar?`, `env?` (JSON por linha).

Checklist:  
- [ ] Build reproduzível em Windows “limpo” (apenas com as Tasks).  
- [ ] `toolchain/build/SenseGrid/` com `.bin/.elf` exportados.  
- [ ] Serial @115200 entregando JSON estável.  
- [ ] Sem chamadas de endpoint no Core (validar include graph).

---

## 6) Pins / Conexões (preencher no bring-up real)

- Radar ME73MS01 → **UART** (3V3).  
  - **ESP32-C3**: TXD = GPIOx, RXD = GPIOy (definir conforme fiação final)  
  - 3V3 e GND compartilhados.  
- I2C env (opcional): SDA = GPIOxx, SCL = GPIOyy.  
> Atualize aqui quando os pinos ficarem definitivos.

---

## 7) Troubleshooting rápido

- **Compila mas não encontra headers de `components/`:** ver `toolchain/include-dirs.txt`.  
- **“Port COM ocupada”**: feche monitores/serial anteriores.  
- **Build lento/falha de rede**: rode a Task de “Configurar timeout”.  
- **IDF aparecendo**: é **legacy**; ignore as Tasks de IDF (estão desativadas).

---

## 8) Estilo de PR para agentes

- PRs **pequenos e incrementais**, com `docs/` atualizados quando afetar contrato.  
- Nunca introduza dependência de endpoint no `core/`. Crie/estenda um Adapter.
