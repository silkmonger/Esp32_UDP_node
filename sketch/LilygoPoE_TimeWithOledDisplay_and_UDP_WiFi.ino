#include <WiFi.h>
#include "time.h"

#define __MINIMAL_HW__

#ifndef __MINIMAL_HW__
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

#define __SKETCH_MAIN__
#include "shared_vars.h"

// extern char push_notification_buffer[PUSH_NOTIFICATION_STRING_SIZE];
String wired_lan_mac_address; // wired LAN events, if applicable, overwrite this with correct MAC address
bool   wired_lan_connection;
bool   wifi_connection_lost;
uint8_t wifi_reconnect_count;
unsigned long wifi_connection_loss_millis;
unsigned long wifi_next_reconnect_attempt_millis;
const uint16_t WIFI_RECONNECT_INTERVAL = 5000;

// #define __USE_WIRED_LAN__
#ifdef __USE_WIRED_LAN__
  #include <ETH.h>
  
  // Ethernet related #defines
  #ifdef ETH_CLK_MODE
  #undef ETH_CLK_MODE
  #endif
  #define ETH_CLK_MODE    ETH_CLOCK_GPIO17_OUT
  // Pin# of the enable signal for the external crystal oscillator (-1 to disable for internal APLL source)
  #define ETH_POWER_PIN   -1
  // Type of the Ethernet PHY (LAN8720 or TLK110)
  #define ETH_TYPE        ETH_PHY_LAN8720
  // I²C-address of Ethernet PHY (0 or 1 for LAN8720, 31 for TLK110)
  #define ETH_ADDR        0
  // Pin# of the I²C clock signal for the Ethernet PHY
  #define ETH_MDC_PIN     23
  // Pin# of the I²C IO signal for the Ethernet PHY
  #define ETH_MDIO_PIN    18

  static bool eth_connected = false;
  static bool eth_was_connected = false;

  void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
      case SYSTEM_EVENT_ETH_START:
        Serial.println("ETH Started");
        //set eth hostname here
        ETH.setHostname("esp32-ethernet");
        break;
      case SYSTEM_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected");
        break;
      case SYSTEM_EVENT_ETH_GOT_IP:
        Serial.print("ETH MAC: ");
        Serial.print(ETH.macAddress());
        Serial.print(", IPv4: ");
        Serial.print(ETH.localIP());
        if (ETH.fullDuplex()) {
          Serial.print(", FULL_DUPLEX");
        }
        Serial.print(", ");
        Serial.print(ETH.linkSpeed());
        Serial.println("Mbps");
        eth_connected = true;

        break;
      case SYSTEM_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");
        eth_connected = false;
        break;
      case SYSTEM_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        eth_connected = false;
        break;
      default:
        break;
    }
  }

#endif // __USE_WIRED_LAN__

/*
 * modified 2023/01/13 by amit barak to connect to home WiFi
 *                     this should also open a UDP port, to test UDP transactions (vs. PC python script for start)
 *
 * there is a good reference at https://youtu.be/v8JodmdxIKU?t=641 (the guy with the swiss accent)
 *   highlighting the LilygoPoE pinout restrictions, and I2C pin re-assignment
 *
 * 2023/01/14 10:28 - successful UDP datagram received (sent from Python script on Win10 PC)
 *
 */

WiFiClient http_client;

// function prototypes from my_UDP tab/namespace
void UDP_enable(void);
bool UDP_enabled(void);
void UDP_receive_and_print_to_serial(void);
void UDP_receive_and_acknowledge(void);
void UDP_restart(void);
bool UDP_send_string_interrupt(char*);
bool UDP_send_bytes_interrupt(uint8_t, uint8_t);
void pn_check_gpio(unsigned long);
void pn_erase_credentials(void);
void ch_setup(void);
// const char* ssid       = "YOUR_SSID";
// const char* password   = "YOUR_PASS";
const char* ssid       = "enter ssid";
const char* password   = "enter wifi key";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 3600;

char timeStringBuff[50]; //50 chars should be enough
char shortTimeStringBuff[30];

bool status = false;
bool previous_status = false;

int gpio_32 = 32;
int gpio_33 = 33;
unsigned long gpio_counter = 0;

unsigned long count = 0;
unsigned long serial_print_time = 0;
unsigned long serial_print_period = 60000;

const int SDA_PIN = 14;
const int SCL_PIN = 15;


