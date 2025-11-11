# Protocolo do Radar (ME73MS01 – família)

> **Objetivo**: documentar frame, campos decodificados e verificação de integridade.

---

## Moldura do frame (downlink → ESP)

```
Header   LEN (BE)   FUNC  CMD1  CMD2   DATA...   SUM
0x55 0xA5  0x00 0xNN   [1]   [1]   [1]   [N-4]     [1]
```

- `Header`: constante `0x55 0xA5`
- `LEN`: tamanho **em big-endian** do bloco a seguir (`FUNC..SUM`)
- `FUNC`, `CMD1`, `CMD2`: identificadores do relatório/comando
- `DATA…`: payload específico do relatório
- `SUM`: **checksum de 1 byte**, calculado como a **soma módulo 256** de:
  ```
  FUNC + CMD1 + CMD2 + todos os bytes de DATA
  ```
  (não inclui `Header` nem `LEN`)

---

## Exemplo real (hex) e parsing

```
55 A5 00 0E  03 81 00 00  01 00 91 00  00 00 00 01  A5  C4
^ ^  ^  ^      ^  ^  ^     ^  ^  ^  ^   ^  ^  ^  ^   ^   ^
| |  |  |      |  |  |     |  |  |  |   |  |  |  |   |   └─ SUM
| |  |  |      |  |  |     |  |  |  |   |  |  |  └─ DATA[?]
| |  |  |      |  |  |     |  |  |  └─ DATA[?]
| |  |  |      |  |  └─ CMD2
| |  |  └─ CMD1
| |  └─ LEN=0x000E (14 bytes → FUNC..SUM)
| └─ Header
└─ Header
```

Heurística usada no firmware para `FUNC=0x03` (relatório ativo):
- `status`         ← `DATA[1]` (`0=none`, `1=move`, `2=exist`)
- `distance_cm`    ← `DATA[2..3]` (BE)
- `speed_cms`      ← `DATA[4..5]` (BE, `int16`)
- `pitch_deg`      ← `DATA[7]`    (placeholder/heurístico)
- `signal`         ← `DATA[8..9]` (BE)

> No exemplo acima:
> - `status = 0x81 & 0x03` (o fabricante costuma embutir flags; o firmware mapeia para 0/1/2)
> - `distance_cm = 0x0091 = 145`
> - `signal = 0x01A5 = 421`

**Observação importante**: a família ME73 pode variar levemente o layout conforme firmware/versão. O parser é tolerante e ignora frames com `LEN` inconsistente ou `SUM` inválido.

---

## Checksum (SUM)

- Implementação no firmware:
  ```cpp
  uint8_t sum = func + cmd1 + cmd2;
  for (auto b : data) sum += b;
  sum &= 0xFF;
  ```
- Frames com SUM incorreto são **descartados**.

---

## Comandos de configuração usados pelo firmware

No boot, o firmware envia:
1. **VO hold** = 3000 ms  
2. **Presence max distance** (ex.: **200 cm = 2 m**)
3. **Salvar** parâmetros (save all)

> A composição exata dos bytes de configuração pode variar por revisão do módulo.  
> A regra geral é: `Header + LEN + (FUNC/CMD...) + PARAMS + SUM`.  
> Para uso prático recomenta-se ajustar o **range** via CLI (`range <cm>`) e deixar o firmware montar o frame correto.

---

## Mapeamento de status

- `0 = none` → vazio
- `1 = move` → movimento detectado
- `2 = exist` → presença estática (respiração/postura sutil)

---

## Notas de alcance

- O limite de presença (**Presence max**) é aplicado no módulo (ex.: 2 m).  
- Valores maiores (ex.: 4–5 m) são possíveis fisicamente, mas o projeto **padrão** usa **2 m** para reduzir interferência e falsos positivos em ambientes pequenos.
