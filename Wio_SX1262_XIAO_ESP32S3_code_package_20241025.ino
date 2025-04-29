#include "config.h"
#include "EEPROM.h"
#include <SPI.h>

unsigned long tempoTransmissaoInicio = 0;
unsigned long tempoTransmissaoFim = 0;


const LoRaWANBand_t Region = AU915;
const uint8_t subBand = 2;
SX1262 radio = new Module(41, 39, 42, 40);
LoRaWANNode node(&radio, &Region, subBand);

uint64_t joinEUI = 0x0000000000000000;
uint64_t devEUI = 0x70B3D57ED006F6EC;
uint8_t appKey[16] = { RADIOLIB_LORAWAN_APP_KEY };
uint8_t nwkKey[16] = { RADIOLIB_LORAWAN_NWK_KEY };

#define MAX_AMOSTRAS 15000
#define TAM_PACOTE 100  // envia 50 amostras por vez (200 bytes)

int16_t ecgBuffer[MAX_AMOSTRAS];
uint16_t total = 0;

enum Estado { COLETANDO,
              ENVIANDO,
              PAUSANDO };
Estado estadoAtual = COLETANDO;

unsigned long tempoInicio, ultimoEnvio;
const unsigned long INTERVALO_COLETA = 2;    // 500Hz = a cada 2ms
const unsigned long DURACAO_COLETA = 30000;  // 30 segundos
const unsigned long INTERVALO_ENVIO = 200;   // a cada 0.2 segundos
const unsigned long PAUSA_FINAL = 60000;     // 1 minuto

String decodeErro(int16_t code) {
  switch (code) {
    case RADIOLIB_ERR_NONE: return "Nenhum erro";
    case RADIOLIB_LORAWAN_NO_DOWNLINK: return "Sem downlink (esperado)";
    case RADIOLIB_ERR_TX_TIMEOUT: return "Timeout de transmissão";
    // case RADIOLIB_ERR_NO_ACK: return "Sem ACK";
    case RADIOLIB_ERR_INVALID_BANDWIDTH: return "Largura de banda inválida";
    case RADIOLIB_ERR_SPI_WRITE_FAILED: return "Falha SPI na escrita";
    // case RADIOLIB_ERR_SPI_READ_FAILED: return "Falha SPI na leitura";
    // case RADIOLIB_ERR_INVALID_PARAM: return "Parâmetro inválido";
    case RADIOLIB_ERR_PACKET_TOO_LONG: return "Pacote muito longo";
    default:
      return "Erro desconhecido: " + String(code);
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(36);
  for (int i = 0; i < 36; i++) EEPROM.write(i, 0);
  EEPROM.commit();

  radio.begin();
  radio.setRfSwitchPins(38, RADIOLIB_NC);

  node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  Serial.println("Conectando...");
  while (node.activateOTAA(LORAWAN_UPLINK_DATA_RATE) != RADIOLIB_LORAWAN_NEW_SESSION) {
    delay(15000);
    Serial.println("Tentando novamente...");
  }

  node.setADR(false);
  node.setDutyCycle(false);
  node.setDatarate(LORAWAN_UPLINK_DATA_RATE);

  tempoInicio = millis();
  ultimoEnvio = millis();
  Serial.println("✅ Sistema pronto!");
}

void loop() {
  unsigned long agora = millis();
  static int indiceEnvio = 0;

  if (estadoAtual == COLETANDO) {
    if ((agora - tempoInicio < DURACAO_COLETA) && total < MAX_AMOSTRAS) {
      static unsigned long ultimoColeta = 0;
      if (agora - ultimoColeta >= INTERVALO_COLETA) {
        ultimoColeta = agora;
        ecgBuffer[total++] = 1500;  // valor fixo simulado (1.5V em mV)
      }
    } else {
      Serial.println("⏫ Iniciando envio...");
      estadoAtual = ENVIANDO;
      tempoInicio = agora;
      indiceEnvio = 0;
      tempoTransmissaoInicio = millis();  // ⏱ início da transmissão
    }
  }

  else if (estadoAtual == ENVIANDO) {
    if ((agora - ultimoEnvio) >= INTERVALO_ENVIO && indiceEnvio < total) {
      ultimoEnvio = agora;

      uint8_t payload[2 * TAM_PACOTE];
      int blocos = min(TAM_PACOTE, total - indiceEnvio);
      for (int i = 0; i < blocos; i++) {
        payload[2 * i] = ecgBuffer[indiceEnvio + i] >> 8;
        payload[2 * i + 1] = ecgBuffer[indiceEnvio + i] & 0xFF;
      }

      int16_t status = node.sendReceive(payload, blocos * 2, LORAWAN_UPLINK_USER_PORT);
      if (status == RADIOLIB_ERR_NONE || status == RADIOLIB_LORAWAN_NO_DOWNLINK) {
        Serial.printf("📤 Pacote [%d-%d] enviado com sucesso\n", indiceEnvio, indiceEnvio + blocos - 1);
      } else {
        Serial.printf("❌ Erro ao enviar pacote [%d-%d]: %s\n", indiceEnvio, indiceEnvio + blocos - 1, decodeErro(status).c_str());
      }

      indiceEnvio += blocos;
    }

    if (indiceEnvio >= total) {
      tempoTransmissaoFim = millis();  // ⏱ fim da transmissão
      unsigned long duracao = tempoTransmissaoFim - tempoTransmissaoInicio;
      Serial.printf("✅ Transmissão completa em %.2f segundos\n", duracao / 1000.0);

      Serial.println("🕒 Pausa de 1 minutos...");
      tempoInicio = agora;
      estadoAtual = PAUSANDO;
    }
  }

  else if (estadoAtual == PAUSANDO) {
    if (agora - tempoInicio >= PAUSA_FINAL) {
      total = 0;
      tempoInicio = millis();
      estadoAtual = COLETANDO;
      Serial.println("🔁 Novo ciclo de coleta iniciado.");
    }
  }
}