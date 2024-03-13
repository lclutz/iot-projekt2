#!/usr/bin/env python3
#Script subscribiert vom publisher und 
#exportiert die Daten in die Datenbank
#OriginalScript von: https://schroederdennis.de/monitoring/tasmota-mqtt-daten-mit-grafana-visualisieren-python-influxdb-script/
#Script wurde angepasst Tosques 30.1.2023

import json
import datetime
from typing import NamedTuple
 
import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient

INFLUXDB_ADDRESS = 'localhost'
INFLUXDB_USER = ''
INFLUXDB_PASSWORD = ''
INFLUXDB_DATABASE = 'sensor_daten'

MQTT_ADDRESS = 'localhost'
MQTT_USER = ''
MQTT_PASSWORD = ''
MQTT_TOPIC = 'haus/temperatur'
MQTT_CLIENT_ID = 'influxbridge'

influxdb_client = InfluxDBClient(INFLUXDB_ADDRESS, 8086, INFLUXDB_USER, INFLUXDB_PASSWORD, None)

def on_connect(client, userdata, flags, rc):
    """ The callback for when the client receives a CONNACK response from the server."""
    print('Connected with result code ' + str(rc))
    client.subscribe(MQTT_TOPIC)


def on_message(client, userdata, msg):
    print("erhaltene daten auf dem Topic: " + msg.topic)
    receiveTime=datetime.datetime.utcnow()
    message=msg.payload.decode("utf-8")
    isFloatValue=False
    try:
        val=float(message)
        isFloatValue=True
    except:
        print("Datum kann nicht in UTF-8 konvertiert werden" + message)
        isFloatValue=False
    if isFloatValue:
        print(str(receiveTime) + ":" + msg.topic + " " + str(val))
    topic=msg.topic
    print("hier das topic vor if: " + topic)

    #hier die Inserts in die Datenbank
    #mittels JSON Objekt

    if "temperatur" in topic:
        measure = json.dumps({"temperatur": message})
    elif "humidity" in topic:
        measure = json.dumps({"humidity": message})

    influxdb_client.write_points(measure)
    print("Daten in Datenbank geschrieben: " + measure)

def _init_influxdb_database():
    databases = influxdb_client.get_list_database()
    if len(list(filter(lambda x: x['name'] == INFLUXDB_DATABASE, databases))) == 0:
        influxdb_client.create_database(INFLUXDB_DATABASE)
    influxdb_client.switch_database(INFLUXDB_DATABASE)
 
def main():
    _init_influxdb_database()
 
    mqtt_client = mqtt.Client(MQTT_CLIENT_ID)
    mqtt_client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
 
    mqtt_client.connect(MQTT_ADDRESS, 1883)
    mqtt_client.loop_forever()
 
 
if __name__ == '__main__':
    print('MQTT to InfluxDB bridge')
    main()
