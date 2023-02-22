#include<Adafruit_Fingerprint.h>
#include<SD.h>
#include<DS3231.h>
#include<LCD_I2C.h>
File data, temp;
#define mySerial Serial1
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
String command;
bool inside, gb, dst;
LCD_I2C lcd(0x27, 16, 2);
RTClib myRTC;
DateTime now;
void setup() {
  Serial.begin(9600);
  finger.begin(57600);  //set the data rate for the sensor serial port
  if (finger.verifyPassword())
    Serial.println("Found fingerprint sensor!");
  else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1);
  }
  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")), Serial.println(finger.status_reg, HEX), Serial.print(F("Sys ID: 0x")), Serial.println(finger.system_id, HEX), Serial.print(F("Capacity: ")), Serial.println(finger.capacity), Serial.print(F("Security level: ")), Serial.println(finger.security_level), Serial.print(F("Device address: ")), Serial.println(finger.device_addr, HEX), Serial.print(F("Packet len: ")), Serial.println(finger.packet_len), Serial.print(F("Baud rate: ")), Serial.println(finger.baud_rate);
  finger.getTemplateCount();
  if (!finger.templateCount)
    Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
  else {
    Serial.println("Waiting for valid finger...");
    Serial.print("Sensor contains ");
    Serial.print(finger.templateCount);
    Serial.println(" templates");
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
    for (unsigned i = finger.templateCount; i; i--)
      data.write(1);
    data.close();
  }
  if (SD.exists("DST")) {
    data = SD.open("DST", FILE_READ);
    while (!data);
    dst = data.read() > 1;
    data.close();
  } else {
    data = SD.open("DST", FILE_WRITE);
    while (!data);
    data.write(1);
    data.close();
    dst = 0;
  }
  lcd.begin(), lcd.backlight();
  Serial.println("LCD Ready!");
  Wire.begin();
  Serial.println("Clock Ready!");
  pinMode(2, OUTPUT);
  Serial.println("Speaker Ready!");
  pinMode(3, INPUT_PULLUP);
  lcd.setCursor(5, 0), lcd.print("Ready!");
  delay(1000);
  Serial.println("Type 'help' to get the list of all commands.");
}
void loop() {
  if (Serial.available()) {  //commands
    lcd.clear(), lcd.setCursor(6, 0), lcd.print("Menu");
    command = Serial.readString();
    if (command == "help\n")
      Serial.println("Here's the list of all commands:\n\nattendee list       Shows a list of all attendees\nclear log           Clears the entries & exits\ndst                 Toggles the DST (Daylight Saving Time)\nenroll              Saves a new fingerprint\nerase fingerprints  Erases all saved fingerprint\nhelp                Shows the list of all commands\nreset data          Resets the participation data\nsaved fingerprints  Shows the number of saved fingerprints\nshow log            Shows the entries & exits");
    else if (command == "dst\n") {
      SD.remove("DST");
      data = SD.open("DST", FILE_WRITE);
      while (!data);
      if (dst)
        data.write(1);
      else
        data.write(2);
      dst = !dst;
    } else if (command == "saved fingerprints\n")
      Serial.print("Saved fingerprints count: "), Serial.println(finger.templateCount);
    else if (command == "attendee list\n") {
      data = SD.open("DATA", FILE_READ);
      while (!data);
      for (unsigned i = 1; data.available(); i++) {
        Serial.print("User #"), Serial.print(i), Serial.print(": ");
        if (data.read() > 1)
          Serial.println("Present");
        else
          Serial.println("Absent");
        data.close();
      }
    } else if (command == "reset data\n") {
      Serial.print("Resetting data...");
      while (SD.exists("DATA"))
        SD.remove("DATA");
      data = SD.open("DATA", FILE_WRITE);
      while (!data);
      for (unsigned i = finger.templateCount; i; i--)
        data.write(1);
      data.close();
      Serial.println("Reset data successfully!");
    } else if (command == "clear log\n") {
      Serial.print("Clearing log...");
      while (SD.exists("LOG.txt"))
        SD.remove("LOG.txt");
      Serial.println("Cleared log successfully!");
    } else if (command == "show log\n") {
      if (SD.exists("LOG.txt")) {
        data = SD.open("LOG.txt", FILE_READ);
        while (!data);
        Serial.println("Log:");
        while (data.available())
          Serial.write(data.read());
        data.close();
      } else
        Serial.println("`LOG.txt` not available!");
    } else if (command == "enroll\n") {
      unsigned id;
      Serial.println("Ready to enroll a fingerprint!");
      Serial.println("Please type in the ID # you want to save this finger as...");
      while (id < 1 || id > finger.templateCount + 2) {
        while (!Serial.available());
        id = Serial.parseInt();
      }
      Serial.print("Enrolling ID #");
      Serial.println(id);
      getFingerprintEnroll(id, 0);
    } else if (command == "erase fingerprints\n") {
      Serial.print("Erasing all saved fingerprints...");
      finger.emptyDatabase();
      while (SD.exists("DATA"))
        SD.remove("DATA");
      Serial.print("Erased all saved fingerprints successfully!\nPlease restart for changes to effect...");
      while (1);
    } else
      Serial.println("Invalid command!");
  }
  if (digitalRead(3) == LOW)
    getFingerprintEnroll(finger.templateCount + 1, 1);
  if (getFingerprintID()) {  //found a match!
    now = myRTC.now();
    data = SD.open("DATA", FILE_READ);
    temp = SD.open("TEMP", FILE_WRITE);
    while (!data || !temp);
    while (data.available())
      if (data.position() == finger.fingerID - 1) {
        inside = data.read() > 1;
        temp.write(2 - inside);
      } else
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
    data.print(now.hour() + dst);
    data.print(':');
    data.print(now.minute() / 10);
    data.print(now.minute() % 10);
    data.print(':');
    data.print(now.second() / 10);
    data.print(now.second() % 10);
    data.print("> User #");
    data.print(finger.fingerID);
    lcd.clear();
    if (inside) {
      data.println(" has exited");
      lcd.print("GoodBye");
    } else {
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
  if (now.hour() + dst > 9)
    lcd.setCursor(4, 1);
  else
    lcd.setCursor(5, 1);
  lcd.print(now.hour() + dst), lcd.print(':'), lcd.print(now.minute() / 10), lcd.print(now.minute() % 10), lcd.print(':'), lcd.print(now.second() / 10), lcd.print(now.second() % 10);
}
unsigned getFingerprintID() {  //returns 0 if failed, otherwise returns ID #
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)
    return 0;
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
    return 0;
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) {
    lcd.clear(), lcd.print("No match!");
    delay(1000);
    return 0;
  }  //found a match!
  return finger.fingerID;
}
void print(String message, bool Switch, bool error, bool Delay) {
  if (Switch) {
    lcd.clear();
    if (error)
      lcd.setCursor(5, 0), lcd.print("Error!");
    else
      lcd.print(message);
    delay(Delay * 1000);
  } else
    Serial.println(message);
}
uint8_t getFingerprintEnroll(unsigned id, bool Switch) {
  long long p = -1;
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
        print("Image taken", Switch, 0, 1);
        break;
      case FINGERPRINT_NOFINGER:
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        print("Communication error", Switch, 1, 0);
        break;
      case FINGERPRINT_IMAGEFAIL:
        print("Imaging error", Switch, 1, 0);
      default:
        print("Unknown error", Switch, 1, 0);
        break;
    }
  }  //OK success!
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      print("Image converted", Switch, 0, 1);
      break;
    case FINGERPRINT_IMAGEMESS:
      print("Image too messy", Switch, 1, 1);
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      print("Communication error", Switch, 1, 1);
      return p;
    case FINGERPRINT_FEATUREFAIL:
      print("Could not find fingerprint features", Switch, 1, 1);
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      print("Could not find fingerprint features", Switch, 1, 1);
      return p;
    default:
      print("Unknown error", Switch, 1, 1);
      return p;
  }
  print("Remove finger", Switch, 0, 0);
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
    p = finger.getImage();
  if (Switch) {
    lcd.clear(), lcd.print("ID #"), lcd.print(id);
    delay(1000);
  } else
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
        print("Image taken", Switch, 0, 1);
        break;
      case FINGERPRINT_NOFINGER:
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        print("Communication error", Switch, 1, 0);
        break;
      case FINGERPRINT_IMAGEFAIL:
        print("Imaging error", Switch, 1, 0);
        break;
      default:
        print("Unknown error", Switch, 1, 0);
        break;
    }
  }  //OK success!
  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      print("Image converted", Switch, 0, 1);
      break;
    case FINGERPRINT_IMAGEMESS:
      print("Image too messy", Switch, 1, 1);
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      print("Communication error", Switch, 1, 1);
      return p;
    case FINGERPRINT_FEATUREFAIL:
      print("Could not find fingerprint features", Switch, 1, 1);
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      print("Could not find fingerprint features", Switch, 1, 1);
      return p;
    default:
      print("Unknown error", Switch, 1, 1);
      return p;
  }  //OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    print("Prints matched!", Switch, 0, 1);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    print("Communication error", Switch, 1, 1);
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    print("Fingerprints did not match", Switch, 1, 1);
    return p;
  } else {
    print("Unknown error", Switch, 1, 1);
    return p;
  }
  Serial.print("ID #");
  Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    unsigned formerTemplateCount = finger.templateCount;
    finger.getTemplateCount();
    if (Switch || finger.templateCount > formerTemplateCount) {
      data = SD.open("DATA", FILE_WRITE);
      while (!data);
      data.write(1);
      data.close();
    }
    print("Stored!", Switch, 0, 1);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    print("Communication error", Switch, 1, 1);
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    print("Could not store in that location", Switch, 1, 1);
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    print("Error writing to flash", Switch, 1, 1);
    return p;
  } else {
    print("Unknown error", Switch, 1, 1);
    return p;
  }
  return true;
}
