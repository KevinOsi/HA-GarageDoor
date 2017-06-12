#include <Arduino.h>
#include <esp8266wifi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <esp8266webserver.h>
#include <Wire.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "default.h" //setup your defaults for local servers


// WIFI parameters
const char* SSID = MY_WIFI_SSID;  // your wifi SSID
const char* PASSWORD = MY_WIFI_PASSWORD; //your WIFI password
String NodeRedAlive = "CONNECTED";
String MyIP = "";

//NPT config
WiFiUDP ntpUDP;
NTPClient TimeClient(ntpUDP, "ca.pool.ntp.org", -21600 , 60000); // use canadian pool and set offset for local -6 hours


// MQTT Config
const char* BROKER_MQTT = MY_BROKER_MQTT_IP;
int BROKER_PORT = MY_BROKER_PORT;
const char* MQTT_USER = MY_MQTT_USER;
const char* MQTT_PASS = MY_MQTT_PASS;


WiFiClient espClient;
PubSubClient MQTT(espClient); // MQTT client setup
String MQTTConnected = "";


//Setup
String DeviceID = "ESP Garage"; //set unique device name for each device
String SystemID = "Sensor-Ctrl Garage";

//pin config
int OpenPin = 16;  //D0 Pin send open cmmd
int LockOutPin = 14;  //D5 lockout pin
int DoorOpenStatePin = 12; //D6 Pin get open contact state
int DoorCloseStatePin = 13;	//D7 Pin  gets close contact state

//MQTT topics
String TopicDeviceAll = "Home/Garage/1/Relays";  //sub comands to device
String TopicStatusNR = "Home/Status/WatchDog/1";  //node red health

String TopicStatusAll = "Home/Garage/1/Status"; //pub  device feedback on current state
String TopicStatusHVAC = "Home/HVAC/1/Sensors/1/Garage/1/Status";  //pub temperature
String TopicStatusDevice = "Home/Garage/1/Status/WatchDog"; //watchdog feedback


//Web server
ESP8266WebServer webServer(80);
String myPage = "";


//Timer Setup
double WDTimer = 0;
double TMPTimer = 0;

//Classes
class DoorControl{
	public:
		int DoorState;  //door's current state (0 closed, 1 Open, 2 in travel)
		int OpenPn; //Pin for door control
		int DoorOpenPn;  //input pin for door close state
		int DoorClosePn;  //input pin for door close state
		int LockoutPn; //
		bool DoorLockoutState;
		DoorControl();
		bool ToggleDoor();
		bool GetDoorState();
		bool Publish();
		bool DoorLockout(bool NewState);
};

DoorControl::DoorControl(){

	this -> DoorState = 0;
	this -> DoorLockoutState = 1;
	this -> OpenPn = OpenPin;
	this -> DoorOpenPn = DoorOpenStatePin;
	this -> DoorClosePn = DoorCloseStatePin;
	this -> LockoutPn = LockOutPin;
}

bool DoorControl::ToggleDoor(){

	//set pin low, delay and return to high (relay off on high!)
	digitalWrite(this->OpenPn, 0);
	delay(500);
	digitalWrite(this->OpenPn,1);


	Serial.println("Sending a door close command");

	return 1;
}

bool DoorControl::GetDoorState(){

	bool openState = 0;
	bool closeState = 0;


	openState = digitalRead(this -> DoorOpenPn);
	closeState = digitalRead(this -> DoorClosePn);

	//Serial.println("Reading pins - Open? " + String(this -> DoorOpenPn) + " - " + String(openState) + " close pin " + String(this -> DoorClosePn) + " - "  + String(closeState) );

	if(!openState && !closeState){  //both high, LOW on pins, in travel
		DoorState = 2;
		Serial.println("Hey the door is in Travel");
		return 1;
	}
	else if(openState && !closeState){
		DoorState = 1;
		Serial.println("Hey the door is Open");
		return 1;

	}
	else if(!openState && closeState) {
		DoorState = 0;
		Serial.println("Hey the door is Closed");
		return 1;
	}
	else{

		Serial.println("Door state read Error");
		return 0;
	}



}

bool DoorControl::Publish(){

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

	String BufferTxt = "{\"Device\" : \"" + DeviceID
			+ "\" , \"Status\" : \"" + StrStatus + "\" , \"Lockout\" : " + String(this->DoorLockoutState) + "\}";

	//Debug
	// Serial.println(BufferTxt);

	if (!MQTT.publish(TopicStatusAll.c_str(), BufferTxt.c_str(),
			sizeof(BufferTxt))) {

		Serial.println("MQTT pub error  !");
	}

	return 1;
}

