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

String SketchVersion = "Doorbell activation sensor v1.1.8";

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

// Declare functions needed for setup function to allow then to be referenced before they are declared
bool SubscribeToMQTTTopic(char Topic[]);
bool SendMQTTMessage(char Message[]);
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

char* WiFi_SSID        = "<WiFi SSID>";                 // INPUT NEEDED: I'm not sure how I need to handle WiFi in the non-IAS
char* WiFi_Password    = "<WiFi Pwd>";                  // INPUT NEEDED: part of the sketch as IAS seems already to have WiFi initialized
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

  //Setup WiFi connection
  EnsureConnectionToWiFi();

  //Setup MQTT connection
  MQTTclient.setServer(MQTT_Broker_IP, atoi(MQTT_Broker_Port));
  MQTTclient.setCallback(callback);

  //Establish MQTTClient connection
  EnsureConnectionToMQTTBroker();

  //Setup board pins
  pinMode(DOORBELL_BUTTON_PIN, INPUT);
  attachInterrupt(DOORBELL_BUTTON_PIN, DoorbellButtonPressed, RISING);
  InteruptAttached = true;
  
  // Publish on MQTT topic, that this device has booted
  Serial.println("Publishing announcement of device boot");
  String bootMessage = "Wemos bootede: " + SketchVersion;
  int bootMessage_len = bootMessage.length() + 1; 
  char bootMessage_char_array[bootMessage_len];
  bootMessage.toCharArray(bootMessage_char_array, bootMessage_len);
  
  while (!SendMQTTMessage(bootMessage_char_array)) {}  
  Serial.println("Published OK");

  Serial.println("Subscribing to topic HGV10/Huset/Fordoer/Ringklokke");
  while (!SubscribeToMQTTTopic("HGV10/Huset/Fordoer/Ringklokke")) {}
  Serial.println("Subscribed OK");
}

bool SendMQTTMessage(char Message[]) {
  Serial.println("Ensuring WiFi is connected...");
  if (EnsureConnectionToWiFi()) {
    Serial.println("WiFi is connected, ensuring MQTT is connected...");
    if (EnsureConnectionToMQTTBroker()) {
      Serial.println("MQTT is connected, publishing message");
      MQTTclient.publish("HGV10/Huset/Fordoer/Ringklokke", Message);
      Serial.println("MQTT message was published!");
    }
  }  
}

bool SubscribeToMQTTTopic(char Topic[]) {
  Serial.println("Ensuring WiFi is connected...");
  if (EnsureConnectionToWiFi()) {
    Serial.println("WiFi is connected, ensuring MQTT is connected...");
    if (EnsureConnectionToMQTTBroker()) {
      Serial.println("MQTT is connected, subscribing to topic");
      MQTTclient.subscribe(Topic);
      //Reset flag indicating is button was pressed
    }
  }    
}

bool EnsureConnectionToWiFi() {
  Serial.println("Ensuring connection to WiFi...");  
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WiFi_SSID, WiFi_Password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.println("WiFi connecting..");
    }
    Serial.println("WiFi connected OK");
  }
  return true;
}

bool EnsureConnectionToMQTTBroker() {
    // If MQTT port has not yet been set, assume the user needs to configure the device and enter Config Mode
  if ( atoi(MQTT_Broker_Port) == 0 ) {
    Serial.println("MQTT Broker IP address is not yet set, entering Config Mode...");
    IAS.espRestart('C');    
  }  
  if (millis() - MQTTConnectionFailedTimeout > 4000) {
    Serial.println("MQTT connecting...");
    Serial.print("MQTT Username = ");
    Serial.println(MQTT_Username);
    if (MQTTclient.connect("ESP8266Client", MQTT_Username, MQTT_Password )) {
      Serial.println("MQTT connected OK");  
      return true;
    } else {
      Serial.print("MQTT connection failed with state ");
      Serial.print(MQTTclient.state());
      MQTTConnectionFailedTimeout =  millis();
      Serial.println("Enter Config mode to set MQTT Username");
      return false;
    }
  }
}

void DoorbellButtonPressed() {
  int button = digitalRead(DOORBELL_BUTTON_PIN);
  if(button == HIGH)
  {
    DoorbellButtonPressedFlag = true;
  }
  return;
}

void triggerDoorbellActiveEvent() {
  SendMQTTMessage("Doerklokke aktiveret");
}

void triggerDoorbellNotActiveEvent() {
  SendMQTTMessage("Doerklokke deaktiveret");
}
 
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
 
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
 
  Serial.println();
  Serial.println("-----------------------");
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
