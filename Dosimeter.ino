/*   Title: STS-5 Arduino Dosimiter
     Author: SeanTheITGuy (theitguysean@gmail.com)
     Date: 2019-04-24
     --
     Simple sketch to count hits from a STS05 GM tube, average over a short period, and report the current
     radiation dosage amounts to the OLED display.
*/

#include <SPI.h>               // include SPI library
#include <Adafruit_GFX.h>      // include adafruit graphics library
#include <Adafruit_PCD8544.h>  // include adafruit PCD8544 (Nokia 5110) library
#include <ESP8266WiFi.h>
#include <ThingSpeak.h>
#include "secrets.h"

#define BATT A0
#define LED D8
#define PLUG D6
#define TUBE D7                             
#define BUZZER D9

// Screen pins
#define SCREEN_RST D0
#define SCREEN_CE D1
#define SCREEN_DC D2
#define SCREEN_DIN D3
#define SCREEN_CLK D4
#define BACKLIGHT D5

// Intervals and levels
#define MIN_VOLTAGE 2.5                       // Sensible bounds for 18650 voltage level.
#define MAX_VOLTAGE 4.2
#define VOLT_CHECK_INTERVAL 10000             // rate at which to check battery voltage (ms)
#define BUZZER_PULSE_LENGTH 2                 // time to hold buzzer high for click (ms)
#define LED_PULSE_LENGTH 50                  // time to hold LED high on a hit (ms)
#define CONNECT_TIMEOUT 10000

#define DEBUG
#ifdef DEBUG
#define DEBUG_START(x) Serial.begin(x);
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_START(x)
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

Adafruit_PCD8544 display = Adafruit_PCD8544(SCREEN_CLK, SCREEN_DIN, SCREEN_DC, SCREEN_CE, SCREEN_RST);

const unsigned char logo [] PROGMEM = {
  // '1, 84x48px
  0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 
  0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xf0, 0x03, 0xf0, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x07, 0x80, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
  0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x07, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0xc0, 0x07, 0xf8, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x3f, 0xff, 0x00, 0x60, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0xf8, 0x07, 0xc0, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x07, 0x01, 0xe0, 0x01, 0xe0, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01, 0x80, 0x00, 0x60, 
  0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0xc0, 0x00, 0xc0, 0x0c, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x1c, 0x00, 0xe0, 0x01, 0xc0, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x70, 0x03, 
  0x80, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x33, 0xf3, 0x00, 0x06, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x30, 0x00, 0x3f, 0xff, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x1c, 
  0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x60, 0x00, 0x03, 0xf0, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 
  0x07, 0xf8, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x60, 0x70, 0x06, 0x18, 0xff, 0xc1, 0x80, 
  0x00, 0x00, 0x00, 0x00, 0x60, 0xff, 0xcc, 0x0c, 0xff, 0xc1, 0x80, 0x00, 0x00, 0x00, 0x00, 0x60, 
  0xef, 0xcc, 0x0c, 0xc0, 0xc1, 0x80, 0x00, 0x00, 0x00, 0x00, 0x60, 0xc0, 0xcc, 0x0c, 0xc0, 0xc1, 
  0x80, 0x00, 0x00, 0x00, 0x00, 0x60, 0xc0, 0xce, 0x1c, 0xc0, 0xc1, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x60, 0x60, 0x67, 0xf9, 0x81, 0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x63, 0xf1, 0x81, 
  0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x30, 0xc3, 0x01, 0x81, 0x80, 0x00, 0x00, 0x00, 
  0x00, 0x30, 0x70, 0x3c, 0x0f, 0x03, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x1e, 0x1c, 
  0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x38, 0x06, 0x1c, 0x07, 0x03, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x18, 0x18, 0x0c, 0x0c, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x0c, 0x0c, 
  0x06, 0x0c, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x0e, 0x1c, 0x06, 0x1c, 0x0e, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x0c, 0x07, 0x18, 0x03, 0x38, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x03, 
  0xf8, 0x03, 0xf0, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0xf0, 0x01, 0xc0, 0x38, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x30, 0x01, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 
  0x80, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0xc0, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x38, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x1e, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x80, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x03, 0xf0, 0x03, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00
};

float update_interval = 5000.0;                 // (ms) Interval that device samples over between refreshes of screen or other data destinations.

unsigned long int hits = 0;                   // records of hit
unsigned long int last_hit = 0;               // micros of the last geiger hit
unsigned long int last_voltage_check = -1*VOLT_CHECK_INTERVAL;     // Time at which voltage was last checked. (set to a negative so battery check is performed immediately).
unsigned long int last_update = 0;            // time at which the stats were last updated.
int battery_percent = 0;
int plug_state = 0;                         // keep track of if device is plugged in or not.

// Set up Wifi junk for Thingspeak recording of data.
unsigned long myChannelNumber = CH_ID;
const char * myWriteAPIKey = WRITE_KEY;
const char* ssid = MY_SSID;
const char* password = MY_PSK;
WiFiClient  client;

