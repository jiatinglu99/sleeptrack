/* Author of Code: Jiating Lu, terrylu@ucs.edu
 *  Group Members: Efaz Muhaimen, eem_680@usc.edu
 * This code is for IdeaHacks 2020 Makeathon Group 8's SleepTrak project,
 * which is a device that tracks one's sleep and finds the best environmental 
 * conditions that lead to best sleeping quality.
 */

#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>   // OLED Display
#include <SparkFun_APDS9960.h>  // Proximity Sensor
#include "Adafruit_Si7021.h"    // Humidity and Temperature Sensor

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool display_on = true;
int on_count = 0;
int total_count = 0;

// Proximity Sensor SparkFun_APDS9960
// Global variables
SparkFun_APDS9960 apds = SparkFun_APDS9960();
uint8_t proximity_data = 0;
bool tapped = false;
bool ptapped = false;

// Color Sensor SparkFun_APDS9960
// Global Variables
uint16_t ambient_light = 0;
uint16_t red_light = 0;
uint16_t green_light = 0;
uint16_t blue_light = 0;

// Humidity and Temperature Sensor Adafruit_Si7021
Adafruit_Si7021 sensor = Adafruit_Si7021();
float humidity = 0.0;
float temperature = 0.0;

// Motion Sensor Grove_PIR_Motion_Sensor
#define pirPin 3
bool inMotion = false;
float noise_level = 0.0;
int restlessness = 0;
bool lastInMotion = false;
bool paused = false;

struct Package{
  int humidity;
  int temperature; 
  int ambient_light;
  int noise_level;
  int restlessness;
};

Package history;

void proximityLightSensorSetup(){
  // Initialize APDS-9960 (configure I2C and initial values)
  apds.init();
  
  // Adjust the Proximity sensor gain
  apds.setProximityGain(PGAIN_2X);
  
  // Start running the APDS-9960 proximity sensor (interrupts)
  if ( apds.enableProximitySensor(false) ) {
    Serial.println(F("Proximity Sensor is now running"));
  } else {
    Serial.println(F("Proximity Sensor FAILED!"));
  }

  // Start running the APDS-9960 light sensor (no interrupts)
  if ( apds.enableLightSensor(false) ) {
    Serial.println(F("Light Sensor is now running"));
  } else {
    Serial.println(F("Light Sensor FAILED!"));
  }
}

void humTempSensorSetup(){
  if (sensor.begin()) {
    Serial.println("Humidity and Temperature Sensor is now running");
  } else {
    Serial.println("Humidity and Temperature Sensor FAILED!");
  }
}


// Immediate Polling
void proximitySensorLoop(){
  apds.readProximity(proximity_data);
  if (proximity_data >= 100){
    // when tapped
    if (!ptapped){
      tapped = true;
      action();
    }
  }
  else{
    tapped = false;
  }
  ptapped = tapped;
}

// Periodic Update
void lightSensorCollect(){
  // Read the light levels (ambient, red, green, blue)
  if (  !apds.readAmbientLight(ambient_light) ||
        !apds.readRedLight(red_light) ||
        !apds.readGreenLight(green_light) ||
        !apds.readBlueLight(blue_light) ) {
    Serial.println("Error reading light values");
  }
}

void humTempSensorCollect(){
  humidity = sensor.readHumidity();
  temperature = sensor.readTemperature();
}

void noiseSensorCollect(){
  long sum = 0;
  for(int i=0; i<32; i++)
  {
    sum += analogRead(1);
  }
  sum >>= 5;
  noise_level = float(sum)/100;
}

void motionSensorSetup(){
  pinMode(pirPin, INPUT);   // declare motion sensor as input
  Serial.println("Motion Sensor is now running!");
}

void motionSensorLoop(){
  inMotion = digitalRead(pirPin);
  if (lastInMotion == 0 && inMotion == 1 && !paused) restlessness++; 
  lastInMotion = inMotion;
}

