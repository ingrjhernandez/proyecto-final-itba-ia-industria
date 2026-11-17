#include <Arduino.h>
#include <Arduino_LSM9DS1.h>
#include <Arduino_LPS22HB.h>
#include <Arduino_APDS9960.h>

// Sensores detectados en esta placa (escaneo I2C en el bus interno Wire1):
//   0x6B + 0x1E -> LSM9DS1  (acelerometro + giroscopio + magnetometro)
//   0x5C        -> LPS22HB (presion barometrica + temperatura)
//   0x39        -> APDS9960 (luz, color, proximidad, gestos)
// No se detecto HTS221 (humedad/temperatura, 0x5F), por eso no se incluye.
//
// La placa no envia nada por su cuenta: solo responde a comandos de texto
// recibidos por el puerto serie, uno por linea (terminados en '\n').

static const unsigned long AVAILABLE_TIMEOUT_MS = 100; // espera max. a que el sensor tenga dato nuevo
static String inputLine;

// El LED RGB de la placa es activo en LOW (LOW = encendido).
void setRGB(bool red, bool green, bool blue) {
  digitalWrite(LEDR, red ? LOW : HIGH);
  digitalWrite(LEDG, green ? LOW : HIGH);
  digitalWrite(LEDB, blue ? LOW : HIGH);
}

void printHelp() {
  Serial.println(F("Comandos disponibles:"));
  Serial.println(F("  ACC       -> acelerometro (g): ACC,ax,ay,az"));
  Serial.println(F("  GYRO      -> giroscopio (dps): GYRO,gx,gy,gz"));
  Serial.println(F("  MAG       -> magnetometro (uT): MAG,mx,my,mz"));
  Serial.println(F("  IMU       -> acc+gyro+mag en una linea"));
  Serial.println(F("  BARO      -> presion (kPa) y temperatura (C): BARO,pressure,temp"));
  Serial.println(F("  PROX      -> proximidad: PROX,valor (o PROX,NA)"));
  Serial.println(F("  COLOR     -> luz/color: COLOR,r,g,b,c (o COLOR,NA)"));
  Serial.println(F("  GESTURE   -> espera un gesto breve: GESTURE,UP/DOWN/LEFT/RIGHT/NONE"));
  Serial.println(F("  ALL       -> todos los sensores en una linea"));
  Serial.println(F("  LED ON|OFF     -> LED integrado (D13)"));
  Serial.println(F("  RGB R|G|B|OFF  -> LED RGB de la placa"));
  Serial.println(F("  HELP      -> esta ayuda"));
}

// Espera hasta AVAILABLE_TIMEOUT_MS a que availableFn() sea verdadero.
bool waitAccel() {
  unsigned long start = millis();
  while (!IMU.accelerationAvailable()) {
    if (millis() - start > AVAILABLE_TIMEOUT_MS) return false;
  }
  return true;
}
bool waitGyro() {
  unsigned long start = millis();
  while (!IMU.gyroscopeAvailable()) {
    if (millis() - start > AVAILABLE_TIMEOUT_MS) return false;
  }
  return true;
}
bool waitMag() {
  unsigned long start = millis();
  while (!IMU.magneticFieldAvailable()) {
    if (millis() - start > AVAILABLE_TIMEOUT_MS) return false;
  }
  return true;
}
bool waitProximity() {
  unsigned long start = millis();
  while (!APDS.proximityAvailable()) {
    if (millis() - start > AVAILABLE_TIMEOUT_MS) return false;
  }
  return true;
}
bool waitColor() {
  unsigned long start = millis();
  while (!APDS.colorAvailable()) {
    if (millis() - start > AVAILABLE_TIMEOUT_MS) return false;
  }
  return true;
}

void cmdAcc() {
  float x = 0, y = 0, z = 0;
  if (waitAccel() && IMU.readAcceleration(x, y, z)) {
    Serial.print(F("ACC,"));
    Serial.print(x, 3); Serial.print(',');
    Serial.print(y, 3); Serial.print(',');
    Serial.println(z, 3);
  } else {
    Serial.println(F("ACC,NA"));
  }
}

