#include<Adafruit_Fingerprint.h>//https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library
#include<RTClib.h>//https://github.com/adafruit/RTClib
#include<LCD_I2C.h>//https://github.com/blackhack/LCD_I2C
#include<SdFat.h>//https://github.com/greiman/SdFat
SdFat32 sd;//Initialize MicroSD module
File32 data,tmp;//Initialize file variables
Adafruit_Fingerprint finger=Adafruit_Fingerprint(&Serial1);//Initialize fingerprint sensor
String command;//Initialize commandline string
char inside,gb,dst;//Initialize boolean variables
LCD_I2C lcd(0x27,16,2);//Initialize LCD screen
RTC_DS3231 rtc;//Initialize time
DateTime now;//Initialize current time
#if HAS_SDIO_CLASS//MicroSD configuration
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(4,DEDICATED_SPI,SD_SCK_MHZ(50))
#else
#define SD_CONFIG SdSpiConfig(4,SHARED_SPI,SD_SCK_MHZ(50))
#endif
SdCardFactory cardFactory;//Initialize MicroSD card identifier
SdCard *m_card=cardFactory.newCard(SD_CONFIG);//Initialize MicroSD card
uint8_t sectorBuffer[512];//Initialize MicroSD sector buffer used for formatting
FatFormatter formatter;//Initialize the formatter
void(*reset)(void)=0;
void setup(){
	Serial.begin(9600),finger.begin(57600);//Set fingerprint sensor baud rate
	if(finger.verifyPassword())//Check if there's a fingerprint sensor
		Serial.println("Found fingerprint sensor!");
	else{
		Serial.println("Did not find fingerprint sensor :(");
		while(1);
	}
	Serial.println(F("Reading sensor parameters")),finger.getParameters();//Get fingerprint sensor parameters
	Serial.print(F("Status: 0x")),Serial.println(finger.status_reg,HEX),Serial.print(F("Sys ID: 0x")),Serial.println(finger.system_id,HEX),Serial.print(F("Capacity: ")),Serial.println(finger.capacity),Serial.print(F("Security level: ")),Serial.println(finger.security_level),Serial.print(F("Device address: ")),Serial.println(finger.device_addr,HEX),Serial.print(F("Packet len: ")),Serial.println(finger.packet_len),Serial.print(F("Baud rate: ")),Serial.println(finger.baud_rate);//Print fingerprint sensor parameters
	finger.getTemplateCount();//Get the number of fingerprint templates
	if(!finger.templateCount)//Notify if there's no fingerprint template
		Serial.print("Sensor doesn't contain any fingerprint data.");
	else//Show the number of fingerprint templates
		Serial.println("Waiting for valid finger..."),Serial.print("Sensor contains "),Serial.print(finger.templateCount),Serial.println(" templates");
	Serial.print("\nInitializing MicroSD card...");
	if(!sd.begin(4)){//Alert if no MicroSD card is detected
		Serial.println("Initialization failed. Please insert MicroSD card and reset the device (or enter any character to format the MicroSD card if it's already inserted).");
		while(!Serial.available());//Wait for a character entry
		if(formatter.format(m_card,sectorBuffer,&Serial))//Format the MicroSD card
			reset();
		else
			while(1);//Stop the code if formatting has failed
	}else
		Serial.println("Initialization done.");
	if(!sd.exists("DATA")){//Create the `DATA` file if it doesn't exist
		data.open("DATA",FILE_WRITE);//Open the file
		while(!data);//Stop if failed to open the file
		for(unsigned long long i=finger.templateCount;i--;)//Write a character with a byte of `00000001` to the file `n` times (where `n` is the number of fingerprints)
			data.write('\0');
		data.close();//Close the file
	}
	if(sd.exists("DST")){//Read the `DST` file if it exists
		data.open("DST",FILE_READ);//Open the file
		while(!data);//Stop if failed to open the file
		dst=data.read();//Read the file
		data.close();//Close the file
	}else{//Create the `DST` file if it doesn't exist
		data.open("DST",FILE_WRITE);//Open the file
		while(!data);//Stop if failed to open the file
		data.write('\0');//Write the default data to the file (which is `00000001`)
		data.close();//Close the file
		dst=0;//Give the DST variable the default value
	}
	while(!sd.exists("SUM"))
		sd.mkdir("SUM");
	while(!sd.exists("LAST"))
		sd.mkdir("LAST");
	lcd.begin(),lcd.backlight();//Start the LCD screen
	rtc.begin();//Start the clock module
	pinMode(2,OUTPUT);//Initialize the buzzer pin
	pinMode(3,INPUT_PULLUP);//Initialize the enroll switch
	lcd.setCursor(5,0),lcd.print("Ready!");//Notify
	delay(1000);//Delay for 1 second
	Serial.println("Type 'help' to get the list of all commands.");//Give a hint for the user
	return;
}
void loop(){
	if(Serial.available()){//Read the commands
		lcd.clear(),lcd.setCursor(6,0),lcd.print("Menu");//Show 'Menu' on the LCD screen
		command=Serial.readString();//Read the command
		command.trim();//Trim the command string
		if(command!=""){
			if(command=="help")//Show help
				Serial.println("Here's the list of all commands:\nattendance time     Shows attendance time\nattendee list       Shows a list of all attendees\nclear log           Clears the entries & exits\ndst                 Toggles the DST (Daylight Saving Time)\nenroll              Saves a new fingerprint\nerase fingerprints  Erases all saved fingerprint\nformat              Formats the MicroSD card and resets the device\nhelp                Shows the list of all commands\nreset attendance    Resets attendance time\nreset data          Resets the participation data\nsaved fingerprints  Shows the number of saved fingerprints\nshow log            Shows the entries & exits\ntime                Changes time configured for the device");
			else if(command=="dst"){//Change DST
				sd.remove("DST");//Delete the `DST` file
				data.open("DST",FILE_WRITE);//Create a new `DST` file
				while(!data);//Stop if failed to open the file
				dst=!dst,data.write(dst);//Change the bit of the DST variable
			}else if(command=="saved fingerprints")//Show the number of saved fingerprints
				Serial.print("Saved fingerprints count: "),Serial.println(finger.templateCount);
			else if(command=="attendee list"){//Show the attendee list
				data.open("DATA",FILE_READ);//Open the `DATA` file
				while(!data);//Stop if failed to open the file
				while(data.available()){//Repeat this while there are bytes available
					Serial.print("User #"),Serial.print(data.position()+1),Serial.print(": ");//Print the number of the user
					if(data.read())//Show 'Present' if the current byte is `00000010`
						Serial.println("Present");
					else//Show 'Absent' if the current byte is `00000001`
						Serial.println("Absent");
				}
				data.close();//Close the file
			}else if(command=="reset data"){//Delete the `DATA` file and create a new one
				Serial.print("Resetting data...");//Notify
				resetData();
				data.open("DATA",FILE_WRITE);//Open a new `DATA file
				while(!data);//Stop if failed to open the file
				for(unsigned long long i=finger.templateCount;i--;)//Write a character with a byte of `00000001` to the file `n` times (where `n` is the number of fingerprints)
					data.write('\0');
				data.close();//Close the file
				Serial.println("Reset data successfully!");//Notify
			}else if(command=="clear log"){//Delete the `LOG.txt` file
				Serial.print("Clearing log...");//Notify
				while(sd.exists("LOG.txt"))//Attempt to delete the file until it's deleted
					sd.remove("LOG.txt");//Delete the file
				Serial.println("Cleared log successfully!");//Notify
			}else if(command=="show log"){//Show the contents of `LOG.txt`
				if(sd.exists("LOG.txt")){//Open the file if it exists
					data.open("LOG.txt",FILE_READ);//Open the file
					while(!data);//Stop if failed to open the file
					Serial.println("Log:");//Show reading the file
					while(data.available())//Read the file byte by byte until it's finished
						Serial.write(data.read());
					data.close();//Close the file
				}else//Alert if there's no `LOG.txt`
					Serial.println("`LOG.txt` not available!");
			}else if(command=="enroll"){//Enroll a new fingerprint
				unsigned id;//Initialize user ID
				Serial.println("Ready to enroll a fingerprint!"),Serial.println("Please type in the ID # you want to save this finger as...");
				do{
					while(!Serial.available());//Wait for an entry
					id=Serial.parseInt();//Read the number
				}while(id<1||id>finger.templateCount+2);//Repeat if an invalid number is entered
				Serial.print("Enrolling ID #"),Serial.println(id);//Notify
				getFingerprintEnroll(id,0);//Enroll
			}else if(command=="erase fingerprints"){//Erase fingerprint templates, delete `DATA` file and reset device
				Serial.print("Erasing all saved fingerprints...");//Notify
				finger.emptyDatabase();//Erase all fingerprint templates
				resetData();
				Serial.println("Erased all saved fingerprints successfully!");//Notify
				reset();//Reset the device
			}else if(command == "format"){//Format the MicroSD card
				if(formatter.format(m_card,sectorBuffer,&Serial))//Format the MicroSD card
					reset();
				else
					while(1);
			}else if(command=="time"){
				Serial.println("Please enter your date & time in `YYYY,MM,DD,hh,mm,ss` (without DST):");
				while(!Serial.available());
				uint16_t clock[6];
				clock[0]=Serial.parseInt(),clock[1]=Serial.parseInt(),clock[2]=Serial.parseInt(),clock[3]=Serial.parseInt(),clock[4]=Serial.parseInt(),clock[5]=Serial.parseInt(),rtc.adjust(DateTime(clock[0],clock[1],clock[2],clock[3],clock[4],clock[5])),Serial.println("Done!");
			}else if(command=="attendance time"){
				unsigned ID;
				Serial.println("Please enter your fingerprint ID:");
				do{
					while(!Serial.available());
					ID=Serial.parseInt();
				}while(!ID||ID>finger.templateCount);
				if(sd.exists("SUM/"+String(ID))){
					unsigned long time=0;
					data.open(("SUM/"+String(ID)).c_str());
					while(data.available())
						time=time*10+data.read()-'0';
					data.close();
					Serial.print("Days: "),Serial.println(time/86400),Serial.print("Hours: "),Serial.println(time%86400/3600),Serial.print("Minutes: "),Serial.println(time%3600/60),Serial.print("Seconds: "),Serial.println(time%60);
				}else
					Serial.println("Days: 0\nHours: 0\nMinutes: 0\nSeconds: 0");
			}else if(command=="reset attendance"){
				unsigned ID;
				Serial.println("Please enter your fingerprint ID:");
				do{
					while(!Serial.available());
					ID=Serial.parseInt();
				}while(!ID||ID>finger.templateCount);
				while(sd.exists("SUM/"+String(ID)))
					sd.remove("SUM/"+String(ID));
				Serial.println("Attendance time has been reset successfully!");
			}else//Alert if an invalid command has been entered
				Serial.println("Invalid command!");
		}
	}
	if(!digitalRead(3))//Enroll a new fingerprint if the enroll switch is enabled
		getFingerprintEnroll(finger.templateCount+1,1);
	if(getFingerprintID()){//Read fingerprints
		unsigned long last=0;
		now=dst?rtc.now()+TimeSpan(0,1,0,0):rtc.now();//Set current time
		data.open("DATA",FILE_READ);//Open `DATA` file
		tmp.open("TMP",FILE_WRITE);//Open `TMP file
		while(!data||!tmp);//Stop if failed to open any of the files
		while(data.available())//Copy the contents of `DATA` to `TMP`
			if(data.position()==finger.fingerID-1){//Replace the byte which indicates the current user with either `00000001` or `00000010`
				inside=data.read();//Read the opposite of the desired byte
				tmp.write(!inside);//Copy the opposite to `TMP`
			}else//Copy the rest of the bytes as they are
				tmp.write(data.read());
		data.close();//Close `DATA`
		tmp.close();//Close `TMP`
		if(sd.exists("LAST/"+String(finger.fingerID))){
			data.open(("LAST/"+String(finger.fingerID)).c_str(),FILE_READ);
			while(!data);
			while(data.available())
				last=last*10+data.read()-'0';
			data.close();
			last=now.unixtime()-last;
		}else
			last=0;
		while(sd.exists("LAST/"+String(finger.fingerID)))
			sd.remove("LAST/"+String(finger.fingerID));
		data.open(("LAST/"+String(finger.fingerID)).c_str(),FILE_WRITE);
		while(!data);
		data.print(now.unixtime());
		data.close();
		if(sd.exists("SUM/"+String(finger.fingerID))){
			unsigned long last0=0;
			data.open(("SUM/"+String(finger.fingerID)).c_str(),FILE_READ);
			while(!data);
			while(data.available())
				last0=last0*10+data.read()-'0';
			data.close();
			last+=last0;
		}
		if(inside){
			while(sd.exists("SUM/"+String(finger.fingerID)))
				sd.remove("SUM/"+String(finger.fingerID));
			data.open(("SUM/"+String(finger.fingerID)).c_str(),FILE_WRITE);
			while(!data);
			data.print(last);
			data.close();
		}
		while(sd.exists("DATA"))//Attempt to delete `DATA` file until it's deleted
			sd.remove("DATA");//Delete the file
		data.open("DATA",FILE_WRITE);//Open a new `DATA` file
		tmp.open("TMP",FILE_READ);//Open the `TMP` file again
		while(!data||!tmp);//Stop if failed to open any of the files
		while(tmp.available())//Copy all of the contents from `TMP` to `DATA`
			data.write(tmp.read());
		data.close();//Close `DATA`
		tmp.close();//Close `TMP`
		while(sd.exists("TMP"))//Attempt to delete `TMP` until it's deleted
			sd.remove("TMP");//Delete the file
		data.open("LOG.txt",FILE_WRITE);//Open `LOG.txt`
		while(!data);//Stop if failed to open the file
		gb=data.size()>1000000000;//Check if the size of the file is more than 1 Gigabytes
		data.print(now.year());//Write the date & the time
		data.print('/');
		data.print(now.month());
		data.print('/');
		data.print(now.day());
		data.print(' ');
		data.print(now.hour());
		data.print(':');
		data.print(now.minute()/10);
		data.print(now.minute()%10);
		data.print(':');
		data.print(now.second()/10);
		data.print(now.second()%10);
		data.print("> User #");//Write the user
		data.print(finger.fingerID);
		lcd.clear();//Clear the LCD screen
		if(inside){//(The user has exited)
			data.println(" has exited");
			lcd.print("GoodBye");
		}else{//(The user has entered)
			data.println(" has entered");
			lcd.print("Welcome");
		}
		data.close();//Close the file
		lcd.setCursor(0,1),lcd.print("User #"),lcd.print(finger.fingerID);//Show the user number on the LCD screen
		if(gb){//Delete some of `LOG.txt` if it exceeds 1 Gigabytes
			data.open("LOG.txt",FILE_READ);//Open `LOG.txt`
			tmp.open("TMP",FILE_WRITE);//Open `TMP`
			while(!data||!tmp);//Stop if failed to open any of the files
			while(data.read()!='\n');//Skip the first line of `LOG.txt`
			while(data.available())//Copy the rest of `LOG.txt` to `TMP`
				tmp.write(data.read());
			data.close();//Close `LOG.txt`
			tmp.close();//Close `TMP`
			sd.remove("LOG.txt");//Delete `LOG.txt`
			data.open("LOG.txt",FILE_WRITE);//Open a new `LOG.txt`
			tmp.open("TMP",FILE_READ);//Open `TMP`
			while(!data&&!tmp);//Stop if failed to open any of the files
			while(tmp.available())//Copy everything from `TMP` to `LOG.txt`
				data.write(tmp.read());
			data.close();//Close `LOG.txt`
			tmp.close();//Close `TMP`
			sd.remove("TMP");//Delete `TMP`
		}
		digitalWrite(2,HIGH);//Make a beeping sound using the buzzer
		delay(1000);//Wait 1 second
		digitalWrite(2,LOW);//Stop the beeping
	}
	now=dst?rtc.now()+TimeSpan(0,1,0,0):rtc.now();//Set the current time
	lcd.clear(),lcd.print(now.year()),lcd.print('/'),lcd.print(now.month()),lcd.print('/'),lcd.print(now.day());//Show the date on the LCD screen
	if(now.hour()>9)//Indent 1 space if it's earlier than 10 P.M
		lcd.setCursor(4,1);
	else
		lcd.setCursor(5,1);
	lcd.print(now.hour()),lcd.print(':'),lcd.print(now.minute()/10),lcd.print(now.minute()%10),lcd.print(':'),lcd.print(now.second()/10),lcd.print(now.second()%10);//Show the time on the LCD screen
	return;
}
void resetData(){
	while(sd.exists("DATA"))//Attempt to delete the file until it's deleted
		sd.remove("DATA");
	File32 dir;
	char name[11];
	dir.open("SUM");
	for(File32 file=dir.openNextFile();file;file=dir.openNextFile()){
		file.getName(name,10);
		file.close();
		while(sd.exists("SUM/"+String(name)))
			sd.remove("SUM/"+String(name));
	}
	dir.close(),dir.open("LAST");
	for(File32 file=dir.openNextFile();file;file=dir.openNextFile()){
		file.getName(name,10);
		file.close();
		while(sd.exists("LAST/"+String(name)))
			sd.remove("LAST/"+String(name));
	}
	dir.close();
	return;
}
char getFingerprintID(){//Initialize the fingerprint reading function
	uint8_t p=finger.getImage();//Read a fingerprint
	if(p!=FINGERPRINT_OK)//Return false if no fingerprint is detected
		return 0;
	p=finger.image2Tz();//Convert fingerprint
	if(p!=FINGERPRINT_OK)//Return false if convertion failed
		return 0;
	p=finger.fingerFastSearch();//Search for the fingerprint
	if(p!=FINGERPRINT_OK){//Return if there's no match for the detected fingerprint
		lcd.clear(),lcd.print("No match!");//Alert on the LCD screen
		delay(1000);//Wait 1 second
		return 0;//Return false
	}
	return 1;//Return true
}
void print(const String message,const char Switch,const char error,const char Delay){//Initialize the printing function
	if(Switch){//Show the message on the LCD screen if the switch is enabled
		lcd.clear();//Clear the screen
		if(error)//Show 'Error!' on the screen if this is an error message
			lcd.setCursor(5,0),lcd.print("Error!");
		else
			lcd.print(message);//Show the message on the screen if this is a normal one
		delay(Delay*1000);//Wait if needed
	}else//Print the message if the switch is not enabled
		Serial.println(message);
	return;
}
void getFingerprintEnroll(const unsigned id,const char Switch){//Initialize the fingerprint enroll function
	uint8_t p=-1;//Initialize fingerprint sensor outputter
	if(Switch)//Notify on the LCD screen if the switch is enabled
		lcd.clear(),lcd.print("Waiting for a"),lcd.setCursor(0,1),lcd.print("valid finger");
	else//Notify
		Serial.print("Waiting for a valid finger to enroll as #"),Serial.println(id);
	while(p!=FINGERPRINT_OK){//Read until a fingerprint has been read
		if(Switch&&digitalRead(3))//Return if the switch is disabled
			return;
		p=finger.getImage();//Read a fingerprint
		switch(p){
			case FINGERPRINT_OK://(A fingerprint is detected)
				print("Image taken",Switch,0,1);
				break;
			case FINGERPRINT_NOFINGER://(No fingerprint is detected)
				break;
			case FINGERPRINT_PACKETRECIEVEERR://(Communication error)
				print("Communication error",Switch,1,0);
				break;
			case FINGERPRINT_IMAGEFAIL://(Imaging error)
				print("Imaging error",Switch,1,0);
				break;
			default://(Unknown error)
				print("Unknown error",Switch,1,0);
		}
	}
	p=finger.image2Tz(1);//Convert the fingerprint
	switch(p){
		case FINGERPRINT_OK://(Fingerprint is converted)
			print("Image converted",Switch,0,1);
			break;
		case FINGERPRINT_IMAGEMESS://(Image was too messy)
			print("Image too messy",Switch,1,1);
			return;
		case FINGERPRINT_PACKETRECIEVEERR://(Communiation error)
			print("Communication error",Switch,1,1);
			return;
		case FINGERPRINT_FEATUREFAIL://(Feature fail)
		case FINGERPRINT_INVALIDIMAGE://(Invalid image)
			print("Could not find fingerprint features",Switch,1,1);
			return;
		default://(Unknown error)
			print("Unknown error",Switch,1,1);
			return;
	}
	print("Remove finger",Switch,0,0);//Notify to remove finger
	delay(2000);//Wait 2 seconds
	p=0;//Set the outputter to 0
	while(p!=FINGERPRINT_NOFINGER)//Read until no fingerprint is detected
		p=finger.getImage();
	if(Switch){//Notify on the LCD screen if the switch is enabled
		lcd.clear(),lcd.print("ID #"),lcd.print(id);
		delay(1000);
	}else//Notify
		Serial.print("ID #"),Serial.println(id);
	p=-1;//Set outputter to -1
	if(Switch)//Notify on the LCD screen if the switch is enabled
		lcd.clear(),lcd.print("Place same"),lcd.setCursor(0,1),lcd.print("finger again");
	else//Notify
		Serial.println("Place same finger again");
	delay(1000);//Wait 1 second
	while(p!=FINGERPRINT_OK){//Read until a fingerprint is detected
		if(digitalRead(3))//Return if the switch is disabled
			return;
		p=finger.getImage();//Read a fingerprint
		switch(p){
			case FINGERPRINT_OK://(A fingerprint is detected)
				print("Image taken",Switch,0,1);
				break;
			case FINGERPRINT_NOFINGER://(No fingetprint is detected)
				break;
			case FINGERPRINT_PACKETRECIEVEERR://(Communication error)
				print("Communication error",Switch,1,0);
				break;
			case FINGERPRINT_IMAGEFAIL://(Imaging error)
				print("Imaging error",Switch,1,0);
				break;
			default://(Unknown error)
				print("Unknown error",Switch,1,0);
		}
	}
	p=finger.image2Tz(2);//Convert the second fingerprint
	switch(p){
		case FINGERPRINT_OK://(Fingerprint is converted)
			print("Image converted",Switch,0,1);
			break;
		case FINGERPRINT_IMAGEMESS://(Image was too messy)
			print("Image too messy",Switch,1,1);
			return;
		case FINGERPRINT_PACKETRECIEVEERR://(Communication error)
			print("Communication error",Switch,1,1);
			return;
		case FINGERPRINT_FEATUREFAIL://(Feature fail)
		case FINGERPRINT_INVALIDIMAGE://(Invalid image)
			print("Could not find fingerprint features",Switch,1,1);
			return;
		default://(Unknown error)
			print("Unknown error",Switch,1,1);
			return;
	}
	p=finger.createModel();//Create model
	switch(p){
		case FINGERPRINT_OK://(Fingerprints matched)
			print("Prints matched!",Switch,0,1);
			break;
		case FINGERPRINT_PACKETRECIEVEERR://(Communication error)
			print("Communication error",Switch,1,1);
			return;
		case FINGERPRINT_ENROLLMISMATCH://(Fingerprints didn't match)
			print("Fingerprints did not match",Switch,1,1);
			return;
		default://(Unknown error)
			print("Unknown error",Switch,1,1);
			return;
	}
	p=finger.storeModel(id);//Store model
	switch(p){
		case FINGERPRINT_OK://(Model stored)
			unsigned long long formerTemplateCount=finger.templateCount;//Save previous number of templates
			finger.getTemplateCount();//Get the number of templates
			if(finger.templateCount > formerTemplateCount){//Add the new user if the current & the previous number of templates differ
				data.open("DATA",FILE_WRITE);//Open `DATA`
				while(!data);//Stop if failed to open the file
				data.write('\0');//Write `00000000` to the file
				data.close();//Close the file
			}
			print("Stored!",Switch,0,1);//Notify
			break;
		case FINGERPRINT_PACKETRECIEVEERR://(Communication error)
			print("Communication error",Switch,1,1);
			break;
		case FINGERPRINT_BADLOCATION://(Bad location)
			print("Could not store in that location",Switch,1,1);
			break;
		case FINGERPRINT_FLASHERR://(Flash error)
			print("Error writing to flash",Switch,1,1);
			break;
		default://(Unknow error)
			print("Unknown error",Switch,1,1);
	}
	return;
}