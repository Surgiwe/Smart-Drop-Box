#include <Surgiwe-project-1_inferencing.h>

#include <WiFi.h>
#include <WiFiMulti.h> 
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <I2CKeyPad.h> 

// =========================================================================
// 1. INTEGRASI LIBRARY EDGE IMPULSE & FIREBASE
// =========================================================================
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"

#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// =========================================================================
// 2. KONFIGURASI KREDENSIAL TELEGRAM & FIREBASE
// =========================================================================
#define BOT_TOKEN       "8851500079:AAGThBLprpEnIafJuZ9tTcHJIrSKjN7yuLk" 
#define CHAT_ID         "8661427015"                                     

#define FIREBASE_HOST   "smartdropbox-866c3-default-rtdb.asia-southeast1.firebasedatabase.app" 
#define FIREBASE_AUTH   "ogegu59M01oiEXoukVwLpxYOOwoHgggVcGL3jLLD"

WiFiClientSecure net_client;
UniversalTelegramBot bot(BOT_TOKEN, net_client);
WiFiMulti wifiMulti; 

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configFirebase;

// =========================================================================
// 3. KONFIGURASI PIN 
// =========================================================================
#define I2C_SDA          14
#define I2C_SCL          15
#define RELAY_PIN        13         
#define DOOR_SENSOR_PIN  12         
#define SERVO_LATCH_PIN   2         
#define LED_PIRANHA_PIN   4         

#define KEYPAD_I2C_ADDR  0x20       
LiquidCrystal_I2C lcd(0x27, 16, 2); 
I2CKeyPad customKeypad(KEYPAD_I2C_ADDR); 

char keymap[19] = "DCBA#9630852*741"; 

// Pinout Kamera AI-Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

Servo latchServo;
String inputPIN = "";      
uint8_t *snapshot_buf = NULL;

int lastWifiStatus = -1;

void resetLcdHardware();
void prosesScanDanDrop();
void prosesBukaPaksa();
bool cekOtpFirebase(String pinInput);
void text_pembuka_aman();

void resetLcdHardware() {
  Wire.end();                   // Matikan bus I2C sementara
  delay(50);
  Wire.begin(I2C_SDA, I2C_SCL); // Inisialisasi ulang Hardware I2C
  delay(50);
  lcd.init();
  lcd.backlight();
  lcd.clear();
}

bool kirimFotoKeTelegram(camera_fb_t* fb_photo, String pesan) {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  net_client.setInsecure();               
  net_client.setHandshakeTimeout(10000); 
  
  String nama_file = "paket_capture.jpg";
  String boundary = "SurgiweDropBoxAI";
  
  String body_chat_id = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(CHAT_ID) + "\r\n";
  String body_caption = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"caption\"\r\n\r\n" + pesan + "\r\n";
  String body_parse_mode = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"parse_mode\"\r\n\r\nMarkdown\r\n";
  String body_pembuka = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"" + nama_file + "\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String body_penutup = "\r\n--" + boundary + "--\r\n";
  
  size_t total_panjang_data = body_chat_id.length() + body_caption.length() + body_parse_mode.length() + body_pembuka.length() + fb_photo->len + body_penutup.length();
  
  if (!net_client.connect("api.telegram.org", 443)) return false;

  net_client.println("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1");
  net_client.println("Host: api.telegram.org");
  net_client.println("Content-Length: " + String(total_panjang_data));
  net_client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  net_client.println();
  
  net_client.print(body_chat_id);
  net_client.print(body_caption); 
  net_client.print(body_parse_mode);
  net_client.print(body_pembuka);
  
  uint8_t *fb_buffer = fb_photo->buf;
  size_t fb_len = fb_photo->len;
  size_t sisa_buffer = fb_len;
  
  while (sisa_buffer > 0) {
    size_t kirim_sekarang = (sisa_buffer < 4096) ? sisa_buffer : 4096;
    net_client.write(fb_buffer, kirim_sekarang);
    fb_buffer += kirim_sekarang;
    sisa_buffer -= kirim_sekarang;
  }
  
  net_client.print(body_penutup);
  
  while (net_client.connected() && net_client.available()) {
    net_client.readStringUntil('\n'); 
  }
  net_client.stop(); 
  
  return true;
}

