/*
	HA-GarageDoor

	An ESP8266 based Garage Door Controller and temperature Monitor developed for the wemos D1 mini lite
	,  SHT30 temperature and Humidity sensor and 2 channel relay


	author KevinOsi
	version 1.1
	date Dec 12, 2019


*/

#include <Arduino.h>
#include <esp8266wifi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <esp8266webserver.h>
#include <Wire.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "webservdata.h" //stores the generic junk for the webserver
#include "default.h" //setup your defaults for local servers

/* **************************

Constants and other declarations


************************** */

// WIFI parameters
const char* SSID = MY_WIFI_SSID;  // Use your wifi SSID
const char* PASSWORD = MY_WIFI_PASSWORD; // Use your WIFI password
const char* HostName = "ESPGarage"; //Host name to help ID
String NodeRedAlive = "CONNECTED"; // NEED TO DUMP THIS FUNCTION SET
String MyIP = "";

//NPT config
WiFiUDP ntpUDP;
NTPClient TimeClient(ntpUDP, "ca.pool.ntp.org", -21600 , 60000); // uses Canadian pool and set offset for local -6 hours


// MQTT Config
const char* BROKER_MQTT = MY_BROKER_MQTT_IP;  //setup your MQTT server settings
int BROKER_PORT = MY_BROKER_PORT;
const char* MQTT_USER = MY_MQTT_USER;
const char* MQTT_PASS = MY_MQTT_PASS;

//Wif setup
WiFiClient espClient;
PubSubClient MQTT(espClient); // MQTT client setup
String MQTTConnected = "";


//Setup
String DeviceID = "ESP Garage"; //set unique device name for each device
String SystemID = "Sensor-Ctrl Garage"; //Family of devices for MQTT


//pin config
int OpenPin = 16;  //D0 Pin send open cmmd
int LockOutPin = 14;  //D5 lockout pin
int DoorOpenStatePin = 12; //D6 Pin get open contact state
int DoorCloseStatePin = 13;	//D7 Pin  gets close contact state

//MQTT topics
String TopicDeviceAll = "/Home/Garage/1/Relays";  //sub commands to device
String TopicStatusNR = "/Home/Status/WatchDog/1";  //node red health

String TopicStatusAll = "/Home/Garage/1/Status"; //pub  device feedback on current state
String TopicStatusHVAC = "/Home/HVAC/1/Sensors/1/Garage/1/Status";  //pub temperature
String TopicStatusDevice = "/Home/Garage/1/Status/WatchDog"; //watch dog feedback


//Web server
ESP8266WebServer webServer(80);
String myPage = "";


//Timer Setup
double WDTimer = 0;
double TMPTimer = 0;
bool DoorStateRefresh =0;


/* **************************

Classes:
	a Logger control class
	a door control, read and publish class
	a temperature/Humidty reading and publish class

************************** */


// Logger function, holds all logs to console, publishes to serial local and to web at /log
class LoggerControl{
	public:
		int Logs;  //Total logged entries
		String LogEntries[25]; //the character arrays for the log entries, note tructated to 256 chars
		bool Log(String NewEntry);  // function to add new entries
		LoggerControl();
};


//keeps a shift register of logs for publishing, initialize as blank
LoggerControl::LoggerControl() {
	this -> Logs = 0;

	for(int i = 0; i < 25; i++){
		this -> LogEntries[i] = "";
	}


}

//add a new entry into the shift register and inc the remainder, drop the last one
bool LoggerControl::Log(String NewEntry){

	// inc log number
	this -> Logs++;

	Serial.println("Log #" + String(this-> Logs) + ": " + NewEntry);


	//shift values
	for(int i = 24; i > 0; i--){

		this -> LogEntries[i] = this -> LogEntries[i-1];

	}


	this -> LogEntries[0] = "Log #" + String(this-> Logs) + ": " + NewEntry;

	return 1;
}

//setup logger control
LoggerControl myLog;



