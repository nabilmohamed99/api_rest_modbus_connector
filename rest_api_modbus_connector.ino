
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <ESP8266HTTPClient.h>
#include <ModbusRTU.h>



// WiFi credentials
const char* ssid = "ELEXPERT_1";
const char* password = "ELEXPERT2022";

// API REST endpoint
const char* apiEndpoint = "http://81.28.5.151/api/v1/DataApi";

struct SensorData {
  float availability;
  float performance;
  float quality;
  float goodUnitsProduced;
  float eolUnitsProduced;
  float eolCases;
  float eolRatedCases;
  float unitsLost;
};
SensorData sensorData;
// Variables pour stocker les données
float availability = 0.0;
float performance = 0.0;
float quality = 0.0;

// NTP settings
const char* ntpServer = "pool.ntp.org";
const int timeZoneOffset = 1; // Décalage horaire par rapport à l'UTC (par exemple, 1 pour l'heure d'été en Europe centrale)

// Timezone pour gérer les conversions de fuseau horaire
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 60}; // Heure d'été en Europe centrale
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 0};     // Heure normale en Europe centrale
Timezone tz(CEST, CET);

// WiFiUDP pour la synchronisation NTP
WiFiUDP udp;


ModbusRTU mb;
void setup() {

     Serial.begin(9600, SERIAL_8N1);
  // Connexion Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    // Serial.println("Connecting to WiFi...");
  }
  // Serial.println("Connected to WiFi");

  // Initialisez la synchronisation NTP
  configTime(timeZoneOffset * 3600, 0, ntpServer);
  while (!time(nullptr)) {
    delay(500);
    // Serial.println("Waiting for NTP time...");
  }
  // Serial.println("NTP time synchronized");

  // Lire les données de l'API et mettre à jour les variables
  // if (getDataFromAPI()) {
  //   // Serial.println("Data from API:");
  //   // Serial.print("Availability: ");
  //   // Serial.println(availability);
  //   // Serial.print("Performance: ");
  //   // Serial.println(performance);
  //   // Serial.print("Quality: ");
  //   // Serial.println(quality);
          
  // } else {
  //   Serial.println("Failed to get data from API");
  // }
  //  if (getDataFromAPI()) {
  //   updateModbusRegisters();
  // } else {
  //   Serial.println("Failed to get data from API");
  // }
  

// Baud rate

   #if defined(ESP32) || defined(ESP8266)
  mb.begin(&Serial);
#else
  mb.begin(&Serial);
  //mb.begin(&Serial, RXTX_PIN);  //or use RX/TX direction control pin (if required)
  mb.setBaudrate(9600);
#endif
 
   mb.slave(1);
   for (int i = 0; i < 10; i++) {
    mb.addHreg(i);
    mb.Hreg(i, 0x0000);   
  }
 
   



  // mb.begin(3,9600,SERIAL_8N1);


  
}

void loop() {
  mb.task();


    // Serial.println("API request dates updated successfully.");
 
       if (getDataFromAPI()) {
    updateModbusRegisters();
  }

      // Serial.println("J'ai reçu des données.");
     
      
 
  delay(100);
 
}

bool getDataFromAPI() {
  WiFiClient client;
  HTTPClient http;

  if (client.connect("81.28.5.151", 80)) {
    //  la date et l'heure actuelles
    time_t now = time(nullptr);
    time_t utcNow = tz.toUTC(now); // Convertir en temps UTC
    struct tm timeinfo;
    gmtime_r(&utcNow, &timeinfo);

    //  la date de début (hier)
    time_t yesterday = now - 24 * 3600; // 24 heures en secondes
    struct tm yesterdayInfo;
    gmtime_r(&yesterday, &yesterdayInfo);

    //  la chaîne de date de début au format requis pour hier
    char startTime[40];
    snprintf(startTime, sizeof(startTime), "%%20%04d-%02d-%02d%%20%02d%%3A%02d%%3A%02d", 
             yesterdayInfo.tm_year + 1900, yesterdayInfo.tm_mon + 1, yesterdayInfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  
    char endTimeStr[40];
    snprintf(endTimeStr, sizeof(endTimeStr), "%%20%04d-%02d-%02d%%20%02d%%3A%02d%%3A%02d%%20", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    String apiRequest = String(apiEndpoint) + "?from=" + startTime + "&to=" + endTimeStr;
    if (http.begin(client, apiRequest)) { 
      int httpCode = http.GET();

      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          // Serial.println("API response received:");

          //  ArduinoJSON to parse the JSON response
          DynamicJsonDocument doc(8192); // Adjust the buffer size as needed
          DeserializationError error = deserializeJson(doc, payload);

          //  Serial.println(payload);

          if (!error) {
       
            // Extract JSON 
            String lineId = doc["LineId"];
            String lineName = doc["LineName"];
            float kpi1 = doc["CausalTimeLostSeconds"];
            float kpi2 = doc["KPI2Metric"];
            float kpi3 = doc["KPI3Metric"];
            float availability = doc["Availability"];
            float performance = doc["Performance"];
            float quality = doc["Quality"];
      

            sensorData.availability = doc["Availability"];
            sensorData.performance = doc["Performance"];
            sensorData.quality = doc["Quality"];
            sensorData.goodUnitsProduced = doc["GoodUnitsProduced"];
            sensorData.eolUnitsProduced = doc["EOLUnitsProduced"];
            sensorData.eolCases = doc["EOLCases"];
            sensorData.eolRatedCases = doc["EOLRatedCases"];
            sensorData.unitsLost = doc["UnitsLost"];
            // Serial.println("Line ID: " + lineId);
            // Serial.println("Line Name: " + lineName);
            // Serial.println("CausalTimeLostSeconds: " + String(kpi1, 1) + "%");
            // Serial.println("KPI2: " + String(kpi2, 1) + "%");
            // Serial.println("KPI3: " + String(kpi3, 1) + "%");
            // Serial.println("Availability: " + String(availability, 1) + "%");
            // Serial.println("Performance: " + String(performance, 1) + "%");
            // Serial.println("Quality: " + String(quality, 1) + "%");
          } else {
            return false;
            // Serial.println("JSON parsing failed");
          }
        } else {
          return false;
          // Serial.println("API request failed");
        }
      }

      http.end();
    } else {
      return false;
      // Serial.println("Unable to connect to API");
    }


  }
  return true;
}

bool updateAPIRequestDates() {
  // Pas besoin de mettre à jour les dates, car nous utilisons l'heure actuelle moins une heure et l'heure actuelle.
  return true;
}

void preTransmission() {
  // Code pour la transmission (le cas échéant)
}

void postTransmission() {
  // Code après la transmission (le cas échéant)
}
int floatToInt16(float floatValue) {
  //  un float en un entier 16 bits
  return static_cast<int>(floatValue * 10);  
}
void updateModbusRegisters() {


  floatToModbusRegisters(sensorData.availability, 0);
  floatToModbusRegisters(sensorData.performance, 2);
  floatToModbusRegisters(sensorData.quality, 4);
}

void floatToModbusRegisters(float floatValue, int startIndex) {
  uint16_t word1, word2;
  int32_t tempInt = *((int32_t*)&floatValue);
  word1 = (uint16_t)(tempInt >> 16);
  word2 = (uint16_t)(tempInt);
  
  mb.Hreg(startIndex, word1);    
  mb.Hreg(startIndex + 1, word2); 
}
