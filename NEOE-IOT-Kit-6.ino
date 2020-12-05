/**********************************************************************************************************************************
  Arduino-Sketch für das NEOE-IOT-Kit-6, "eCO2-Sensor, Thermometer, Hygrometer mit OLED-Display. Arduino-Programmierung.
  MQTT-kompatibel zur Anbindung an Home Assistant. Aufbau-Variante "Breadboard"."
  Dieser Arduino-Sketch wird in folgendem Tutorial verwendet:
  https://www.neoe.io/blogs/tutorials/eco2-sensor-thermometer-hygrometer-mit-oled-display-mqtt-kompatibel-variante-breadboard
  Fragen und Anregungen bitte in unserer Facebook-Gruppe adressieren, damit die gesamte Community davon profitiert.
  https://www.facebook.com/groups/neoe.io/
  Datum der letzten Änderung: 5. Dezember, 2020
**********************************************************************************************************************************/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include "ccs811.h"
#include "ClosedCube_HDC1080.h"
#include <ArduinoJson.h>

// WLAN-Zugangsdaten hier hinterlegen
const char* ssid = "NAME DES WLAN NETZWERKS"; // Anführungszeichen beibehalten
const char* password = "WLAN-PASSWORT"; // Anführungszeichen beibehalten, also z.B. so: "Geheim"

// Die für den MQTT-Server erforderlichen Daten hier hinterlegen
const char* mqtt_client = "NEOE-IOT-KIT-6-1"; // Wenn mehrere "NEOE IOT-Kits 6" im Einsatz sind, einfach mit der letzten Zahl durchnummerieren
const char* mqtt_server = "IP-ADRESSE DES MQTT-SERVERS"; // Anführungszeichen beibehalten, also z.B. so: "192.168.0.123"
const uint16_t mqtt_port = 1883;
const char* mqtt_user = "BENUTZERNAME"; // Anführungszeichen beibehalten
const char* mqtt_password = "PASSWORT"; // Anführungszeichen beibehalten, also z.B. so: "Geheim"

// MQTT-Topics für Home Assistant MQTT Auto Discovery
const char* mqtt_config_topic_temperatur = "homeassistant/sensor/temperatur-wohnzimmer/config";  // Name des Zimmers bei Bedarf ändern
const char* mqtt_config_topic_luftfeuchtigkeit = "homeassistant/sensor/luftfeuchtigkeit-wohnzimmer/config";  // Name des Zimmers bei Bedarf ändern
const char* mqtt_config_topic_eco2 = "homeassistant/sensor/eco2-wohnzimmer/config"; // Name des Zimmers bei Bedarf ändern
const char* mqtt_config_topic_tvoc = "homeassistant/sensor/TVOC-wohnzimmer/config"; // Name des Zimmers bei Bedarf ändern
const char* mqtt_state_topic = "homeassistant/sensor/tlec-wohnzimmer/state";  // Name des Zimmers bei Bedarf ändern

// Speicher-Reservierung für JSON-Dokument, kann mithilfe von arduinojson.org/v6/assistant eventuell noch optimiert werden
StaticJsonDocument<512> doc_config_temperatur;
StaticJsonDocument<512> doc_config_luftfeuchtigkeit;
StaticJsonDocument<512> doc_config_eco2;
StaticJsonDocument<512> doc_config_tvoc;
StaticJsonDocument<512> doc_state;

char mqtt_config_data_temperatur[512];
char mqtt_config_data_luftfeuchtigkeit[512];
char mqtt_config_data_eco2[512];
char mqtt_config_data_tvoc[512];
char mqtt_state_data[512];

WiFiClient espClient;
PubSubClient client(espClient);

CCS811 ccs811(0); // nWAKE auf D3 = GPIO 0
ClosedCube_HDC1080 hdc1080;
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Korrekturfaktor für Thermometer, aufgrund von Wärmeentwicklung des CCS811
uint16_t temperaturKorrekturfaktor = 0.5;

// Funktion um Werte per MQTT zu übermitteln
void publishData(float p_temperatur, float p_luftfeuchtigkeit, float p_eco2, float p_tvoc) {
  doc_state["temperatur"] = p_temperatur;
  doc_state["luftfeuchtigkeit"] = p_luftfeuchtigkeit;
  doc_state["eco2"] = p_eco2;
  doc_state["tvoc"] = p_tvoc;
  Serial.println(p_temperatur);
  Serial.println(p_luftfeuchtigkeit);
  Serial.println(p_eco2);
  Serial.println(p_tvoc);
  serializeJson(doc_state, mqtt_state_data);
  client.publish(mqtt_state_topic, mqtt_state_data);
}