//Door control class, reads and keeps door states, publishes results
class DoorControl{
	public:
		int DoorState;  //door's current state (0 closed, 1 Open, 2 in travel)
		int OpenPn; //Pin for door control
		int DoorOpenPn;  //input pin for door close state
		int DoorClosePn;  //input pin for door close state
		int LockoutPn; //
		bool DoorLockoutState;
		String DoorStateStr; //Door state string format
		String DoorLockoutStateStr; //Lock out state string format
		DoorControl();
		bool ToggleDoor();
		bool GetDoorState();
		bool Publish();
		bool DoorLockout(bool NewState);
		void ISR_OpenPin();
		void ISR_ClosePin();
	private:
		char debounceISR_OpenPin;
		char debounceISR_ClosePin;

};


//initialize the door control states
DoorControl::DoorControl(){

	this -> DoorState = 0;
	this -> DoorLockoutState = 1;
	this -> OpenPn = OpenPin;
	this -> DoorOpenPn = DoorOpenStatePin;
	this -> DoorClosePn = DoorCloseStatePin;
	this -> LockoutPn = LockOutPin;
	this -> DoorStateStr = "null";
	this -> DoorLockoutStateStr = "Off";
}

//toggles the door, on an action writes the the appropriate pin to activate the relay
bool DoorControl::ToggleDoor(){

	//set pin low, delay and return to high (relay off on high!)
	digitalWrite(this->OpenPn, 0);
	delay(500);
	digitalWrite(this->OpenPn,1);


	//Serial.println("Sending a door close command");
	myLog.Log("Sending a door Toggle Command");

	return 1;
}

//reads the door state via the current pin setting, sets readable string values
bool DoorControl::GetDoorState(){

	bool openState = 0;
	bool closeState = 0;


	openState = digitalRead(this -> DoorOpenPn);
	closeState = digitalRead(this -> DoorClosePn);

	//Serial.println("Reading pins - Open? " + String(this -> DoorOpenPn) + " - " + String(openState) + " close pin " + String(this -> DoorClosePn) + " - "  + String(closeState) );

	if(!openState && !closeState){  //both high, LOW on pins, in travel
		DoorState = 2;
		this ->DoorStateStr = "In Travel";
		//Serial.println("Hey the door is in Travel");
		myLog.Log("Reading Pins, Door is in Travel");
		return 1;
	}
	else if(openState && !closeState){
		DoorState = 1;
		this ->DoorStateStr = "Open";
		//Serial.println("Hey the door is Open");
		myLog.Log("Reading Pins, Door is Open");
		return 1;

	}
	else if(!openState && closeState) {
		DoorState = 0;
		this ->DoorStateStr = "Closed";
		//Serial.println("Hey the door is Closed");
		myLog.Log("Reading Pins, Door is Close");
		return 1;
	}
	else{

		//Serial.println("Door state read Error");
		this ->DoorStateStr = "ERROR";
		myLog.Log("Reading Pins, Door State unknown, ERROR");
		return 0;
	}



}

//publishes a json document of the current state of the door and the lock out relay state
bool DoorControl::Publish(){

	/*
	int Status = this -> DoorState;
	String StrStatus = "";

	switch(Status){
	case 0:
		StrStatus = "Closed";
		break;
	case 1:
		StrStatus = "Open";
		break;
	case 2:
		StrStatus = "in Travel";
		break;
	default:
		StrStatus = "ERROR";
	};
	*/

	String BufferTxt = "{\"Device\" : \"" + DeviceID
			+ "\" , \"Status\" : \"" + this ->DoorStateStr + "\" , \"Lockout\" : " + this->DoorLockoutStateStr + "\}";

	//Debug
	// Serial.println(BufferTxt);
	myLog.Log(BufferTxt);


	if (!MQTT.publish(TopicStatusAll.c_str(), BufferTxt.c_str(),
			sizeof(BufferTxt))) {

		Serial.println("MQTT pub error  !");
	}

	return 1;
}


//lock out toggle to simulate a door open state and lock out the garage door from opening
bool DoorControl::DoorLockout(bool NewState){


	this-> DoorLockoutState  = NewState;

	digitalWrite(this-> LockoutPn, NewState);

	if( NewState == 0 ){

		this->DoorLockoutStateStr = "on";

	}
	else{

		this->DoorLockoutStateStr = "off";
	}


	return 1;
}




// setup door
DoorControl myDoor;

