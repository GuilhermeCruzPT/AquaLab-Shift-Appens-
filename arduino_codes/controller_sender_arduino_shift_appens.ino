#include <RH_ASK.h>
#include <SPI.h>
#include <DHT.h>
#include "DFRobot_EC.h"
#include "DFRobot_PH.h"
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Definições de pinos e tipos de sensores
#define DHTPIN 2
#define DHTTYPE DHT22
#define EC_PIN A1
#define TURBIDITY_PIN A3
#define ONE_WIRE_BUS 4
#define PH_PIN A2

// Inicialização de sensores
DHT dht(DHTPIN, DHTTYPE);
DFRobot_EC ec;
RH_ASK driver;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Variáveis globais
float voltage_ec, ecValue;
float voltage_ph, phValue;
float hum, temp_amb;

static unsigned long timepoint = millis();

// Estrutura para envio de dados
struct SensorData {
  float temp_agua;
  float temp_amb;
  float humidade;
  float ph;
  float eletro;
  char turbidez[7];
};
SensorData dados;

void setup() {
  Serial.begin(9600);

  if (!driver.init()) {
    Serial.println("Falha ao iniciar o receptor");
  }

  dht.begin();
  ec.begin();
  sensors.begin();
}

void loop() {
  sensors.requestTemperatures(); 
  dados.temp_agua = sensors.getTempCByIndex(0);
  Serial.print("Temperatura agua:");
  Serial.println(dados.temp_agua);

  // Leitura de temperatura e humidade ambiente
  hum = dht.readHumidity();
  temp_amb = dht.readTemperature();
  dados.humidade = isnan(hum) ? -1.0 : hum;
  dados.temp_amb = temp_amb;

  Serial.print("Humidade: "); Serial.println(hum);
  Serial.print("Temperatura ambiente: "); Serial.println(temp_amb);

  // Leitura do sensor de condutividade elétrica
  if (millis() - timepoint > 1000U) {
    timepoint = millis();

    voltage_ec = analogRead(EC_PIN) / 1024.0 * 5000;
    ecValue = ec.readEC(voltage_ec, dados.temp_agua);
    dados.eletro = ecValue;

    float raw_voltage_ph = analogRead(PH_PIN) / 1024.0 * 5.0;
    float offset = 0.498;  // Ajustado para que 0.498V corresponda a pH 7.0
    phValue = 7.0 + ((raw_voltage_ph - offset) / 0.18);
    dados.ph = phValue;

    Serial.print("Condutividade: "); Serial.println(ecValue, 2);
    Serial.print("Tensão do pH (V): "); Serial.print(raw_voltage_ph, 3);
    Serial.print(" | pH ajustado: "); Serial.println(phValue, 2);
  }

  // Leitura da turbidez
  int leitura_turbidez = analogRead(TURBIDITY_PIN);
  if (leitura_turbidez > 700) {
    strcpy(dados.turbidez, "LIMPA");
  } else if (leitura_turbidez > 600) {
    strcpy(dados.turbidez, "MEDIA");
  } else {
    strcpy(dados.turbidez, "TURVA");
  }
  Serial.println(dados.turbidez);

  // Envio dos dados via rádio frequência
  driver.send((uint8_t*)&dados, sizeof(dados));
  driver.waitPacketSent();
  Serial.println("Dados enviados!");

  // Calibração automática da condutividade (pode ser removida em produção)
  ec.calibration(voltage_ec, dados.temp_agua);

  delay(1000);
}