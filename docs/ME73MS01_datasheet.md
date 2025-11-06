# ME73MS01 — Datasheet em Markdown (Resumo completo)

> **Fonte:** Datasheet oficial “ME73MS01_Datasheet_C_EN V1.0 (2024‑07‑01)” — MinewSemi.  
> **Propósito:** condensar **todo o conteúdo técnico** do PDF em um `.md` navegável, preservando números, comandos e notas práticas.
> **Módulo:** Radar mmWave **24 GHz** para **detecção de presença humana** (FMCW) com **medição de distância** e **micro‑movimento** (ex.: respiração).

---

## 1. Visão geral

- **Modelo:** ME73MS01 (24 GHz mmWave FMCW).  
- **Uso típico:** detecção de presença em **smart home**, iluminação pública, segurança (indoor/outdoor), automação/industrial.  
- **Diferencial:** além de detecção de movimento, detecta **micro‑movimentos** (respiração) para inferir presença mesmo com pessoa parada.  
- **Cobertura (referência):** **±60°** (azimute) e **±60°** (elevação).  
- **Faixa de detecção:** **0,5 m a 6 m** (depende da instalação, ângulo, volume do corpo, ambiente e carenagem).  
- **Frequência:** **24,00–24,25 GHz** (ISM).  
- **Dimensões do módulo (PCB):** **20 × 20 mm**.  
- **Montagem recomendada:** teto ou parede.  
- **Interface:** **UART** (115200, 8N1, sem paridade) + 1 saída digital **O** (presença).

---

## 2. Especificações elétricas

- **Tensão de operação (típica):** **5 V** (datasheet também cita **3,6–5,5 V**).  
- **Corrente média:** **22 mA**.  
- **Consumo máx.:** **0,40 W**.  
- **Temperatura de operação:** **−40 °C a +85 °C**.  
- **IOs expostos:** **3** (UART TX/RX + OUT).  
- **Saída “O” (presença):** **3 V** com alvo presente, **0 V** sem alvo *(opção 5 V sob encomenda)*.

> **Nota de projeto:** o pino **V** é indicado como 4–5,5 V na tabela de pinos; a folha de “Electrical Specification” lista 3,6–5,5 V. Adote **5 V padrão** e respeite **3,6–5,5 V** como janela; evite 5 V nas linhas lógicas — **UART é 3,3 V**.

---

## 3. Pinos e sinalização

| Nº | Símbolo | Tipo | Descrição |
|---:|:-------:|:----:|---|
| 1 | **O** | OUT | Saída digital de presença: **3 V** com alvo, **0 V** sem alvo *(variante 5 V sob pedido)* |
| 2 | **T** | UART TX | **TX do módulo** (vai ao **RX do host**) |
| 3 | **R** | UART RX | **RX do módulo** (vem do **TX do host**) |
| 4 | **G** | GND | Terra |
| 5 | **V** | VCC | Alimentação **(tip. 5 V; 3,6–5,5 V)** |

**Conector mínimo de depuração/uso:** **V**, **G**, **T**, **R**, **O**.

---

## 4. Protocolo UART (depuração/configuração)

- **Serial:** **115200**, 8 data, 1 stop, **sem paridade**.  
- **Endianness:** **big‑endian** para todos os parâmetros multi‑byte.  
- **Estrutura do quadro (host→radar e radar→host):**

```
[Header:2][Len:2][Func:1][CMD1:1][CMD2:1][Data:N][SUM:1]
```

- **Header:**  
  - Host → Radar: `0x55 0x5A`  
  - Radar → Host: `0x55 0xA5`
- **Len (2B):** comprimento de `Func + CMD + Data + SUM` (big‑endian).  
- **Func (1B):** `0x00=Read`, `0x01=Write`, `0x02=Passive report`, `0x03=Active report`.  
- **CMD:** 2 bytes (`CMD1` = classe; `CMD2` = função específica).  
- **Data:** N bytes conforme comando.  
- **SUM (1B):** **soma de todos os bytes anteriores** (mod 256, manter só 8 bits baixos).

### 4.1. Formato do relatório ativo (exemplo real)
Exemplo de mensagem recebida do radar (**Active report**):

```
55 A5 00 0E 03 81 00 00 01 00 5E 00 00 00 00 01 78 64
```

Interpretação (campos típicos `Data[0..9]`):
- **Target ID**: byte 8 (0x00)  
- **Status**: byte 9 `0x01` → **movimento** (`0`: ninguém; `1`: corpo em movimento; `2`: existência/parado)  
- **Distância**: bytes 10–11 `0x00 0x5E` → **94 cm = 0,94 m**  
- **Velocidade**: 16‑bit com sinal (**cm/s**)  
- **Direção/Ângulo (cos, pitch)**: 8‑bit com sinal (**graus**)  
- **Sinal**: bytes 16–17 `0x01 0x78` → **376**

