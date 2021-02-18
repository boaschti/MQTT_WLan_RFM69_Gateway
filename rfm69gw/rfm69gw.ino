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







#define userpage_existing
#define userpage2_existing

//#define AllowAcessPoint         // opens a acessPoint if saved Wlan is not reachable
//#define WiFiNotRequired         // if there is no connection -> return from setup_server() (and run main loop) or restart esp until connection
//#define showKeysInWeb true      // shows keys in WEB page (for debug! not recommendet!!)

//#define usersubscribe_existing  //if a UserSubscribe() exists. Else the Server will connect automatically to set Topic
#define MQTTBrokerChanged_existing  // if a MQTTBrokerChanged() exists you can do sth if Broker is changed via web

#define DeviceName "RF_Gateway"
#define UserPageName "RF Module"
#define MqttBrokerChangedMsg "Set RFM69 encrypt key again!"

#include "C:\Users\sebas\Documents\ESP8266-Server\ESPServer.h"

#ifdef usersubscribe_existing
void UserSubscribe(){
}
#endif // usersubscribe_existing


#ifdef userpage_existing
static const char PROGMEM USERPAGE_HTML[] = R"rawliteral(
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

    <p><a href="/"><button type="button">Home</button></a>
</body>
</html>
)rawliteral";



#endif // userpage_existing


#include "C:\Users\sebas\Documents\MQTT_Node_RFM69\ProgableNode\RFM69.cpp"
#include "C:\Users\sebas\Documents\MQTT_Node_RFM69\ProgableNode\RFM69.h"
#include "C:\Users\sebas\Documents\MQTT_Node_RFM69\ProgableNode\RFM69registers.h"

//#include <RFM69.h>                //https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>

uint32_t reachableNode[8];

char RadioConfig[128];

// Default values
const char PROGMEM ENCRYPTKEY[] = "sampleEncryptKey";

//Passwort und Benutzer zum Softwareupdate ueber Browser

#define NEWNODE_NODEID    "254"
#define NETWORKID		      1  //the same on all nodes that talk to each other
#define NODEID			      1

//Match frequency to the hardware version of the radio
#define FREQUENCY		RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY      RF69_915MHZ
#define IS_RFM69HCW		false // set to 'true' if you are using an RFM69HCW module
#define POWER_LEVEL		31

#define Led_user		0

RFM69 radio;

#define GC_POWER_LEVEL    (pUserData->powerlevel & 0x1F)
#define GC_IS_RFM69HCW  ((pUserData->powerlevel & 0x80) != 0)



typedef struct {
  char        encryptkey[16+1];
  uint8_t     networkid;
  uint8_t     nodeid;
  uint8_t     powerlevel; // bits 0..4 power level, bit 7 RFM69HCW 1=true
  uint8_t     rfmfrequency;
} userEEProm_mt;
userEEProm_mt userEEProm;

userEEProm_mt * pUserData;


void UserEEPromSetup(void* eepromPtr) {
      userEEProm_mt* pUserData = (userEEProm_mt*)eepromPtr;
      Serial.println("ENCRYPTKEY");
      strcpy_P(pUserData->encryptkey, ENCRYPTKEY);
      Serial.println("NETWORKID");
      pUserData->networkid = NETWORKID;
      Serial.println("NODEID");
      pUserData->nodeid = NODEID;
      Serial.println("powerlevel");
      pUserData->powerlevel = ((IS_RFM69HCW)?0x80:0x00) | POWER_LEVEL;
      Serial.println("rfmfrequency");
      pUserData->rfmfrequency = FREQUENCY;
};

uint32_t UserEEPromChecksum(uint32_t checksum, void* eepromPtr) {
        uint8_t * pUserData = (uint8_t *)eepromPtr;

        for (uint32_t i = 0; i < sizeof(userEEProm_mt); i++)
        {
            checksum += *pUserData++;
        }
    
        return checksum;
};


char GatewayConsole_topic[20];

#ifdef MQTTBrokerChanged_existing
void MQTTBrokerChanged(){
    //Wenn der Key geaendert wird dann loeschen wir auch den encrypt key
    #if showKeysInWeb == false
        strcpy_P(pUserData->encryptkey, ENCRYPTKEY);
        SaveEEpromData();
    #endif    
}
#endif // MQTTBrokerChanged_existing

void myPrintln(char *msg){
  
    mqttpublish(GatewayConsole_topic, msg);
    Serial.println(msg);
    
}

void myPrint(char *msg){

    mqttpublish(GatewayConsole_topic, msg);
    Serial.print(msg);
    
} 

