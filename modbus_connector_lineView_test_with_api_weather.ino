#include <WiFi.h>
#include <ModbusRTU.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <EEPROM.h>
// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "sdkconfig.h"
// const char* apiEndpoint = "http://81.28.5.151/api/v1/DataApi";
// const char* apiEndpoint = "http://81.28.5.151/api/v1/DataApi";
String openWeatherMapApiKey = "a026dddda9a2a779fc853892373c67aa";
String city = "Porto";
String countryCode = "PT";
bool configInProgress = false;
unsigned long prevMillis = 0;
 const unsigned  long intervall = 1000;

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



// struct SensorData {
//   float availability;
//   float performance;
//   float quality;
//   float goodUnitsProduced;
//   float eolUnitsProduced;
//   float eolCases;
//   float eolRatedCases;
//   float unitsLost;
// };
// SensorData sensorData;
float sensorData[10]={0.0};
const char* ntpServer = "pool.ntp.org";
const int timeZoneOffset = 1; // Décalage horaire par rapport à l'UTC (par exemple, 1 pour l'heure d'été en Europe centrale)

// Timezone pour gérer les conversions de fuseau horaire
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 60}; // Heure d'été en Europe centrale
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 0};     // Heure normale en Europe centrale
Timezone tz(CEST, CET);

// WiFiUDP pour la synchronisation NTP
WiFiUDP udp;
#define RXD1 12
#define TXD1 12

ModbusRTU mb;


const char* baudRateIDFormHTML = R"(
<!doctype html>
<html lang='en'>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Baud Rate and Device ID Configuration</title>
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
      margin-top: 60px;
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
  </style>
</head>
<body>
  <div class='navbar'>
    <h1>MEIER ENERGY</h1>
  </div>
  <div class='content-container'>
    <div class='form-container'>
      <h1>Baud Rate and Device ID Configuration</h1>
      <form action='/config_baudrate_id' method='post'>
        <div class='form-floating'>
<select class='form-control' name='baudRate' required>
  <option value=''>Sélectionnez le baud rate</option>
  <option value='0'>0 - 2400</option>
  <option value='1'>1 - 4800</option>
  <option value='2'>2 - 9600</option>
  <option value='3'>3- 19200</option>
  <option value='4'>4 - 38400</option>
  <option value='5'>5 - 57600</option>
  <option value='5'>6 - 115200</option>

</select>       </div>
        <div class='form-floating'>
          <input type='number' class='form-control' name='deviceID' placeholder='Device ID' required>
        </div>
        <button type='submit'>Save</button>
      </form>
    </div>
  </div>
</body>
</html>
)";

const char* configFormHTML = R"(
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
  <input type='text' class='form-control' name='ipAddress' placeholder='IP Address' required>
</div>
<div class='form-floating'>
  <input type='text' class='form-control' name='gateway' placeholder='Gateway' required>
</div>
<div class='form-floating'>
  <input type='text' class='form-control' name='subnetMask' placeholder='Subnet Mask' required>
</div>
<div class='form-floating'>
  <input type='text' class='form-control' name='dns' placeholder='DNS' required>
</div>
      <div class='form-floating'>
   
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
const char* failureHTML = R"(
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
  Serial.println("je suis ici sauvegarde");
  Serial.println(user_wifi.ipAddress);


  String jsonStr;
  serializeJson(wifiJson, jsonStr);
  Serial.println(jsonStr);

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
  String jsonStr;
  for (int i = 0; i < EEPROM_SIZE; i++) {
    char c = EEPROM.read(i);
    if (c == '\0') {
      break;
    }
    jsonStr += c;
  }

  DynamicJsonDocument jsonDoc(512);

  try {
    if (!jsonStr.isEmpty()) {
      DeserializationError error = deserializeJson(jsonDoc, jsonStr);

      if (!error) {
        Serial.println("je charge correctement");
        JsonObject wifiJson = jsonDoc.as<JsonObject>();
        user_wifi.ssid = wifiJson["ssid"].as<String>();
        user_wifi.password = wifiJson["password"].as<String>();
        user_wifi.ipAddress = wifiJson["ipAddress"].as<String>();
        user_wifi.gateway = wifiJson["gateway"].as<String>();
        user_wifi.subnetMask = wifiJson["subnetMask"].as<String>();
        user_wifi.dns = wifiJson["dns"].as<String>();
        user_wifi.baudRate= wifiJson["baudRate"].as<long>();
        user_wifi.deviceID=  wifiJson["deviceID"].as<int>();
        Serial.println(user_wifi.ipAddress);
        Serial.println(user_wifi.subnetMask);
        Serial.println(user_wifi.subnetMask);
      } else {
        throw std::runtime_error("Error deserializing JSON");
      }
    } else {

      user_wifi.ssid = "ELEXPERT_TPLINK";
      user_wifi.password = "ELEXPERT@2020";
      user_wifi.ipAddress = "192.168.1.245";
      user_wifi.gateway = "192.168.1.1";
      user_wifi.subnetMask = "255.255.255.0";
      user_wifi.dns = "8.8.8.8";
    }
  } catch (const std::exception& e) {
    Serial.println("Exception caught: ");
    Serial.println(e.what());
    // En cas d'erreur, utilisez les valeurs par défaut
    user_wifi.ssid = "default_ssid";
    user_wifi.password = "default_password";
    user_wifi.ipAddress = "";
    user_wifi.gateway = "";
    user_wifi.subnetMask = "";
    user_wifi.dns = "";
  }
}

