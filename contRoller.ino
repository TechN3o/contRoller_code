/* logo
 __       __  __                                __                 
/  \     /  |/  |                              /  |                
$$  \   /$$ |$$/  _______    ______   _______  $$ |   __  __    __ 
$$$  \ /$$$ |/  |/       \  /      \ /       \ $$ |  /  |/  |  /  |
$$$$  /$$$$ |$$ |$$$$$$$  |/$$$$$$  |$$$$$$$  |$$ |_/$$/ $$ |  $$ |
$$ $$ $$/$$ |$$ |$$ |  $$ |$$ |  $$ |$$ |  $$ |$$   $$<  $$ |  $$ |
$$ |$$$/ $$ |$$ |$$ |  $$ |$$ \__$$ |$$ |  $$ |$$$$$$  \ $$ \__$$ |
$$ | $/  $$ |$$ |$$ |  $$ |$$    $$/ $$ |  $$ |$$ | $$  |$$    $$ |
$$/      $$/ $$/ $$/   $$/  $$$$$$/  $$/   $$/ $$/   $$/  $$$$$$$ |
                                                         /  \__$$ |
                                                         $$    $$/ 
                                                          $$$$$$/  


*/
#include <Arduino.h>
#include <Wire.h>  // ahoj 11
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SmallTools.h>  //SmallTools is my own library, that im rn developing
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_PCF8574.h>
#include <Timer.h>
#include "PCF8574.h"

WiFiUDP ntpUDP;
Adafruit_SSD1306 display;
SmallTools st;
NTPClient TC(ntpUDP, "europe.pool.ntp.org", 1 * 3600, 10 * 3600);  // two hours offset = 2 * 3600, updating every 10 hours
PCF8574 pcf(0x20);

// #include "PinDefinitionsAndMore.h"
// #include <IRremote.hpp>  // include the library

#undef IR_RECEIVE_PIN
#define IR_RECEIVE_PIN 7
#define IS_DISCONNECTED WiFi.status() != WL_CONNECTED
#define S_ESP 0  // Source_ESP for led class
#define S_PCF 1  // Source_PCF for led class
const char *ssid = "seminare";
const char *password = "seminare";
int16_t minuteOffset = 0;
typedef void (*functionPointer)();


class led {
public:
  uint8_t pin;
  bool state = false;
  uint8_t source = 0;
  led(uint8_t in, uint8_t s) {  // in: pin; s: source
    pin = in;
    source = s;
    if (sourceIs(S_ESP)) pinMode(pin, OUTPUT);
    off();
  }
  void toggle() {
    state = !state;
    if (sourceIs(S_ESP)) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, state);
    } else pcf.write(pin, state);
  }
  void on() {
    state = true;
    if (sourceIs(S_ESP)) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, true);
    } else pcf.write(pin, true);
  }
  void off() {
    state = false;
    if (sourceIs(S_ESP)) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, false);
    } else pcf.write(pin, false);
  }
  bool getState() {
    return state;
  }
  bool sourceIs(uint8_t input) {  // checks if source is ESP or PCF; if(sourceIs(S_ESP)) then...;
    return (source == input);
  }
};
led PRO_LED(6, S_PCF);
led SCRE_LED(5, S_PCF);
led FX_LED(7, S_PCF);
led LED4(D10, S_ESP);
led LED5(D3, S_ESP);


class touch {
public:
  uint8_t pin;
  touch(uint8_t in, functionPointer onTouchActionBuffer) {  // in: pin; odkaz na funkci jako onTouchActionBuffer ; -all touches are in pcf
    pin = in;
    onTouchActionPointer = onTouchActionBuffer;  // onTouchActionPointer je ted onTouchActionBuffer - ULOŽENÍ
  }
  bool read() {
    return pcf.read(pin);
  }
  void exectuteAction() {
    onTouchActionPointer();
  }
  void executeOnTouchAction() {
    if (read()) onTouchActionPointer();
  }
  void waitForTouch() {
    while (!read())
      ;
  }
  void waitForRelease() {
    while (read())
      ;
  }
  void waitAndExecuteOnTouchAction() {
    waitForTouch();
    exectuteAction();
  }

private:
  functionPointer onTouchActionPointer = nullptr;  // globální uložení ve třídě
};

