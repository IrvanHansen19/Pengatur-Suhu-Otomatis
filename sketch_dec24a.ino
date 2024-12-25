#define BLYNK_TEMPLATE_ID "TMPL63x_E3xdf"
#define BLYNK_TEMPLATE_NAME "Pengatur Suhu Otomatis"

#include <DHT.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#define DHTPIN 4          // Pin untuk sensor DHT22
#define TRIG_PIN 12       // Pin Trig untuk sensor ultrasonik
#define ECHO_PIN 14       // Pin Echo untuk sensor ultrasonik
#define RELAY_PIN 13      // Pin untuk mengontrol relay
#define FAN_PWM_PIN 32    // Pin PWM untuk kipas
#define LED_RED 18        // LED untuk suhu tinggi
#define LED_YELLOW 19     // LED untuk suhu sedang
#define LED_GREEN 21      // LED untuk suhu rendah

#define DHTTYPE DHT22     // Jenis sensor DHT
DHT dht(DHTPIN, DHTTYPE);

char auth[] = "9LyJG51heTfXMszs01_ApXDmVC4z7pYj"; // Token Blynk
char ssid[] = "BLOKD2 LT1 KAMAR 101";        // Nama WiFi
char pass[] = "101101101";    // Password WiFi

int lastPwmOutput = -1; // Variabel untuk memantau perubahan kecepatan kipas

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = duration * 0.034 / 2; // Konversi waktu ke jarak (cm)
  return distance;
}

// Fungsi Membership Function (Segitiga)
float triangleMF(float x, float a, float b, float c) {
  if (x <= a || x >= c) return 0.0;
  if (x == b) return 1.0;
  if (x > a && x < b) return (x - a) / (b - a);
  if (x > b && x < c) return (c - x) / (c - b);
  return 0.0;
}

// Fungsi Logika Fuzzy
int fuzzyLogic(float temperature, float humidity) {
  float cold = triangleMF(temperature, 0, 15, 20);
  float comfortable = triangleMF(temperature, 20, 23, 26);
  float hot = triangleMF(temperature, 26, 30, 40);

  float dry = triangleMF(humidity, 0, 20, 30);
  float moderate = triangleMF(humidity, 30, 45, 60);
  float humid = triangleMF(humidity, 60, 80, 100);

  // Rule Base untuk output pasti
  if (cold > 0 && dry > 0) {
    return 85; // Slow
  } else if (cold > 0 && moderate > 0) {
    return 85; // Slow
  } else if (cold > 0 && humid > 0) {
    return 170; // Medium
  } else if (comfortable > 0 && dry > 0) {
    return 170; // Medium
  } else if (comfortable > 0 && moderate > 0) {
    return 170; // Medium
  } else if (comfortable > 0 && humid > 0) {
    return 170; // Medium
  } else if (hot > 0 && dry > 0) {
    return 170; // Medium
  } else if (hot > 0 && moderate > 0) {
    return 255; // Fast
  } else if (hot > 0 && humid > 0) {
    return 255; // Fast
  }

  // Default fallback jika tidak ada kondisi dominan
  return 0; 
}

void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, LOW);

  ledcAttach(FAN_PWM_PIN, 25000, 8);  

  Serial.println("Connecting to WiFi...");
  Blynk.begin(auth, ssid, pass);

  Serial.println("System Initialized.");
  digitalWrite(RELAY_PIN, LOW);
  ledcWrite(FAN_PWM_PIN, 0);
}

void loop() {
  Blynk.run();

  long distance = measureDistance();
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Blynk.virtualWrite(V1, temperature); // Kirim suhu ke Blynk Virtual Pin 1
  Blynk.virtualWrite(V2, humidity);    // Kirim kelembapan ke Blynk Virtual Pin 2

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C, Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > 0 && distance < 50) { // Hidupkan sistem jika jarak di bawah 50cm
    Serial.println("Object detected within 50 cm. Activating system.");
    digitalWrite(RELAY_PIN, HIGH);

    int pwmOutput = fuzzyLogic(temperature, humidity);
    Serial.print("Calculated PWM Output: ");
    Serial.println(pwmOutput);

    ledcWrite(FAN_PWM_PIN, pwmOutput);

    // Kirim data ke Blynk hanya jika ada perubahan
    if (pwmOutput != lastPwmOutput) {
      Blynk.virtualWrite(V4, pwmOutput); // Kirim kecepatan kipas ke Blynk
      Serial.print("Updated fan speed to Blynk: ");
      Serial.println(pwmOutput);
      lastPwmOutput = pwmOutput;
    }

    // Perbarui LED berdasarkan nilai PWM
    if (pwmOutput == 85) {
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, LOW);
      Blynk.logEvent("fan_speed", "Fan speed set to Slow");
    } else if (pwmOutput == 170) {
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, HIGH);
      digitalWrite(LED_RED, LOW);
      Blynk.logEvent("fan_speed", "Fan speed set to Medium");
    } else if (pwmOutput == 255) {
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, HIGH);
      Blynk.logEvent("fan_speed", "Fan speed set to Fast");
    }
  } else {
    Serial.println("No object detected within 50 cm. Turning off system.");
    digitalWrite(RELAY_PIN, LOW);
    ledcWrite(FAN_PWM_PIN, 0);

    // Kirim 0 ke Blynk jika kipas mati
    if (lastPwmOutput != 0) {
      Blynk.virtualWrite(V4, 0);
      Serial.println("Updated fan speed to 0 on Blynk.");
      lastPwmOutput = 0;
    }

    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, LOW);
  }

  delay(500);
}