// For interfacing the device
void polling(){
  proximitySensorLoop();
  motionSensorLoop();
}

// For data to be recorded
void dataCollect(){
  lightSensorCollect();
  humTempSensorCollect();
  noiseSensorCollect();
}

void compareResult(){
  if (history.restlessness > restlessness){
    history.humidity = int(humidity);
    history.temperature = int(temperature); 
    history.ambient_light = int(ambient_light);
    history.noise_level = int(noise_level);
    history.restlessness = int(restlessness);
  }
  EEPROM.put(0, history);
}

// Action for tapping the gesture sensor 
void action(){
  if (display_on){
    // if display is on, pause or resume
    on_count = 0;
    paused = !paused;
  }
  else{
    // turn on display
    on_count = 0;
    display_on = true;
    display.ssd1306_command(SSD1306_DISPLAYON); // To switch display back on
  }
}

void displaySetup(){
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();// internal buffer image
  delay(500);
  // Clear the buffer
  display.clearDisplay();

}

void displayData(){
  display.clearDisplay();
  display.setTextSize (1);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  //display.cp437(true);         // Use full 256 char 'Code Page 437' font

  display.println(F(" Current Environment"));
  display.drawLine(0, 8, 128, 8, SSD1306_WHITE);
  //display.drawLine(0, 0, 128, 0, SSD1306_WHITE);
  display.setCursor(0, 10);

  // Humidity and Temperature
  display.print(F("Hum: "));
  display.print(humidity, 1);
  display.print(F("% Temp:"));
  display.print(temperature, 1);
  display.println(F("C"));
  display.print(F("Lux: "));
  display.print(ambient_light);
  display.print(F("   "));
  
  display.print(F("Noise: "));
  display.print(noise_level, 1);
  
  for (int i = 26; i <= 36; i++)
    display.drawLine(0, i, 128, i, SSD1306_WHITE);
  display.setCursor(0, 28);
  display.setTextColor(SSD1306_BLACK);
  display.print(F("   MOTION: "));
  if (inMotion) display.print(F("DETECTED"));
  else display.print(F("  NONE"));
  //display.drawLine(0, 36, 128, 36, SSD1306_WHITE);

  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 56);     // Start at top-left corner

  display.println(F(" Ideal   Environment"));
  display.drawLine(0, 54, 128, 54, SSD1306_WHITE);
  display.setCursor(0, 10);
  
  // Ideal Humidity and Temperature
  display.setCursor(0, 38);
  display.print(F("Hum: "));
  display.print(history.humidity);
  display.print(F("%   Temp:"));
  display.print(history.temperature);
  display.println(F(" C"));
  display.print(F("Lux: "));
  display.print(history.ambient_light);
  display.print(F("   "));
  
  display.print(F("  Noise: "));
  display.print(history.noise_level);

  if (paused){
    display.fillRect(15, 20, 98, 24, SSD1306_BLACK);
    display.setTextSize (2);
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(30,24);
    display.print(F("PAUSED"));
  }
  display.display();
}

void setup() {
  EEPROM.get(0, history);
  Serial.begin(9600);
  proximityLightSensorSetup();
  humTempSensorSetup();
  motionSensorSetup();
  displaySetup();
  
  // Update sensor values before tracking
  dataCollect();
  displayData();
}

void loop() {
  // Immediate Polling
  polling();
  if (total_count%20 == 0 && !paused) { // once a second
    dataCollect();
  }
  if (total_count%5 == 0) {
    if (display_on) displayData();
  }

  if (total_count >= 12000){ // once 10 minutes
    total_count = 0;
    compareResult();
  }

  total_count++;
  if (display_on) {
    on_count++;
    if (on_count >= 500){// after 30 seconds
      // Shleep mode
      display.ssd1306_command(SSD1306_DISPLAYOFF); // To switch display off
      display_on = false;
    }
  }
  //Serial.println(on_count);
  delay(50);// 0.05 second
}
