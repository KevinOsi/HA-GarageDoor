# HA-GarageDoor
My home automation garage door program for the Wemos D1 mini Lite, ESP8285 chip

Project uses MQTT control and a built in webserver for basic status (control not yet implemented)

Features:

* Temperature and humidity monitoring functions
* limit switch state monitoring on door, for accurate open, close and in travel positioning
* Open/close control
* Lockout function for closed state


Webserver:

	IP:80/  root displays information on topics
	IP:80/log  displays the last 25 log messages to assist with debugging and getting status updates on the device


MQTT control:

open command example:

	Topic - /Home/Garage/1/Relays
	Msg - { "Action": 1 }


Lockout:

	MSG  - { "LockOut": 1 }


typical return values

	Topic - /Home/HVAC/1/Sensors/1/Garage/1/Status 
	MSG - {"Device" : "ESP Garage" , "System" : "Sensor-Ctrl Garage" , "Temperature" : 9.03, "Humidity" : 57.47}
	Topic - /Home/Garage/1/Status
	MSG - {"Device" : "ESP Garage" , "Status" : "Closed" , "Lockout" : Off}

	
	
TODO:
* Add control function to the webserver
* create a better routine to specify travel to open or travel to close state rather than just a toggle to ensure door gets to desired state
* optimize webserver template (need to research better method to store data)