void setup() {
  Serial.begin(115200);
  
  resetLcdHardware();
  
  if (customKeypad.begin()) {
    customKeypad.loadKeyMap(keymap);
  }
  
  lcd.setCursor(0, 0); lcd.print("Smart Drop Box");
  lcd.setCursor(0, 1); lcd.print("Booting System..");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);            
  
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);  

  ESP32PWM::allocateTimer(0);
  latchServo.setPeriodHertz(50); 
  latchServo.attach(SERVO_LATCH_PIN, 500, 2500); 
  latchServo.write(90);          
  delay(200);

  // =========================================================================
  // KONFIGURASI WIFI
  // =========================================================================
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); 
  WiFi.setSleep(false);        

  wifiMulti.addAP("Hey yo!", "Limwaifiluv27");
  wifiMulti.addAP("Qwerty", "0123456789");               
        
  Serial.println("\n[WIFI] Memulai pencarian jaringan Multi-SSID...");
  lcd.setCursor(0, 1); lcd.print("Connecting WiFi.");

  int timeout_counter = 0;
  while (wifiMulti.run() != WL_CONNECTED && timeout_counter < 30) {
    delay(500);
    timeout_counter++;
    Serial.print(".");
  }

  lcd.clear();

  if (WiFi.status() == WL_CONNECTED) {
    String connectedSSID = WiFi.SSID();
    Serial.println("\n[WIFI] Terhubung ke: " + connectedSSID);
    
    lcd.setCursor(0, 0); lcd.print("WiFi Connected!");
    if(connectedSSID.length() > 16) connectedSSID = connectedSSID.substring(0, 16);
    lcd.setCursor(0, 1); lcd.print(connectedSSID);
    delay(1500); 
    
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Connect Database");
    
    configFirebase.database_url = FIREBASE_HOST;
    configFirebase.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.reconnectWiFi(true);
    Firebase.begin(&configFirebase, &auth);
    
    if (Firebase.ready()) {
      bot.sendMessage(CHAT_ID, "🤖 Smart Drop Box AI Online!", "");
      Serial.println("[SYSTEM] Booting Selesai. System Ready.");
    }
  } else {
    Serial.println("\n[WIFI] Gagal Terhubung. Berjalan dalam mode lokal.");
    lcd.setCursor(0, 0); lcd.print("WiFi Timeout!");
    lcd.setCursor(0, 1); lcd.print("Bypass Mode OS");
    delay(1500);
  }

  pinMode(LED_PIRANHA_PIN, OUTPUT);
  digitalWrite(LED_PIRANHA_PIN, LOW); 

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM; 
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 

  config.frame_size = FRAMESIZE_QQVGA; 
  config.jpeg_quality = 15;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    lcd.clear(); 
    lcd.print("Cam Init Error!");
    Serial.println("[ERROR] Inisialisasi Kamera Gagal!");
    while(true);
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_awb_gain(s, 1);     
    s->set_whitebal(s, 1);     
    s->set_exposure_ctrl(s, 1); 
    s->set_gain_ctrl(s, 1);     
    s->set_brightness(s, 0);    
    s->set_contrast(s, 1);      
    s->set_saturation(s, 0);    
  }
  
  lcd.clear();
}

