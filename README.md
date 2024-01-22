
# Tracket: Vehicle Control System
GPS and Motion Tracking System

## Overview
Tracket is an advanced tracking system designed for real-time GPS and motion sensing applications. Utilizing the power of ESP32, it offers a robust solution for various tracking needs.

## Features
- **Real-Time GPS Tracking:** Using `TinyGPS++` for high-precision location tracking.
- **Motion Sensing:** Integrates `Adafruit_MPU6050` for motion detection and analysis.
- **Web Connectivity:** Manages network connections and web server functionalities.
- **Data Handling:** Efficient data storage and retrieval using `LittleFS` and `ArduinoJson`.

## Hardware Requirements
- Any Arduino-compatible board with WiFi capabilities (ESP32).
- `TinyGPS++` compatible GPS module.
- `Adafruit_MPU6050` accelerometer and gyroscope module.
- Necessary cables and power supply.

## Software Dependencies
- ArduinoJson
- WiFiManager
- HTTPClient
- LittleFS
- ESPmDNS
- [Other libraries as used in the sketch]

## Installation
1. Install Arduino IDE and necessary board packages.
2. Clone this repository or download the `Tracket_main.ino` file.
3. Install all the required libraries mentioned above.
4. Connect the hardware components as per the schematic provided (if available).
5. Upload the sketch to your board.

## Configuration
- WiFi credentials and other settings can be configured through the `WiFiManager` interface.
- Modify the constants and settings in the sketch to suit your specific application needs.

## Usage
Once set up, the system will start tracking GPS location and motion data. Access this data through the defined web server interface or as configured in the sketch.

## Contributing
Contributions to Tracket are welcome! Feel free to fork the repository, make your changes, and create a pull request.

## License
This project was a capstone project for university graduation, the project had a web app for control. A full presentation found [here](https://youtu.be/8SLUGXAe2Js).

## Acknowledgments
- Contributors and maintainers of the libraries used in this project.
- Thanks to [Jihad Abdl Ghani](https://github.com/jihad-A-G) for being a great partner in the project.
