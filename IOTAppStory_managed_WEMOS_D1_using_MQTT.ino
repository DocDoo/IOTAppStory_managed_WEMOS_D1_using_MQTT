/*
  This is an initial sketch to be used as a "blueprint" to create apps which can be used with IOTappstory.com infrastructure
  Your code can be filled wherever it is marked.

  Copyright (c) [2016] [Andreas Spiess]

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  virginSoilFull V2.2.2
*/

char* SketchVersion = "Doorbell activation sensor v1.2.0";
char* MQTT_Topic    = "HGV10/Huset/Fordoer/Ringklokke";

#define COMPDATE __DATE__ __TIME__
#define build_str  "Version: " __DATE__ " " __TIME__;
#define MODEBUTTON D3                                        // Button pin on the esp for selecting modes. D3 for the Wemos! 0 for generic devices

/* ------ NON IAS CODE SECTION START ------ */
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#define DOORBELL_BUTTON_PIN D5                              // Doorbell button pin (with 10k resistor connected to ground for pull-up)

// Declare the variable as volatile to avoid the compiler optimizing it (and thus removing it from the programm)
volatile bool DoorbellButtonPressedFlag = false;
// Declare that the interupt should be chached in RAM
void ICACHE_RAM_ATTR DoorbellButtonPressed ();

WiFiClient espClient;
PubSubClient MQTTclient(espClient);
#define MSG_BUFFER_SIZE (100)
char msg[MSG_BUFFER_SIZE];

/* ------ NON IAS CODE SECTION END ------ */

#include <IOTAppStory.h>                                    // IotAppStory.com library
IOTAppStory IAS(COMPDATE, MODEBUTTON);                      // Initialize IotAppStory

// ================================================ EXAMPLE VARS =========================================
unsigned long DoorbellSensorTimeout;                    // Detach the Doorbell sensor interrupt for 4 secs when activated
unsigned long MQTTConnectionFailedTimeout;              // Determine if MQTT connection attempt has timed out
bool InteruptAttached = false;                          // Determine if the interrupt is currently attached
bool DoorbellButtonPressedProcessingInProgress = false; // Determine if we are currently processing a Dorbell button pressed event

// We want to be able to edit these example variables below from the wifi config manager
// Currently only char arrays are supported.
// Use functions like atoi() and atof() to transform the char array to integers or floats
// Use IAS.dPinConv() to convert Dpin numbers to integers (D6 > 14)

char* MQTT_Broker_IP   = "<MQTT Broker IP>";
char* MQTT_Broker_Port = "0000";
char* MQTT_Username    = "<MQTT Broker Username>";
char* MQTT_Password    = "<MQTT Broker Password>";