void loop() {
  if (Serial.available() > 0) {
    String perintah = Serial.readStringUntil('\n');
    perintah.trim(); perintah.toLowerCase(); 
    if (perintah == "restart" || perintah == "reset") {
      Serial.println("[SYSTEM] Perintah Restart diterima via Serial Monitor");
      delay(500); ESP.restart(); 
    }
  }

  // =========================================================================
  // TAHAP 1: STANDBY TOTAL & MONITORING KONEKSI
  // =========================================================================
  digitalWrite(RELAY_PIN, LOW);       
  digitalWrite(LED_PIRANHA_PIN, LOW); 
  latchServo.write(90);             
  inputPIN = ""; 

  int currentWifiStatus = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
  if (currentWifiStatus != lastWifiStatus) {
    resetLcdHardware();
    lcd.setCursor(0, 0); lcd.print("SMART DROP BOX  ");
    lcd.setCursor(0, 1); 
    if (currentWifiStatus == 1) {
      lcd.print("Tekan A: Start  ");
    } else {
      lcd.print("A:Start [OFFLINE]");
    }
    lastWifiStatus = currentWifiStatus;
  }

  while (true) {
    if (Serial.available() > 0) return; 

    if (WiFi.status() != WL_CONNECTED) { 
      wifiMulti.run(); 
    }

    if (digitalRead(DOOR_SENSOR_PIN) == HIGH) {
      prosesBukaPaksa();
      lastWifiStatus = -1;
      return; 
    }

    if (customKeypad.isPressed()) {
      char keyAwal = customKeypad.getChar();
      
      if (keyAwal == 'A') { 
        Serial.println("[LOG] Tombol A ditekan - Memulai mode input PIN");
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Sistem Aktif!   ");
        delay(500);
        break; 
      }
      else if (keyAwal == 'B') { 
        Serial.println("[LOG] Tombol B ditekan - Tes Trapdoor (Buka & Tutup)");
        
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Tes Trapdoor... ");
        lcd.setCursor(0, 1); lcd.print("TrapDoor Terbuka");
        latchServo.write(0);  
        delay(1500); 
        
        lcd.setCursor(0, 1); lcd.print("TrapDoor Menutup");
        latchServo.write(90);
        delay(800);
        
        while(customKeypad.isPressed()) { 
          delay(10); 
        }
        
        resetLcdHardware();
        lastWifiStatus = -1; 
        return; 
      }
      
      while(customKeypad.isPressed()){ delay(10); }
    }
    delay(50); 
  }

  // =========================================================================
  // TAHAP 2: PROSES INPUT PIN (15 Detik Timeout)
  // =========================================================================
  unsigned long waktuMulaiInput = millis(); 
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Masukan PIN:    ");
  
  while (true) {
    lcd.setCursor(0, 1); 
    lcd.print(inputPIN + "                ");

    if (millis() - waktuMulaiInput > 15000) {
      Serial.println("[LOG] Timeout input PIN (15 detik habis)");
      lcd.clear(); lcd.print("Waktu Habis!    ");
      delay(1500); 
      lcd.clear();
      lastWifiStatus = -1;
      return; 
    }

    if (customKeypad.isPressed()) {
      char key = customKeypad.getChar();
      
      if (key != I2C_KEYPAD_NOKEY) {
        waktuMulaiInput = millis(); 
        
        if (key == '#') { 
          Serial.println("[LOG] Validasi PIN OTP: " + inputPIN);
          lcd.clear(); 
          lcd.setCursor(0, 0); lcd.print("Validasi OTP... ");

          if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[LOG] Validasi Gagal - Device Offline");
            lcd.clear(); 
            lcd.setCursor(0, 0); lcd.print("DEVICE OFFLINE ");
            lcd.setCursor(0, 1); lcd.print("Cek Koneksi WiFi");
            delay(2000);
            lastWifiStatus = -1;
            break;
          }

          if (cekOtpFirebase(inputPIN)) { 
            Serial.println("[LOG] PIN Valid! Membuka Relay Solenoid");
            
            digitalWrite(RELAY_PIN, HIGH); 
            delay(150); 

            resetLcdHardware();
            lcd.setCursor(0, 0); lcd.print("PIN BENAR!      ");
            lcd.setCursor(0, 1); lcd.print("Silakan Buka Box");

            while (digitalRead(DOOR_SENSOR_PIN) == LOW) { 
              delay(50); 
            }
            Serial.println("[LOG] Pintu atas dibuka oleh Kurir");
            
            digitalWrite(LED_PIRANHA_PIN, HIGH);

            resetLcdHardware();
            text_pembuka_aman();
            
            while (digitalRead(DOOR_SENSOR_PIN) == HIGH) { 
              delay(50); 
            }
            Serial.println("[LOG] Pintu atas ditutup kembali oleh Kurir");
            
            digitalWrite(RELAY_PIN, LOW); 
            delay(100);
            
            resetLcdHardware(); 
            prosesScanDanDrop(); 
            lastWifiStatus = -1;
            break; 
          } else {
            Serial.println("[LOG] PIN Salah / Expired: " + inputPIN);
            lcd.clear(); 
            lcd.setCursor(0, 0); lcd.print("PIN SALAH/EXPIRED");
            delay(2000);
            inputPIN = ""; 
            lcd.clear();
            lcd.setCursor(0, 0); lcd.print("Masukan PIN:    ");
          }
        } 
        else if (key == '*') { 
          inputPIN = ""; 
        } 
        else {
          if (inputPIN.length() < 4 && key >= '0' && key <= '9') { 
            inputPIN += key; 
          }
        }
        while (customKeypad.isPressed()) { delay(10); } 
      }
    }
    delay(30);
  }
}