// Register a geiger tube pulse
void ICACHE_RAM_ATTR register_hit() {
  hits++;
  digitalWrite(BUZZER, LOW);
  digitalWrite(LED, LOW);
  if(!plug_state) // Only buzz if not plugged in.
  {
      digitalWrite(BUZZER, HIGH);  // Turn on the buzzer and LED to be turned off after defined pulse lengths.
  }
  digitalWrite(LED, HIGH);
  last_hit = millis();    
}

void setup() {
  DEBUG_START(115200);

  // Start the LCD display
  display.begin();
  display.setContrast(50);

  display.clearDisplay();
  display.drawBitmap(0,0,logo,84,48,BLACK);
  display.display();        // Display the bitmap set above
  delay(2000);
  display.clearDisplay();   

  // Set output devices to off
  digitalWrite(LED, LOW);
  digitalWrite(BUZZER, LOW);
    
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(BATT, INPUT);
  pinMode(TUBE, INPUT_PULLUP);
  pinMode(PLUG, INPUT);  
  pinMode(BACKLIGHT, OUTPUT);
  pinMode(SCREEN_CE, OUTPUT);
  pinMode(SCREEN_DC, OUTPUT);
  pinMode(SCREEN_DIN, OUTPUT);
  pinMode(SCREEN_CLK, OUTPUT);
  pinMode(SCREEN_RST, OUTPUT);

  ThingSpeak.begin(client);  // Initialize ThingSpeak

  // Attach interrupt to tube pin to register radiation hits.
  attachInterrupt(digitalPinToInterrupt(TUBE), register_hit, RISING);
}
    
void loop() {  
  plug_state = digitalRead(PLUG);
  
  digitalWrite(BACKLIGHT, !plug_state); // Turn backlight off if device is plugged in to charge.
  update_interval = plug_state ? 300000.0 : 5000.0;  // Update data destinations every 5 seconds on battery, or 5 minutes when plugged in.
  
  // Ensure buzzer and LED only stay on for defined intervals
  if (millis() > last_hit + BUZZER_PULSE_LENGTH) { 
    digitalWrite(BUZZER, LOW);
  }
  if (millis() > last_hit + LED_PULSE_LENGTH) {
    digitalWrite(LED, LOW);
  }
  
  // Read voltage from 18650 pin
  if (millis() > last_voltage_check + VOLT_CHECK_INTERVAL) {

    float voltage = analogRead(BATT) * 2.0 * 3.3 / 1023.0 * 0.945; // Read vbat through 50% voltage divider then convert for 3.3v analog range (and correct a weird offset by refactoring to 94.5%)

    DEBUG_PRINT("Voltage: ");
    DEBUG_PRINTLN(voltage);

    // Calculate percent from analog voltage reading
    battery_percent = ( ( voltage - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE) ) * 100;
    battery_percent = battery_percent > 100 ? 100 : (battery_percent < 0 ? 0 : battery_percent); // fix if out of bounds

    // Reset voltage check timer.
    last_voltage_check = millis();   
  }

  if (millis() > last_update + update_interval) {
    // calculate units
    float counts_per_minute = hits * (60000.0 / update_interval);  // convert hits per interval to hits per minute
    float microsievert_per_hour = counts_per_minute * 0.0057;    // Calibrated for STS-05/STM-20 tubes.
    float microroentgen_per_hour = microsievert_per_hour * 0.114;

    hits = 0;                     // Reset counters
    last_update = millis();

    DEBUG_PRINT(" Battery Percent: ");
    DEBUG_PRINT(battery_percent);
    DEBUG_PRINT(" Plug State: ");
    DEBUG_PRINT(digitalRead(PLUG));
    DEBUG_PRINT(" CPM:");
    DEBUG_PRINT(counts_per_minute);
    DEBUG_PRINT(" uS/h: ");
    DEBUG_PRINT(microsievert_per_hour);
    DEBUG_PRINT(" uR/h: ");
    DEBUG_PRINTLN(microroentgen_per_hour);    

    // Clear screen and prep for new text
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE);
    
    // Display stats on screen
    display.setCursor(0,10);
    display.print("CPM:   ");
    display.println(counts_per_minute);
    display.print("uSv/h: ");
    display.println(microsievert_per_hour);
    display.print("uR/h:  ");
    display.println(microroentgen_per_hour);

    // Update battery info on screen
    display.setCursor(30,40);
    display.print("batt:");
    display.print(battery_percent);
    display.println("%");
    display.display();   

    // If we are plugged in, connect to wifi and send data to thingspeak
    if(plug_state) 
    {
      // Reconnect to Wifi, if required
      if(WiFi.status() != WL_CONNECTED)
      {
        DEBUG_PRINT("Attempting to connect to SSID: ");
        DEBUG_PRINTLN(ssid);
        long int beginConnect = millis();
        while (WiFi.status() != WL_CONNECTED && millis() < beginConnect + CONNECT_TIMEOUT) 
        {
          WiFi.begin(ssid, password);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
          DEBUG_PRINT(".");
          delay(500);     
        }
      }

      // Send data to Thingspeak.
      ThingSpeak.setField(1, counts_per_minute);
      ThingSpeak.setStatus("OK");
      int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
      if(x == 200) 
      {
        DEBUG_PRINTLN("Channel update successful.");
      } else 
      {
        DEBUG_PRINTLN("Problem updating channel. HTTP error code " + String(x));
      }
    }
  }
}