/*
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
            '<th>Message</th>' +
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

*/

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
  <form method='POST' action='/configUserPage' enctype='multipart/form-data'>
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
    <p><input type='submit' value='Save changes'>
  </form>
    <h3>RFM69 Node Configuration</h3>
        <form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
            <label>nodeId &nbsp</label>
            <input type='number' name='nodeid' min='1' max='253' size='3'>
            &nbsp &nbsp &nbsp &nbsp
            request Data from Node:
            <input type='checkbox' name='rAll_0' value='1'>
            &nbsp &nbsp &nbsp &nbsp
            <input type='submit' value='set Node Id to edit'><br>
        </form>
        <form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
            <input type='hidden' name='setupNewNode'>
            <p>
            Setup new nodes:
            <P>
            <label>new nodeId &nbsp</label>
            <input type='number' name='w_27' min='1' max='253' size='3'>
            &nbsp &nbsp &nbsp &nbsp
            <input type='submit' value='setup new node'><br>
        </form>  
    <p><a href="/"><button type="button">Home</button></a>
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
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<label>nodeId &nbsp</label>
<input type='number' name='nodeid' min='1' max='253' size='3'>
&nbsp &nbsp &nbsp &nbsp
request Data from Node:
<input type='checkbox' name='rAll_0' value='1'>
&nbsp &nbsp &nbsp &nbsp
<input type='submit' value='set Node Id to edit'><br>
</form>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='led_2' value='blink'>
<input type='submit' value='test'><br>
</form>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_0' value='255'>
<input type='submit' value='reset Node'><br>
</form>
DIO: 
miniNode, maxiNode  -ArduinoSPS Board-
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
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_0' value='0'>
<td> Pin0 -A1- &nbsp </td>
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
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_1' value='0'>
<td> Pin1 -A2- &nbsp </td>
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
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_2' value='0'>
<td> Pin2 -A3- &nbsp </td>
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
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_3' value='0'>
<td> Pin3 -A4- &nbsp </td>
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
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_4' value='0'>
<td> Pin4 -A5- &nbsp </td>
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
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_5' value='0'>
<td> Pin5 -D6- &nbsp </td>
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
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_6' value='0'>
<td> Pin6 -D7- &nbsp </td>
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
<tr>
<td> ArduinoSPS Board: &nbsp </td>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_7' value='0'>
<td> D8 &nbsp </td>
<td> <input type='checkbox' name='w_7' value='1'> </td>
<td> <input type='checkbox' name='w_7' value='2'> </td>
<td> <input type='checkbox' name='w_7' value='4'> </td>
<td> <input type='checkbox' name='w_7' value='8'> </td>
<td> <input type='checkbox' name='w_7' value='16'> </td>
<td> <input type='checkbox' name='w_7' value='32'> </td>
<td> <input type='checkbox' name='w_7' value='64'> </td>
<td> <input type='checkbox' name='w_7' value='128'> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_8' value='0'>
<td> D9 &nbsp </td>
<td> <input type='checkbox' name='w_8' value='1'> </td>
<td> <input type='checkbox' name='w_8' value='2'> </td>
<td> <input type='checkbox' name='w_8' value='4'> </td>
<td> <input type='checkbox' name='w_8' value='8'> </td>
<td> <input type='checkbox' name='w_8' value='16'> </td>
<td> <input type='checkbox' name='w_8' value='32'> </td>
<td> <input type='checkbox' name='w_8' value='64'> </td>
<td> <input type='checkbox' name='w_8' value='128'> </td>
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
<td> fuelHigh &nbsp &nbsp </td>
<td> raw &nbsp &nbsp </td>
<td> Volt &nbsp &nbsp </td>
<td> *2 &nbsp &nbsp </td>
<td> *4 &nbsp &nbsp </td>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_13' value='0'>
<td> Pin2 (A5) &nbsp </td>
<td> <input type="checkbox" name="w_13" value="1"> </td>
<td> <input type="checkbox" name="w_13" value="2"> </td>
<td> <input type="checkbox" name="w_13" value="4"> </td>
<td> <input type="checkbox" name="w_13" value="8"> </td>                  
<td> <input type="checkbox" name="w_13" value="16"> </td>
<td> <input type="checkbox" name="w_13" value="32"> </td>
<td> <input type="checkbox" name="w_13" value="64"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_12' value='0'>
<td> Pin3 (A4)&nbsp </td>
<td> <input type="checkbox" name="w_12" value="1"> </td>
<td> <input type="checkbox" name="w_12" value="2"> </td>
<td> <input type="checkbox" name="w_12" value="4"> </td>
<td> <input type="checkbox" name="w_12" value="8"> </td>
<td> <input type="checkbox" name="w_12" value="16"> </td>
<td> <input type="checkbox" name="w_12" value="32"> </td>
<td> <input type="checkbox" name="w_12" value="64"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_10' value='0'>
<td> Pin5 (A2)&nbsp </td>
<td> <input type="checkbox" name="w_10" value="1"> </td>
<td> <input type="checkbox" name="w_10" value="2"> </td>
<td> <input type="checkbox" name="w_10" value="4"> </td>
<td> <input type="checkbox" name="w_10" value="8"> </td>
<td> <input type="checkbox" name="w_10" value="16"> </td>
<td> <input type="checkbox" name="w_10" value="32"> </td>
<td> <input type="checkbox" name="w_10" value="64"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_11' value='0'>
<td> Pin6 (A3)&nbsp </td>
<td> <input type="checkbox" name="w_11" value="1"> </td>
<td> <input type="checkbox" name="w_11" value="2"> </td>
<td> <input type="checkbox" name="w_11" value="4"> </td>
<td> <input type="checkbox" name="w_11" value="8"> </td>
<td> <input type="checkbox" name="w_11" value="16"> </td>
<td> <input type="checkbox" name="w_11" value="32"> </td>
<td> <input type="checkbox" name="w_11" value="64"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<td> ArduinoSPS Board: &nbsp </td>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_9' value='0'>
<td> A1 &nbsp </td>
<td> <input type='checkbox' name='w_9' value='1'> </td>
<td> <input type='checkbox' name='w_9' value='2'> </td>
<td> <input type='checkbox' name='w_9' value='4'> </td>
<td> <input type='checkbox' name='w_9' value='8'> </td>
<td> <input type='checkbox' name='w_9' value='16'> </td>
<td> <input type='checkbox' name='w_9' value='32'> </td>
<td> <input type='checkbox' name='w_9' value='64'> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_14' value='0'>
<td> A6 &nbsp </td>
<td> <input type='checkbox' name='w_14' value='1'> </td>
<td> <input type='checkbox' name='w_14' value='2'> </td>
<td> <input type='checkbox' name='w_14' value='4'> </td>
<td> <input type='checkbox' name='w_14' value='8'> </td>
<td> <input type='checkbox' name='w_14' value='16'> </td>
<td> <input type='checkbox' name='w_14' value='32'> </td>
<td> <input type='checkbox' name='w_14' value='64'> </td>
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
<td> debounceLong &nbsp &nbsp </td>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_15' value='0'>
<td> &nbsp &nbsp </td>
<td> <input type="checkbox" name="w_15" value="1"> </td>
<td> <input type="checkbox" name="w_15" value="2"> </td>
<td> <input type="checkbox" name="w_15" value="4"> </td>
<td> <input type="checkbox" name="w_15" value="8"> </td>
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
<td> lockThermostate &nbsp &nbsp </td>
<td> free &nbsp &nbsp </td>
<td> free &nbsp &nbsp </td>
<td> ssd1306_64x48 &nbsp &nbsp </td>
<td> ssd1306_128x64 &nbsp &nbsp </td>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
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
chipSetup:
<table>
<tbody>
<tr>
<td> &nbsp &nbsp </td>
<td> extCrystal &nbsp &nbsp </td>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_19' value='0'>
<td> &nbsp &nbsp  </td>
<td> <input type="checkbox" name="w_19" value="1"> </td>
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
<td> delOldData &nbsp &nbsp </td>
<td> sendAgain &nbsp &nbsp </td>
<td> DisplayLongOn &nbsp &nbsp </td>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_20' value='0'>
<td> &nbsp &nbsp  </td>
<td> <input type="checkbox" name="w_20" value="1"> </td>
<td> <input type="checkbox" name="w_20" value="2"> </td>
<td> <input type="checkbox" name="w_20" value="4"> </td>
<td> <input type="checkbox" name="w_20" value="8"> </td>
<td> <input type="checkbox" name="w_20" value="16"> </td>
<td> <input type="checkbox" name="w_20" value="32"> </td>
<td> <input type="checkbox" name="w_20" value="64"> </td>
<td> <input type="checkbox" name="w_20" value="128"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
</tbody>
</table>
<table>
<tbody>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_21' value='0'>
<td> sleepTimeMulti &nbsp &nbsp </td>
<td> <input type="text" name="w_21" id="value"> </td>
<td> <input type='submit' value='save'></td>
<td> sleepTime in sec =  sleepTimeMulti * sleepTime * 8s </td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_22' value='0'>
<td> sleepTime 8s &nbsp &nbsp </td>
<td> <input type="text" name="w_22" id="value"> </td>
<td> <input type='submit' value='save'></td>
<td> set sleepTime to 255 and sleepTimeMulti to 255 -> (Powersave 0.1uA) </td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_23' value='0'>
<td> watchdogTimeout 5s &nbsp &nbsp </td>
<td> <input type="text" name="w_23" id="value"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_24' value='0'>
<td> watchdogDelay 5s &nbsp &nbsp </td>
<td> <input type="text" name="w_24" id="value"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_26' value='0'>
<td> contrast &nbsp &nbsp </td>
<td> <input type="text" name="w_26" id="value"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_27' value='0'>
<td> nodeId &nbsp &nbsp </td>
<td> <input type="text" name="w_27" id="value"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_28' value='0'>
<td> networkId &nbsp &nbsp </td>
<td> <input type="text" name="w_28" id="value"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_29' value='0'>
<td> gatewayId &nbsp &nbsp </td>
<td> <input type="text" name="w_29" id="value"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
<input type='hidden' name='w_30' value='0'>
<td> sensorDelay 10s &nbsp &nbsp </td>
<td> <input type="text" name="w_30" id="value"> </td>
<td> <input type='submit' value='save'></td>
</form>
</tr>
<tr>
<form method='POST' action='/configUserPage2' enctype='multipart/form-data'>
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


