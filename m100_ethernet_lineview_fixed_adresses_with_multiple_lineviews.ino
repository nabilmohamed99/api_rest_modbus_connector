#include <WiFi.h>
#include <ModbusRTU.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <EEPROM.h>

const int BASE_MODBUS_ADDRESS = 100;
const int FLOAT_REGISTERS_PER_MACHINE = 10;
const char* apiEndpoint = "http://172.16.245.9/api/v1/DataApi";

bool configInProgress = false;
unsigned long prevMillis = 0;
 const unsigned  long intervall = 1000;
 #define RELOAD_DOG_PIN 3

AsyncWebServer server(80);
struct settings {
  String ssid;
  String password;
  String ipAddress;
  String gateway;
  String subnetMask;
  String dns;
  long baudRate=9600;
  int deviceID=1;
} user_wifi = {};
#define EEPROM_SIZE 512

  String machineIDsLine7[] = {
    "bd04634c-00b3-404d-b77e-84ff1a8249de",  // Combi
    "dff8a411-9a16-45a0-8dd6-d9f176aa5697",  // Etiqueteuse
    "2ef4059b-17f1-482b-9b78-3329ff9a17c1",  // Fardeleuse
    "6ce4631e-55e9-4daa-a9a7-2e6fb51c4698",  // Palettiseur
    "5f8c523d-6551-4d1f-9ca0-2d6fa8f02d9c",  // Poigneteuse
    "4d513069-dcad-444e-be89-fb7d3e1ab134",  // Convoyeur de bouteilles
    "f8693fbf-5f96-45b6-a1c1-69c387c3cf8"    // Convoyeur Etiqueteuse - Fardeleuse
  };

  String machineIDsLine8[] = {
    "fe655ff8-e8ad-4ada-adc3-6e8c8a6f5588",  // Etiqueteuse
    "e924f468-6df3-4e37-801e-43986da8cfa8",  // Soutireuse
    "bd04634c-00b3-404d-b77e-84ff1a8249de",  // Fardeleuse
    "dff8a411-9a16-45a0-8dd6-d9f176aa5697",  // Poigneteuse
    "2ef4059b-17f1-482b-9b78-3329ff9a17c1",  // Palettiseur
    "6ce4631e-55e9-4daa-a9a7-2e6fb51c4698",  // Convoyeur de paquets
    "5f8c523d-6551-4d1f-9ca0-2d6fa8f02d9c"   // Convoyeur de bouteilles
  };





float sensorData[7*7]={0.0};
const char* ntpServer = "pool.ntp.org";
const int timeZoneOffset = 1; 
// Timezone pour gérer les conversions de fuseau horaire
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 60}; // Heure d'été en Europe centrale
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 0};     // Heure normale en Europe centrale
Timezone tz(CEST, CET);

// WiFiUDP pour la synchronisation NTP
WiFiUDP udp;
#define RXD1 12
#define TXD1 12

#define RXD2 16
#define TXD2 17

ModbusRTU mb;



#include <Ethernet.h>
#include <ETH.h>
#define ETH_ADDR 1
#define ETH_POWER_PIN 5
#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18
#define ETH_TYPE ETH_PHY_IP101
static bool eth_connected = false;

IPAddress local_IP(172, 16, 245, 100);
IPAddress gateway(172, 16, 245, 1);      
IPAddress subnet(255, 255, 255, 0);     
IPAddress primaryDNS(8, 8, 8, 8);       
IPAddress secondaryDNS(8, 8, 4, 4); 

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      // Serial.println("ETH Started");
      ETH.setHostname("esp32-ethernet");  //set eth hostname here
   
      break;
      
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      //Serial.println("[case ARDUINO_EVENT_ETH_CONNECTED]");
      //Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("case:ARDUINO_EVENT_ETH_GOT_IP: ");
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.println(ETH.macAddress());

      Serial.print("IPv4:     localip:");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(",      FULL_DUPLEX");
      }
      Serial.print(",");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      //Serial.println("[WiFiEvent]->default");
      break;
  }
}



void sauvegarderSettingsDansEEPROM() {
  DynamicJsonDocument jsonDoc(512);
  JsonObject wifiJson = jsonDoc.to<JsonObject>();
  wifiJson["ssid"] = user_wifi.ssid;
  wifiJson["password"] = user_wifi.password;
  wifiJson["ipAddress"] = user_wifi.ipAddress;
  wifiJson["gateway"] = user_wifi.gateway;
  wifiJson["subnetMask"] = user_wifi.subnetMask;
  wifiJson["dns"] = user_wifi.dns;
  wifiJson["baudRate"]=user_wifi.baudRate;
  wifiJson["deviceID"]=user_wifi.deviceID;
  // Serial.println("je suis ici sauvegarde");
  // Serial.println(user_wifi.ipAddress);


  String jsonStr;
  serializeJson(wifiJson, jsonStr);
  // Serial.println(jsonStr);

  if (jsonStr.length() + 1 >= EEPROM_SIZE) {
    return;
  }

  for (size_t i = 0; i < jsonStr.length(); i++) {
    EEPROM.write(i, jsonStr[i]);
  }
  EEPROM.write(jsonStr.length(), '\0');

   EEPROM.commit();
}

