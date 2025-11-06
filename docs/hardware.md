# SenseGrid — Hardware (A2/HW · UART do Radar)

> **Escopo desta etapa:** fixar a pinagem real já em uso no firmware atual (`sketch/SenseGrid/SenseGrid.ino`)  
> e registrar isso oficialmente.  
> **Meta A2/HW:** manter **UART0 só para logs** e isolar o radar em outra UART.

---

## 1) MCU / placa / ideia geral

- MCU atual no build: alvo configurado como `esp32:esp32:esp32c3` em `.vscode/settings.json`.
- Firmware em execução agora (SenseGrid.ino) já está operando assim:
  - `Serial` → console/log a 115200.
  - `HardwareSerial Radar(1)` → segunda UART dedicada ao módulo radar de presença 24 GHz.

Regra de arquitetura física:
- **UART0 = debug/CLI/log (Serial).**
- **Radar vai em outra UART (Radar).**
- A linha digital simples de presença do radar também é lida separada.

---

## 2) Pinagem decidida (estado atual e testado)

Trecho atual em `SenseGrid.ino`:

```cpp
static const int PIN_OUT = 4;    // O -> GPIO4
static const int RADAR_RX = 1;   // T -> RX (ESP)
static const int RADAR_TX = 3;   // R -> TX (ESP)
HardwareSerial Radar(1);
```

Tabela consolidada:

| Função no ESP              | GPIO ESP | Direção no ESP        | Pino no radar | Descrição                                           |
|---------------------------|----------|-----------------------|---------------|----------------------------------------------------|
| `PIN_OUT` (entrada)       | GPIO4    | INPUT_PULLDOWN        | `O`           | Saída digital pronta do radar (detecção/presença). |
| `RADAR_RX` (ESP RX)       | GPIO1    | ESP lê (radar → ESP)  | `T`           | Linha de telemetria TX do radar indo pro ESP.      |
| `RADAR_TX` (ESP TX)       | GPIO3    | ESP envia (ESP → radar)| `R`          | Linha de comando do ESP indo pro RX do radar.      |
| Console / Logs (`Serial`) | UART0    | USB serial            | —             | Monitor Serial @115200 para debug.                 |

Notas importantes:
- O print no `setup()` confirma:  
  `"[UART1] RX=GPIO1  TX=GPIO3  @115200  (CP2102 fora do caminho)"`  
  Ou seja: a UART usada pro radar (Radar.begin(...)) está mapeada em `GPIO1` (RX do ESP) e `GPIO3` (TX do ESP) a 115200 baud.
- A linha `PIN_OUT` em GPIO4 está sendo lida com `INPUT_PULLDOWN` e gera logs tipo `[OUT=1] t=...` quando muda.

Isso é o que já roda hoje. Esse é o que vai pro doc como baseline oficial.

---

## 3) Fiação física

Resumo do cabeamento entre ESP e módulo radar:

```text
Radar.O  ──► ESP32 GPIO4        (leitura digital de presença)
Radar.T  ──► ESP32 GPIO1 (RX)   (telemetria TX do radar -> ESP)
Radar.R  ◄─► ESP32 GPIO3 (TX)   (comandos ESP -> RX do radar)
Radar.VCC ─► 3V3
Radar.GND ─► GND comum
```

Importante:
- TX do radar (pino marcado `T` no módulo) vai no **RX do ESP** (`RADAR_RX = GPIO1`).
- RX do radar (pino marcado `R`) recebe dados do **TX do ESP** (`RADAR_TX = GPIO3`).
- `O` é uma saída digital pronta do módulo (alto/baixo = presença), indo direto pra GPIO4.

⚠ Não usar 5 V nesse radar nem nas linhas de UART. Usar 3V3 + GND comum.

---

## 4) Impacto no software

### 4.1 Inicialização atual (já existente em `SenseGrid.ino`)

```cpp
Serial.begin(115200);
pinMode(PIN_OUT, INPUT_PULLDOWN);
delay(400);
Radar.begin(115200, SERIAL_8N1, RADAR_RX, RADAR_TX);
Serial.println("\n[UART1] RX=GPIO1  TX=GPIO3  @115200  (CP2102 fora do caminho)");
```

- `Serial` continua exclusivo para log / CLI / debug humano.
- `Radar` é a UART que conversa binário com o módulo (envia frames `sendFrame(...)`, lê resposta e despeja hexdump).

### 4.2 O que precisa ser refletido no código “oficial” do repo

Para não deixar isso só enterrado no `.ino`, vamos padronizar:

Criar `sketch/SenseGrid/pins_radar.h`:

```cpp
#pragma once
// Linha discreta de ocupação
static const int SG_PIN_RADAR_OCC = 4;   // Radar.O -> GPIO4

// UART dedicada ao radar
static const int SG_RADAR_UART_NUM = 1;  // HardwareSerial(1)
static const int SG_RADAR_RX = 1;        // Radar.T -> ESP RX (GPIO1)
static const int SG_RADAR_TX = 3;        // Radar.R <- ESP TX (GPIO3)
static const long SG_RADAR_BAUD = 115200;
```

Criar `sketch/SenseGrid/radar_serial.cpp` (ponte estável pra camada C do driver):

```cpp
#include <Arduino.h>
#include "pins_radar.h"

HardwareSerial Radar(SG_RADAR_UART_NUM);

void radar_serial_begin() {
  pinMode(SG_PIN_RADAR_OCC, INPUT_PULLDOWN);
  delay(400); // dar tempo pro radar acordar
  Radar.begin(SG_RADAR_BAUD, SERIAL_8N1, SG_RADAR_RX, SG_RADAR_TX);
}

int radar_serial_available()        { return Radar.available(); }
int radar_serial_read_byte()        { return Radar.read(); }
size_t radar_serial_write_byte(uint8_t b) { return Radar.write(b); }
int radar_occupancy_pin_read()      { return digitalRead(SG_PIN_RADAR_OCC); }
```

Isso permite:
- mover o parser/driver binário pra `components/` em C puro;
- manter só essa camada Arduino/C++ como “Adapter de UART física”.

---

## 5) Próximos passos

- [ ] Adicionar `docs/hardware.md` (este arquivo) no repo.
- [ ] Criar `sketch/SenseGrid/pins_radar.h` e `sketch/SenseGrid/radar_serial.cpp` usando os valores acima.
- [ ] Atualizar qualquer diagrama de pinagem/README interno citando **GPIO1 / GPIO3 / GPIO4** como padrão.
- [ ] Garantir que nenhuma parte do `core/` acesse `Serial` direto. Log = Adapter; radar UART = Adapter.

---

## 6) Definition of Done (A2/HW)

- [x] Par de GPIO definido para UART do radar:
  - RX do ESP = GPIO1 (Radar.T)
  - TX do ESP = GPIO3 (Radar.R)
- [x] Linha digital de ocupação = GPIO4 (Radar.O)
- [x] Registrado neste documento `docs/hardware.md`
- [ ] Headers/ponte Arduino criados (`pins_radar.h`, `radar_serial.cpp`) com esses valores
- [ ] Commitado no repositório

Este documento passa a ser a referência de pinagem oficial da fase A2.
