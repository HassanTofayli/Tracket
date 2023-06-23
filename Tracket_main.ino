#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <HardwareSerial.h>

#define FORMAT_LITTLEFS_IF_FAILED true

long timezone = 1;
byte daysavetime = 1;

const char* config_filename = "/config.json";
const int serialNumber = 123456;
String name = "DefaultVehcileName";
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
String api_user_key;
const char* serverUrl = "http://192.168.1.109:3000/esp";
WiFiClient wifiClient;
HTTPClient httpClient;
HardwareSerial SerialGPS(2);   // Define the serial port to use for the GPS module
char gpsData[256]; // Declare a character array to store GPS data

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
  if (api_user_key == "NoKey") {
    Serial.println("No User Key Found To Get the Private Paste");
    return "false";
  }
  http.begin("https://pastebin.com/api/api_raw.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST("api_dev_key=" + api_dev_key + "&api_user_key=" + api_user_key + "&api_option=show_paste&api_paste_key=" + configPasteKey);

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
    return obj[key].as<String>();  // Return the value associated with the key
  }

  return "false";  // Return "false" if the key is not found
}
String createNewPaste() {
  String url = "https://pastebin.com/api/api_post.php";
  String postData = "api_option=paste&api_user_key=" + api_user_key + "&api_paste_private=2&api_paste_name=My%20Private%20Paste&api_paste_expire_date=N&api_dev_key=" + String(api_dev_key) + "&api_paste_code=This%20is%20my%20private%20paste%20content.";
  httpClient.begin(url);
  httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = httpClient.POST(postData);
  String response = httpClient.getString();
  httpClient.end();
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Paste created. URL: " + response);
  } else {
    Serial.println("Error creating paste");
  }
}
String getUserApiKey() {

  HTTPClient http;
  http.begin("https://pastebin.com/api/api_login.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST("api_dev_key=" + api_dev_key + "&api_user_name=" + api_user_name + "&api_user_password=" + api_user_password);

  // Check the response
  String response = http.getString();
  if (response.startsWith("Bad API request")) {
    Serial.println("Login failed: " + response);
    return "NoKey";
  }

  http.end();
  Serial.println("getUserApiKey: " + response);
  return response;
}

void sendTextData( String data) {
  HTTPClient http;
  String serverUrl = "http://192.168.1.109:3000/esp";
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("X-Device", "ESP32");
  http.addHeader("X-Sensor", "DHT11");
  http.addHeader("X-Temp", "25");
  http.addHeader("X-Humidity", "50");

  StaticJsonDocument<200> jsonDoc;
  jsonDoc["message"] = data;
  jsonDoc["timestamp"] = millis();
  String requestBody;
  serializeJson(jsonDoc, requestBody);
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




void setup() {
  Serial.begin(115200);
  SerialGPS.begin(9600, SERIAL_8N1, 3, 1); // Initialize the GPS module serial port
  wm.setConfigPortalBlocking(false);
  wm.setTimeout(30);
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
  api_user_key = getUserApiKey();
  // Serial.println(api_user_key);
  // if(api_user_key!="NoKey"){
  //   configFetching();
  // createNewPaste();
  // }

  // // Get user key
  // HTTPClient http;
  // String url = "https://pastebin.com/api/api_login.php";
  // String postData = "api_option=login&api_user_name=" + String(api_user_name) + "&api_user_password=" + String(api_user_password) + "&api_dev_key=" + String(api_dev_key);
  // http.begin(url);
  // http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // int httpCode = http.POST(postData);
  // String response = http.getString();
  // http.end();
  // if (httpCode == HTTP_CODE_OK) {
  //   String api_user_key = response;
  //   Serial.println("User key retrieved: " + api_user_key);

  // Create private paste
}

void loop() {

  if (WiFi.isConnected() || WiFi.status() != WL_CONNECTED) {
    // wm.stopConfigPortal();
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
  if (SerialGPS.available()) { // Check if GPS data is available
    Serial.write(SerialGPS.read()); // Echo GPS data to the Serial Monitor
  }
String gpsData = ""; // Declare a string variable to store GPS data
  
  // Wait for GPS data to become available
  while (SerialGPS.available() > 0) {
    char c = SerialGPS.read(); // Read character from GPS module
    gpsData += c; // Append character to gpsData string
    delay(1);
  }
  
  if (gpsData.length() > 0) {
    // Print GPS data to Serial Monitor
    Serial.println(gpsData);
  }
  String dataToSend = gpsData;
  sendTextData(dataToSend);

  delay(5000);
}