// const char * string_var_initial_value[] = {"1", "12", "123"};


// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...

#ifndef __MINIMAL_HW__
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#endif
bool display_ok = false;


// global (variable) strings for general purpose command handshake
// const uint8_t STRING_VARS_COUNT = 3;
// const uint8_t STRING_VARS_SIZE = 32;
char string_var[STRING_VARS_COUNT][STRING_VARS_SIZE] = {"first", "second", "third"};
uint8_t led_pin_number;
bool led_pin_enabled;
bool led_polarity;

// main tab function prototypes
bool check_wifi_connection(void);
void led_on(bool turn_it_on);
void led_off(void);

// bool enter_api_key_token(void);
// bool enter_api_key_user(void);
// bool enter_api_credentials(void);
void setup()
{
  wifi_connection_lost = false;
  wifi_reconnect_count = 0;
  wifi_connection_loss_millis = 0xDEADBEEF;
  wifi_next_reconnect_attempt_millis = 0xDEADBEEF;

  led_pin_number = 0;
  led_pin_enabled = false;
  led_polarity = false;
  // Initialize I2C on pins 14, 15
  // Wire.setClock(100000);
  #ifndef __MINIMAL_HW__
  Wire.begin(SDA_PIN, SCL_PIN);
  #endif  

  // GPIO configuration
  #ifndef __MINIMAL_HW__
    pinMode(gpio_32, OUTPUT);
    pinMode(gpio_33, OUTPUT);
    update_gpios();
  #endif
  
  Serial.begin(115200);

  #ifndef __MINIMAL_HW__
    // try to initialize display  
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if(display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
      display_ok = true;
      Serial.println(F("SSD1306 allocation succeeded"));
      // Show initial display buffer contents on the screen --
      // the library initializes this with an Adafruit splash screen.
      display.display();
      delay(2000); // Pause for 2 seconds

      oled_text("here we go ...", "");

      delay(2000);

      // Clear the buffer
      display.clearDisplay();
    }
    else
    {
      Serial.println(F("SSD1306 allocation failed"));
    }
  #endif

  #ifdef __USE_WIRED_LAN__

    WiFi.onEvent(WiFiEvent);
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
    oled_text("Wired LAN image", "");
    Serial.println("Wired LAN image");
    wired_lan_mac_address = ETH.macAddress();
    wired_lan_connection = true;
    delay(1000);

  #else // __USE_WIRED_LAN__

    //connect to WiFi
    wired_lan_mac_address = "";
    wired_lan_connection = false;
    Serial.printf("Connecting to %s ", ssid);
    oled_text("Connecting", ssid);
    // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);  // necessary work-around for 'setHostname'
    // WiFi.setHostname("ESP32_PoE");
    // set_DHCP_hostname("ESP32_PoE");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(2000);
        Serial.print(".");
    }
    Serial.println(" CONNECTED");
    oled_text("Connected", "");

  #endif // __USE_WIRED_LAN__
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  serial_print_time = millis();

  // for(int i=0; i < STRING_VARS_COUNT; i++)
  //   strcpy(string_var[i], string_var_initial_value[i]);

  pn_erase_credentials();
  // strcpy(push_notification_buffer, "");
  delay(1000);

  UDP_enable();
  ch_setup();
} // void setup()

void update_gpios(void)
{
  digitalWrite(gpio_32, gpio_counter & 0x1);
  digitalWrite(gpio_33, (gpio_counter >> 1) & 0x1);
  gpio_counter++;  
}

String ip_address_string(IPAddress ip)
{
  return String(ip & 0x000000ff) + '.'
       + String((ip & 0x0000ff00) >> 8) + '.'
       + String((ip & 0x00ff0000) >> 16) + '.'
       + String((ip & 0xff000000) >> 24);
}

