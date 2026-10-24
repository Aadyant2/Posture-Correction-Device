/**
 * =========================================================================
 * Project: Wearable Posture Sensor
 * Target MCU: Seeed Studio XIAO ESP32-C3
 * Components:
 *   - Seeed Studio XIAO ESP32-C3 (Microcontroller)
 *   - MPU6050 (GY-521) IMU Lower (Address 0x68, AD0 -> GND)
 *   - MPU6050 (GY-521) IMU Upper (Address 0x69, AD0 -> 3V3)
 *   - DRV2605L Haptic Motor Driver (Address 0x5A, EN -> 3V3)
 *   - Coin Vibration Motor (ERM) on DRV2605L OUT+/OUT-
 *   - Pushbutton on D1 (GPIO3) -> GND (Calibration Tare)
 *   - Status LED on D2 (GPIO4) via 220 Ohm resistor -> GND
 *   - I2C Bus on D4 (GPIO6 / SDA) and D5 (GPIO7 / SCL)
 * 
 * Rules Adherence:
 *   - INT pins on both IMUs left unconnected (Polled I2C architecture)
 *   - No duplicate pin usage
 *   - Shared single 3.3V logic & power rail
 * =========================================================================
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_DRV2605.h>

// --- PIN ASSIGNMENTS (XIAO ESP32-C3) ---
#define PIN_I2C_SDA      6   // XIAO D4 / GPIO6
#define PIN_I2C_SCL      7   // XIAO D5 / GPIO7
#define PIN_BUTTON_TARE  3   // XIAO D1 / GPIO3 (Internal Pullup)
#define PIN_LED_ALERT    4   // XIAO D2 / GPIO4 (Active HIGH)

// --- I2C ADDRESSES ---
#define ADDR_IMU_LOWER   0x68  // AD0 to GND
#define ADDR_IMU_UPPER   0x69  // AD0 to 3V3
#define ADDR_DRV2605L    0x5A  // Fixed DRV2605L address

// --- POSTURE THRESHOLDS ---
#define SLOUCH_ANGLE_THRESHOLD_DEG  18.0f // Angular deviation triggering alert
#define SLOUCH_DEBOUNCE_MS          1200  // Time in bad posture before haptic buzz
#define ALERT_COOLDOWN_MS           3500  // Delay between repeated buzzes

// --- HARDWARE OBJECTS ---
Adafruit_MPU6050 imuLower;
Adafruit_MPU6050 imuUpper;
Adafruit_DRV2605 haptic;

// --- RUNTIME STATE ---
float baselineOffsetAngle = 0.0f; // Calibrated posture reference
float currentSlouchAngle  = 0.0f;
unsigned long slouchStartTime = 0;
unsigned long lastAlertTime   = 0;
bool isCurrentlySlouching     = false;

// Compute pitch angle in degrees from accelerometer readings
float calculatePitch(sensors_event_t &a) {
  // pitch = atan2(-ax, sqrt(ay^2 + az^2)) * 180 / PI
  float pitchRad = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z));
  return pitchRad * (180.0f / PI);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("===========================================");
  Serial.println("  Wearable Posture Sensor (XIAO ESP32-C3)  ");
  Serial.println("===========================================");

  // 1. Initialize GPIO
  pinMode(PIN_BUTTON_TARE, INPUT_PULLUP);
  pinMode(PIN_LED_ALERT, OUTPUT);
  digitalWrite(PIN_LED_ALERT, LOW);

  // 2. Initialize Shared I2C Bus on GPIO6 (SDA) and GPIO7 (SCL)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000); // 400kHz Fast Mode

  // 3. Initialize IMU Lower (Address 0x68)
  Serial.print("Connecting IMU Lower (0x68)... ");
  if (!imuLower.begin(ADDR_IMU_LOWER, &Wire)) {
    Serial.println("FAILED! Check wiring of IMU lower AD0->GND.");
    while (1) {
      digitalWrite(PIN_LED_ALERT, !digitalRead(PIN_LED_ALERT));
      delay(200);
    }
  }
  imuLower.setAccelerometerRange(MPU6050_RANGE_4_G);
  imuLower.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("OK");

  // 4. Initialize IMU Upper (Address 0x69)
  Serial.print("Connecting IMU Upper (0x69)... ");
  if (!imuUpper.begin(ADDR_IMU_UPPER, &Wire)) {
    Serial.println("FAILED! Check wiring of IMU upper AD0->3V3.");
    while (1) {
      digitalWrite(PIN_LED_ALERT, !digitalRead(PIN_LED_ALERT));
      delay(100);
    }
  }
  imuUpper.setAccelerometerRange(MPU6050_RANGE_4_G);
  imuUpper.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("OK");

  // 5. Initialize DRV2605L Haptic Driver (Address 0x5A)
  Serial.print("Connecting DRV2605L (0x5A)... ");
  if (!haptic.begin(&Wire)) {
    Serial.println("FAILED! Check DRV2605L EN->3V3 and SDA/SCL.");
    while (1) {
      digitalWrite(PIN_LED_ALERT, !digitalRead(PIN_LED_ALERT));
      delay(400);
    }
  }
  haptic.selectLibrary(1); // Standard ERM motor library
  haptic.setMode(DRV2605_MODE_INTTRIG); // Internal trigger mode
  Serial.println("OK");

  // Startup acknowledgment: brief buzz and LED flash
  playHapticClick(1); // 1 = Strong Click 100%
  digitalWrite(PIN_LED_ALERT, HIGH);
  delay(150);
  digitalWrite(PIN_LED_ALERT, LOW);

  Serial.println("System ready. Press button on D1 to tare baseline posture.");
}

void playHapticClick(uint8_t effectNumber) {
  haptic.setWaveform(0, effectNumber); // Waveform slot 0
  haptic.setWaveform(1, 0);            // End sequence
  haptic.go();                         // Fire waveform
}

void loop() {
  unsigned long now = millis();

  // Check Tare Button (Active LOW with debounce)
  if (digitalRead(PIN_BUTTON_TARE) == LOW) {
    delay(50); // Debounce
    if (digitalRead(PIN_BUTTON_TARE) == LOW) {
      Serial.println("[TARE] Calibrating posture zero baseline...");
      
      // Read current angles
      sensors_event_t aLower, gLower, tempLower;
      sensors_event_t aUpper, gUpper, tempUpper;
      imuLower.getEvent(&aLower, &gLower, &tempLower);
      imuUpper.getEvent(&aUpper, &gUpper, &tempUpper);

      float pitchLower = calculatePitch(aLower);
      float pitchUpper = calculatePitch(aUpper);
      baselineOffsetAngle = pitchUpper - pitchLower;

      // Two short chirps for confirmation
      digitalWrite(PIN_LED_ALERT, HIGH);
      playHapticClick(47); // Buzz 1
      delay(120);
      digitalWrite(PIN_LED_ALERT, LOW);
      delay(80);
      digitalWrite(PIN_LED_ALERT, HIGH);
      playHapticClick(47);
      delay(120);
      digitalWrite(PIN_LED_ALERT, LOW);

      Serial.print("Baseline set to: ");
      Serial.print(baselineOffsetAngle, 2);
      Serial.println(" deg");
      
      while (digitalRead(PIN_BUTTON_TARE) == LOW); // Wait for release
    }
  }

  // Sample IMUs
  sensors_event_t aLower, gLower, tempLower;
  sensors_event_t aUpper, gUpper, tempUpper;
  imuLower.getEvent(&aLower, &gLower, &tempLower);
  imuUpper.getEvent(&aUpper, &gUpper, &tempUpper);

  float pitchLower = calculatePitch(aLower);
  float pitchUpper = calculatePitch(aUpper);

  // Relative spinal flexion: Upper Thoracic vs Lower Lumbar
  float rawDelta = (pitchUpper - pitchLower) - baselineOffsetAngle;

  // Exponential moving average filter (alpha = 0.2)
  currentSlouchAngle = (0.2f * rawDelta) + (0.8f * currentSlouchAngle);

  // Evaluate posture
  if (abs(currentSlouchAngle) >= SLOUCH_ANGLE_THRESHOLD_DEG) {
    if (!isCurrentlySlouching) {
      isCurrentlySlouching = true;
      slouchStartTime = now;
    }

    // Trigger alert if in bad posture longer than debounce interval
    if ((now - slouchStartTime) >= SLOUCH_DEBOUNCE_MS) {
      digitalWrite(PIN_LED_ALERT, HIGH);

      if ((now - lastAlertTime) >= ALERT_COOLDOWN_MS) {
        Serial.print("[ALERT] Slouch detected! Angle: ");
        Serial.print(currentSlouchAngle, 1);
        Serial.println(" deg -> Triggering ERM Haptics");

        // Fire distinct double-pulse reminder
        playHapticClick(14); // Strong Buzz 100%
        lastAlertTime = now;
      }
    }
  } else {
    // Good posture restored
    isCurrentlySlouching = false;
    digitalWrite(PIN_LED_ALERT, LOW);
  }

  delay(40); // ~25Hz sample rate is optimal for wearable battery life
}
