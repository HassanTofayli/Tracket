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

#define FORMAT_LITTLEFS_IF_FAILED true

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;      // GMT offset: UTC+2 (Lebanon Standard Time)
const int daylightOffset_sec = 3600;  // Daylight offset: UTC+3 (Lebanon Daylight Time)

const char* config_filename = "/config.json";
const int serialNumber = 123456;
String name = "DefaultVehcileName";
String type;
int batterySize;
int memorySize;
int version;
String sensors;
bool wifi_connected = false;
unsigned long lastTime = 0;
bool configFetchingSucc = false;
const char* serverUrl = "http://192.168.1.109:3000/esp";
DynamicJsonDocument dataToSend(1024);
WiFiManager wm;
TinyGPSPlus gps;  // Create a TinyGPS++ object to handle GPS data
WebServer server(80);
Adafruit_MPU6050 mpu;



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
}
void appendToFile(String filename, String message) {
  File file = LittleFS.open(filename, "a");
  if (!file) {
    Serial.println("writeFile -> failed to open file for writing");
    return;
  }
  if (file.print(message))
    Serial.println("File updated");
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
}
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
void sendData(String data) {
  HTTPClient http;
  String serverUrl = "http://192.168.1.109:3000/esp";
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("X-Device", "ESP32");
  http.addHeader("X-Sensor", "DHT11");
  http.addHeader("X-Temp", "25");
  http.addHeader("X-Humidity", "50");
  dataToSend["message"] = data;
  dataToSend["timestamp"] = millis();


  String requestBody;
  serializeJson(dataToSend, requestBody);
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
  dataToSend["time"]["dayOfWeek"] = dayOfWeek;
  dataToSend["time"]["month"] = month;
  dataToSend["time"]["dayOfMonth"] = dayOfMonth;
  dataToSend["time"]["year"] = year;
  dataToSend["time"]["hour"] = hour;
  dataToSend["time"]["minute"] = minute;
  dataToSend["time"]["second"] = second;

  return true;
}




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
  String str;
  serializeJson(dataToSend, str);
  server.send(200, "text/json", str);
}





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
        dataToSend["gps"]["latitude"] = latitude;
        dataToSend["gps"]["longitude"] = longitude;
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
  dataToSend["accelerometer"]["AccelerationX"] = a.acceleration.x;
  dataToSend["accelerometer"]["AccelerationY"] = a.acceleration.y;
  dataToSend["accelerometer"]["AccelerationZ"] = a.acceleration.z;

  /* Add gyroscope values */
  dataToSend["gyroscope"]["RotationX"] = g.gyro.x;
  dataToSend["gyroscope"]["RotationY"] = g.gyro.y;
  dataToSend["gyroscope"]["RotationZ"] = g.gyro.z;

  /* Add temperature value */
  dataToSend["temperature"] = temp.temperature;

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

void setup() {



  Serial.begin(115200);
  // neogps.begin(9600, SERIAL_8N1, 16, 17);  // Initialize the GPS module serial port at 9600 baud
  wm.setConfigPortalBlocking(false);
  wm.setTimeout(30);
  wm.autoConnect("ESP32-HassanTofayli");
  sendData("hi from beg");


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
    dataToSend["ip_address"] = ipAddress;
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


  dataToSend["currentTime"] = printLocalTime();

  dataToSend["gps"]["updated"] = getGPS();

  getAccelerometer();


  sendData("No message");
  delay(1000);
}
