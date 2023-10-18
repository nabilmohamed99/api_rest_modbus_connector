#include <WiFi.h>
#include <ModbusRTU.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <EEPROM.h>
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
#define RXD2 16
#define TXD2 17

ModbusRTU mb;
WebServer    server(80);
struct settings {
  String ssid;
  String password;
   int modbusSlave;
  int modbusBaudrate;
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
      margin: 0;
      padding: 0;
    }
    body {
      font-family: 'Helvetica', sans-serif;
      font-size: 16px;
      background-color: #f5f5f5;
      display: flex;
      flex-direction: column;
      align-items: center;
      height: 100vh;
    }
    .navbar {
      background-color: #2a5ba6;
      color: #fff;
      padding: 10px;
      text-align: center;
      width: 100%;
      position: fixed;
      top: 0;
      z-index: 1;
    }
    .navbar h1 {
      margin: 0;
    }
    .content-container {
      margin-top: 60px; /* Assurez-vous que le contenu n'est pas caché sous la barre de navigation */
      display: flex;
      justify-content: center;
      align-items: center;
      flex-grow: 1;
    }
    .form-container {
      background-color: #fff;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
      border-radius: 5px;
      padding: 20px;
      width: 400px;
    }
    .form-container h1 {
      text-align: center;
      margin-bottom: 20px;
      color: #2a5ba6;
    }
    .form-control {
      width: 100%;
      padding: 10px;
      margin: 10px 0;
      border: 1px solid #ced4da;
      border-radius: 3px;
    }
    .form-control:focus {
      border-color: #2a5ba6;
    }
    button {
      cursor: pointer;
      background-color: #2a5ba6;
      color: #fff;
      border: none;
      border-radius: 3px;
      padding: 10px;
      width: 100%;
    }
    button:hover {
      background-color: #007bff;
    }
    .form-link {
      text-align: right;
      margin-top: 10px;
    }
    .form-link a {
      color: #32C5FF;
      text-decoration: none;
    }
  </style>
</head>
<body>
  <div class='navbar'>
    <h1>MEIER ENERGY</h1>
  </div>
  <div class='content-container'>
    <div class='form-container'>
      <h1>Configuration du wifi</h1>
      <form action='/' method='post' onsubmit='return validateForm()'>
        <div class='form-floating'>
          <input type='text' class='form-control' name='ssid' placeholder='SSID' required>
        </div>
        <div class='form-floating'>
          <input type='password' class='form-control' name='password' placeholder='Mot de passe' required>
        </div>
      
      <div class='form-floating'>
    <input type='number' class='form-control' name='modbusSlave' placeholder='Modbus Slave' >
  </div>
  <div class='form-floating'>
    <input type='number' class='form-control' name='modbusBaudrate' placeholder='Modbus Baudrate' >
  </div>
          <p class='form-link'><a href='https://www.meierenergy.com'>meierenergy.com</a></p>

  <button type='submit'>Save</button>
      </form>
    </div>
  </div>
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
          e.preventDefault();
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
  <title>Échec de la configuration</title>
  <style>
    body {
      font-family: 'Arial', sans-serif;
      background-color: #f7f7f7;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      margin: 0;
    }
    
    .form-signin {
      text-align: center;
      background-color: #fff;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.2);
      border-radius: 5px;
      padding: 20px;
      width: 400px;
    }

    h1 {
      color: #2a5ba6;
      font-size: 28px;
    }

    p {
      font-size: 18px;
    }

    a {
      text-decoration: none;
      color: #2a5ba6;
      font-weight: bold;
    }

    a:hover {
      color: #007bff;
    }
  </style>
</head>
<body>
  <main class='form-signin'>
    <h1>Échec de la configuration</h1>
    <br/>
    <p>Veuillez vérifier vos paramètres et réessayer.</p>
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
    wifiJson["modbusSlave"] = user_wifi.modbusSlave;
  wifiJson["modbusBaudrate"] = user_wifi.modbusBaudrate;

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
        user_wifi.modbusSlave = wifiJson["modbusSlave"].as<int>();
      user_wifi.modbusBaudrate = wifiJson["modbusBaudrate"].as<int>();
 
    } else {
 
    }
  } else {

  }
}



void setup() {
      Serial.begin(9600, SERIAL_8N1);
   EEPROM.begin(1024);
      Serial2.begin(9600,  SERIAL_8N1,RXD2,TXD2);

 
  chargerSettingsDepuisEEPROM();
   
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(user_wifi.ssid, user_wifi.password);


  WiFi.softAP("Meier Getway", "meierenergy");
   configTime(timeZoneOffset * 3600, 0, ntpServer);
  unsigned long startTime = millis(); 
  unsigned long syncInterval = 10000; 

while (!time(nullptr)) {
  if (millis() - startTime > syncInterval) {
    break;
  }
  delay(500);
}
  
   #if defined(ESP32) || defined(ESP8266)
  mb.begin(&Serial2,4); 
   mb.setBaudrate(user_wifi.modbusBaudrate);


#else

  mb.begin(&Serial2,4);
  //mb.begin(&Serial, RXTX_PIN);  //or use RX/TX direction control pin (if required)
#endif

  mb.slave(user_wifi.modbusSlave);
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
    if (tryConnectWiFi(user_wifi.ssid, user_wifi.password,2000)) {
      Serial.println("oui je suis connectée");
      delay(10);
   
    } 
    
  }
  if (WiFi.status() == WL_CONNECTED) {

      

       if (getDataFromAPI()) {
         Serial.println("Bonjour j'ai recuperer les données depuis l 'api");
        updateModbusRegisters();
        delay(2000);
  }
     if (time(nullptr) == 0) {
      configTime(timeZoneOffset * 3600, 0, ntpServer);
    }
  }

  
}



void handlePortal() {
  if (server.method() == HTTP_POST) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    int modbusId = server.arg("modbusSlave").toInt();
    int newBaudrate = server.arg("modbusBaudrate").toInt();

  
    if (ssid.length() > 0 && password.length() > 0) {
            Serial.println("oui je suis dans hadnlePortal");

      if (tryConnectWiFi(ssid, password,3500) &&  (modbusId >= 1 && modbusId <= 255 && newBaudrate >= 1200 && newBaudrate <= 115200)) {
 
      user_wifi.ssid = ssid;
      user_wifi.password = password; 
      user_wifi.modbusSlave = modbusId;
      user_wifi.modbusBaudrate = newBaudrate;   
    
    
      server.send(200,   "text/html",  "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Please restart the device.</p></main></body></html>" );
      sauvegarderSettingsDansEEPROM() ;      
       mb.begin(&Serial2,4); 
      mb.slave(user_wifi.modbusSlave);
      mb.setBaudrate(user_wifi.modbusBaudrate);
      delay(200);
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

bool tryConnectWiFi(const String &ssid, const String &password, unsigned long maxConnectTime) {

  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startTime = millis();


  while (WiFi.status() != WL_CONNECTED && millis() - startTime < maxConnectTime) {
    delay(300); 
  }

  if (WiFi.status() == WL_CONNECTED) {
   
    return true;
  } else {
    
    return false;
  }
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
  

  
          DynamicJsonDocument doc(8192); // Adjust the buffer size as needed
          DeserializationError error = deserializeJson(doc, payload);



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
   Serial.println(sensorData.availability);
    Serial.println(sensorData.availability);
     Serial.println(sensorData.availability);


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