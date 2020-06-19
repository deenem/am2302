import Adafruit_DHT
import paho.mqtt.client as mqtt
import time


def registerSensors(client):
    client.publish('homeassistant/sensor/ext_temp/config',
                   '{"name":"External Temp", "device_class": "temperature", "unit_of_measurement": "°C", "state_topic": "homeassistant/ext_temp/state" }')
    client.publish('homeassistant/sensor/ext_humidity/config',
                   '{"name":"External Humidity", "device_class": "humidity", "unit_of_measurement": "%",   "state_topic": "homeassistant/ext_humidity/state" }')


def publishValues(temp, hum):
    client.publish('homeassistant/ext_temp/state', round(temp, 2))
    client.publish('homeassistant/ext_humidity/state', round(hum, 2))


def on_publish(client, userdata, mid):
    print('published - {} - {} - {} '.format(client, userdata, mid))


def on_connect(client, userdata, flags, rc):
    print('connected - {} - {} - {} - {}'.format(client, userdata, flags, rc))
    # can registerevent listeners once connected
    registerSensors(client)


def on_message(client, userdata, msg):
    print('messaged -' + msg.topic+" "+str(msg.payload))


print('begin')
client = mqtt.Client('exttemp')
client.on_connect = on_connect
client.on_message = on_message
client.on_publish = on_publish
client.username_pw_set(username='inverter', password='inverter')
print('connect')
client.connect('10.0.1.45', port=1883)
client.loop_start()
while (True):
    humidity, temperature = Adafruit_DHT.read_retry(Adafruit_DHT.AM2302, 4)
    print('read', humidity, temperature)
    publishValues(temperature, humidity)
    time.sleep(10)