// ================================================ SETUP ================================================
void setup() {
  Serial.println("Setup has begun");

  IAS.preSetDeviceName("HGV10_Fordor_Doorbell");        // preset deviceName, this is also your MDNS responder

  IAS.addField(MQTT_Broker_IP, "MQTT Broker IP address", 17, 'L');
  IAS.addField(MQTT_Broker_Port, "MQTT Broker Port", 7, 'N');
  IAS.addField(MQTT_Username, "MQTT Broker Username", 51, 'L');
  IAS.addField(MQTT_Password, "MQTT Broker Password", 51, 'L');

  // Print the current build string
  Serial.print("Current app build : ");
  String version = build_str;
  Serial.println(version);

  // You can configure callback functions that can give feedback to the app user about the current state of the application.
  // In this example we use serial print to demonstrate the call backs. But you could use leds etc.

  IAS.onModeButtonShortPress([]() {
    Serial.println(F(" If mode button is released, I will enter in firmware update mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
  });

  IAS.onModeButtonLongPress([]() {
    Serial.println(F(" If mode button is released, I will enter in configuration mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
  });

  IAS.onModeButtonVeryLongPress([]() {
    Serial.println(F(" If mode button is released, I won't do anything unless you program me to."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
    /* TIP! You can use this callback to put your app on it's own configuration mode */
  });
  
  Serial.println("Doing IAS.begin('P')");
  IAS.begin('P');                                     // Optional parameter: What to do with EEPROM on First boot of the app? 'F' Fully erase | 'P' Partial erase(default) | 'L' Leave intact

  IAS.setCallHome(true);                              // Set to true to enable calling home frequently (disabled by default)
  IAS.setCallHomeInterval(7200);                      // Call home interval in seconds, use 60s only for development. Please change it to at least 2 hours in production

  // Allow for entering Config mode immediately on startup
  Serial.println("Doing first IAS.loop");
  IAS.loop();
  
  //-------- Your Setup starts from here ---------------
  Serial.begin(115200);  

  //Setup MQTT connection
  MQTTclient.setServer(MQTT_Broker_IP, atoi(MQTT_Broker_Port));
  MQTTclient.setCallback(subscribeReceive);

  //Setup board pins
  pinMode(DOORBELL_BUTTON_PIN, INPUT);
  attachInterrupt(DOORBELL_BUTTON_PIN, DoorbellButtonPressed, RISING);
  InteruptAttached = true;

  Serial.println("Subscribing to MQTT topic");
  MQTTclient.subscribe(MQTT_Topic);
  Serial.println("Subscribed OK to MQTT topic");
   
  //Construct the message which will be published to the MQTT interface
  snprintf(msg, MSG_BUFFER_SIZE, "WeMos D1 Mini - First loop of %s", SketchVersion);
  Serial.println(msg);
  publishToMQTT();  
}

void DoorbellButtonPressed() {
  int button = digitalRead(DOORBELL_BUTTON_PIN);
  if(button == HIGH) {
    DoorbellButtonPressedFlag = true;
  }
  return;
}

void triggerDoorbellActiveEvent() {
  //Construct the message which will be published to the MQTT interface
  snprintf(msg, MSG_BUFFER_SIZE, "Doerklokke aktiveret");
  Serial.println(msg);
  publishToMQTT();
}

void triggerDoorbellNotActiveEvent() {
  //Construct the message which will be published to the MQTT interface
  snprintf(msg, MSG_BUFFER_SIZE, "Doerklokke deaktiveret");
  Serial.println(msg);
  publishToMQTT(); 
}
 
// ================================================ LOOP =================================================
void loop() {
  IAS.loop();                                   // this routine handles the calling home functionality and reaction of the MODEBUTTON pin. If short press (<4 sec): update of sketch, long press (>7 sec): Configuration

  //-------- Your Sketch starts from here ---------------
  if ( DoorbellButtonPressedFlag ) {
    // Processing of the DoorbellButtonPressed event is under way. Deattach the interrupt to avoid more events until we are done processing this one and ready for the next event
    if (InteruptAttached) {
      Serial.println("Detaching Interrupt");
      detachInterrupt(digitalPinToInterrupt(DOORBELL_BUTTON_PIN));
      InteruptAttached = false;
    }
    // Now we need to check if we need to respond to the DoorbellButtonPressed event or we must wait due to the timeout
    if (millis() - DoorbellSensorTimeout > 4000) {
      if ( !DoorbellButtonPressedProcessingInProgress ) {
        DoorbellButtonPressedProcessingInProgress = true;
        Serial.println("Dørklokke aktiveret");  
        triggerDoorbellActiveEvent();    
        // MQTT message sent, now update the DoorbellSensorTimeout to not do this again for four seconds
        DoorbellSensorTimeout = millis();
      } else {
        // Doorbell Button Pressed Processing is in progress and the clean up phase is reached as the DoorbellSensorTimeout has passed
        // Reattach the interrupt to prepare for the next ring on the door bell
        Serial.println("Attaching Interrupt");
        attachInterrupt(DOORBELL_BUTTON_PIN, DoorbellButtonPressed, RISING);
        InteruptAttached = true;
        // Send MQTT message that the doorbell is no longer being activated
        triggerDoorbellNotActiveEvent();
        //Reset flag indicating is button was pressed
        DoorbellButtonPressedFlag = false;
        DoorbellButtonPressedProcessingInProgress = false;
      }      
    }
  } 
   
  // Check for incomming messages and retain the connection with the MQTT broker
  MQTTclient.loop();
}

// ================================================ Extra functions ======================================

void subscribeReceive(char* topic, byte* payload, unsigned int length) {
  char data[100]; // buffer for constructing Serial.Print messages
  char* Received_MQTT_Topic = topic;
  payload[length] = '\0';
  char* Received_MQTT_Message = (char*)payload;

  sprintf(data, "In topic [%s], this message arrived [%s]", Received_MQTT_Topic, Received_MQTT_Message);
  Serial.println(data);

  // Use this command to trigger the MQTT interface to put the WeMos D1 into IOTAppStory Configuration Mode
  // Doing this will allow you to access its IOTAppStory configuration and reconfigure it before exiting configuration mode again
  // http://192.168.1.11/admin/plugins/mqttgateway/mqtt.php?topic=HGV10/Huset/Fordoer/Ringklokke&value=IAS_Config_Start
  
  // Check if a command to enter Configuration Mode was received
  if ((strcmp(Received_MQTT_Topic, MQTT_Topic) == 0) && (strcmp(Received_MQTT_Message, "IAS_Config_Start") == 0)) {
    Serial.println("Received MQTT command to enter IAS Configuration Mode. Now restarting WeMos D1 Mini to do that.");
    IAS.espRestart('C');        
  }

  Serial.println("-----------------------");
}

void reconnect() {
  // If MQTT port has not yet been set, assume the user needs to configure the device and enter Config Mode
  if ( atoi(MQTT_Broker_Port) == 0 ) {
    Serial.println("MQTT Broker IP address is not yet set, entering Config Mode...");
    IAS.espRestart('C');    
  }  
  
  // Loop until we're reconnected
  while (!MQTTclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (MQTTclient.connect(clientId.c_str(), MQTT_Username, MQTT_Password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      sprintf(msg, "WeMos D1 Mini - MQTT reconnected for %s", SketchVersion);
      publishToMQTT();
      // ... and resubscribe
      MQTTclient.subscribe(MQTT_Topic);
    } else {
      Serial.print("MQTT reconnect failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" try again in 5 seconds");
      Serial.println("If you want to enter Config mode, then press and connect D3 and GND between 4 and 7 secs starting in 2 seconds...");
      // Wait 2 seconds for user to begin pressing button
      delay(2000);
      IAS.loop(); // this routine handles the calling home functionality and reaction of the MODEBUTTON pin. If short press (<4 sec): update of sketch, long press (>7 sec): Configuration 
      delay(3000);
    }
  }
}

void publishToMQTT() {
  if (!MQTTclient.connected()) {
    reconnect();
  }
  MQTTclient.loop();
  delay(2);
  MQTTclient.publish(MQTT_Topic, msg);
}
