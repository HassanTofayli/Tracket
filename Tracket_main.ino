#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <TinyGPS++.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <ESP.h>

#define FORMAT_LITTLEFS_IF_FAILED true

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;      // GMT offset: UTC+2 (Lebanon Standard Time)
const int daylightOffset_sec = 3600;  // Daylight offset: UTC+3 (Lebanon Daylight Time)

// Enter Company Serial Number Here
const int serialNumber = 123456; 


const char* config_filename = "/config.json";
const char* data_filename = "/data.txt";
String name = "DefaultVehcileName";
String type;
int batterySize;
int memorySize;
int version;
String sensors;
bool wifi_connected = false;
unsigned long lastTime = 0;
bool configFetchingSucc = false;
// const char* serverUrl = "http://192.168.33.99:3000/esp"; //hassan Tofayli
const char* serverUrl = "http://192.168.1.109:3000/esp";
// const char* serverUrl = "https://esp-dt.glitch.me/esp";
DynamicJsonDocument configToSend(1024);
DynamicJsonDocument dataToAppend(1024);
DynamicJsonDocument dataToSend(60000);
WiFiManager wm;
TinyGPSPlus gps;  // Create a TinyGPS++ object to handle GPS data
WebServer server(80);
Adafruit_MPU6050 mpu;


//File System Custom Functions
void writeFile(String filename, String message) {
  File file = LittleFS.open(filename, "w");
  if (!file) {
    Serial.println("writeFile -> failed to open file for writing");
    return;
  }
  if (file.print(message))
    Serial.println("File written");
  else
    Serial.println("Write failed");

  file.close();
}/*
void appendToFile(String filename, String message) {
  File file = LittleFS.open(filename, "a");
  if (!file) {
    Serial.println("writeFile -> failed to open file for writing");
    writeFile(filename, message);
    return;
  }
  if (file.print(message))
    Serial.println("File updated: " + message);
  else
    Serial.println("append failed");

  file.close();
}
String readFile(String filename) {
  File file = LittleFS.open(filename);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }
  String fileText = "";
  while (file.available())
    String fileText = file.readString();

  file.close();
  return fileText;
}*/
String printFile(const char* path) {
  // Open file for reading
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "No File Found: " + String(path);
  }
  // Read data from file
  String data = file.readString();
  file.close();
  return data;
}



//File System Library Functions
void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path, "r");
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.printf("- read from file: %s\r\n", path);
    String fileContent="";
    
    while(file.available()){
        char tmp = file.read();
        if (tmp != -1) {
            Serial.write(tmp);
            fileContent += tmp;
        }
    }
    dataToSend["sn"] = serialNumber;
    dataToSend["fs"] = file.size();
    // Serial.println("This is the FileContent from readFile: " + fileContent);
    
    file.close();

    dataToSend["fc"] = fileContent.c_str();
    Serial.printf("readFile: data: %s\r\n", path);
}/*
void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}*/
void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\r\nMessage:", path);
    Serial.println(message);
    File file = fs.open(path, FILE_APPEND,true);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    if(file.print(message)){
        file.print(", ");
        Serial.println(message);
        Serial.print("- message appended - ");
    } else {
        Serial.println("- append failed");
    }
    file.close();
}
void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\r\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("- file renamed");
    } else {
        Serial.println("- rename failed");
    }
}
void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}