bool DoorControl::DoorLockout(bool NewState){


	this-> DoorLockoutState  = NewState;

	digitalWrite(this-> LockoutPn, NewState);


	return 1;
}

// setup door
DoorControl myDoor;


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

TempSens::TempSens(){
	this -> Temp = 0;
	this -> Hum = 0;
	this -> sclPin = 5;
	this -> sdaPin = 4;

	Wire.begin(sdaPin, sclPin);
}

bool TempSens::Publish(){

	String myString = "";

	myString = "{\"Device\" : \""  + DeviceID + "\" , \"System\" : \"" + SystemID + "\" , \"Temperature\" : " + String(this -> Temp)
			+ ", \"Humidity\" : " + String(this -> Hum) + "}";


	Serial.println(myString);


	if(!MQTT.publish(TopicStatusHVAC.c_str() , myString.c_str())){

		Serial.println("MQTT - Data push failed");
		return 0;
	}


	return 1;
}

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



// functions and stuff
 void initPins() {

	pinMode(OpenPin, OUTPUT);
	pinMode(LockOutPin, OUTPUT);
	pinMode(DoorOpenStatePin, INPUT);
	pinMode(DoorCloseStatePin, INPUT);
	digitalWrite(OpenPin, 1);  // needs to be high for relay closed
	digitalWrite(LockOutPin, 1); // needs to be high for relay closed

}

// Setup serial connection
void initSerial() {
	Serial.begin(115200);
}