bool cekOtpFirebase(String pinInput) {
  QueryFilter query;
  query.orderBy("otp_keypad");
  query.equalTo(pinInput);
  query.limitToFirst(1);

  if (Firebase.RTDB.getJSON(&fbdo, "/otp_resi", &query)) {
    if (fbdo.dataType() == "json" && fbdo.jsonString() != "null" && fbdo.jsonString() != "{}") {
      FirebaseJson &json = fbdo.jsonObject();
      size_t len = json.iteratorBegin();
      String resiKey, value;
      int type = 0;
      String targetStatusPath = "";
      bool statusActive = false;

      if (len > 0) {
        json.iteratorGet(0, type, resiKey, value);
        FirebaseJsonData jsonData;
        json.get(jsonData, resiKey + "/status");
        if (jsonData.success && jsonData.stringValue == "active") {
          statusActive = true;
          targetStatusPath = "/otp_resi/" + resiKey + "/status";
        }
      }
      json.iteratorEnd();

      if (statusActive && targetStatusPath != "") {
        if (Firebase.RTDB.setString(&fbdo, targetStatusPath, "used")) {
          return true;
        }
      }
    }
  }
  return false; 
}

void prosesScanDanDrop() {
  Serial.println("[LOG] Memulai pemindaian AI Edge Impulse...");
  
  lcd.setCursor(0, 0); lcd.print("Pintu Terkunci! ");
  lcd.setCursor(0, 1); lcd.print("Memindai AI.... ");
  
  digitalWrite(LED_PIRANHA_PIN, HIGH);
  delay(1500); 

  for (int i = 0; i < 2; i++) {
    camera_fb_t* fb_dummy = esp_camera_fb_get();
    if (fb_dummy) esp_camera_fb_return(fb_dummy);
    delay(50);
  }

  camera_fb_t* fb = esp_camera_fb_get(); 

  if (!fb) {
    Serial.println("[ERROR] Kamera gagal mengambil gambar!");
    lcd.setCursor(0, 1); lcd.print("Kamera Malfungsi");
    digitalWrite(LED_PIRANHA_PIN, LOW); 
    delay(2000);
    return;
  }

  size_t cam_w = 160;
  size_t cam_h = 120;
  snapshot_buf = (uint8_t *)ps_malloc(cam_w * cam_h * 3);
  
  bool paketValid = false; 
  String detailSkorAI = "📊 *Detail Klasifikasi AI:*\n"; 

  if (snapshot_buf != NULL) {
    if (fmt2rgb888(fb->buf, fb->len, fb->format, snapshot_buf)) {
      signal_t signal;
      signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
      
      signal.get_data = [](size_t offset, size_t length, float *out_ptr) -> int {
        size_t target_w = EI_CLASSIFIER_INPUT_WIDTH;
        size_t cam_w_st = 160;
        size_t start_x = (cam_w_st > target_w) ? (cam_w_st - target_w) / 2 : 0;
        size_t start_y = (120 > EI_CLASSIFIER_INPUT_HEIGHT) ? (120 - EI_CLASSIFIER_INPUT_HEIGHT) / 2 : 0;

        for (size_t i = 0; i < length; i++) {
          size_t total_offset = offset + i;
          size_t target_y_idx = total_offset / target_w;
          size_t target_x_idx = total_offset % target_w;
          size_t src_x = start_x + target_x_idx;
          size_t src_y = start_y + target_y_idx;
          size_t pixel_idx = (src_y * cam_w_st) + src_x;

          out_ptr[i] = (snapshot_buf[pixel_idx * 3] << 16) | 
                       (snapshot_buf[pixel_idx * 3 + 1] << 8) | 
                       snapshot_buf[pixel_idx * 3 + 2];
        }
        return 0;
      };

      ei_impulse_result_t result = { 0 };
      EI_IMPULSE_ERROR ei_err = run_classifier(&signal, &result, false);
      
      float scorePaket = 0.0;
      float scoreBukanPaket = 0.0;
      float scoreKosong = 0.0;

      if (ei_err == EI_IMPULSE_OK) {
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
          String labelName = String(result.classification[ix].label);
          float score = result.classification[ix].value;
          labelName.toLowerCase();

          if (labelName.indexOf("bukan") >= 0) {
            scoreBukanPaket = score;
          } else if (labelName.indexOf("kosong") >= 0) {
            scoreKosong = score;
          } else if (labelName.indexOf("paket") >= 0) {
            scorePaket = score;
          }
        }

        // Menggabungkan probabilitas bukan paket dan kosong
        float totalBukanPaket = scoreBukanPaket + scoreKosong;
        if (totalBukanPaket > 1.0) totalBukanPaket = 1.0;

        int persenPaket = scorePaket * 100;
        int persenBukanPaket = totalBukanPaket * 100;

        // Hanya mencetak 2 kelas ke teks Telegram
        detailSkorAI += "- paket: " + String(persenPaket) + "%\n";
        detailSkorAI += "- bukanpaket: " + String(persenBukanPaket) + "%\n";

        if (scorePaket > totalBukanPaket && scorePaket >= 0.50) {
          paketValid = true;
          detailSkorAI += "\n📌 *Kesimpulan:* AI mendeteksi *PAKET*";
          Serial.println("[LOG] Kesimpulan AI: PAKET");
        } else {
          paketValid = false;
          detailSkorAI += "\n📌 *Kesimpulan:* AI mendeteksi *BUKAN PAKET*";
          Serial.println("[LOG] Kesimpulan AI: BUKAN PAKET");
        }
      }
    }
    free(snapshot_buf); 
    snapshot_buf = NULL; 
  }

  String pesanTelegram = "";
  
  if (paketValid) {
    Serial.println("[LOG] Hasil AI: Paket Valid -> Membuka Trapdoor");
    lcd.setCursor(0, 1); lcd.print("Paket Terdeteksi");
    pesanTelegram = "📦 *[INFO] PAKET VALID TERDETEKSI*\n\n" + detailSkorAI + "\n\nMenjatuhkan ke kompartemen bawah...";
    kirimFotoKeTelegram(fb, pesanTelegram);
    
    digitalWrite(LED_PIRANHA_PIN, LOW); 
    
    lcd.setCursor(0, 1); lcd.print("TrapDoor: Buka  ");
    latchServo.write(0);  
    delay(4000);            
    
    lcd.setCursor(0, 1); lcd.print("TrapDoor: Kunci ");
    latchServo.write(90);  
    delay(1000);            
    lcd.setCursor(0, 1); lcd.print("Drop Sukses!    ");
    Serial.println("[LOG] Proses Drop Paket Selesai.");
  } else {
    Serial.println("[LOG] Hasil AI: Bukan Paket (Laci Kosong atau Benda Asing)");
    lcd.setCursor(0, 1); lcd.print("Bukan Paket!    ");
    pesanTelegram = "⚠️ *[PERINGATAN] BUKAN PAKET*\n\n" + detailSkorAI + "\n\nObjek tidak valid atau laci kosong. Trapdoor tidak dibuka.";
    kirimFotoKeTelegram(fb, pesanTelegram);
    
    digitalWrite(LED_PIRANHA_PIN, LOW); 
    delay(2000);
  }
  
  esp_camera_fb_return(fb); 
  delay(1000); 
  lcd.clear();
}

