
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h> 

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <ESP8266HTTPClient.h>
#include <ModbusRTU.h>
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

const char* ntpServer = "pool.ntp.org";
const int timeZoneOffset = 1; // Décalage horaire par rapport à l'UTC (par exemple, 1 pour l'heure d'été en Europe centrale)

// Timezone pour gérer les conversions de fuseau horaire
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 60}; // Heure d'été en Europe centrale
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 0};     // Heure normale en Europe centrale
Timezone tz(CEST, CET);

// WiFiUDP pour la synchronisation NTP
WiFiUDP udp;


ModbusRTU mb;
ESP8266WebServer    server(80);
struct settings {
  String ssid;
  String password;
} user_wifi = {};
#define EEPROM_SIZE 512
String configFormHTML = R"(
<!doctype html>
<html lang='en'>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Wifi Setup</title>
  <style>
    * {
      box-sizing: border-box;
    }
    body {
      margin: 0;
      font-family: 'Segoe UI', Roboto, 'Helvetica Neue', Arial, 'Noto Sans', 'Liberation Sans';
      font-size: 1rem;
      font-weight: 400;
      line-height: 1.5;
      color: #212529;
      background-color: #f5f5f5;
    }
    .form-control {
      display: block;
      width: 100%;
      height: calc(1.5em + .75rem + 2px);
      border: 1px solid #ced4da;
    }
    button {
      cursor: pointer;
      border: 1px solid transparent;
      color: #fff;
      background-color: #007bff;
      border-color: #007bff;
      padding: .5rem 1rem;
      font-size: 1.25rem;
      line-height: 1.5;
      border-radius: .3rem;
      width: 100%;
    }
    .form-signin {
      width: 100%;
      max-width: 400px;
      padding: 15px;
      margin: auto;
    }
    h1 {
      text-align: center;
    }
  </style>
</head>
<body>
  <main class='form-signin'>
    <form action='/' method='post' onsubmit='return validateForm()'>
      <h1>Wifi Setup</h1><br/>
      <div class='form-floating'>
        <label>SSID</label>
        <input type='text' class='form-control' name='ssid' required>
      </div>
      <div class='form-floating'><br/>
        <label>Password</label>
        <input type='password' class='form-control' name='password' required>
      </div>
      <br/><br/>
      <button type='submit'>Save</button>
      <p style='text-align: right'><a href='https://www.mrdiy.ca' style='color: #32C5FF'>mrdiy.ca</a></p>
    </form>
  </main>
  <script>
    function validateForm() {
      var ssid = document.forms['configForm']['ssid'].value;
      console.log(ssid);
      var password = document.forms['configForm']['password'].value;
      console.log(password);
      if (ssid === '' || password === '') {
        alert('Both SSID and Password must be filled out');
        return false;
      }
    }
     document.addEventListener('DOMContentLoaded', function () {
    document.forms['configForm'].addEventListener('submit', function (e) {
      if (!validateForm()) {
        e.preventDefault(); // Empêche la soumission du formulaire
      }
    });
  });
  </script>
</body>
</html>
)";
String failureHTML = R"(
<!doctype html>
<html lang='en'>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Wifi Setup</title>
  <style>
    /* Votre CSS pour la page d'échec ici */
  </style>
</head>
<body>
  <main class='form-signin'>
    <h1>Wifi Setup</h1>
    <br/>
    <p>Échec de la configuration. Veuillez vérifier vos paramètres et réessayer.</p>
    <a href='/'>Retour à la configuration</a>
  </main>
</body>
</html>
)";

void sauvegarderSettingsDansEEPROM() {
  DynamicJsonDocument jsonDoc(256); // Ajustez la taille du document JSON en fonction de vos besoins
  JsonObject wifiJson = jsonDoc.to<JsonObject>();
  wifiJson["ssid"] = user_wifi.ssid;
  wifiJson["password"] = user_wifi.password;

  String jsonStr;
  serializeJson(wifiJson, jsonStr);

  if (jsonStr.length() + 1 >= EEPROM_SIZE) {
    return;
  }

  // Écrire l'enregistrement JSON dans l'EEPROM
  for (size_t i = 0; i < jsonStr.length(); i++) {
    EEPROM.write(i, jsonStr[i]);
  }
  EEPROM.write(jsonStr.length(), '\0'); // Terminer avec un caractère nul

  EEPROM.commit();

}


