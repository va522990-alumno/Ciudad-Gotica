# Ciudad Gotica

Proyecto escolar de ciudad inteligente basado en **Arduino Nano + ESP32**. El Nano procesa sensores, LCD, alarmas y 24 salidas LED mediante registros 74HC595. El ESP32 recibe comandos por Bluetooth clasico y los comunica al Nano.

## Estructura

- `Arduino_Nano/Ciudad_Gotica_Nano.ino`: sensores, LCD, alarmas, 24 LEDs y salida D8.
- `ESP32/Ciudad_Gotica_ESP32.ino`: Bluetooth y comunicacion serial con el Nano.

## Conexiones del Arduino Nano

| Pin Nano | Componente / funcion |
|---|---|
| D2 | Salida del sensor PIR HC-SR501 |
| D3 | Datos del DHT11 |
| D4 | DATA de los 74HC595 (pin 14) |
| D5 | CLOCK de los 74HC595 (pin 11) |
| D6 | LATCH de los 74HC595 (pin 12) |
| D7 | Buzzer |
| D8 | Salida para luces automaticas por oscuridad |
| D10 | RX de SoftwareSerial; recibe desde ESP32 GPIO17 |
| D11 | TX de SoftwareSerial; envia hacia ESP32 GPIO16 mediante divisor de voltaje |
| A0 | Salida analogica del sensor de sonido |
| A1 | Lectura de la fotoresistencia |
| A2 | Sensor de agua (desactivado en el codigo por defecto) |
| A4 | SDA: LCD I2C y MPU-6050 |
| A5 | SCL: LCD I2C y MPU-6050 |

**Importante:** comparte GND entre las dos placas. El pin TX del Nano trabaja a 5 V; usa un divisor de voltaje antes de conectarlo al RX del ESP32. No conectes directamente 5 V al GPIO del ESP32.

El codigo del Nano usa tres registros 74HC595 en cascada para disponer de 24 salidas. DATA, CLOCK y LATCH son compartidos por los tres.

## Conexion ESP32-Nano

| ESP32 | Arduino Nano |
|---|---|
| GPIO17 (TX2) | D10 (RX) |
| GPIO16 (RX2) | D11 (TX), con divisor de voltaje hacia ESP32 |
| GND | GND |

La velocidad serial entre placas es 9600 baudios.

## Distribucion de los 24 LEDs

| Salidas | Elemento |
|---|---|
| LED 1-3 | Casas de la zona 1 |
| LED 4-6 | Casas de la zona 2 |
| LED 7-9 | Edificio 1 |
| LED 10-12 | Edificio 2 |
| LED 13-17 | Comercios |
| LED 18-24 | Planta de luz |

## Comandos disponibles

Enviar cada comando por Bluetooth y terminarlo con salto de linea:

- `ALL_ON` / `ALL_OFF`: todas las salidas LED.
- `ZONA1_ON` / `ZONA1_OFF`: casas 1 a 3.
- `ZONA2_ON` / `ZONA2_OFF`: casas 4 a 6.
- `EDIFICIO1_ON` / `EDIFICIO1_OFF`.
- `EDIFICIO2_ON` / `EDIFICIO2_OFF`.
- `COMERCIOS_ON` / `COMERCIOS_OFF`.
- `PLANTA_ON` / `PLANTA_OFF`.
- `CASA1_ON` hasta `CASA6_OFF`: control individual de casas.
- `LED1_ON` hasta `LED24_OFF`: control individual.
- `ESTADO`: consulta el estado numerico de las salidas.

El nombre Bluetooth del ESP32 es **Ciudad-Gotica**.

## Sensores y comportamiento

- PIR: detecta movimiento.
- DHT11: mide temperatura; el umbral de alarma esta configurado en 35 °C.
- Sensor de sonido: umbral inicial de 300 (lectura analogica); debe calibrarse en el entorno real.
- MPU-6050: detecta cambios bruscos de aceleracion. Su umbral tambien requiere pruebas.
- Fotoresistencia: mide luz por A1; si la lectura cae por debajo de 550, activa D8. D8 se desactiva cuando vuelve a haber luz suficiente.
- Sensor de agua: reservado para A2, pero desactivado por defecto (`USAR_SENSOR_AGUA = false`).
- LCD I2C: muestra alertas; cuando no hay alertas muestra temperatura y lectura de luz.
- Buzzer: reproduce el patron correspondiente hasta tres veces por evento.

Los umbrales de luz, sonido, temperatura y aceleracion son valores iniciales, no valores universales. Ajustarlos mediante pruebas controladas.

## Librerias del Arduino Nano

Instalar desde el Administrador de bibliotecas del Arduino IDE:
- DHT sensor library (Adafruit)
- Adafruit Unified Sensor
- LiquidCrystal_I2C

El ESP32 requiere el paquete de placas ESP32 para Arduino IDE y la libreria `BluetoothSerial` incluida en el core.

## Puesta en marcha

1. Instalar las librerias necesarias.
2. Cargar primero `Ciudad_Gotica_Nano.ino` al Arduino Nano.
3. Cargar `Ciudad_Gotica_ESP32.ino` al ESP32.
4. Conectar las placas con el cableado UART indicado y GND comun.
5. Emparejar el telefono con el dispositivo Bluetooth **Ciudad-Gotica**.
6. Enviar comandos terminados en salto de linea.

## Notas de seguridad y pruebas

- El patron de sonido del MPU-6050 es demostrativo; no es una alerta sismica oficial ni sustituye sistemas certificados.
- Las alarmas de este prototipo son demostrativas y no deben usarse como sistema real de proteccion contra incendios, sismos o inundaciones.
- Antes de conectar cargas a D8, utiliza una etapa de potencia adecuada; no alimentes focos o cargas de corriente elevada directamente desde un pin del Nano.
- Verifica el orden fisico de los registros 74HC595 y el sentido de sus salidas con LEDs de prueba y resistencias.
