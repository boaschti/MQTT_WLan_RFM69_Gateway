/* RFM69 library and code by Felix Rusu - felix@lowpowerlab.com
// Get libraries at: https://github.com/LowPowerLab/
// Make sure you adjust the settings in the configuration section below !!!
// **********************************************************************************
// Copyright Felix Rusu, LowPowerLab.com
// Library and code by Felix Rusu - felix@lowpowerlab.com
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// You should have received a copy of the GNU General    
// Public License along with this program.
// If not, see <http://www.gnu.org/licenses></http:>.
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************/
// Required Modifications:
// 1. PubSubClient.h #define MQTT_MAX_PACKET_SIZE 256
// 2. add delay(1); in line 244 in RFM69.cpp to reset watchdog
// setup Gateway IP: 192.168.4.1





#define SERIAL_BAUD   115200

#include <ESP8266WiFi.h>
#include <RFM69.h>                //https://www.github.com/lowpowerlab/rfm69
#include <pgmspace.h>
#include <PubSubClient.h>


uint32_t reachableNode[8];

char RadioConfig[128];

// Default values
const char PROGMEM ENCRYPTKEY[] = "sampleEncryptKey";
const char PROGMEM MDNS_NAME[] = "rfm69gw1";
const char PROGMEM MQTT_BROKER[] = "raspberrypi";
const char PROGMEM RFM69AP_NAME[] = "RFM69-AP";
//Passwort und Benutzer zum Softwareupdate ueber Browser

const char PROGMEM UPDATEUSER[]		  = "B";//sw
const char PROGMEM UPDATEPASSWORD[] =	"B";//sw
const char PROGMEM MQTTPASSWORD[]   = "B";
const char PROGMEM MQTTUSER[]   = "B";

#define NEWNODE_NODEID    "254"
#define NETWORKID		      1  //the same on all nodes that talk to each other
#define NODEID			      1

//Match frequency to the hardware version of the radio
#define FREQUENCY		RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY      RF69_915MHZ
#define IS_RFM69HCW		false // set to 'true' if you are using an RFM69HCW module
#define POWER_LEVEL		31
#define showKeysInWeb	false
#define Led_user		0//sw

#define HiddenString "xxx"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
RFM69 radio;

// vvvvvvvvv Global Configuration vvvvvvvvvvv
#include <EEPROM.h>

struct _GLOBAL_CONFIG {
  uint32_t    checksum;
  char        rfmapname[32];
  char        mqttbroker[32];
  char        mqttclientname[32];
  char        mdnsname[32];
  uint32_t    ipaddress;  // if 0, use DHCP
  uint32_t    ipnetmask;
  uint32_t    ipgateway;
  uint32_t    ipdns1;
  uint32_t    ipdns2;
  char        encryptkey[16+1];
  uint8_t     networkid;
  uint8_t     nodeid;
  uint8_t     powerlevel; // bits 0..4 power level, bit 7 RFM69HCW 1=true
  uint8_t     rfmfrequency;
  char        updateUser[20];
  char        updatePassword[20];
  char        mqttPassword[20];
  char        mqttUser[20];  
};

#define GC_POWER_LEVEL    (pGC->powerlevel & 0x1F)
#define GC_IS_RFM69HCW  ((pGC->powerlevel & 0x80) != 0)

struct _GLOBAL_CONFIG *pGC;
// ^^^^^^^^^ Global Configuration ^^^^^^^^^^^

// vvvvvvvvv ESP8266 WiFi vvvvvvvvvvv
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager


void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void wifi_setup(void) {
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing. Wipes out SSID/password.
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  
  wifiManager.setConfigPortalTimeout(180);
  
  while(!wifiManager.autoConnect(pGC->rfmapname)) {
    
  }

  Serial.println("connected");
}

// ^^^^^^^^^ ESP8266 WiFi ^^^^^^^^^^^

// vvvvvvvvv Global Configuration vvvvvvvvvvv
uint32_t gc_checksum() {
  uint8_t *p = (uint8_t *)pGC;
  uint32_t checksum = 0;
  p += sizeof(pGC->checksum);
  for (size_t i = 0; i < (sizeof(*pGC) - 4); i++) {
    checksum += *p++;
  }
  return checksum;
}

void eeprom_setup() {
  EEPROM.begin(4096);
  pGC = (struct _GLOBAL_CONFIG *)EEPROM.getDataPtr();
  // if checksum bad init GC else use GC values
  if (gc_checksum() != pGC->checksum) {
      Serial.println("Factory reset");
      memset(pGC, 0, sizeof(*pGC));
      Serial.println("ENCRYPTKEY");
      strcpy_P(pGC->encryptkey, ENCRYPTKEY);
      Serial.println("RFM69AP_NAME");
      strcpy_P(pGC->rfmapname, RFM69AP_NAME);
      Serial.println("MQTT_BROKER");
      strcpy_P(pGC->mqttbroker, MQTT_BROKER);
      Serial.println("MDNS_NAME");
      strcpy_P(pGC->mdnsname, MDNS_NAME);
      Serial.println("mqttclientname");
      //bei der folgenden Zeile haengt sich der Chip auf: Exception (28)
      //strcpy(pGC->mqttclientname, WiFi.hostname().c_str());
      Serial.println("NETWORKID");
      pGC->networkid = NETWORKID;
      Serial.println("NODEID");
      pGC->nodeid = NODEID;
      Serial.println("powerlevel");
      pGC->powerlevel = ((IS_RFM69HCW)?0x80:0x00) | POWER_LEVEL;
      Serial.println("rfmfrequency");
      pGC->rfmfrequency = FREQUENCY;
      Serial.println("UPDATEPW");
      strncpy_P(pGC->updateUser, UPDATEUSER, 20);
      strncpy_P(pGC->updatePassword, UPDATEPASSWORD, 20);    
      strncpy_P(pGC->mqttUser, MQTTUSER, 20);
      strncpy_P(pGC->mqttPassword, MQTTPASSWORD, 20);       
      Serial.println("checksum");
      pGC->checksum = gc_checksum();
      Serial.println("EEPROM.commit");
      EEPROM.commit();
  }
}
// ^^^^^^^^^ Global Configuration ^^^^^^^^^^^

// vvvvvvvvv ESP8266 web sockets vvvvvvvvvvv
#include <ESP8266mDNS.h>

// URL: http://rfm69gw.local
MDNSResponder mdns;

void mdns_setup(void) {
  if (pGC->mdnsname[0] == '\0') return;

  if (mdns.begin(pGC->mdnsname, WiFi.localIP())) {
    Serial.println("MDNS responder started");
    mdns.addService("http", "tcp", 80);
    mdns.addService("ws", "tcp", 81);
  }
  else {
    Serial.println("MDNS.begin failed");
  }
  Serial.printf("Connect to http://%s.local or http://", pGC->mdnsname);
  Serial.println(WiFi.localIP());
}


static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
<title>RFM69 Gateway</title>
<style>
"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }"
</style>
<script>
var websock;
function start() {
  var rfm69nodes = [];
  websock = new WebSocket('ws://' + window.location.hostname + ':81/');
  websock.onopen = function(evt) { console.log('websock open'); };
  websock.onclose = function(evt) { console.log('websock close'); };
  websock.onerror = function(evt) { console.log(evt); };
  websock.onmessage = function(evt) {
    //console.log(evt);
    // evt.data holds the gateway radio status info in JSON format
    var gwobj = JSON.parse(evt.data);
    if (gwobj && gwobj.msgType) {
      if (gwobj.msgType === "config") {
        var eConfig = document.getElementById('rfm69Config');
        eConfig.innerHTML = '<p>Frequency ' + gwobj.freq + ' MHz' +
          ', Network ID:' + gwobj.netid +
          ', RFM69HCW:' + gwobj.rfm69hcw +
          ', Power Level:' + gwobj.power;
      }
      else if (gwobj.msgType === "status") {
        var eStatus = document.getElementById('rfm69Status');
        rfm69nodes[gwobj.senderId] = gwobj;
        var aTable = '<table>';
        aTable = aTable.concat(
            '<tr>' +
            '<th>Node</th>' +
            '<th>RSSI</th>' +
            '<th>Packets</th>' +
            '<th>Miss</th>' +
            '<th>Dup</th>' +
            '<th>Last</th>' +
            '</tr>');
        for (var i = 0; i <= 255; i++) {
          if (rfm69nodes[i]) {
            aTable = aTable.concat('<tr>' +
                '<td>' + rfm69nodes[i].senderId + '</td>' +
                '<td>' + rfm69nodes[i].rssi + '</td>' +
                '<td>' + rfm69nodes[i].rxMsgCnt  + '</td>' +
                '<td>' + rfm69nodes[i].rxMsgMiss + '</td>' +
                '<td>' + rfm69nodes[i].rxMsgDup  + '</td>' +
                '<td>' + rfm69nodes[i].message + '</td>' +
                '</tr>');
          }
        }
        aTable = aTable.concat('</table>');
        eStatus.innerHTML = aTable;
      }
      else {
      }
    }
  };
}
</script>
</head>
<body onload="javascript:start();">
<h2>RFM69 Gateway</h2>
<div id="rfm69Config"></div>
<div id="rfm69Status">Waiting for node data</div>
<div id="configureGateway">
  <p><a href="/configGW"><button type="button">Configure Gateway</button></a>
