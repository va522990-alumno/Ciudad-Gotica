/*
  CIUDAD GOTICA - ESP32
  Recibe comandos por Bluetooth clasico y los reenvia al Arduino Nano.
  Requiere la libreria BluetoothSerial incluida en el core ESP32.

  Cableado UART:
  ESP32 GPIO17 (TX2) -> Nano D10 (RX)
  ESP32 GPIO16 (RX2) <- Nano D11 (TX), usando divisor de voltaje
  GND ESP32 <-> GND Nano

  El Nano espera comandos terminados en salto de linea.
  Nombre Bluetooth: Ciudad-Gotica
*/

#include <BluetoothSerial.h>

BluetoothSerial Bluetooth;
HardwareSerial EnlaceNano(2);

const int PIN_RX_NANO = 16; // ESP32 recibe desde TX del Nano
const int PIN_TX_NANO = 17; // ESP32 transmite hacia RX del Nano
const unsigned long VELOCIDAD = 9600;

String comandoBluetooth = "";
String respuestaNano = "";

void enviarComandoAlNano(String comando) {
  comando.trim();
  if (comando.length() == 0) return;

  EnlaceNano.println(comando);
  Serial.print("Enviado al Nano: ");
  Serial.println(comando);
}

void setup() {
  Serial.begin(115200);
  EnlaceNano.begin(VELOCIDAD, SERIAL_8N1, PIN_RX_NANO, PIN_TX_NANO);

  if (!Bluetooth.begin("Ciudad-Gotica")) {
    Serial.println("Error al iniciar Bluetooth");
  } else {
    Serial.println("Bluetooth listo: Ciudad-Gotica");
  }

  Serial.println("Puente Bluetooth <-> Arduino Nano iniciado");
}

void loop() {
  // Recibir comandos desde el telefono por Bluetooth.
  while (Bluetooth.available()) {
    char c = Bluetooth.read();

    if (c == '\n' || c == '\r') {
      if (comandoBluetooth.length() > 0) {
        enviarComandoAlNano(comandoBluetooth);
        comandoBluetooth = "";
      }
    } else if (comandoBluetooth.length() < 48) {
      comandoBluetooth += c;
    } else {
      comandoBluetooth = "";
      Bluetooth.println("ERROR:COMANDO_DEMASIADO_LARGO");
    }
  }

  // Devolver al telefono las respuestas y alertas que mande el Nano.
  while (EnlaceNano.available()) {
    char c = EnlaceNano.read();
    if (c == '\n') {
      if (respuestaNano.length() > 0) {
        Bluetooth.println(respuestaNano);
        Serial.println(respuestaNano);
        respuestaNano = "";
      }
    } else if (c != '\r' && respuestaNano.length() < 100) {
      respuestaNano += c;
    } else if (respuestaNano.length() >= 100) {
      respuestaNano = "";
    }
  }
}