void configurerWiFiAsync() {
  configInProgress = true;
}

void handleConfig(AsyncWebServerRequest* request) {
  if (request->method() == HTTP_POST) {
    Serial.println("dfff");
    String ssid = request->arg("ssid");
    String password = request->arg("password");
    String ipAddress = request->arg("ipAddress");
    String gateway = request->arg("gateway");
    String subnetMask = request->arg("subnetMask");
    String dns = request->arg("dns");

    if (ssid.length() > 0 && password.length() > 0) {
      if (tryConnectWiFi(ssid, password, ipAddress, gateway, subnetMask, dns, 3500)) {
        user_wifi.ssid = ssid;
        user_wifi.password = password;
        user_wifi.ipAddress = ipAddress;
        user_wifi.gateway = gateway;
        user_wifi.subnetMask = subnetMask;
        user_wifi.dns = dns;
        Serial.println(ipAddress);
        // Serial.println("je suis ici");
        delay(100);
        request->send(200, "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Please restart the device.</p></main></body></html>");
        delay(100);
        sauvegarderSettingsDansEEPROM();
        Serial.println("test config");


        ESP.restart();
        delay(5000);

      } else {
        request->send(200, "text/html", failureHTML);
      }
    } else {
      request->send(200, "text/html", failureHTML);
    }
  } else {
    request->send(200, "text/html", configFormHTML);
  }
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
    delay(300);
  }
  return WiFi.status() == WL_CONNECTED;
  // if (WiFi.status() == WL_CONNECTED) {
  //   Serial.println("WiFi connected");
  //   return true;
  // } else {
  //   Serial.println("WiFi connection failed");
  //   return false;
  // }
}

#define RESTART_COUNT_ADDR 1022
int restartCounteur;




void setup() {
   EEPROM.begin(1024);
      chargerSettingsDepuisEEPROM();

     Serial.begin(user_wifi.baudRate, SERIAL_8N1);
    //  Serial.println("Erreur ici");
     Serial1.begin(user_wifi.baudRate,SERIAL_8N1,33,32);

      // Serial2.begin(9600,  SERIAL_8N1,RXD2,TXD2);
    
     user_wifi.ssid = "ELEXPERT_1";
      user_wifi.password = "ELEXPERT2022";
      user_wifi.ipAddress = "192.168.1.245";
      user_wifi.gateway = "192.168.1.1";
      user_wifi.subnetMask = "255.255.255.0";
      user_wifi.dns = "8.8.8.8";

 
   
  tryConnectWiFi(user_wifi.ssid, user_wifi.password, user_wifi.ipAddress, user_wifi.gateway, user_wifi.subnetMask, user_wifi.dns, 10000);

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
  
    configurerWiFiAsync();
    server.on("/", handleConfig);
    server.on("/config_baudrate_id", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(200, "text/html", baudRateIDFormHTML);
});
server.on("/config_baudrate_id", HTTP_POST, handleBaudRateIDConfig);




    server.begin();
  
    delay(1000);

  
//    #if defined(ESP32) || defined(ESP8266)
//   // mb.begin(&Serial2,4); 
//   mb.begin(&Serial1,12); 


//   mb.setBaudrate(user_wifi.baudRate);


// #else

  mb.begin(&Serial1,12);

  //mb.begin(&Serial, RXTX_PIN);  //or use RX/TX direction control pin (if required)

  mb.slave(user_wifi.deviceID);
   for (int i = 0; i < 10; i++) {
    mb.addHreg(i);
     mb.Hreg(i, 0x0000);   
  }
 
  

    Serial.println(user_wifi.baudRate);
        Serial.println(user_wifi.deviceID);

     xTaskCreatePinnedToCore(ModbusTask, "ModbusTask",8192, NULL,4 ,NULL,0);
  Serial.println("ALL GOOD");

}

