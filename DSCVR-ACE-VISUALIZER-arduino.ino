#include <WiFiS3.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include <XPT2046_Touchscreen.h>
#include <time.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUDP.h>
#include <yxml.h>
#include <TinyXML.h>
#include <vector>

// Pin definitions for UNO R4 WiFi
#define TFT_DC    9
#define TFT_CS    10
#define TFT_RST   8
#define TOUCH_CS  7
#define SD_CS     4

// Satellite positionsz
struct Position {
  float x;
  float y;
  float z;
  String time;
};

Position dscovrPos = {0, 0, 0, ""};
Position acePos = {0, 0, 0, ""};
Position dscovrPosLast = {0, 0, 0, ""};
Position acePosLast = {0, 0, 0, ""};

struct SatelliteData {
    float x;
    float y;
    String time;
};

struct DataSet {
    SatelliteData* data;
    int size;
};

DataSet aceData = {nullptr, 0};
DataSet dscovrData = {nullptr, 0};

// Initialize display using Arduino_GFX
Arduino_DataBus *bus = new Arduino_UNOPAR8();
Arduino_GFX *gfx = new Arduino_ILI9341(bus, A4 /* RST */, 3 /* rotation */, false /* IPS */);

// Initialize touch
XPT2046_Touchscreen touch(TOUCH_CS);

// Constants from original code (referenced from variables.js)
const long distanceToSun = 93000000;    // miles
const long radiusSun = 432690;          // sun radius in miles
const long distanceToL1 = 1000000;      // distance to L1 from Earth
const long l1 = 1600000;                // L1 distance in miles
const float speedOfLight = 3e8;         // m/s 
const float dscovrFrequencyMHz = 2215;
const float frequencyMHzACE = 2278.35;
const int minutesPerPoint = 12;         // SSCweb data resolution
const long millisPerMinute = 60 * 1000;
const int DAYS_OF_DATA = 7; 

const char* WIFI_SSID = "<SSID>";     // Replace with your WiFi SSID
const char* WIFI_PASSWORD = "<PASSWORD>";   // Replace with your WiFi password

// SEZ calculations (simplified for 2D)
const float sezHalfrad = tan(0.5 * PI / 180.0) * l1;  // SEZ 0.5 radius
const float sez2rad = tan(2.0 * PI / 180.0) * l1;     // SEZ 2 radius
const float sez4rad = tan(4.0 * PI / 180.0) * l1;     // SEZ 4 radius

// Additional SD card configuration
const char* WIFI_FILE = "/wifi.txt";
bool sdAvailable = false;

// Touch screen calibration for 2.8" display
// These values might need adjustment based on your specific display
#define TOUCH_MIN_X 150   // Adjusted from 300
#define TOUCH_MAX_X 3800
#define TOUCH_MIN_Y 150   // Adjusted from 200
#define TOUCH_MAX_Y 3800

// Screen dimensions for 2.8" display
#define SCREEN_WIDTH 240  
#define SCREEN_HEIGHT 320
#define EARTH_X (SCREEN_HEIGHT/2)
#define EARTH_Y (SCREEN_WIDTH/2)

std::vector<SatelliteData> acePoints;
std::vector<SatelliteData> dscovrPoints;

void drawSatellite(Position pos, uint16_t color, String name, int size, bool showCoords);
void drawVisualization();
void drawGrid();
void drawLegend();
void drawLoadingScreen(String message);
void drawSEZCircles();

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Replace the setupTime function with this version
void setupTime() {
    timeClient.begin();
    timeClient.setTimeOffset(0); // Use UTC time
    
    gfx->print("\nWaiting for NTP time sync: ");
    Serial.print("Waiting for NTP time sync: ");
    int attempts = 0;
    
    while (!timeClient.update() && attempts < 10) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (timeClient.isTimeSet()) {
        gfx->print("\nTime synchronized!");
        Serial.println("\nTime synchronized!");
        time_t epochTime = timeClient.getEpochTime();
        setTime(epochTime);
        
        time_t now = timeClient.getEpochTime();
        struct tm *timeinfo = gmtime(&now);
        char buffer[25];
        strftime(buffer, 25, "%Y-%m-%d %H:%M:%S", timeinfo);
        gfx->print("\nCurrent UTC time: ");
        gfx->print(buffer);
        Serial.print("Current UTC time: ");
        Serial.println(buffer);
    } else {
        Serial.println("\nFailed to sync time!");
    }
}

