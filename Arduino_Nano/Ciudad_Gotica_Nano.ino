/*
  CIUDAD GOTICA - ARDUINO NANO
  Control de 24 LEDs (3 x 74HC595), sensores, LCD, alarmas y salida
  automatica de iluminacion por oscuridad en D8.

  Librerias necesarias:
  - DHT sensor library (Adafruit)
  - Adafruit Unified Sensor
  - LiquidCrystal_I2C

  Comunicacion ESP32:
  Nano D10 (RX) <- ESP32 GPIO17 (TX)
  Nano D11 (TX) -> divisor de voltaje -> ESP32 GPIO16 (RX)
  GND comun.
*/

#include <Wire.h>
#include <SoftwareSerial.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// -------------------- TIPOS --------------------
enum TipoAlarma { SIN_ALARMA, ALARMA_PIR, ALARMA_TEMP, ALARMA_SONIDO, ALARMA_MPU, ALARMA_AGUA };
struct Nota { unsigned int frecuencia; unsigned int duracion; };

// -------------------- PINES --------------------
SoftwareSerial enlaceESP(10, 11); // RX, TX
const byte TAM_BUFFER = 48;
char bufferComando[TAM_BUFFER];
byte indiceBuffer = 0;

const byte DATA_PIN = 4;
const byte CLOCK_PIN = 5;
const byte LATCH_PIN = 6;
const byte PIN_BUZZER = 7;
const byte PIN_LUCES_OSCURIDAD = 8;
const byte PIN_PIR = 2;
const byte PIN_DHT = 3;
const byte PIN_SONIDO = A0;
const byte PIN_LUZ = A1;
const byte PIN_AGUA = A2;

#define TIPO_DHT DHT11
DHT dht(PIN_DHT, TIPO_DHT);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------------------- UMBRALES --------------------
const float TEMP_MAXIMA = 35.0;
const int SONIDO_MAXIMO = 300;
const int AGUA_MAXIMA = 600;
const int UMBRAL_MPU = 6000;
const int UMBRAL_LUZ = 550;
const bool USAR_SENSOR_AGUA = false; // Cambiar a true cuando A2 tenga sensor conectado.

const unsigned long INTERVALO_DHT = 2000;
const unsigned long INTERVALO_LCD = 1000;
const unsigned long INTERVALO_MPU = 100;
const unsigned long INTERVALO_LUZ = 200;
const unsigned long INTERVALO_AGUA = 250;

// -------------------- 24 LEDS / 3 x 74HC595 --------------------
uint32_t estadoLEDs = 0;
const uint32_t MASCARA_24 = 0x00FFFFFFUL;

void actualizarRegistros() {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, (estadoLEDs >> 16) & 0xFF);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, (estadoLEDs >> 8) & 0xFF);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, estadoLEDs & 0xFF);
  digitalWrite(LATCH_PIN, HIGH);
}

void cambiarLED(byte numero, bool encender) {
  if (numero < 1 || numero > 24) return;
  uint32_t mascara = 1UL << (numero - 1);
  if (encender) estadoLEDs |= mascara;
  else estadoLEDs &= ~mascara;
  actualizarRegistros();
}

void cambiarGrupo(byte primero, byte ultimo, bool encender) {
  for (byte led = primero; led <= ultimo; led++) {
    uint32_t mascara = 1UL << (led - 1);
    if (encender) estadoLEDs |= mascara;
    else estadoLEDs &= ~mascara;
  }
  actualizarRegistros();
}

