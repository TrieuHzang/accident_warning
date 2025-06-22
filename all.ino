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
char ssid[] = "NVKHANH";
char pass[] = "87561234";

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

  Serial.println("✅ Đang chạy Blynk và cảm biến...");

  byte trangThaiMPU = camBienMPU.begin();
  if (trangThaiMPU != 0) {
    Serial.println("❌ Không tìm thấy MPU6050!");
    while (1);
  }
  camBienMPU.calcGyroOffsets();
  Serial.println("✅ MPU6050 sẵn sàng");

  Serial.print("📡 Đang kiểm tra GPS");
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

  Blynk.virtualWrite(V3, "🛵 Không phát hiện tai nạn");
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
  float gocX = camBienMPU.getAccAngleX();
  float gocY = camBienMPU.getAccAngleY();

  // Cập nhật Acceleration (V4)
  Blynk.virtualWrite(V4, giaTocRong);

  // Luôn in Gia tốc ròng, Góc X, Góc Y
  Serial.print("Gia tốc ròng: ");
  Serial.print(giaTocRong);
  Serial.print(" | Góc X: ");
  Serial.print(gocX);
  Serial.print(" | Góc Y: ");
  Serial.println(gocY);

  // Kiểm tra GPS
  if (gps.location.isValid()) {
    Serial.println("✅ Đã cập nhật vị trí");

    float vanToc = gps.speed.kmph();
    String viDoStr = String(gps.location.lat(), 6);
    String kinhDoStr = String(gps.location.lng(), 6);
    String linkViTri = "https://maps.google.com/?q=" + viDoStr + "," + kinhDoStr;

    // Cập nhật Speed (V0)
    Blynk.virtualWrite(V0, (int)vanToc);

    // Cập nhật Location link (V6)
    Blynk.virtualWrite(V6, linkViTri);
  } else {
    Serial.println("⚠️ Không tìm thấy vị trí");
  }

  if (giaTocRong > nguongGiaToc && !dangCanhBao) {
    Serial.println("⚠️ Phát hiện gia tốc thay đổi lớn! Đang tiến hành theo dõi góc nghiêng trong 10 giây...");

    unsigned long batDauTheoDoi = millis();
    bool phatHienNghiengManh = false;

    while (millis() - batDauTheoDoi < 10000) {
      camBienMPU.update();
      while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
      }

      float gocX = abs(camBienMPU.getAngleX());
      float gocY = abs(camBienMPU.getAngleY());

      if (gocX > nguongDoNghieng || gocY > nguongDoNghieng) {
        phatHienNghiengManh = true;
        break;
      }
      delay(50);
    }

    if (phatHienNghiengManh) {
      Serial.println("✅ Phát hiện góc nghiêng lớn! Đang tiến hành kiểm tra duy trì trong 5s...");

      unsigned long batDauNghiengLienTuc = millis();
      bool duocDuyTri5s = true;

      while (millis() - batDauNghiengLienTuc < 5000) {
        camBienMPU.update();
        while (gpsSerial.available()) {
          gps.encode(gpsSerial.read());
        }

        float gocX = abs(camBienMPU.getAngleX());
        float gocY = abs(camBienMPU.getAngleY());

        if (gocX < nguongDoNghieng && gocY < nguongDoNghieng) {
          duocDuyTri5s = false;
          Serial.println("❌ Góc nghiêng đã bị gián đoạn, xác nhận không có tai nạn.");
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

    // Cập nhật Status (V3)
    Blynk.virtualWrite(V3, "🚨 Phát hiện tai nạn!");
  }

  if (dangCanhBao && millis() - thoiGianCanhBao > 30000) {
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
    taiNanDuocXacNhan = false;
    dangCanhBao = false;
    Serial.println("✅ Đã tắt cảnh báo.");

    // Reset Status (V3)
    Blynk.virtualWrite(V3, "🛵 Không phát hiện tai nạn");
  }

  delay(1000);
}