int scaleDistance(float realDistance) {
    // Adjust scale factor to use FULL screen width and height
    float scaleFactor = (SCREEN_WIDTH / 0.8) / distanceToL1;
    return (int)(realDistance * scaleFactor);
}

// Add these time-related functions
String zeroPad(int num) {
  if (num < 10) {
    return "0" + String(num);
  }
  return String(num);
}

String convertTime(time_t timestamp) {
  struct tm *timeinfo = gmtime(&timestamp);
  
  char buffer[20];
  sprintf(buffer, "%04d%02d%02dT%02d%02d%02dZ",
          timeinfo->tm_year + 1900,
          timeinfo->tm_mon + 1,
          timeinfo->tm_mday,
          timeinfo->tm_hour,
          timeinfo->tm_min,
          timeinfo->tm_sec);
          
  return String(buffer);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize SPI devices
  SPI.begin();
  
  // Initialize SD card
  // if (SD.begin(SD_CS)) {
  //   sdAvailable = true;
  //   loadWiFiCredentials();
  // }
  
  // Initialize display
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
    while (1);
  }
  
  // Initialize touch
  touch.begin();
  touch.setRotation(3);
  
  // if (!sdAvailable || !hasWiFiCredentials()) {
  //   setupWiFiInput();
  // } else {
  //   connectToWiFi();
  // }

   connectToWiFi();
   setupTime();
}

void drawKeyboard(const char keyboard[4][10], String currentInput, bool isSSID) {
  gfx->fillScreen(BLACK);
  
  // Draw header
  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);
  gfx->setCursor(10, 10);
  gfx->print(isSSID ? "Enter WiFi SSID:" : "Enter Password:");
  
  // Draw input field
  gfx->drawRect(10, 40, SCREEN_WIDTH-20, 30, WHITE);
  gfx->setCursor(15, 50);
  gfx->print(isSSID ? currentInput : String(currentInput.length(), '*'));
  
  // Draw keyboard
  int keyWidth = SCREEN_WIDTH / 10;
  int keyHeight = 40;
  int startY = 80;
  
  for(int row = 0; row < 4; row++) {
    for(int col = 0; col < 10; col++) {
      int x = col * keyWidth;
      int y = startY + row * keyHeight;
      
      gfx->drawRect(x, y, keyWidth, keyHeight, WHITE);
      gfx->setCursor(x + keyWidth/3, y + keyHeight/3);
      gfx->print(keyboard[row][col]);
    }
  }
  
  // Draw Next/Clear buttons
  gfx->drawRect(10, SCREEN_HEIGHT-40, 100, 30, WHITE);
  gfx->setCursor(30, SCREEN_HEIGHT-35);
  gfx->print(isSSID ? "Next" : "Done");
  
  gfx->drawRect(SCREEN_WIDTH-110, SCREEN_HEIGHT-40, 100, 30, WHITE);
  gfx->setCursor(SCREEN_WIDTH-90, SCREEN_HEIGHT-35);
  gfx->print("Clear");
}

