# Car Distance & Shock Alarm (Arduino UNO)

[English](README.md) · Bahasa Indonesia

Sistem alarm untuk mobil berbasis Arduino UNO yang:

1. **Mengukur jarak ke kendaraan di depan** (HC-SR04). Jika terlalu dekat → alarm.
2. **Mendeteksi guncangan / benturan / rem mendadak** (MPU6050). Jika terdeteksi → alarm.
3. Setiap alarm ditampilkan lewat **buzzer, LED, LCD 16x2 (I2C)** dan **dikirim ke HP via Bluetooth**.

> Status (12/10/2026): **FINAL.** Fase 0-3 dan 5 selesai; semua modul, firmware utama, dan app Android BLE lulus uji di meja (bukti: [docs/test-results.md](docs/test-results.md)). Fase 4 (uji di mobil) dilewati atas keputusan pengguna; checklist untuk melanjutkan ada di [docs/phase4-checklist.md](docs/phase4-checklist.md). Lihat [PLAN.md](PLAN.md) dan [CLAUDE.md](CLAUDE.md).

---

## Struktur repositori, web console, dan rilis

| Folder | Isi |
|---|---|
| `firmware/` | Sketch Arduino UNO (`car_alarm`) |
| `app/` | App Android (Kotlin, BLE), lihat `app/README.md` |
| `console/` | Web console (monitoring, test IO, kalibrasi, learning, setting lewat Web Serial), lihat `console/README.md` |
| `tests/` | Sketch uji per modul hardware |
| `docs/` | Bukti uji, wiring, logo, checklist |
| `.github/workflows/` | `ci.yml` (uji), `pages.yml` (deploy console ke GitHub Pages), `release.yml` (hex firmware + APK + zip console) |

**Web console:** <https://skillshub-electronics.github.io/car-alarm/> (setelah repo dipublikasikan). Setelah repo dibuat, aktifkan *Settings → Pages → Source: GitHub Actions*; setiap push ke `main` yang mengubah `console/` men-deploy halaman itu. Versi hosted memakai Web Serial (Chrome/Edge); Compile & Upload tersedia lewat server lokal (`console/server`); tanpa server lokal, halaman hosted punya **Cloud build** (Settings): GitHub Actions meng-compile lewat `build-firmware.yml`, lalu halaman mem-flash hasilnya lewat Web Serial (UNO dan ESP32; butuh token GitHub fine-grained milik sendiri, lihat [console/README.md](console/README.md)).

**Rilis:** buat tag `vX.Y.Z` (mis. `git tag v2.1.0 && git push --tags`); workflow `release.yml` membangun firmware, APK (ditandatangani kunci debug), dan zip console, lalu melampirkannya ke GitHub Release. Lisensi: MIT ([LICENSE](LICENSE)).

---

## 1. Komponen

| Komponen | Fungsi | Antarmuka |
|---|---|---|
| Arduino UNO (ATmega328P, 5V) | Otak sistem | - |
| HC-SR04 | Ukur jarak depan (2-400 cm, sudut ~15°) | Digital: Trig, Echo |
| MPU6050 (GY-521) | Akselerometer + giroskop 3 sumbu | I2C (0x68) |
| Modul Bluetooth berlabel "HC-05" (ternyata BLE-only, lihat keputusan D1) | Kirim notifikasi ke HP | UART 9600 (SoftwareSerial), BLE GATT `FFE0/FFE1` |
| LCD 16x2 + backpack I2C | Tampilan status | I2C (0x27 atau 0x3F) |
| Buzzer | Alarm suara | Digital via transistor |
| LED (+ resistor) | Alarm visual | Digital |
| Transistor NPN (2N2222 / BC547), resistor | Driver buzzer, pembatas arus LED, pembagi tegangan Bluetooth | - |

## 2. Cara kerja (ringkas)

```
HC-SR04 --> jarak (cm) ----+
                           +--> Logika status --> Buzzer / LED / LCD / Bluetooth
MPU6050 --> akselerasi ----+
```