// -------------------- COMANDOS RECIBIDOS DEL ESP32 --------------------
void procesarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  bool encender;

  if (cmd == "ALL_ON" || cmd == "TODO_ON") {
    estadoLEDs = MASCARA_24;
    actualizarRegistros();
    enlaceESP.println("OK:ALL_ON");
    return;
  }
  if (cmd == "ALL_OFF" || cmd == "TODO_OFF") {
    estadoLEDs = 0;
    actualizarRegistros();
    enlaceESP.println("OK:ALL_OFF");
    return;
  }
  if (cmd == "ESTADO") {
    enlaceESP.print("LEDS:");
    enlaceESP.println(estadoLEDs);
    return;
  }

  // Casas: LEDs 1-3 zona 1 y LEDs 4-6 zona 2.
  if (cmd == "ZONA1_ON" || cmd == "ZONA_1_ON") { cambiarGrupo(1, 3, true); enlaceESP.println("OK:ZONA1_ON"); return; }
  if (cmd == "ZONA1_OFF" || cmd == "ZONA_1_OFF") { cambiarGrupo(1, 3, false); enlaceESP.println("OK:ZONA1_OFF"); return; }
  if (cmd == "ZONA2_ON" || cmd == "ZONA_2_ON") { cambiarGrupo(4, 6, true); enlaceESP.println("OK:ZONA2_ON"); return; }
  if (cmd == "ZONA2_OFF" || cmd == "ZONA_2_OFF") { cambiarGrupo(4, 6, false); enlaceESP.println("OK:ZONA2_OFF"); return; }

  // Edificios: edificio 1 = LEDs 7-9; edificio 2 = LEDs 10-12.
  if (cmd == "EDIFICIO1_ON") { cambiarGrupo(7, 9, true); enlaceESP.println("OK:EDIFICIO1_ON"); return; }
  if (cmd == "EDIFICIO1_OFF") { cambiarGrupo(7, 9, false); enlaceESP.println("OK:EDIFICIO1_OFF"); return; }
  if (cmd == "EDIFICIO2_ON") { cambiarGrupo(10, 12, true); enlaceESP.println("OK:EDIFICIO2_ON"); return; }
  if (cmd == "EDIFICIO2_OFF") { cambiarGrupo(10, 12, false); enlaceESP.println("OK:EDIFICIO2_OFF"); return; }

  // Comercios = LEDs 13-17; planta de luz = LEDs 18-24.
  if (cmd == "COMERCIOS_ON") { cambiarGrupo(13, 17, true); enlaceESP.println("OK:COMERCIOS_ON"); return; }
  if (cmd == "COMERCIOS_OFF") { cambiarGrupo(13, 17, false); enlaceESP.println("OK:COMERCIOS_OFF"); return; }
  if (cmd == "PLANTA_ON" || cmd == "PLANTA_DE_LUZ_ON") { cambiarGrupo(18, 24, true); enlaceESP.println("OK:PLANTA_ON"); return; }
  if (cmd == "PLANTA_OFF" || cmd == "PLANTA_DE_LUZ_OFF") { cambiarGrupo(18, 24, false); enlaceESP.println("OK:PLANTA_OFF"); return; }

  // Comandos CASA1_ON ... CASA6_OFF.
  for (byte casa = 1; casa <= 6; casa++) {
    String on = "CASA" + String(casa) + "_ON";
    String off = "CASA" + String(casa) + "_OFF";
    if (cmd == on) { cambiarLED(casa, true); enlaceESP.println("OK:" + cmd); return; }
    if (cmd == off) { cambiarLED(casa, false); enlaceESP.println("OK:" + cmd); return; }
  }

  // Comandos LED1_ON ... LED24_OFF.
  for (byte led = 1; led <= 24; led++) {
    String on = "LED" + String(led) + "_ON";
    String off = "LED" + String(led) + "_OFF";
    if (cmd == on) { cambiarLED(led, true); enlaceESP.println("OK:" + cmd); return; }
    if (cmd == off) { cambiarLED(led, false); enlaceESP.println("OK:" + cmd); return; }
  }

  enlaceESP.println("ERROR:COMANDO_DESCONOCIDO");
}

// -------------------- MPU-6050 --------------------
bool mpuDisponible = false;
bool primeraLecturaMPU = true;
int axAnterior = 0, ayAnterior = 0, azAnterior = 0;

void despertarMPU() {
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0);
  mpuDisponible = (Wire.endTransmission() == 0);
}

bool leerAcelerometro(int &ax, int &ay, int &az) {
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)0x68, (uint8_t)6) != 6) return false;
  ax = (int16_t)((Wire.read() << 8) | Wire.read());
  ay = (int16_t)((Wire.read() << 8) | Wire.read());
  az = (int16_t)((Wire.read() << 8) | Wire.read());
  return true;
}

// -------------------- PATRONES DE ALARMA --------------------
const Nota notasPIR[] = {{1400,180},{0,100}};
const Nota notasTemp[] = {{2200,150},{0,100},{2200,150},{0,100},{2200,150},{0,150}};
const Nota notasSonido[] = {{650,250},{0,150},{650,250},{0,150}};
// Patrón demostrativo; no es una alerta sísmica oficial.
const Nota notasMPU[] = {{700,180},{0,40},{1000,180},{0,40},{1400,350},{0,150}};
const Nota notasAgua[] = {{900,600},{0,150},{1600,180},{0,100},{1600,180},{0,150}};

TipoAlarma alarmaActual = SIN_ALARMA;
bool sonidoActivo = false;
byte indiceNota = 0, repeticionesRealizadas = 0;
const byte REPETICIONES_ALARMA = 3;
unsigned long inicioNota = 0;

