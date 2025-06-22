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
char ssid[] = "Choang";
char pass[] = "choang1313:)";

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
    if (millis() - batDau > 20000) {  // Timeout 20s
      Serial.println("\n❌ Kết nối WiFi thất bại, vui lòng kiểm tra mạng hoặc mật khẩu.");
      while (1);  // Dừng lại để debug
    }
  }

  Serial.println("\n✅ WiFi đã kết nối.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("⏳ Đang kết nối Blynk...");
  Blynk.begin(auth, ssid, pass);  // Dùng Blynk.begin để ESP32 tự giữ kết nối

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
  while (millis() - batDauGPS < 10000) {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
      if (gps.location.isValid()) {
        Serial.println("\n✅ GPS sẵn sàng");
        return;
      }
    }
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n⚠️ GPS không khả dụng trong 10 giây đầu");
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

  Serial.print("Gia tốc ròng: ");
  Serial.print(giaTocRong);
  Serial.print(" | Góc X: ");
  Serial.print(gocX);
  Serial.print(" | Góc Y: ");
  Serial.println(gocY);

  if (gps.location.isValid()) {
    float vanToc = gps.speed.kmph();
    float kinhDo = gps.location.lng();
    float viDo = gps.location.lat();

    String vanTocStr = String((int)vanToc);
    String kinhDoStr = String(kinhDo, 6);
    String viDoStr = String(viDo, 6);
    String linkViTri = "https://maps.google.com/?q=" + viDoStr + "," + kinhDoStr;

    Blynk.virtualWrite(V0, vanTocStr.toInt());
    Blynk.virtualWrite(V1, kinhDoStr);
    Blynk.virtualWrite(V2, viDoStr);
    Blynk.virtualWrite(V3, linkViTri);
  }

  if (giaTocRong > nguongGiaToc && !dangCanhBao) {
    Serial.println("⚠️ Gia tốc lớn! Theo dõi góc nghiêng 10 giây...");

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
      Serial.println("✅ Góc nghiêng lớn phát hiện! Kiểm tra duy trì 5s...");

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
          Serial.println("❌ Góc nghiêng bị gián đoạn!");
          break;
        }
        delay(100);
      }

      if (duocDuyTri5s) {
        taiNanDuocXacNhan = true;
        Serial.println("🚨 Tai nạn đã xác nhận!");
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

    String msg = "🚨 Tai nạn được phát hiện!\n";

    if (gps.location.isValid()) {
      msg += "📍 Vị trí: https://maps.google.com/?q=" +
             String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
    } else {
      msg += "⚠️ Không có tín hiệu GPS.";
    }

    if (gps.date.isValid() && gps.time.isValid()) {
      int gioVN = (gps.time.hour() + 7) % 24;
      char thoiGianStr[30];
      sprintf(thoiGianStr, "%02d/%02d/%04d %02d:%02d:%02d",
              gps.date.day(), gps.date.month(), gps.date.year(),
              gioVN, gps.time.minute(), gps.time.second());
      msg += "\n⏰ Thời điểm: " + String(thoiGianStr);
    } else {
      msg += "\n⏰ Thời điểm: không xác định";
    }

    Blynk.logEvent("phat_hien_tai_nan", msg);
    Blynk.virtualWrite(V1, msg);
  }

  if (dangCanhBao && millis() - thoiGianCanhBao > 30000) {
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
    taiNanDuocXacNhan = false;
    dangCanhBao = false;
    Serial.println("✅ Đã tắt cảnh báo.");
  }

  delay(1000);
}
