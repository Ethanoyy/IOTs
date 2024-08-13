#include <Keypad.h>
#undef HIGH
#undef LOW
#include <Servo.h>
Servo servo;
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>  
#include <ArduinoJson.h>
#include <lcd_i2c.h>
lcd_i2c lcd(0x3E, 16, 2);

#include <ThingSpeak.h>
#include <ESP8266HTTPClient.h>

// * ThingSpeak Credentials *
unsigned long channelID = 2604246; // Your ThingSpeak channel ID
String apiKey = "BJOUVPWLNV7TA6DY"; // Your WriteAPIKey for the channel
const char* server = "https://api.thingspeak.com/update";
const char* fingerprint = "96:13:d2:f9:1b:b6:ef:9f:9e:ed:5d:43:b3:e6:1e:52:a3:ea:16:ac";

// Replace with your network credentials
//const char* ssid = "ET0731-IOTS";
//const char* password = "iotset0731";
const char* ssid = "SINGTEL-08A5";
const char* pass = "ydwc8npHpYgwSY3";

// Initialize Telegram BOT
#define BOTtoken "7078373689:AAFkCsuxBy-3EE6tazr8jVe5P8SHgwGNLRU"  // your Bot Token (Get from Botfather)
#define CHAT_ID "2123139054"

#ifdef ESP8266
  X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

WiFiClientSecure clientTelegram;
WiFiClientSecure clientThingSpeak;
UniversalTelegramBot bot(BOTtoken, clientTelegram);

// Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

const byte ROWS = 4; // four rows
const byte COLS = 4; // four columns

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {2, 0, 9, 10};
byte colPins[COLS] = {15, 13, 12, 14}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String password = "0000"; // initial password
String input_password;
String otp = "..";
unsigned long otpGeneratedTime = 0;
const unsigned long otpValidityPeriod = 60000; // 1 minute
String new_password;
String verify_new_password;
bool change_password_mode = false;
bool verify_current_password_mode = false;
int star_count = 0;
int hash_count = 0;

// Generate a 4-digit OTP
String generateOTP() {
  String otp = "";
  for (int i = 0; i < 4; i++) {
    otp += String(random(0, 10)); // Random digit between 0 and 9
  }
  otpGeneratedTime = millis(); // Record the time when OTP is generated
  return otp;
}

void teleprompt(){
    if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    Serial.print("Number of new messages: ");
    Serial.println(numNewMessages);

    while(numNewMessages) {
      Serial.println("Handling new messages...");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following command for otp.\n\n";
      welcome += "/otp to request otp \n";
      bot.sendMessage(chat_id, welcome, "");
    }
    
    if (text == "/otp") {
      otp = generateOTP(); // Generate a new OTP
      bot.sendMessage(chat_id, "Your OTP is: " + otp + ". It is valid for 1 minute.", "");
    }
  }
}

void invalidateOTP() {
  otp = ".."; // Clear the OTP
  otpGeneratedTime = 0; // Reset the generation time
}

void open() {
  servo.write(90);
  Serial.println("Door open");
  lcd.print("Door open");
  updateThingSpeak("1");
  delay(1000);
  lcd.clear();
}

void locked() {
  servo.write(0);
  Serial.println("Door locked");
  lcd.print("Door locked");
  updateThingSpeak("0");
  delay(1000);
  lcd.clear();
}

void updateThingSpeak(String status) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    clientThingSpeak.setFingerprint(fingerprint);
    Serial.println("[HTTPS] begin ...");
    http.begin(clientThingSpeak, server);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String httpRequestData = "api_key=" + apiKey;
    httpRequestData += "&field1=" + status;
    int httpCode = http.POST(httpRequestData);
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    if (httpCode == 200) {
      Serial.println("Channels update successful.");
    } else {
      Serial.println("Data upload failed .....");
      Serial.println("Problem updating channel. HTTP error code " + String(httpCode));
    }
    http.end();
    delay(7000);
    clientThingSpeak.stop();  // Stop the client
  }
}

void setup() {
  Serial.begin(115200);
  lcd.begin();
  #ifdef ESP8266
    configTime(0, 0, "pool.ntp.org");      // get UTC time via NTP
    clientTelegram.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  #endif
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  #ifdef ESP32
    clientTelegram.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  #endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("WiFi connected!");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());
  servo.attach(16); // D0
  servo.write(0);
  ThingSpeak.begin(clientThingSpeak);
  randomSeed(analogRead(0)); // Initialize the random number generator
}

