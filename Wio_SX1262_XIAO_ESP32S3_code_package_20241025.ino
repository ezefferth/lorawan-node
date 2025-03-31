#include "config.h"
#include "EEPROM.h"
#include <SPI.h>

// Região e sub-banda
const LoRaWANBand_t Region = AU915;
const uint8_t subBand = 2;

// Pinos do SX1262
SX1262 radio = new Module(41, 39, 42, 40); // (NSS, DIO1, RESET, BUSY)

// Criando o nó LoRaWAN
LoRaWANNode node(&radio, &Region, subBand);

// Chaves LoRaWAN (definidas no config.h)
uint64_t joinEUI = 0x0000000000000000;
uint64_t devEUI  = 0x70B3D57ED006F6EC;
uint8_t appKey[16] = { RADIOLIB_LORAWAN_APP_KEY };
uint8_t nwkKey[16] = { RADIOLIB_LORAWAN_NWK_KEY };

#define LORAWAN_DEV_INFO_SIZE 36
uint8_t deviceInfo[LORAWAN_DEV_INFO_SIZE] = {0};

#define UPLINK_PAYLOAD_MAX_LEN 64
uint8_t uplinkPayload[UPLINK_PAYLOAD_MAX_LEN] = {0};
uint16_t uplinkPayloadLen = 0;

uint32_t previousMillis = 0;

void setup() {
  Serial.begin(115200);

  if (!EEPROM.begin(LORAWAN_DEV_INFO_SIZE)) {
    Serial.println("❌ EEPROM falhou");
    while (1);
  }

  //deviceInfoLoad();
  Serial.println("Inicializando rádio LoRa...");
  int16_t state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Erro ao iniciar rádio: %d\n", state);
    while (1);
  }

  radio.setRfSwitchPins(38, RADIOLIB_NC);

  node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  Serial.println("Conectando via OTAA...");

  while (1) {
    state = node.activateOTAA(LORAWAN_UPLINK_DATA_RATE);
    if (state == RADIOLIB_LORAWAN_NEW_SESSION) break;
    Serial.println("Falha na conexão OTAA, tentando novamente...");
    delay(15000);
  }

  node.setADR(false);
  node.setDatarate(LORAWAN_UPLINK_DATA_RATE);
  node.setDutyCycle(false);

  Serial.println("📡 Pronto para enviar mensagens LoRaWAN!");
}

void loop() {
  const char* message = "{ \"teste\": \"ok\" }";
  uplinkPayloadLen = strlen(message);
  memcpy(uplinkPayload, message, uplinkPayloadLen);

  Serial.print("Enviando payload: ");
  Serial.println(message);

  int16_t state = node.sendReceive(uplinkPayload, uplinkPayloadLen, LORAWAN_UPLINK_USER_PORT);

  if (state != RADIOLIB_LORAWAN_NO_DOWNLINK && state != RADIOLIB_ERR_NONE) {
    Serial.println("Erro ao enviar pacote:");
    Serial.println(state);
  } else {
    Serial.println("✅ Uplink enviado com sucesso!");
  }

  delay(60000); // envia a cada 60 segundos
}