</div>
<div id="configureNodes">
  <p><a href="/configGWnode"><button type="button">Configure Nodes</button></a>
</div>
<div id="FirmwareUpdate">
  <p><a href="/updater"><button type="button">Update Gateway Firmware</button></a>
</div>
</body>
</html>
)rawliteral";

static const char PROGMEM CONFIGUREGW_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
  <title>RFM69 Gateway Configuration</title>
  <style>
    "body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }"
  </style>
</head>
<body>
  <h2>RFM69 Gateway Configuration</h2>
  <a href="/configGWrfm69"><button type="button">RFM69/GW</button></a>
  <p>
  <a href="/configGWmqtt"><button type="button">MQTT</button></a>
  <p>
  <a href="/"><button type="button">Home</button></a>
</body>
</html>
)rawliteral";

static const char PROGMEM CONFIGUREGWRFM69_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
  <title>RFM69 Gateway Configuration</title>
  <style>
    "body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }"
  </style>
</head>
<body>
  <h3>RFM69 Gateway Configuration</h3>
  <form method='POST' action='/configGWrfm69' enctype='multipart/form-data'>
    <label>RFM69 Network ID</label>
    <input type='number' name='networkid' value="%d" min="1" max="255" size="3"><br>
    <label>RFM69 Node ID</label>
    <input type='number' name='nodeid' value="%d" min="1" max="255" size="3"><br>
    <label>RFM69 Encryption Key</label>
    <input type='text' name='encryptkey' value="%s" size="16" maxlength="16"><br>
    <label>RFM69 Power Level</label>
    <input type='number' name='powerlevel' value="%d" min="0" max="31"size="2"><br>
    <label>RFM69 Frequency</label>
    <select name="rfmfrequency">
    <option value="31" %s>315 MHz</option>
    <option value="43" %s>433 MHz</option>
    <option value="86" %s>868 MHz</option>
    <option value="91" %s>915 MHz</option>
    </select><br>
    <label for=hcw>RFM69 HCW</label><br>
    <input type='radio' name='rfm69hcw' id="hcw" value="1" %s> True<br>
    <input type='radio' name='rfm69hcw' id="hcw" value="0" %s> False<br>
    <label>RFM69 AP name</label>
    <input type='text' name='rfmapname' value="%s" size="32" maxlength="32"><br>
    <label>DNS name</label>
    <input type='text' name='mdnsname' value="%s" size="32" maxlength="32"><br>
    <label>Updater password:</label><br>
    <label>old user</label>
    <input type='text' name='olduser' value="%s" size="20" maxlength="20"><br>
    <label>old password</label>
    <input type='text' name='oldpassword' value="%s" size="20" maxlength="20"><br>
    <label>new user</label>
    <input type='text' name='newuser' size="20" maxlength="20"><br>
    <label>new password</label>
    <input type='text' name='newpassword' size="20" maxlength="20"><br>
    <p><input type='submit' value='Save changes'>
  </form>
  <p><a href="/configGW"><button type="button">Cancel</button></a><a href="/configGWreset"><button type="button">Factory Reset</button></a>
</body>
</html>
)rawliteral";

static const char PROGMEM CONFIGUREGWMQTT_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
  <title>RFM69 Gateway MQTT Configuration</title>
  <style>
    "body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }"
  </style>
</head>
<body>
  <h3>RFM69 Gateway MQTT Configuration</h3>
  <form method='POST' action='/configGWmqtt' enctype='multipart/form-data'>
    <label>MQTT broker</label>
    <input type='text' name='mqttbroker' value="%s" size="32" maxlength="32"><br>
    <label>MQTT client name</label>
    <input type='text' name='mqttclientname' value="%s" size="32" maxlength="32"><br>
    <label>MQTT client user</label>
    <input type='text' name='mqttclientuser' value="%s" size="32" maxlength="20"><br>
    <label>MQTT client password</label>
    <input type='text' name='mqttclientpassword' value="%s" size="32" maxlength="20"><br>
    <label>note: if you have seen %s and changed MQTT broker then set RFM69 encrypt key again!</label>
    <p><input type='submit' value='Save changes'>
  </form>
  <p><a href="/configGW"><button type="button">Cancel</button></a>
</body>
</html>
)rawliteral";

static const char PROGMEM SELECTNODE[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name = 'viewport' content = 'width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0'>
  <title>RFM69 Node Configuration</title>
  <style>
    'body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }'
  </style>
</head>
<body>
    <h3>RFM69 Node Configuration</h3>
    <p><a href="/"><button type="button">Home</button></a>
        <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
            <label>nodeId &nbsp</label>
            <input type='number' name='nodeid' min='1' max='253' size='3'>
            &nbsp &nbsp &nbsp &nbsp
            request Data from Node:
            <input type='checkbox' name='rAll_0' value='1'>
            &nbsp &nbsp &nbsp &nbsp
            <input type='submit' value='set Node Id to edit'><br>
        </form>
        <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
            <input type='hidden' name='setupNewNode'>
            <p>
            Setup new nodes:
            <P>
            <label>new nodeId &nbsp</label>
            <input type='number' name='w_27' min='1' max='253' size='3'>
            &nbsp &nbsp &nbsp &nbsp
            <input type='submit' value='setup new node'><br>
        </form>
</body>
</html>
)rawliteral";