void loop() {
  char key = keypad.getKey();
  if (key) {
    Serial.println(key);
    lcd.print("*");
    if (verify_current_password_mode) {
      if (key == '*') {
        input_password = ""; // clear input password
      } else if (key == '#') { // enter
        if (password == input_password) {
          Serial.println("Password verified, you can now change your password.");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter new ");
          lcd.setCursor(0, 1);
          lcd.print("password");
          // delay to allow the message to be read before clearing
          delay(2000);
          lcd.clear();
          verify_current_password_mode = false;
          change_password_mode = true;
          input_password = ""; // clear input password
        } else {
          Serial.println("Password incorrect, returning to normal mode.");
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Password wrong");
          delay(1000);
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Return to ");
          lcd.setCursor(0,1);
          lcd.print("normal mode");
          // delay to allow the message to be read before clearing
          delay(2000);
          lcd.clear();
          verify_current_password_mode = false;
          input_password = ""; // clear input password
        }
      } else {
        input_password += key; // append new character to input password string
      }
    } else if (change_password_mode) {
      if (key == '#' && hash_count == 0) {
        verify_new_password = new_password;
        Serial.println("Enter new password again to change the password.");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter new ");
        lcd.setCursor(0, 1);
        lcd.print("password again");
        // delay to allow the message to be read before clearing
        delay(2000);
        lcd.clear();
        hash_count++;
        new_password = "";
      } else if (key == '#' && hash_count == 1) {
        if (verify_new_password != new_password) {
          Serial.println("Password incorrect, returning to normal mode.");
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Password wrong");
          delay(1000);
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Return to ");
          lcd.setCursor(0, 1);
          lcd.print("normal mode");
          // delay to allow the message to be read before clearing
          delay(2000);
          lcd.clear();
          verify_current_password_mode = false;
          new_password = "";
          change_password_mode = false;
          hash_count = 0;
        } else {
          password = new_password;
          change_password_mode = false;
          Serial.println("Password changed successfully!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Password changed");
          lcd.setCursor(0, 1);
          lcd.print("successfully");
          // delay to allow the message to be read before clearing
          delay(2000);
          lcd.clear();
          hash_count = 0;
          new_password = "";
        }
      } else {
        new_password += key; // append new character to new password string
      }
    } else {
      if (key == '*') {
        star_count++;
        if (star_count == 3) {
          Serial.println("Enter current password to change the password.");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter current");
          lcd.setCursor(0, 1);
          lcd.print("password");
          // delay to allow the message to be read before clearing
          delay(2000);
          lcd.clear();
          verify_current_password_mode = true;
          star_count = 0; // reset star count
          input_password = ""; // clear input password
        }
      } else {
        star_count = 0; // reset star count if any other key is pressed
      }

      if (!verify_current_password_mode && !change_password_mode) {
        if (key == '*') {
          input_password = ""; // clear input password
        } else if (key == '#') {
      unsigned long currentTime = millis();
      if (password == input_password) {
            Serial.println("The password is correct, ACCESS GRANTED!");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Password correct");
            lcd.setCursor(0, 1);
            lcd.print("Access granted");
            // delay to allow the message to be read before clearing
            delay(2000);
            lcd.clear();
            open(); // Open the door if the password is correct
            delay(5000); // Wait for 5 seconds before locking
            locked(); // Lock the door afterwards
          }else if(input_password == otp && (currentTime - otpGeneratedTime) <= otpValidityPeriod){
            Serial.println("OTP is correct, ACCESS GRANTED!");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Password correct");
            lcd.setCursor(0, 1);
            lcd.print("Access granted");
            // delay to allow the message to be read before clearing
            delay(2000);
            lcd.clear();
            open(); // Open the door if the password is correct
            delay(5000); // Wait for 5 seconds before locking
            locked(); // Lock the door afterwards
            invalidateOTP();
          }else if (input_password == otp &&(currentTime - otpGeneratedTime) > otpValidityPeriod){
             Serial.println("The OTP has expired, ACCESS DENIED!");
             lcd.clear();
             lcd.setCursor(0, 0);
             lcd.print("Password wrong");
             lcd.setCursor(0, 1);
             lcd.print("Access denied");
             // delay to allow the message to be read before clearing
             delay(2000);
             lcd.clear();
             invalidateOTP(); // Invalidate OTP after it expires
          }
          else {
            Serial.println("The password is incorrect, ACCESS DENIED!");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Password wrong");
            lcd.setCursor(0, 1);
            lcd.print("Access denied");
            // delay to allow the message to be read before clearing
            delay(2000);
            lcd.clear();
          }
        input_password = ""; // clear input password  
    }else if(key == 'A'){
      Serial.println("Check telegram for OTP");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Check telegram");
      lcd.setCursor(0, 1);
      lcd.print("for OTP");
      // delay to allow the message to be read before clearing
      delay(2000);
      lcd.clear();
      input_password = ""; // clear input password
      teleprompt();
      clientTelegram.stop();  // Stop the Telegram client
    } else {
          input_password += key; // append new character to input password string
        }
      }
    } 
    
  }
}