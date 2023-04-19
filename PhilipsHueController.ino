/*
    Philips Hue Controller
    Frank Dulko

    Takes three analog inputs as red, green, blue values, and converts
    the color to HSL before sending to Philips light.

    Values are sent when pus hbutton is released. Uses AceButton library
    to manage button.

    Uses single neopixel to preview color.

    Uses switch to turn the philips light on and off.
*/

#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiNINA.h>   // for Nano 33 IoT, MKR1010
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h"
#include <AceButton.h>
using namespace ace_button;

#define RPIN A0
#define GPIN A1
#define BPIN A2
#define BRI_PIN A3
#define PIXEL_PIN 2
#define NUMPIXELS 1
#define BUTTON_PIN 3
#define SWITCH_PIN 4
#define ON 1
#define OFF 0

AceButton button(BUTTON_PIN);
Adafruit_NeoPixel pixels(NUMPIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

void handleEvent(AceButton*, uint8_t, uint8_t);

int r, g, b, bri, switchState, lastSwitchState, h, s, l;
float hue, sat, lum;

int status = WL_IDLE_STATUS;      // the Wifi radio's status
char hueHubIP[] = "172.22.151.226";  // IP address of the HUE bridge
String hueUserName = "HUR8ubtDPPVmSWzVIciwT-sbL8eX3ROFF-E77ZP5"; // hue bridge username

// make a wifi instance and a HttpClient instance:
WiFiClient wifi;
HttpClient httpClient = HttpClient(wifi, hueHubIP);
// change the values of these two in the arduino_serets.h file:
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  if (!Serial); delay(3000);// wait for serial port to connect.

  //  //attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);
    delay(2000);
  }

  // you're connected now, so print out the data:
  Serial.print("You're connected to the network IP = ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  button.setEventHandler(handleEvent);

  //Set up initial state
  switchState = digitalRead(SWITCH_PIN);
  lastSwitchState = switchState;

  String state = switchState == ON ? "true" : "false";
  pixels.begin();
  pixels.setBrightness(254);

  getRGB();
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();

  RGBtoHSL(r, g, b);

  String cmd[4] = {"on", "hue", "bri", "sat"};
  String values[4] = {state, String(h), String(l), String(s)};
  sendRequest(3, cmd, values);
  ////////////////////////
}

void loop() {
  button.check();

  handleSwitch();

  pixels.clear();

  getRGB();

  pixels.setPixelColor(0, pixels.Color(r, g, b));

  pixels.show();

  RGBtoHSL(r, g, b);
}

void getRGB() {
  r = analogRead(RPIN);
  g = analogRead(GPIN);
  b = analogRead(BPIN);

  r = map(r, 0, 1023, 254, 0);
  g = map(g, 0, 1023, 254, 0);
  b = map(b, 0, 1023, 254, 0);
}

void RGBtoHSL(int r, int g, int b) {
  float rf = float(r) / 254;
  float gf = float(g) / 254;
  float bf = float(b) / 254;
  float minimum = min(min(rf, gf), bf);
  float maximum = max(max(rf, gf), bf);

  lum = 0.5 * (maximum + minimum);

  if (lum < 1) {
    sat = (maximum - minimum) / (1 - abs(2 * lum - 1));
  }
  else if (lum == 1) {
    sat = 0;
  }


  lum *= 100;
  lum = map(lum, 0, 100, 0, 25400);
  lum /= 100;
  l = round(lum);

  sat *= 100;
  sat = map(sat, 0, 100, 0, 25400);
  sat /= 100;
  s = round(sat);

  if (minimum == maximum) {
    hue = 0;
  }

  if (maximum == rf) {
    hue = (gf - bf) / (maximum - minimum);
  }
  else if (maximum == gf) {
    hue = 2 + (bf - rf) / (maximum - minimum);
  }
  else {
    hue = 4 + (rf - gf) / (maximum - minimum);
  }

  hue = hue * 60;
  if (hue < 0) hue = hue + 360;

  h = round(hue);
  h = map(h, 0, 360, 0, 65535);
}

void sendRequest(int light, String cmd[4], String value[4]) {
  // make a String for the HTTP request path:
  String request = "/api/" + hueUserName;
  request += "/lights/";
  request += light;
  request += "/state/";

  String contentType = "application/json";

  String hueCmd = "{";
  // make a string for the JSON command:
  for (int i = 0; i < 4; i++) {
    hueCmd += "\"";
    hueCmd += cmd[i];
    hueCmd += "\":";
    hueCmd += value[i];
    if (i < 3) {
      hueCmd += ",";
    }
  }
  hueCmd += "}";

  // see what you assembled to send:
  Serial.print("PUT request to server: ");
  Serial.println(request);
  Serial.print("JSON command to server: ");
  Serial.println(hueCmd);
  // make the PUT request to the hub:
  httpClient.put(request, contentType, hueCmd);

  // read the status code and body of the response
  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();

  Serial.print("Status code from server: ");
  Serial.println(statusCode);
  Serial.print("Server response: ");
  Serial.println(response);
  Serial.println();
}

void handleEvent(AceButton* /* button */, uint8_t eventType,
                 uint8_t /* buttonState */) {
  switch (eventType) {
    case AceButton::kEventPressed:
      break;
    case AceButton::kEventReleased:
      Serial.println("Button Released");
      String cmd[4] = {"on", "hue", "bri", "sat"};
      String values[4] = {"true", String(h), String(l), String(s)};
      sendRequest(3, cmd, values);
      break;
  }
}

void handleSwitch() {
  lastSwitchState = switchState;
  switchState = digitalRead(SWITCH_PIN);

  if (lastSwitchState != switchState) {
    if (switchState == ON) {
      String cmd[4] = {"on", "hue", "bri", "sat"};
      String values[4] = {"true", String(h), String(l), String(s)};
      sendRequest(3, cmd, values);
    }
    else {
      String cmd[4] = {"on", "hue", "bri", "sat"};
      String values[4] = {"false", String(h), String(l), String(s)};
      sendRequest(3, cmd, values);
    }
  }
}
