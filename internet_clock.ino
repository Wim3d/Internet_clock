/*
   Wifi clock W&J
*/

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#include <String.h>

#include <NTPClient.h>
#include <Timezone.h>
#include <credentials.h> // Wifi credentials
// include the SevenSegmentTM1637 library
#include "SevenSegmentTM1637.h"
//#include "SevenSegmentExtended.h"
//#include "SevenSegmentFun.h"

// for HTTPupdate
const char* host = "Clock_WJ_Client";
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
const char* software_version = "version 5";

WiFiClient espClient;
PubSubClient client(espClient);

#define UPDATE 1000 // ms for updating screen

#define MQTT_RECONNECT_DELAY 10
#define WIFI_CONNECT_TIMEOUT_S 15

// Define NTP properties
#define NTP_OFFSET   0      // offset in seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "nl.pool.ntp.org"  // change this to whatever pool is closest (see ntp.org)

// Set up the NTP UDP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);
uint32_t time1, time2;;
boolean colon = true;       // colon in screen

// Create a display object
#define PIN_CLK 1   // define CLK pin TX pin (any digital pin)
#define PIN_DIO 3   // define DIO pin RX (any digital pin)
//SevenSegmentFun display(PIN_CLK, PIN_DIO);
SevenSegmentTM1637 display(PIN_CLK, PIN_DIO);

String t;
int display_brightness = 50;

time_t local, utc;
long lastReconnectAttempt;

void setup ()
{
  display.begin();            // initializes the display
  delay(500);
  display.setPrintDelay(200);
  display.clear();
  display.setBacklight(display_brightness);
  display.setColonOn(false);
  display.print(software_version);
  delay(300);
  display.clear();
  timeClient.begin();   // Start the NTP UDP client

  // Connect to wifi
  setup_wifi();
  // set program mode
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();
  String s = "IP: ";
  for (int i = 0; i < 4; i++)
    s += i  ? "." + String(WiFi.localIP()[i]) : String(WiFi.localIP()[i]);
  const char* IP = s.c_str();
  display.print(IP);
  delay(300);

  // for HTTPupdate
  MDNS.begin(host);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
  httpServer.on("/", handleRoot);

  client.publish("Clock_WJ/status", "Clock WJ online");
  display.clear();
  display.setColonOn(true);
  time1 = millis();
}

void loop()
{
  client.loop();
  httpServer.handleClient();    // for HTTPupdate
  if (!client.connected())
  {
    if (now() - lastReconnectAttempt > MQTT_RECONNECT_DELAY)
    {
      lastReconnectAttempt = now();
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }

  // update the NTP client and get the UNIX UTC timestamp
  timeClient.update();
  unsigned long epochTime =  timeClient.getEpochTime();

  // convert received time stamp to time_t object
  utc = epochTime;

  // Then convert the UTC UNIX timestamp to local time
  TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Time (Frankfurt, Paris) *Sun, 2*
  TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       //Central European Time (Frankfurt, Paris)
  Timezone CE(CEST, CET);
  local = CE.toLocal(utc);

  if (millis() > time1 + UPDATE)
  {
    display_time();
  }
}


void setup_wifi()
{
  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(mySSID, myPASSWORD);   //mySSID and //myPASSWORD from credentials.h
  time1 = now();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(50);
    time2 = now();
    if ((time2 - time1) > WIFI_CONNECT_TIMEOUT_S) // wifi connection lasts too long
    {
      break;
      ESP.restart();
    }
  }
}

void callback(char* topic, byte * payload, unsigned int length) {
  String stringOne = (char*)payload;

  if ((char)topic[9] == 'b')     // check for the "b" of Clock_WJ/brightness
  {
    // Switch the brightness
    String brightness = stringOne.substring(0, length);
    display_brightness = brightness.toInt();
    if (display_brightness < 10)
      display_brightness = 10; // minimum value of the brightness
    display.setBacklight(display_brightness);
  }
}

boolean reconnect()
{
  if (WiFi.status() != WL_CONNECTED) {    // check if WiFi connection is present
    setup_wifi();
  }
  if (client.connect(host)) {
    // resubscribe
    client.subscribe("Clock_WJ/brightness");
  }
  return client.connected();
}

void display_time()
{
  t = "";
  // format the time to 12-hour format with AM/PM and no seconds
  if (hour(local) < 10) // add a space if hour is under 10
    t += " ";
  t += hour(local);
  //t += ":"; // colon is handled in display
  if (minute(local) < 10) // add a zero if minute is under 10
    t += "0";
  t += minute(local);
  // print the time on the display
  //display.clear();
  //display.setColonOn(false);
  //display.setBacklight(brightness.toInt());
  //display.setColonOn(colon);
  display.print(t);  // display time
  //colon = !colon;
  time1 = millis();
}

void handleRoot() {
  String message = "WimIOT\nDevice: ";
  message += host;
  message += "\nSoftware version: ";
  message += software_version;
  message += "\nTime: ";
  if (hour(local) < 10) // add a space if hour is under 10
    message += " ";
  message += hour(local);
  message += ":";
  if (minute(local) < 10) // add a zero if minute is under 10
    message += "0";
  message += minute(local);
  message += "\nUpdatepath at http://[IP]/update";
  httpServer.send(200, "text/plain", message);
}