void chargerSettingsDepuisEEPROM() {
  // Lire les caractères de l'EEPROM jusqu'à ce qu'un caractère nul soit atteint
  String jsonStr;
  for (int i = 0; i < EEPROM_SIZE; i++) {
    char c = EEPROM.read(i);
    if (c == '\0') {
      break;
    }
    jsonStr += c;
  }


  DynamicJsonDocument jsonDoc(256); 

  if (!jsonStr.isEmpty()) {
    DeserializationError error = deserializeJson(jsonDoc, jsonStr);

    if (!error) {
      JsonObject wifiJson = jsonDoc.as<JsonObject>();
      user_wifi.ssid = wifiJson["ssid"].as<String>();
      user_wifi.password = wifiJson["password"].as<String>();
 
    } else {
 
    }
  } else {

  }
}



void setup() {
      Serial.begin(9600, SERIAL_8N1);
   EEPROM.begin(1024);
 
  chargerSettingsDepuisEEPROM();
   
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(user_wifi.ssid, user_wifi.password);


  WiFi.softAP("Meier Getway", "meierenergy");
   configTime(timeZoneOffset * 3600, 0, ntpServer);
  while (!time(nullptr)) {
    delay(500);
    // Serial.println("Waiting for NTP time...");
  }
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
    // mb.Hreg(i, 0x0000);   
  }
 
  
  byte tries = 0;
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(1000);
  //   if (tries++ > 20) {
  //     WiFi.mode(WIFI_AP);
  //     WiFi.softAP("Setup Portal", "mrdiy.ca");
  //     break;
  //   }
  // }
  // Serial.println("qui a back again");
  server.on("/",  handlePortal);
  server.begin();
}


void loop() {
    mb.task();

  
  
    server.handleClient();

    if (WiFi.status() != WL_CONNECTED) {
    // Si la connexion Wi-Fi est perdue, essayez de vous reconnecter
    if (tryConnectWiFi(user_wifi.ssid, user_wifi.password)) {
      delay(10);
      
    floatToModbusRegisters(0, 0);
    floatToModbusRegisters(0, 2);
    floatToModbusRegisters(0, 4);
      
    } 
  }
  if (WiFi.status() == WL_CONNECTED) {

       if (getDataFromAPI()) {
    updateModbusRegisters();
  }
  }

  
}

// void handlePortal() {

//   if (server.method() == HTTP_POST) {

//     strncpy(user_wifi.ssid,     server.arg("ssid").c_str(),     sizeof(user_wifi.ssid) );
//     strncpy(user_wifi.password, server.arg("password").c_str(), sizeof(user_wifi.password) );
//    if  (server.arg("password").length()!=0 ||   server.arg("password").length()!=0) {

//     user_wifi.ssid[server.arg("ssid").length()] = user_wifi.password[server.arg("password").length()] = '\0';
//     EEPROM.put(0, user_wifi);
//     EEPROM.commit();

//     server.send(200,   "text/html",  "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Please restart the device.</p></main></body></html>" );
//    }else {

//   server.send(200,   "text/html",configFormHTML);
//   } 
//   }
  
//   else {

//   server.send(200,   "text/html",configFormHTML);
//   }
// }

void handlePortal() {
  if (server.method() == HTTP_POST) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
  
    if (ssid.length() > 0 && password.length() > 0) {
      if (tryConnectWiFi(ssid, password)) {
    
      user_wifi.ssid = ssid;
      user_wifi.password = password;    
      sauvegarderSettingsDansEEPROM()  ;      
        // Rediriger vers la page de succès
      delay(10);
      server.send(200,   "text/html",  "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Please restart the device.</p></main></body></html>" );
      //  ESP.restart();
      } else {
        // Rediriger vers la page d'échec de configuration
        server.send(200, "text/html", failureHTML);
      }
    } else {
      server.send(200, "text/html", failureHTML);
     
    }
  } else {
    server.send(200, "text/html", configFormHTML);
  }
}

bool tryConnectWiFi(const String &ssid, const String &password) {
  WiFi.begin(ssid.c_str(), password.c_str());
  WiFi.waitForConnectResult();  // Attend la connexion ou un échec
  if (WiFi.status() == WL_CONNECTED) {
    return true;  // Connexion réussie
  }


  return false;  // Les deux tentatives ont échoué
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