void cmdGyro() {
  float x = 0, y = 0, z = 0;
  if (waitGyro() && IMU.readGyroscope(x, y, z)) {
    Serial.print(F("GYRO,"));
    Serial.print(x, 3); Serial.print(',');
    Serial.print(y, 3); Serial.print(',');
    Serial.println(z, 3);
  } else {
    Serial.println(F("GYRO,NA"));
  }
}

void cmdMag() {
  float x = 0, y = 0, z = 0;
  if (waitMag() && IMU.readMagneticField(x, y, z)) {
    Serial.print(F("MAG,"));
    Serial.print(x, 2); Serial.print(',');
    Serial.print(y, 2); Serial.print(',');
    Serial.println(z, 2);
  } else {
    Serial.println(F("MAG,NA"));
  }
}

void cmdImu() {
  float ax = 0, ay = 0, az = 0;
  float gx = 0, gy = 0, gz = 0;
  float mx = 0, my = 0, mz = 0;
  bool a = waitAccel() && IMU.readAcceleration(ax, ay, az);
  bool g = waitGyro() && IMU.readGyroscope(gx, gy, gz);
  bool m = waitMag() && IMU.readMagneticField(mx, my, mz);
  if (!a || !g || !m) {
    Serial.println(F("IMU,NA"));
    return;
  }
  Serial.print(F("IMU,"));
  Serial.print(ax, 3); Serial.print(',');
  Serial.print(ay, 3); Serial.print(',');
  Serial.print(az, 3); Serial.print(',');
  Serial.print(gx, 3); Serial.print(',');
  Serial.print(gy, 3); Serial.print(',');
  Serial.print(gz, 3); Serial.print(',');
  Serial.print(mx, 2); Serial.print(',');
  Serial.print(my, 2); Serial.print(',');
  Serial.println(mz, 2);
}

void cmdBaro() {
  float pressure = BARO.readPressure();
  float temp = BARO.readTemperature();
  Serial.print(F("BARO,"));
  Serial.print(pressure, 3); Serial.print(',');
  Serial.println(temp, 2);
}

void cmdProx() {
  if (waitProximity()) {
    Serial.print(F("PROX,"));
    Serial.println(APDS.readProximity());
  } else {
    Serial.println(F("PROX,NA"));
  }
}

void cmdColor() {
  int r = 0, g = 0, b = 0, c = 0;
  if (waitColor()) {
    APDS.readColor(r, g, b, c);
    Serial.print(F("COLOR,"));
    Serial.print(r); Serial.print(',');
    Serial.print(g); Serial.print(',');
    Serial.print(b); Serial.print(',');
    Serial.println(c);
  } else {
    Serial.println(F("COLOR,NA"));
  }
}

void cmdGesture() {
  unsigned long start = millis();
  while (!APDS.gestureAvailable()) {
    if (millis() - start > 1500) { // el gesto necesita que muevas la mano; se le da mas margen
      Serial.println(F("GESTURE,NONE"));
      return;
    }
  }
  int gesture = APDS.readGesture();
  Serial.print(F("GESTURE,"));
  switch (gesture) {
    case GESTURE_UP:    Serial.println(F("UP")); break;
    case GESTURE_DOWN:  Serial.println(F("DOWN")); break;
    case GESTURE_LEFT:  Serial.println(F("LEFT")); break;
    case GESTURE_RIGHT: Serial.println(F("RIGHT")); break;
    default:            Serial.println(F("NONE")); break;
  }
}