//Configuration Functions ==> These functions are used to get the saved settings saved before on the esp memory
bool saveConfig() {
  // Create a JSON object
  DynamicJsonDocument doc(1024);
  doc["name"] = "name";
  doc["type"] = "type";
  doc["batterySize"] = 4000;
  doc["memorySize"] = 16;
  doc["version"] = 1;
  doc["sensors"]["1"] = "gps";
  doc["sensors"]["2"] = "temperature";
  doc["sensors"]["3"] = "humidity";

  String jsonString;
  serializeJsonPretty(doc, jsonString);
  Serial.println(jsonString);

  String tmp = "";
  if (serializeJson(doc, tmp) == 0) {  // return nb of successfully written characters
    Serial.println("Failed to write config file!");
    return false;
  }
  Serial.println(tmp);
  writeFile(config_filename, tmp);

  String s = printFile(config_filename);
  Serial.print("The saved config is: \n" + s);

  return true;
}
bool saveConfig(const char* json) {
  StaticJsonDocument<512> doc;
  // Parse the JSON document
  auto error = deserializeJson(doc, json);
  if (error) {
    Serial.println("saveConfig -> Error parsing JSON");
    return false;
  }

  // Read variables from JSON
  name = doc["name"].as<String>();
  type = doc["type"].as<String>();
  batterySize = doc["batterySize"].as<int>();
  memorySize = doc["memorySize"].as<int>();
  version = doc["version"].as<int>();
  sensors = doc["sensors"].as<String>();

  File configFile = LittleFS.open(config_filename, "w");
  if (!configFile) {
    Serial.println("saveConfig -> Failed to open config file for writing");
    return false;
  }

  if (configFile.print(json)) {
    Serial.println("saveConfig -> Config file saved");
  } else {
    Serial.println("saveConfig -> Failed to write config file");
    configFile.close();
    return false;
  }

  configFile.close();

  return true;
}
bool readConfig() {
  File file = LittleFS.open(config_filename, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
  }
  int config_file_size = file.size();
  Serial.println("Config file size: " + String(config_file_size));

  if (config_file_size > 512) {
    Serial.println("Config file too large");
    return false;
  }
  String config_data = file.readString();
  StaticJsonDocument<512> doc;
  auto error = deserializeJson(doc, config_data);
  if (error) {
    Serial.println("Error interpreting config file");
    return false;
  }

  const String _name = doc["name"];
  const String _type = doc["type"];
  const int _batterySize = doc["batterySize"];
  const int _memorySize = doc["memorySize"];
  const int _version = doc["version"];


  // Read the "sensors" array
  const JsonArray& sensorsArray = doc["sensors"].as<JsonArray>();
  const int _sensorsNumber = sensorsArray.size();
  String _sensors[_sensorsNumber];
  for (int i = 0; i < _sensorsNumber; ++i) {
    _sensors[i] = sensorsArray[i].as<String>();
  }

  name = _name;
  type = _type;
  batterySize = _batterySize;
  memorySize = _memorySize;
  version = _version;

  String sensors[_sensorsNumber];

  // Copy the sensor names individually
  for (int i = 0; i < _sensorsNumber; ++i) {
    sensors[i] = _sensors[i];
  }

  return true;
}
//Data Processing
bool saveData(){
  // File file = LittleFS.open(data_filename, "a", true);
  // if (!file || file.isDirectory()) {
  //   Serial.println("Failed to open file for reading, writing new file");
  //   return false;
  // }
  // dataToAppend["fileSize"] = file.size();
  printLocalTime();
  // Serial.printf("Appending to file: %s\r\n", data_filename);
  String jsonStr;
  serializeJson(dataToAppend, jsonStr);
  const char* t = jsonStr.c_str();
  appendFile(LittleFS, data_filename,t);

  Serial.println("dataToAppend content as JSON:");
  Serial.println(jsonStr);
  return true;
}


