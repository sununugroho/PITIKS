#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6AcQ86ZCI"
#define BLYNK_TEMPLATE_NAME "PITIKS"
#define BLYNK_AUTH_TOKEN "KzHDRWwrf_oE-TKzZiWM3Jka6y6Bl5Bp"

#include <Ultrasonic.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <BlynkSimpleEsp32.h>
#include <FirebaseESP32.h> // Pustaka Firebase untuk ESP32

#define TRIG_PIN 33    // Pin trigger ultrasonik terhubung ke pin 33 pada ESP32
#define ECHO_PIN 32    // Pin echo ultrasonik terhubung ke pin 32 pada ESP32
#define SERVO_PIN 14   // Pin servo terhubung ke pin 14 pada ESP32
#define DHT_PIN 25     // Pin data DHT11 terhubung ke pin 25 pada ESP32
#define RELAY_PIN 26   // Pin relay terhubung ke pin 26 pada ESP32

Ultrasonic ultrasonic(TRIG_PIN, ECHO_PIN); // Objek untuk sensor ultrasonik
Servo servo; // Objek untuk servo
RTC_DS3231 rtc; // Objek RTC
DHT dht(DHT_PIN, DHT11); // Objek untuk sensor DHT11
int relayPin = RELAY_PIN; // Variabel untuk pin relay

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "sss";
char password[] = "999999999";

// Firebase configuration
#define FIREBASE_HOST "pitiks-c49b7-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "y0AyhGNbDb57FUFmOOB02rxJqG226H0uGhsm5SJq"

FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

int bulbValue, servoValue;

BLYNK_WRITE(V0){
  bulbValue = param.asInt();
  digitalWrite(relayPin, bulbValue);
}

BLYNK_WRITE(V1){
  servoValue = param.asInt();
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  pinMode(relayPin, OUTPUT); // Mengatur pin relay sebagai output
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Tidak terhubung ke jaringan WiFi...");
  }
  
  Serial.println("Terhubung ke jaringan WiFi!");
  
  // Inisialisasi koneksi NTP
  timeClient.begin();
  timeClient.setTimeOffset(25200); // Atur offset waktu sesuai dengan zona waktu Anda (dalam detik)
  Blynk.begin(auth, ssid, password);
  Wire.begin();
  rtc.begin();

  // Periksa apakah RTC kehilangan daya, jika ya, atur waktu default
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time to default!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  servo.attach(SERVO_PIN); // Menghubungkan servo ke pin 14

  // Inisialisasi Firebase
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  Blynk.run(); // Jalankan Blynk
  timeClient.update(); // Perbarui waktu dari server NTP
  unsigned long epochTime = timeClient.getEpochTime(); // Dapatkan waktu epoch dari NTP
  DateTime now = DateTime(epochTime); // Konversi waktu epoch ke DateTime
  
  // Baca jarak dari sensor ultrasonik
  float distance = ultrasonic.read();
  
  // Hitung persentase sisa pakan
  float feedPercentage = 100 - (distance / 10) * 100;
  
  // Cetak jarak yang terbaca ke Serial Monitor
  Serial.print("jarak pakan: ");
  Serial.print(distance);
  Serial.println(" cm");
  Blynk.virtualWrite(V2, feedPercentage);

  // Baca suhu dan kelembaban dari sensor DHT11
  float temperature = dht.readTemperature();
  Blynk.virtualWrite(V4, temperature);
  float humidity = dht.readHumidity();
  Blynk.virtualWrite(V3, humidity);
  
  // Cetak suhu dan kelembaban ke Serial Monitor
  Serial.print("Suhu: ");
  Serial.print(temperature);
  Serial.print(" °C, Kelembaban: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Ambil tanggal dan waktu
  int currentYear = now.year();
  int currentMonth = now.month();
  int currentDay = now.day();
  int currentHour = now.hour();
  int currentMinute = now.minute();
  int currentSecond = now.second();

  // Buat string tanggal dengan format day-month-year
  char dateStr[11];
  sprintf(dateStr, "%02d-%02d-%04d", currentDay, currentMonth, currentYear);
  
  // Cetak tanggal dan waktu ke Serial Monitor
  Serial.print("Tanggal: ");
  Serial.print(dateStr);
  Serial.print(" ");
  Serial.print("waktu saat ini: ");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.print(currentMinute);
  Serial.print(":");
  Serial.println(currentSecond);

  // Kirim data ke Firebase
  sendToFirebase(temperature, humidity, feedPercentage, dateStr);

  // Logika untuk jadwal pakan otomatis
  if (distance < 10) {
    if ((currentHour == 16 && currentMinute == 13 && currentSecond == 0) || 
        (currentHour == 16 && currentMinute == 14 && currentSecond == 0) || 
        (currentHour == 16 && currentMinute == 15 && currentSecond == 0) || 
        (currentHour == 16 && currentMinute == 16 && currentSecond == 0)) {
      activateServo(); // Jika jarak kurang dari 10 cm dan waktu sesuai dengan jadwal, aktifkan servo
    }
  } else {
    servo.write(0); // Nonaktifkan servo jika jarak lebih dari atau sama dengan 10 cm
  }

  // Logika untuk mengontrol relay
  if (temperature < 30 || humidity > 60) {
    digitalWrite(relayPin, LOW); // Nyalakan relay jika suhu kurang dari 20°C atau kelembaban lebih dari 60%
    Blynk.virtualWrite(V0, 1);
    Serial.println("Lampu Menyala!"); // Tambahkan ini
  } else {
    Blynk.virtualWrite(V0, 0);
    digitalWrite(relayPin, HIGH); // Matikan relay jika tidak memenuhi kondisi
  }

  delay(1000); // Delay untuk mengurangi laju pembacaan (setiap 1 detik)
}

void activateServo() {
  // Aktifkan servo
  servo.write(180); // Posisi 180 derajat membuka servo
  Serial.println("Beri pakan!"); // Tambahkan ini
  delay(servoValue); // Tunggu 1 detik
  servo.write(0); // Nonaktifkan servo setelah diberi pakan
}

void sendToFirebase(float temperature, float humidity, float feedPercentage, const char* dateStr) {
  if (Firebase.ready()) {
    Firebase.setFloat(firebaseData, "/sensor/temperature", temperature);
    Firebase.setFloat(firebaseData, "/sensor/humidity", humidity);
    Firebase.setFloat(firebaseData, "/sensor/feedPercentage", feedPercentage);
    Firebase.setString(firebaseData, "/sensor/date", dateStr);
  } else {
    Serial.println("Failed to send data to Firebase");
  }
}