static const char PROGMEM CONFIGURENODE[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name = 'viewport' content = 'width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0'>
  <title>RFM69 Node Configuration</title>
  <style>
    'body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }'
  </style>
</head>
<body>
    <h3>RFM69 Node Configuration</h3>
    <p><a href="/"><button type="button">Home</button></a>
        <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
            <label>nodeId &nbsp</label>
            <input type='number' name='nodeid' min='1' max='253' size='3'>
            &nbsp &nbsp &nbsp &nbsp
            request Data from Node:
            <input type='checkbox' name='rAll_0' value='1'>
            &nbsp &nbsp &nbsp &nbsp
            <input type='submit' value='set Node Id to edit'><br>
        </form>
        <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
            <input type='hidden' name='led_0' value='blink'>
            <input type='submit' value='test'><br>
        </form>
        <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
            <input type='hidden' name='w_0' value='255'>
            <input type='submit' value='reset Node'><br>
        </form>
    DIO:
    <table>
        <tbody>
            <tr>
                <td> &nbsp &nbsp </td>
                <td> out &nbsp &nbsp </td>
                <td> high &nbsp &nbsp </td>
                <td> intRise &nbsp &nbsp </td>
                <td> intFall &nbsp &nbsp </td>
                <td> wdDefault &nbsp &nbsp </td>
                <td> wdReq &nbsp &nbsp </td>
                <td> readInput &nbsp &nbsp </td>
                <td> count &nbsp &nbsp </td>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_0' value='0'>
                    <td> Pin0 &nbsp </td>
                    <td> <input type='checkbox' name='w_0' value='1'> </td>
                    <td> <input type='checkbox' name='w_0' value='2'> </td>
                    <td> <input type='checkbox' name='w_0' value='4'> </td>
                    <td> <input type='checkbox' name='w_0' value='8'> </td>
                    <td> <input type='checkbox' name='w_0' value='16'> </td>
                    <td> <input type='checkbox' name='w_0' value='32'> </td>
                    <td> <input type='checkbox' name='w_0' value='64'> </td>
                    <td> <input type='checkbox' name='w_0' value='128'> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr> 
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_1' value='0'>
                    <td> Pin1 &nbsp </td>
                    <td> <input type='checkbox' name='w_1' value='1'> </td>
                    <td> <input type='checkbox' name='w_1' value='2'> </td>
                    <td> <input type='checkbox' name='w_1' value='4'> </td>
                    <td> <input type='checkbox' name='w_1' value='8'> </td>
                    <td> <input type='checkbox' name='w_1' value='16'> </td>
                    <td> <input type='checkbox' name='w_1' value='32'> </td>
                    <td> <input type='checkbox' name='w_1' value='64'> </td>
                    <td> <input type='checkbox' name='w_1' value='128'> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr> 
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_2' value='0'>
                    <td> Pin2 &nbsp </td>
                    <td> <input type='checkbox' name='w_2' value='1'> </td>
                    <td> <input type='checkbox' name='w_2' value='2'> </td>
                    <td> <input type='checkbox' name='w_2' value='4'> </td>
                    <td> <input type='checkbox' name='w_2' value='8'> </td>
                    <td> <input type='checkbox' name='w_2' value='16'> </td>
                    <td> <input type='checkbox' name='w_2' value='32'> </td>
                    <td> <input type='checkbox' name='w_2' value='64'> </td>
                    <td> <input type='checkbox' name='w_2' value='128'> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr> 
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_3' value='0'>
                    <td> Pin3 &nbsp </td>
                    <td> <input type='checkbox' name='w_3' value='1'> </td>
                    <td> <input type='checkbox' name='w_3' value='2'> </td>
                    <td> <input type='checkbox' name='w_3' value='4'> </td>
                    <td> <input type='checkbox' name='w_3' value='8'> </td>
                    <td> <input type='checkbox' name='w_3' value='16'> </td>
                    <td> <input type='checkbox' name='w_3' value='32'> </td>
                    <td> <input type='checkbox' name='w_3' value='64'> </td>
                    <td> <input type='checkbox' name='w_3' value='128'> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr> 
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_4' value='0'>
                    <td> Pin4 &nbsp </td>
                    <td> <input type='checkbox' name='w_4' value='1'> </td>
                    <td> <input type='checkbox' name='w_4' value='2'> </td>
                    <td> <input type='checkbox' name='w_4' value='4'> </td>
                    <td> <input type='checkbox' name='w_4' value='8'> </td>
                    <td> <input type='checkbox' name='w_4' value='16'> </td>
                    <td> <input type='checkbox' name='w_4' value='32'> </td>
                    <td> <input type='checkbox' name='w_4' value='64'> </td>
                    <td> <input type='checkbox' name='w_4' value='128'> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr> 
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_5' value='0'>
                    <td> Pin5 &nbsp </td>
                    <td> <input type='checkbox' name='w_5' value='1'> </td>
                    <td> <input type='checkbox' name='w_5' value='2'> </td>
                    <td> <input type='checkbox' name='w_5' value='4'> </td>
                    <td> <input type='checkbox' name='w_5' value='8'> </td>
                    <td> <input type='checkbox' name='w_5' value='16'> </td>
                    <td> <input type='checkbox' name='w_5' value='32'> </td>
                    <td> <input type='checkbox' name='w_5' value='64'> </td>
                    <td> <input type='checkbox' name='w_5' value='128'> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr> 
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_6' value='0'>
                    <td> Pin6 &nbsp </td>
                    <td> <input type='checkbox' name='w_6' value='1'> </td>
                    <td> <input type='checkbox' name='w_6' value='2'> </td>
                    <td> <input type='checkbox' name='w_6' value='4'> </td>
                    <td> <input type='checkbox' name='w_6' value='8'> </td>
                    <td> <input type='checkbox' name='w_6' value='16'> </td>
                    <td> <input type='checkbox' name='w_6' value='32'> </td>
                    <td> <input type='checkbox' name='w_6' value='64'> </td>
                    <td> <input type='checkbox' name='w_6' value='128'> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
        </tbody>
    </table>
Analog:
    <table>
        <tbody>
           <tr>
              <td> &nbsp &nbsp </td>
              <td> plant &nbsp &nbsp </td>
              <td> ldr &nbsp &nbsp </td>
              <td> rain &nbsp &nbsp </td>
              <td> raw &nbsp &nbsp </td>
              <td> Volt &nbsp &nbsp </td>
              <td> *2 &nbsp &nbsp </td>
              <td> *4 &nbsp &nbsp </td>
              </tr>
           <tr>
               <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                  <input type='hidden' name='w_13' value='0'>
                  <td> Pin2 (A5) &nbsp </td>
                  <td> <input type="checkbox" name="w_13" value="1"> </td>
                  <td> <input type="checkbox" name="w_13" value="2"> </td>
                  <td> <input type="checkbox" name="w_13" value="4"> </td>
                  <td> <input type="checkbox" name="w_13" value="8"> </td>                  
                  <td> <input type="checkbox" name="w_13" value="16"> </td>
                  <td> <input type="checkbox" name="w_13" value="64"> </td>
                  <td> <input type="checkbox" name="w_13" value="128"> </td>
                  <td> <input type='submit' value='save'></td>
              </form>
           </tr>
           <tr>
               <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                  <input type='hidden' name='w_12' value='0'>
                  <td> Pin3 (A4)&nbsp </td>
                  <td> <input type="checkbox" name="w_12" value="1"> </td>
                  <td> <input type="checkbox" name="w_12" value="2"> </td>
                  <td> <input type="checkbox" name="w_12" value="4"> </td>
                  <td> <input type="checkbox" name="w_12" value="8"> </td>
                  <td> <input type="checkbox" name="w_12" value="16"> </td>
                  <td> <input type="checkbox" name="w_12" value="64"> </td>
                  <td> <input type="checkbox" name="w_12" value="128"> </td>
                  <td> <input type='submit' value='save'></td>
              </form>
           </tr>
           <tr>
              <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                  <input type='hidden' name='w_10' value='0'>
                  <td> Pin5 (A2)&nbsp </td>
                  <td> <input type="checkbox" name="w_10" value="1"> </td>
                  <td> <input type="checkbox" name="w_10" value="2"> </td>
                  <td> <input type="checkbox" name="w_10" value="4"> </td>
                  <td> <input type="checkbox" name="w_10" value="8"> </td>
                  <td> <input type="checkbox" name="w_10" value="16"> </td>
                  <td> <input type="checkbox" name="w_10" value="64"> </td>
                  <td> <input type="checkbox" name="w_10" value="128"> </td>
                  <td> <input type='submit' value='save'></td>
              </form>
           </tr>
           <tr>
              <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                  <input type='hidden' name='w_11' value='0'>
                  <td> Pin6 (A3)&nbsp </td>
                  <td> <input type="checkbox" name="w_11" value="1"> </td>
                  <td> <input type="checkbox" name="w_11" value="2"> </td>
                  <td> <input type="checkbox" name="w_11" value="4"> </td>
                  <td> <input type="checkbox" name="w_11" value="8"> </td>
                  <td> <input type="checkbox" name="w_11" value="16"> </td>
                  <td> <input type="checkbox" name="w_11" value="64"> </td>
                  <td> <input type="checkbox" name="w_11" value="128"> </td>
                  <td> <input type='submit' value='save'></td>
              </form>
           </tr>
        </tbody>
    </table>
digitalSensors:
    <table>
        <tbody>
          <tr>
              <td> &nbsp &nbsp </td>
              <td> DS18b20 &nbsp &nbsp </td>
              <td> HC05 &nbsp &nbsp </td>
              <td> BME/P280 &nbsp &nbsp </td>
          </tr>
          <tr>
              <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                  <input type='hidden' name='w_15' value='0'>
                  <td> &nbsp &nbsp </td>
                  <td> <input type="checkbox" name="w_15" value="1"> </td>
                  <td> <input type="checkbox" name="w_15" value="2"> </td>
                  <td> <input type="checkbox" name="w_15" value="4"> </td>
                  <td> <input type='submit' value='save'></td>
              </form>
          </tr>
        </tbody>
    </table>
digitalOut:
    <table>
        <tbody>
          <tr>
              <td> &nbsp &nbsp </td>
              <td> free &nbsp &nbsp </td>
              <td> free &nbsp &nbsp </td>
              <td> free &nbsp &nbsp </td>
              <td> ssd1306_64x48 &nbsp &nbsp </td>
              <td> ssd1306_128x64 &nbsp &nbsp </td>
          </tr>
          <tr>
              <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                   <input type='hidden' name='w_16' value='0'>
                   <td> &nbsp &nbsp  </td>
                   <td> <input type="checkbox" name="w_16" value="1"> </td>
                   <td> <input type="checkbox" name="w_16" value="2"> </td>
                   <td> <input type="checkbox" name="w_16" value="4"> </td>
                   <td> <input type="checkbox" name="w_16" value="8"> </td>
                   <td> <input type="checkbox" name="w_16" value="16"> </td>
                  <td> <input type='submit' value='save'></td>
              </form>
          </tr>
        </tbody>
    </table>
nodeControll:
    <table>
        <tbody>
            <tr>
                <td> &nbsp &nbsp </td>
                <td> sensorPower &nbsp &nbsp </td>
                <td> pumpSensorV &nbsp &nbsp </td>
                <td> sensorPowerSleep &nbsp &nbsp </td>
                <td> debugLed &nbsp &nbsp </td>
                <td> displayAlwaysOn &nbsp &nbsp </td>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_20' value='0'>
                    <td> &nbsp &nbsp  </td>
                    <td> <input type="checkbox" name="w_20" value="1"> </td>
                    <td> <input type="checkbox" name="w_20" value="2"> </td>
                    <td> <input type="checkbox" name="w_20" value="4"> </td>
                    <td> <input type="checkbox" name="w_20" value="8"> </td>
                    <td> <input type="checkbox" name="w_20" value="16"> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
        </tbody>
    </table>
 
    <table>
        <tbody>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_21' value='0'>
                    <td> sleepTimeMulti &nbsp &nbsp </td>
                    <td> <input type="text" name="w_21" id="value"> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_22' value='0'>
                    <td> sleepTime &nbsp &nbsp </td>
                    <td> <input type="text" name="w_22" id="value"> </td>
                    <td> <input type='submit' value='save'></td>
                    <td> set sleepTime to 255 and sleepTimeMulti to 255 -> Watchdog disabled (Powersave 0.1uA) </td>
                </form>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_23' value='0'>
                    <td> watchdogTimeout &nbsp &nbsp </td>
                    <td> <input type="text" name="w_23" id="value"> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_24' value='0'>
                    <td> watchdogDelay &nbsp &nbsp </td>
                    <td> <input type="text" name="w_24" id="value"> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_26' value='0'>
                    <td> contrast &nbsp &nbsp </td>
                    <td> <input type="text" name="w_26" id="value"> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_27' value='0'>
                    <td> nodeId &nbsp &nbsp </td>
                    <td> <input type="text" name="w_27" id="value"> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_28' value='0'>
                    <td> networkId &nbsp &nbsp </td>
                    <td> <input type="text" name="w_28" id="value"> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_29' value='0'>
                    <td> gatewayId &nbsp &nbsp </td>
                    <td> <input type="text" name="w_29" id="value"> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='w_30' value='0'>
                    <td> sensorDelay &nbsp &nbsp </td>
                    <td> <input type="text" name="w_30" id="value"> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
            <tr>
                <form method='POST' action='/configGWnode' enctype='multipart/form-data'>
                    <input type='hidden' name='none' value='0'>
                    <td> encryptKey (16 chars!)&nbsp &nbsp </td>
                    <td> <input type="text" name="key_1" id="value" size="16" maxlength="16" minlenght="16"> </td>
                    <td> <input type='submit' value='save'></td>
                </form>
            </tr>
        </tbody>
    </table>
    <p><a href="/"><button type="button">Home</button></a>
</body>
</html>
)rawliteral";

