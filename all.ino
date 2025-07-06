#define BLYNK_TEMPLATE_ID "TMPL6LZ4sAz_g"
#define BLYNK_TEMPLATE_NAME "canhbao"
#define BLYNK_AUTH_TOKEN "tSj2GkQsaSLXVIS-B65MM2k3lVwK2Fhj"

#include <Wire.h>
#include <MPU6050_light.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <HardwareSerial.h>
#include <BlynkSimpleEsp32.h>
#include <math.h>

// ===== CẤU HÌNH WIFI VÀ BLYNK =====
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "hazang";
char pass[] = "khongcho";

MPU6050 camBienMPU(Wire);
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

const int ledPin = 2;
const int buzzerPin = 4;

const float nguongGiaToc = 1.0;
const float nguongDoNghieng = 50.0;

bool taiNanDuocXacNhan = false;
bool dangCanhBao = false;
unsigned long thoiGianCanhBao = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(buzzerPin, LOW);

  Serial.println("⏳ Đang kết nối WiFi...");
  WiFi.begin(ssid, pass);
  unsigned long batDau = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    if (millis() - batDau > 20000) {
      Serial.println("\n❌ Kết nối WiFi thất bại.");
      while (1);
    }
  }
  Serial.println("\n✅ WiFi đã kết nối.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("⏳ Đang kết nối Blynk...");
  Blynk.begin(auth, ssid, pass);
  Serial.println("✅ Blynk đã kết nối.");

  if (camBienMPU.begin() != 0) {
    Serial.println("❌ Không tìm thấy MPU6050!");
    while (1);
  }
  camBienMPU.calcGyroOffsets();
  Serial.println("✅ MPU6050 sẵn sàng");

  Serial.print("📡 Kiểm tra GPS");
  unsigned long batDauGPS = millis();
  bool gpsTimThay = false;

  while (millis() - batDauGPS < 10000) {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
      if (gps.location.isValid()) {
        gpsTimThay = true;
        Serial.println("\n✅ GPS sẵn sàng");
        break;
      }
    }
    if (gpsTimThay) break;
    Serial.print(".");
    delay(500);
  }
  if (!gpsTimThay) {
    Serial.println("\n⚠️ GPS không hoạt động");
  }

  Blynk.virtualWrite(V5, "🛵 Không phát hiện tai nạn");
}

void loop() {
  Blynk.run();
  camBienMPU.update();

  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  float ax = camBienMPU.getAccX();
  float ay = camBienMPU.getAccY();
  float az = camBienMPU.getAccZ();
  float giaTocRong = sqrt(ax * ax + ay * ay + az * az) - 1.0;

  float gocX = camBienMPU.getAngleX();
  float gocY = camBienMPU.getAngleY();

  Blynk.virtualWrite(V4, giaTocRong);

  // In ra màn hình góc và gia tốc
  Serial.print("Gia tốc ròng: ");
  Serial.print(giaTocRong);
  Serial.print(" | Góc X: ");
  Serial.print(gocX);
  Serial.print(" | Góc Y: ");
  Serial.println(gocY);

  // Nếu GPS có tín hiệu, lấy vị trí và gửi lên Blynk
  if (gps.location.isValid()) {
    float vanToc = gps.speed.kmph();
    String linkViTri = "https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
    Blynk.virtualWrite(V0, (int)vanToc);
    Blynk.virtualWrite(V3, linkViTri);
  }

  if (giaTocRong > nguongGiaToc && !dangCanhBao) {
    Serial.println("⚠️ Phát hiện gia tốc vượt ngưỡng! Đang kiểm tra góc nghiêng...");

    unsigned long batDauTheoDoi = millis();
    bool phatHienNghiengManh = false;

    while (millis() - batDauTheoDoi < 10000) {
      camBienMPU.update();
      while (gpsSerial.available()) gps.encode(gpsSerial.read());
      gocX = abs(camBienMPU.getAngleX());
      gocY = abs(camBienMPU.getAngleY());
      if (gocX > nguongDoNghieng || gocY > nguongDoNghieng) {
        phatHienNghiengManh = true;
        break;
      }
      delay(50);
    }

    if (phatHienNghiengManh) {
      Serial.println("✅ Phát hiện góc nghiêng vượt ngưỡng! Đang kiểm tra duy trì trong 5s...");
      unsigned long batDauNghiengLienTuc = millis();
      bool duocDuyTri5s = true;

      while (millis() - batDauNghiengLienTuc < 5000) {
        camBienMPU.update();
        while (gpsSerial.available()) gps.encode(gpsSerial.read());
        gocX = abs(camBienMPU.getAngleX());
        gocY = abs(camBienMPU.getAngleY());
        if (gocX < nguongDoNghieng && gocY < nguongDoNghieng) {
          duocDuyTri5s = false;
          Serial.println("❌ Góc nghiêng không được duy trì trên 5s, xác nhận không có tai nạn.");
          break;
        }
        delay(100);
      }

      if (duocDuyTri5s) {
        taiNanDuocXacNhan = true;
        Serial.println("🚨 Xác nhận có tai nạn!");
      }
    } else {
      Serial.println("❌ Không phát hiện nghiêng mạnh.");
    }
  }

  if (taiNanDuocXacNhan && !dangCanhBao) {
    digitalWrite(ledPin, HIGH);
    digitalWrite(buzzerPin, HIGH);
    dangCanhBao = true;
    thoiGianCanhBao = millis();
    Blynk.virtualWrite(V5, "🚨 Phát hiện tai nạn!");
  }

  if (dangCanhBao && millis() - thoiGianCanhBao > 30000) {
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
    taiNanDuocXacNhan = false;
    dangCanhBao = false;
    Serial.println("✅ Đã tắt cảnh báo.");
    Blynk.virtualWrite(V5, "🛵 Không phát hiện tai nạn");
  }

  delay(1000);
}