byte prioridadAlarma(TipoAlarma tipo) {
  switch (tipo) {
    case ALARMA_AGUA: return 5;
    case ALARMA_MPU: return 4;
    case ALARMA_TEMP: return 3;
    case ALARMA_SONIDO: return 2;
    case ALARMA_PIR: return 1;
    default: return 0;
  }
}

const Nota* obtenerPatron(TipoAlarma tipo, byte &cantidad) {
  switch (tipo) {
    case ALARMA_PIR: cantidad = sizeof(notasPIR)/sizeof(notasPIR[0]); return notasPIR;
    case ALARMA_TEMP: cantidad = sizeof(notasTemp)/sizeof(notasTemp[0]); return notasTemp;
    case ALARMA_SONIDO: cantidad = sizeof(notasSonido)/sizeof(notasSonido[0]); return notasSonido;
    case ALARMA_MPU: cantidad = sizeof(notasMPU)/sizeof(notasMPU[0]); return notasMPU;
    case ALARMA_AGUA: cantidad = sizeof(notasAgua)/sizeof(notasAgua[0]); return notasAgua;
    default: cantidad = 0; return NULL;
  }
}

void enviarAlerta(String mensaje) {
  enlaceESP.println("ALERTA:" + mensaje);
  Serial.println("ALERTA:" + mensaje);
}

void iniciarAlarma(TipoAlarma tipo) {
  if (tipo == SIN_ALARMA) return;
  if (alarmaActual != SIN_ALARMA && prioridadAlarma(tipo) <= prioridadAlarma(alarmaActual)) return;
  alarmaActual = tipo;
  indiceNota = 0;
  repeticionesRealizadas = 0;
  inicioNota = millis();
  sonidoActivo = true;
  byte cantidad;
  const Nota *patron = obtenerPatron(alarmaActual, cantidad);
  if (patron && cantidad && patron[0].frecuencia) tone(PIN_BUZZER, patron[0].frecuencia);
  else noTone(PIN_BUZZER);
}

void actualizarBuzzer() {
  if (!sonidoActivo) return;
  byte cantidad;
  const Nota *patron = obtenerPatron(alarmaActual, cantidad);
  if (!patron || !cantidad) {
    noTone(PIN_BUZZER);
    sonidoActivo = false;
    alarmaActual = SIN_ALARMA;
    return;
  }
  unsigned long ahora = millis();
  if (ahora - inicioNota >= patron[indiceNota].duracion) {
    indiceNota++;
    if (indiceNota >= cantidad) {
      indiceNota = 0;
      repeticionesRealizadas++;
      if (repeticionesRealizadas >= REPETICIONES_ALARMA) {
        sonidoActivo = false;
        alarmaActual = SIN_ALARMA;
        noTone(PIN_BUZZER);
        Serial.println("Alarma terminada");
        enlaceESP.println("ALARMA:TERMINADA");
        return;
      }
    }
    inicioNota = ahora;
    if (patron[indiceNota].frecuencia) tone(PIN_BUZZER, patron[indiceNota].frecuencia);
    else noTone(PIN_BUZZER);
  }
}

// -------------------- SENSORES Y ESTADOS --------------------
float temperatura = NAN, humedad = NAN;
int valorSonido = 0, valorLuz = 0, valorAgua = -1;
bool condicionPIR = false, condicionTemperatura = false, condicionSonido = false;
bool condicionMPU = false, condicionAgua = false;
bool anteriorPIR = false, anteriorTemperatura = false, anteriorSonido = false;
bool anteriorMPU = false, anteriorAgua = false;

unsigned long tiempoDHT = 0, tiempoMPU = 0, tiempoLuz = 0;
unsigned long tiempoAgua = 0, tiempoLCD = 0;