// Funktion zur WLAN-Verbindung
void setup_wifi() {
  delay(10);
  /* Mit WLAN verbinden */
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

// Funktion zur MQTT-Verbindung
void reconnect() {
  while (!client.connected()) {
    if (client.connect(mqtt_client, mqtt_user, mqtt_password)) {
    } else {
      delay(5000);
    }
  }
}

// Funktion für Home Assistant MQTT Auto Discovery - Temperatur
void configMqttTemperatur() {
  doc_config_temperatur["name"] = "Temperatur Wohnzimmer"; // Name des Zimmers bei Bedarf ändern
  doc_config_temperatur["device_class"] = "temperature";
  doc_config_temperatur["state_topic"] = mqtt_state_topic;
  doc_config_temperatur["unit_of_measurement"] = "C";
  doc_config_temperatur["value_template"] = "{{ value_json.temperatur}}";
  serializeJson(doc_config_temperatur, mqtt_config_data_temperatur);
  client.publish(mqtt_config_topic_temperatur, mqtt_config_data_temperatur, true);
  delay(1000);
}

// Funktion für Home Assistant MQTT Auto Discovery - Luftfeuchtigkeit
void configMqttLuftfeuchtigkeit() {
  doc_config_luftfeuchtigkeit["name"] = "Luftfeuchtigkeit Wohnzimmer"; // Name des Zimmers bei Bedarf ändern
  doc_config_luftfeuchtigkeit["device_class"] = "humidity";
  doc_config_luftfeuchtigkeit["state_topic"] = mqtt_state_topic;
  doc_config_luftfeuchtigkeit["unit_of_measurement"] = "%";
  doc_config_luftfeuchtigkeit["value_template"] = "{{ value_json.luftfeuchtigkeit}}";
  serializeJson(doc_config_luftfeuchtigkeit, mqtt_config_data_luftfeuchtigkeit);
  client.publish(mqtt_config_topic_luftfeuchtigkeit, mqtt_config_data_luftfeuchtigkeit, true);
  delay(1000);
}

// Funktion für Home Assistant MQTT Auto Discovery - eco2
void configMqtteCO2() {
  doc_config_eco2["name"] = "eCO2-Wert Wohnzimmer";  // Name des Zimmers bei Bedarf ändern
  doc_config_eco2["state_topic"] = mqtt_state_topic;
  doc_config_eco2["unit_of_measurement"] = "ppm";
  doc_config_eco2["value_template"] = "{{ value_json.eco2}}";
  serializeJson(doc_config_eco2, mqtt_config_data_eco2);
  client.publish(mqtt_config_topic_eco2, mqtt_config_data_eco2, true);
  delay(1000);
}

// Funktion für Home Assistant MQTT Auto Discovery - TVOC
void configMqttTVOC() {
  doc_config_tvoc["name"] = "TVOC-Wert Wohnzimmer";  // Name des Zimmers bei Bedarf ändern
  doc_config_tvoc["state_topic"] = mqtt_state_topic;
  doc_config_tvoc["unit_of_measurement"] = "ppb";
  doc_config_tvoc["value_template"] = "{{ value_json.tvoc}}";
  serializeJson(doc_config_tvoc, mqtt_config_data_tvoc);
  client.publish(mqtt_config_topic_tvoc, mqtt_config_data_tvoc, true);
  delay(1000);
}

void setup() {

  // Display aktivieren
  u8g2.begin();

  // HDC1080 aktivieren
  hdc1080.begin(0x40);

  // CCS811 aktivieren
  Serial.print("setup: CCS811 ");
  ccs811.set_i2cdelay(50); // Needed for ESP8266 because it doesn't handle I2C clock stretch correctly
  ccs811.begin();

  // CCS811 starten, mit einer Messung alle 10 Sekunden
  ccs811.start(CCS811_MODE_10SEC);

  // WLAN aktivieren
  setup_wifi();

  // MQTT aktivieren
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512);
  if (!client.connected()) reconnect();
  configMqttTemperatur();
  configMqttLuftfeuchtigkeit();
  configMqtteCO2();
  configMqttTVOC();

}

void loop() {

  // Daten vom Sensor lesen
  uint16_t eco2, tvoc, errstat, raw;
  uint16_t temperaturGelesen = hdc1080.readTemperature();
  uint16_t temperatur = temperaturGelesen - temperaturKorrekturfaktor;
  uint8_t luftfeuchtigkeit = hdc1080.readHumidity();
  ccs811.set_envdata(temperatur, luftfeuchtigkeit);
  ccs811.read(&eco2, &tvoc, &errstat, &raw);

  // Daten an den MQTT-Server senden
  publishData(temperatur, luftfeuchtigkeit, eco2, tvoc);

  // Daten an Display ausgeben
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.firstPage();
  do {
    u8g2.setCursor(0, 12);
    u8g2.print("Temperatur: ");
    u8g2.print(temperatur);
    u8g2.print(" C");
    u8g2.setCursor(0, 28);
    u8g2.print("Luftfeuchte: ");  // Eigentlich ist der Begriff "Luftfeuchtigkeit" gängiger, passt aber nicht aufs Display
    u8g2.print(luftfeuchtigkeit);
    u8g2.print(" %");
    u8g2.setCursor(0, 44);
    u8g2.print("eCO2: ");
    u8g2.print(eco2);
    u8g2.print(" ppm");
    u8g2.setCursor(0, 60);
    u8g2.print("TVOC: ");
    u8g2.print(tvoc);
    u8g2.print(" ppb");
  } while ( u8g2.nextPage() );
  delay(1000);

}
