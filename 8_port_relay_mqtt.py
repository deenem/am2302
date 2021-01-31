
import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO


#GPIO Pins
RELAYS = [5,6,13,16,19,20,21,26]

MQTT_HOST="10.0.1.45"
MQTT_PORT=1883
MQTT_CLIENT="pirelay"
MQTT_USER="inverter"
MQTT_PWD="inverter"
UID="pirelay1"


def initRelays(client):
    for r in range(8):
        GPIO.setup(RELAYS[r], GPIO.OUT)
        GPIO.output(RELAYS[r], GPIO.HIGH)a
        state_topic = 'homeassistant/switch/{}/state/relay{}'.format(MQTT_CLIENT, r)
        client.publish(state_topic, 'OFF')    
        
def registerSwitches(client):
    for r in range(8):
        config_topic ='homeassistant/switch/{}_{}/config'.format(MQTT_CLIENT,r)
        state_topic = 'homeassistant/switch/{}/state/relay{}'.format(MQTT_CLIENT, r)
        name = 'Relay Switch {}'.format(r)
        command_topic = 'homeassistant/switch/{}/command/relay{}'.format(MQTT_CLIENT, r)
        device = '{{"name": "{}", "identifiers" : "{}" }}'.format(MQTT_CLIENT, UID)
        unique_id = '{}_{}'.format(UID,r)
        client.publish(config_topic,'{{"name":"{}", "device": {}, "unique_id":"{}", "state_topic": "{}", "command_topic":"{}"}}'.format(name, device, unique_id, state_topic, command_topic))

    
def on_publish(client, userdata, mid):
    print('published - {} - {} - {} '.format(client, userdata, mid))  
    
def on_connect(client, userdata, flags, rc):
    print('connected - {} - {} - {} - {}'.format(client, userdata, flags, rc))  
    # can registerevent listeners once connected
    registerSwitches(client)
    initRelays(client)
            
def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8") 
    
    print('message - #{}#{}# '.format(msg.topic, payload))

# get relay index
    idx = 0
    if (msg.topic.endswith('relay0')):
        idx = 0
    elif (msg.topic.endswith('relay1')):
        idx = 1
    elif (msg.topic.endswith('relay2')):
        idx = 2        
    elif (msg.topic.endswith('relay3')):
        idx = 3
    elif (msg.topic.endswith('relay4')):
        idx = 4
    elif (msg.topic.endswith('relay5')):
        idx = 5
    elif (msg.topic.endswith('relay6')):
        idx = 6
    elif (msg.topic.endswith('relay7')):
        idx = 7

# get ON or OFF payload        
    direction = GPIO.HIGH
    if payload == 'ON':
        direction = GPIO.LOW

# Change the relay
    GPIO.output(RELAYS[idx], direction)

# Publish new state
    state_topic = 'homeassistant/switch/{}/state/relay{}'.format(MQTT_CLIENT, idx)
    client.publish(state_topic, payload)    

def initRelays(client):
    for r in range(8):
        GPIO.setup(RELAYS[r], GPIO.OUT)
        GPIO.output(RELAYS[r], GPIO.HIGH)
        state_topic = 'homeassistant/switch/{}/state/relay{}'.format(MQTT_CLIENT, r)
        client.publish(state_topic, 'OFF')    
    

print('begin')    

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)


print('mqtt connect')
client = mqtt.Client(MQTT_CLIENT)
client.on_connect = on_connect
client.on_message = on_message
client.on_publish = on_publish
client.username_pw_set(username=MQTT_USER, password=MQTT_PWD)
print('connect')
client.connect(MQTT_HOST, port=MQTT_PORT )

client.subscribe('homeassistant/switch/{}/command/+'.format(MQTT_CLIENT))

client.loop_forever()
