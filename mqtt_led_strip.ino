//#define LOW_POWER
#ifdef LOW_POWER
#define BRIGHTNESS 1
#define NUM_LEDS 5
#else // Non LOW_POWER
#define BRIGHTNESS 128
#define NUM_LEDS 60
#endif // LOW_POWER

#define WIPE_DELAY (int)(1000 / NUM_LEDS)

/* ***** LED ***** */
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER
#define CLK_PIN 1
#define DAT_PIN 2
#include <FastLED.h>


/* ***** WiFi ***** */
#include "secrets.h"
//#define NANO33
#ifdef NANO33
#include <WiFiNINA.h>
#else // ESP8266
#include <ESP8266WiFi.h>
#endif


/* ***** MQTT ***** */
#include <string.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"


/* ***** Globals ***** */
static CRGB leds[NUM_LEDS];
static WiFiClient wifiClient;
Adafruit_MQTT_Client mqttClient(&wifiClient, mqtt_server, mqtt_port, mqtt_user, mqtt_pass);
Adafruit_MQTT_Subscribe mqttColorStr(&mqttClient, mqtt_topic, MQTT_QOS_1);


static void backoff_blink(unsigned int * seconds, uint32_t color) {
  for (int i = 0; i < *seconds * 5; i++) {  // 5 blinks per second @ 100ms on, 100ms off
    led_blink(0, color, 100);
  }

  *seconds = *seconds * 2;
  if (*seconds > 64) {
    *seconds = 64;
  }
}

/* check_wifi()
 * Checks the status of wifi and reconnects if necessary.
 * First LED will flash Orange if WiFi connection in in progress
 */
static void check_wifi() {
  unsigned int backoff = 1;  // Seconds before retry

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print("w");
      backoff_blink(&backoff, CRGB::Yellow);
    }
    Serial.println("");
    Serial.println("WiFi connected");
  }
}


/* Connect MQTT()
 * Manages connecting / reconnecting to the MQTT broker
 */
static void mqtt_connect() {
  int8_t ret;
  unsigned int backoff = 1;  // Seconds before retry

  if (mqttClient.connected()) {
    return;
  }

  Serial.println("Connecting to MQTT");


  while ((ret = mqttClient.connect()) != 0) {
    Serial.println("MQTT Connection error");
    mqttClient.disconnect();
    backoff_blink(&backoff, CRGB::Purple);
  }

  Serial.println("MQTT Connected");
}


/* set_led()
 * Sets a single LED to a provided color
 * @led_num: 0-index array number of the LED number
 * @color: Color entry from the FastLED library
 */
static inline void set_led(int led_num, uint32_t color) {
  // Turn the LED on, then pause
  leds[led_num] = color;
  FastLED.show();
}


/* led_blink_multi()
 * One "full cycle" of a blink. color1 and color2 blink on and off dor delay_ms
 * @led_num: 0-index array number of the LED number
 * @color1: First color entry from FastLED library
 * @color2: Second color for the blink (black?)
 * @delay_ms: How long each color should display for
 */
static void led_blink_multi(int led_num, uint32_t color1, uint32_t color2, unsigned int delay_ms) {
  set_led(led_num, color1);
  delay(delay_ms);
  set_led(led_num, color2);
  delay(delay_ms);
}


/* led_blink()
 * A blink cycle where the second color is Black
 * @led_num: 0-index array number of the LED number
 * @color: Color entry from the FastLED library
 * @delay_ms: How long each color should display for
 */
static void led_blink(int led_num, uint32_t color, unsigned int delay_ms) {
  led_blink_multi(led_num, color, CRGB::Black, delay_ms);
}


/* set_all()
 * Sets all LEDs to a particular color
 * @color: Color entry from the FastLED library
 * @delay_ms: How long to wait between each LED
 */
static void led_set_all(uint32_t color, unsigned int delay_ms) {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (delay_ms) {
      set_led(i, color);
      delay(delay_ms);
    } else {
      leds[i] = color;
    }
  }
  if (!delay_ms) {
    FastLED.show();
  }
}



/* mqtt_recv()
 * Callback when we receive an MQTT message
 * @msg_size - Size of the message received 
 */
void mqtt_recv(char * msg, uint16_t len) {
  Serial.print("Received: ");
  Serial.println(msg);

  if (strncmp((char*)msg, "red", len) == 0)
    led_set_all(CRGB::Red, WIPE_DELAY);
  else if (strncmp((char*)msg, "orange", len) == 0)
    led_set_all(CRGB::Orange, WIPE_DELAY);
  else if (strncmp((char*)msg, "yellow", len) == 0)
    led_set_all(CRGB::Yellow, WIPE_DELAY);
  else if (strncmp((char*)msg, "green", len) == 0)
    led_set_all(CRGB::Green, WIPE_DELAY);
  else if (strncmp((char*)msg, "blue", len) == 0)
    led_set_all(CRGB::Blue, WIPE_DELAY);
  else
    Serial.print("Invalid message received");
}


/* setup()
 * Usual Arduino setup
 */
void setup() {
  // Give a second to let things settle (like the LED caps)
  // Sub-optimal, but whatever
  delay(1000);

  // Wait for Serial to come up
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // LED Setup
  FastLED.addLeds<APA102, DAT_PIN, CLK_PIN, BGR>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  // WiFi Setup
#ifdef NANO33
// TODO
#else
  WiFi.begin(ssid, pass);
#endif
  check_wifi();

  // MQTT setup
  mqttColorStr.setCallback(mqtt_recv);
  mqttClient.subscribe(&mqttColorStr);

  // Show that we've completed initialization
  led_set_all(CRGB::White, 0);
  delay(1000);
}


/* loop()
 * Usual Arduino loop
 */
void loop() {
  check_wifi();
  mqtt_connect();

  mqttClient.processPackets(10 * 1000);

  Serial.println("Sending keep-alive");
  if (! mqttClient.ping())
    mqttClient.disconnect();
}
