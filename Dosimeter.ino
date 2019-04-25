/*   Title: STS-5 Arduino Dosimiter 
 *   Author: SeanTheITGuy (theitguysean@gmail.com)
 *   Date: 2019-04-24
 *   --
 *   Simple sketch to count hits from a STS05 GM tube, average over a short period, and report the current
 *   radiation dosage amounts to the OLED display.
*/

#include <Bounce2.h>
#include <U8glib.h>

#define TUBE 2                                 // Define required pins.
#define BUZZER 4
#define LED 7
#define BATT A0

// Screen Defines
#define D0 12
#define D1 11
#define CS 8
#define DC 9
#define RESET 10

// Intervals and levels
#define MIN_VOLTAGE 3.3                       // Sensible bounds for 18650 voltage level.
#define MAX_VOLTAGE 4.4
#define VOLT_CHECK_INTERVAL 10000             // rate at which to check battery voltage (ms)
#define UPDATE_INTERVAL 4000                  // rate at which stats are updated. (ms)
#define BOUNCE_INTERVAL 0                     // Debounce interval (ms)
#define BUZZER_PULSE_LENGTH 2                 // time to hold buzzer high for click (ms)
#define LED_PULSE_LENGTH 10                   // time to hold LED high on a hit (ms)

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

unsigned long int counts_per_minute = 0;      // record of CPM
float microsievert_per_hour = 0.0;            // record of uSv/h
unsigned long int hits = 0;                   // records of hit
unsigned long int last_hit = 0;               // micros of the last geiger hit
unsigned long int last_voltage_check = 0;     // Time at which voltage was last checked.
unsigned long int last_update = 0;            // time at which the stats were last updated.

unsigned int battery_percent = 0;

Bounce bouncer = Bounce();                  // Debouncer for tube input
U8GLIB_SSD1306_128X64 u8g(D0, D1, CS, DC, RESET);  // screen device

void setup() {
  DEBUG_START(115200);

  pinMode(TUBE, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(BATT, INPUT);
  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(CS, OUTPUT);
  pinMode(DC, OUTPUT);
  pinMode(RESET, OUTPUT);
  
  bouncer.attach(TUBE, INPUT_PULLUP);  
  bouncer.interval(BOUNCE_INTERVAL);

 // init display.
 u8g.firstPage();
 do { 
  u8g.setFont(u8g_font_unifont_76);
  u8g.setScale2x2();
  u8g.drawStr(24, 24, "B");
  u8g.undoScale();
 } while( u8g.nextPage() );
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

    float voltage = (analogRead(BATT) * 5.0) / 1024.0;

    DEBUG_PRINT("Voltage: ");
    DEBUG_PRINTLN(voltage);
    
    // Calculate percent from analog voltage reading
    battery_percent = ( ( voltage - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE) ) * 100;
    battery_percent = battery_percent > 100 ? 100 : (battery_percent < 0 ? 0 : battery_percent); // fix if out of bounds
    
    // Reset voltage check timer.
    last_voltage_check = millis();    

    DEBUG_PRINT(" Battery Percent: ");
    DEBUG_PRINTLN(battery_percent);
  }

  if (millis() > last_update + UPDATE_INTERVAL) {
    // calculate the CPM
    counts_per_minute = hits * (60000 / UPDATE_INTERVAL);  // convert hits per interval to hits per minute
    microsievert_per_hour = counts_per_minute / 120.0;    // uS/h = 120cpm if and only if tube is calibrated to Cs127, which functional STS-5 generally are.

    hits = 0;                     // Reset counters
    last_update = millis();

    // Update display.
    u8g.firstPage();
    do { 
      draw();
    } while( u8g.nextPage() );
    
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

// Draw the required data on screen.
void draw(void) {
  // Draw battery stats
  u8g.setFont(u8g_font_6x12);
  u8g.drawStr(73, 12, "BATT:");
  u8g.setPrintPos(103, 12);
  u8g.print(battery_percent);
  u8g.drawStr(121, 12, "%");

  u8g.setFont(u8g_font_profont22);
  u8g.drawStr(24, 36, "CPM:");
  u8g.drawStr(0, 54, "uSv/h:");
  u8g.setPrintPos(74, 36);
  u8g.print(counts_per_minute);
  u8g.setPrintPos(74, 54);
  u8g.print(microsievert_per_hour);  
}