void chargerSettingsDepuisEEPROM() {
  // Serial.println("ChargerSettingsDepuisEEPROM");
  delay(1000);
  // Serial.println("Erreur ici");
  String jsonStr;
  for (int i = 0; i < EEPROM_SIZE; i++) {
    char c = EEPROM.read(i);

    if (c == '\0') {
      break;
    }
    jsonStr += c;
  }
    // Serial.println("Erreur 2");


  DynamicJsonDocument jsonDoc(512);

  
    if (!jsonStr.isEmpty()) {
      DeserializationError error = deserializeJson(jsonDoc, jsonStr);

      if (!error) {
        Serial.println(jsonStr);
        JsonObject wifiJson = jsonDoc.as<JsonObject>();
        user_wifi.ssid = wifiJson["ssid"].as<String>();
        user_wifi.password = wifiJson["password"].as<String>();
        user_wifi.ipAddress = wifiJson["ipAddress"].as<String>();
        user_wifi.gateway = wifiJson["gateway"].as<String>();
        user_wifi.subnetMask = wifiJson["subnetMask"].as<String>();
        user_wifi.dns = wifiJson["dns"].as<String>();
        // Serial.println("les valeurs lue de baudrate et deviceID ");
        // Serial.println(wifiJson["baudRate"].as<String>());
        // Serial.println("############");
        if ( wifiJson["baudRate"].as<String>()==""||   wifiJson["deviceID"].as<String>()==""){
            Serial.println("je suis ici");
             user_wifi.baudRate= 9600;
             user_wifi.deviceID=  1;


        }else{
             user_wifi.baudRate= wifiJson["baudRate"].as<long>();
             user_wifi.deviceID=  wifiJson["deviceID"].as<int>();

        }
     
        // Serial.println(user_wifi.ipAddress);
        // Serial.println(user_wifi.subnetMask);
        // Serial.println(user_wifi.subnetMask);
      } else {
          user_wifi.ssid = "";
      user_wifi.password = "";
      user_wifi.ipAddress = "";
      user_wifi.gateway = "";
      user_wifi.subnetMask = "";
      user_wifi.dns = "";
      }
    } else {
              Serial.println("Erreur dans la deserialisation 2");

      user_wifi.ssid = "";
      user_wifi.password = "";
      user_wifi.ipAddress = "";
      user_wifi.gateway = "";
      user_wifi.subnetMask = "";
      user_wifi.dns = "";
      user_wifi.baudRate=9600;
      user_wifi.deviceID=1;
    }
 
}

void configurerWiFiAsync() {
  configInProgress = true;
}



bool tryConnectWiFi(const String& ssid, const String& password, const String& ipAddress, const String& gateway, const String& subnetMask, const String& dns, unsigned long maxConnectTime) {
  WiFi.disconnect();

  IPAddress ipAddr;
  IPAddress gatewayAddr;
  IPAddress subnetAddr;
  IPAddress dnsAddr;


  ipAddr.fromString(ipAddress);
  gatewayAddr.fromString(gateway);
  subnetAddr.fromString(subnetMask);
  dnsAddr.fromString(dns);
  if (!ipAddr || !gatewayAddr || !subnetAddr || !dnsAddr) {
    return false;
  }
  // Serial.println("pas de probleme ici");
  WiFi.config(ipAddr, gatewayAddr, subnetAddr, dnsAddr);

  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startTime < maxConnectTime) {
        // Serial.println("..");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    return true;
  } else {
    Serial.println("WiFi connection failed");
    return false;
  }
}

#define RESTART_COUNT_ADDR 1022
int restartCounteur;




void setup() {
   EEPROM.begin(1024);

   

     Serial.begin(9600, SERIAL_8N1);
      // chargerSettingsDepuisEEPROM();

    //  Serial.println("Erreur ici");
  
      Serial1.begin(user_wifi.baudRate,SERIAL_8N1,33,32);
    //  Serial2.begin(user_wifi.baudRate,SERIAL_8N1,RXD2,TXD2);

    
      //     user_wifi.ssid = "ELEXPERT_1";
      // user_wifi.password = "ELEXPERT2022";
      // user_wifi.ipAddress = "192.168.1.51";
      // user_wifi.gateway = "192.168.1.1";
      // user_wifi.subnetMask = "255.255.255.0";
      // user_wifi.dns = "8.8.8.8";
 
    // WiFi.onEvent(WiFiEvent);
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLOCK_GPIO0_IN);  //
   while (!ETH.linkUp()) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println();
  ETH.config(local_IP, gateway, subnet, primaryDNS);
  //reload_button_setup();
  // tryConnectWiFi(user_wifi.ssid, user_wifi.password, user_wifi.ipAddress, user_wifi.gateway, user_wifi.subnetMask, user_wifi.dns, 10000);

   configTime(timeZoneOffset * 3600, 0, ntpServer);
  unsigned long startTime = millis(); 
  unsigned long syncInterval = 10000; 

