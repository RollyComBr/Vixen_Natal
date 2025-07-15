#include <ESP8266WiFi.h>
#include <ESPAsyncE131.h>
#include <Adafruit_PCF8574.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP.h> // Necessário para usar a memória RTC

// --- CONFIG E1.31 ---
#define E131_UNIVERSE 1      // Universe que você configurou no Vixen
#define CHANNEL_COUNT 8      // Total de canais que vamos controlar (1 PCF8574)
ESPAsyncE131 e131(CHANNEL_COUNT);

// --- CONFIG PCF8574 ---
Adafruit_PCF8574 pcf;

// --- VARIÁVEIS PARA RESET DE FÁBRICA ---
// Estrutura para salvar na memória RTC
struct {
  uint32_t magic;           // Número mágico para verificar a validade dos dados
  uint32_t resetCount;      // Contador de resets rápidos
  uint32_t lastResetTime;   // Tempo do último reset
} rtcData;

#define RTC_MAGIC 0xAABBCCDD // Valor mágico para identificação dos dados na RTC
#define RESET_COUNT_THRESHOLD 7 // Número de resets para acionar o reset de fábrica
#define RESET_TIME_WINDOW_MS 4000 // Janela de tempo para resets rápidos (4 segundos)

void setup() {
  delay(100); // Pequeno delay para o ESP8266 inicializar

  // --- LÓGICA DE RESET DE FÁBRICA ---
  ESP.rtcUserMemoryRead(0, (uint32_t*)&rtcData, sizeof(rtcData));
  unsigned long currentTime = millis();

  if (rtcData.magic == RTC_MAGIC && (currentTime - rtcData.lastResetTime < RESET_TIME_WINDOW_MS)) {
    rtcData.resetCount++;
  } else {
    rtcData.resetCount = 1;
  }

  rtcData.lastResetTime = currentTime;
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtcData, sizeof(rtcData));

  if (rtcData.resetCount >= RESET_COUNT_THRESHOLD) {
    WiFiManager wifiManager;
    wifiManager.resetSettings(); // Apaga as credenciais WiFi salvas

    // Zera o contador RTC para não entrar em loop infinito de reset de fábrica
    rtcData.magic = 0; // Invalida os dados RTC
    rtcData.resetCount = 0;
    rtcData.lastResetTime = 0;
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtcData, sizeof(rtcData));

    ESP.restart(); // Reinicia o ESP para aplicar as novas configurações do WiFiManager
    delay(5000); // Espera o restart
  }
  // --- FIM DA LÓGICA DE RESET DE FÁBRICA ---

  // --- CONFIGURAR WIFI COM WIFIMANAGER ---
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120); // 120 segundos para configurar no modo AP

  // Se não conseguir conectar ao WiFi, ele entra em modo AP.
  // "ESP8266_E131_Config" é o nome do AP, "e131rele" é a senha do AP.
  if (!wifiManager.autoConnect("ESP8266_E131_Config", "e131rele")) {
    delay(3000);
    ESP.restart(); // Tenta reiniciar em caso de falha de conexão inicial
    delay(5000);
  }

  // Iniciar PCF8574
  if (!pcf.begin(0x20, &Wire)) {
    // Em um ambiente de produção sem serial, se o PCF8574 não for encontrado,
    // o dispositivo pode reiniciar para tentar novamente, ou travar se for crítico.
    // Para este caso, vamos travar para indicar um problema de hardware persistente.
    while (1);
  }

  for (uint8_t i = 0; i < 8; i++) {
    pcf.pinMode(i, OUTPUT);
    pcf.digitalWrite(i, HIGH); // Relé ativo em LOW (desligado)
  }

  // Iniciar recepção E1.31
  // CONSIDERE MUDAR PARA E131_MULTICAST SE SEU VIXEN ENVIAR ASSIM!
  // if (!e131.begin(E131_MULTICAST, E131_UNIVERSE)) {
  if (!e131.begin(E131_UNICAST, E131_UNIVERSE)) {
    // Em um ambiente de produção sem serial, se o E1.31 não iniciar,
    // o dispositivo pode reiniciar para tentar novamente, ou travar.
    while (1); // Trava o programa em caso de erro crítico no E1.31
  }
}

void loop() {
  // --- Processamento de Pacotes E1.31 ---
  if (e131.isEmpty()) {
    return; // Não há pacotes para processar
  }

  e131_packet_t packet;
  e131.pull(&packet);  // Recebe pacote

  // --- Interpretar Canais e Controlar Relés ---
  for (int i = 0; i < CHANNEL_COUNT; i++) {
    uint8_t valor = packet.property_values[i + 1]; // [0] é start code, começa no 1
    uint8_t currentRelayState;

    if (valor > 127) {
      currentRelayState = LOW; // Ativa relé (ativo em LOW)
    } else {
      currentRelayState = HIGH; // Desliga relé
    }

    pcf.digitalWrite(i, currentRelayState); // Ativa/Desativa relé
  }
}
