#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <U8g2lib.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>

// pins
#define RST_PIN        22
#define SS_PIN         5
#define BUZZER         27
#define LED_PIN        13
#define FINGER_RX      25
#define FINGER_TX      26
#define OLED_SDA       21
#define OLED_SCL       22

// Wi-Fi & Sheets
#define WIFI_SSID      "Tope"
#define WIFI_PASSWORD  "agrismart5555"
const String sheet_url = "https://script.google.com/macros/s/AKfycbwVbZWn_6yCJNBgdsZOG7ETJYaylf"
                         "NHLAm3-3x1C0OLfmOC3LXIoYmOp3rP-aOAH8qB/exec?name=";

// timing
unsigned long lastCardReadTime       = 0;
const unsigned long cardReadInterval = 200;
unsigned long lastWifiAttempt        = 0;
const unsigned long wifiReconnectInterval = 5000;
unsigned long welcomeScreenStartTime = 0;
const unsigned long welcomeDisplayDuration = 3000;  // 3 seconds

// welcome message
String personName = "";
String scanMethod = "";
bool showingWelcome = false;

// display & sensors
U8G2_SSD1306_128X64_NONAME_F_SW_I2C oled(
  U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE
);
MFRC522           mfrc522(SS_PIN, RST_PIN);
HardwareSerial    fingerSerial(2);
Adafruit_Fingerprint finger(&fingerSerial);

// fingerprint names
const char* fingerprintNames[] = {
  "Unknown","Ada Destiny Ocheme","Ark of God Medude","Blessing Ochapa",
  "Ali Damkwana Buba","Nathaniel John","Joy Christopher","Favour Raphael",
  "Ozoemelam Ifesanchi Divinefavour","Anna Sokyes","Temitope Okunbamu",
  "Johnson Okunade","Ejim Doreen", "Angela Jinan","Oluyemi Olubodun"
};

// forward declarations
bool   joinWiFi();
String readRFIDName();
String getRFIDUID();
void   processRFID();
void   processFingerprint(int);
int    getFingerprintID();
void   sendToGoogleSheets(const String&, const String&);
String urlEncode(const String&);
void   beepBuzzer(int,int=100);


void setup() {
  Serial.begin(115200);
  Serial.println("Starting setup");

  // init display
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(100);
  if (!oled.begin()) {
    Serial.println("OLED not found");
    while (1) {
      digitalWrite(LED_PIN, HIGH);
      delay(300);
      digitalWrite(LED_PIN, LOW);
      delay(300);
    }
  }
  oled.clearBuffer();
  oled.setFont(u8g2_font_ncenB08_tr);
  oled.drawStr(10, 15, "Starting up");
  oled.sendBuffer();
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  oled.clearBuffer();
  oled.drawStr(10, 15, "Initializing");
  oled.drawStr(10, 30, "hardware");
  oled.sendBuffer();
  delay(1000);

  // init RFID
  SPI.begin();
  delay(50);
  mfrc522.PCD_Init();
  delay(50);
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  // init fingerprint
  fingerSerial.begin(57600, SERIAL_8N1, FINGER_RX, FINGER_TX);
  finger.begin(57600);

  // start Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // show connecting screen
  oled.clearBuffer();
  oled.drawStr(10, 15, "Connecting Wi Fi");
  oled.sendBuffer();

  // wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
  }

  // show connected + IP
  oled.clearBuffer();
  oled.drawStr(10, 15, "Wi Fi connected");
  oled.drawStr(10, 30, WiFi.localIP().toString().c_str());
  oled.sendBuffer();
  delay(1000);

  // ready to scan
  oled.clearBuffer();
  oled.drawStr(10, 15, "Ready to scan");
  oled.drawStr(10, 30, "card or finger");
  oled.sendBuffer();

  Serial.print("Wi Fi up, IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Setup complete");
}