#include <WebSocketsServer.h>     //https://github.com/Links2004/arduinoWebSockets
#include <Hash.h>
ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void webSocketEvent(uint8_t num, int type, uint8_t * payload, size_t length)
{
  Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        // Send the RFM69 radio configuration one time after connection
        webSocket.sendTXT(num, RadioConfig, strlen(RadioConfig));
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\r\n", num, payload);

      // send data to all connected clients
      //webSocket.broadcastTXT(payload, length);
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      //hexdump(payload, length);

      // echo data back to browser
      //webSocket.sendBIN(num, payload, length);
      break;
    default:
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
  }
}

void handleRoot()
{
  Serial.print("Free heap="); Serial.println(ESP.getFreeHeap());
    
  webServer.send_P(200, "text/html", INDEX_HTML);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";
  for (uint8_t i=0; i<webServer.args(); i++){
    message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", message);
}

void handleconfiguregw()
{
  webServer.send_P(200, "text/html", CONFIGUREGW_HTML);
}

// Reset global config back to factory defaults
void handleconfiguregwreset()
{
  pGC->checksum++;
  EEPROM.commit();
  ESP.reset();
  delay(1000);
}

#define SELECTED_FREQ(f)  ((pGC->rfmfrequency==f)?"selected":"")

void handleconfiguregwrfm69()
{
  size_t formFinal_len = strlen_P(CONFIGUREGWRFM69_HTML) + sizeof(*pGC);
  char *formFinal = (char *)malloc(formFinal_len);
  if (formFinal == NULL) {
    Serial.println("formFinal malloc failed");
    return;
  }
  #if showKeysInWeb == true
      snprintf_P(formFinal, formFinal_len, CONFIGUREGWRFM69_HTML,
          pGC->networkid, pGC->nodeid,  pGC->encryptkey, GC_POWER_LEVEL,
          SELECTED_FREQ(RF69_315MHZ), SELECTED_FREQ(RF69_433MHZ),
          SELECTED_FREQ(RF69_868MHZ), SELECTED_FREQ(RF69_915MHZ),
          (GC_IS_RFM69HCW)?"checked":"", (GC_IS_RFM69HCW)?"":"checked",
          pGC->rfmapname, pGC->mdnsname, pGC->updateUser, pGC->updatePassword
          );
  #else
      snprintf_P(formFinal, formFinal_len, CONFIGUREGWRFM69_HTML,
          pGC->networkid, pGC->nodeid, HiddenString, GC_POWER_LEVEL,
          SELECTED_FREQ(RF69_315MHZ), SELECTED_FREQ(RF69_433MHZ),
          SELECTED_FREQ(RF69_868MHZ), SELECTED_FREQ(RF69_915MHZ),
          (GC_IS_RFM69HCW)?"checked":"", (GC_IS_RFM69HCW)?"":"checked",
          pGC->rfmapname, pGC->mdnsname, HiddenString, HiddenString
          );
  #endif
  webServer.send(200, "text/html", formFinal);
  free(formFinal);
}

void handleconfiguregwrfm69Write()
{
  bool commit_required = false;
  bool oldPasswordSeen = false;
  bool oldUserSeen = false;
  String argi, argNamei;

  for (uint8_t i=0; i<webServer.args(); i++) {
    Serial.print(webServer.argName(i));
    Serial.print('=');
    Serial.println(webServer.arg(i));
    argi = webServer.arg(i);
    argNamei = webServer.argName(i);
    if (argNamei == "networkid") {
      uint8_t formnetworkid = argi.toInt();
      if (formnetworkid != pGC->networkid) {
        commit_required = true;
        pGC->networkid = formnetworkid;
      }
    }
    else if (argNamei == "nodeid") {
      uint8_t formnodeid = argi.toInt();
      if (formnodeid != pGC->nodeid) {
        commit_required = true;
        pGC->networkid = formnodeid;
      }
    }
    else if (argNamei == "encryptkey") {
      const char *enckey = argi.c_str();
      if (strcmp(enckey, HiddenString) != 0){
          if (strcmp(enckey, pGC->encryptkey) != 0) {
              commit_required = true;
              strcpy(pGC->encryptkey, enckey);
              Serial.println("encryptkey changed");
          }
      }
    }
    else if (argNamei == "rfmapname") {
      const char *apname = argi.c_str();
      if (strcmp(apname, pGC->rfmapname) != 0) {
        commit_required = true;
        strcpy(pGC->rfmapname, apname);
      }
    }
    else if (argNamei == "powerlevel") {
      uint8_t powlev = argi.toInt();
      if (powlev != GC_POWER_LEVEL) {
        commit_required = true;
        pGC->powerlevel = (GC_IS_RFM69HCW << 7) | powlev;
      }
    }
    else if (argNamei == "rfm69hcw") {
      uint8_t hcw = argi.toInt();
      if (hcw != GC_IS_RFM69HCW) {
        commit_required = true;
        pGC->powerlevel = (hcw << 7) | GC_POWER_LEVEL;
      }
    }
    else if (argNamei == "rfmfrequency") {
      uint8_t freq = argi.toInt();
      if (freq != pGC->rfmfrequency) {
        commit_required = true;
        pGC->rfmfrequency = freq;
      }
    }
    else if (argNamei == "olduser") {
        const char *olduser = argi.c_str();
        if (strcmp(olduser, HiddenString) != 0){
            if (strcmp(olduser, pGC->updateUser) == 0) {
                oldUserSeen = true;
            }
        }
    }
    else if (argNamei == "oldpassword") {
        const char *oldpw = argi.c_str();
        if (strcmp(oldpw, HiddenString) != 0){
            if (strcmp(oldpw, pGC->updatePassword) == 0) {
                oldPasswordSeen = true;
            }
        }
    }
    else if (argNamei == "newuser") {
      const char *newusr = argi.c_str();
        if ((strcmp(newusr, HiddenString) != 0) && oldPasswordSeen && oldUserSeen){
            if (strcmp(newusr, pGC->updatePassword) != 0) {
                commit_required = true;
                strcpy(pGC->updateUser, newusr);
                Serial.println("user changed");
            }
        }
    }
    else if (argNamei == "newpassword") {
      const char *newpw = argi.c_str();
        if ((strcmp(newpw, HiddenString) != 0) && oldPasswordSeen && oldUserSeen){
            if (strcmp(newpw, pGC->updatePassword) != 0) {
                commit_required = true;
                strcpy(pGC->updatePassword, newpw);
                Serial.println("password changed");
            }
        }
    }
    else if (argNamei == "mdnsname") {
      const char *mdns = argi.c_str();
      if (strcmp(mdns, pGC->mdnsname) != 0) {
        commit_required = true;
        strcpy(pGC->mdnsname, mdns);
      }
    }
  }
  handleRoot();
  if (commit_required) {
    pGC->checksum = gc_checksum();
    EEPROM.commit();
    ESP.reset();
    delay(1000);
  }
}

void handleconfiguregwmqtt()
{
  size_t formFinal_len = strlen_P(CONFIGUREGWMQTT_HTML) + sizeof(*pGC);
  char *formFinal = (char *)malloc(formFinal_len);
  if (formFinal == NULL) {}
  #if showKeysInWeb == true
    snprintf_P(formFinal, formFinal_len, CONFIGUREGWMQTT_HTML,
        pGC->mqttbroker, pGC->mqttclientname, pGC->mqttUser, pGC->mqttPassword, HiddenString
        );
  #else
    snprintf_P(formFinal, formFinal_len, CONFIGUREGWMQTT_HTML,
        HiddenString, pGC->mqttclientname,  HiddenString, HiddenString, HiddenString
        );
  #endif
  webServer.send(200, "text/html", formFinal);
  free(formFinal);
}

void handleconfiguregwmqttWrite()
{
  bool commit_required = false;
  String argi, argNamei;

  for (uint8_t i=0; i<webServer.args(); i++) {
    Serial.print(webServer.argName(i));
    Serial.print('=');
    Serial.println(webServer.arg(i));
    argi = webServer.arg(i);
    argNamei = webServer.argName(i);
    if (argNamei == "mqttbroker") {
        const char *broker = argi.c_str();
        if (strcmp(broker, HiddenString) != 0){
            if (strcmp(broker, pGC->mqttbroker) != 0) {
                commit_required = true;
                strcpy(pGC->mqttbroker, broker);
                //Wenn der Key geaendert wird dann loeschen wir auch den encrypt key
                #if showKeysInWeb == false
                    strcpy_P(pGC->encryptkey, ENCRYPTKEY);
                #endif
            }
        }
    }
    else if (argNamei == "mqttclientname") {
      const char *client = argi.c_str();
      if (strcmp(client, pGC->mqttclientname) != 0) {
        commit_required = true;
        strcpy(pGC->mqttclientname, client);
      }
    }
    else if (argNamei == "mqttclientpassword"){
        const char *pw = argi.c_str();
        if (strcmp(pw, HiddenString) != 0){
            if (strcmp(pw, pGC->mqttPassword) != 0){
              commit_required = true;
              strcpy(pGC->mqttPassword, pw);
            }
        }
    }
    else if (argNamei == "mqttclientuser"){
        const char *user = argi.c_str();
        if (strcmp(user, HiddenString) != 0){
            if (strcmp(user, pGC->mqttUser) != 0){
              commit_required = true;
              strcpy(pGC->mqttUser, user);
            }
        }
    }
  }
  
  handleRoot();
  if (commit_required) {
    pGC->checksum = gc_checksum();
    EEPROM.commit();
    ESP.reset();
    delay(1000);
  }
}

void handleconfigurenode(){
  
  for (uint8_t i=0; i<webServer.args(); i++) {
      Serial.print(webServer.argName(i));
      Serial.print('=');
      Serial.println(webServer.arg(i));
  }
  webServer.send(200, "text/html", SELECTNODE);
  
}

void handleconfigurenodeWrite(){
  String argi, argNamei;
  char tempValueChar[21];
  static char tempNodeId[5] = "";
  const char *argnameStr;
  uint8_t tempValue = 0;
  boolean sendData = false;
  boolean sendKey = false;

  for (uint8_t i=0; i<webServer.args(); i++) {
      Serial.print(webServer.argName(i));
      Serial.print('=');
      Serial.println(webServer.arg(i));
      argi = webServer.arg(i);
      argNamei = webServer.argName(i);
      argnameStr = argNamei.c_str();
      const char *argStr = argi.c_str();
      if (strncmp(argnameStr, "w_", 2) == 0) {
          tempValue += atoi(argStr);
          itoa(tempValue, tempValueChar, 10);
          sendData = true;
      }else if (strncmp(argnameStr, "rAll_0", 6) == 0){
          strncpy(tempValueChar, argStr, 5);
          sendData = true;
      }else if (strncmp(argnameStr, "nodeid", 6) == 0) {
          strncpy(tempNodeId, argStr, 5);
      }else if (strncmp(argnameStr, "setupNewNode", 12) == 0) {
          //set tempNodeId to standard nodeid for new nodes
          strncpy(tempNodeId, NEWNODE_NODEID, 5);
          sendKey = true;
      }else if ((strncmp(argnameStr, "key_", 4) == 0) && (strlen(argStr) == 16)) {
          strncpy(tempValueChar, argStr, 16+1);
          sendData = true;
      }else if (strncmp(argnameStr, "led_", 4) == 0) {
          strncpy(tempValueChar, argStr, 6);
          sendData = true;
      }else{

      }
  }
  
  Serial.println("built topic");
  char temptopic[30]="rfmOut/";
  char tempNetworkId[5];
  itoa(pGC->networkid, tempNetworkId, 10);
  strncat(temptopic, tempNetworkId, 5);
  strcat(temptopic, "/");
  strncat(temptopic, tempNodeId, 5);
  strcat(temptopic, "/");
  strncat(temptopic, argnameStr,7);

  Serial.println("built message");
  char jsonMessage[35] = "{\"";
  if (sendData){
      //built json Message      
      strncat(jsonMessage, argnameStr, 9);
      strcat(jsonMessage, "\":\"");
      strncat(jsonMessage, tempValueChar, 20);
      strcat(jsonMessage, "\"");
      //Wenn eine neue Node konfiguriert wurde übernehmen wir die neue Node Id
      if (sendKey && (strncmp(argnameStr, "w_27", 4) == 0)){
          strncpy(tempNodeId, tempValueChar, 5);
          Serial.print("set to new nodeId:");
          Serial.println(tempNodeId);
      }
  }

  if(sendKey){
      Serial.println("try to send encrypt key to node");
      //wir bilden das Topic 
      char tempTopic[25] = "rfmOut/";
      char tempNetworkId[5];
      itoa(NETWORKID,tempNetworkId,10);
      strncat(tempTopic, tempNetworkId, 4);
      strcat(tempTopic, "/") ;    
      strncat(tempTopic, NEWNODE_NODEID, 4);
      strcat(tempTopic, "/1");
      
      //setze das RFM69 auf standard Settings
      radio.initialize(pGC->rfmfrequency, pGC->nodeid, NETWORKID);
      char tempEncryptkey[16+1];
      strcpy_P(tempEncryptkey, ENCRYPTKEY);
      radio.encrypt(tempEncryptkey);
      
      //wir bilden die Nachricht mit den Encryptkey und networkId
      char tempPayload[RF69_MAX_DATA_LEN+1]= "";
      if (sendData){
          strcat(tempPayload, jsonMessage);
          strcat(tempPayload, ", ");
      }else{
          strcat(tempPayload, "{");
      }
      strcat(tempPayload, "\"key_1\":\"");
      strncat(tempPayload, pGC->encryptkey, 16);
      //strcat(tempPayload, "\"}");
      strcat(tempPayload,"\", \"w_28\":\"");
      itoa(pGC->networkid,tempNetworkId,10);
      strncat(tempPayload, tempNetworkId, 16);
      strcat(tempPayload, "\"}");
      //sende die Nachricht
      callback(tempTopic, (byte*)tempPayload, strnlen(tempPayload, RF69_MAX_DATA_LEN));

      //setze das RFM auf den Normal Betrieb
      radio.initialize(pGC->rfmfrequency, pGC->nodeid, pGC->networkid);
      radio.encrypt(pGC->encryptkey);
      Serial.println("RFM is set to normal Mode");
  }else if (sendData){
      if (strncmp(tempNodeId, "", 5) != 0) {
          Serial.println("send message");
          strcat(jsonMessage, "}");
          mqttClient.publish(temptopic, jsonMessage, true);
      }
  }
  
  webServer.send(200, "text/html", CONFIGURENODE);
  
} 

void websock_setup(void) {
  webServer.on("/", handleRoot);
  webServer.on("/configGW", HTTP_GET, handleconfiguregw);
  webServer.on("/configGWrfm69", HTTP_GET, handleconfiguregwrfm69);
  webServer.on("/configGWrfm69", HTTP_POST, handleconfiguregwrfm69Write);
  webServer.on("/configGWmqtt", HTTP_GET, handleconfiguregwmqtt);
  webServer.on("/configGWmqtt", HTTP_POST, handleconfiguregwmqttWrite);
  webServer.on("/configGWreset", HTTP_GET, handleconfiguregwreset);
  webServer.on("/configGWnode", HTTP_GET, handleconfigurenode);
  webServer.on("/configGWnode", HTTP_POST, handleconfigurenodeWrite);
  webServer.onNotFound(handleNotFound);
  webServer.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// vvvvvvvvv ESP8266 Web OTA Updater vvvvvvvvvvv
#include "ESP8266HTTPUpdateServer.h"
ESP8266HTTPUpdateServer httpUpdater;

void ota_setup() {
  httpUpdater.setup(&webServer, "/updater", pGC->updateUser, pGC->updatePassword);//sw
}

// ^^^^^^^^^ ESP8266 Web OTA Updater ^^^^^^^^^^^

// ^^^^^^^^^ ESP8266 web sockets ^^^^^^^^^^^

void updateClients(uint8_t senderId, int32_t rssi, const char *message);

// vvvvvvvvv RFM69 vvvvvvvvvvv
#include <RFM69.h>                //https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__) || defined(__AVR_ATmega88) || defined(__AVR_ATmega8__) || defined(__AVR_ATmega88__)
#define RFM69_CS      10
#define RFM69_IRQ     2
#define RFM69_IRQN    digitalPinToInterrupt(RFM69_IRQ)
#define RFM69_RST     9
#define LED           13  // onboard blinky
#elif defined(__arm__)//Use pin 10 or any pin you want
// Tested on Arduino Zero
#define RFM69_CS      10
#define RFM69_IRQ     5
#define RFM69_IRQN    digitalPinToInterrupt(RFM69_IRQ)
#define RFM69_RST     6
#define LED           13  // onboard blinky
#elif defined(ESP8266)
// ESP8266
#define RFM69_CS      15  // GPIO15/HCS/D8
#define RFM69_IRQ     4   // GPIO04/D2
#define RFM69_IRQN    digitalPinToInterrupt(RFM69_IRQ)
#define RFM69_RST     2   // GPIO02/D4
#define LED           0   // GPIO00/D3, onboard blinky for Adafruit Huzzah
#else
#define RFM69_CS      10
#define RFM69_IRQ     2
#define RFM69_IRQN    digitalPinToInterrupt(RFM69_IRQ)
#define RFM69_RST     9
#define LED           13  // onboard blinky
#endif



void radio_setup(void) {
  int freq;
  static const char PROGMEM JSONtemplate[] =
    R"({"msgType":"config","freq":%d,"rfm69hcw":%d,"netid":%d,"power":%d})";
  char payload[128];

  radio = RFM69(RFM69_CS, RFM69_IRQ, GC_IS_RFM69HCW, RFM69_IRQN);
  // Hard Reset the RFM module
  pinMode(15, SPECIAL);  ///< GPIO15 Init SCK
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, HIGH);
  delay(100);
  digitalWrite(RFM69_RST, LOW);
  delay(100);

  // Initialize radio
  if (radio.initialize(pGC->rfmfrequency, pGC->nodeid, pGC->networkid)){
	  Serial.println(" -> OK");	  //sw
	  }
  else {
	  Serial.println(" -> FAIL!");//sw
	  mqttClient.publish("rfmIn", "RFM Setup Failed"); //sw
	  }
	
  if (GC_IS_RFM69HCW) {
    radio.setHighPower();    // Only for RFM69HCW & HW!
  }
  radio.setPowerLevel(GC_POWER_LEVEL); // power output ranges from 0 (5dBm) to 31 (20dBm)

  if (pGC->encryptkey[0] != '\0') radio.encrypt(pGC->encryptkey);

  pinMode(LED, OUTPUT);

  Serial.print("\nListening at ");
  switch (pGC->rfmfrequency) {
    case RF69_433MHZ:
      freq = 433;
      break;
    case RF69_868MHZ:
      freq = 868;
      break;
    case RF69_915MHZ:
      freq = 915;
      break;
    case RF69_315MHZ:
      freq = 315;
      break;
    default:
      freq = -1;
      break;
  }
  Serial.print(freq); Serial.print(' ');
  Serial.print(pGC->rfmfrequency); Serial.println(" MHz");

  size_t len = snprintf_P(RadioConfig, sizeof(RadioConfig), JSONtemplate,
      freq, GC_IS_RFM69HCW, pGC->networkid, GC_POWER_LEVEL);
  if (len >= sizeof(RadioConfig)) {
    Serial.println("\n\n*** RFM69 config truncated ***\n");
  }
}

void radio_loop(void) {
  //check if something was received (could be an interrupt from the radio)
  if (radio.receiveDone())
  {
    uint8_t senderId;
    int16_t rssi;
    uint8_t data[RF69_MAX_DATA_LEN + 1];
    uint8_t dataLen;

    //save packet because it may be overwritten
    senderId = radio.SENDERID;
    rssi = radio.RSSI;
    dataLen = radio.DATALEN;
    memcpy(data, (void *)radio.DATA, dataLen);
    data[dataLen] = 0; //add 0 terminisation
    //check if sender wanted an ACK
    if (radio.ACKRequested())
    {
	  //digitalWrite(Led_user, 1);
      radio.sendACK();
    }
    radio.receiveDone(); //put radio in RX mode
    digitalWrite(Led_user, 1);
    updateClients(senderId, rssi, (const char *)data);
    digitalWrite(Led_user, 0);
  }else{
    radio.receiveDone(); //put radio in RX mode
  }
}

// ^^^^^^^^^ RFM69 ^^^^^^^^^^^

// vvvvvvvvv MQTT vvvvvvvvvvv
// *** Be sure to modify PubSubClient.h ***
//
// Be sure to increase the MQTT maximum packet size by modifying
// PubSubClient.h. Add the following line to the top of PubSubClient.h.
// Failure to make this modification means no MQTT messages will be
// published.
// #define MQTT_MAX_PACKET_SIZE 256


uint8_t getNodeId(char *topic){
    
    //get the Node Address by reading topic eg. bla/bla/123/bla -> returns int8 123
	char parts[15][20];
	char *p_start, *p_end;
	uint8_t i = 0;
	p_start = topic;
	while(1) {
		p_end = strchr(p_start, '//');
		if (p_end) {
			strncpy(parts[i], p_start, p_end-p_start);
			parts[i][p_end-p_start] = 0;
			i++;
			p_start = p_end + 1;
		}
		else {
			// sopy the last bit - might as well copy 20
			strncpy(parts[i], p_start, 20);
			break;
		}
	}
    
    return atoi(parts[i-1]);
    
}


void callback(char* topic, byte* payload, unsigned int length) {
	

	uint8_t nodeAdress;
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	for (unsigned int i = 0; i < length; i++) {
		Serial.print((char)payload[i]);
	}
	Serial.println();

	nodeAdress = getNodeId(topic);
    
	Serial.print("Nodeadress: ");
	Serial.println(nodeAdress);

    uint8_t varNumber = nodeAdress / 32;
    uint8_t bitNumber = varNumber * 32;
    bitNumber = nodeAdress - bitNumber;
    
	if ((nodeAdress != 0) && (length > 0) && (reachableNode[varNumber] & (1<<bitNumber))){
    Serial.println("Thinking node is reachable");
		if (!radio.sendWithRetry(nodeAdress, payload, length)){
			//Wir konnten nicht senden-> wir warten und probieren es noch einmal
			delay(150);
      Serial.println("Try again");
			if (!radio.sendWithRetry(nodeAdress, payload, length)){
				//
				Serial.println("Error sending Message to Node");
                char temp[90] ="\"err\":\"sending Message from Topic ";
                strncat(temp, topic,25);
                strncat(temp, " to Node ", 10);
                char temp2[5];
                itoa(nodeAdress, temp2, 10);
                strncat(temp, temp2, 4);
                strncat(temp, "\"",3);
                mqttClient.publish("rfmIn", temp);
            }else{
                Serial.println("Msg sended to Node");
                if (strncmp(topic, "rfmBackup", 9) != 0) {
                    //send 0 Byte retained Message to delete retained messages
                    Serial.println("Delete orig Msg");
                    mqttClient.publish(topic, "", true);
                    //send orig message to backup topic only if its no config reg
                    if ((strncmp(PayloadBck, "{\"p_", 4) == 0)||(strncmp(PayloadBck , "{\"d",3) == 0)||(strncmp(PayloadBck , "{\"t",3) == 0)){
                        Serial.println("Store orig Msg");
                        mqttClient.publish(NodeBackup_topic, PayloadBck, true);
                    }
                }
            }
        }else{
            Serial.println("Msg sended to Node");
            if (strncmp(topic, "rfmBackup", 9) != 0) {
                //send 0 Byte retained Message to delete retained messages
                Serial.println("Delete orig Msg");
                mqttClient.publish(topic, "", true);
                //send orig message to backup topic only if its no config reg
                if ((strncmp(PayloadBck, "{\"p_", 4) == 0)||(strncmp(PayloadBck , "{\"d",3) == 0)||(strncmp(PayloadBck , "{\"t",3) == 0)){
                    Serial.println("Store orig Msg");
                    mqttClient.publish(NodeBackup_topic, PayloadBck, true);
                }
            }
        }
    }else if (length > 0){
        Serial.println(" -> not reachable we will not send");   
    }else if (length == 0){
        Serial.println(" -> 0 byte message. We did nothing.");  
    }else{
        Serial.println(" -> We did nothing."); 
    }
}

void mqtt_setup() {
  mqttClient.setServer(pGC->mqttbroker, 1883);
  mqttClient.setCallback(callback);
}

void reconnect() {
  static const char PROGMEM RFMOUT_TOPIC[] = "rfmOut/%d/#"; //Broker -> Node
  char sub_topic[32];

  // Loop until we're reconnected
  //while (!mqttClient.connected()) {//sw
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(pGC->mqttclientname, pGC->mqttUser, pGC->mqttPassword)) {
      Serial.println("connected");
      delay(1000);
      // Once connected, publish an announcement...
      mqttClient.publish("rfmIn", "Gateway connected");
      // ... and resubscribe
      snprintf_P(sub_topic, sizeof(sub_topic), RFMOUT_TOPIC, pGC->networkid);
      mqttClient.subscribe(sub_topic);
      Serial.printf("subscribe topic [%s]\r\n", sub_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(1000);
    }
  //}//sw
}

void mqtt_loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
}

