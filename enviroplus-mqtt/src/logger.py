import logging, sys, collections, threading, traceback, math

import paho.mqtt.client as mqtt

try:
    # Transitional fix for breaking change in LTR559
    from ltr559 import LTR559
    ltr559 = LTR559()
except ImportError:
    import ltr559

from bme280 import BME280
from pms5003 import PMS5003
from enviroplus import gas


class EnvLogger:

    # altitude in feet to adjust air pressure, don't make it 0
    alt_ft  = 5400
    sensors = [
		['prox',	'proximity',		'', 			'Proximity', 		'm'],
		['lux', 	'lux', 			'illuminance', 		'Luminosity', 		'lx'],
		['temp', 	'temperature', 		'temperature', 		'Temperature', 		'°C'],
		['pressure', 	'pressure', 		'pressure', 		'Pressure', 		'hPa'],
		['hum', 	'humidity', 		'humidity', 		'Humidity', 		'%'],
		['gas_ox', 	'gas/oxidising',	'', 			'Gas/NO2',		'ppm'],
		['gas_red', 	'gas/reducing', 	'carbon_monoxide', 	'Gas/CO', 		'ppm'],
		['gas_nh3', 	'gas/nh3', 		'', 			'Ammonia', 		'ppm'],
		['pm10', 	'particulate/1.0',	'', 			'Particulate10',	'ug/m3'],
		['pm25', 	'particulate/2.5', 	'', 			'Particulate25',	'ug/m3'],
		['pm100',	'particulate/10.0', 	'', 			'Particulate100', 	'ug/m3']
		];


    def __init__(self, client_id, host, port, username, password, prefix, use_pms5003, num_samples):
        
        logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)

        self.bme280 = BME280()

        self.prefix = prefix

        self.connection_error = None
        self.client = mqtt.Client(client_id=client_id)
        self.client.on_connect = self.__on_connect
        self.client.username_pw_set(username, password)
        self.client.connect(host, port)
        self.client.loop_start()

        self.samples = collections.deque(maxlen=num_samples)
        self.latest_pms_readings = {}

        if use_pms5003:
            self.pm_thread = threading.Thread(target=self.__read_pms_continuously)
            self.pm_thread.daemon = True
            self.pm_thread.start()
    

    def __on_connect(self, client, userdata, flags, rc):
        errors = {
            1: "incorrect MQTT protocol version",
            2: "invalid MQTT client identifier",
            3: "server unavailable",
            4: "bad username or password",
            5: "connection refused"
        }

        if rc > 0:
            self.connection_error = errors.get(rc, "unknown error")


    def __read_pms_continuously(self):
        """Continuously reads from the PMS5003 sensor and stores the most recent values
        in `self.latest_pms_readings` as they become available.

        If the sensor is not polled continously then readings are buffered on the PMS5003,
        and over time a significant delay is introduced between changes in PM levels and 
        the corresponding change in reported levels."""

        pms = PMS5003()
        while True:
            try:
                pm_data = pms.read()
                self.latest_pms_readings = {
                    "particulate/1.0": pm_data.pm_ug_per_m3(1.0, atmospheric_environment=True),
                    "particulate/2.5": pm_data.pm_ug_per_m3(2.5, atmospheric_environment=True),
                    "particulate/10.0": pm_data.pm_ug_per_m3(None, atmospheric_environment=True),
                }
            except:
                print("Failed to read from PMS5003. Resetting sensor.")
                traceback.print_exc()
                pms.reset()


    def take_readings(self):
        gas_data = gas.read_all()

        red_r0 = 200000
        ox_r0 = 20000
        nh3_r0 = 750000

        red_in_ppm = math.pow(10, -1.25 * math.log10(gas_data.reducing/red_r0) + 0.64)
        ox_in_ppm = math.pow(10, math.log10(gas_data.oxidising/ox_r0) - 0.8129)
        nh3_in_ppm = math.pow(10, -1.8 * math.log10(gas_data.nh3/nh3_r0) - 0.163)
        
        adj_pressure= self.bme280.get_pressure()
        if self.alt_ft > 0:
          adj_pressure = adj_pressure + (self.alt_ft /30)

        readings = {
            "proximity": ltr559.get_proximity(),
            "lux": ltr559.get_lux(),
            "temperature": self.bme280.get_temperature(),
            "pressure": adj_pressure,
            "humidity": self.bme280.get_humidity(),
            "gas/oxidising": ox_in_ppm,
            "gas/reducing": red_in_ppm,
            "gas/nh3": nh3_in_ppm,
        }

        readings.update(self.latest_pms_readings)
        return readings


    def publishDiscovery(self):
        for sensor in self.sensors:
            topic = 'homeassistant/sensor/{}_{}/config'.format(self.prefix,sensor[0])

            payload = '{{"name":"Enviro {}", "unique_id":"{}_{}", "unit_of_measurement": "{}", "state_topic": "{}/{}" }}'.format(sensor[3], self.prefix, sensor[0], sensor[4], self.prefix, sensor[1])
            if len(sensor[2]) > 0:
                payload = '{{"name":"Enviro {}", "unique_id":"{}_{}", "device_class": "{}",  "unit_of_measurement": "{}", "state_topic": "{}/{}" }}'.format(sensor[3], self.prefix, sensor[0], sensor[2], sensor[4], self.prefix, sensor[1])

            #logging.debug('config:{}:{}', payload, topic)
            self.client.publish(topic, payload)

    def publish(self, topic, value):
        topic = self.prefix.strip("/") + "/" + topic
        self.client.publish(topic, str(value))


    def update(self, publish_readings=True):
        self.samples.append(self.take_readings())

        if publish_readings:
            for topic in self.samples[0].keys():
                value_sum = sum([d[topic] for d in self.samples])
                value_avg = value_sum / len(self.samples)
                self.publish(topic, value_avg)


    def destroy(self):
        self.client.disconnect()
        self.client.loop_stop()
