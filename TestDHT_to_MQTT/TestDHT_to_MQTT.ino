#include <PubSubClient.h>
#include <ESP8266WiFi.h>

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
  virginSoilBasic V2.1.2
*/

#define COMPDATE __DATE__ __TIME__
#define build_str  "Version: " __DATE__ " " __TIME__;
#define MODEBUTTON 0                    // Button pin on the esp for selecting modes. D3 for the Wemos!

char* thingsboardServer = "172.31.0.15";

uint8_t MAC_array[6];
char MAC_char[18];

int MessureTime = 60000;




int status = WL_IDLE_STATUS;
unsigned long lastSend;
WiFiClient wifiClient;

PubSubClient client(wifiClient);

#include <IOTAppStory.h>                // IotAppStory.com library
IOTAppStory IAS(COMPDATE, MODEBUTTON);  // Initialize IotAppStory

// ================================================ SETUP ================================================
void setup() {
  Serial.begin(115200);

  lastSend = 0;
  WiFi.macAddress(MAC_array);
  for (int i = 0; i < sizeof(MAC_array); ++i) {
    sprintf(MAC_char, "%s%02x", MAC_char, MAC_array[i]);
  }
  String Name = "Temperatur_";
  Name += MAC_char;
  IAS.preSetDeviceName(Name.c_str());                         // preset deviceName this is also your MDNS responder: http://iasblink.local
  IAS.begin('P');         // Optional parameter: What to do with EEPROM on First boot of the app? 'F' Fully erase | 'P' Partial erase(default) | 'L' Leave intact
  IAS.setCallHome(true);                                    // Set to true to enable calling home frequently (disabled by default)
  IAS.setCallHomeInterval(7200);
  IAS.addField(thingsboardServer, "MQTT_Server", 12, 'L');                    // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.

  client.setServer( thingsboardServer, 1883 );

  //-------- Your Setup starts from here ---------------

}



// ================================================ LOOP =================================================
void loop() {
  IAS.loop();       // this routine handles the reaction of the MODEBUTTON pin. If short press (<4 sec): update of sketch, long press (>7 sec): Configuration

  //-------- Your Sketch starts from here ---------------
  if ( millis() - lastSend > 60000 ) { // Update and send only after 1 seconds

    lastSend = millis();
  }

}


void getAndSendTemperatureAndHumidityData()
{
  char attributes[100];
  String topic = "ESP/" ;

  reconnect();
  Serial.println("sending version data as a kind of keep-alive signal");
  topic = "ESP/" ;
  topic += MAC_char;
  topic += "/version";
  String version = build_str;
  version.toCharArray( attributes, 100 );
  client.publish( topic.c_str(), attributes );

  Serial.println("Collecting temperature data.");

  // Reading temperature or humidity takes about 250 milliseconds!
  float h = 88;
  // Read temperature as Celsius (the default)
  float t = 23;

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C 1");

  String temperature = String(t);
  String humidity = String(h);


  // Just debug messages
  Serial.print( "Sending temperature and humidity : [" );
  Serial.print( temperature ); Serial.print( "," );
  Serial.print( humidity );
  Serial.print( "]   -> " );

  // Prepare a JSON payload string
  String payload = temperature;
  String payload2 = humidity;


  // Send payload
  topic = "ESP/" ;
  topic += MAC_char;
  topic += "/TEMP";
  temperature.toCharArray( attributes, 100 );
  client.publish( topic.c_str(), attributes );
  topic = "ESP/" ;
  topic += MAC_char;
  topic += "/HUM";
  humidity.toCharArray( attributes, 100 );
  client.publish( topic.c_str(), attributes );



  Serial.println( attributes );

}

void reconnect() {
  // Loop until we're reconnected

  Serial.print("Connecting to ThingsBoard node ...");
  // Attempt to connect (clientId, username, password)
  String clientID = "ESP8266 ";
  clientID += MAC_char;
  if ( client.connect(clientID.c_str(), "2FASD2", NULL) ) {
    Serial.println( "[DONE]" );
  } else {
    Serial.print( "[FAILED] [ rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying in 5 seconds]" );
    // Wait 5 seconds before retrying
    delay( 5000 );
  }
}