void handleKeyPress(int x, int y, const char keyboard[4][10], String &ssid, String &password, bool &isSSID) {
  int keyWidth = SCREEN_WIDTH / 10;
  int keyHeight = 40;
  int startY = 80;
  
  // Check keyboard area
  if (y >= startY && y < startY + 4*keyHeight) {
    int row = (y - startY) / keyHeight;
    int col = x / keyWidth;
    
    if (row < 4 && col < 10) {
      String &currentInput = isSSID ? ssid : password;
      currentInput += keyboard[row][col];
    }
  }
  
  // Check buttons
  if (y >= SCREEN_HEIGHT-40 && y < SCREEN_HEIGHT-10) {
    if (x < 110) { // Next/Done button
      if (isSSID && ssid.length() > 0) {
        isSSID = false;
      }
    } else if (x >= SCREEN_WIDTH-110) { // Clear button
      String &currentInput = isSSID ? ssid : password;
      currentInput = "";
    }
  }
}


bool hasWiFiCredentials() {
  return sdAvailable && SD.exists(WIFI_FILE);
}

void loadWiFiCredentials(String ssid, String password) {
  if (sdAvailable) {
    File file = SD.open(WIFI_FILE, FILE_WRITE);
    if (file) {
      file.println(ssid);
      file.println(password);
      file.close();
    }
  }
}

void saveWiFiCredentials(String ssid, String password) {
  if (sdAvailable) {
    File file = SD.open(WIFI_FILE, FILE_WRITE);
    if (file) {
      file.println(ssid);
      file.println(password);
      file.close();
    }
  }
}

void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  drawLoadingScreen("Connecting to WiFi...");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    drawLoadingScreen("Connected! \n");
    Serial.println("Connected to WiFi");
    delay(1000);
  } else {
    drawLoadingScreen("Connection Failed! \n");
    Serial.println("Connection Failed");
    delay(2000);
  }
}

void setupWiFiInput() {
  static const char keyboard[4][10] = {
    {'1','2','3','4','5','6','7','8','9','0'},
    {'q','w','e','r','t','y','u','i','o','p'},
    {'a','s','d','f','g','h','j','k','l','.'},
    {'z','x','c','v','b','n','m','_','-','@'}
  };
  
  String inputSSID = "";
  String inputPassword = "";
  bool isEnteringSSID = true;
  
  while (true) {
    drawKeyboard(keyboard, isEnteringSSID ? inputSSID : inputPassword, isEnteringSSID);
    
    if (touch.touched()) {
      TS_Point p = touch.getPoint();
      
      // Convert touch coordinates
      int x = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
      int y = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);
      
      handleKeyPress(x, y, keyboard, inputSSID, inputPassword, isEnteringSSID);
      
      if (!isEnteringSSID && inputPassword.length() > 0) {
        saveWiFiCredentials(inputSSID, inputPassword);
        WiFi.begin(inputSSID.c_str(), inputPassword.c_str());
        return;
      }
      
      delay(250); // Debounce
    }
  }
}

void loop() {
  static unsigned long lastUpdateTime = 0;
  const unsigned long UPDATE_INTERVAL = 43200000;  // 12 hours in milliseconds
  
  unsigned long currentTime = millis();
  
  // Check if it's time for an update or if this is the first run
  if (lastUpdateTime == 0 || (currentTime - lastUpdateTime >= UPDATE_INTERVAL)) {
    Serial.println("\n=== Fetching new satellite data and updating display ===");
    
    // Fetch new data and immediately draw
    fetchSatelliteData();
    drawVisualization();
    
    // Update the last update time
    lastUpdateTime = currentTime;
    Serial.println("=== Update complete, waiting 12 hours for next update ===\n");
  }
  
  // Small delay to prevent busy-waiting
  delay(1000);
}

