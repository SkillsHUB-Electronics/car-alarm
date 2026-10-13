# Per-module test sketches (phase 1)

Run them in order. One module must pass before moving on. Open the Serial Monitor at **9600 baud**.
Pins follow section 3 of the main README.

| Sketch | Purpose | Library | Passes if |
|---|---|---|---|
| `test_01_led_buzzer` | LED + buzzer via transistor | - | Green, yellow and red LEDs light in turn; the buzzer sounds 3 times |
| `test_02_i2c_lcd` | I2C scan + LCD | LiquidCrystal I2C (Frank de Brabander) | The scan shows 0x27/0x3F (and 0x68 if the MPU is fitted); the LCD shows text and a counter |
| `test_03_hcsr04` | HC-SR04 distance | - | Error at 20/50/100/200 cm is small and stable; a timeout prints "out of range" |
| `test_04_mpu6050` | Accelerometer | - | Magnitude ≈ 1.00 g at rest, rises when shaken |
| `test_05_bluetooth` | BLE module | - | The phone receives `TEST,BT,n`; text from the phone shows in the Serial Monitor |
| `test_07_bt_at` | Bluetooth AT diagnosis | - | Used to find out that the module is BLE-only |
| `test_06_web_console` | All interfaces over serial (115200) | LiquidCrystal I2C | Legacy; the web console's **Test IO** page does the same with the main firmware (`IO,*` commands), so the old web page for this sketch was removed |

## Testing through the web console (main firmware)

Once the main firmware `firmware/car_alarm` is on the UNO, use the [web console](../console/README.md):

1. Local (full features): run [console/server/start.bat](../console/server/start.bat) (needs Node.js and Arduino IDE 2; the IDE's `arduino-cli` is used automatically). The browser opens `http://localhost:8765`. Use **Chrome or Edge**. Or open the hosted GitHub Pages version for everything except compile/upload.
2. **Compile & Upload** panel (local only): pick the sketch (`firmware/car_alarm`) and the port (the UNO is selected automatically), click **Compile & Upload**. The Arduino IDE does not need to be open; close its Serial Monitor if it is open. The panel can also compile/upload the sketches in `tests/`.
3. Click **Connect**, choose the UNO port in the browser dialog (9600 baud). Browsers require this dialog for Web Serial.
4. **Test IO** drives the outputs (LEDs, buzzer, LCD, backlight, Bluetooth TX); the Dashboard shows the inputs (distance, IMU) live. **Calibration** compares the HC-SR04 with a ruler (true distance vs raw reading, linear correction stored in EEPROM).

Combined test (1.6): fit the LCD and the MPU6050 together on A4/A5, run `test_02_i2c_lcd`, then `test_04_mpu6050`. Both must keep working.

## Notes
- If the LCD is blank but the scan finds an address: turn the contrast potentiometer on the backpack, or change `LCD_ADDR`.
- The "HC-05" module in this project is BLE-only: pair nothing in Android settings, scan for it in the Car Alarm app or a BLE app instead.
- Record measurements (distance error, g at rest) in `docs/test-results.md`.
- The console's unit test: `node console/test/ml.test.js`.