#define SELECTED_FREQ(f)  ((pUserData->rfmfrequency==f)?"selected":"")

void handleconfigureUser()
{
  size_t formFinal_len = strlen_P(CONFIGUREGWRFM69_HTML) + sizeof(*pUserData);
  char *formFinal = (char *)malloc(formFinal_len);
  if (formFinal == NULL) {
    Serial.println("formFinal malloc failed");
    return;
  }
  #if showKeysInWeb == true
      snprintf_P(formFinal, formFinal_len, CONFIGUREGWRFM69_HTML,
          pUserData->networkid, pUserData->nodeid,  pUserData->encryptkey, GC_POWER_LEVEL,
          SELECTED_FREQ(RF69_315MHZ), SELECTED_FREQ(RF69_433MHZ),
          SELECTED_FREQ(RF69_868MHZ), SELECTED_FREQ(RF69_915MHZ),
          (GC_IS_RFM69HCW)?"checked":"", (GC_IS_RFM69HCW)?"":"checked"
          );
  #else
      snprintf_P(formFinal, formFinal_len, CONFIGUREGWRFM69_HTML,
          pUserData->networkid, pUserData->nodeid, HiddenString, GC_POWER_LEVEL,
          SELECTED_FREQ(RF69_315MHZ), SELECTED_FREQ(RF69_433MHZ),
          SELECTED_FREQ(RF69_868MHZ), SELECTED_FREQ(RF69_915MHZ),
          (GC_IS_RFM69HCW)?"checked":"", (GC_IS_RFM69HCW)?"":"checked"
          );
  #endif
  
  webServer.send(200, "text/html", formFinal);
  free(formFinal);  
}