void loop() {
  // clear welcome screen after timeout
  if (showingWelcome &&
      millis() - welcomeScreenStartTime >= welcomeDisplayDuration) {
    showingWelcome = false;
    oled.clearBuffer();
    oled.drawStr(10, 15, "Ready to scan");
    oled.drawStr(10, 35, "card or finger");
    oled.sendBuffer();
  }

  // reconnect Wi-Fi if dropped
  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastWifiAttempt >= wifiReconnectInterval) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWifiAttempt = millis();
  }

  if (showingWelcome) {
    delay(50);
    return;
  }

  // throttle scans
  if (millis() - lastCardReadTime < cardReadInterval) return;
  lastCardReadTime = millis();

  // RFID
  mfrc522.PCD_Init();
  if (mfrc522.PICC_IsNewCardPresent() &&
      mfrc522.PICC_ReadCardSerial()) {
    processRFID();
    return;
  }

  // fingerprint
  int fid = getFingerprintID();
  if (fid >= 0) {
    processFingerprint(fid);
  }
}

bool joinWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 10000) {
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

String readRFIDName() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  byte buf[18];
  byte len = sizeof(buf);
  if (mfrc522.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_A, 2, &key, &mfrc522.uid
      ) == MFRC522::STATUS_OK
   && mfrc522.MIFARE_Read(2, buf, &len) == MFRC522::STATUS_OK) {
    buf[16] = 0;
    String s = (char*)buf;
    s.trim();
    return s;
  }
  return "Unknown";
}

String getRFIDUID() {
  String uid;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += '0';
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void processRFID() {
  digitalWrite(LED_PIN, HIGH);
  String name = readRFIDName();
  String id = getRFIDUID();

  Serial.print("RFID detected - Name: ");
  Serial.print(name);
  Serial.print(" ID: ");
  Serial.println(id);

  showingWelcome = true;
  welcomeScreenStartTime = millis();

  oled.clearBuffer();
  oled.drawStr(10, 15, "Welcome!");
  oled.drawStr(10, 30, name.c_str());
  oled.drawStr(10, 45, "RFID detected");
  oled.sendBuffer();

  beepBuzzer(2);
  if (WiFi.status() == WL_CONNECTED) {
    sendToGoogleSheets(name, "RFID");
  }
  digitalWrite(LED_PIN, LOW);
}

void processFingerprint(int fid) {
  digitalWrite(LED_PIN, HIGH);
  String name = (fid > 0 && fid < (int)(sizeof(fingerprintNames)/sizeof(*fingerprintNames)))
                ? fingerprintNames[fid]
                : "Unknown";

  Serial.print("Fingerprint detected - Name: ");
  Serial.print(name);
  Serial.print(" ID: ");
  Serial.println(fid);

  showingWelcome = true;
  welcomeScreenStartTime = millis();

  oled.clearBuffer();
  oled.drawStr(10, 15, "Welcome!");
  oled.drawStr(10, 30, name.c_str());
  oled.drawStr(10, 45, "Fingerprint ID:");
  oled.drawStr(10, 60, String(fid).c_str());
  oled.sendBuffer();

  beepBuzzer(2);
  if (WiFi.status() == WL_CONNECTED) {
    sendToGoogleSheets(name, "Fingerprint");
  }
  digitalWrite(LED_PIN, LOW);
}

int getFingerprintID() {
  if (finger.getImage() != FINGERPRINT_OK) return -1;
  if (finger.image2Tz() != FINGERPRINT_OK) return -1;
  if (finger.fingerFastSearch() != FINGERPRINT_OK) return -1;
  return finger.fingerID;
}

void sendToGoogleSheets(const String& name, const String& method) {
  WiFiClientSecure client;
  client.setInsecure();
  String url = sheet_url + urlEncode(name) + "&method=" + urlEncode(method);
  HTTPClient https;
  if (https.begin(client, url)) {
    https.GET();
    https.end();
  }
}

String urlEncode(const String& str) {
  String out;
  for (char c : str) {
    if (c == ' ') out += "%20";
    else out += c;
  }
  return out;
}

void beepBuzzer(int beeps, int duration) {
  for (int i = 0; i < beeps; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(duration);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }
}
