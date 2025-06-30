#define BLYNK_TEMPLATE_ID "TMPL6LZ4sAz_g"
#define BLYNK_TEMPLATE_NAME "canhbao"
#define BLYNK_AUTH_TOKEN "tSj2GkQsaSLXVIS-B65MM2k3lVwK2Fhj"

#include <Wire.h>
#include <MPU6050_light.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <HardwareSerial.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <math.h>

// ===== CẤU HÌNH WIFI VÀ BLYNK =====
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Nam Khanh #T1";
char pass[] = "11113333";

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

// ✅ Google Geolocation API key
String apiKey = "AIzaSyCBqT3yAgFrW8VViAfpVJDP5SDtz5PrJ5c";

// Hàm định vị bằng Google Geolocation API (bỏ log chi tiết)
String getLocationByGoogle() {
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("⚠️ Không tìm thấy WiFi để gửi Google");
    return "";
  }

  String payload = "{ \"wifiAccessPoints\": [";
  for (int i = 0; i < n; ++i) {
    if (i != 0) payload += ",";
    payload += "{";
    payload += "\"macAddress\": \"" + WiFi.BSSIDstr(i) + "\",";
    payload += "\"signalStrength\": " + String(WiFi.RSSI(i));
    payload += "}";
  }
  payload += "]}";

  HTTPClient http;
  String url = "https://www.googleapis.com/geolocation/v1/geolocate?key=" + apiKey;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(payload);
  String response = http.getString();
  http.end();

  if (httpResponseCode > 0) {
    int latIndex = response.indexOf("\"lat\":");
    int lngIndex = response.indexOf("\"lng\":");
    if (latIndex != -1 && lngIndex != -1) {
      float lat = response.substring(latIndex + 6, response.indexOf(",", latIndex)).toFloat();
      float lng = response.substring(lngIndex + 6, response.indexOf("\n", lngIndex)).toFloat();
      String link = "https://maps.google.com/?q=" + String(lat, 6) + "," + String(lng, 6);
      Serial.println("✅ Lấy vị trí từ Google thành công");
      return link;
    }
  }

  Serial.println("❌ Không lấy được vị trí từ Google");
  return "";
}

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

  // Ban đầu status báo không có tai nạn
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

  Blynk.virtualWrite(V4, giaTocRong);

  Serial.print("Gia tốc ròng: ");
  Serial.println(giaTocRong);

  // Lấy vị trí GPS hoặc Google
  String linkViTri = "";
  if (gps.location.isValid()) {
    float vanToc = gps.speed.kmph();
    linkViTri = "https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
    Blynk.virtualWrite(V0, (int)vanToc);
  } else {
    linkViTri = getLocationByGoogle();
  }

  // Luôn cập nhật Location lên V3
  if (linkViTri != "") {
    Blynk.virtualWrite(V3, linkViTri);
  }

  // Phát hiện tai nạn
  if (giaTocRong > nguongGiaToc && !dangCanhBao) {
    Serial.println("⚠️ Phát hiện gia tốc lớn! Đang kiểm tra góc nghiêng...");

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
      Serial.println("✅ Góc nghiêng lớn! Kiểm tra duy trì 5s...");
      unsigned long batDauNghiengLienTuc = millis();
      bool duocDuyTri5s = true;

      while (millis() - batDauNghiengLienTuc < 5000) {
        camBienMPU.update();
        while (gpsSerial.available()) gps.encode(gpsSerial.read());
        float gocX = abs(camBienMPU.getAngleX());
        float gocY = abs(camBienMPU.getAngleY());
        if (gocX < nguongDoNghieng && gocY < nguongDoNghieng) {
          duocDuyTri5s = false;
          Serial.println("❌ Góc nghiêng không duy trì, hủy tai nạn.");
          break;
        }
        delay(100);
      }

      if (duocDuyTri5s) {
        taiNanDuocXacNhan = true;
        Serial.println("🚨 Xác nhận tai nạn!");
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