while (!time(nullptr)) {
  if (millis() - startTime > syncInterval) {
    break;
  }
  delay(500);
}
  
   


  

  

  mb.begin(&Serial1,12);
  //  mb.begin(&Serial2,4);
  //  mb.begin(&Serial); 


  mb.slave(user_wifi.deviceID);
   for (int i = 0; i < 1000; i++) {
    mb.addHreg(i);
     mb.Hreg(i, 0x0000);   
  }
 

    // xTaskCreatePinnedToCore(
    // xTask_watchdog,   
    // "xTask_watchdog",
    // 4096,            
    // NULL,           
    // 2,              
    // NULL,
    // 0); 
  xTaskCreatePinnedToCore(ModbusTask, "ModbusTask",8192, NULL,3,NULL,0);
  
  Serial.println("ALL GOOD");
  

}
void xTask_watchdog(void *xTask1) {

    while (1) {
      pinMode(3, OUTPUT);
      digitalWrite(3, 0);
      delay(50);
    
      digitalWrite(3, 1);
      delay(50);

      
      digitalWrite(3, 0);
      delay(50);
    }
}
void ModbusTask(void *pvParameters){
  for(;;){
    // Serial.println("ModbusTask");
    mb.task();    
    vTaskDelay(pdMS_TO_TICKS(350)); 

  }
}



void loop() {
    unsigned CurrentMillis = millis();


  //   if ((WiFi.status() != WL_CONNECTED)  && CurrentMillis-prevMillis>=10000) {
  //      Serial.println("Not Connected");
       
  //   // if (  tryConnectWiFi(user_wifi.ssid, user_wifi.password, user_wifi.ipAddress, user_wifi.gateway, user_wifi.subnetMask, user_wifi.dns, 5000  )){
 
  //   //   delay(50);
     
      
  //   // } 
  //   prevMillis=CurrentMillis;
  // }
  // if (WiFi.status() == WL_CONNECTED) {


      
      if (CurrentMillis-prevMillis>=intervall){
       if (getDataFromAPILastMinute()) {
        updateModbusRegisters();
       }
       else{
    Serial.println("Aucune reponse de serveur");
  }
      }
  // }  
     prevMillis=CurrentMillis;
  //     }
  //    if (time(nullptr) == 0) {
  //     configTime(timeZoneOffset * 3600, 0, ntpServer);
  //   }
  // }
  getMachinePerformanceForLine("172.16.245.7", machineIDsLine7, 7, "Ligne 7");




  delay(10000);
  
}

void getMachinePerformanceForLine(String lineIP, const String machineIDs[], int machineCount, String lineName) {
  Serial.println("Récupération des données pour " + lineName);
  
  WiFiClient client;
  HTTPClient http;

  for (int machineIndex = 0; machineIndex < machineCount; machineIndex++) {
    String apiRequest = "http://" + lineIP + "/LineView/MachineLeanMetrics?id=" + machineIDs[machineIndex] + "&type=tonow&minutes=60";
    Serial.println(machineIDs[machineIndex]);
    if (http.begin(client, apiRequest)) { 
      int httpCode = http.GET();

      if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println(payload);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);

        if (error == DeserializationError::Ok) {
          float availability = doc["Efficiency"].as<float>(); 
          float performance = doc["Performance"].as<float>();
          float quality = doc["UnitsProduced"].as<float>();
          float goodUnits = doc["UnitsLost"].as<float>();
          float unitsLost = doc["UnitsRejected"].as<float>();

          int startAddress = BASE_MODBUS_ADDRESS + machineIndex * FLOAT_REGISTERS_PER_MACHINE;

          floatToModbusRegisters(availability, startAddress);
          floatToModbusRegisters(performance, startAddress + 2);
          floatToModbusRegisters(quality, startAddress + 4);
          floatToModbusRegisters(goodUnits, startAddress + 6);
          floatToModbusRegisters(unitsLost, startAddress + 8);

        } else {
          Serial.println("Erreur de désérialisation");
        }
      } else {
        Serial.printf("Erreur HTTP: %d\n", httpCode);
      }
      http.end();
    } else {
      Serial.println("Erreur lors de l'initialisation de HTTP");
    }
  }
}

