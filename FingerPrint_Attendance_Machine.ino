#include<Adafruit_Fingerprint.h>
#include<SD.h>
#include<Wire.h>
#include<DS3231.h>
#include<LCD_I2C.h>

File data, temp;

#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
// For UNO and others without hardware serial, we must use software serial...
// pin #2 is IN from sensor (GREEN wire)
// pin #3 is OUT from arduino  (WHITE wire)
// Set up the serial port to use softwareserial..
SoftwareSerial mySerial(2, 3);

#else
// On Leonardo/M0/etc, others with hardware serial, use hardware serial!
// #0 is green wire, #1 is white
#define mySerial Serial1

#endif


Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
String command;
byte inside;
bool gb;
unsigned int id;
LCD_I2C lcd(0x27, 16, 2);
RTClib myRTC;
DateTime now;

void setup()
{
  Serial.begin(9600);
  while (!Serial);  // For Yun/Leo/Micro/Zero/...
  delay(100);
  Serial.println("\n\nAdafruit finger detect test");

  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1);
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

  finger.getTemplateCount();

  if (!finger.templateCount) {
    Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
  }
  else {
    Serial.println("Waiting for valid finger...");
    Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" templates");
  }
  Serial.print("\nInitializing MicroSD card...");
  if (!SD.begin(4)) {
    Serial.println("initialization failed. Please insert MicroSD card and reset the device.");
    while (1);
  } else
    Serial.println("initialization done.");
  if (!SD.exists("DATA")) {
    data = SD.open("DATA", FILE_WRITE);
    while (!data);
    for (unsigned int i = finger.templateCount; i; i--)
      data.write(1);
    data.close();
  }
  lcd.begin();
  lcd.backlight();
  Serial.println("LCD Ready!");
  Wire.begin();
  Serial.println("Clock Ready!");
  pinMode(2, OUTPUT);
  Serial.println("Speaker Ready!");
  pinMode(3, INPUT);
  lcd.setCursor(5, 0);
  lcd.print("Ready!");
  delay(1000);
  Serial.println("Type 'help' to get the list of all commands.");
}

