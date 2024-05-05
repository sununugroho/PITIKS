#include <Ultrasonic.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define TRIG_PIN 33    // Pin trigger ultrasonik terhubung ke pin 33 pada ESP32
#define ECHO_PIN 32    // Pin echo ultrasonik terhubung ke pin 32 pada ESP32
#define SERVO_PIN 14   // Pin servo terhubung ke pin 14 pada ESP32
#define DHT_PIN 25     // Pin data DHT11 terhubung ke pin 25 pada ESP32
#define LED_PIN 26     // Pin LED terhubung ke pin 26 pada ESP32

Ultrasonic ultrasonic(TRIG_PIN, ECHO_PIN); // Objek untuk sensor ultrasonik
Servo servo; // Objek untuk servo
RTC_DS3231 rtc; // Objek RTC
DHT dht(DHT_PIN, DHT11); // Objek untuk sensor DHT11
int ledPin = LED_PIN; // Variabel untuk pin LED

const char* ssid = "sss";
const char* password = "999999999";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  pinMode(ledPin, OUTPUT); // Mengatur pin LED sebagai output
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Tidak terhubung ke jaringan WiFi...");
  }
  
  Serial.println("Terhubung ke jaringan WiFi!");
  
  // Inisialisasi koneksi NTP
  timeClient.begin();
  timeClient.setTimeOffset(25200); // Atur offset waktu sesuai dengan zona waktu Anda (dalam detik)
  
  Wire.begin();
  rtc.begin();

  // Periksa apakah RTC kehilangan daya, jika ya, atur waktu default
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time to default!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  servo.attach(SERVO_PIN); // Menghubungkan servo ke pin 14
}

void loop() {
  timeClient.update(); // Perbarui waktu dari server NTP
  DateTime now = timeClient.getEpochTime(); // Ambil waktu sekarang
  
  // Baca jarak dari sensor ultrasonik
  float distance = ultrasonic.read();
  
  // Cetak jarak yang terbaca ke Serial Monitor
  Serial.print("jarak pakan: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Baca suhu dan kelembaban dari sensor DHT11
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // Cetak suhu dan kelembaban ke Serial Monitor
  Serial.print("Suhu: ");
  Serial.print(temperature);
  Serial.print(" °C, Kelembaban: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Cetak waktu ke Serial Monitor
  Serial.print("waktu saat ini: ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());

  // Logika untuk jadwal pakan otomatis
  if (distance < 10) {
    int currentHour = now.hour();
    int currentMinute = now.minute();
    int currentSecond = now.second();

    if ((currentHour == 6 && currentMinute == 0 && currentSecond == 0) || 
        (currentHour == 9 && currentMinute == 0 && currentSecond == 0) || 
        (currentHour == 12 && currentMinute == 0 && currentSecond == 0) || 
        (currentHour == 21 && currentMinute == 42 && currentSecond == 30)) {
      activateServo(); // Jika jarak kurang dari 10 cm dan waktu sesuai dengan jadwal, aktifkan servo
    }
  } else {
    servo.write(0); // Nonaktifkan servo jika jarak lebih dari atau sama dengan 10 cm
  }

  // Logika untuk mengontrol LED
  if (temperature < 30 && humidity > 60) {
    digitalWrite(ledPin, HIGH); // Nyalakan LED jika suhu kurang dari 30°C dan kelembaban lebih dari 60%
    Serial.println("Lampu menyala!"); // Tambahkan ini
  } else {
    digitalWrite(ledPin, LOW); // Matikan LED jika tidak memenuhi kondisi
  }

  delay(1000); // Delay untuk mengurangi laju pembacaan (setiap 1 detik)
}

void activateServo() {
  // Aktifkan servo
  servo.write(180); // Posisi 180 derajat membuka servo
  Serial.println("beri pakan!"); // Tambahkan ini
  delay(1000); // Tunggu 1 detik
  servo.write(0); // Nonaktifkan servo setelah diberi pakan
}