void myDisplay(String s, uint8_t t = 1, bool n = false) {  // string, text size, bool if requires to clear display
  if (n) {
    display.clearDisplay();
    display.setCursor(0, 0);
  }
  display.setTextSize(t);
  st.comment(s);  // optional
  display.println(s);
}

void touch2Action();  // declaration of actions
void touch3Action();
void touch4Action();
void touch5Action();
void touch6Action();

touch touch2(4, touch2Action);  // FX
touch touch3(3, touch3Action);  //PRO ON
touch touch4(2, touch4Action);  //SCRE OFF
touch touch5(0, touch5Action);  //PRO OFF
touch touch6(1, touch6Action);  //SCRE ON

Timer resetTimer;
void touch2Action() {
  PRO_LED.toggle();
  SCRE_LED.toggle();
  FX_LED.toggle();
  resetTimer.start();
  touch2.waitForRelease();
  if (resetTimer.read() >= 6000) {
    LED5.on();
    delay(2000);
    ESP.restart();
  } else resetTimer.stop();
}
void touch3Action() {
  PRO_LED.on();
  touch3.waitForRelease();
}
void touch4Action() {
  SCRE_LED.off();
  touch4.waitForRelease();
}

Timer showURL;

void touch5Action() {
  PRO_LED.off();
  showURL.start();
  touch5.waitForRelease();
  if (showURL.read() >= 6000) {
    while (st.doForMs(6000)) {
      myDisplay(F("github.com/TechN3o"), 1, true);
      myDisplay(F("/contRoller"), 1);
      display.display();
      display.clearDisplay();
    }
  } else showURL.stop();
}
void touch6Action() {
  SCRE_LED.on();
  touch6.waitForRelease();
}


int combineHoursMinutes(uint8_t hours, uint8_t minutes) {  //combining hours and minutes into just minutes
  return hours * 60 + minutes;
}
int fromMinutesToHours(uint16_t input) {  // from minutes count how many hours is that and to prevent displaying e.g. 24:50, theres condition to eliminate this
  if ((input / 60) >= 24) return input / 60 - 24;
  else return input / 60;
}
int remainingMinutesFromHours(uint16_t input) {
  return input % 60;
}
String addColon(uint16_t input) {  // adding colon between hours and minutes while also taking care of that zero if minutes are less than 10, to show e.g. 14:05 instead of 14:5
  if (remainingMinutesFromHours(input) < 10) return ":0";
  else return ":";
}

String returnTimeString() {
  TC.update();
  static uint16_t allDayMinutes;
  allDayMinutes = combineHoursMinutes(TC.getHours(), TC.getMinutes()) + minuteOffset;
  // now returning string for display


  return String(fromMinutesToHours(allDayMinutes)) + addColon(allDayMinutes) + String(remainingMinutesFromHours(allDayMinutes));
}

// 7:05 - 7:50 - 8:00 - 8:45 - 8:50 - 9:35 - 9:45 - 10:30 - 10:45 - 11:30 - 11:35 - 12:20 - 12:30 - 13:15 - 13:20 - 14:05 - 14:10 - 14:55 - 15:00 - 15:45 - 15:50 - 16:35
const int timeTable[] = {
  // chat gpt here 🙏          aaand of course, it could have been done, simply by the fact that these times are separated by 45 minute interval and breaks are 5, 10, 15 and again minutes, but [EXCUSE]
  // ..also maybe these functions could be preprocessor functions, so no calculations are performed, as the times are constant
  combineHoursMinutes(7, 5),  // 425
  combineHoursMinutes(7, 50),
  combineHoursMinutes(8, 0),
  combineHoursMinutes(8, 45),
  combineHoursMinutes(8, 50),
  combineHoursMinutes(9, 35),
  combineHoursMinutes(9, 45),
  combineHoursMinutes(10, 30),
  combineHoursMinutes(10, 45),
  combineHoursMinutes(11, 30),
  combineHoursMinutes(11, 35),
  combineHoursMinutes(12, 20),
  combineHoursMinutes(12, 25),
  combineHoursMinutes(13, 10),
  combineHoursMinutes(13, 15),
  combineHoursMinutes(14, 0),
  combineHoursMinutes(14, 5),
  combineHoursMinutes(14, 50),
  combineHoursMinutes(14, 55),
  combineHoursMinutes(15, 40),
  combineHoursMinutes(15, 50),
  combineHoursMinutes(16, 35)  //995
};
Timer leftTimer;
int minutesLeft(bool f = false) {  // f: forced to calculate
  static int output = 0;
  uint16_t cTime = combineHoursMinutes(TC.getHours(), TC.getMinutes()) + minuteOffset;  // currentTime to compare

  if ((leftTimer.read() >= 5000) || f) {  // inicializace to calculate
    leftTimer.pause();
    leftTimer.start();
    st.print(F("Calculating minutes left"));
    if (cTime > timeTable[arraySize(timeTable) - 1]) {  // case if the time is not contained in timeTable
      st.print(F(", but out of range of time table, max(minutes):"));
      st.println(String(timeTable[arraySize(timeTable) - 1]));
      return 0;
    }


    for (uint8_t i = 0; i < arraySize(timeTable); i++) {
      if (cTime < timeTable[i]) {
        output = timeTable[i] - cTime;
        if (timeTable[i] == cTime) LED5.on();  // if current time (cTime) and that specific element of timeTable match, then the bell is ringing >> turning signal blue LED5 led
        else LED5.off();
        return output;
      }
    }
  } else {
    return output;
  }
  // If current time is after the last time in the table, return 0 or some other value
  return 0;
}

