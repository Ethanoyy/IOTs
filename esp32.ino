#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Adafruit_Fingerprint.h>
#include <SPI.h>
#include <MFRC522.h>

// WiFi credentials
const char* ssid = "ET0731-IOTS";
const char* password = "iotset0731";

// Telegram BOT Token (Get from BotFather)
const char* botToken = "7146565713:AAHaccAmPPZZuOc5QaFJ1UliZw8p4KiasCY";

// Initialize Telegram BOT
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// Use hardware serial on ESP32
HardwareSerial mySerial(2);  // Using UART2 (RX: GPIO16, TX: GPIO17)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

uint8_t id;
volatile int finger_status = -1;
#define SS_PIN 5
#define RST_PIN 0
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.


void setup() {
  Serial.begin(115200);                       // Serial monitor
  mySerial.begin(57600, SERIAL_8N1, 16, 17);  // Initialize UART2 with baud rate, RX and TX pins
  Serial.begin(9600);
  SPI.begin();      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  client.setInsecure();  // This line disables certificate verification (for testing purposes only)

  // Initialize fingerprint sensor
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }

  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }

  delay(1000);
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    if (text == "/start") {
      bot.sendMessage(chat_id, "Welcome! Send /enroll to register a new fingerprint or /detect to detect a fingerprint.", "");
    }

    if (text == "/enroll") {
      bot.sendMessage(chat_id, "Please type in the ID # (from 1 to 127) you want to save this finger as...", "");
      id = readnumberFromTelegram(chat_id);
      if (id != 0) {
        bot.sendMessage(chat_id, "Enrolling ID #" + String(id), "");
        enrollFingerprint(chat_id);
      }
    }

    if (text == "/detect") {
      bot.sendMessage(chat_id, "Place your finger on the sensor...", "");
      int f = 1;
      while (f==1){
      finger_status = getFingerprintIDez();
      Serial.println(finger_status);
      if (finger_status != -1 and finger_status != -2) {
        bot.sendMessage(chat_id, "Fingerprint Match. ID #" + String(finger_status), "");
        bot.sendMessage(chat_id, "Please tap your card!", "");
        f = 0;
      } else {
        if (finger_status == -2) {
          for (int ii = 0; ii < 2; ii++) {
            bot.sendMessage(chat_id, "Fingerprint Not Match.", "");
            f = 1;
          }
        }
      }
      delay(2000);
    }
    readRFID(chat_id);

    }
  }
}

uint8_t readnumberFromTelegram(String chat_id) {
  uint8_t num = 0;
  while (num == 0) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    for (int i = 0; i < numNewMessages; i++) {
      String text = bot.messages[i].text;
      num = text.toInt();
      if (num == 0 || num > 127 || num < 1) {
        bot.sendMessage(chat_id, "Invalid ID. Please send a number between 1 and 127.", "");
      }
    }
  }
  return num;
}


void enrollFingerprint(String chat_id) {
  int p = -1;
  bot.sendMessage(chat_id, "Waiting for a valid finger to enroll as ID #" + String(id), "");

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        bot.sendMessage(chat_id, "Image taken", "");
        break;
      case FINGERPRINT_NOFINGER:
        delay(100);
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        bot.sendMessage(chat_id, "Communication error", "");
        break;
      case FINGERPRINT_IMAGEFAIL:
        bot.sendMessage(chat_id, "Imaging error", "");
        break;
      default:
        bot.sendMessage(chat_id, "Unknown error", "");
        break;
    }
  }

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      bot.sendMessage(chat_id, "Image converted", "");
      break;
    case FINGERPRINT_IMAGEMESS:
      bot.sendMessage(chat_id, "Image too messy", "");
      return;
    case FINGERPRINT_PACKETRECIEVEERR:
      bot.sendMessage(chat_id, "Communication error", "");
      return;
    case FINGERPRINT_FEATUREFAIL:
      bot.sendMessage(chat_id, "Could not find fingerprint features", "");
      return;
    case FINGERPRINT_INVALIDIMAGE:
      bot.sendMessage(chat_id, "Could not find fingerprint features", "");
      return;
    default:
      bot.sendMessage(chat_id, "Unknown error", "");
      return;
  }

  bot.sendMessage(chat_id, "Remove finger", "");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  bot.sendMessage(chat_id, "Place same finger again", "");

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        bot.sendMessage(chat_id, "Image taken", "");
        break;
      case FINGERPRINT_NOFINGER:
        delay(100);
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        bot.sendMessage(chat_id, "Communication error", "");
        break;
      case FINGERPRINT_IMAGEFAIL:
        bot.sendMessage(chat_id, "Imaging error", "");
        break;
      default:
        bot.sendMessage(chat_id, "Unknown error", "");
        break;
    }
  }

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      bot.sendMessage(chat_id, "Image converted", "");
      break;
    case FINGERPRINT_IMAGEMESS:
      bot.sendMessage(chat_id, "Image too messy", "");
      return;
    case FINGERPRINT_PACKETRECIEVEERR:
      bot.sendMessage(chat_id, "Communication error", "");
      return;
    case FINGERPRINT_FEATUREFAIL:
      bot.sendMessage(chat_id, "Could not find fingerprint features", "");
      return;
    case FINGERPRINT_INVALIDIMAGE:
      bot.sendMessage(chat_id, "Could not find fingerprint features", "");
      return;
    default:
      bot.sendMessage(chat_id, "Unknown error", "");
      return;
  }

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    bot.sendMessage(chat_id, "Prints matched!", "");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    bot.sendMessage(chat_id, "Communication error", "");
    return;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    bot.sendMessage(chat_id, "Fingerprints did not match", "");
    return;
  } else {
    bot.sendMessage(chat_id, "Unknown error", "");
    return;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    bot.sendMessage(chat_id, "Stored!", "");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    bot.sendMessage(chat_id, "Communication error", "");
    return;
  } else if (p == FINGERPRINT_BADLOCATION) {
    bot.sendMessage(chat_id, "Could not store in that location", "");
    return;
  } else if (p == FINGERPRINT_FLASHERR) {
    bot.sendMessage(chat_id, "Error writing to flash", "");
    return;
  } else {
    bot.sendMessage(chat_id, "Unknown error", "");
    return;
  }
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != 2) {
  }
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != 2) {
  }
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -2;

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);
  return finger.fingerID;
}

int readRFID(String chat_id) {
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) 
  {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) 
  {
    return;
  }

  //Show UID on serial monitor
  Serial.print("UID tag :");
  String content= "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++) 
  {
     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     Serial.print(mfrc522.uid.uidByte[i], HEX);
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  Serial.print("Message : ");
  content.toUpperCase();
  if (content.substring(1) == "D7 C5 7B 62") //change here the UID of the card/cards that you want to give access
  {
    bot.sendMessage(chat_id, "Authorized access", "");
    Serial.println();
    delay(3000);
  }
 
 else   {
    bot.sendMessage(chat_id, "Access denied", "");
    delay(1000);
    bot.sendMessage(chat_id, "Please scan your fingerprint!", "");
  }
}