// NETWOEKINGGGG
//Network Functions allow working with communication and responce proccessing
String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;

  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);

  // If you need Node-RED/server authentication, insert user and password below
  //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

  // Send HTTP POST request
  int httpResponseCode = http.GET();
  String payload = "false";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code for url: ");
    Serial.println(serverName);
    Serial.println("response code: " + httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
String searchJson(String jsonContent) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, jsonContent);

  // Search for the key that matches the serial number
  JsonObject obj = doc.as<JsonObject>();
  String key = "serial-" + String(serialNumber);
  if (obj.containsKey(key)) {
    Serial.print("Found key: ");
    Serial.println(key);
    return obj[key].as<String>();  // Return the value associated with the key
  }

  return "false";  // Return "false" if the key is not found
}
void sendConfig(String data) {
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("X-Device", "ESP32");
  http.addHeader("X-Sensor", "DHT11");
  http.addHeader("X-Temp", "25");
  http.addHeader("X-Humidity", "50");
  configToSend["serialNumber"] = serialNumber;
  configToSend["message"] = data;
  configToSend["timestamp"] = millis();


  String requestBody;
  serializeJson(configToSend, requestBody);
  Serial.println(requestBody);
  int httpResponse = http.POST(requestBody);
  if (httpResponse > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponse);
    String response = http.getString();
    Serial.println(response);
  } else {
    Serial.print("HTTP Error code: ");
    Serial.println(httpResponse);
  }
  http.end();
}
void sendJSON(String data) {
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device", "ESP32");
  http.addHeader("X-Sensor", "DHT11");
  http.addHeader("X-Temp", "25");
  http.addHeader("X-Humidity", "50");
  configToSend["serialNumber"] = serialNumber;
  configToSend["message"] = data;
  configToSend["timestamp"] = millis();


  String requestBody;
  serializeJson(configToSend, requestBody);
  Serial.println(requestBody);
  int httpResponse = http.POST(requestBody);
  if (httpResponse > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponse);
    String response = http.getString();
    Serial.println(response);
  } else {
    Serial.print("HTTP Error code: ");
    Serial.println(httpResponse);
  }
  http.end();
}


bool printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return false;
  }
  int dayOfWeek = timeinfo.tm_wday;
  int month = timeinfo.tm_mon + 1;  // tm_mon ranges from 0 to 11, so add 1
  int dayOfMonth = timeinfo.tm_mday;
  int year = timeinfo.tm_year + 1900;  // tm_year is the number of years since 1900
  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;
  int second = timeinfo.tm_sec;

  Serial.print("Day of the week: ");
  Serial.println(dayOfWeek);
  Serial.print("Month: ");
  Serial.println(month);
  Serial.print("Day of the Month: ");
  Serial.println(dayOfMonth);
  Serial.print("Year: ");
  Serial.println(year);
  Serial.print("Hour: ");
  Serial.println(hour);
  Serial.print("Minute: ");
  Serial.println(minute);
  Serial.print("Second: ");
  Serial.println(second);

  //
  // Set the time components in the JSON object
  configToSend["time"]["dayOfWeek"] = dayOfWeek;
  configToSend["time"]["month"] = month;
  configToSend["time"]["dayOfMonth"] = dayOfMonth;
  configToSend["time"]["year"] = year;
  configToSend["time"]["hour"] = hour;
  configToSend["time"]["minute"] = minute;
  configToSend["time"]["second"] = second;

  //update dataToAppend
  dataToAppend["t"]["m"] = month;
  dataToAppend["t"]["d"] = dayOfMonth;
  dataToAppend["t"]["y"] = year;
  dataToAppend["t"]["h"] = hour;
  dataToAppend["t"]["min"] = minute;
  dataToAppend["t"]["s"] = second;

  return true;
}



//Endpoints methods for handeling web server
void handleRoot() {

  char temp[400];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 400,

           "<html>\
              <head>\
                <meta http-equiv='refresh' content='5'/>\
                <title>ESP32 Demo</title>\
                <style>\
                  body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
                </style>\
              </head>\
              <body>\
                <h1>Hello from ESP32!</h1>\
                <p>Uptime: %02d:%02d:%02d</p>\
              </body>\
            </html>",

           hr, min % 60, sec % 60);
  server.send(200, "text/html", temp);
}
void sendUpdate() {
  configToSend["timestamp"] = millis();
  printLocalTime();
  String str;
  serializeJson(configToSend, str);
  server.send(200, "text/json", str);
}
void deleteData(){
  deleteFile(LittleFS, data_filename);
  server.send(200,"text/plain","filedeleted");
}
void sendData(){
  readFile(LittleFS, data_filename);
  String str;
  serializeJson(dataToSend, str);
  // Serial.println("sentData: after request from server: " + str);
   Serial.println("sentData");
  dataToSend["fc"] = "";
  dataToSend["fs"] = "";

  server.send(200, "text/json", str);
  delay(10);
  // sendConfig(dt);
  // sendJSON(dt);
}