void handleconfigureUserWrite()
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
      if (formnetworkid != pUserData->networkid) {
        commit_required = true;
        Serial.println("NetworkId seen");
        pUserData->networkid = formnetworkid;
      }
    }
    else if (argNamei == "nodeid") {
      uint8_t formnodeid = argi.toInt();
      if (formnodeid != pUserData->nodeid) {
        commit_required = true;
        pUserData->nodeid = formnodeid;
      }
    }
    else if (argNamei == "encryptkey") {
      const char *enckey = argi.c_str();
      if (strcmp(enckey, HiddenString) != 0){
          if (strcmp(enckey, pUserData->encryptkey) != 0) {
              commit_required = true;
              strcpy(pUserData->encryptkey, enckey);
              Serial.println("encryptkey changed");
          }
      }
    }
    else if (argNamei == "powerlevel") {
      uint8_t powlev = argi.toInt();
      if (powlev != GC_POWER_LEVEL) {
        commit_required = true;
        pUserData->powerlevel = (GC_IS_RFM69HCW << 7) | powlev;
      }
    }
    else if (argNamei == "rfm69hcw") {
      uint8_t hcw = argi.toInt();
      if (hcw != GC_IS_RFM69HCW) {
        commit_required = true;
        pUserData->powerlevel = (hcw << 7) | GC_POWER_LEVEL;
      }
    }
    else if (argNamei == "rfmfrequency") {
      uint8_t freq = argi.toInt();
      if (freq != pUserData->rfmfrequency) {
        commit_required = true;
        pUserData->rfmfrequency = freq;
      }
    }
  }

  if (commit_required) {
    handleRoot();
    Serial.println("save to EEProm");
    SaveEEpromData();
    Serial.println("Reset ESP");
    ESP.reset();
    delay(1000);
  }else{
    webServer.send(200, "text/html", CONFIGURENODE);
  }
}


void handleconfigureUser2(){
}


void handleconfigureUserWrite2(){
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
  itoa(pUserData->networkid, tempNetworkId, 10);
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
      radio.initialize(pUserData->rfmfrequency, pUserData->nodeid, NETWORKID);
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
      strncat(tempPayload, pUserData->encryptkey, 16);
      //strcat(tempPayload, "\"}");
      strcat(tempPayload,"\", \"w_28\":\"");
      itoa(pUserData->networkid,tempNetworkId,10);
      strncat(tempPayload, tempNetworkId, 16);
      strcat(tempPayload, "\"}");
      //sende die Nachricht
      callback(tempTopic, (byte*)tempPayload, strnlen(tempPayload, RF69_MAX_DATA_LEN));

      //setze das RFM auf den Normal Betrieb
      radio.initialize(pUserData->rfmfrequency, pUserData->nodeid, pUserData->networkid);
      radio.encrypt(pUserData->encryptkey);
      Serial.println("RFM is set to normal Mode");
  }else if (sendData){
      if (strncmp(tempNodeId, "", 5) != 0) {
          Serial.println("send message");
          strcat(jsonMessage, "}");
          mqttpublish(temptopic, jsonMessage, true);
      }
  }
  
  webServer.send(200, "text/html", CONFIGURENODE);
  
} 

    
// ^^^^^^^^^ ESP8266 web sockets ^^^^^^^^^^^

