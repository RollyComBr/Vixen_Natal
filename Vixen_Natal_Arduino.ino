#include <Adafruit_PCF8574.h>
#include <EEPROM.h>

Adafruit_PCF8574 pcf;

// Configuracoes dinâmicas
int PORTAS_TOTAIS;
int PORTAS_PCF;
int PORTAS_595;
int qtdeCI;

#define pinSH_CP 4
#define pinST_CP 3
#define pinDS    2

byte ciBuffer[8]; // Suporta até 64 saídas 595
unsigned long ultimaAtivacao[64];
const unsigned long TEMPO_MINIMO_LIGADO = 5;

#define EEPROM_ADDR_PT 0
#define EEPROM_ADDR_PC 1
#define EEPROM_ADDR_PH 2

void atualizarConfiguracoesDerivadas() {
  PORTAS_595 = PORTAS_TOTAIS - PORTAS_PCF;
  if (PORTAS_595 < 0) PORTAS_595 = 0;
  qtdeCI = (PORTAS_595 + 7) / 8;
}

void carregarConfiguracoes() {
  PORTAS_TOTAIS = EEPROM.read(EEPROM_ADDR_PT);
  PORTAS_PCF = EEPROM.read(EEPROM_ADDR_PC);

  if (PORTAS_TOTAIS == 255 || PORTAS_TOTAIS == 0 || PORTAS_TOTAIS > 64) PORTAS_TOTAIS = 16;
  if (PORTAS_PCF == 255 || PORTAS_PCF > PORTAS_TOTAIS) PORTAS_PCF = 8;

  atualizarConfiguracoesDerivadas();
}

void salvarEEPROM(byte endereco, byte valor) {
  if (EEPROM.read(endereco) != valor) {
    EEPROM.update(endereco, valor);
  }
}

void salvarConfiguracoes() {
  salvarEEPROM(EEPROM_ADDR_PT, PORTAS_TOTAIS);
  salvarEEPROM(EEPROM_ADDR_PC, PORTAS_PCF);
  salvarEEPROM(EEPROM_ADDR_PH, PORTAS_595);
}

void ciWrite(byte pino, bool estado) {
  bitWrite(ciBuffer[pino / 8], pino % 8, estado);
  digitalWrite(pinST_CP, LOW);
  for (int nC = qtdeCI - 1; nC >= 0; nC--) {
    for (int nB = 7; nB >= 0; nB--) {
      digitalWrite(pinSH_CP, LOW);
      digitalWrite(pinDS, bitRead(ciBuffer[nC], nB));
      digitalWrite(pinSH_CP, HIGH);
    }
  }
  digitalWrite(pinST_CP, HIGH);
}

void setup() {
  Serial.begin(57600);

  carregarConfiguracoes();

  pinMode(pinSH_CP, OUTPUT);
  pinMode(pinST_CP, OUTPUT);
  pinMode(pinDS, OUTPUT);

  if (!pcf.begin(0x20, &Wire)) {
    Serial.println("Erro: PCF8574 não encontrado");
    while (1);
  }

  for (uint8_t p = 0; p < 8; p++) {
    pcf.pinMode(p, OUTPUT);
    pcf.digitalWrite(p, HIGH);
  }

  for (uint8_t i = 0; i < 8; i++) {
    ciBuffer[i] = 0;
  }

  ciWrite(0, false);
}

void loop() {
  processarSerial();
}

void processarSerial() {
  static byte buffer[64];
  static byte index = 0;
  static unsigned long lastByteTime = 0;
  static String inputString = "";

  while (Serial.available()) {
    char inChar = (char)Serial.read();

    if (isAlpha(inChar) || inChar == ' ' || isDigit(inChar) || inChar == '\n') {
      if (inChar == '\n') {
        inputString.trim();
        interpretarComando(inputString);
        inputString = "";
      } else {
        inputString += inChar;
      }
      continue;
    }

    unsigned long now = millis();
    if (now - lastByteTime > 20) {
      index = 0;
    }
    lastByteTime = now;

    buffer[index++] = inChar;
    if (index == PORTAS_TOTAIS) {
      aplicarComandosReles(buffer);
      index = 0;
    }
  }
}

void interpretarComando(String comando) {
  if (comando.startsWith("CFG PT")) {
    int valor = comando.substring(7).toInt();
    if (valor > 0 && valor <= 64) {
      PORTAS_TOTAIS = valor;

      if ((PORTAS_PCF + PORTAS_595) > PORTAS_TOTAIS) {
        PORTAS_PCF = 0;
        PORTAS_595 = 0;
        EEPROM.update(EEPROM_ADDR_PC, 0);
        EEPROM.update(EEPROM_ADDR_PH, 0);
        Serial.println("Valores de PCF e 595 zerados para evitar inconsistência");
      }

      atualizarConfiguracoesDerivadas();
      salvarConfiguracoes();
      Serial.println("PORTAS_TOTAIS atualizado para " + String(PORTAS_TOTAIS));
    } else {
      Serial.println("Valor invalido para PT");
    }
  } else if (comando.startsWith("CFG PC")) {
    int valor = comando.substring(7).toInt();
    if (valor >= 0 && valor <= PORTAS_TOTAIS) {
      PORTAS_PCF = valor;
      atualizarConfiguracoesDerivadas();
      salvarConfiguracoes();
      Serial.println("PORTAS_PCF atualizado para " + String(PORTAS_PCF));
    } else {
      Serial.println("Valor invalido para PC");
    }
  } else if (comando.startsWith("CFG PH")) {
    int valor = comando.substring(7).toInt();
    if (valor >= 0 && valor <= PORTAS_TOTAIS) {
      PORTAS_595 = valor;
      PORTAS_PCF = PORTAS_TOTAIS - PORTAS_595;
      atualizarConfiguracoesDerivadas();
      salvarConfiguracoes();
      Serial.println("PORTAS_595 atualizado para " + String(PORTAS_595));
    } else {
      Serial.println("Valor invalido para PH");
    }
  } else if (comando == "GET CFG") {
    Serial.println("CFG ATUAL:");
    Serial.println("PORTAS_TOTAIS: " + String(PORTAS_TOTAIS));
    Serial.println("PORTAS_PCF: " + String(PORTAS_PCF));
    Serial.println("PORTAS_595: " + String(PORTAS_595));
    Serial.println("qtdeCI: " + String(qtdeCI));
  } else {
    Serial.println("Comando desconhecido: " + comando);
  }
}

void aplicarComandosReles(byte* buffer) {
  for (int j = 0; j < PORTAS_TOTAIS; j++) {
    bool novoEstado = buffer[j];

    if (j < PORTAS_PCF) {
      if (novoEstado == 1) {
        pcf.digitalWrite(j, LOW);
        ultimaAtivacao[j] = millis();
      } else {
        if (millis() - ultimaAtivacao[j] >= TEMPO_MINIMO_LIGADO) {
          pcf.digitalWrite(j, HIGH);
        }
      }
    } else {
      byte pin595 = j - PORTAS_PCF;
      if (novoEstado == 1) {
        ciWrite(pin595, true);
        ultimaAtivacao[j] = millis();
      } else {
        if (millis() - ultimaAtivacao[j] >= TEMPO_MINIMO_LIGADO) {
          ciWrite(pin595, false);
        }
      }
    }
  }
}
