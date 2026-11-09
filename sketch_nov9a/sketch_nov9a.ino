#include <Wire.h>
#include <DHT20.h>
#include <BH1750.h>

// ---------- Chân phần cứng ----------
const int mySDA = 4;
const int mySCL = 5;

const int led_temp  = 38;
const int led_hum   = 47;
const int led_light = 21;

// Chân GPIO cấp nguồn cho cảm biến (qua MOSFET/load switch).
// Chưa đấu dây kiểu này thì để -1, code sẽ bỏ qua power-cycle.
const int SENSOR_PWR_PIN = -1;

// ---------- Cấu hình ----------
const uint32_t I2C_FREQ       = 100000;
const int      MAX_FAIL_COUNT = 3;
const float    LUX_THRESHOLD  = 50.0;
const float    TEMP_THRESHOLD = 30.0;
const float    HUM_THRESHOLD  = 70.0;

BH1750 lightMeter;
DHT20 DHT;

int bhFailCount  = 0;
int dhtFailCount = 0;
bool bhReady     = false;

// ---------- Hàm hỗ trợ ----------

// Bơm 9 xung clock để slave đang kẹt nhả SDA, rồi gửi STOP
void i2cBusRecover() {
  Wire.end();
  pinMode(mySDA, INPUT_PULLUP);
  pinMode(mySCL, OUTPUT);

  for (int i = 0; i < 9; i++) {
    digitalWrite(mySCL, LOW);  delayMicroseconds(5);
    digitalWrite(mySCL, HIGH); delayMicroseconds(5);
  }

  // Điều kiện STOP
  pinMode(mySDA, OUTPUT);
  digitalWrite(mySDA, LOW);  delayMicroseconds(5);
  digitalWrite(mySCL, HIGH); delayMicroseconds(5);
  digitalWrite(mySDA, HIGH); delayMicroseconds(5);

  pinMode(mySDA, INPUT_PULLUP);
  Wire.begin(mySDA, mySCL, I2C_FREQ);
}

// Tắt/mở nguồn cảm biến (chỉ chạy nếu có chân PWR)
void sensorPowerCycle() {
  if (SENSOR_PWR_PIN < 0) return;
  digitalWrite(SENSOR_PWR_PIN, LOW);
  delay(500);
  digitalWrite(SENSOR_PWR_PIN, HIGH);
  delay(200);
}

bool initBH1750() {
  bhReady = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);
  return bhReady;
}

bool initDHT() {
  DHT.begin();
  return DHT.isConnected();
}

// Phục hồi: nguồn -> bus -> khởi tạo lại cảm biến
void recoverSensors() {
  Serial.println("[RECOVER] Dang phuc hoi cam bien...");
  sensorPowerCycle();
  i2cBusRecover();

  Serial.println(initBH1750() ? "[OK] BH1750 phuc hoi thanh cong"
                              : "[ERROR] BH1750 van loi");
  Serial.println(initDHT() ? "[OK] DHT phuc hoi thanh cong"
                           : "[ERROR] DHT van loi");

  bhFailCount = 0;
  dhtFailCount = 0;
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("\n=== KIEM TRA SENSOR & LED ESP32-S3 ===");

  pinMode(led_temp, OUTPUT);
  pinMode(led_hum, OUTPUT);
  pinMode(led_light, OUTPUT);

  // Chớp LED lúc khởi động
  digitalWrite(led_temp, HIGH);
  digitalWrite(led_hum, HIGH);
  digitalWrite(led_light, HIGH);
  delay(500);
  digitalWrite(led_temp, LOW);
  digitalWrite(led_hum, LOW);
  digitalWrite(led_light, LOW);

  // Cấp nguồn cảm biến qua GPIO (nếu có)
  if (SENSOR_PWR_PIN >= 0) {
    pinMode(SENSOR_PWR_PIN, OUTPUT);
    digitalWrite(SENSOR_PWR_PIN, LOW);
    delay(300);
    digitalWrite(SENSOR_PWR_PIN, HIGH);
    delay(200);
  }

  // Giải phóng bus nếu cảm biến đang kẹt từ lần trước
  i2cBusRecover();

  Serial.println(initBH1750() ? "[OK] BH1750 khoi tao thanh cong!"
                              : "[ERROR] Khong tim thay BH1750! Kiem tra day I2C/nguon.");
  Serial.println(initDHT() ? "[OK] DHT khoi tao thanh cong!"
                           : "[ko OK] DHT khoi tao ko thanh cong!");

  delay(1000);
}

// ---------- Loop ----------
void loop() {
  Serial.println("----------------------------------------");

  // --- BH1750 ---
  bool bhOk = false;
  if (bhReady && lightMeter.measurementReady()) {
    float lux = lightMeter.readLightLevel();
    if (lux >= 0) {                      // lux < 0 là lỗi đọc
      bhOk = true;
      bhFailCount = 0;
      Serial.print("Cuong do anh sang: ");
      Serial.print(lux);
      Serial.println(" lx");
      digitalWrite(led_light, (lux < LUX_THRESHOLD) ? HIGH : LOW);
    }
  }
  if (!bhOk) {
    bhFailCount++;
    Serial.print("Loi: Khong doc duoc BH1750 (lan ");
    Serial.print(bhFailCount);
    Serial.println(")");
  }

  // --- DHT20 ---
  int status = DHT.read();
  if (status == DHT20_OK) {
    dhtFailCount = 0;
    float temp = DHT.getTemperature();
    float hum  = DHT.getHumidity();

    Serial.print("Nhiet do: ");
    Serial.print(temp, 1);
    Serial.println(" *C");
    Serial.print("Do am:    ");
    Serial.print(hum, 1);
    Serial.println(" %");

    digitalWrite(led_temp, (temp > TEMP_THRESHOLD) ? HIGH : LOW);
    digitalWrite(led_hum,  (hum > HUM_THRESHOLD) ? HIGH : LOW);
  } else {
    dhtFailCount++;
    Serial.print("Loi doc DHT, ma loi: ");
    Serial.println(status);
  }

  // --- Tự phục hồi nếu lỗi liên tiếp ---
  if (bhFailCount >= MAX_FAIL_COUNT || dhtFailCount >= MAX_FAIL_COUNT) {
    recoverSensors();
  }

  delay(2000);
}