void ModbusTask(void *pvParameters){
  for(;;){
    // Serial.println("ModbusTask");
    mb.task();    
    vTaskDelay(pdMS_TO_TICKS(350)); 

  }
}
void handleBaudRateIDConfig(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    String baudRateStr = request->arg("baudRate");
    String deviceIDStr = request->arg("deviceID");
    Serial.println(baudRateStr);
    Serial.println(deviceIDStr);

    if (baudRateStr.length() > 0 && deviceIDStr.length() > 0) {
       int indexBaud= baudRateStr.toInt();
       changeBaud(indexBaud);
      user_wifi.deviceID = deviceIDStr.toInt();


      request->send(200, "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Configuration sauvegardée </title><style>body{font-family:'Helvetica',sans-serif;font-size:16px;background-color:#f5f5f5;display:flex;flex-direction:column;align-items:center;height:100vh;}.content-container{display:flex;justify-content:center;align-items:center;flex-grow:1;}h1{text-align:center;color:#2a5ba6;}</style></head><body><div class='content-container'><h1>Configuration sauvegardée ! Veuillez redémarrer l'appareil.</h1></div></body></html>");
      delay(2000);
      sauvegarderSettingsDansEEPROM();

      ESP.restart();
    } else {
      request->send(200, "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Erreur de configuration</title><style>body{font-family:'Helvetica',sans-serif;font-size:16px;background-color:#f5f5f5;display:flex;flex-direction:column;align-items:center;height:100vh;}.content-container{display:flex;justify-content:center;align-items:center;flex-grow:1;}h1{text-align:center;color:red;}</style></head><body><div class='content-container'><h1>Erreur :(</h1></div></body></html>");
    }
  } else {
    request->send(200, "text/html", baudRateIDFormHTML);
  }
}




void loop() {
    unsigned CurrentMillis = millis();


    if (WiFi.status() != WL_CONNECTED) {
    if (  tryConnectWiFi(user_wifi.ssid, user_wifi.password, user_wifi.ipAddress, user_wifi.gateway, user_wifi.subnetMask, user_wifi.dns, 5000)){
 
      delay(10);
   
    } 
    
  }
  if (WiFi.status() == WL_CONNECTED) {
    // Serial.println("Connectee");
          // sensorData[0] = random(0, 400) / 10.0;   
          // sensorData[1] = random(950, 1050);       
          // sensorData[2] = random(0, 101);         
          // sensorData[3] = random(0, 31) / 10.0;     
          // sensorData[4] = random(0, 101) / 10.0;   
          // sensorData[5] = random(0, 31) / 10.0;     
          // sensorData[6] = random(0, 231) / 10.0;     
          // sensorData[7] = random(0, 231) / 10.0;    
          // sensorData[8] = random(0, 2031) / 10.0;     
          // updateModbusRegisters();

 


  


      
      if (CurrentMillis-prevMillis>=intervall){
       if (getDataFromAPI()) {
        updateModbusRegisters();
  }
      prevMillis=CurrentMillis;
      }
     if (time(nullptr) == 0) {
      configTime(timeZoneOffset * 3600, 0, ntpServer);
    }
  }



  
}








bool getDataFromAPI() {
  WiFiClient client;
  HTTPClient http;



    String apiRequest = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&APPID=" + openWeatherMapApiKey;
    if (http.begin(client, apiRequest)) { 
      int httpCode = http.GET();

      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
  

  
          DynamicJsonDocument doc(1024); 
          DeserializationError error = deserializeJson(doc, payload);


          if (error == DeserializationError::Ok) {

       
             sensorData[0] = doc["main"]["temp"].as<float>();
             sensorData[1] = doc["main"]["pressure"].as<float>();
             sensorData[2] = doc["main"]["humidity"].as<float>();
             sensorData[3] = doc["wind"]["speed"].as<float>();
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


  
  floatToModbusRegisters(sensorData[0], 0);
  floatToModbusRegisters(sensorData[1], 2);
  floatToModbusRegisters(sensorData[2], 4);
  floatToModbusRegisters(sensorData[3], 6);
  floatToModbusRegisters(sensorData[4], 6);

  floatToModbusRegisters(sensorData[5], 6);

  floatToModbusRegisters(sensorData[6], 6);
  floatToModbusRegisters(sensorData[7], 6);
  floatToModbusRegisters(sensorData[8], 6);




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