// ^^^^^^^^^ MQTT ^^^^^^^^^^^

struct _nodestats {
  unsigned long recvMessageCount;
  unsigned long recvMessageMissing;
  unsigned long recvMessageDuplicate;
  unsigned long recvMessageSequence;
};

typedef struct _nodestats nodestats_t;
nodestats_t *nodestats[256];  // index by node ID

struct _nodestats *get_nodestats(uint8_t nodeID)
{
  if (nodestats[nodeID] == NULL) {
    nodestats[nodeID] = (nodestats_t *)malloc(sizeof(nodestats_t));
    if (nodestats[nodeID] == NULL) {
      Serial.println("\n\n*** nodestats malloc() failed ***\n");
      return NULL;
    }
    memset(nodestats[nodeID], 0, sizeof(nodestats_t));
  }
  return nodestats[nodeID];
}

void updateClients(uint8_t senderId, int32_t rssi, const char *message)
{
  char nodeRx_topic[32];
  nodestats_t *ns;
  ns = get_nodestats(senderId);
  if (ns == NULL) {
    Serial.println("\n\n*** updatedClients failed ***\n");
    return;
  }
  unsigned long sequenceChange, newMessageSequence;
  static const char PROGMEM JSONtemplate[] =
	R"({"rxMsgCnt":%lu,"rxMsgMiss":%lu,"rxMsgDup":%lu,"rssi":%d})";
    //R"({"msgType":"status","rxMsgCnt":%lu,"rxMsgMiss":%lu,"rxMsgDup":%lu,"senderId":%d,"rssi":%d,"message":"%s"})";
  char payload[192], topic[32], hash[10], pubPayload[50];
  char infoPayload[192], infoTopic[32];

  //Serial.printf("\r\nnode %d msg %s\r\n", senderId, message);
  newMessageSequence = strtoul(message, NULL, 10);
  ns->recvMessageCount++;
  //Serial.printf("nms %lu rmc %lu rms %lu\r\n",
  //    newMessageSequence, ns->recvMessageCount, ns->recvMessageSequence);
  if (ns->recvMessageCount != 1) {
    // newMessageSequence == 0 means the sender just start up.
    // Or the counter wrapped. But the counter is uint32_t so
    // that will take a very long time.
    if (newMessageSequence != 0) {
      if (newMessageSequence == ns->recvMessageSequence) {
        ns->recvMessageDuplicate++;
      }
      else {
        if (newMessageSequence > ns->recvMessageSequence) {
          sequenceChange = newMessageSequence - ns->recvMessageSequence;
        }
        else {
          sequenceChange = 0xFFFFFFFFUL - (ns->recvMessageSequence - newMessageSequence);
        }
        if (sequenceChange > 1) {
          ns->recvMessageMissing += sequenceChange - 1;
        }
      }
    }
  }
  ns->recvMessageSequence = newMessageSequence;
  //Serial.printf("nms %lu rmc %lu rms %lu\r\n",
  //    newMessageSequence, ns->recvMessageCount, ns->recvMessageSequence);

  // Send using JSON format (http://www.json.org/)
  // The JSON will look like this:
  // {
  //   "rxMsgCnt": 123,
  //   "rxMsgMiss": 0,
  //   "rxMsgDup": 0,
  //   "senderId": 12,
  //   "rssi": -30,
  //   "message": "Hello World #1234"
  // }
    size_t len = snprintf_P(infoPayload, sizeof(infoPayload), JSONtemplate,
      ns->recvMessageCount, ns->recvMessageMissing,
      ns->recvMessageDuplicate, rssi);
    len = snprintf(infoTopic, sizeof(infoTopic), "rfmIn/%d/%d/rxInfo", pGC->networkid, senderId); //Node -> Broker //sw      
      
      
    if (len >= sizeof(payload)) {
        Serial.println("\n\n*** RFM69 packet truncated ***\n");
    }
    // send received message to all connected web clients
    webSocket.broadcastTXT(payload, strlen(payload));

	//subscribe the topic of the Node to get retained messages from Broker -> Node (sleeping Nodes) sw
	snprintf(nodeRx_topic, sizeof(nodeRx_topic), "rfmOut/%d/%d/#", pGC->networkid, senderId); //Broker -> Node

    uint8_t nodeAdress = getNodeId(nodeRx_topic);
    uint8_t varNumber = nodeAdress / 32;
    uint8_t bitNumber = varNumber * 32;
    bitNumber = nodeAdress - bitNumber;
    
    if (message[0] == 17){
        //mqttClient.publish("rfmIn", "subscribed Node");
        //remember if node is subscribed
        Serial.println("Command 17 subscribe on:");
        Serial.println(nodeRx_topic);
        reachableNode[varNumber] |= (1<<bitNumber);
        mqttClient.subscribe(nodeRx_topic);
    }else if (message[0] == 18){
        //mqttClient.publish("rfmIn", "unsubscribed Node");
        //remember if node is unsubscribed
        Serial.println("Command 18 unsubscribe on:");
        Serial.println(nodeRx_topic);    
        Serial.println(NodeBackup_topic_temp); 
        reachableNode[varNumber] &= ~(1<<bitNumber);
        mqttClient.unsubscribe(nodeRx_topic);
    }else if (message[0] == 19){
        //subsrcibe node to Backup topic to get old Messages on Node Startup (Node sends 20 on startup, Gateway puts sended messages from nodeRx_topic to NodeBackup_topic_temp)
        Serial.println("Command 19 subscribe on:");
        Serial.println(nodeRx_topic);
        Serial.println(NodeBackup_topic_temp);
        reachableNode[varNumber] |= (1<<bitNumber);
        mqttClient.subscribe(nodeRx_topic);
        mqttClient.subscribe(NodeBackup_topic_temp);
    }else if (message[0] == 20){
        //remember that node is reachable, this kommand is sent by nodes without sleep at startup
        Serial.println("Command 20 unsubscribe on:");
        Serial.println(nodeRx_topic);
        Serial.println(NodeBackup_topic_temp);
        reachableNode[varNumber] |= (1<<bitNumber);
    }else{
        const char *p_start, *p_end;
        uint8_t messageLen = strlen(message);
        p_start = message;
        // Message str input example: "R_12":"125"
        //a message like: "R_12":"125", "g_13":"243" will be publisched on Topic: rfmIn/networkid/nodeid/R_12 Payload: {"R_12":"125"} and on Topic: rfmIn/networkid/nodeid/g_13 Payload: {"g_13":"243"}
        for (uint8_t i =0; i < 10; i++) {
            //mqttClient.publish("debug", "AA");
            //mqttClient.publish("debug", p_start);
            p_end = strchr(p_start, '/"');          //search first " to set pointer
            if (p_end) {		
                //mqttClient.publish("debug", "AB");
                p_start = p_end;
            }else{
                //mqttClient.publish("debug", "AC");
                break;
            }
            //mqttClient.publish("debug", "A");
            p_end = strchr(p_start+1, '/"');          //search second " to set pointer
            if (p_end) {							//copy R_12
                strncpy(hash, p_start+1, p_end-p_start-1);
                hash[p_end-p_start-1] = 0;
            }
            //mqttClient.publish("debug", "B");
            p_end = strchr(p_start, ',');          //search , to set pointer
            if (p_end) {							//copy "R_12":"125"
                //mqttClient.publish("debug", "C");
                strcpy(pubPayload, "{");
                strncat(pubPayload, p_start, p_end-p_start);
                strncat(pubPayload, "}", 1);
                pubPayload[p_end-p_start+2] = 0;
                p_start = p_end;
            }else{
                //mqttClient.publish("debug", "D");
                strcpy(pubPayload, "{");
                strncat(pubPayload, p_start, sizeof(pubPayload) - 2);
                strncat(pubPayload, "}", 1);
                pubPayload[p_end-p_start+2] = 0;
                i = 20;
                p_start = p_end - 1;
            }

            //mqttClient.publish("debug", "E");
            // mqtt publish the same message
            len = snprintf(topic, sizeof(topic), "rfmIn/%d/%d/%s", pGC->networkid, senderId, hash); //Node -> Broker //sw
            if (len >= sizeof(topic)) {
                Serial.println("\n\n*** MQTT topic truncated ***\n");
            }
            if ((strlen(payload)+1+strlen(topic)+1) > MQTT_MAX_PACKET_SIZE) {
                Serial.println("\n\n*** MQTT message too long! ***\n");
            }                    
            if (!mqttClient.publish(topic, pubPayload, true)) {
                Serial.println("\n\n*** mqtt publish failed ***\n");
            }       
        }
        //Serial.printf("topic [%s] message [%s]\r\n", topic, payload);
        //mqttClient.publish("debug", "F");
        if (!mqttClient.publish(infoTopic, infoPayload, true)) {
            Serial.println("\n\n*** mqtt publish rxInfo failed ***\n");
        }        
    }
}