void loop()
{
  delay(10);
  update_gpios();
  // delay(30000);
  unsigned long millis_now = millis();
  pn_check_gpio(millis_now);
  bool period_expired = (millis_now > (serial_print_time + serial_print_period)) || (millis_now < serial_print_time);
  // if((millis_now > (serial_print_time + serial_print_period)) || (millis_now < serial_print_time))
  if( period_expired or (count < 2) )
  {
    previous_status = status;
    #ifdef __USE_WIRED_LAN__
      status = eth_connected;
      String ip_address = ip_address_string(ETH.localIP());

      if(status != previous_status)
      {
        String line1 = status ? "Connected" : "Disconnected";
        String line2 = status ? String(ETH.linkSpeed()) + " Mbps" : "";
        oled_text(line1, line2);
        delay(2000);
      }
    #else // __USE_WIRED_LAN__
      status = WiFi.isConnected();
      String ip_address = ip_address_string(WiFi.localIP());
    #endif // __USE_WIRED_LAN__
    // String ip_address = ip_address_string(WiFi.localIP());
    String s = "\n" + String(count) + " loops done (" + status + " " + ip_address + ")";
    Serial.println(s);
    printLocalTime();
    // oled_text(ip_address);
    oled_text(shortTimeStringBuff, ip_address + " (" + String(count) + ")");
    // UDP_send_string_interrupt("---");
    UDP_send_bytes_interrupt(0, 0);

    serial_print_time = millis_now;
    count++;
  }
  // UDP_receive_and_print_to_serial();
  if(wired_lan_connection || check_wifi_connection(millis_now))
  {
    UDP_receive_and_acknowledge();
  }
}

#ifdef __MINIMAL_HW__
void oled_text(String first_line, String second_line){}
#else
void oled_text(String first_line, String second_line)
{
  if(display_ok)
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 1);
    display.println(first_line);
    if(second_line != "")
    {
      display.setCursor(10, 17);
      display.println(second_line);
    }
    display.display();
    delay(100);
  }
} // void oled_text(String first_line, String second_line)
#endif

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  strftime(shortTimeStringBuff, sizeof(shortTimeStringBuff), "%b %d, %H:%M:%S", &timeinfo);
  Serial.println(timeStringBuff);
} // void printLocalTime()


#ifndef __MINIMAL_HW__
void testdrawrect(void) {
  display.clearDisplay();

  for(int16_t i=0; i<display.height()/2; i+=2) {
    display.drawRect(i, i, display.width()-2*i, display.height()-2*i, SSD1306_WHITE);
    display.display(); // Update screen with each newly-drawn rectangle
    delay(1);
  }

  delay(2000);
}

void testfillrect(void) {
  display.clearDisplay();

  for(int16_t i=0; i<display.height()/2; i+=3) {
    // The INVERSE color is used so rectangles alternate white/black
    display.fillRect(i, i, display.width()-i*2, display.height()-i*2, SSD1306_INVERSE);
    display.display(); // Update screen with each newly-drawn rectangle
    delay(1);
  }

  delay(2000);
}
#endif

void debug_error(String message)
{
  Serial.println("[NOTOK] debug: " + message);
}

void debug_ok(String message)
{
  Serial.println("[OK] debug: " + message);
}

void reset_loop_counter(void)
{
  count = 0;
}

bool check_wifi_connection(unsigned long _millis)
{
  if(wired_lan_connection)
    return false;

  bool connected=false;
  if(wifi_connection_lost)
  {
    if(_millis > wifi_next_reconnect_attempt_millis)
    {
      WiFi.disconnect();
      WiFi.reconnect();
      for(int i=0; ((i<4) && !connected); i++)
      { 
        delay(100); 
        connected = (WiFi.status() == WL_CONNECTED) ? true : false; 
      }

      if(connected)
      {
        led_on(false);
        wifi_connection_lost = false;
        wifi_reconnect_count++;
        UDP_restart();
      }
      else
      {
        led_on(true);
        wifi_next_reconnect_attempt_millis = _millis + WIFI_RECONNECT_INTERVAL;
        Serial.println("[NOTOK] WiFi not connected, reconnect attempt failed");
      }
    }
  }
  else
  {
    if(WiFi.status() == WL_CONNECTED)
    {
      connected = true;
    }
    else{
      wifi_connection_lost = true;
      wifi_connection_loss_millis = _millis;
      wifi_next_reconnect_attempt_millis = _millis + WIFI_RECONNECT_INTERVAL;
      Serial.println("[NOTOK] WiFi connection just failed. reconnect interval: " + String(WIFI_RECONNECT_INTERVAL));
    }
  }
  return connected;
} // bool check_wifi_connection(void)

void led_off(void){ led_on(false); }

void led_on(bool turn_it_on)
{
  if(led_pin_enabled)
  {
    uint8_t output_value = turn_it_on == led_polarity ? HIGH : LOW;
    digitalWrite(led_pin_number, output_value);
  }
} // 