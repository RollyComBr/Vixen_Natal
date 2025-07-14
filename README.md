# Vixen_Natal

# Controle de Relés via Serial com PCF8574 e 74HC595

Este projeto permite o controle de até 64 saídas digitais (relés) usando expansores I/O do tipo **PCF8574** e **74HC595**, com comunicação via porta serial. Suporta configuração dinâmica e persistência em **EEPROM** para manter os parâmetros mesmo após reinicialização.

---

## Funcionalidades

- Controle de relés com acionamento via serial.
- Suporte a até 64 saídas combinando PCF8574 e 74HC595.
- Armazenamento das configurações na EEPROM.
- Comandos para configuração e consulta dos parâmetros via terminal serial.

---

## Pinagem

| Função       | Pino Arduino |
|--------------|--------------|
| SH_CP (clock) | 4            |
| ST_CP (latch) | 3            |
| DS (dados)    | 2            |
| PCF8574 I2C   | padrão SDA/SCL |

---

## Comandos Seriais

Use os comandos abaixo no terminal serial (baudrate 57600) para configurar e operar o sistema.

### 📌 Configuração

- `CFG PT <n>`  
  Define o número total de portas usadas (máx: 64).  
  Ex: `CFG PT 16`

- `CFG PC <n>`  
  Define quantas dessas portas são controladas por PCF8574.  
  Ex: `CFG PC 8`

- `CFG PH <n>`  
  Define quantas portas são controladas por 74HC595.  
  Ex: `CFG PH 8`

> ⚠️ Se a soma `PCF + 595` for maior que `PORTAS_TOTAIS`, os valores de `PCF` e `595` são automaticamente zerados para evitar inconsistência.

### 🔎 Consulta

- `GET CFG`  
  Exibe as configurações atuais armazenadas:
  `CFG ATUAL:`
  `PORTAS_TOTAIS: 16`
  `PORTAS_PCF: 8`
  `PORTAS_595: 8`
  `qtdeCI: 1`

---

## Formato dos Dados

A cada comando de controle de relés, deve-se enviar `PORTAS_TOTAIS` bytes pela serial. Cada byte representa o estado de uma porta:

- `1`: Ativa
- `0`: Desativa

Exemplo (para 16 portas):
```plaintext
0000111100001111