void cmdAll() {
  float ax = 0, ay = 0, az = 0;
  float gx = 0, gy = 0, gz = 0;
  float mx = 0, my = 0, mz = 0;
  waitAccel(); IMU.readAcceleration(ax, ay, az);
  waitGyro(); IMU.readGyroscope(gx, gy, gz);
  waitMag(); IMU.readMagneticField(mx, my, mz);

  float pressure = BARO.readPressure();
  float baroTemp = BARO.readTemperature();

  int proximity = -1, r = 0, g = 0, b = 0, c = 0;
  if (waitProximity()) proximity = APDS.readProximity();
  if (waitColor()) APDS.readColor(r, g, b, c);

  Serial.print(F("ALL,"));
  Serial.print(ax, 3); Serial.print(',');
  Serial.print(ay, 3); Serial.print(',');
  Serial.print(az, 3); Serial.print(',');
  Serial.print(gx, 3); Serial.print(',');
  Serial.print(gy, 3); Serial.print(',');
  Serial.print(gz, 3); Serial.print(',');
  Serial.print(mx, 2); Serial.print(',');
  Serial.print(my, 2); Serial.print(',');
  Serial.print(mz, 2); Serial.print(',');
  Serial.print(pressure, 3); Serial.print(',');
  Serial.print(baroTemp, 2); Serial.print(',');
  Serial.print(proximity); Serial.print(',');
  Serial.print(r); Serial.print(',');
  Serial.print(g); Serial.print(',');
  Serial.print(b); Serial.print(',');
  Serial.println(c);
}

void cmdLed(const String &arg) {
  if (arg == "ON") {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println(F("LED,ON"));
  } else if (arg == "OFF") {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println(F("LED,OFF"));
  } else {
    Serial.println(F("ERR,LED_ARG"));
  }
}

void cmdRgb(const String &arg) {
  if (arg == "R") {
    setRGB(true, false, false);
    Serial.println(F("RGB,RED"));
  } else if (arg == "G") {
    setRGB(false, true, false);
    Serial.println(F("RGB,GREEN"));
  } else if (arg == "B") {
    setRGB(false, false, true);
    Serial.println(F("RGB,BLUE"));
  } else if (arg == "OFF") {
    setRGB(false, false, false);
    Serial.println(F("RGB,OFF"));
  } else {
    Serial.println(F("ERR,RGB_ARG"));
  }
}

void processCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  String upper = line;
  upper.toUpperCase();

  int spaceIdx = upper.indexOf(' ');
  String cmd = (spaceIdx == -1) ? upper : upper.substring(0, spaceIdx);
  String arg = (spaceIdx == -1) ? "" : upper.substring(spaceIdx + 1);
  arg.trim();

  if (cmd == "ACC") cmdAcc();
  else if (cmd == "GYRO") cmdGyro();
  else if (cmd == "MAG") cmdMag();
  else if (cmd == "IMU") cmdImu();
  else if (cmd == "BARO") cmdBaro();
  else if (cmd == "PROX") cmdProx();
  else if (cmd == "COLOR") cmdColor();
  else if (cmd == "GESTURE") cmdGesture();
  else if (cmd == "ALL") cmdAll();
  else if (cmd == "LED") cmdLed(arg);
  else if (cmd == "RGB") cmdRgb(arg);
  else if (cmd == "HELP" || cmd == "?") printHelp();
  else {
    Serial.print(F("ERR,UNKNOWN_COMMAND,"));
    Serial.println(line);
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long waitStart = millis();
  while (!Serial && millis() - waitStart < 3000) {
    ; // espera a que se abra el puerto serie (timeout 3s si no hay monitor)
  }

  Serial.println(F("Arduino Nano 33 BLE Sense Lite - API serie de sensores"));

  bool ok = true;
  if (!IMU.begin()) {
    Serial.println(F("ERROR: no se pudo inicializar la IMU (LSM9DS1)."));
    ok = false;
  }
  if (!BARO.begin()) {
    Serial.println(F("ERROR: no se pudo inicializar el barometro (LPS22HB)."));
    ok = false;
  }
  if (!APDS.begin()) {
    Serial.println(F("ERROR: no se pudo inicializar el sensor de luz/gestos (APDS9960)."));
    ok = false;
  }
  if (!ok) {
    Serial.println(F("Continuando con los sensores disponibles..."));
  }
  APDS.setGestureSensitivity(80);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  setRGB(false, false, false);

  printHelp();
  Serial.println(F("Listo. Esperando comandos..."));

  inputLine.reserve(32);
}

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      processCommand(inputLine);
      inputLine = "";
    } else if (c != '\r') {
      inputLine += c;
    }
  }
}
