#!/usr/bin/env python3
#publishd die Temperatur und die Feuchtigkeit vom Sensor
#Tosques 28.1.2023

#import der notwendigen Biblioteken
import paho.mqtt.client as mqtt
import time
from influxdb import InfluxDBClient

#notwenide Variablen
dht22gpioPin=4

#IP-Adresse Broker
broker='localhost'
port=1883

publish_topic="haus"

retain_message=False

def on_connect(client, userdata, flags, rc):
	if rc==0:
		print("broker {} verbunden - returnCode={}".format(broker,rc))
	else:
		print("broker {} kann nicht verbunden werden - returnCode{}".format(broker, rc))

def on_message(client, userdata, message):
	global m
	print("jetzt in on_message")
	m="message_received ", str(message.payload.decode("utf-8"))
	print("message ", m)

#MQTT-Verbindung
client=mqtt.Client("MeinMQTT-Client")
client.on_connect=on_connect #Callback-Funktion einbinden
client.on_message=on_message
client.connect(broker, port)
client.loop_start()
	
print("Verbindung zu Broker {} wird aufgebaut".format(broker))

def get_temp():
	return 21.0

def get_humi():
	return 50.0

#publish-Abschnitt
while True:
	try:
		temperatur = get_temp()
		print("Topic:{}/temperatur -- Temperatur={}".format(publish_topic,temperatur))
		client.publish("{}/temperatur".format(publish_topic), "{:.1f}".format(temperatur))
		time.sleep(5)
		feuchtigkeit = get_humi()
		print("Topic:{}/feuchtigkeit -- Feuchtigkeit={}".format(publish_topic,feuchtigkeit))
		client.publish("{}/feuchtigkeit".format(publish_topic), "{:.1f}".format(feuchtigkeit))
		
		time.sleep(5)
	except RuntimeError as error:
		print(error.args[0])
		time.sleep(5)

	except KeyboardInterrupt:
		client.disconnect();
		client.loop_stop
	




