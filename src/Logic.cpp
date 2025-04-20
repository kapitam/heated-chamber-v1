#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ----- LCD Setup -----
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ----- Pins -----
#define BUTTON_ADD     34
#define BUTTON_CONFIRM 35  // Unused
#define BUTTON_MINUS   32
#define ONE_WIRE_BUS   33
#define RELAY_PIN      25

// ----- OneWire and Dallas Sensor -----
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ----- Constants -----
#define HOLD_THRESHOLD      50   // ms before starting repeat
#define REPEAT_INTERVAL     100   // ms between repeats when held
#define HYSTERESIS            2
#define DEBOUNCE_DELAY       50   // ms for debounce

// ----- State Tracking -----
int tempset = 25;
float tempread = 0;
bool forceDisplayUpdate = true;

// ----- Timing for Non-blocking Temperature Read -----
unsigned long lastTempRequestTime = 0;
unsigned long tempConversionDelay = 750;
bool tempRequested = false;

// ----- LCD Timing -----
unsigned long lastDisplayUpdate = 0;

// ----- Button Struct -----
struct ButtonState {
  int pin;
  bool wasPressed;
  unsigned long pressStart;
  unsigned long lastRepeat;
  unsigned long lastDebounceTime;
};

ButtonState addButton   = {BUTTON_ADD, false, 0, 0, 0};
ButtonState minusButton = {BUTTON_MINUS, false, 0, 0, 0};
ButtonState selectButton = {BUTTON_CONFIRM, false, 0, 0, 0};

// --  Display type
bool currentControl = 1;
// ----- Setup -----
void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_ADD, INPUT);
  pinMode(BUTTON_CONFIRM, INPUT);
  pinMode(BUTTON_MINUS, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  lcd.init();
  lcd.backlight();

  sensors.begin();
  sensors.setWaitForConversion(false);  // <-- async conversion

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Heated Chamber");
  lcd.setCursor(4, 1);
  lcd.print("+    Set   -");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Target: ");
  lcd.print(tempset);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("Current: --.- C");
}

// ----- Button Handler with Debounce -----
void handleButton(ButtonState &btn, int &target, int delta) {
  bool state = digitalRead(btn.pin) == HIGH;
  unsigned long now = millis();

  if (state && !btn.wasPressed) {
    // Just pressed
    btn.pressStart = now;
    btn.lastRepeat = now;
    target += delta;
    forceDisplayUpdate = true;
  } 
  else if (state && btn.wasPressed) {
    // Held
    if (now - btn.pressStart >= HOLD_THRESHOLD && now - btn.lastRepeat >= REPEAT_INTERVAL) {
      target += delta;
      btn.lastRepeat = now;
      forceDisplayUpdate = true;
    }
  }

  btn.wasPressed = state;
}

bool buttonSelect(ButtonState &btn) {
  bool state = digitalRead(btn.pin) == HIGH;
  unsigned long now = millis();

  if (state && !btn.wasPressed) {
    btn.pressStart = now;
    btn.lastRepeat = now;
    forceDisplayUpdate = true;
  } 
  else if (state && btn.wasPressed) {
    // Held
    if (now - btn.pressStart >= HOLD_THRESHOLD && now - btn.lastRepeat >= REPEAT_INTERVAL) {
      btn.lastRepeat = now;
      forceDisplayUpdate = true;
      return true;
    }
  }

  btn.wasPressed = state;
  return false;
}

// ----- Relay Logic -----
void updateRelay(bool logic) {
  if (logic == true) {
    if (tempread > tempset + HYSTERESIS) {
      digitalWrite(RELAY_PIN, LOW); // OFF
    } else if (tempread < tempset - HYSTERESIS) {
      digitalWrite(RELAY_PIN, HIGH);  // ON
    }
  } else  {
    digitalWrite(RELAY_PIN, LOW); // OFF
  }
}

// ----- Display Update -----
void defultUpdateDisplay() {
  if (forceDisplayUpdate || millis() - lastDisplayUpdate >= 1000) {
    lcd.setCursor(8, 0);
    lcd.print(tempset);
    lcd.print(" C  ");

    lcd.setCursor(9, 1);
    lcd.print(tempread, 1);
    lcd.print(" C  ");

    lastDisplayUpdate = millis();
    forceDisplayUpdate = false;
    //Serial.println(tempset);
  }
}

void readUpdateDisplay() {
  if (forceDisplayUpdate || millis() - lastDisplayUpdate >= 1000) {
    lcd.setCursor(0, 0);
    lcd.print("Read Only");

    lcd.setCursor(9, 1);
    lcd.print(tempread, 1);
    lcd.print(" C  ");

    lastDisplayUpdate = millis();
    forceDisplayUpdate = false;
    //Serial.println(tempset);
  }
}

void controlUpdater(bool toggle) {
  if (toggle) {
    currentControl = !currentControl;  // toggle between 0 and 1
  }

  if (currentControl == 1) {
    defultUpdateDisplay();
  } else {
    readUpdateDisplay();
  }
}

// ----- Main Loop -----
void loop() {
  unsigned long now = millis();

  // --- Non-blocking temperature reading ---
  if (!tempRequested) {
    sensors.requestTemperatures();
    lastTempRequestTime = now;
    tempRequested = true;
  } else if (now - lastTempRequestTime >= tempConversionDelay) {
    tempread = sensors.getTempCByIndex(0);
    tempRequested = false;
    forceDisplayUpdate = true;
  }

  // --- Button Handling ---
  handleButton(addButton, tempset, +1);
  handleButton(minusButton, tempset, -1);

  // --- Relay and Display ---
  updateRelay(currentControl);
  controlUpdater(buttonSelect(selectButton));
  delay(150);
}