### 4.2. Principais comandos de configuração/leitura
Abaixo, **padrões de comandos** do datasheet (host→radar = `55 5A ...`; resposta radar→host = `55 A5 ...`). Valores **DATAx** são **big‑endian**.

| Parâmetro | Leitura | Escrita | Resposta | Valor padrão / Observações |
|---|---|---|---|---|
| **Versão de SW** | `55 5A 00 04 00 00 01 B4` | — | `55 A5 00 11 02 00 01 DATA1…DATA13 SUM` | 13 bytes |
| **Salvar todos os parâmetros** | — | `55 5A 00 04 01 20 04 D8` | (ack) | Necessário após ajustes |
| **Restaurar padrões** | — | `55 5A 00 04 01 20 01 D5` | `55 A5 00 04 02 20 01 21` | — |
| **Distância máx. detecção (cm)** | `55 5A 00 04 00 80 0D 40` | `55 5A 00 06 01 80 0D DATA1 DATA2 SUM` | `55 A5 00 06 02 80 0D DATA1 DATA2 SUM` | **600 cm** (motion) |
| **Distância mín. detecção (cm)** | `55 5A 00 04 00 80 0C 3F` | `55 5A 00 06 01 80 0C DATA1 DATA2 SUM` | `55 A5 00 06 02 80 0C DATA1 DATA2 SUM` | **10 cm** |
| **Distância máx. presença (cm)** | `55 5A 00 04 00 80 0E 41` | `55 5A 00 06 01 80 0E DATA1 DATA2 SUM` | `55 A5 00 06 02 80 0E DATA1 DATA2 SUM` | **450 cm** |
| **Threshold movimento ≤1 m** | `55 5A 00 04 00 80 03 36` | `55 5A 00 06 01 80 03 DATA1 DATA2 SUM` | `55 A5 ... 02 80 03 ...` | **200** |
| **Threshold movimento =1 m** | `55 5A 00 04 00 80 04 37` | `55 5A 00 06 01 80 04 DATA1 DATA2 SUM` | `55 A5 ... 02 80 04 ...` | **120** |
| **Threshold presença ≤1 m** | `55 5A 00 04 00 80 09 3C` | `55 5A 00 06 01 80 09 DATA1 DATA2 SUM` | `55 A5 ... 02 80 09 ...` | **300** |
| **Threshold detecção ≤1 m** | `55 5A 00 04 00 80 0A 3D` | `55 5A 00 06 01 80 0A DATA1 DATA2 SUM` | `55 A5 ... 02 80 0A ...` | **300** |
| **VO: tempo em nível (ms)** | `55 5A 00 04 00 80 14 47` | `55 5A 00 06 01 80 14 DATA1 DATA2 SUM` | `55 A5 ... 02 80 14 ...` | **20000 ms** (5 s no exemplo) |
| **VO: modo de indicação** | `55 5A 00 04 00 80 15 48` | `55 5A 00 05 01 80 15 DATA1 SUM` | `55 A5 00 05 02 80 15 DATA1 SUM` | `0x00`=alto quando presença; `0x01`=baixo |

**Exemplos do manual:**  
- **Set motion max = 5 m (500 cm):** `55 5A 00 06 01 80 0D 01 F4 38` → resp.: `55 A5 00 06 02 80 0D 01 F4 84`  
- **Set presence max = 4 m (400 cm):** `55 5A 00 06 01 80 0E 01 90 D5` → resp.: `55 A5 00 06 02 80 0E 01 90 21`  
- **Set VO hold = 5 s (5000 ms):** `55 5A 00 06 01 80 14 13 88 E5` → resp.: `55 A5 00 06 02 80 14 13 88 31`  
- **Salvar em flash:** `55 5A 00 04 01 20 04 D8` (obrigatório após escrever).

---

## 5. Instalação e testes (cenários de referência)

### 5.1. Teto (altura de referência **3 m**)
- **Raio** para **corpo parado**: até **3 m** (configurável).  
- **Raio** para **corpo em movimento**: até **3 m** (configurável).  
- Coberturas **ilustrativas** variam com carenagem/material e ambiente.

### 5.2. Parede (altura ~**1 m**)
- Coberturas de referência para **parado** (laranja) e **em movimento** (azul).  
- Usar primeiro com **parâmetros padrão**; ajustar **thresholds** conforme necessidade.