### Status sistem

| Status | Pemicu | Buzzer | LED | LCD | Bluetooth |
|---|---|---|---|---|---|
| NORMAL | Jarak aman, tidak ada guncangan | Mati | Hijau / mati | "SAFE" + "Dist: 124cm" | Status berkala (opsional) |
| WASPADA | Jarak masuk batas peringatan | Beep lambat | Kedip lambat | "CLOSE DISTANCE" | `WARN,DIST,<cm>` |
| BAHAYA | Jarak masuk batas kritis | Beep cepat | Kedip cepat | "DANGER: CLOSE!" | `ALARM,DIST,<cm>` |
| GUNCANGAN | Lonjakan akselerasi melebihi ambang | Bunyi panjang | Nyala / kedip | "IMPACT!" | `ALARM,SHOCK,<g>` |
| REM MENDADAK | Perlambatan searah gerak melebihi ambang | Bunyi panjang | Nyala / kedip | "HARD BRAKING!" | `ALARM,BRAKE,<g>` |

Teks LCD berbahasa Inggris (maks 16 karakter per baris); baris 2: `Dist:%4dcm`, ditambah `ERR` bila MPU6050 tidak terbaca. Pesan boot: "Car Alarm" / "Calibrating MPU" lalu "Ready" atau "MPU not found!". Level jarak naik **seketika** saat lebih dekat, tetapi turun ke level lebih aman hanya setelah kondisi aman bertahan **1 detik** (`DIST_RELEASE_HOLD_MS`).

Prioritas jika bersamaan: GUNCANGAN/REM > BAHAYA > WASPADA > NORMAL.
Angka ambang adalah **nilai awal** dan harus dikalibrasi di uji lapangan (lihat bagian 5).

## 3. Pin map (final, sesuai `firmware/car_alarm/config.h`)

Diagram wiring: [docs/wiring.svg](docs/wiring.svg) (Indonesia) · [docs/wiring-en.svg](docs/wiring-en.svg) (English). Pin mengikuti `firmware/car_alarm/config.h`.

| Fungsi | Pin UNO | Catatan |
|---|---|---|
| LCD I2C SDA / SCL | A4 / A5 | Berbagi bus dengan MPU6050 |
| MPU6050 SDA / SCL | A4 / A5 | Alamat 0x68 (AD0 ke GND) |
| HC-SR04 Trig | D2 | Output |
| HC-SR04 Echo | D3 | Input |
| Bluetooth TX → UNO | D10 (SoftwareSerial RX) | Hindari D0/D1 agar upload tidak bentrok |
| Bluetooth RX ← UNO | D11 (SoftwareSerial TX) | **Wajib pembagi tegangan 5V→3.3V** (HC-05/06/HM-10) |
| Buzzer (via transistor) | D8 | Basis transistor lewat resistor ~1 kΩ |
| LED merah | D5 | Resistor 220 Ω |
| LED hijau/kuning (opsional) | D6 / D7 | Resistor 220 Ω |
| Tombol mute/reset | - | Tidak dipakai (diputuskan) |

### Rangkaian pendukung
- **Buzzer**: Pin D8 → resistor 1 kΩ → basis NPN; emitor ke GND; buzzer antara 5V dan kolektor; dioda flyback hanya jika buzzer tipe magnetik/coil.
- **LED**: Pin → resistor 220 Ω → LED → GND.
- **Bluetooth RX**: UNO TX (D11) → resistor 1 kΩ → RX modul, dan RX modul → resistor 2 kΩ → GND (pembagi ≈ 3.3 V).

## 4. Keputusan

