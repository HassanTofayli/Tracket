#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <time.h>
#include <TinyGPS++.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <ESP.h>

#define FORMAT_LITTLEFS_IF_FAILED true
#define CHUNK_SIZE 5000  // or as much as an HTTP POST request can handle

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;      // GMT offset: UTC+2 (Lebanon Standard Time)
const int daylightOffset_sec = 3600;  // Daylight offset: UTC+3 (Lebanon Daylight Time)

// Enter Company Serial Number Here
const int serialNumber = 347896;
String sn = "347896";
int currentDataSize;
const char* data_filename = "/data.txt";
String name = "name";
String type = "type";
int batterySize = 4000;
int memorySize = 16;
int version = 1;
String sensors[] = { "gps", "temperature", "humidity" };
bool wifi_connected = false;
unsigned long lastTime = 0;
bool configFetchingSucc = false;
// const char* serverUrl = "http://192.168.33.99:3000/esp"; //hassan Tofayli
const char* serverUrl = "http://192.168.1.106:3000/esp";
const char* sendDataUrl = "http://192.168.1.106:3000/getdata";
// const char* serverUrl = "http://192.168.1.109:3000/esp"; Home
// const char* sendDataUrl = "http://192.168.1.109:3000/getdata"; Home
// const char* serverUrl = "https://esp-dt.glitch.me/esp";
int arrayindex = 0;
unsigned long previousMillis = 0;
String configToSend = "";
String dataToAppend = "";
String dataToSend = "";
WiFiManager wm;
TinyGPSPlus gps;  // Create a TinyGPS++ object to handle GPS data
WebServer server(80);
Adafruit_MPU6050 mpu;
String ipAddress;

String travel_name;
String travel_dest;
String travel_tkn="notkn";
String travel_year;
String travel_month;
String travel_day;
String travel_hour;
String travel_min;
double latitudes[200];
double longitudes[200];

