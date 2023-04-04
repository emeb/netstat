/*
  Neopixels updated from within ticker with ping determining base color
  04-01-2023 E. Brombaugh
*/

// http://arduino.esp8266.com/versions/1.6.5-1160-gef26c5f/doc/reference.html
#include <ESP8266WiFi.h>       // https://github.com/esp8266/Arduino
extern "C" {
  #include <user_interface.h>  // https://github.com/esp8266/Arduino
  #include <ping.h>            // https://github.com/esp8266/Arduino
}
#include <Adafruit_NeoPixel.h>
#include <Ticker.h>

// Ticker setup
Ticker led_tick;

// Neopixel setup
#define NP_PIN 13
#define NP_NUMPIXELS 2
Adafruit_NeoPixel pixels(NP_NUMPIXELS, NP_PIN, NEO_RGB + NEO_KHZ800);
int npState = 0;

// Builtin LED setup
#define LED_PIN 14
int ledState = 0;

/******* EDIT for your network *******/
const char* mySSID     = "YOUR_SSID";
const char* myPASSWORD = "YOUR_PASSWORD";
const uint8_t pingCount = 1;     // number of Ping repetition
const uint8_t pingInterval = 1;  // Ping repetition every n sec 
const uint8_t ipCount = 2;       // number of IP addresses in pingIp[] array
const char* pingIp[ipCount] = {"YOUR_ROUTER_IP", "8.8.8.8"};
struct ping_option pingOpt, pOpt;
struct ping_resp pingResp;
int pingstate = 0;

// Network status 0-2 determines color
int netstat = 0;
int count = 0;

// map netstat to colors - 0 or 3 (no router) = red, 1 (no google) = yellow, 2 (good) = green
const int netcolors[4] = {0, 65536/6, 65536/3, 0};

//
// Neopixel update in background
//
void led_update()
{
  int statcolor, wobble, h, c;

  // get base status color
  statcolor = netcolors[netstat&3];

  // get time-based wobble
  wobble = ((npState > 128 ? 255-npState : npState)-64)<<6;
  
  // first neopixel color
  c = pixels.ColorHSV(statcolor+wobble, 255, netstat < 3 ? 255: 0);
  //Serial.print(npState);
  //Serial.print(" ");
  //Serial.println(c);
  if(c == 65535)
    c = 63999; // try to fix dropout at 0xXXFFFF - probably due to 1st LED getting 3.3V levels
  pixels.setPixelColor(0, c);

  // second neopixel color
  c = pixels.ColorHSV(statcolor-wobble, 255, netstat < 3 ? 255: 0);
  //Serial.print(" ");
  //Serial.println(c);
  pixels.setPixelColor(1, c);

  // refresh the string
  pixels.show();

  // update the wave
  npState = (npState + 1) & 255;
}

//
// ping receive callback - invoked for each pings received or timed out
//
void pingRecv(void *arg, void *pdata)
{
#if 0
  //struct ping_option *pingOpt = (struct ping_option *)arg;
  struct ping_resp *pingResp = (struct  ping_resp *)pdata;
  
  if(pingResp->ping_err == -1)
    Serial.println("No Pong (device OFFline)");
  else
  {
    Serial.print("ping recv: bytes = ");
    Serial.print(pingResp->bytes);
    Serial.print(", time = ");
    Serial.print(pingResp->resp_time);
    Serial.println("ms");
  }
#endif
}

//
// ping finished callback - invoked after all pings received or timed out
//
void pingDone(void *arg, void *pdata)
{
  // args are unused
  //struct ping_option *pingOpt = (struct ping_option *)arg;
  //struct ping_resp *pingResp = (struct  ping_resp *)pdata;
  //Serial.println("ping finished");
  pingstate = 1;
}

//
// ping an IP address and return status
//
int do_ping(const char *targetIpAddress)
{
  //Serial.print("Ping IP: ");
  //Serial.println(targetIpAddress);
  
  struct ping_option *pingOpt = &pOpt;
  pingOpt->count = pingCount;
  pingOpt->coarse_time = pingInterval;
  pingOpt->ip = ipaddr_addr(targetIpAddress);
  ping_regist_recv(pingOpt, pingRecv);  // Pong callback function 'pingRecv'
  ping_regist_sent(pingOpt, pingDone);  // Ping finished callback function 'pingDone'
  pingstate = 0;
  ping_start(pingOpt);  // start Ping

  // wait for ping complete
  while(!pingstate)
  {
    delay(10);
  }
  //Serial.println("do_ping - complete");
  //Serial.println(pingResp.ping_err);
  if(pingResp.ping_err == -1)
    return 0;
  
  return 1;
}

//
// get status of internet by pinging local router and google
//
int get_netstate(void)
{
  int result = 0;

  // Must be connected to proceed
  if(WiFi.status() == WL_CONNECTED)
  {
    // first try router  
    if(do_ping(pingIp[0]))
    {
      // then try google
      if(do_ping(pingIp[1]))
        result = 2;
      else
        result = 1;
    }
  }
  
  return result;
}

//
// Arduino start code
//
void setup() {
  // initialize serial communication at 115200 bps
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n\rTickerNeo_netstat");

  // init Wemos D1 LED
  Serial.println("\n\nInitialize I/O");
  pinMode(LED_PIN, OUTPUT);

  // init Neopixels
  Serial.println("Initialize Neopixels");
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)

  // start neopixel update in background
  led_tick.attach(0.1, led_update);

  // init WiFi
  WiFi.disconnect();
  Serial.print("WiFi connecting");
  WiFi.begin(mySSID, myPASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(200);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//
// Arduino loop in foreground to blink onboard LED
//
void loop()
{
  // Toggle the D1 SCK LED
  digitalWrite(LED_PIN, ledState);
  ledState = ledState ^ 1;

#if 0
  // update netstat every 8 sec for testing
  count++;
  netstat = (count >> 3)&3;
#else
  // get actual network status
  netstat = get_netstate()&3;
#endif
  Serial.print("Netstat: ");
  Serial.println(netstat);

  // pause for a second
  delay(1000);
}
