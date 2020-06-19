import pifacedigitalio as p
import paho.mqtt.client as mqtt


sensors = [
    {
        'name': 'button' 
    }
]

def eventh(event):
    global mqtt_client
    print ("{}:{}".format(event.pin_num,event.direction))
    topic = ""
    state = ""
    if (event.pin_num == 4):
        topic = 'homeassistant/remote/buttonA'
    elif (event.pin_num == 5):
        topic = 'homeassistant/remote/buttonB'
    elif (event.pin_num == 6):
        topic = 'homeassistant/remote/buttonC'
    elif (event.pin_num == 7):
        topic = 'homeassistant/remote/buttonD'

    if (event.direction == 0):
        state = "OFF"
    else: 
        state = "ON"
    client.publish(topic, state)

def registerEventListeners():
    piface = p.PiFaceDigital()
    listen8 = p.InputEventListener(chip=piface)
    listen8.register(7, p.IODIR_RISING_EDGE, eventh)
    listen8.register(7, p.IODIR_FALLING_EDGE, eventh)
    listen8.register(6, p.IODIR_RISING_EDGE, eventh)
    listen8.register(6, p.IODIR_FALLING_EDGE, eventh)
    listen8.register(5, p.IODIR_RISING_EDGE, eventh)
    listen8.register(5, p.IODIR_FALLING_EDGE, eventh)
    listen8.register(4, p.IODIR_RISING_EDGE, eventh)
    listen8.register(4, p.IODIR_FALLING_EDGE, eventh)
    listen8.activate()

def registerSwitches(client):
    client.publish('homeassistant/binary_sensor/remoteButtonA/config', 
    '{"name":"Remote Button A", "state_topic": "homeassistant/remote/buttonA" }')
    client.publish('homeassistant/binary_sensor/remoteButtonB/config', 
    '{"name":"Remote Button B", "state_topic": "homeassistant/remote/buttonB" }')
    client.publish('homeassistant/binary_sensor/remoteButtonC/config', 
    '{"name":"Remote Button C", "state_topic": "homeassistant/remote/buttonC" }')
    client.publish('homeassistant/binary_sensor/remoteButtonD/config', 
    '{"name":"Remote Button D", "state_topic": "homeassistant/remote/buttonD" }')

    
def on_publish(client, userdata, mid):
    print('published - {} - {} - {} '.format(client, userdata, mid))  
    
def on_connect(client, userdata, flags, rc):
    print('connected - {} - {} - {} - {}'.format(client, userdata, flags, rc))  
    # can registerevent listeners once connected
    registerSwitches(client)
    registerEventListeners()
    
def on_message(client, userdata, msg):
    print('messaged -' + msg.topic+" "+str(msg.payload))

print('begin')
client = mqtt.Client('pirelay')
client.on_connect = on_connect
client.on_message = on_message
client.on_publish = on_publish
client.username_pw_set(username='inverter', password='inverter')
print('connect')
client.connect('10.0.1.45', port=1883 )
client.loop_forever()