void loop()                     // run over and over again
{

  if (Serial.available()) {     // commands
    command = Serial.readString();
    if (signal(command == "help\n"))
      Serial.println("Here's the list of all commands:\n\nclear log           Clears the entries & exits\nenroll              Saves a new fingerprint\nerase fingerprints  Erases all saved fingerprint\nhelp                Shows the list of all commands\nreset data          Resets the participation data\nsaved fingerprints  Shows the number of saved fingerprints\nshow log            Shows the entries & exits");
    else if (signal(command == "saved fingerprints\n"))
      Serial.print("Saved fingerprints count: "), Serial.println(finger.templateCount);
    else if (signal(command == "reset data\n")) {
      Serial.print("Resetting data...");
      while (SD.exists("DATA"))
        SD.remove("DATA");
      data = SD.open("DATA", FILE_WRITE);
      while (!data);
      for (unsigned int i = finger.templateCount; i; i--)
        data.write(1);
      data.close();
      Serial.println("Reset data successfully!");
    }
    else if (signal(command == "clear log\n")) {
      Serial.print("Clearing log...");
      while (SD.exists("LOG.txt"))
        SD.remove("LOG.txt");
      Serial.println("Cleared log successfully!");
    }
    else if (signal(command == "show log\n")) {
      data = SD.open("LOG.txt", FILE_READ);
      while (!data && SD.exists("LOG.txt"));
      Serial.println("Log:");
      while (data.available())
        Serial.write(data.read());
      data.close();
    }
    else if (signal(command == "enroll\n")) {
      Serial.println("Ready to enroll a fingerprint!");
      Serial.println("Please type in the ID # you want to save this finger as...");
      id = 0;
      while (id < 1 || id > finger.templateCount + 2) {
        while (!Serial.available());
        id = Serial.parseInt();
      }
      Serial.print("Enrolling ID #");
      Serial.println(id);
      getFingerprintEnroll(0);
      finger.getTemplateCount();
      while (SD.exists("DATA"))
        SD.remove("DATA");
      data = SD.open("DATA", FILE_WRITE);
      while (!data);
      for (unsigned int i = finger.templateCount; i; i--)
        data.write(1);
      data.close();
    }
    else if (signal(command == "erase fingerprints\n")) {
      Serial.print("Erasing all saved fingerprints...");
      finger.emptyDatabase();
      while (SD.exists("DATA"))
        SD.remove("DATA");
      Serial.print("Erased all saved fingerprints successfully!\nPlease restart for changes to effect...");
      while (1);
    }
    else
      Serial.println("Invalid command!");
  }
  if (digitalRead(3) == LOW) {
    id = finger.templateCount + 1;
    getFingerprintEnroll(1);
  }
  if (getFingerprintID()) {    // found a match!
    now = myRTC.now();
    data = SD.open("DATA", FILE_READ);
    temp = SD.open("TEMP", FILE_WRITE);
    while (!data || !temp);
    while (data.available())
      if (data.position() == finger.fingerID - 1) {
        inside = data.read();
        temp.write(1 + 2 - inside);
      }
      else
        temp.write(data.read());
    data.close();
    temp.close();
    while (SD.exists("DATA"))
      SD.remove("DATA");
    data = SD.open("DATA", FILE_WRITE);
    temp = SD.open("TEMP", FILE_READ);
    while (!data || !temp);
    while (temp.available())
      data.write(temp.read());
    data.close();
    temp.close();
    while (SD.exists("TEMP"))
      SD.remove("TEMP");
    data = SD.open("LOG.txt", FILE_WRITE);
    while (!data);
    gb = data.size() > 1000000000;
    data.print(now.year());
    data.print('/');
    data.print(now.month());
    data.print('/');
    data.print(now.day());
    data.print(' ');
    data.print(now.hour());
    data.print(':');
    data.print(now.minute() / 10);
    data.print(now.minute() % 10);
    data.print(':');
    data.print(now.second() / 10);
    data.print(now.second() % 10);
    data.print("> User #");
    data.print(finger.fingerID);
    lcd.clear();
    if (inside > 1) {
      data.println(" has exited");
      lcd.print("GoodBye");
    }
    else {
      data.println(" has entered");
      lcd.print("Welcome");
    }
    data.close();
    lcd.setCursor(0, 1), lcd.print("User #"), lcd.print(finger.fingerID);
    if (gb) {
      data = SD.open("LOG.txt", FILE_READ);
      temp = SD.open("TEMP", FILE_WRITE);
      while (!data || !temp);
      while (data.read() != '\n');
      while (data.available())
        temp.write(data.read());
      data.close();
      temp.close();
      SD.remove("LOG.txt");
      data = SD.open("LOG.txt", FILE_WRITE);
      temp = SD.open("TEMP", FILE_READ);
      while (!data && !temp);
      while (temp.available())
        data.write(temp.read());
      data.close();
      temp.close();
      SD.remove("TEMP");
    }
    digitalWrite(2, HIGH);
    delay(1000);
    digitalWrite(2, LOW);
  }

  now = myRTC.now();
  lcd.clear(), lcd.print(now.year()), lcd.print('/'), lcd.print(now.month()), lcd.print('/'), lcd.print(now.day());
  if (now.hour() > 9)
    lcd.setCursor(4, 1);
  else
    lcd.setCursor(5, 1);
  lcd.print(now.hour()), lcd.print(':'), lcd.print(now.minute() / 10), lcd.print(now.minute() % 10), lcd.print(':'), lcd.print(now.second() / 10), lcd.print(now.second() % 10);
}

// returns 0 if failed, otherwise returns ID #
int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return 0;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return 0;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return 0;

  // found a match!
  return finger.fingerID;
}