void xmlCallback(uint8_t statusflags, char* tagName, uint16_t taglen, char* data, uint16_t datalen) {
    static String currentSatellite;
    static bool inData = false;
    
    // Track if we have each coordinate for first positions
    static bool aceHasX = false;
    static bool aceHasY = false;
    static bool aceHasZ = false;
    static bool aceHasTime = false;
    static bool dscovrHasX = false;
    static bool dscovrHasY = false;
    static bool dscovrHasZ = false;
    static bool dscovrHasTime = false;
    
    // Get the simple tag name without path
    String tag = String(tagName);
    int lastSlash = tag.lastIndexOf('/');
    if (lastSlash != -1) {
        tag = tag.substring(lastSlash + 1);
    }
    
    // Check if we're entering or leaving a Data block
    if (tag == "Data") {
        if (statusflags & STATUS_START_TAG) {
            inData = true;
        } else if (statusflags & STATUS_END_TAG) {
            inData = false;
        }
        return;
    }
    
    // Only process if we have data and we're in a Data block
    if (inData && (statusflags & STATUS_TAG_TEXT) && data != nullptr) {
        String value = String(data);
        value.trim();
        
        if (value.length() > 0) {
            if (tag == "Id") {
                currentSatellite = value;
                currentSatellite.toLowerCase();
            } 
            else if (currentSatellite == "ace") {
                // Handle first position
                if (tag == "X" && !aceHasX) {
                    acePos.x = value.toFloat();
                    aceHasX = true;
                } 
                else if (tag == "Y" && !aceHasY) {
                    acePos.y = value.toFloat();
                    aceHasY = true;
                }
                else if (tag == "Z" && !aceHasZ) {
                    acePos.z = value.toFloat();
                    aceHasZ = true;
                }
                else if (tag == "Time" && !aceHasTime) {
                    acePos.time = value;
                    aceHasTime = true;
                }
                
                // Always update last position
                if (tag == "X") acePosLast.x = value.toFloat();
                else if (tag == "Y") acePosLast.y = value.toFloat();
                else if (tag == "Z") acePosLast.z = value.toFloat();
                else if (tag == "Time") acePosLast.time = value;
            }
            else if (currentSatellite == "dscovr") {
                // Handle first position
                if (tag == "X" && !dscovrHasX) {
                    dscovrPos.x = value.toFloat();
                    dscovrHasX = true;
                } 
                else if (tag == "Y" && !dscovrHasY) {
                    dscovrPos.y = value.toFloat();
                    dscovrHasY = true;
                }
                else if (tag == "Z" && !dscovrHasZ) {
                    dscovrPos.z = value.toFloat();
                    dscovrHasZ = true;
                }
                else if (tag == "Time" && !dscovrHasTime) {
                    dscovrPos.time = value;
                    dscovrHasTime = true;
                }
                
                // Always update last position
                if (tag == "X") dscovrPosLast.x = value.toFloat();
                else if (tag == "Y") dscovrPosLast.y = value.toFloat();
                else if (tag == "Z") dscovrPosLast.z = value.toFloat();
                else if (tag == "Time") dscovrPosLast.time = value;
            }
        }
    }
}

void parseSatelliteData(const char* xmlString) {
    Serial.println("\n=== First 200 chars of XML ===");
    for(int i = 0; i < 200 && xmlString[i]; i++) {
        Serial.print(xmlString[i]);
    }
    Serial.println("\n===========================\n");
    
    // Initialize XML parser
    uint8_t buffer[512]; // Increased buffer size
    TinyXML xml;
    xml.init(buffer, sizeof(buffer), xmlCallback);
    
    // Parse the XML string character by character
    Serial.println("Starting XML parsing...");
    for (size_t i = 0; xmlString[i]; i++) {
        xml.processChar(xmlString[i]);
    }
    
    // Print final positions
    Serial.println("\nFinal Positions:");
    Serial.print("ACE - X: "); Serial.print(acePos.x);
    Serial.print(" Y: "); Serial.println(acePos.y);
    Serial.print("Z: "); Serial.println(acePos.z);
    Serial.print("Time: "); Serial.println(acePos.time);
    Serial.print("DSCOVR - X: "); Serial.print(dscovrPos.x);
    Serial.print(" Y: "); Serial.println(dscovrPos.y);
    Serial.print("Z: "); Serial.println(dscovrPos.z);
    Serial.print("Time: "); Serial.println(dscovrPos.time);
}