//setup wifi connection
void initWiFi() {
	delay(10);
	Serial.println();
	Serial.print("Connecting: ");
	Serial.println(SSID);

	WiFi.begin(SSID, PASSWORD); // Wifi Connect
	while (WiFi.status() != WL_CONNECTED) {
		delay(100);
		Serial.print(".");
	}

	Serial.println("");
	Serial.print(SSID);
	Serial.println(" | IP ");
	Serial.println(WiFi.localIP());

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
	Serial.print("Topic ");
	Serial.print(topic);
	Serial.print(" | ");
	Serial.println(message);

	// JSON parser
	JsonObject& parsed = jsonBuffer.parseObject(message);
	if (!parsed.success()) {
		Serial.println("parseObject() failed");
		return;
	}




	if(strcmp(topic, TopicDeviceAll.c_str())){

		Serial.println("found it ");
	}
	Serial.println(String(topic) + "  and " + TopicDeviceAll);

		if(parsed["Action"] == 1){
				if(!myDoor.ToggleDoor()){
					Serial.println("the Toggle Failed!");
				}
				return; // kill loop on execute
		}

		if(parsed["LockOut"] == 1){
			if(!myDoor.DoorLockout(0)   ){  // NOTE!  relay is off on high
				Serial.println("the lockout Failed!");
			}

		}
		else if(parsed["LockOut"] == 0){
			Serial.println("lock out is off ");

			if(!myDoor.DoorLockout(1)   ){ // NOTE! relay is ON on low!
				Serial.println("the lockout Failed!");
			}

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
		Serial.print("Attempting MQTT Broker Connection: ");
		Serial.println(BROKER_MQTT);
		if (MQTT.connect(DeviceID.c_str(), MQTT_USER, MQTT_PASS)) {   // set unique name
			Serial.println("Connected");

			MQTT.subscribe(TopicDeviceAll.c_str(), 1);
			MQTT.subscribe(TopicStatusNR.c_str(), 1);


			MQTTConnected = "CONNECTED";
		} else {
			Serial.println("Connection Failed");
			Serial.println("Attempting Reconnect in 2 seconds");

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

const char Header[] =
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/materialize/0.98.2/css/materialize.min.css\">"
"<link href=\"https://fonts.googleapis.com/icon?family=Material+Icons\" rel=\"stylesheet\">"
"<nav><div class=\"nav-wrapper teal\">"
"<a href=\"\" class=\"brand-logo\">Irrigation System</a>"
"<ul id=\"nav-mobile\" class=\"right hide-on-med-and-down\"></ul>"
"</div></nav>"
"<div class=\"card teal lighten-2\">"
"<div class=\"card-content\">"
"<span class=\"card-title\">System Info:</span>"
"<p>Current System Information</p>"
"</div>"
"<ul class=\"collection\">"
"<li class=\"collection-item avatar\">"
"<i class=\"material-icons circle blue\">import_export</i>"
"<a class=\"collection-item\"><span class=\"badge\">"
;

const char Header2[] =
"</span>MQTT</a>"
"</li>"
"<li class=\"collection-item avatar\">"
"<i class=\"material-icons circle blue\">developer_board</i>"
"<a href=\"http://"
;


const char Header21[] =
":1880\" class=\"collection-item\"><span class=\"badge\">"
;


const char Header3[] =
"</span>NodeRed</a>"
"</li>"
"<li class=\"collection-item avatar\">"
"<i class=\"material-icons circle blue\">blur_on</i>"
"<a class=\"collection-item\"><span class=\"badge\">"
"<a class=\"collection-item\">System Status</a>"
"<table class=\"striped\">"
"<tbody>"
"<tr><td>Current Temp</td><td>"
;

const char Header4[] =
"</td></tr>"
"<tr><td>Current Humidity</td><td>"
;

const char Body1[] =
"</td></tr>"
"</tbody>"
"</table>"
"</li>"
"</ul>"
"</div>"
"<div class=\"card teal lighten-2\">"
"<div class=\"card-content\">"
"<span class=\"card-title\">MQTT Topics</span>"
"<p>Current Topics being subscribed and published to</p>"
"</div>"
"<ul class=\"collection\">"
"<li class=\"collection-item avatar\">"
"<i class=\"material-icons circle blue\">file_download</i>"
"<table class=\"striped\">"
"<thead>"
"<tr><th>Subscriptions</th></tr>"
"</thead>"
"<tbody>"
"<tr><td>"
;

const char Table1[] =
"</td></tr>"
"<tr><td>"
;

const char Body2[] =
"</td></tr>"
"</tbody>"
"</table>"
"</li>"
"<li class=\"collection-item avatar\">"
"<i class=\"material-icons circle blue\">file_upload</i>"
"<table class=\"striped\">"
"<thead>"
"<tr><th>Publications</th></tr>"
"</thead>"
"<tbody>"
"<tr><td>"
;

const char Body3[] =
"</td></tr>"
"</tbody>"
"</table>"
"</li>"
"</ul>"
"</div>"
;


void BuildPage()
{
	myPage = "";

	  myPage = Header + MQTTConnected + Header2 + BROKER_MQTT + Header21 + NodeRedAlive + Header3 + "TEMP" + Header4 + "HUM" +
	  			  Body1 + TopicDeviceAll + Table1 + TopicStatusNR  +
	  			  Body2 + TopicStatusAll + Table1 + TopicStatusDevice +
	  			  Body3 ;



}

void handle_root() {


    BuildPage();
	webServer.send(200, "text/html", myPage);



}

void startWebserver()
{
    webServer.on("/", handle_root);
    webServer.begin();
    Serial.println("setting up web server");
}

void returnFail(String msg)
{
  webServer.sendHeader("Connection", "close");
  webServer.sendHeader("Access-Control-Allow-Origin", "*");
  webServer.send(500, "text/plain", msg + "\r\n");
}

void handleSubmit(){

	int rebuild = 0;  // flag if changes then rebuild;


	// checks arguments and then sends in new commands


	//if there are changes and a rebuild is called activate
	if (rebuild) {
		BuildPage();
		webServer.send(200, "text/html", myPage);

	}


}

void setup() {



  initPins();
  initSerial();

  initWiFi();
  initMQTT();

  // start webserver!
  startWebserver();

  //gpio 5 and 4 ???
  //const int sclPin = 5;
  //const int sdaPin = 4;
  //Wire.begin(sdaPin, sclPin);

  TimeClient.begin();
  TimeClient.update();
  Serial.println("Starup time - Epoch time " + String(TimeClient.getEpochTime()) + " - Formated time - Day" + TimeClient.getDay() + " - " + TimeClient.getFormattedTime());

  WDTimer = millis();
  TMPTimer = millis();

}

void WatchDogTimer() {

	String BufferTxt = "{\"Device\" : \"" + DeviceID
			+ "\" , \"Status\" : \"Alive\" ,  \"TimeStamp\" : "
			+ String(TimeClient.getEpochTime()) + " , \"IP\" : \"" + MyIP + "\"}";

	//Debug
	// Serial.println(BufferTxt);

	if (!MQTT.publish(TopicStatusDevice.c_str(), BufferTxt.c_str(),
			sizeof(BufferTxt))) {

		Serial.println("MQTT pub error  !");
	}

}

void loop() {


  webServer.handleClient();


   if (!MQTT.connected()) {
		reconnectMQTT(); // Retry Worker MQTT Server connection
	}
	recconectWiFi(); // Retry WiFi Network connection
	MQTT.loop();


	//check if time stamp is active on first in queue, else empty


	if(millis() > (TMPTimer + 5000)){


		TMPTimer = millis();

		myDoor.GetDoorState();
		myDoor.Publish();

		mySensors.GetData();
		mySensors.Publish();

	}



	//send out watchdog timer update every 20 seconds
	if(millis() > (WDTimer + 20000)){

		WDTimer = millis();
		TimeClient.update();
		WatchDogTimer();

	}


}