void setup() {
	Serial.begin(SERIAL_BAUD);
	Serial.println("\nRFM69 WiFi Gateway");

	Serial.println(ESP.getResetReason());

	// Adafruit Huzzah has an LED on GPIO0 with negative logic.
	// Turn if off.
	pinMode(0, OUTPUT);
	digitalWrite(0, HIGH);
  
	pinMode(Led_user, OUTPUT);
	digitalWrite(Led_user, 0);
  
  
	/*
	#define testpin 16
	pinMode(testpin, OUTPUT);
	digitalWrite(testpin, 1);
	delay(100);
	digitalWrite(testpin, 0);
	delay(100);
	digitalWrite(testpin, 1);
	delay(100);
	digitalWrite(testpin, 0);
	*/
  
	Serial.println("eeprom_setup"); //sw
	eeprom_setup();
	Serial.println("wifi_setup");//sw
	wifi_setup();
	Serial.println("mdns_setup");//sw
	mdns_setup();
	Serial.println("mqtt_setup");//sw
	mqtt_setup();
	Serial.println("ota_setup");//sw
	ota_setup();
	Serial.println("websock_setup");//sw
	websock_setup();
	Serial.print("radio_setup");//sw
	radio_setup();
	digitalWrite(Led_user, 1);//sw
	delay(500);//sw
	digitalWrite(Led_user, 0);//sw
	
    reachableNode[0] = 0xFFFFFFFF;
    reachableNode[1] = 0xFFFFFFFF;
    reachableNode[2] = 0xFFFFFFFF;
    reachableNode[3] = 0xFFFFFFFF;
    reachableNode[4] = 0xFFFFFFFF;
    reachableNode[5] = 0xFFFFFFFF;
    reachableNode[6] = 0xFFFFFFFF;
    reachableNode[7] = 0xFFFFFFFF;
    
	//GPF12 = 48;
	//GPF13 = 48;
	//GPF14 = 48;
	//Serial.print("GPF12: ");
	//Serial.print(GPF12);
	//Serial.print(" GPF13: ");
	//Serial.print(GPF13);
	//Serial.print(" GPF14: ");
	//Serial.print(GPF14);
	//Serial.print(" GPF15: ");
	//Serial.print(GPF15);
	//Serial.println("");
	Serial.println("setup_finished");//sw
}

void loop() {
  //Serial.println("radio_loop");
  radio_loop();
  //Serial.println("mqtt_loop");
  mqtt_loop();
  //Serial.println("webSocket");
  webSocket.loop();
  //Serial.println("webServer");
  webServer.handleClient();

}