void fetchSatelliteData() {
    Serial.println("\n=== Starting fetchSatelliteData ===");
    gfx->print("\nFetching satellite data...");
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected!");
        return;
    }

    WiFiClient client;
    if (!client.connect("sscweb.gsfc.nasa.gov", 80)) {
        Serial.println("Connection failed!");
        return;
    }
    Serial.println("Connected to server successfully");

    // Update time and create request
    timeClient.update();
    time_t now = timeClient.getEpochTime();
    time_t daysAgo = now - (DAYS_OF_DATA * 24 * 60 * 60);
    
    char startTime[20], endTime[20];
    struct tm *timeinfo;
    
    timeinfo = gmtime(&daysAgo);
    sprintf(startTime, "%04d%02d%02dT%02d%02d%02dZ",
            timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
    timeinfo = gmtime(&now);
    sprintf(endTime, "%04d%02d%02dT%02d%02d%02dZ",
            timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
    String path = "/WS/sscr/2/locations/ace,dscovr/" + String(startTime) + "," + String(endTime) + "/gse/";
    
    // Send request with increased timeout
    client.setTimeout(20000); // 20 second timeout
    client.print("GET " + path + " HTTP/1.1\r\n");
    client.print("Host: sscweb.gsfc.nasa.gov\r\n");
    client.print("Accept: application/xml\r\n");
    client.print("Connection: close\r\n\r\n");

    // Skip headers more efficiently
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) {
            break;
        }
    }

    // Parse the XML data
    gfx->print("\nParsing XML data...\n");
    Serial.println("Parsing satellite data...\n");
    
    // Process XML directly without storing complete response
    TinyXML xml;
    uint8_t buffer[1024]; // Increased buffer size
    xml.init(buffer, sizeof(buffer), xmlCallback);
    
    // Read and process the response in smaller chunks
    const int CHUNK_SIZE = 512;
    char chunk[CHUNK_SIZE];
    int bytesRead = 0;
    unsigned long timeout = millis() + 30000; // 30 second timeout
    
    while (client.connected() && millis() < timeout) {
        if (client.available()) {
            // Read a chunk of data
            bytesRead = client.read((uint8_t*)chunk, CHUNK_SIZE - 1);
            if (bytesRead > 0) {
                chunk[bytesRead] = 0; // Null terminate
                
                // Process each character through the XML parser
                for (int i = 0; i < bytesRead; i++) {
                    xml.processChar(chunk[i]);
                }
            }
        } else {
            delay(10); // Small delay when waiting for data
        }
    }

    client.stop();
    
    // Print final positions for verification
    Serial.println("\nFinal Positions:");
    Serial.print("ACE - X: "); Serial.print(acePos.x);
    Serial.print(" Y: "); Serial.println(acePos.y);
    Serial.print("Z: "); Serial.println(acePos.z);
    Serial.print("Time: "); Serial.println(acePos.time);
    Serial.print("DSCOVR - X: "); Serial.print(dscovrPos.x);
    Serial.print(" Y: "); Serial.println(dscovrPos.y);
    Serial.print("Z: "); Serial.println(dscovrPos.z);
    Serial.print("Time: "); Serial.println(dscovrPos.time);
}


// Add this helper function to check for hex chunk sizes
bool isHexadecimalChunkSize(String line) {
    line.trim();
    if (line.length() == 0) return false;
    
    // Check if the line contains only hexadecimal characters
    for (unsigned int i = 0; i < line.length(); i++) {
        char c = line.charAt(i);
        if (!isHexadecimalDigit(c)) {
            return false;
        }
    }
    return true;
}

bool isHexadecimalDigit(char c) {
    return (c >= '0' && c <= '9') || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F');
}

void updateSatellitePosition(Position& pos, float x, float y) {
    pos.x = x;
    pos.y = y;
}

