# HA-GarageDoor
My home automation garage door program for the Wemos D1 mini Lite, ESP8285 chip

Project uses MQTT control, Built in webserver for basic status - (control not yet implemented)

Features:

* door access

* limit switch state monitoring on door

* Lockout function for closed state


See Wiki page for hardware details


MQTT control:

open command example:

	Topic - Home/Garage/1/Relays
	Msg - {"Device":"ESP Garage","System":"Garage Door A","Action":1}


Lockout:

	MSG  - {"Device":"ESP Garage","System":"Garage Door A","LockOut":1}


typical return values

	MSG - { Device: "ESP Garage", System: "Sensor-Ctrl Garage", Temperature: 33.88, Humidity: 31.14 }
	MSG - { Device: "ESP Garage", Status: "Closed", Lockout: "on" }
