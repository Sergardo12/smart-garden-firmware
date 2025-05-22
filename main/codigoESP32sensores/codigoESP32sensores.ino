#include <WiFi.h>
#include <HTTPClient.h> //Se usa para enviar datos a Firebase Firestore vía HTTP
#include <ArduinoJson.h> //Sirve para construir el cuerpo JSON que se envía a Firestore
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>

// --- WiFi ---
const char* ssid = "Kopito";
const char* password = "kopito1995";

// --- Firebase ---
const char* API_KEY = "AIzaSyABIveW4aopDj-tEFnFJ1rre0sGZOBEAqk";  // Desde configuración de tu proyecto
const char* FIREBASE_PROJECT_ID = "sergito-70512";
const char* email = "sensor@huerto.com";
const char* passwordAuth = "12345678";
String idToken = "";

// --- DS18B20 ---
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// --- FC-28 ---
#define HUMEDAD_SUELO_PIN 34  // Pin analógico del ESP32


void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi conectado");

  // Configurar zona horaria UTC-5 (Perú) y servidor NTP
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // Esperar a que se sincronice el reloj
  Serial.print("Esperando sincronización de hora");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.printf("\nHora local: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  Serial.println("Hora sincronizada");
  
  sensors.begin();

  if (loginFirebase()) {
    Serial.println("Autenticado correctamente.");
  } else {
    Serial.println("Error al autenticar.");
  }
}

void loop() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  int humedadValor = analogRead(HUMEDAD_SUELO_PIN);  // 0 a 4095
  float humedadPorcentaje = (4095 - humedadValor) * 100.0 / 4095.0;

  Serial.println("Temperatura: " + String(tempC) + " °C");
  Serial.println("Humedad del suelo: " + String(humedadPorcentaje) + " %");
  
  if (idToken != "") {
  enviarDatosFirestore(tempC, humedadPorcentaje);
}

  delay(10000); // Esperar 10 segundos
}

bool loginFirebase() {
  HTTPClient http;
  http.begin("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + String(API_KEY));
  http.addHeader("Content-Type", "application/json");

  String body = "{\"email\":\"" + String(email) + "\",\"password\":\"" + String(passwordAuth) + "\",\"returnSecureToken\":true}";

  int httpCode = http.POST(body);

  if (httpCode == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, response);
    idToken = doc["idToken"].as<String>();
    http.end();
    return true;
  } else {
    Serial.println("Error de login: " + http.getString());
    http.end();
    return false;
  }
}

void enviarDatosFirestore(float temperatura, float humedadPorcentaje) {
  HTTPClient http;
  String url = "https://firestore.googleapis.com/v1/projects/" + String(FIREBASE_PROJECT_ID) +
               "/databases/(default)/documents/sensores";
  http.begin(url);
  http.addHeader("Authorization", "Bearer " + idToken);
  http.addHeader("Content-Type", "application/json");

  time_t now;
  time(&now);

  struct tm *utcTime = gmtime(&now);  // hora UTC verdadera

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", utcTime);

  String json = "{\n";
  json += "  \"fields\": {\n";
  json += "    \"temperatura\": {\"doubleValue\": " + String(temperatura, 2) + "},\n";
  json += "    \"humedad\": {\"doubleValue\": " + String(humedadPorcentaje, 2) + "},\n";
  json += "    \"fecha\": {\"timestampValue\": \"" + String(buffer) + "\"}\n";
  json += "  }\n";
  json += "}";

  int httpCode = http.POST(json);

  if (httpCode > 0) {
    Serial.println("Datos enviados a Firestore.");
    Serial.println(http.getString());
  } else {
    Serial.println("Error al enviar datos: " + String(httpCode));
    Serial.println(http.getString());
  }

  http.end();
}

