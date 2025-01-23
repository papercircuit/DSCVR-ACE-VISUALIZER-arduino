# DSCOVR-ACE Orbit Visualizer

An Arduino-based visualization tool for tracking and displaying the positions of DSCOVR (Deep Space Climate Observatory) and ACE (Advanced Composition Explorer) satellites in real-time. This project uses an Arduino UNO R4 WiFi board with a 2.8" TFT touchscreen display to show the satellites' positions in GSE (Geocentric Solar Ecliptic) coordinates. This is a port of the original [DSCVR-ACE-VISUALIZER](https://github.com/papercircuit/DSCVR-ACE-VISUALIZER) project to the Arduino UNO R4 WiFi board. 

https://dscovr-ace-visualizer.vercel.app/

## Features

- Real-time visualization of DSCOVR and ACE satellite positions
- Automatic data updates every 12 hours from NASA's SSCWeb
- Interactive 2.8" TFT touchscreen display
- GSE coordinate system visualization with grid
- SEZ (Sun-Earth-Zero) angle indicators (2° and 4° circles)
- WiFi connectivity for data retrieval
- NTP time synchronization
- Detailed satellite position information display

## Hardware Requirements

- Arduino UNO R4 WiFi board
- 2.8" TFT Display with ILI9341 controller (I used https://www.amazon.com/dp/B01EUVJYME?ref=ppx_yo2ov_dt_b_fed_asin_title)
- microSD card module (optional)

### Pin Connections
TFT Display:
DC: Pin 9
CS: Pin 10
RST: Pin 8
Touch Controller:
CS: Pin 7
SD Card (optional):
CS: Pin 4

## Software Dependencies

The following libraries are required:

- WiFiS3
- ArduinoHttpClient
- ArduinoJson
- SD (optional)
- Arduino_GFX_Library
- XPT2046_Touchscreen
- TimeLib
- NTPClient
- TinyXML
- vector (C++ STL)

## Installation

1. Install the Arduino IDE
2. Install all required libraries through the Arduino Library Manager
3. Clone this repository:

```bash
git clone https://github.com/papercircuit/DSCOVR-ACE-Visualizer-arduino.git
```
4. Open the `.ino` file in Arduino IDE
5. Update the WiFi credentials:

```cpp
const char* WIFI_SSID = "<Your_SSID>";
const char* WIFI_PASSWORD = "<Your_Password>";
```

6. Upload the sketch to your Arduino UNO R4 WiFi

## Usage

1. Power on the device
2. The display will show a loading screen while connecting to WiFi
3. Once connected, it will fetch initial satellite position data
4. The visualization will update automatically every 12 hours
5. The display shows:
   - Current positions of both satellites
   - GSE coordinate grid
   - SEZ angle indicators
   - Satellite coordinates in miles

## Display Information

- Blue dot: DSCOVR satellite
- Green dot: ACE satellite
- Red circle: 2° SEZ angle
- Orange circle: 4° SEZ angle
- Grid: GSE Y and Z coordinates in thousands of miles
- Current satellite positions shown with coordinates

## Data Source

Satellite position data is retrieved from NASA's SSCWeb service:
`sscweb.gsfc.nasa.gov`

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- NASA SSCWeb for providing satellite position data
- Arduino community for library support
- Original concept inspired by NASA's space weather monitoring systems

## Notes

- The visualization updates every 12 hours to avoid overwhelming the NASA SSCWeb service
- WiFi credentials can be configured via the touchscreen interface if SD card storage is enabled
- All positions are displayed in GSE (Geocentric Solar Ecliptic) coordinates
- Distances are in miles from Earth's center

## Troubleshooting

- If the display shows "Connection Failed", check your WiFi credentials or press the reset button on the Arduino
- If satellite positions aren't updating, verify your internet connection