//a Class to store and read the temperature sensor values from the SHT30 module
class TempSens{
	public:
		float Temp;
		float Hum;

		TempSens();
		bool Publish();
		bool GetData();

	private:
		int sclPin;
		int sdaPin;

};

//intialization values
TempSens::TempSens(){
	this -> Temp = 0;
	this -> Hum = 0;
	this -> sclPin = 5;
	this -> sdaPin = 4;

	Wire.begin(sdaPin, sclPin);
}

//creates a json document and publishes via mqtt
bool TempSens::Publish(){

	String myString = "";

	myString = "{\"Device\" : \""  + DeviceID + "\" , \"System\" : \"" + SystemID + "\" , \"Temperature\" : " + String(this -> Temp)
			+ ", \"Humidity\" : " + String(this -> Hum) + "}";


	//Serial.println(myString);
	myLog.Log(myString);

	if(!MQTT.publish(TopicStatusHVAC.c_str() , myString.c_str())){

		//Serial.println("MQTT - Data push failed");
		myLog.Log("MQTT - Data push failed on Climate Temp sensor");
		return 0;
	}


	return 1;
}


//perform the read cycle from the sensor store data in the class
bool TempSens::GetData(){

	String myString;
	unsigned int data[6];
	float cTemp=0;
	float fTemp=0;
	float humidity=0;
	int	_address = 0x45;

	// Start I2C Transmission
	Wire.beginTransmission(_address);
	// Send measurement command
	Wire.write(0x2C);
	Wire.write(0x06);
	// Stop I2C transmission
	Wire.endTransmission();
	delay(500);

	// Request 6 bytes of data
	Wire.requestFrom(_address, 6);

	// Read 6 bytes of data
	// cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
	if (Wire.available() == 6)
	{
	data[0] = Wire.read();
	data[1] = Wire.read();
	data[2] = Wire.read();
	data[3] = Wire.read();
	data[4] = Wire.read();
	data[5] = Wire.read();
	}

	// Convert the data
	cTemp = ((((data[0] * 256.0) + data[1]) * 175) / 65535.0) - 45;
	fTemp = (cTemp * 1.8) + 32;
	humidity = ((((data[3] * 256.0) + data[4]) * 100) / 65535.0);

	this -> Temp  = cTemp;
	this -> Hum	= humidity;



	return 1;
}

//setup sensors
TempSens mySensors;





/* **************************

Main functions for program execution


************************** */



//ISRs for state changes to trigger an update to the door state rather than wait for the timer cycle
void ISR_OpenPin(){


	static unsigned long last_interrupt_time = 0;
	unsigned long interrupt_time = millis();

	// If interrupts come faster than 200ms, assume it's a bounce and ignore
	if (interrupt_time - last_interrupt_time > 200)
	{
		DoorStateRefresh = 1;
	}
	last_interrupt_time = interrupt_time;


	//DoorStateRefresh = 1;
}

void ISR_ClosePin(){


	static unsigned long last_interrupt_time2 = 0;
	unsigned long interrupt_time2 = millis();

	// If interrupts come faster than 200ms, assume it's a bounce and ignore
	if (interrupt_time2 - last_interrupt_time2 > 200)
	{
		DoorStateRefresh = 1;
	}
	last_interrupt_time2 = interrupt_time2;



	//DoorStateRefresh = 1;
}


//initialization of the pin modes and attachment of intterupts
 void initPins() {

	pinMode(OpenPin, OUTPUT);
	pinMode(LockOutPin, OUTPUT);
	pinMode(DoorOpenStatePin, INPUT);
	pinMode(DoorCloseStatePin, INPUT);
	digitalWrite(OpenPin, 1);  // needs to be high for relay closed
	digitalWrite(LockOutPin, 1); // needs to be high for relay closed

	//attach intertupts to pins

	attachInterrupt(digitalPinToInterrupt(DoorOpenStatePin), ISR_OpenPin , CHANGE);
	attachInterrupt(digitalPinToInterrupt(DoorCloseStatePin), ISR_ClosePin , CHANGE);

}

// Setup serial connection
void initSerial() {
	Serial.begin(115200);
}

