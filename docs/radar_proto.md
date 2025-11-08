
# ME73MS01 — Protocolo de Quadros (Frames)

> **Resumo**: o módulo responde pela UART com cabeçalho `0x55 0xA5`, seguido de um campo de tamanho (`LEN`), identificadores de função/comando, *payload* e **checksum** (soma módulo 256). Os comandos enviados ao módulo usam `0x55 0x5A` como cabeçalho.

## Estrutura Geral

```
Resposta (do radar para o host)

+--------+--------+------+-------+------+------+-------------------+----------+
| 0x55   | 0xA5   | LENH | LENL  | FUNC | CMD1 | CMD2 |  PAYLOAD    | CHKSUM   |
+--------+--------+------+-------+------+------+-------------------+----------+
   1B       1B      1B     1B      1B     1B     1B     (LEN-4) B      1B
```

- **Header**: `0x55 0xA5` para respostas; `0x55 0x5A` para comandos.
- **LEN**: tamanho em bytes **a partir de `FUNC` até `CHKSUM`** (ou seja, inclui `FUNC`,`CMD1`,`CMD2`,`PAYLOAD` e `CHKSUM`). *Endian*: **big-endian** (`LENH` primeiro).
- **FUNC/CMD1/CMD2**: classificadores da mensagem.
- **PAYLOAD**: dados específicos do comando/função.
- **CHKSUM**: **soma de 8 bits** de `FUNC + CMD1 + CMD2 + PAYLOAD` (sem carregar), tomada `mod 256`. **Não** inclui o header `0x55 0xA5` nem o próprio campo `LEN` na soma.

### Quadro de Medição (telemetria)

Quando o módulo envia uma leitura de presença/movimento, o *payload* mínimo contém:

```
Offset (a partir de FUNC)         Conteúdo              Tipo     Obs
0x00                               FUNC                  u8
0x01                               CMD1                  u8
0x02                               CMD2                  u8
0x03                               STATUS                u8      0:none, 1:move, 2:exist
0x04..0x05                         DIST_CM               u16     distância em centímetros
0x06..0x07                         SPEED_CMS             i16     velocidade em cm/s (sinalizada)
0x08..0x09                         SIGNAL                u16     amplitude/SNR relativo (0..≈4095)
0x0A ..                            [reservado/opcional]
...                                CHKSUM                u8      soma 8-bit de (FUNC..payload)
```

> Observação: alguns firmwares colocam bytes reservados ou campos extras após `SIGNAL`. O driver valida o `LEN` e o `CHKSUM` e usa os campos mínimos acima quando presentes.

### Exemplo (hex)

```
55 A5 00 0E  03 00 00  02  00 98  00 00  03 F8  XX
|hdr| LEN=14 |F|C1|C2| ST | dist | speed | sig |chk|
                03       02          152  0       1016
```

- `STATUS=0x02 (exist)`
- `DIST_CM=0x0098 = 152 cm`
- `SPEED_CMS=0x0000 = 0 cm/s`
- `SIGNAL=0x03F8 = 1016`
- `CHKSUM=XX` deve ser igual à soma de `FUNC..SIGNAL` (módulo 256).

## Checksum (CHKSUM)

**Cálculo** (pseudo‑código):

```c
uint8_t calc_chk(const uint8_t* p /*aponta para FUNC*/, size_t len /*inclui CHKSUM*/) {
    uint32_t s = 0;
    for (size_t i = 0; i < len-1; ++i) s += p[i];
    return (uint8_t)(s & 0xFF);
}
```

Validação no frame:
1. Verifique header `0x55 0xA5`.
2. Leia `LEN = (LENH<<8) | LENL`.
3. Garante que o buffer recebido tem **exatamente** `4 + LEN` bytes (4 = header+len).
4. Calcule `CHK = calc_chk(&buf[4], LEN)` e compare com `buf[4 + LEN - 1]`.

## State Machine (parse incremental)

Para tolerar ruído/eco e realinhar rápido, o parser segue estes estados:
```
IDLE → SYNC1(0x55) → SYNC2(0xA5) → LEN_H → LEN_L → BODY (LEN bytes) → VALIDATE → (OK → FRAME) / (ERRO → IDLE)
```
- Ao detectar erro de `LEN` ou `CHKSUM`, descarte o frame e volte para `IDLE` procurando um novo `0x55`.
- O HAL do projeto oferece `uart_read_frame()` que já sincroniza em `0x55 0xA5`. O driver ainda **revalida** `LEN` e `CHKSUM` e **descarta** o frame caso haja qualquer inconsistência.

## Conversão e Normalização (SI)

O driver expõe os campos em duas formas:
- **Bruto** (compatibilidade): `distance_cm`, `speed_cms`, `signal` (u16/i16/u16).
- **SI** (normalizado):  
  - `dist_m = distance_cm / 100.0` (m)  
  - `speed_mps = speed_cms / 100.0` (m/s)  
  - `snr = signal / RADAR_SIGNAL_MAX` (0..1), com `RADAR_SIGNAL_MAX` padrão **4095.0** (12 bits).

## Tabela de Status

- `0` = **none** (sem presença)
- `1` = **move** (movimento “macro”/gesto/deslocamento)
- `2` = **exist** (presença parada, micro‑movimento/respiração)

## Comandos úteis (host → radar)

- **VO hold (ms)**: `55 5A 00 06 01 80 14 <msH> <msL> <chk>`  
- **Presence max (cm)**: `55 5A 00 06 01 80 0E <cmH> <cmL> <chk>`  
- **Salvar parâmetros**: `55 5A 00 04 01 20 04 <chk>`

> O `chk` dos comandos é **soma** de `FUNC+CMD1+CMD2+PAYLOAD`. Não inclui `55 5A` nem o campo `LEN`.
