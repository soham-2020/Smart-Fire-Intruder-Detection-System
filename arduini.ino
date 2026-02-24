#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <NewPing.h>
#include <IRremote.h>

#define DHT_PIN       7
#define FLAME_PIN     6
#define TRIG_PIN      9
#define ECHO_PIN      10
#define IR_PIN        11
#define DHT_TYPE      DHT11
#define MAX_DISTANCE  200

DHT dht(DHT_PIN, DHT_TYPE);
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

bool systemArmed = true;
unsigned long lastAlert = 0;
#define COOLDOWN_MS 10000  // 10 seconds between alerts

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Starting");
  delay(2000);
  lcd.clear();

  dht.begin();
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  pinMode(FLAME_PIN, INPUT);

  Serial.println("SYSTEM READY");
  lcd.print("System Ready");
}

void loop() {
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  int flame  = digitalRead(FLAME_PIN);
  int dist   = sonar.ping_cm();

  // IR Remote arm/disarm
  if (IrReceiver.decode()) {
    uint32_t code = IrReceiver.decodedIRData.command;
    if (code == 0x45) {
      systemArmed = true;
      Serial.println("System ARMED");
    }
    if (code == 0x46) {
      systemArmed = false;
      Serial.println("System DISARMED");
    }
    IrReceiver.resume();
  }

  // LCD Update
  lcd.clear();
  if (!systemArmed) {
    lcd.setCursor(0, 0);
    lcd.print("System DISARMED");
    delay(1000);
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C H:");
  lcd.print(hum, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  if (flame == LOW) {
    lcd.print("!! FIRE ALERT !!");
  } else if (dist > 0 && dist < 30) {
    lcd.print("INTRUDER! ");
    lcd.print(dist);
    lcd.print("cm");
  } else {
    lcd.print("Dist:");
    lcd.print(dist);
    lcd.print("cm OK");
  }

  // Only send alert with cooldown
  bool alertNeeded = false;
  if (flame == LOW)              alertNeeded = true;
  if (dist > 0 && dist < 30)    alertNeeded = true;

  if (alertNeeded && systemArmed &&
      (millis() - lastAlert > COOLDOWN_MS)) {
    lastAlert = millis();

    Serial1.print("TEMP:");  Serial1.print(temp);
    Serial1.print(",HUM:");  Serial1.print(hum);
    Serial1.print(",FLAME:"); Serial1.print(flame);
    Serial1.print(",DIST:"); Serial1.print(dist);
    Serial1.print(",ARMED:"); Serial1.println(systemArmed);

    Serial.println("ALERT SENT TO ESP32!");
  }

  delay(500);
}