void updateClients(uint8_t senderId, int32_t rssi, const char *message);

// vvvvvvvvv RFM69 vvvvvvvvvvv
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
  
  Serial.print("Class and Pins...");
  radio = RFM69(RFM69_CS, RFM69_IRQ, GC_IS_RFM69HCW, RFM69_IRQN);
  // Hard Reset the RFM module
  pinMode(15, SPECIAL);  ///< GPIO15 Init SCK
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, HIGH);
  delay(100);
  digitalWrite(RFM69_RST, LOW);
  delay(100);
  
  Serial.print("Init...");
  // Initialize radio
  if (radio.initialize(pUserData->rfmfrequency, pUserData->nodeid, pUserData->networkid)){
	  Serial.println(" -> OK");	  
	  }
  else {
	  Serial.println(" -> FAIL!");
	  mqttpublish("rfmIn", "RFM Setup Failed"); 
	  }
	
  if (GC_IS_RFM69HCW) {
    radio.setHighPower();    // Only for RFM69HCW & HW!
  }
  radio.setPowerLevel(GC_POWER_LEVEL); // power output ranges from 0 (5dBm) to 31 (20dBm)

  if (pUserData->encryptkey[0] != '\0') radio.encrypt(pUserData->encryptkey);

  pinMode(LED, OUTPUT);

  Serial.print("\nListening at ");
  switch (pUserData->rfmfrequency) {
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
  Serial.print(pUserData->rfmfrequency); Serial.println(" MHz");

  size_t len = snprintf_P(RadioConfig, sizeof(RadioConfig), JSONtemplate,
      freq, GC_IS_RFM69HCW, pUserData->networkid, GC_POWER_LEVEL);
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



unsigned long getMessageId(char *message){
   
    // Message str input: {"R_12":"125", "R13":123}
    //Diese folgende Schleife macht aus dem String 3 oder eine vielzahl von 3 NULL terminierte Strings
    //o.g. message ergibt parts[0] == "R_12" , parts[1] == "/"125/"", parts[2] == "R13", parts[3] == "/"123/"" i == 3
    #define partlengh 12
    #define maxParts 2
    char parts[maxParts][partlengh];
    char *p_start, *p_end;
    uint8_t i = 0;
    boolean integervalue;
    p_start = message;
    while(1) {
        p_end = strchr(p_start, '/"');
        if (p_end) {                                   //search first " to set pointer
            p_start = p_end;
            Serial.println("Name found");
        }
        p_end = strchr(p_start + 1, '/"');                 //search second " to set pointer
        if ((p_end != p_start)&& p_end && p_start) {        //be shure that Name is found  
            if ((i + 2) > maxParts){
                break;
            }
            Serial.print("copy Name ");
            if ((p_end-p_start-1) < partlengh){ 
                strncpy(parts[i], p_start+1, p_end-p_start-1);  //copy "R_12"
                Serial.print(parts[i]);
            }else{
                Serial.println(" Error");
                break;
            }
            Serial.println(" Ok");
            parts[i][p_end-p_start-1] = 0;
            i++;
            p_start = p_end + 1;
            Serial.print("search :");
            p_end = strchr(p_start, ':' );
            if (p_end) {                               //copy "/"125/""
                Serial.println(" found");
                p_start = p_end + 1;
                Serial.print("search , ");
                p_end = strchr(p_start, ',');
                if (!p_end){
                    Serial.print("not found search } ");
                    p_end = strchr(p_start, '/}'); //if no " is in String then search }
                }
                if (p_end) {                           //prepare to search next "
                    Serial.println("found");
                    Serial.print("Copy value ");
                    if ((p_end-p_start)< partlengh){
                        strncpy(parts[i], p_start, p_end-p_start);
                    }else{
                        Serial.println("Error");
                        break;
                    }                    
                    parts[i][p_end-p_start] = 0;
                    Serial.println("Ok");
                    i++;
                    p_start = p_end + 1;
                    p_end = strchr(p_start, '/"');     //search next " to set pointer
                    if (!p_end){
                        Serial.println("No Name found");
                        p_end = strchr(p_start, '/}'); //if no " is in String then search }
                    }
                    if (p_end) {                   
                        p_start = p_end;
                    }
                }else{
                    Serial.println("No , and } found");
                    strncpy(parts[i], p_start, partlengh);
                    i++;
                    p_start = p_end;
                    break;
                }
            }
        }
        else {
            break;
        }
    }
    
    
    uint8_t listLen = 0;
    if (i){
        listLen = i - 1;
    }
    
    unsigned long messageId = 0x00000000UL;
    
    if (listLen){
        for (i = 0; i <= listLen; i++){
            Serial.print("part ");
            Serial.print(i);
            Serial.print("  ");
            Serial.println(parts[i]);
        }
        
        Serial.print("listLen ");
        Serial.println(listLen+1);
    
    
    
        for (uint8_t i=0; i<=listLen; i++){
            if (strcmp(parts[i] , "id") == 0){
                messageId = atoi(parts[i+1]);
                Serial.print("id in Msg found: ");
                Serial.println(messageId);
            }
        }
    }
    
    Serial.println("return now");
    
    //messageId = 0x00000000UL;
    return messageId;
}



void callback(char* topic, byte* payload, unsigned int length) {
    
    char NodeBackup_topic[32];
    char PayloadBck[MQTT_MAX_PACKET_SIZE+1];
    uint8_t nodeAdress;

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
        PayloadBck[i] = payload[i];
    }
    Serial.println();
    PayloadBck[length] = 0;
 
    nodeAdress = getNodeId(topic);
    Serial.print("Nodeadress: ");
    Serial.print(nodeAdress);
    
    // We search first hash to built backuptopic
    if (length > 0){
        char hash[10] = "0";
        const char *p_start, *p_end;
        //byte *p_start, *p_end;
        p_start = (char*)payload;
        // Message str input example: "R_12":"125"
        //a message like: "R_12":"125", "g_13":"243" will be publisched on Topic: rfmIn/networkid/nodeid/R_12 Payload: {"R_12":"125"} and on Topic: rfmIn/networkid/nodeid/g_13 Payload: {"g_13":"243"}
        for (uint8_t i =0; i < 4; i++) {
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
            if (p_end) {                                                                              //copy R_12
                strncpy(hash, p_start+1, p_end-p_start-1);
                hash[p_end-p_start-1] = 0;
                break;
            }
        }
        //subscribe the topic of the Node to get retained messages from Broker -> Node (sleeping Nodes) sw
        snprintf(NodeBackup_topic, sizeof(NodeBackup_topic), "rfmBackup/%d/%d/%s", pUserData->networkid, nodeAdress, hash); //Broker -> Node
    }
 
            
    uint8_t varNumber = nodeAdress / 32;
    uint8_t bitNumber = varNumber * 32;
    bitNumber = nodeAdress - bitNumber;
   
    if ((nodeAdress != 0) && (length > 0) && (reachableNode[varNumber] & (1<<bitNumber))){
        Serial.println(" -> reachable");
        Serial.flush();
        if (!radio.sendWithRetry(nodeAdress, payload, length)){
            //Wir konnten nicht senden-> wir warten und probieren es noch einmal
            /*delay(150);
            Serial.println("Try again");
            if (!radio.sendWithRetry(nodeAdress, payload, length)){*/
                Serial.println("Error sending Message to Node");
                char temp[90] ="\"err\":\"sending Message from Topic ";
                strncat(temp, topic,25);
                strncat(temp, " to Node ", 10);
                char temp2[5];
                itoa(nodeAdress, temp2, 10);
                strncat(temp, temp2, 4);
                strncat(temp, "\"",3);
                mqttpublish("txInfo", temp);/*
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
            }*/
        }else{
            Serial.println("Msg sended to Node");
            if (strncmp(topic, "rfmBackup", 9) != 0) {
                //send 0 Byte retained Message to delete retained messages
                Serial.println("Delete orig Msg");
                mqttpublish(topic, "", true);
                //send orig message to backup topic only if its no config reg
                if ((strncmp(PayloadBck, "{\"p_", 4) == 0)||(strncmp(PayloadBck , "{\"d",3) == 0)||(strncmp(PayloadBck , "{\"t",3) == 0)){
                    Serial.println("Store orig Msg");
                    mqttpublish(NodeBackup_topic, PayloadBck, true);
                }
            }
        }
        //Wir setzen das RFM sofort wieder in den RX Mode es sollten und dürfen hier keine Daten im Buffer sein
        radio.receiveDone(); 
    }else if (length > 0){
        Serial.println(" -> not reachable we will not send");   
    }else if (length == 0){
        Serial.println(" -> 0 byte message. We did nothing.");  
    }else{
        Serial.println(" -> We did nothing."); 
    }
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
    
    nodestats_t *ns;
    ns = get_nodestats(senderId);
    if (ns == NULL) {
        Serial.println("\n\n*** updatedClients failed ***\n");
        return;
    }
    
    Serial.print("got Msg from node[");
    Serial.print(senderId);
    Serial.print("] ");
    uint8_t length;
    length = strlen(message);
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)message[i]);
    }
    Serial.println();
    
    unsigned long sequenceChange, newMessageSequence;
    boolean sendMessage = true; // true to support old Nodes
 
    //Serial.printf("\r\nnode %d msg %s\r\n", senderId, message);
    //newMessageSequence = strtoul(message, NULL, 10);
    Serial.print("getID");
    newMessageSequence = getMessageId((char*)message);
    //newMessageSequence = 0x00000000UL;
    Serial.println("->OK: ");
    ns->recvMessageCount++;
    //Serial.printf("nms %lu rmc %lu rms %lu\r\n",
    //Serial.println("Aa");
    //    newMessageSequence, ns->recvMessageCount, ns->recvMessageSequence);
    if (ns->recvMessageCount != 1) {
        //Serial.println("A");
        // newMessageSequence == 0 means the sender just start up.
        // Or the counter wrapped. But the counter is uint32_t so
        // that will take a very long time.
        if (newMessageSequence != 0) {
            Serial.println("B");
            if (newMessageSequence == ns->recvMessageSequence) {
                ns->recvMessageDuplicate++;
                // we will not send to server if msg is dublicated
                sendMessage = false;
                Serial.println("c");
                sequenceChange = 0;
            }else if (ns->recvMessageSequence){   //if recvMessageSequence = 0 we assume that gw is started and ists the first msg of the node
                if (newMessageSequence > ns->recvMessageSequence) {
                    sequenceChange = newMessageSequence - ns->recvMessageSequence;
                    Serial.println("d");
                    Serial.println(newMessageSequence);
                    Serial.println(ns->recvMessageSequence);
                    Serial.println(sequenceChange);
                }else{
                    Serial.println("e");
                    // we will not send to server if msg is a old msg
                    sendMessage = false;
                    sequenceChange = 0xFFFFFFFFUL - (ns->recvMessageSequence - newMessageSequence);
                }   
            }else{
                Serial.println("f");
                sequenceChange = 1;
            }
            if (sequenceChange > 1) {
                Serial.println("g");
                ns->recvMessageMissing += sequenceChange - 1;
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
    
    
    //Serial.println("built rxTopic");
    char nodeRx_topic[32];
            //subscribe the topic of the Node to get retained messages from Broker -> Node (sleeping Nodes) sw
            snprintf(nodeRx_topic, sizeof(nodeRx_topic), "rfmOut/%d/%d/#", pUserData->networkid, senderId); //Broker -> Node

    //Serial.println("built backupTopic");
    char NodeBackup_topic_temp[32];
            //subscribe the topic of the Node to get retained messages from Broker -> Node (sleeping Nodes) sw
            snprintf(NodeBackup_topic_temp, sizeof(NodeBackup_topic_temp), "rfmBackup/%d/%d/#", pUserData->networkid, senderId); //Broker -> Node
            
    uint8_t nodeAdress = getNodeId(nodeRx_topic);
    uint8_t varNumber = nodeAdress / 32;
    uint8_t bitNumber = varNumber * 32;
    bitNumber = nodeAdress - bitNumber;
    
    /*
      Following commands controlls the gateway to get old msg for backuptopic or normal topic. The gateway also remembers if node is reachable.
      17: subscribe on NodeTopic / Node reachable
      18: unsubscribe both topics / Node NOT reachable
      19: subsrcibe on BackupTopic / Node reachable
      20: unsubscribe both topics / Node reachable
    */
    
    bool command = false;
    
    if (message[0] == 17){
        //mqttClient.publish("rfmIn", "subscribed Node");
        //remember if node is subscribed
        Serial.println("Command 17 subscribe on:");
        Serial.println(nodeRx_topic);
        reachableNode[varNumber] |= (1<<bitNumber);
        mqttSubscribe(nodeRx_topic);
        command = true;
    }else if (message[0] == 18){
        //mqttClient.publish("rfmIn", "unsubscribed Node");
        //remember if node is unsubscribed
        Serial.println("Command 18 unsubscribe on:");
        Serial.println(nodeRx_topic);    
        Serial.println(NodeBackup_topic_temp); 
        reachableNode[varNumber] &= ~(1<<bitNumber);
        mqttUnsubscribe(nodeRx_topic);
        mqttUnsubscribe(NodeBackup_topic_temp);
        command = true;
    }else if (message[0] == 19){
        //subsrcibe node to Backup topic to get old Messages on Node Startup (Node sends 20 on startup, Gateway puts sended messages from nodeRx_topic to NodeBackup_topic_temp)
        Serial.println("Command 19 subscribe on:");
        Serial.println(NodeBackup_topic_temp);
        //Serial.println(nodeRx_topic);
        reachableNode[varNumber] |= (1<<bitNumber);
        mqttSubscribe(NodeBackup_topic_temp);
        //mqttClient.subscribe(nodeRx_topic);
        command = true;
    }else if (message[0] == 20){
        //remember that node is reachable, this kommand is sent by nodes without sleep at startup
        Serial.println("Command 20 unsubscribe on:");
        Serial.println(nodeRx_topic);
        Serial.println(NodeBackup_topic_temp);
        reachableNode[varNumber] |= (1<<bitNumber);
        mqttUnsubscribe(NodeBackup_topic_temp);
        mqttUnsubscribe(nodeRx_topic);
        command = true;
    }
    else if (sendMessage){
        const char *p_start, *p_end;
        uint8_t messageLen = strlen(message);
        p_start = message;
        // Message str input example: "R_12":"125"
        //a message like: "R_12":"125", "g_13":"243" will be publisched on Topic: rfmIn/networkid/nodeid/R_12 Payload: {"R_12":"125"} and on Topic: rfmIn/networkid/nodeid/g_13 Payload: {"g_13":"243"}
        for (uint8_t i =0; i < 10; i++) {
            char hash[10], pubPayload[50], topic[32];
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
            if (p_end) {                                                                              //copy R_12
                strncpy(hash, p_start+1, p_end-p_start-1);
                hash[p_end-p_start-1] = 0;
            }
            //mqttClient.publish("debug", "B");
            p_end = strchr(p_start, ',');               //search , to set pointer
            if (!p_end){
                p_end = strchr(p_start, '\}');          //search } if , is not in string to set pointer
            }
            if (p_end) {                                                                              //copy "R_12":"125"
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
            size_t len = snprintf(topic, sizeof(topic), "rfmIn/%d/%d/%s", pUserData->networkid, senderId, hash); //Node -> Broker 
            if (len >= sizeof(topic)) {
                Serial.println("\n*** MQTT topic truncated ***\n");
            }               
            mqttpublish(topic, pubPayload, true);
        }     
    }else{
        Serial.println("*** Message dublicated! -> discarded! ***");
    }

    static const char PROGMEM JSONtemplateInfo[] =
        R"({"rxMsgCnt":%lu,"rxMsgMiss":%lu,"rxMsgDup":%lu,"rssi":%d})";
    static const char PROGMEM JSONtemplateWeb[] =    
        R"({"msgType":"status","rxMsgCnt":%lu,"rxMsgMiss":%lu,"rxMsgDup":%lu,"senderId":%d,"rssi":%d,"message":")";
    
    //Serial.println("built info payload");
    char infoPayload[255], infoTopic[32];
    snprintf_P(infoPayload, sizeof(infoPayload), JSONtemplateInfo,
      ns->recvMessageCount, ns->recvMessageMissing,
      ns->recvMessageDuplicate, rssi);
    snprintf(infoTopic, sizeof(infoTopic), "rfmIn/%d/%d/rxInfo", pUserData->networkid, senderId); //Node -> Broker    
    mqttpublish(infoTopic, infoPayload, true);
    
    snprintf_P(infoPayload, sizeof(infoPayload), JSONtemplateWeb,
      ns->recvMessageCount, ns->recvMessageMissing,
      ns->recvMessageDuplicate, senderId, rssi);
    
/*    
    for (uint32_t i = 0; i< strlen(message); i++){
        
        if (&message[i] == "\""){
            strncpy(&message[i], "+");
        }        

        if (strcmp(infoPayload[i], "\"", 1) == 0){
            infoPayload[i] = "+"
        }
        if(strcmp(infoPayload[i], "}", 1) == 0){
            infoPayload[i] = "S"
        }

    }
*/
    if (!command){
        strncat(infoPayload, message, length);
    }
    
    strncat(infoPayload, "}", 5);
    // send received message to all connected web clients
    webSocket.broadcastTXT(infoPayload, strlen(infoPayload));
    
}

void setup() {
	Serial.begin(SERIAL_BAUD);
	Serial.println("\nRFM69 WiFi Gateway");
	Serial.println("setup server");

	pinMode(Led_user, OUTPUT);
	digitalWrite(Led_user, LOW);
  
    UserEEPromSetupCallback = UserEEPromSetup;
    UserEEPromChecksumCallback = UserEEPromChecksum;
    UserEEPromAdditioanlSize = sizeof(userEEProm);

    setup_server();
    pUserData = (userEEProm_mt *)endOfEepromServerData;
    
	Serial.print("radio_setup...");
	digitalWrite(Led_user, HIGH);
	radio_setup();
	delay(500);
	digitalWrite(Led_user, LOW);
  
    reachableNode[0] = 0xFFFFFFFF;
    reachableNode[1] = 0xFFFFFFFF;
    reachableNode[2] = 0xFFFFFFFF;
    reachableNode[3] = 0xFFFFFFFF;
    reachableNode[4] = 0xFFFFFFFF;
    reachableNode[5] = 0xFFFFFFFF;
    reachableNode[6] = 0xFFFFFFFF;
    reachableNode[7] = 0xFFFFFFFF;
    
    snprintf(GatewayConsole_topic, sizeof(GatewayConsole_topic), "rfmConsole/%d/%d", pUserData->networkid, pUserData->nodeid);
    
	Serial.println("setup_finished");
}

void loop() {

  radio_loop();
  loop_server();

}

