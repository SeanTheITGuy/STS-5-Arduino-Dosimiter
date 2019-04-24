/*   Title: STS-5 Arduino Dosimiter 
 *   Author: SeanTheITGuy (theitguysean@gmail.com)
 *   Date: 2019-04-24
 *   --
 *   Simple sketch to count hits from a STS05 GM tube, average over a short period, and report the current
 *   radiation dosage amounts to the OLED display.
*/

#include <Bounce2.h>
#include <ssd1306.h>

#define TUBE 8                                 // Define required pins.
#define LED 7
#define BUZZER 4
#define SCL 3
#define SDA 2
#define BATT A0

#define MIN_VOLTAGE 3.3                       // Sensible bounds for 18650 voltage level.
#define MAX_VOLTAGE 4.4
#define VOLT_CHECK_INTERVAL 10000             // rate at which to check battery voltage (ms)
#define UPDATE_INTERVAL 5000                  // rate at which stats are updated. (ms)
#define BOUNCE_INTERVAL 0                     // Debounce interval (ms)
#define BUZZER_PULSE_LENGTH 2                 // time to hold buzzer high for click (ms)
#define LED_PULSE_LENGTH 10                   // time to hold LED high on a hit (ms)

//#define DEBUG
#ifdef DEBUG
  #define DEBUG_START(x) Serial.begin(x); 
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_START(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)  
#endif

unsigned long int hits = 0;                   // records of hit
unsigned long int last_hit = 0;               // micros of the last geiger hit
unsigned long int last_voltage_check = 0;     // Time at which voltage was last checked.
unsigned long int last_update = 0;            // time at which the stats were last updated.

Bounce bouncer = Bounce();                  // Debouncer for tube input

void setup() {
  DEBUG_START(115200);

  pinMode(TUBE, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(SCL, OUTPUT);
  pinMode(SDA, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(BATT, INPUT);
  
  bouncer.attach(TUBE, INPUT_PULLUP);
  
  bouncer.interval(BOUNCE_INTERVAL);

  ssd1306_128x64_i2c_init();
  ssd1306_fillScreen(0x00);
  ssd1306_setFixedFont(ssd1306xled_font6x8);
  ssd1306_printFixed (0,  8, "BATT: ", STYLE_ITALIC);
  ssd1306_printFixed (0, 16, "CPM:  ", STYLE_BOLD);
  ssd1306_printFixed (0, 24, "µSv/h:", STYLE_BOLD);

  DEBUG_PRINTLN("Starting..");
}

void loop() {
  
  // Check if tube has pulled detector pin to ground (rad hit)
  bouncer.update();
  if(bouncer.fell()) {
    register_hit();
  } else {
    no_hit();
  }
  
  // Read voltage from 18650 pin
  if (millis() > last_voltage_check + VOLT_CHECK_INTERVAL) {
    
    // Calculate percent from analog voltage reading
    int battery_percent = round( ( ( (analogRead(BATT) * 5) / 1024 - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE) ) * 100 );
    battery_percent = battery_percent > 100 ? 100 : (battery_percent < 0 ? 0 : battery_percent); // fix if out of bounds

    // Convert percent int to string for printing on display
    char text[4];
    itoa(battery_percent, text, 10);
    ssd1306_setFixedFont(ssd1306xled_font6x8);
    ssd1306_printFixed (42, 8, text, STYLE_NORMAL); 

    // Reset voltage check timer.
    last_voltage_check = millis();    

    DEBUG_PRINT(" Battery Percent: ");
    DEBUG_PRINTLN(battery_percent);
  }

  if (millis() > last_update + UPDATE_INTERVAL) {
    // calculate the CPM
    unsigned long int counts_per_minute = hits * (60000 / UPDATE_INTERVAL);  // convert hits per interval to hits per minute
    float microsievert_per_hour = counts_per_minute / 120.0;    // uS/h = 120cpm if and only if tube is calibrated to Cs127, which functional STS-5 generally are.

    // Update display with new data
    char text[10];                                              // char buffer for float conversion
    
    ssd1306_setFixedFont(ssd1306xled_font6x8);

    itoa(counts_per_minute, text, 10);                         // CPM 
    ssd1306_printFixed(42, 16, text, STYLE_BOLD);

    dtostrf(microsievert_per_hour, 5, 3, text);                
    ssd1306_printFixed(42, 24, text, STYLE_BOLD);

    hits = 0;                     // Reset counters
    last_update = millis();
    
    DEBUG_PRINT("Refresh Display - CPM:");
    DEBUG_PRINT(counts_per_minute);
    DEBUG_PRINT(" uS/h: ");
    DEBUG_PRINTLN(microsievert_per_hour);
  }
} 

// Register a geiger tube pulse
void register_hit() {
  unsigned long int start = micros();
  hits++;
  digitalWrite(BUZZER, HIGH);  // Turn on the buzzer and LED to be turned off after defined pulse lengths.
  digitalWrite(LED, HIGH);
  last_hit = millis();
}

// Register no pulse on last geiger tube check
void no_hit() {
  unsigned long int now = millis();
  if(now > last_hit + BUZZER_PULSE_LENGTH) {  // turn off buzzer and LED if pulse length has elasped.
    digitalWrite(BUZZER, LOW);
  }
  if(now > last_hit + LED_PULSE_LENGTH) {
    digitalWrite(LED, LOW);
  }
}