**Sudah dikunci:** D1 = modul berlabel HC-05 yang ternyata BLE-only (diubah 12/10/2026, lihat PLAN.md), D2 = app Android native ringan dengan BLE GATT, D5 = ambang tetap, D10 = charger USB. HP Android, tanpa tombol mute/reset (D8 disesuaikan: guncangan/rem auto-clear setelah 10 detik).
**Dikunci 12/10/2026 (Fase 0 selesai):** D3 active buzzer, D4 3 LED, D6 kalibrasi otomatis, D7 nilai awal (dikalibrasi di Fase 4), D9 `Wire`, D11 `arduino-cli` + Gradle command-line.
Detail opsi di bawah dipertahankan sebagai referensi riwayat keputusan (rekomendasi ⭐ di sana bisa berbeda dari pilihan akhir; yang berlaku ada di PLAN.md). Status resmi ada di [PLAN.md](PLAN.md).

Setiap keputusan punya **rekomendasi (⭐)**. Jawab dengan kode, misalnya "D1=HC-05, D2=A, ...".
Setelah dipilih, catat hasilnya di bagian "Keputusan Terkunci" pada [PLAN.md](PLAN.md).

### D1. Modul Bluetooth
| Opsi | Kelebihan | Kekurangan |
|---|---|---|
| ⭐ **A. HC-05** | Murah, mudah, SPP klasik, banyak app Android, bisa master/slave | Tidak kompatibel dengan iPhone |
| B. HC-06 | Paling sederhana (hanya slave) | Tidak kompatibel dengan iPhone, konfigurasi terbatas |
| C. HM-10 (BLE) | Kompatibel Android **dan** iPhone, hemat daya | Aplikasi/protokol sedikit lebih rumit (GATT) |

Rekomendasi: A jika HP Android. **Pilih C jika HP iPhone** (HC-05/06 tidak akan terdeteksi).

### D2. Aplikasi di HP ("notifikasi")
| Opsi | Penjelasan |
|---|---|
| ⭐ **A. Serial Bluetooth Terminal** (app siap pakai) | Tanpa coding; pesan teks muncul di layar. Bukan notifikasi push sungguhan. Cocok untuk prototipe. |
| B. MIT App Inventor (buat app sendiri) | Bisa notifikasi, suara (TTS), getar, tampilan berwarna. Tanpa coding berat. |
| C. App native (Flutter / Kotlin) | Paling fleksibel, paling lama dikerjakan. |

Catatan: "notifikasi ke HP" yang benar-benar muncul di notification bar memerlukan app buatan sendiri (B/C).

### D3. Jenis buzzer
| Opsi | Penjelasan |
|---|---|
| ⭐ **A. Active buzzer** | Cukup HIGH/LOW, pola bunyi lewat timing. Sederhana. |
| B. Passive buzzer | Bisa nada berbeda (melodi, nada naik) memakai `tone()`, tetapi memakai Timer2 dan perlu kode lebih. |

### D4. LED
| Opsi | Penjelasan |
|---|---|
| A. 1 LED merah | Paling sederhana, status dibedakan oleh pola kedip. |
| ⭐ **B. 3 LED (hijau/kuning/merah)** | Status terlihat langsung tanpa perlu menghitung kedipan. Butuh 3 pin dan 3 resistor. |

### D5. Logika ambang jarak
| Opsi | Penjelasan |
|---|---|
| ⭐ **A. Ambang tetap, 2 tingkat** | Contoh: WASPADA < 100 cm, BAHAYA < 50 cm. Mudah diuji dan dipahami. |
| B. Ambang dinamis berdasarkan kecepatan | Jarak aman bertambah seiring kecepatan. Butuh sumber kecepatan (GPS atau OBD-II), **tidak ada** di daftar komponen. Integrasi akselerometer MPU6050 untuk kecepatan akan drift dan tidak akurat. |
| C. Ambang bisa diatur dari HP | Kirim perintah via Bluetooth (mis. `SET,WARN,120`) dan simpan ke EEPROM. Bisa ditambahkan di fase lanjutan di atas A. |

### D6. Pemasangan & kalibrasi MPU6050
| Opsi | Penjelasan |
|---|---|
| A. Pasang lurus, sumbu X = arah depan mobil | Sederhana, tetapi harus presisi. |
| ⭐ **B. Kalibrasi otomatis saat start** (mobil diam, rata) | Sistem merekam gravitasi sebagai nol; sumbu "depan" ditentukan saat kalibrasi/konfigurasi. Toleran pada pemasangan miring. |

