# RFM69 JSON MQTT Gateway
 
This projekt is a Gateway based on a ESP8266 and RFM69 radio which connects via MQTT to a broker and send/receives messages to RFM69 nodes.
The gateway has a web interface to konfigure itself, MQTT, nodes, firmware update, passwords ect.
Just download ESP flasher an flash *.bin and have fun...
 
### Board
- WEMOS mini D1 + RFM69
- Led to show MQTT publish
 
### Software Features
- Access Point on startup without valid network connection to configure WLan
- webinterface to configure the whole RFM69 Network
- Gateway remembers if Node is reachable (commands 17, 18, 19 from node)
- the Gateway cuts long json Messages to single Messages e.g. {"p_0":"1", "p_1":"1"} -> topic:rfmIn/networkId/nodeId/p_0 message:{"p_0":"1"} and topic:rfmIn/networkId/nodeId/p_1 message:{"p_1":"1"}
- publishes node informations like rssi, message counter (in future: dublicate messages, lost messages)
 
**MQTT configuration**
![alt text](https://github.com/boaschti/MQTT_WLan_RFM69_Gateway/blob/master/pictures/mqttConfig.jpg)
 
**Gateway configuration**
![alt text](https://github.com/boaschti/MQTT_WLan_RFM69_Gateway/blob/master/pictures/GatewayConfig.jpg)
 
**node configuration**
![alt text](https://github.com/boaschti/MQTT_WLan_RFM69_Gateway/blob/master/pictures/nodeConfig.jpg)

**setup new node**
![alt text](https://github.com/boaschti/MQTT_WLan_RFM69_Gateway/blob/master/pictures/learnNewNode.jpg)

**Gateway hardware**
![alt text](https://github.com/boaschti/MQTT_WLan_RFM69_Gateway/blob/master/pictures/gatewayHardware.jpg)