//these functions are meant to proccess sernsors and there values
bool getGPS() {
  bool updated = false;
  // Read data from the GPS module
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      // Check if new GPS data is available
      if (gps.location.isUpdated()) {
        // Retrieve latitude and longitude
        float latitude = gps.location.lat();
        float longitude = gps.location.lng();
        configToSend["gps"]["latitude"] = latitude;
        configToSend["gps"]["longitude"] = longitude;
        // Create a string to store the formatted latitude and longitude
        updated = true;
      }
    }
  }
  return updated;
}
void setupAccelerometer() {
  int i = 0;
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (i < 100) {
      delay(10);
      i++;
    }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Accelerometer range set to: ");
  switch (mpu.getAccelerometerRange()) {
    case MPU6050_RANGE_2_G:
      Serial.println("+-2G");
      break;
    case MPU6050_RANGE_4_G:
      Serial.println("+-4G");
      break;
    case MPU6050_RANGE_8_G:
      Serial.println("+-8G");
      break;
    case MPU6050_RANGE_16_G:
      Serial.println("+-16G");
      break;
  }
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Gyro range set to: ");
  switch (mpu.getGyroRange()) {
    case MPU6050_RANGE_250_DEG:
      Serial.println("+- 250 deg/s");
      break;
    case MPU6050_RANGE_500_DEG:
      Serial.println("+- 500 deg/s");
      break;
    case MPU6050_RANGE_1000_DEG:
      Serial.println("+- 1000 deg/s");
      break;
    case MPU6050_RANGE_2000_DEG:
      Serial.println("+- 2000 deg/s");
      break;
  }

  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.print("Filter bandwidth set to: ");
  switch (mpu.getFilterBandwidth()) {
    case MPU6050_BAND_260_HZ:
      Serial.println("260 Hz");
      break;
    case MPU6050_BAND_184_HZ:
      Serial.println("184 Hz");
      break;
    case MPU6050_BAND_94_HZ:
      Serial.println("94 Hz");
      break;
    case MPU6050_BAND_44_HZ:
      Serial.println("44 Hz");
      break;
    case MPU6050_BAND_21_HZ:
      Serial.println("21 Hz");
      break;
    case MPU6050_BAND_10_HZ:
      Serial.println("10 Hz");
      break;
    case MPU6050_BAND_5_HZ:
      Serial.println("5 Hz");
      break;
  }
}
void getAccelerometer(){
  /* Get new sensor events with the readings */
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  /* Add accelerometer values */
  configToSend["accelerometer"]["AccelerationX"] = a.acceleration.x;
  configToSend["accelerometer"]["AccelerationY"] = a.acceleration.y;
  configToSend["accelerometer"]["AccelerationZ"] = a.acceleration.z;

  /* Add gyroscope values */
  configToSend["gyroscope"]["RotationX"] = g.gyro.x;
  configToSend["gyroscope"]["RotationY"] = g.gyro.y;
  configToSend["gyroscope"]["RotationZ"] = g.gyro.z;

  /* Add temperature value */
  configToSend["temperature"] = temp.temperature;
  dataToAppend["tmp"] = temp.temperature;

  /* Print out the values */
  Serial.print("Acceleration X: ");
  Serial.print(a.acceleration.x);
  Serial.print(", Y: ");
  Serial.print(a.acceleration.y);
  Serial.print(", Z: ");
  Serial.print(a.acceleration.z);
  Serial.println(" m/s^2");

  Serial.print("Rotation X: ");
  Serial.print(g.gyro.x);
  Serial.print(", Y: ");
  Serial.print(g.gyro.y);
  Serial.print(", Z: ");
  Serial.print(g.gyro.z);
  Serial.println(" rad/s");

  Serial.print("Temperature: ");
  Serial.print(temp.temperature);
  Serial.println(" degC");

  Serial.println("");
}


