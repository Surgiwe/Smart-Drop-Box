# 📦 Rancang Bangun Smart Drop Box Penerima Paket Otomatis Berbasis TinyML pada ESP32-CAM dengan Sistem Keamanan OTP

Repository ini berisi kode sumber (source code) dan dokumentasi proyek Tugas Akhir dari Program Studi Teknik Komputer, Fakultas Teknologi Informasi, Institut Teknologi Batam (ITEBA) Tahun 2026.

---

## 🌟 Deskripsi Proyek
Sistem **Smart Drop Box** ini dirancang untuk mengatasi permasalahan keamanan saat kurir mengantar paket ke rumah dalam kondisi kosong atau ditinggal pemilik. Alat ini mengintegrasikan teknologi *Internet of Things* (IoT) dan *Artificial Intelligence* edge computing (**TinyML**) secara *on-device* menggunakan mikrokontroler **ESP32-CAM**.

### 🛠️ Fitur Utama:
1. **Sistem Keamanan OTP Dinamis:** Akses pintu luar dikontrol menggunakan Keypad 4x4 (I2C) yang divalidasi secara *real-time* ke Firebase berdasarkan 3 digit terakhir nomor resi pengiriman.
2. **Deteksi Objek Visual (TinyML):** Memanfaatkan kamera OV2640 dan model CNN terkuantisasi (INT8) untuk mengklasifikasikan kondisi interior boks menjadi kategori **Paket** atau **Bukan Paket** tanpa bergantung pada server cloud.
3. **Mekanisme Trap Door Otomatis:** Jika paket terdeteksi valid secara visual, motor servo akan membuka sekat interior (*trap door*) agar paket meluncur aman ke ruang penyimpanan bawah.
4. **Notifikasi Telegram Bot:** Mengirimkan laporan status log aktivitas beserta lampiran foto bukti transaksi secara *real-time* ke Telegram pemilik rumah.

---

## 🔌 Skema Rangkaian Kelistrikan (*Schematic Diagram*)
Berikut adalah skema perancangan jalur kelistrikan keseluruhan sistem yang menghubungkan mikrokontroler ESP32-CAM dengan modul sensor, aktuator, dan catu daya:

> <img width="1920" height="1080" alt="skematik" src="https://github.com/user-attachments/assets/b1aee7f1-a563-485b-88d4-53c358198b73" />

*Catatan Sistem Daya:* Jalur kelistrikan dibagi menjadi dua sirkuit paralel menggunakan *Buck Converter* (12V ke 5V) untuk mengisolasi arus komponen digital dari beban induktif solenoid dan motor servo.

---

## 📷 Dokumentasi Prototipe Fisik
Berikut adalah bentuk fisik dari prototipe *Smart Drop Box* yang telah dirancang dan diuji coba:

> <img width="2296" height="4080" alt="Prototipe" src="https://github.com/user-attachments/assets/ddf3ea0d-3b0f-4d43-96c7-2372b4306d74" />

---

## ⚙️ Komponen Perangkat Keras (*Hardware*)
- Mikrokontroler **ESP32-CAM** (ESP-32S + Kamera OV2640)
- Solenoid Door Lock 12V DC
- Motor Servo MG996R (Metal Gear High Torque)
- Keypad Matriks 4x4 + Modul Konverter I2C (PCF8574)
- Display LCD 16x2 dengan Modul I2C
- Magnetic Door Switch (Sensor Pintu MC-38)
- Modul Relay 2-Channel 5V
- Buck Converter LM2596 (Step-down)
- LED Piranha (Pencahayaan Interior Aktif)
- Power Supply 12V 5A

---

## 💻 Pustaka Pendukung (*Arduino Libraries*)
Pastikan pustaka berikut terinstal di **Arduino IDE** Anda sebelum melakukan *compile* program utama (`SmartDropBox.ino`):
- `esp_camera.h`
- `WiFi.h` & `WiFiClientSecure.h`
- `UniversalTelegramBot`
- `Firebase_ESP_Client`
- `LiquidCrystal_I2C`
- `ESP32Servo`
- `I2CKeyPad`
- Pustaka Model Inferensi Edge Impulse (`Surgiwe-project-1_inferencing.h`)

---

## 📄 Lisensi & Hak Cipta
Proyek Tugas Akhir ini dikembangkan sebagai salah satu syarat untuk memperoleh gelar Sarjana Teknik Komputer (S.T.) pada Program Studi Teknik Komputer, Fakultas Teknologi Informasi, Institut Teknologi Batam (ITEBA) Tahun 2026.