bool signal(bool b) {
  if (b)
    lcd.clear(), lcd.setCursor(6, 0), lcd.print("Menu");
  return b;
}

void LCDPrint(String s) {
  lcd.clear(), lcd.print(s), delay(1000);
}

void LCDError() {
  lcd.clear(), lcd.print("Error");
}

uint8_t getFingerprintEnroll(bool Switch) {

  int p = -1;
  if (Switch)
    lcd.clear(), lcd.print("Waiting for"), lcd.setCursor(0, 1), lcd.print("valid finger");
  else
    Serial.print("Waiting for valid finger to enroll as #"), Serial.println(id);
  while (p != FINGERPRINT_OK) {
    if (Switch && digitalRead(3) == HIGH)
      return true;
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        if (Switch)
          LCDPrint("Image taken");
        else
          Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        if (Switch)
          LCDError();
        else
          Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        if (Switch)
          LCDError();
        else
          Serial.println("Imaging error");
        break;
      default:
        if (Switch)
          LCDError();
        else
          Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      if (Switch)
        LCDPrint("Image converted");
      else
        Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      if (Switch)
        LCDPrint("Error");
      else
        Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      if (Switch)
        LCDPrint("Error");
      else
        Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      if (Switch)
        LCDPrint("Error");
      else
        Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      if (Switch)
        LCDPrint("Error");
      else
        Serial.println("Could not find fingerprint features");
      return p;
    default:
      if (Switch)
        LCDPrint("Error");
      else
        Serial.println("Unknown error");
      return p;
  }

  if (Switch)
    lcd.clear(), lcd.print("Remove finger");
  else
    Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
    p = finger.getImage();
  if (Switch) {
    lcd.clear(), lcd.print("ID #"), lcd.print(id);
    delay(1000);
  }
  else
    Serial.print("ID #"), Serial.println(id);
  p = -1;
  if (Switch)
    lcd.clear(), lcd.print("Place same"), lcd.setCursor(0, 1), lcd.print("finger again");
  else
    Serial.println("Place same finger again");
  delay(1000);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (Switch && digitalRead(3) == HIGH)
      return true;
    switch (p) {
      case FINGERPRINT_OK:
        if (Switch)
          LCDPrint("Image taken");
        else
          Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        if (Switch)
          LCDError();
        else
          Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        if (Switch)
          LCDError();
        else
          Serial.println("Imaging error");
        break;
      default:
        if (Switch)
          LCDError();
        else
          Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      if (Switch)
        LCDPrint("Image converted");
      else
        Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      if (Switch)
        LCDPrint("Error");
      else
        Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      if (Switch)
        LCDPrint("Error");
      else
        Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      if (Switch)
        LCDPrint("Error");
      else
        Serial.print("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      if (Switch)
        LCDPrint("Error");
      else
        Serial.println("Could not find fingerprint features");
      return p;
    default:
      if (Switch)
        LCDPrint("Error");
      else
        Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    if (Switch)
      LCDPrint("Prints matched!");
    else
      Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    if (Switch)
      LCDPrint("Error");
    else
      Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    if (Switch)
      LCDPrint("Error");
    else
      Serial.println("Fingerprints did not match");
    return p;
  } else {
    if (Switch)
      LCDPrint("Error");
    else
      Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    if (Switch) {
      finger.getTemplateCount();
      data = SD.open("DATA", FILE_WRITE);
      while (!data);
      data.write(1);
      data.close();
      LCDPrint("Stored!");
    }
    else
      Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    if (Switch)
      LCDPrint("Error");
    else
      Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    if (Switch)
      LCDPrint("Error");
    else
      Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    if (Switch)
      LCDPrint("Error");
    else
      Serial.println("Error writing to flash");
    return p;
  } else {
    if (Switch)
      LCDPrint("Error");
    else
      Serial.println("Unknown error");
    return p;
  }

  return true;
}