// bitmaps generated at: https://javl.github.io/image2cpp/

// 'wifi_0', 15x14px
const unsigned char epd_bitmap_wifi_0[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x07, 0xc0, 0x08, 0x20, 0x10, 0x10, 0x03, 0x80, 0x03, 0x80
};
// 'wifi_2', 15x14px
const unsigned char epd_bitmap_wifi_2[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xe0, 0x1f, 0xf0, 0x30, 0x18, 0x67, 0xcc,
  0x4f, 0xe4, 0x18, 0x30, 0x13, 0x90, 0x07, 0xc0, 0x07, 0xc0, 0x03, 0x80
};
// 'wifi_full', 15x14px
const unsigned char epd_bitmap_wifi_full[] PROGMEM = {
  0x00, 0x00, 0x0f, 0xe0, 0x3f, 0xf8, 0x60, 0x0c, 0xcf, 0xe6, 0x9f, 0xf2, 0x30, 0x18, 0x67, 0xcc,
  0x4f, 0xe4, 0x18, 0x30, 0x13, 0x90, 0x07, 0xc0, 0x07, 0xc0, 0x03, 0x80
};
// 'wifi_1', 15x14px
const unsigned char epd_bitmap_wifi_1[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xc0,
  0x0f, 0xe0, 0x18, 0x30, 0x13, 0x90, 0x07, 0xc0, 0x07, 0xc0, 0x03, 0x80
};

void displayRSSI(int8_t input = -58, uint8_t xpos = 113) {
  uint8_t value = abs(input);

  if (value <= 54) display.drawBitmap(xpos, 0, epd_bitmap_wifi_full, 15, 14, WHITE);                    // best
  else if (st.isInRange(value, 55, 58)) display.drawBitmap(xpos, 0, epd_bitmap_wifi_2, 15, 14, WHITE);  // average
  else if (st.isInRange(value, 58, 65)) display.drawBitmap(xpos, 0, epd_bitmap_wifi_1, 15, 14, WHITE);  // ok
  else if (value >= 66) display.drawBitmap(xpos, 0, epd_bitmap_wifi_0, 15, 14, WHITE);                  // worse}
}
Timer disconnectCheckTimer;
void ifDisconnected(bool d = false) {
  if ((disconnectCheckTimer.read() >= 10 * 1000) || d) {
    disconnectCheckTimer.pause();
    disconnectCheckTimer.start();
    if (IS_DISCONNECTED) {
      WiFi.begin(ssid, password);
      while (IS_DISCONNECTED) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        myDisplay(F("Pripojovani..."), 1, true);
        myDisplay("- ssid: " + String(ssid), 1);
        myDisplay("- heslo: " + st.returnNchars('*', strlen(password) - 1) + password[strlen(password) - 1], 1);
        myDisplay(F("+ Minonky..."), 1);
        display.display();
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      myDisplay(F("Pripojeno!"), 1, true);
      myDisplay("+ ssid: " + String(ssid), 1);
      myDisplay("+ heslo: " + st.returnNchars('*', strlen(password) - 1) + password[strlen(password) - 1], 1);
      myDisplay(F("reset NTP..."), 1);
      displayRSSI(WiFi.RSSI());
      display.display();
      TC.end();
      vTaskDelay(3000 / portTICK_PERIOD_MS);
      TC.begin();  // start NTP klienta
    }
  } else st.println("WiFi strenght (RSSI): " + String(WiFi.RSSI()) + " dBm");  // connection strenght // the lower number, the better connection/strenght
}
/*

void irReadDisplay() {
  if (IrReceiver.decode() || (Serial.available() > 0)) {

         // Print a summary of received data
    if (IrReceiver.decodedIRData.protocol == UNKNOWN) {
      Serial.println("ERROR UN");
      IrReceiver.resume();
    } else {
      IrReceiver.resume();  // Early enable receiving of the next IR frame
      Serial.println(IrReceiver.decodedIRData.command);
    }
    if (IrReceiver.decodedIRData.command == 0) {
      myDisplay("ERROR", 4, true);
    } else {
      myDisplay(String(IrReceiver.decodedIRData.command), 4, true);
    }
    st.comment(String(IrReceiver.decodedIRData.command));
  }
} */