//setup wifi connection
void initWiFi() {

	delay(10);

    //Log wifi connection start
    myLog.Log("WIFI Connecting to : " + String(SSID));


    WiFi.hostname(HostName);  //Set device host name
	WiFi.begin(SSID, PASSWORD); // Wifi Connect

	while (WiFi.status() != WL_CONNECTED) {
		delay(100);
		Serial.print(".");
	}
	Serial.println();

	//Log host name etc..
    myLog.Log("SSID " + String(SSID) + " | IP " + String(WiFi.localIP()) + "| HOST " + String(HostName) + " | MAC " + String(WiFi.macAddress()));


	MyIP = String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]);


}

// Receive messages
void mqtt_callback(char* topic, byte* payload, unsigned int length) {

	String message;
	StaticJsonBuffer<600> jsonBuffer;

	//split out payload in message, parse into JSON buffer;
	for (int i = 0; i < length; i++) {
		char c = (char) payload[i];
		message += c;
	}

	// serial debug JSON msg and topic recieved
	//Serial.print("Topic ");
	//Serial.print(topic);
	//Serial.print(" | ");
	//Serial.println(message);

	myLog.Log("New MQTT Sub " + String(topic) + " | " + String(message));

	// JSON parser
	JsonObject& parsed = jsonBuffer.parseObject(message);
	if (!parsed.success()) {
		// log failed parsing
		myLog.Log("MQTT SUB parseObject() failed");
		return;
	}



	//If the subscribed topic is the ALL topic, perform actions

	if(strcmp(topic, TopicDeviceAll.c_str())){


	}

		if(parsed["Action"] == 1){
				if(!myDoor.ToggleDoor()){
					//Serial.println("the Toggle Failed!");
					myLog.Log("Failed to Toggle The door");
				}
				return; // kill loop on execute
		}

		if(parsed["LockOut"] == 1){
			//Serial.println("lock out is on ");
			myLog.Log("Lock Out is turned on");

			if(!myDoor.DoorLockout(0)   ){  // NOTE!  relay is off on high
				//Serial.println("the lockout Failed!");
				myLog.Log("Lock Out failed to turn on");
			}

		}
		else if(parsed["LockOut"] == 0){
			//Serial.println("lock out is off ");
			myLog.Log("Lock Out is turned off");

			if(!myDoor.DoorLockout(1)   ){ // NOTE! relay is ON on low!
				//Serial.println("the lockout Failed!");
				myLog.Log("Lock Out failed to turn off");
			}

		}
		else {

			myLog.Log("Unrecognized commands on MQTT, check syntax");
		}

	message = "";
	Serial.println();
	Serial.flush();

}

// MQTT Broker connection
void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback);
}

void reconnectMQTT() {
	while (!MQTT.connected()) {
		//Serial.print("Attempting MQTT Broker Connection: ");
		//Serial.println(BROKER_MQTT);

		myLog.Log("Attempting MQTT Connection to : " + String(BROKER_MQTT));

		if (MQTT.connect(DeviceID.c_str(), MQTT_USER, MQTT_PASS)) {   // set unique name

			myLog.Log("Successfully Connected to MQTT Broker : " + String(BROKER_MQTT));

			MQTT.subscribe(TopicDeviceAll.c_str(), 1);
			MQTT.subscribe(TopicStatusNR.c_str(), 1);


			MQTTConnected = "CONNECTED";
		} else {
			//Serial.println("Connection Failed");
			//Serial.println("Attempting Reconnect in 2 seconds");

			myLog.Log("Connection to Broker Failed. retry in 2 seconds");

			MQTTConnected = "DISABLED";
			delay(2000);
		}
	}
}

void recconectWiFi() {

	while (WiFi.status() != WL_CONNECTED) {
		delay(100);
		Serial.print(".");

	}

}

//Webserver handler and builder
void BuildPage()
{
	myPage = "";

	  myPage = Header + MQTTConnected + Header2 + String(BROKER_MQTT) + Header21 + NodeRedAlive + Header3 +
			      String(mySensors.Temp) + Header4 + String(mySensors.Hum) + Header5 + myDoor.DoorStateStr + Header6 + myDoor.DoorLockoutStateStr +
	  			  Body1 + TopicDeviceAll + Table1 + TopicStatusNR  +
	  			  Body2 + TopicStatusAll + Table1 + TopicStatusDevice +
	  			  Body3 ;


}

