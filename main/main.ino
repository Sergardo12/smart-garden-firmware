#include <OneWire.h>
#include <DallasTemperature.h>
#include <TEMT6000.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// --- CREDENCIALES WI-FI ---
#define WIFI_SSID "TuRedWiFi"
#define WIFI_PASSWORD "TuClaveWiFi"

// --- FIREBASE ---
#define API_KEY "TU_API_KEY"
#define DATABASE_URL "https://tuprojecto-default-rtdb.firebaseio.com/"

// --- SENSORES ---
#define PIN_TEMP_DS18B20 4 // Pin para DS18B20
#define PIN_HUMEDAD_SUELO 34   // A0 en ESP32
#define PIN_LUZ_SOLAR 35       // A1 en ESP32

// --- ACTUADORES ---
#define PIN_BOMBA 13
#define PIN_LUZ_PLANTA 12
#define PIN_VENTILADOR 14

// --- OBJETOS FIREBASE ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// --- VARIABLES ---
float temperatura = 0.0, humedadAire = 0.0;
int humedadSuelo = 0, nivelLuz = 0;
bool modoManual = false;
bool bombaEncendida = false, luzEncendida = false, ventiladorEncendido = false;

// --- UMBRALES DINÁMICOS (recibidos desde Firebase) ---
int umbralHumedadSuelo = 0;
int umbralTemperatura = 0;
int umbralLuz = 0;

// --- SENSOR DS18B20 ---
OneWire oneWire(PIN_TEMP_DS18B20);
DallasTemperature sensores(&oneWire);

// --- SENSOR TEMT6000 ---
TEMT6000 lightSensor(PIN_LUZ_SOLAR);

void setup() {
  Serial.begin(115200);

  // Inicializar sensores
  sensores.begin();

  // Configurar pines de actuadores
  pinMode(PIN_BOMBA, OUTPUT);
  pinMode(PIN_LUZ_PLANTA, OUTPUT);
  pinMode(PIN_VENTILADOR, OUTPUT);

  // Conectar a Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi conectado.");

  // Configurar Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = "";
  auth.user.password = "";
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase inicializado.");
}

void loop() {
  // 1. Leer sensores
  // Lectura de temperatura DS18B20
  sensores.requestTemperatures();
  temperatura = sensores.getTempCByIndex(0);
  if (isnan(temperatura)) {
    Serial.println("Error al leer temperatura.");
    temperatura = 0.0;
  }

  // Lectura de humedad del suelo (sensor FC-28)
  humedadSuelo = analogRead(PIN_HUMEDAD_SUELO);

  // Lectura de luz (TEMT6000)
  nivelLuz = lightSensor.read();

  // 2. Obtener configuración desde Firebase
  if (Firebase.RTDB.getBool(&fbdo, "/modoManual"))
    modoManual = fbdo.boolData();

  if (Firebase.RTDB.getInt(&fbdo, "/config/umbralHumedad"))
    umbralHumedadSuelo = fbdo.intData();

  if (Firebase.RTDB.getInt(&fbdo, "/config/umbralTemp"))
    umbralTemperatura = fbdo.intData();

  if (Firebase.RTDB.getInt(&fbdo, "/config/umbralLuz"))
    umbralLuz = fbdo.intData();

  // 3. Enviar datos a Firebase
  Firebase.RTDB.setFloat(&fbdo, "/datos/temperatura", temperatura);
  Firebase.RTDB.setInt(&fbdo, "/datos/humedadSuelo", humedadSuelo);
  Firebase.RTDB.setInt(&fbdo, "/datos/luz", nivelLuz);

  // 4. Control manual (si está activado)
  if (modoManual) {
    Firebase.RTDB.getBool(&fbdo, "/manual/bomba");
    bombaEncendida = fbdo.boolData();
    Firebase.RTDB.getBool(&fbdo, "/manual/luz");
    luzEncendida = fbdo.boolData();
    Firebase.RTDB.getBool(&fbdo, "/manual/ventilador");
    ventiladorEncendido = fbdo.boolData();
  } else {
    // Control automático
    bombaEncendida = (humedadSuelo > umbralHumedadSuelo);
    luzEncendida = (nivelLuz < umbralLuz);
    ventiladorEncendido = (temperatura > umbralTemperatura);
  }

  // 5. Activar actuadores
  digitalWrite(PIN_BOMBA, bombaEncendida);
  digitalWrite(PIN_LUZ_PLANTA, luzEncendida);
  digitalWrite(PIN_VENTILADOR, ventiladorEncendido);

  // 6. Refrescar estado en Firebase
  Firebase.RTDB.setBool(&fbdo, "/estado/bomba", bombaEncendida);
  Firebase.RTDB.setBool(&fbdo, "/estado/luz", luzEncendida);
  Firebase.RTDB.setBool(&fbdo, "/estado/ventilador", ventiladorEncendido);

  delay(5000); // Esperar 5 segundos
}