void actualizarSensores() {
  unsigned long ahora = millis();
  condicionPIR = (digitalRead(PIN_PIR) == HIGH);
  valorSonido = analogRead(PIN_SONIDO);
  condicionSonido = (valorSonido >= SONIDO_MAXIMO);

  if (ahora - tiempoDHT >= INTERVALO_DHT) {
    tiempoDHT = ahora;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) { temperatura = t; condicionTemperatura = (t >= TEMP_MAXIMA); }
    if (!isnan(h)) humedad = h;
  }

  if (USAR_SENSOR_AGUA && ahora - tiempoAgua >= INTERVALO_AGUA) {
    tiempoAgua = ahora;
    valorAgua = analogRead(PIN_AGUA);
    condicionAgua = (valorAgua >= AGUA_MAXIMA);
  } else if (!USAR_SENSOR_AGUA) {
    valorAgua = -1;
    condicionAgua = false;
  }

  if (mpuDisponible && ahora - tiempoMPU >= INTERVALO_MPU) {
    tiempoMPU = ahora;
    int ax, ay, az;
    if (leerAcelerometro(ax, ay, az)) {
      if (primeraLecturaMPU) {
        axAnterior = ax; ayAnterior = ay; azAnterior = az;
        primeraLecturaMPU = false;
        condicionMPU = false;
      } else {
        long dx = ax - axAnterior, dy = ay - ayAnterior, dz = az - azAnterior;
        long cambio2 = dx*dx + dy*dy + dz*dz;
        long umbral2 = (long)UMBRAL_MPU * UMBRAL_MPU;
        condicionMPU = (cambio2 > umbral2);
        axAnterior = ax; ayAnterior = ay; azAnterior = az;
      }
    } else condicionMPU = false;
  }

  // La fotorresistencia controla exclusivamente D8.
  if (ahora - tiempoLuz >= INTERVALO_LUZ) {
    tiempoLuz = ahora;
    valorLuz = analogRead(PIN_LUZ);
    digitalWrite(PIN_LUCES_OSCURIDAD, (valorLuz < UMBRAL_LUZ) ? HIGH : LOW);
  }

  if (condicionPIR && !anteriorPIR) { enviarAlerta("MOVIMIENTO"); iniciarAlarma(ALARMA_PIR); }
  if (condicionTemperatura && !anteriorTemperatura) { enviarAlerta("TEMPERATURA"); iniciarAlarma(ALARMA_TEMP); }
  if (condicionSonido && !anteriorSonido) { enviarAlerta("SONIDO"); iniciarAlarma(ALARMA_SONIDO); }
  if (condicionMPU && !anteriorMPU) { enviarAlerta("MOVIMIENTO_MPU"); iniciarAlarma(ALARMA_MPU); }
  if (condicionAgua && !anteriorAgua) { enviarAlerta("AGUA"); iniciarAlarma(ALARMA_AGUA); }

  anteriorPIR = condicionPIR;
  anteriorTemperatura = condicionTemperatura;
  anteriorSonido = condicionSonido;
  anteriorMPU = condicionMPU;
  anteriorAgua = condicionAgua;
}

void actualizarPantalla() {
  if (millis() - tiempoLCD < INTERVALO_LCD) return;
  tiempoLCD = millis();
  lcd.clear();

  lcd.setCursor(0, 0);
  if (condicionAgua) lcd.print("ALERTA: AGUA");
  else if (condicionMPU) lcd.print("ALERTA: MPU");
  else if (condicionTemperatura) lcd.print("ALERTA: TEMP");
  else if (condicionSonido) lcd.print("ALERTA: SONIDO");
  else if (condicionPIR) lcd.print("ALERTA: MOV");
  else {
    lcd.print("Temp: ");
    if (!isnan(temperatura)) { lcd.print(temperatura, 1); lcd.print((char)223); lcd.print("C"); }
    else lcd.print("--");
    lcd.setCursor(0, 1);
    lcd.print("Luz: ");
    lcd.print(valorLuz);
  }
}

// -------------------- INICIO Y BUCLE --------------------
void setup() {
  Serial.begin(9600);
  enlaceESP.begin(9600);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LUCES_OSCURIDAD, OUTPUT);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_SONIDO, INPUT);

  Wire.begin();
  dht.begin();
  lcd.init();
  lcd.backlight();
  despertarMPU();

  estadoLEDs = 0;
  actualizarRegistros();
  digitalWrite(PIN_LUCES_OSCURIDAD, LOW);
  noTone(PIN_BUZZER);

  lcd.setCursor(0, 0);
  lcd.print("Ciudad Gotica");
  lcd.setCursor(0, 1);
  lcd.print("Nano listo");
  enlaceESP.println("NANO_LISTO:24_LEDS_SENSORES");
  Serial.println("Ciudad Gotica - Nano iniciado");
}

void loop() {
  while (enlaceESP.available() > 0) {
    char c = enlaceESP.read();
    if (c == '\n') {
      bufferComando[indiceBuffer] = '\0';
      if (indiceBuffer > 0) procesarComando(String(bufferComando));
      indiceBuffer = 0;
    } else if (c != '\r') {
      if (indiceBuffer < TAM_BUFFER - 1) {
        bufferComando[indiceBuffer++] = c;
      } else {
        indiceBuffer = 0;
      }
    }
  }
  actualizarSensores();
  actualizarBuzzer();
  actualizarPantalla();
}