void prosesBukaPaksa() {
  Serial.println("[ALARM] AKSES DIPAKSA! Sensor pintu terbuka secara ilegal!");
  
  digitalWrite(RELAY_PIN, LOW); 
  delay(100);
  
  resetLcdHardware();
  lcd.setCursor(0, 0); lcd.print("!! ALERT !!     ");
  lcd.setCursor(0, 1); lcd.print("AKSES DIPAKSA!! ");
  
  digitalWrite(LED_PIRANHA_PIN, HIGH);
  delay(1000); 

  camera_fb_t* fb_dummy = esp_camera_fb_get();
  if (fb_dummy) esp_camera_fb_return(fb_dummy);
  delay(100);

  camera_fb_t* fb_pencuri = esp_camera_fb_get();
  if (fb_pencuri) {
    String pesanDarurat = "🚨 *[BAHAYA] KOTAK DIBUKA PAKSA!*";
    kirimFotoKeTelegram(fb_pencuri, pesanDarurat);
    esp_camera_fb_return(fb_pencuri);
  }
  
  digitalWrite(LED_PIRANHA_PIN, LOW); 
  
  while (digitalRead(DOOR_SENSOR_PIN) == HIGH) {
    lcd.noBacklight(); delay(200);
    lcd.backlight(); delay(200);
  }
  lcd.clear();
}

void text_pembuka_aman() {
  lcd.setCursor(0, 0); lcd.print("Pintu Terbuka... ");
  lcd.setCursor(0, 1); lcd.print("Tutup Jika Sudah");
}