//File System Custom Functions
bool writeFile(String filename, String message) {
  File file = LittleFS.open(filename, "w");
  if (!file) {
    Serial.println("writeFile -> failed to open file for writing");
    return false;
  }
  if (file.print(message))
    Serial.println("File written");
  else
    Serial.println("Write failed");

  file.close();
  return true;
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



//File System Library Functions
void readFile(fs::FS& fs, const char* path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.printf("- read from file: %s\r\n", path);
  String fileContent = "";

  while (file.available()) {
    char tmp = file.read();
    if (tmp != -1) {
      Serial.write(tmp);
      fileContent += tmp;
    }
  }

  String serialNumberString = String(serialNumber);
  int fileSize = serialNumberString.length() + file.size() + 10;
  dataToSend = "sn=" + serialNumberString + "&fs=" + fileSize + fileContent;
  Serial.println("readFile method fileContent: " + dataToSend);
  file.close();

  Serial.printf("readFile: data: %s\r\n", path);
} /*
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
void appendFile(fs::FS& fs, const char* path, const char* message) {
  Serial.printf("Appending to file: %s\r: Message:", path);
  String messageUpdate = String(message) + ";";
  // arrayindex++;
  File file = fs.open(path, FILE_APPEND, true);
  if (!file) {
    Serial.println("- failed to open file for appending");
    return;
  }
  if (file.print(messageUpdate.c_str())) {
    Serial.print(messageUpdate);
    Serial.print(" - message appended - ");
    currentDataSize = file.size();
    Serial.print("\ncurrentDataSize: " + String(currentDataSize) + "\n");
  } else {
    Serial.println("- append failed");
  }
  file.close();
}
void renameFile(fs::FS& fs, const char* path1, const char* path2) {
  Serial.printf("Renaming file %s to %s\r\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("- file renamed");
  } else {
    Serial.println("- rename failed");
  }
}
void deleteFile(fs::FS& fs, const char* path) {
  File file = LittleFS.open(path, "w");
  if (file) {
    file.close();
    Serial.println("\nFile cleared successfully");
  } else {
    Serial.println("\nFailed to clear file");
  }
  // arrayindex=0;
  Serial.printf("Deleting file: %s\r\n", path);
  if (fs.remove(path)) {
    Serial.println("\n\n- file deleted\n\n");
  } else {
    Serial.println("\n\n- delete failed\n\n");
  }
}




//Data Processing
bool saveData() {
  const char* t = dataToAppend.c_str();
  appendFile(LittleFS, data_filename, t);
  Serial.print("dataToAppend content as String:");
  Serial.println(dataToAppend);
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
void sendConfig(String data) {
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("sn", sn);
  http.addHeader("dataSize", String(currentDataSize));
  int httpResponse = http.POST(configToSend);
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
    String time = "time=0-0-0-0-0-0";
    Serial.print(time);
    dataToAppend += time;
    configToSend += time;
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
  Serial.print(dayOfWeek);
  Serial.print(", Month: ");
  Serial.print(month);
  Serial.print(", Day of the Month: ");
  Serial.print(dayOfMonth);
  Serial.print(", Year: ");
  Serial.print(year);
  Serial.print(", Hour: ");
  Serial.print(hour);
  Serial.print(", Minute: ");
  Serial.print(minute);
  Serial.print(", Second: ");
  Serial.println(second);

  // Update dataToAppend
  String time = "time=" + String(year) + "-" + String(month) + "-" + String(dayOfMonth) + "-" + String(hour) + "-" + String(minute) + "-" + String(second);
  Serial.print(time);
  dataToAppend += time + ",";
  configToSend += time;
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
  configToSend += "&millis=" + String(millis());
  server.send(200, "text/plain", configToSend);
  int startIndex = configToSend.indexOf("&millis=");
  configToSend = configToSend.substring(0, startIndex);
}
void sendTravelData() {
  const char* trv = "/travel.txt";
  sendDataInChunks(trv,false);
  server.send(200, "text/plain", "OK");
}
void deleteData() {
  deleteFile(LittleFS, data_filename);
  server.send(200, "text/plain", "filedeleted");
}
void sendData() {
  // readFile(LittleFS, data_filename);
  sendDataInChunks(data_filename, true);
  Serial.println("sentData: after request from server: ");
  server.send(200, "text/plain", "sent to /esp");
  dataToSend = "";

  delay(10);
}
void sendDataInChunks(const char* path, bool del) {
  bool received = del;
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");

    HTTPClient http;


    size_t file_size = file.size();
    // int file_size_int = file.size();
    size_t chunks = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;  // Calculate number of chunks
    // String fs = String(file_size);


    for (int i = 0; i < chunks; i++) {
      http.begin(sendDataUrl);  //Specify destination for HTTP request
      http.addHeader("Content-Type", "text/plain");
      http.addHeader("sn", sn);
      http.addHeader("tkn", travel_tkn);
      // http.addHeader("fs=", fs);
      char buffer[CHUNK_SIZE + 1];
      int bytesRead = file.readBytes(buffer, CHUNK_SIZE);
      buffer[bytesRead] = '\0';  // null terminate the buffer
      Serial.println(buffer);
      // Now send this chunk to the server
      int httpResponseCode = http.POST(buffer);  //Send the request

      if (httpResponseCode > 0) {
        String response = http.getString();      //Get the response to the request
        Serial.print(httpResponseCode + " - ");  //Print return code
        Serial.println(response);                //Print request response payload
      } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
        received = false;
      }

      http.end();  //Free resources
    }

    file.close();



  } else {
    Serial.println("File does not exist.");
    received = false;
  }
  if (received)
    deleteFile(LittleFS, path);
}


void handleGetTravel() {
  String message = "This is message from handleGetTravel\n";


  for (int i = 0; i < server.args(); i++) {
    String argname = server.argName(i);
    String arg = server.arg(i);
    //ROUTE
    if (argname == "route") {

      Serial.print("I am arg==rout");
      StaticJsonDocument<512> doc;
      deserializeJson(doc, arg);
      JsonArray array = doc.as<JsonArray>();

      // Iterate over the array and extract latitude and longitude values
      for (int i = 0; i < array.size(); i++) {
        JsonObject obj = array[i];
        double lat = obj["lat"];
        double lng = obj["lng"];
        latitudes[i] = lat;
        longitudes[i] = lng;
      }

      // Print extracted values
      for (int i = 0; i < 19; i++) {
        Serial.print("Latitude ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(latitudes[i], 6);
        Serial.print("Longitude ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(longitudes[i], 6);
      }
    }
    else if (argname=="name"){
      travel_name = arg;
    } else if (argname=="destination"){
      travel_dest = arg;
    } else if (argname=="dataToken"){
      travel_tkn = arg;
    } else if (argname == "year") {
      travel_year = arg;
    } else if (argname == "month") {
      travel_month = arg;
    } else if (argname == "day") {
      travel_day = arg;
    } else if (argname == "hour") {
      travel_hour = arg;
    } else if (argname == "minutes") {
      travel_min = arg;
    }

    message += argname + ": ";
    message += arg;
  }
  Serial.println(message);
  // Send the message in response
  server.send(200, "text/plain", message);
  message = "";

  File file = LittleFS.open("/travel.txt", "w");
  if (file) {
    file.print(travel_name);
    file.print(",");
    file.print(travel_dest);
    file.print(",");
    file.print(travel_tkn);
    file.print(",");
    file.print(travel_year);
    file.print(",");
    file.print(travel_month);
    file.print(",");
    file.print(travel_day);
    file.print(",");
    file.print(travel_hour);
    file.print(",");
    file.print(travel_min);
    file.print(",");
    for (int i = 0; i < 19; i++) {
      file.print(latitudes[i], 6);
      file.print(",");
      file.print(longitudes[i], 6);
      file.print(",");
    }
    file.close();
  } else {
    Serial.println("Failed to open file for writing");
  }

} /*
void handleTravelGet() {
  server
  String data = "nothing\n";

  static DynamicJsonDocument jsonBuffer(1024);
  if (server.hasArg("plain") == false){ //Check for any body
  for (int i = 0; i < server.headers(); i++) {
    String headerName = server.headerName(i);
    String header = server.header(i);
    data += headerName + ": " + header+"\n"; 
    Serial.print(headerName + ": " + header);
  }
    // data += data+"\nthe server.args="+Stringa(server.args());
    server.send(200, "text/plain", "Body not received, "+data);
    return;
  }

  for (int i = 0; i < server.headers(); i++) {
    String headerName = server.headerName(i);
    String header = server.header(i);
    data += headerName + ": " + header+"\n"; 
    Serial.print(headerName + ": " + header);
  }
    server.send(200, "text/plain", "Body not received, " +data);

}
*/


void saveJSONData(const char* filename, const String& jsonData) {
  File file = LittleFS.open(filename, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  file.print(jsonData);
  file.close();
}






//these functions are meant to proccess sernsors and there values
bool getGPS() {
  String values = "&updated=false&lat=0&lng=0";
  String valuesToAppend = ",updated=false,lat=0,lng=0";
  bool updated = false;
  // Read data from the GPS module
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      // Check if new GPS data is available
      if (gps.location.isUpdated()) {
        // Retrieve latitude and longitude
        float latitude = gps.location.lat();
        float longitude = gps.location.lng();
        values = "&updated=true&lat=" + String(latitude) + "&lng=" + String(longitude);
        valuesToAppend = ",updated=true,lat=" + String(latitude) + ",lng=" + String(longitude);
        updated = true;
      }
    }
  }
  configToSend += values;
  dataToAppend += valuesToAppend;
  Serial.println(valuesToAppend);
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
void getAccelerometer() {
  /* Get new sensor events with the readings */
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  configToSend += "&accelerationX=" + String(a.acceleration.x);
  configToSend += "&accelerationY=" + String(a.acceleration.y);
  configToSend += "&accelerationZ=" + String(a.acceleration.z);

  configToSend += "&rotationX=" + String(g.gyro.x);
  configToSend += "&rotationY=" + String(g.gyro.y);
  configToSend += "&rotationZ=" + String(g.gyro.z);

  configToSend += "&temperature=" + String(temp.temperature);
  dataToAppend += "tmp=" + String(temp.temperature);

  /* Print out the values */
  Serial.print("Acceleration X: ");
  Serial.print(a.acceleration.x);
  Serial.print(", Y: ");
  Serial.print(a.acceleration.y);
  Serial.print(", Z: ");
  Serial.print(a.acceleration.z);
  Serial.print(" m/s^2");

  Serial.print(", Rotation X: ");
  Serial.print(g.gyro.x);
  Serial.print(", Y: ");
  Serial.print(g.gyro.y);
  Serial.print(", Z: ");
  Serial.print(g.gyro.z);
  Serial.print(" rad/s");

  Serial.print(", Temperature: ");
  Serial.print(temp.temperature);
  Serial.println(" degC");
}



void getMemory() {
  // Calculate size of configToSend
  size_t dataSize = configToSend.length();
  Serial.print("Size of configToSend: ");
  Serial.print(dataSize);

  // Get the total free space
  size_t totalBytes = LittleFS.totalBytes();
  Serial.print(", Total free space: ");
  Serial.print(totalBytes);

  // Get the used space
  size_t usedBytes = LittleFS.usedBytes();
  Serial.print(", Used space: ");
  Serial.println(usedBytes);

  // Fill LittleFS information in configToSend
  configToSend += "&totalBytes=" + String(totalBytes);
  configToSend += "&usedBytes=" + String(usedBytes);
}

void setup() {
  Serial.begin(115200);
  // neogps.begin(9600, SERIAL_8N1, 16, 17);  // Initialize the GPS module serial port at 9600 baud
  wm.setConfigPortalBlocking(true);
  // wm.setTimeout(30);
  while (!wm.autoConnect("ESP32-HassanTofayli")) {
    Serial.println("connecting... :)");
  }
  sendConfig("Hello from esp, sn=" + String(serialNumber));


  if (!LittleFS.begin(false)) {
    Serial.println("setup -> LITTLEFS Mount --> failed");
    Serial.println("setup -> No file system system found; Memory will be formatted");
    if (!LittleFS.begin(true)) {
      Serial.println("setup -> LITTLEFS mount failed");
      Serial.println("setup -> Formatting not possible");
      return;
    } else Serial.println("setup -> The file system is formatted");
  } else Serial.println("setup -> LITTLEFS mounted successfully");

  Serial.println("My serialNumber is: " + String(serialNumber));
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

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
  server.on("/sendTravel", HTTP_GET, handleGetTravel);
  server.on("/sendTravelData", HTTP_GET, sendTravelData);

  server.begin();
  Serial.println("HTTP server started");


  // Serial of GPS data
  Serial2.begin(9600);
  while (!Serial2)
    ;
  Serial.println("GPS Module Example");
}

void loop() {
  unsigned long currentMillis = millis();
  if (WiFi.isConnected() || WiFi.status() != WL_CONNECTED) {
    // wm.stopConfigPortal();
    if (currentMillis - previousMillis >= 5000) {
      Serial.print(" -WiFi connected: ");
      Serial.print(WiFi.localIP());
      ipAddress = WiFi.localIP().toString();
    }
  } else {
    if (currentMillis - previousMillis >= 1000) {
      ipAddress = "0.0.0.0";
      Serial.print("No WiFi connected");
    }
    wm.setConfigPortalBlocking(false);
    wifi_connected = wm.autoConnect("ESP32-HassanTofayli");
    if (wifi_connected) {
      // Stop the access point
      wm.stopConfigPortal();
    }
  }
  server.handleClient();
  delay(2);
  // delay(500);


  if (currentMillis - previousMillis >= 5000) {
    configToSend = "sn=" + String(serialNumber) + "&";
    configToSend += "&ip_address=" + ipAddress + "&";
    dataToAppend = "";
    printLocalTime();
    getAccelerometer();
    getGPS();
    getMemory();
    saveData();
    sendConfig("");
    sendDataInChunks(data_filename, true);
    previousMillis = currentMillis;
  }
}