### D7. Ambang guncangan & rem mendadak (nilai awal)
| Parameter | Nilai awal usulan | Catatan |
|---|---|---|
| Guncangan (selisih magnitudo dari 1 g) | > 0.6 g singkat | Deteksi benturan/getaran kuat |
| Rem mendadak (perlambatan searah gerak) | < -0.4 g selama ≥ 150 ms | Rem keras tipikal 0.5-0.8 g |
| Cooldown antar alarm | 3 detik | Cegah spam notifikasi |

Semua nilai dibuat `#define` dan dikalibrasi di uji jalan. Rem mendadak sulit dibedakan dari "mobil miring / jalan menurun" tanpa kalibrasi D6-B.

### D8. Perilaku alarm setelah kondisi hilang
| Opsi | Penjelasan |
|---|---|
| ⭐ **A. Auto-clear untuk jarak, latch untuk guncangan** | Alarm jarak mati sendiri saat aman; alarm guncangan bertahan beberapa detik/ sampai tombol ditekan. |
| B. Semua auto-clear | Sederhana, tetapi kejadian guncangan mudah terlewat. |
| C. Semua latch sampai tombol reset | Aman, tetapi butuh tombol tambahan. |

### D9. Library MPU6050
| Opsi | Penjelasan |
|---|---|
| ⭐ **A. Baca register langsung via `Wire`** | Tanpa dependensi, hemat memori, sedikit kode. |
| B. `MPU6050_light` | Mudah dipakai, ada kalibrasi bawaan. |
| C. Adafruit MPU6050 | Lengkap tetapi lebih berat. |

### D10. Sumber daya di mobil
| Opsi | Penjelasan |
|---|---|
| ⭐ **A. Charger mobil USB 5V → kabel USB ke UNO** | Aman, regulasi sudah ada, paling mudah. |
| B. Modul buck converter (LM2596) 12V→5V ke pin 5V/Vin | Lebih rapi/permanen. Atur ke 5.0 V **sebelum** disambung. |
| C. Langsung 12V ke Vin/jack UNO | **Tidak disarankan**: tegangan mobil 13.8-14.4 V dan banyak lonjakan, regulator UNO cepat panas. |

### D11. Lingkungan pengembangan
| Opsi | Penjelasan |
|---|---|
| ⭐ **A. Arduino IDE + simulasi Wokwi (opsional)** | Paling mudah; Wokwi mendukung UNO, HC-SR04, MPU6050, LCD I2C untuk uji logika sebelum hardware. |
| B. PlatformIO (VS Code) | Struktur proyek lebih rapi, manajemen library otomatis. |

## 5. Nilai ambang dan hasil uji

Sumber kebenaran: `firmware/car_alarm/config.h`. **Ditetapkan final oleh pengguna (12/10/2026), tetapi hanya teruji di meja; tidak dikalibrasi di mobil.**

| Parameter | Nilai | Status |
|---|---|---|
| WASPADA / BAHAYA | ≤ 100 cm / ≤ 50 cm, histeresis 5 cm, jeda tahan turun level 1 s | Teruji di meja |
| Guncangan | selisih magnitudo > 0.6 g, 2 sampel beruntun | Teruji di meja (2.1 g dan 1.1 g memicu, ≤ 0.33 g tidak) |
| Rem mendadak | perlambatan sumbu +Y > 0.4 g selama ≥ 150 ms | Teruji di meja (terukur 0.5 g; margin tipis) |
| Auto-clear guncangan/rem | 10 s (buzzer kontinu 3 s pertama) | Teruji (10,0 s) |
| Pesan BLE | satu baris `TIPE,SUMBER[,NILAI]`; cooldown 3 s per jenis kejadian; jarak tiap 500 ms | Teruji end-to-end dengan app |
| Kalibrasi MPU6050 | otomatis saat start (mobil diam, rata) | Teruji |

