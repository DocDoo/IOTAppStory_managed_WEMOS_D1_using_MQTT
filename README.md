# IoTAppStory-WEMOS_D5_to_MQTT
 WEMOS D1 Mini based arduino solution converting 3.3V signal on D5 to MQTT message in configured MQTT topic and MQTT broker

Detect 3.3V input on D5 on WEMOS D1 Mini and send a MQTT message on the topic "HGV10/Huset/Fordoer/Ringklokke" as a result.

This app has a pretty specialized purpose which is to send MQTT messages to the MQTT topic "HGV10/Huset/Fordoer/Ringklokke" specified MQTT broker. The IP address, port, username and password allowing the app to connect to the broker is configured by the IoT App Story Configuration Mode.

The MQTT messages are sent when a 3.3V signal is detected on D5 of a WEMOS D1 Mini device.