//Build the log page hosted at /log
void BuildLog()
{
	myPage = "";

	myPage = "<meta http-equiv=\"refresh\" content=\"30\"/>This is the log file <br /><br />";

	for (int i = 0; i < 25; i++){

		myPage = myPage + myLog.LogEntries[i] + "<br />";


	}


}


//root handler for webpage function at /
void handle_root() {

    BuildPage();
	webServer.send(200, "text/html", myPage);


}

//log handler for webpage function at /log
void handle_log() {

    BuildLog();
	webServer.send(200, "text/html", myPage);


}


//intialize the webserver and setup the handlers
void startWebserver()
{
    webServer.on("/", handle_root);
    webServer.on("/log", handle_log);
    webServer.begin();

    //Serial.println("setting up web server");
    myLog.Log("Setting up web server on /");

}

void returnFail(String msg)
{
  webServer.sendHeader("Connection", "close");
  webServer.sendHeader("Access-Control-Allow-Origin", "*");
  webServer.send(500, "text/plain", msg + "\r\n");
}


//the handler for a page submission on the web page, not yet implemented but could setup some buttons
void handleSubmit(){

	int rebuild = 0;  // flag if changes then rebuild;


	// checks arguments and then sends in new commands


	//if there are changes and a rebuild is called activate
	if (rebuild) {
		BuildPage();
		webServer.send(200, "text/html", myPage);

	}


}


// creates a stay alive message with time stamp every 20 seconds back to listeners
void WatchDogTimer() {

	String BufferTxt = "{\"Device\" : \"" + DeviceID
			+ "\" , \"Status\" : \"Alive\" ,  \"TimeStamp\" : "
			+ String(TimeClient.getEpochTime()) + " , \"IP\" : \"" + MyIP + "\"}";

	//Debug
	//Serial.println(BufferTxt);
	myLog.Log("MQTT Sending :" + BufferTxt);


	if (!MQTT.publish(TopicStatusDevice.c_str(), BufferTxt.c_str(),
			sizeof(BufferTxt))) {

		//Serial.println("MQTT pub error  !");
		myLog.Log("MQTT pub error on Watchdog");
	}

}


//Setup and main loop
void setup() {


  //run init functions
  initPins();
  initSerial();
  initWiFi();
  initMQTT();

  // start webserver!
  startWebserver();

  // Start time client
  TimeClient.begin();
  TimeClient.update();
  //Serial.println("Startup time - Epoch time " + String(TimeClient.getEpochTime()) + " - Formated time - Day" + TimeClient.getDay() + " - " + TimeClient.getFormattedTime());
  myLog.Log("Startup time - Epoch time " + String(TimeClient.getEpochTime()) + " - Formated time - Day" + TimeClient.getDay() + " - " + TimeClient.getFormattedTime());

  // setup timers
  WDTimer = millis(); //read cycle timer
  TMPTimer = millis(); //watch  dog cycle timer



  //init sensor and states, publish base values
  myDoor.GetDoorState();
  mySensors.GetData();


}


//The main loop that should run forever
void loop() {


  webServer.handleClient();


   if (!MQTT.connected()) {
		reconnectMQTT(); // Retry Worker MQTT Server connection
	}
	recconectWiFi(); // Retry WiFi Network connection
	MQTT.loop();


	//Read and publish Cycle every 30 seconds
	if(millis() > (TMPTimer + 30000)){


		TMPTimer = millis();

		myDoor.GetDoorState();
		myDoor.Publish();

		mySensors.GetData();
		mySensors.Publish();

	}


    //  I dunno if i even need the watchdogs... //

	//send out watch dog timer update every 5 minutes
	//if(millis() > (WDTimer + 300000)){

	//	WDTimer = millis();
		//TimeClient.update();
	//	WatchDogTimer();

	//}

	if(DoorStateRefresh == 1){

		//get and publish door state on open/close action
		myDoor.GetDoorState();
		myDoor.Publish();

		DoorStateRefresh = 0;

	}

}