void parseAndUpdatePositions(String payload) {
    gfx->print("Parsing XML data...\n");
    Serial.println("Parsing XML data...\n");
    
    if(payload.length() == 0) {
        Serial.println("Empty payload received");
        return;
    }

    unsigned long parseStartTime = millis();
    const unsigned long PARSE_TIMEOUT = 20000; // 20 seconds timeout

    // Find first Data section
    int dataStart = payload.indexOf("<Data>");
    while(dataStart != -1) {
        if(millis() - parseStartTime > PARSE_TIMEOUT) {
            Serial.println("XML parsing timeout");
            return;
        }

        int dataEnd = payload.indexOf("</Data>", dataStart);
        if(dataEnd == -1) break;

        // Extract this Data section
        String dataSection = payload.substring(dataStart, dataEnd + 7);

        // Get satellite ID
        int idStart = dataSection.indexOf("<Id>");
        int idEnd = dataSection.indexOf("</Id>");
        if(idStart != -1 && idEnd != -1) {
            String satelliteId = dataSection.substring(idStart + 4, idEnd);
            satelliteId.toLowerCase();

            // Get first X coordinate
            int xStart = dataSection.indexOf("<X>");
            int xEnd = dataSection.indexOf("</X>");
            
            // Get first Y coordinate
            int yStart = dataSection.indexOf("<Y>");
            int yEnd = dataSection.indexOf("</Y>");

            if(xStart != -1 && xEnd != -1 && yStart != -1 && yEnd != -1) {
                float x = dataSection.substring(xStart + 3, xEnd).toFloat();
                float y = dataSection.substring(yStart + 3, yEnd).toFloat();

                // Update appropriate satellite position
                if(satelliteId == "dscovr") {
                    dscovrPos.x = x;
                    dscovrPos.y = y;
                    Serial.println("Updated DSCOVR position:");
                    Serial.print("X: "); Serial.println(dscovrPos.x);
                    Serial.print("Y: "); Serial.println(dscovrPos.y);
                }
                else if(satelliteId == "ace") {
                    acePos.x = x;
                    acePos.y = y;
                    Serial.println("Updated ACE position:");
                    Serial.print("X: "); Serial.println(acePos.x);
                    Serial.print("Y: "); Serial.println(acePos.y);
                }
            }
        }

        // Look for next Data section
        dataStart = payload.indexOf("<Data>", dataEnd);
    }
}

void drawVisualization() {
    gfx->fillScreen(BLACK);
    
    // Draw timestamps at the top - centered
    gfx->setTextSize(1);
    
    // Calculate text widths for centering
    // String dscovrText = dscovrPosLast.time;
    String aceText = acePosLast.time;
    
    // Each character is approximately 6 pixels wide in text size 1
    // int dscovrWidth = dscovrText.length() * 6;
    int aceWidth = aceText.length() * 6;
    
    // Center the timestamps
    // int dscovrX = (SCREEN_HEIGHT - dscovrWidth) / 2;
    int aceX = (SCREEN_HEIGHT - aceWidth) / 2;
    
    // DSCOVR timestamp
    // gfx->setTextColor(BLUE);
    // gfx->setCursor(dscovrX, 5);
    // gfx->print(dscovrText);
    
    // ACE timestamp
    // gfx->setTextColor(GREEN);
    // gfx->setCursor(aceX, 15);
    // gfx->print(aceText);
    
    drawGrid();
    drawSEZCircles();
    
    // Draw historical positions (smaller dots)
    drawSatellite(dscovrPos, BLUE, "DSCOVR", 2, false);
    drawSatellite(acePos, GREEN, "ACE", 2, false);
    
    // Draw current positions (larger dots with coordinates)
    drawSatellite(dscovrPosLast, BLUE, "DSCOVR", 4, true);
    drawSatellite(acePosLast, GREEN, "ACE", 4, true);
    
    drawLegend();
}