> **Importante:** FOV e distâncias no manual foram obtidos no **ambiente de teste** do fabricante. Em campo, **carenagem**, **altura**, **materiais** e **ambiente** alteram resultados — reajuste thresholds e distâncias máximas.

---

## 6. “Upper Computer” (software de PC)

- Ferramenta: “**24G Millimeter Wave Human Detection**” (da MinewSemi).  
- Passos: alimentar em **5 V**; abrir porta serial; visualizar **distância** e **estado**.  
- Permite configurar **sensibilidades** (curta/média distância), **distâncias** (presença/movimento), **delay**, **VO mode/hold**, etc.; clique em **Save** para gravar em flash.  
- Janela de **estatísticas de distância/sinal** apresenta curvas de **distância (parado vs movimento)** e **intensidade** em tempo real.
- Modo “**People Stationary**”: define N ocorrências consecutivas de OCC para considerar “parado” na interface.

---

## 7. Boas práticas e precauções

1) Evitar **metal** ou objetos à frente da antena (bloqueio/eco).  
2) **Carenagem** (material/espessura/distância) altera energia e thresholds — iniciar com defaults e **fazer tuning** no produto final.  
3) **ABS/PC** recomendados; **não usar metal** ou pintura metalizada. Espessura **2–3 mm**; manter **folga ~2,5 mm** da antena à parede interna.  
4) Evitar **correntes de ar** (AC, fans) e objetos vibrando.  
5) Sentado **de costas** ou **de lado** ao radar pode reduzir detecção de micro‑movimento (respiração).  
6) Múltiplos módulos: manter **>0,5 m** entre eles e **evitar antenas face‑a‑face**.  
7) FOV e alcance variam; trate valores como **referência**.

### 7.1. Requisitos de layout/SMT
- Antena do módulo deve ficar **~1 mm acima** dos demais componentes do PCB.  
- **Não contaminar** o chip durante SMT; soldagem **plana**, sem empeno.  
- Superfície da carenagem **plana** (evitar curvatura sobre a área irradiada).

---

## 8. Armazenamento e manuseio

- **Armazenagem (sem abrir):** **5–35 °C**, **20–70 %RH**; usar em **até 6 meses** após recebimento (se >6 meses, revalidar).  
- **Sensibilidade à umidade:** **MSL2 (J‑STD‑020)**. Após abrir, manter **≤30 °C / <60 %RH**; usar preferencialmente em **3–6 meses**.  
- **Bake** (se exposto a umidade/tempo):  
  - **120 °C (±5 °C) por 8 h** *ou* **90 °C (+8/−0 °C) por 24 h**.  
- **Transporte/manuseio:** evitar choques; não tocar terminais com mãos nuas; ESD pode danificar.

---

## 9. Qualidade e certificações

- Fábrica com **ISO9001/14001/27001**, **OHSA18001**, **BSCI**; testes de **potência TX**, **sensibilidade**, **consumo**, **estabilidade** e **envelhecimento**.  
- Capacidade de produção em milhões/ano.  
- Marcas e certificações (RoHS, REACH, CE, FCC, etc.) são dos respectivos proprietários.

---

## 10. Conformidade FCC (EUA)

- **Parte 15** (15.249). Operação sujeita a: (1) não causar interferência danosa; (2) aceitar interferências.  
- **Antena:** PCB integrada (sem troca pelo integrador).  
- **Exposição RF:** manter **≥20 cm** do corpo; não co‑locar com outros transmissores.  
- **Etiquetagem:** se o ID FCC não ficar visível no produto final, rotular com “Contains FCC ID: **2BDJ6‑ME73MS01**”.  
- **Responsabilidade do integrador:** garantir conformidade do **host** com **Part 15B** (emissões digitais).

---

## 11. Documentos relacionados e suporte

- Catálogo de módulos, manual de nomenclatura e atualizações estão no site da **MinewSemi**.  
- Contato: **minewsemi@minew.com** — Shenzhen, China.  
- Recomenda‑se registrar‑se no site para receber **PCN** e atualizações de documentos.

---

## 12. Apêndice — Dicas de integração com ESP32‑C3 (SenseGrid)

- **UART do radar** em **UART1** (ex.: **RX=GPIO1**, **TX=GPIO3**) e **O** em **GPIO4**; **UART0** fica para logs.  
- **Níveis lógicos:** UART a **3,3 V**; **não** alimente pinos de I/O com 5 V.  
- **Persistência:** após `Write`, enviar **Save** (`CMD1=0x20, CMD2=0x04`).  
- **Checksum:** some **todos** os bytes até antes do `SUM` e aplique `& 0xFF`.

> Este `.md` foi gerado a partir do PDF original e organizado para consulta rápida no projeto SenseGrid.
