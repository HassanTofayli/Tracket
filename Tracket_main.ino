#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

#define FORMAT_LITTLEFS_IF_FAILED true

const String config_filename = "/config.json";
const int serialNumber = 12345;
String name;
String type;
int batterySize;
int memorySize;
int version;
String sensors;

WiFiManager wm;

void setup() {
  Serial.begin(115200);
  wm.setConfigPortalBlocking(false);
  wm.autoConnect("ESP32-HassanTofayli");


  Serial.print("config file before: ");
  printConfigFile();
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
    if (readConfig() == false) {
      Serial.println("setup -> Could not read Config file -> initializing new file");
      if (saveConfig()) {
        Serial.println("setup -> Config file saved with content: ");
        printConfigFile();
      }
    }
  }

  Serial.println("My serialNumber is: " + String(serialNumber));
}

void loop() {

  if (WiFi.isConnected()) {
    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("No WiFi connected");
  }

  Serial.println(name+", "+"batterySize: "+batterySize);


  /* if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    doc["data"] = "Hello, MongoDB!";

    // Serialize JSON to string
    String jsonPayload;
    serializeJson(doc, jsonPayload);

    int httpResponseCode = http.POST(jsonPayload);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } */

  delay(5000);  // Wait for 5 seconds
}







bool saveConfig() {

  // Create a JSON object
  DynamicJsonDocument doc(1024);

  // Set values in the JSON object
  doc["name"] = "name";
  doc["type"] = "type";
  doc["batterySize"] = 4000;
  doc["memorySize"] = 16;
  doc["version"] = 1;
  doc["sensors"]["1"] = "gps";
  doc["sensors"]["2"] = "temperature";
  doc["sensors"]["3"] = "humidity";



  // Serialize the JSON object to the config file
  if (serializeJson(doc, configFile) == 0) {  // return nb of successfully written characters
    Serial.println("Failed to write config file!");
    configFile.close();
    return false;
  }
  configFile.close();
  Serial.println("Config file created and filled with values.");

  // Read and print the contents of the config file
  String configFileContent = readFile(config_filename);
  Serial.println("Config file content:");
  Serial.println(configFileContent);

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
  String file_content = readFile(config_filename);

  int config_file_size = file_content.length();
  Serial.println("Config file size: " + String(config_file_size));

  if(config_file_size > 512) {
    Serial.println("Config file too large");
    return false;
  }

  StaticJsonDocument<512> doc;
  auto error = deserializeJson(doc, file_content);
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






void writeFile(String filename, String message) {
  File file = LittleFS.open(filename, "w");
  if (!file) {
    Serial.println("writeFile -> failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}
void appendToFile(String filename, String message) {
  File file = LittleFS.open(filename, "a");
  if (!file) {
    Serial.println("writeFile -> failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File updated");
  } else {
    Serial.println("append failed");
  }
  file.close();
}
String readFile(String filename) {
  File file = LittleFS.open(filename);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }

  String fileText = "";
  while (file.available()) {
    String fileText = file.readString();
  }
  file.close();
  return fileText;
}

void printConfigFile() {
  String configContent = readFile(config_filename);
  Serial.println("Config file content:");
  Serial.println(configContent);
}


// {
//   "name": "Example Name",
//   "type": "Example Type",
//   "batterySize": 3000,
//   "memorySize": 64,
//   "version": 2,
//   "sensors": {
//     "1": "gps",
//     "2": "temperature",
//     "3": "humidity"
//   }
//
// }