Tidak diuji (Fase 4 dilewati): rem biasa vs rem keras di jalan, speed bump, jalan bergelombang, pintu dibanting, catu daya charger mobil, uji > 1 jam. Semuanya ada di [docs/phase4-checklist.md](docs/phase4-checklist.md).

## 6. Batasan yang perlu diketahui (jujur)

- **HC-SR04** efektif ~2-3 m, sudut sempit, dan terpengaruh permukaan miring/lunak, hujan, dan suhu. Cocok untuk **kecepatan rendah / macet / parkir**, **bukan** peringatan tabrakan di jalan tol. Ini sistem pembantu, bukan pengaman keselamatan.
- Sensor harus dipasang dengan sudut pandang bebas (mis. di balik grill/bumper depan, atau di dasbor menghadap depan tidak lewat kaca; kaca memantulkan ultrasonik).
- Akselerometer tidak bisa membedakan 100% rem mendadak dari jalan bergelombang; perlu penyaringan (rata-rata bergerak + durasi minimum). Kemiringan terus-menerus (≈ 24°) setara −0.4 g dan bisa terbaca sebagai rem; pasang papan rata dan kaku.
- Margin deteksi rem tipis (terukur 0.5 g vs ambang 0.4 g) sampai dikalibrasi di jalan.
- Echo yang hilang (permukaan miring/lunak, di luar kerucut sensor) ditangani dengan jeda tahan 1 s dan tidak pernah memicu alarm, tetapi artinya alarm jarak bisa tidak muncul pada penghalang yang tidak memantulkan.
- Bluetooth LE berjangkauan ~10 m dan HP harus menjalankan app (Foreground Service) agar notifikasi muncul. App hanya diuji di satu HP (Samsung SM-A566B).
- Modul "HC-05" hanya BLE: tidak muncul di pemindaian Bluetooth Classic, dan tidak bisa dipakai app SPP biasa. Pakai app Car Alarm atau app BLE seperti Serial Bluetooth Terminal.
- Tanpa tombol: alarm guncangan/rem hilang sendiri setelah 10 s; kejadiannya tercatat di riwayat app.
- Ambang tidak bisa diubah tanpa mengunggah ulang firmware (opsi 3.4 belum dibuat).

## 7. Ide pengembangan

- Ambang bisa diubah dari app dan disimpan di EEPROM (PLAN 3.4, opsi D5-C).
- Ambang jarak dinamis berdasarkan kecepatan, dengan GPS HP atau OBD-II (akselerometer saja akan drift).
- Logging kejadian ke kartu SD atau ekspor riwayat app.
- Pemisahan rem dan kemiringan dengan giroskop MPU6050 (pitch).
- Pembaca sensor jarak kedua (belakang) untuk parkir mundur.
- App: pengaturan suara/getar, dukungan iPhone (butuh app iOS).

## 8. Struktur folder

```
Arduino Car Distance Alarm/
├─ README.md / README.id.md <- dokumen ini (English / Indonesia)
├─ PLAN.md          <- tahapan kerja + keputusan terkunci
├─ CLAUDE.md        <- panduan untuk Claude / kontributor
├─ firmware/car_alarm/ <- sketch utama (modul .h/.cpp per fungsi, config.h)
├─ app/             <- aplikasi Android native (Kotlin, BLE)
├─ console/         <- web console (index.html, ml.js); console/server = helper lokal
├─ docs/            <- wiring.svg/wiring-en.svg, test-results.md, phase4-checklist.md
└─ tests/           <- sketch uji per modul
```

## 9. Daftar belanja

UNO + kabel USB, HC-SR04, GY-521 (MPU6050), modul BLE berlabel HC-05 (atau HM-10), LCD 1602 + I2C backpack (0x27), active buzzer, LED merah/kuning/hijau, NPN 2N2222/BC547, resistor (220 Ω ×3, 1 kΩ ×2, 2 kΩ ×1), breadboard/perfboard + jumper, charger mobil USB.