void drawSEZCircles() {
  // Calculate SEZ radii
  int sez2 = scaleDistance(sez2rad);
  int sez4 = scaleDistance(sez4rad);
  
  // Draw circles centered at L1
  gfx->drawCircle(EARTH_X, EARTH_Y, sez2, RED);    // 2.0 degree circle
  gfx->drawCircle(EARTH_X, EARTH_Y, sez4, ORANGE); // 4.0 degree circle
}

void drawSatellite(Position pos, uint16_t color, String name, int size, bool showCoords) {
  // Use GSE Y for screen X and GSE Z for screen Y
  int screenX = EARTH_X + scaleDistance(pos.y);
  int screenY = EARTH_Y - scaleDistance(pos.z);
  
  // Draw the satellite point
  gfx->fillCircle(screenX, screenY, size, color);
  gfx->setTextColor(color);
  gfx->setTextSize(1);
  
  // Only show name and coordinates for current position (larger dots)
  if (showCoords) {
    // Draw satellite name
    gfx->setCursor(screenX + 5, screenY - 5);
    gfx->print(name);
    
    int textX = screenX + 5;
    int textY = screenY + 5;
    
    gfx->setCursor(textX, textY);
    gfx->print("X: ");
    gfx->print(int(pos.x));
    
    gfx->setCursor(textX, textY + 10);
    gfx->print("Y: ");
    gfx->print(int(pos.y));
    
    gfx->setCursor(textX, textY + 20);
    gfx->print("Z: ");
    gfx->print(int(pos.z));
  }
}

void drawGrid() {
  // Draw axis lines
  gfx->drawFastHLine(0, EARTH_Y, SCREEN_HEIGHT, DARKGREY);
  gfx->drawFastVLine(EARTH_X, 0, SCREEN_WIDTH, DARKGREY);
  
  // Calculate marker spacing based on screen dimensions
  float scaleFactor = (SCREEN_WIDTH / 0.8) / distanceToL1;  // Same as in scaleDistance()
  int markerSpacing = 100000;  // 100k mile increments
  
  // Draw Y-axis distance markers (horizontal axis)
  for (int i = -300000; i <= 300000; i += markerSpacing) {
    // Skip the 0 point
    if (i == 0) continue;
    
    int x = EARTH_X + (int)(i * scaleFactor);
    
    if (x >= 0 && x < SCREEN_HEIGHT) {
      // Draw tick mark
      gfx->drawFastVLine(x, SCREEN_WIDTH-5, 5, DARKGREY);
      
      // Draw label
      gfx->setTextColor(WHITE);
      gfx->setTextSize(1);
      gfx->setCursor(x-10, SCREEN_WIDTH-15);
      gfx->print(i/1000);
      gfx->print("k");
    }
  }
  
  // Draw Z-axis distance markers (vertical axis)
  for (int i = -300000; i <= 300000; i += markerSpacing) {
    // Skip the 0 point
    if (i == 0) continue;
    
    int y = EARTH_Y - (int)(i * scaleFactor);  // Subtract because Y increases downward
    
    if (y >= 0 && y < SCREEN_WIDTH) {
      // Draw tick mark
      gfx->drawFastHLine(0, y, 5, DARKGREY);
      
      // Draw label
      gfx->setTextColor(WHITE);
      gfx->setTextSize(1);
      gfx->setCursor(6, y-4);
      gfx->print(i/1000);
      gfx->print("k");
    }
  }
  
  // Add axis labels
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(5, 5);
  gfx->print("GSE Z");
  gfx->setCursor(SCREEN_WIDTH - 5, SCREEN_WIDTH - 25);
  gfx->print("GSE Y");
}

void drawLegend() {
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  
  // Title at top
  // gfx->setCursor(5, 5);
  // gfx->print("DSCOVR/ACE Orbit Visualizer");
  
  // Scale at bottom
  gfx->setCursor(5, SCREEN_HEIGHT - 20);
  gfx->print("GSE X-axis (miles)");
}

void drawLoadingScreen(String message) {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(5, 5);
  gfx->print(message);
}