void getMemory(){
  // Calculate size of configToSend
  size_t dataSize = measureJson(configToSend);
  Serial.print("Size of configToSend: ");
  Serial.println(dataSize);

  // Get the total free space
  size_t totalBytes = LittleFS.totalBytes();
  Serial.print("Total free space: ");
  Serial.println(totalBytes);

  // Get the used space
  size_t usedBytes = LittleFS.usedBytes();
  Serial.print("Used space: ");
  Serial.println(usedBytes);

  // Fill LittleFS information in configToSend
  configToSend["memory"]["totalBytes"] = totalBytes;
  configToSend["memory"]["usedBytes"] = usedBytes;
  configToSend["memory"]["dataSize"] = dataSize;
}



void setup() {
  Serial.begin(115200);
  // neogps.begin(9600, SERIAL_8N1, 16, 17);  // Initialize the GPS module serial port at 9600 baud
  wm.setConfigPortalBlocking(true);
  // wm.setTimeout(30);
  while(!wm.autoConnect("ESP32-HassanTofayli")){
        Serial.println("connecting... :)");
    }
  sendConfig("hi from beg");


  if (!LittleFS.begin(false)) {
    Serial.println("setup -> LITTLEFS Mount --> failed");
    Serial.println("setup -> No file system system found; Memory will be formatted");
    if (!LittleFS.begin(true)) {
      Serial.println("setup -> LITTLEFS mount failed");
      Serial.println("setup -> Formatting not possible");
      return;
    } else {
      Serial.println("setup -> The file system is formatted");
    }
  } else {
    Serial.println("setup -> LITTLEFS mounted successfully");
    Serial.print("config file before: ");
    Serial.print(printFile(config_filename));

    if (readConfig() == false) {
      Serial.println("setup -> Could not read Config file -> initializing new file");
      if (saveConfig()) {
        Serial.println("setup -> Config file saved with content: ");
        printFile(config_filename);
      }
    }
  }
  Serial.println("My serialNumber is: " + String(serialNumber));
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  // accelometer gyroscope temprature sensor
  setupAccelerometer();


  // WebServer Configuration
  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }
  // End Points
  server.on("/", handleRoot);
  server.on("/getUpdate", sendUpdate);
  server.on("/sendData", sendData);
  server.on("/deleteData", deleteData);

  server.begin();
  Serial.println("HTTP server started");


  // Serial of GPS data
  Serial2.begin(9600);
  while (!Serial2)
    ;
  Serial.println("GPS Module Example");
}

void loop() {
  if (WiFi.isConnected() || WiFi.status() != WL_CONNECTED) {
    // wm.stopConfigPortal();
    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
    String ipAddress = WiFi.localIP().toString();
    configToSend["ip_address"] = ipAddress;
  } else {
    Serial.println("No WiFi connected");
    wm.setConfigPortalBlocking(false);
    wifi_connected = wm.autoConnect("ESP32-HassanTofayli");
    if (wifi_connected) {
      // Stop the access point
      wm.stopConfigPortal();
    }
  }
  server.handleClient();
  delay(1000);

  configToSend["currentTime"] = printLocalTime();

  configToSend["gps"]["updated"] = getGPS();

  getAccelerometer();

  getMemory();

  unsigned long currentMillis = millis();
  unsigned long previousMillis = 0;
  if (currentMillis - previousMillis >= 4000) {
    saveData();
    previousMillis = currentMillis;
  }

  
  // sendData();

  // sendConfig(dt);
  // sendJSON(dt);
}
