#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

#define FORMAT_LITTLEFS_IF_FAILED true

long timezone = 1;
byte daysavetime = 1;

const char* config_filename = "/config.json";
const int serialNumber = 123456;
String name;
String type;
int batterySize;
int memorySize;
int version;
String sensors;
bool wifi_connected = false;
WiFiManager wm;
unsigned long lastTime = 0;
String configPasteKey = "v8TbLWDj";
String configPasteLink = "https://pastebin.com/api/api_raw.php?i=" + configPasteKey;
bool configFetchingSucc = false;
String api_dev_key = "nbDlIM7wkLevjB6-wdT__xI2nSHhLnhW";
String api_user_name = "HassanTofayli";
String api_user_password = "Tracket.vehiclecontrolsystem";
String vehiclePasteKey = "";

WiFiClient wifiClient;
HTTPClient httpClient;


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
  Serial.print(s + " \nbool saveConfig() {everythingisSuccess}");

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
    Serial.print("HTTP Response code: ");
    Serial.println(serverName);
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

String getPrivatePaste(String url) {
  
  HTTPClient http;
  http.begin("https://pastebin.com/api/api_login.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST("api_dev_key="+api_dev_key+"&api_user_name="+api_user_name+"&api_user_password="+api_user_password);

  // Check the response
  String response = http.getString();
  if (response.startsWith("Bad API request")) {
    Serial.println("Login failed");
    return "Login failed" + response;
  }

  http.end();

  // Send another HTTP request to get the private paste
  http.begin("https://pastebin.com/api/api_raw.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST("api_dev_key="+api_dev_key+"&api_user_key=" + response + "&api_option=show_paste&api_paste_key="+configPasteKey);

  // Check the response
  String payload = http.getString();
  Serial.println("Peivate Paste Content: " + payload);
  http.end();
  return payload;
}

String configFetching() {

  String configJson = getPrivatePaste(configPasteLink);
  if (configJson != "false") {
    configFetchingSucc = true;
    Serial.println("Config File: " + configJson);
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, configJson);
      String outputString;
      serializeJson(doc, outputString);
      Serial.println(outputString);
    vehiclePasteKey = searchJson(outputString);
    Serial.println(vehiclePasteKey);
  } else {
    Serial.print("Error: ");
    Serial.println(configJson);
  }
  return configJson;
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
    return obj[key].as<String>(); // Return the value associated with the key
  }

  return "false"; // Return "false" if the key is not found
}



void setup() {
  Serial.begin(115200);
  wm.setConfigPortalBlocking(false);
  wm.autoConnect("ESP32-HassanTofayli");


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
  configFetching();
}

void loop() {

  if (WiFi.isConnected() || WiFi.status() != WL_CONNECTED) {

    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("No WiFi connected");
    wm.setConfigPortalBlocking(false);
    wifi_connected = wm.autoConnect("ESP32-HassanTofayli");
    if (wifi_connected) {
      // Stop the access point
      wm.stopConfigPortal();
    }
  }


  delay(5000);
}
