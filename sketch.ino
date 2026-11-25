#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Inisialisasi LCD 20x4 (alamat I2C standar di Wokwi: 0x27)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Pin Sensor Simulasi (ADC1 ESP32)
const int PIN_VOLT  = 34;
const int PIN_CURR  = 35;
const int PIN_SPEED = 32;
const int PIN_WARN  = 2;

// Variabel Akumulasi Data
unsigned long prevMillis = 0;
const unsigned long INTERVAL_MS = 500; // Update setiap 0.5 detik

double totalEnergy_Wh  = 0.0;
double totalDistance_km = 0.0;
double elapsedTime_s   = 0.0;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_WARN, OUTPUT);

  lcd.init();
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print("EV JOULEMETER");
  lcd.setCursor(0, 1);
  lcd.print("Initializing Sys...");
  delay(1500);
  lcd.clear();

  // Header CSV untuk Telemetry di Serial Monitor
  Serial.println("Time_s,Voltage_V,Current_A,Power_W,Speed_kmh,Distance_km,Energy_Wh,Efficiency_km_kWh,Status");
  prevMillis = millis();
}

// Fungsi mapping nilai float dari ADC 12-bit (0 - 4095)
double mapFloat(int raw, double outMin, double outMax) {
  return (double)raw * (outMax - outMin) / 4095.0 + outMin;
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - prevMillis >= INTERVAL_MS) {
    double dt_s = (currentMillis - prevMillis) / 1000.0;
    prevMillis = currentMillis;
    elapsedTime_s += dt_s;

    // 1. Baca Input Simulasi Sensor
    double voltage_V = mapFloat(analogRead(PIN_VOLT), 42.0, 54.6); // Baterai 13S Li-Ion
    double current_A = mapFloat(analogRead(PIN_CURR), 0.0, 30.0);  // Arus Motor
    double speed_kmh = mapFloat(analogRead(PIN_SPEED), 0.0, 50.0); // Kecepatan

    // 2. Hitung Daya Sesaat & Integrasi Energi/Jarak
    double power_W = voltage_V * current_A;
    totalEnergy_Wh   += power_W * (dt_s / 3600.0);   // Wh = W * jam
    totalDistance_km += speed_kmh * (dt_s / 3600.0); // km = (km/jam) * jam

    // 3. Hitung Efisiensi (Jarak per Energi: km/kWh)
    double totalEnergy_kWh = totalEnergy_Wh / 1000.0;
    double efficiency_km_kWh = 0.0;
    if (totalEnergy_kWh > 0.00001) {
      efficiency_km_kWh = totalDistance_km / totalEnergy_kWh;
    }

    // 4. Cek Kondisi Safety (Overcurrent / Undervoltage)
    String status = "OK";
    if (current_A > 25.0 || voltage_V < 43.5) {
      digitalWrite(PIN_WARN, HIGH);
      status = (current_A > 25.0) ? "OVER_CURR" : "LOW_BATT";
    } else {
      digitalWrite(PIN_WARN, LOW);
    }

    // 5. Tampilkan ke Driver Dashboard (LCD 20x4)
    lcd.setCursor(0, 0);
    lcd.printf("V:%4.1fV  I:%4.1fA    ", voltage_V, current_A);

    lcd.setCursor(0, 1);
    lcd.printf("P:%4.0fW  E:%6.2fWh ", power_W, totalEnergy_Wh);

    lcd.setCursor(0, 2);
    lcd.printf("Spd:%4.1f D:%5.3fkm ", speed_kmh, totalDistance_km);

    lcd.setCursor(0, 3);
    if (status == "OK") {
      lcd.printf("Eff:%6.1f km/kWh  ", efficiency_km_kWh);
    } else {
      lcd.printf("WARN: %-13s", status.c_str());
    }

    // 6. Kirim Data Telemetry CSV ke Serial Monitor
    Serial.printf("%.1f,%.2f,%.2f,%.1f,%.1f,%.4f,%.3f,%.2f,%s\n",
                  elapsedTime_s, voltage_V, current_A, power_W,
                  speed_kmh, totalDistance_km, totalEnergy_Wh,
                  efficiency_km_kWh, status.c_str());
  }
}