void updateModbusRegistersForMachine(int startIndex) {
  // 5 metrics to be stored in Modbus for each machine
  for (int metricIndex = 0; metricIndex < 5; metricIndex++) {
    floatToModbusRegisters(sensorData[startIndex + metricIndex], startIndex + metricIndex * 2);
  }
}


bool getDataFromAPI() {
  WiFiClient client;
  HTTPClient http;


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
          Serial.println(payload);
  

  
          DynamicJsonDocument doc(1024); 
          DeserializationError error = deserializeJson(doc, payload);


          if (error == DeserializationError::Ok) {

       
             sensorData[0] =  doc["Availability"].as<float>();
             sensorData[1] =  doc["Performance"].as<float>();
             sensorData[2] =doc["Quality"].as<float>();
            return true;
         
      

       
     
          } else {
            return false;
      
          }
        } else {
          return false;

        }
      }

      http.end();
    } else {
      return false;
      // Serial.println("Unable to connect to API");
    }


  
}



bool getDataFromAPILastMinute() {
  WiFiClient client;
  HTTPClient http;

  for (int i = 7; i <= 12; i++) {
    String apiRequest = "http://172.16.245." + String(i) + "/api/v1/DataApi/LastMinutesLineData?lastMinutes=1";

    if (http.begin(client, apiRequest)) { 
      int httpCode = http.GET();

      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          Serial.println(payload);

          DynamicJsonDocument doc(1024); 
          DeserializationError error = deserializeJson(doc, payload);

          if (error == DeserializationError::Ok) {
            int deviceIndex = (i - 7) * 7; 
            sensorData[deviceIndex + 0] = doc["Availability"].as<float>();
            sensorData[deviceIndex + 1] = doc["Performance"].as<float>();
            sensorData[deviceIndex + 2] = doc["Quality"].as<float>();
            sensorData[deviceIndex + 3] = doc["GoodUnitsProduced"].as<float>(); 
            sensorData[deviceIndex + 4] = doc["EOLUnitsProduced"].as<float>();
            sensorData[deviceIndex + 5] = doc["UnitsLost"].as<float>(); 
            sensorData[deviceIndex + 6] = doc["ExternalTimeSeconds"].as<float>(); 
          } else {
            Serial.println("Erreur de désérialisation");
          }
        } else {
          Serial.printf("Erreur HTTP: %d\n", httpCode);
        }
      } else {
        Serial.println("Erreur de requête HTTP");
      }
      http.end();
    } else {
      Serial.println("Erreur lors de l'initialisation de HTTP");
    }
  }
  return true; 
}

bool updateAPIRequestDates() {
  return true;
}

void preTransmission() {
  // Code pour la transmission (le cas échéant)
}

void postTransmission() {
  // Code après la transmission (le cas échéant)
}
int floatToInt16(float floatValue) {

  return static_cast<int>(floatValue * 10);  
}
void updateModbusRegisters() {
  const int registersPerDevice = 14;
  const int numberOfDevices = 6; 

  for (int deviceIndex = 0; deviceIndex < numberOfDevices; deviceIndex++) {
    int startIndex = deviceIndex * registersPerDevice; 

    floatToModbusRegisters(sensorData[deviceIndex * 7 + 0], startIndex);     
    floatToModbusRegisters(sensorData[deviceIndex * 7 + 1], startIndex + 2); 
    floatToModbusRegisters(sensorData[deviceIndex * 7 + 2], startIndex + 4); 
    floatToModbusRegisters(sensorData[deviceIndex * 7 + 3], startIndex + 6); 
    floatToModbusRegisters(sensorData[deviceIndex * 7 + 4], startIndex + 8); 
    floatToModbusRegisters(sensorData[deviceIndex * 7 + 5], startIndex + 10); 
    floatToModbusRegisters(sensorData[deviceIndex * 7 + 6], startIndex + 12);
  }
}

void floatToModbusRegisters(float floatValue, int startIndex) {
  uint16_t word1, word2;
  int32_t tempInt = *((int32_t*)&floatValue);
  word1 = (uint16_t)(tempInt >> 16);
  word2 = (uint16_t)(tempInt);
  
  mb.Hreg(startIndex, word1);    
  mb.Hreg(startIndex + 1, word2); 
}


void changeBaud(int indexBaud){
  switch (indexBaud) {

    case 0:
     user_wifi.baudRate=2400;
     break;
    case 1:
      user_wifi.baudRate=4800;
     
      break;
    case 2:
         user_wifi.baudRate=9600;
      
      break;
    case 3:
         user_wifi.baudRate=19200;
    break;


     case 4:
         user_wifi.baudRate=38400;
        break;
       case 5:
         user_wifi.baudRate=57600;
        break;
        case 6:
         user_wifi.baudRate=115200;
    break;
       

  }
}