void TaskDisplay(void *pvParameters);
void TaskTouch(void *pvParameters);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  st.setCommentDivideLine('#', '=', '#');
  st.commentSessionTimeMS(SET, 280);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Wire.begin();
  pcf.begin();

  display.clearDisplay();
  display.setTextColor(WHITE);
  myDisplay(F("start.."), 3);
  // IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  ifDisconnected(true);
  TC.begin();  // start NTP client
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  leftTimer.start();
  disconnectCheckTimer.start();
  PRO_LED.off();
  SCRE_LED.off();
  FX_LED.off();
  LED4.off();
  LED5.off();
  xTaskCreate(
    TaskDisplay,  // Funkce úlohy
    "display",    // Název úlohy
    16384,        // Velikost zásobníku
    NULL,         // Parametry úlohy
    2,            // Priorita úlohy
    NULL          // Handle úlohy
  );
  xTaskCreate(
    TaskTouch,  // Funkce úlohy
    "touch",    // Název úlohy
    1000,       // Velikost zásobníku
    NULL,       // Parametry úlohy
    1,          // Priorita úlohy
    NULL        // Handle úlohy
  );
}

void loop() {}
void TaskDisplay(void *pvParameters) {
  for (;;) {
    myDisplay(returnTimeString(), 2, true);  // main text, what time is it
    // st.println("--returnTimeString");
    myDisplay("zbyva " + String(minutesLeft()) + "min", 2);  // how many minutes left till end of class/break
    // st.println("--minutes left");
    displayRSSI(WiFi.RSSI());
    // st.println("--display RSSI");
    ifDisconnected();  // check wifi status
    // st.println("-- if disconnected");
    if (Serial.available()) {
      // st.println("-- entered Serial receiving");
      int16_t SerPI = Serial.parseInt();
      st.println("Serial available - parseInt[" + String(SerPI) + "]");
      Serial.read();
      switch (SerPI) {  // if serial recevies:
        case 200:       // will retry to connect to wifi
          WiFi.begin(ssid, password);
          break;
        case 205:  // will disconnect, but when checking for connnection status (above), it will connect back again
          WiFi.disconnect();
          break;
        default:  // else if the code doesnt match these above, the number will be set as minute offset
          minuteOffset = SerPI;
          st.println("minuteOffset is: " + String(SerPI));
          myDisplay("zbyva " + String(minutesLeft(true)) + "min", 2);  // how many minutes left till end of class/break
      }
    }
    vTaskDelay(400 / portTICK_PERIOD_MS);
    display.display();
    vTaskDelay(300 / portTICK_PERIOD_MS);
    display.clearDisplay();
    // st.println("-- display display");
  }
}
void TaskTouch(void *pvParameters) {
  for (;;) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    touch2.executeOnTouchAction();
    touch3.executeOnTouchAction();
    touch4.executeOnTouchAction();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    touch5.executeOnTouchAction();
    touch6.executeOnTouchAction();
  }
}









//