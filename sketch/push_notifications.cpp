#include "esp32-hal-gpio.h"
#include "stdint.h"
/*
 filename: push_notifications.cpp

 description: namespace and methods for HTTP client interface, with 'pushover' push notifications

    using WiFiClientSecure, but no digital certificate

    potentially promising video: https://youtu.be/32VcKyI0dio
    per this (and some other) videos, the way to work is a step by step 'client.print()' or 'client.println()' calls
    sending the HTTP request bit by bit.
    there seems to be a debate wheter connect() should be done to port 80 or port 443

  good video on ArduinoJson: https://youtu.be/NYP_CxdYzLo

 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include "shared_vars.h"
extern char string_var[][STRING_VARS_SIZE];
char push_notification_buffer[PUSH_NOTIFICATION_STRING_SIZE];

void debug_error(String);
void debug_ok(String);

// external function prototypes
void led_on(bool turn_it_on);
void led_off(void);

namespace my_PN
{
  // String http_post_data = "{'token': 'xxxxxxxxxxxx', 'user': 'ssssssssssssss', 'message': '123'}";
  String http_post_data = "";
  WiFiClientSecure client;

  const uint8_t API_TOKEN_SIZE = 32;
  const uint8_t API_USER_SIZE = 32;

  const char * server = "https://api.pushover.net/1/messages.json";
  const char * test_server = "www.howsmyssl.com";
  const char * weather_server = "api.openweathermap.org";

  char pushover_api_token[API_TOKEN_SIZE];
  char pushover_api_user[API_USER_SIZE];
  bool pushover_api_cerdentials_ready = false;
  char push_notification_buffer[PUSH_NOTIFICATION_STRING_SIZE];

  // const char * dummy_api_json_response = "{'coord': {'lon': 34.8265, 'lat': 32.1701}, 'weather': [{'id': 801, 'main': 'Clouds', 'description': 'few clouds', 'icon': '02d'}], 'base': 'stations', 'main': {'temp': 295.23, 'feels_like': 294.28, 'temp_min': 294.46, 'temp_max': 295.23, 'pressure': 1013, 'humidity': 30}, 'visibility': 10000, 'wind': {'speed': 3.09, 'deg': 190}, 'clouds': {'all': 20}, 'dt': 1678968166, 'sys': {'type': 1, 'id': 6845, 'country': 'IL', 'sunrise': 1678938623, 'sunset': 1678981720}, 'timezone': 7200, 'id': 294778, 'name': 'Herzliya', 'cod': 200}";
  char api_response[] = "{\"coord\": {\"lon\": 34.8265, \"lat\": 32.1701}, \"weather\": [{\"id\": 801, \"main\": \"Clouds\", \"description\": \"few clouds\", \"icon\": \"02d\"}], \"base\": \"stations\", \"main\": {\"temp\": 295.23, \"feels_like\": 294.28, \"temp_min\": 294.46, \"temp_max\": 295.23, \"pressure\": 1013, \"humidity\": 30}, \"visibility\": 10000, \"wind\": {\"speed\": 3.09, \"deg\": 190}, \"clouds\": {\"all\": 20}, \"dt\": 1678968166, \"sys\": {\"type\": 1, \"id\": 6845, \"country\": \"IL\", \"sunrise\": 1678938623, \"sunset\": 1678981720}, \"timezone\": 7200, \"id\": 294778, \"name\": \"Herzliya\", \"cod\": 200}";
  char json_x[] = "{\"sensor\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}";
  StaticJsonDocument<200> json_doc;

  const uint16_t HTTP_RESPONSE_BUFFER_SIZE = 1024;
  char http_response_buffer[HTTP_RESPONSE_BUFFER_SIZE];

  // push notification on GPIO value change (added 2023/04/12)
  bool           push_on_gpio_enabled;
  uint8_t        push_on_gpio_pin_number;
  bool           push_on_gpio_true_rising_false_falling;
  int            push_on_gpio_previous_read_value;
  uint16_t       push_on_gpio_backoff_seconds;
  const uint16_t push_on_gpio_minimum_backoff = 30;
  unsigned long  push_on_gpio_next_push_millis;
  unsigned long  push_on_gpio_backoff_window_millis;
  bool           push_on_gpio_backoff_wraparound_flag;                 // millis() wrap-around logic
  // added 2023-04-13
  uint16_t       push_on_gpio_supressed_events;
  unsigned long  push_on_gpio_last_edge_detect_millis;
  bool           push_on_gpio_backoff_flag;

  //
  // GPIO events log
  //
  const uint8_t  GPEL_SIZE=6;
  uint32_t       gpel_event_list[GPEL_SIZE];
  uint8_t        gpel_write_ptr;
  uint8_t        gpel_read_ptr;
  uint16_t       gpel_overrun_count;

  // private function prototypes
  void gpio_event_handler(unsigned long);
  uint8_t gpio_event_log_event_count(void);
  void purge_gpio_event_log(void);
  uint32_t read_from_gpio_event_log(void);

  void post_request(void)
  {
    client.setInsecure();
    if (!client.connect(server, 443))
    {
      Serial.println("Connection failed!");
    }
    else {
      Serial.println("Connected to server!");
    }
    client.stop();
  } // void post_request(void)

  uint32_t get_request_test(void)
  {
    client.setInsecure();
    uint32_t rv = 0xDEADBEEF;
    if (!client.connect(test_server, 443))
      Serial.println("Connection failed!");
    else
    {
      Serial.println("Connected to server!");
      // Make a HTTP request:
      client.println("GET https://www.howsmyssl.com/a/check HTTP/1.0");
      client.println("Host: www.howsmyssl.com");
      client.println("Connection: close");
      client.println();

      while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
          Serial.println("headers received");
          break;
        }
      }
      // if there are incoming bytes available
      // from the server, read them and print them:
      uint32_t char_count = 0;
      while (client.available()) {
        char c = client.read();
        Serial.write(c);
        if((++char_count % 64) == 0)
          Serial.println("    ");
      }

      client.stop();
      Serial.println();
      Serial.println(String(char_count) + " characters received as payload\n");
      rv = char_count;
    } // successful connection
    return rv;
  } // void get_request_test(void)

  void print_gpio_event_list(void)
  {
    uint32_t event_millis;
    Serial.println("gpio_event_log_event_count() = " + String(gpio_event_log_event_count()) + "/" + String(GPEL_SIZE));
    while(gpio_event_log_event_count() != 0)
    {
      event_millis = read_from_gpio_event_log();
      Serial.println("  " + String(gpio_event_log_event_count()) + " "+ String(event_millis) + " milliseconds");
    }
    Serial.println("gpel_overrun_count = " + String(gpel_overrun_count));
  } // void print_gpio_event_list(void)

  uint32_t openweather_get_request(void)
  {
    client.setInsecure();
    uint32_t rv = 0xDEADBEEF;
    if (!client.connect(weather_server, 443))
      Serial.println("Connection failed!");
    else
    {
      String lon = "34.826509";
      String lat = "32.170149";
      String k1 = "b243a10a58a97f95080af796a429b718";
      String request_url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(lat) + "&lon=" + String(lon) + "&APPID=" + String(k1);
      Serial.println("Connected to server!");
      // Make a HTTP request:
      client.println("GET " + request_url + " HTTP/1.0");
      client.println("Host: " + String(weather_server));
      client.println("Connection: close");
      client.println();

      while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
          Serial.println("headers received");
          break;
        }
      }
      // if there are incoming bytes available
      // from the server, read them and print them:
      uint32_t char_count = 0;
      bool buffer_overrun = false;
      while (client.available()) {
        char c = client.read();
        Serial.write(c);
        if(char_count < HTTP_RESPONSE_BUFFER_SIZE)
          http_response_buffer[char_count] = c;
        else
          buffer_overrun = true;
        if((++char_count % 64) == 0)
          Serial.println("    ");
      }
      client.stop();
      Serial.println();
      Serial.println(String(char_count) + " characters received as payload\n");

      // try parsing response
      if(buffer_overrun)
        Serial.println("[NOTOK] response buffer overrun");
      else
      {
        http_response_buffer[char_count] = NULL;
        DeserializationError error = deserializeJson(json_doc, http_response_buffer);
        if (error)
          Serial.println("[NOTOK] JSON parsing failed");
        else
        {
          // copy-paste from arduinojson assistant
          JsonObject main = json_doc["main"];
          float main_temp = main["temp"]; // 287.27
          float main_feels_like = main["feels_like"]; // 287.04
          float main_temp_min = main["temp_min"]; // 287.23
          float main_temp_max = main["temp_max"]; // 288.2
          int main_pressure = main["pressure"]; // 1011
          int main_humidity = main["humidity"]; // 88
          Serial.println("[OK] Temperature:" + String(main_temp) + ", feels like " + String(main_feels_like) + "\n\n");
        } // successful parsing
      } // response buffer intact

      rv = char_count;
    } // successful connection
    return rv;
  } // uint32_t openweather_get_request(void)

  uint32_t test_post_request(void)
  {
    char * post_test_server = "httpbin.org";
    client.setInsecure();
    uint32_t rv = 0xDEADBEEF;
    if (!client.connect(post_test_server, 443))
      Serial.println("Connection failed!");
    else
    {
      String request_url = "https://httpbin.org/post?x=123";
      Serial.println("Connected to server!");
      // Make a HTTP request:
      client.println("POST " + request_url + " HTTP/1.0");
      client.println("Host: " + String(post_test_server));
      client.println("Connection: close");
      client.println();

      while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
          Serial.println("headers received");
          break;
        }
      }
      // if there are incoming bytes available
      // from the server, read them and print them:
      uint32_t char_count = 0;
      while (client.available()) {
        char c = client.read();
        if(char_count < HTTP_RESPONSE_BUFFER_SIZE)
          http_response_buffer[char_count] = c;
        Serial.write(c);
        if((++char_count % 64) == 0)
          Serial.println("    ");
      }
      http_response_buffer[char_count-1] = NULL;

      client.stop();
      Serial.println();
      Serial.println(String(char_count) + " characters received as payload (" + String(strlen(http_response_buffer)) + ")");
      DeserializationError error = deserializeJson(json_doc, http_response_buffer);
      if(error)
      {
        Serial.println("[NOTOK] JSON parsing failed");
      }
      else
      {
        const char * x_value = json_doc["args"]["x"];
        Serial.println("[OK] JSON parsing success, x=" + String(x_value));
      }
      Serial.println("\n");
      rv = char_count;
    } // successful connection
    return rv;
  } // uint32_t test_post_request()

  uint32_t push_notification(void)
  {
    if(!pushover_api_cerdentials_ready)
    {
      Serial.println("[NOTOK] pushover API credentials missing");
      return 0xDEADBEEF;
    }

    char * pushover_server = "api.pushover.net";
    client.setInsecure();
    uint32_t rv = 0xDEADBEEF;
    if (!client.connect(pushover_server, 443))
      Serial.println("Connection failed!");
    else
    {
      String message = strlen(push_notification_buffer)==0 ? "123" : String(push_notification_buffer);
      String request_url = "https://api.pushover.net/1/messages.json?token=" + String(pushover_api_token) + "&user=" 
                                                                             + String(pushover_api_user) + "&message=" + message;
      Serial.println("Connected to server! (text: " + message + ")");
      // Make a HTTP request:
      client.println("POST " + request_url + " HTTP/1.0");
      client.println("Host: " + String(pushover_server));
      client.println("Connection: close");
      client.println();

      while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
          Serial.println("headers received");
          break;
        }
      }
      // if there are incoming bytes available
      // from the server, read them and print them:
      uint32_t char_count = 0;
      while (client.available()) {
        char c = client.read();
        if(char_count < HTTP_RESPONSE_BUFFER_SIZE)
          http_response_buffer[char_count] = c;
        Serial.write(c);
        if((++char_count % 64) == 0)
          Serial.println("    ");
      }
      http_response_buffer[char_count-1] = NULL;

      client.stop();
      Serial.println();
      Serial.println(String(char_count) + " characters received as payload (" + String(strlen(http_response_buffer)) + ")");
      DeserializationError error = deserializeJson(json_doc, http_response_buffer);
      if(error)
      {
        Serial.println("[NOTOK] JSON parsing failed");
      }
      else
      {
        Serial.println("[OK] JSON parsing success");
        int status = json_doc["status"];
        const char * request = json_doc["request"];
        Serial.println("     Server response: status=" + String(status) + ", request=" + String(request));
        strcpy(string_var[0], request);
      }
      Serial.println("\n");
      rv = char_count;
    } // successful connection
    return rv;
  } // uint32_t push_notification(void)

  void parse_json_test(void)
  {
    // using example at: https://arduinojson.org/v6/example/parser/
    Serial.println("starting 'parse_json_test()'");
    DeserializationError error = deserializeJson(json_doc, json_x);
    if(error) {
      Serial.println("[NOTOK] 'parse_json_test' failed");
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    }
    else
    {
      Serial.println("[OK] 'parse_json_test' passed");
      const char* sensor = json_doc["sensor"];
      long time = json_doc["time"];
      double latitude = json_doc["data"][0];
      double longitude = json_doc["data"][1];
      char lon_buff[10];
      char lat_buff[10];

      // Print values.
      dtostrf(longitude, 4, 6, lon_buff); // simple String(longitude) won't do, it rounds to two decimal places
      dtostrf(latitude, 4, 6, lat_buff);  // simple String(latitude) won't do, it rounds to two decimal places
      String readout = "Sensor: " + String(sensor) + ", time: " + String(time) + ", GPS coordinates: (" + lat_buff + ", " + lon_buff + ")";
      Serial.println(readout);
      // Serial.println(sensor);
      // Serial.println(time);
      // Serial.println(latitude, 6);
      // Serial.println(longitude, 6);
    } // JSON parsing (deserialization) success
  } // void parse_json_test(void)

  uint32_t set_pushover_credentials(void)
  {
    /*-----------------------------------------------------------------------------------------------
     copy string variables 0 and 1 to API user and token buffers, respectively.
     then erase string variables 0 and 1
     this is needed so that we do not keep hard-coded credentials in the sketch
     -----------------------------------------------------------------------------------------------*/
     uint32_t rv = NOTOK_STATUS_32b; // error
     if((strlen(string_var[0]) < API_USER_SIZE) && (strlen(string_var[1]) < API_TOKEN_SIZE))
     {
       strcpy(pushover_api_user, string_var[0]);
       strcpy(pushover_api_token, string_var[1]);
       pushover_api_cerdentials_ready = true;
       for(int n=0; n<2; n++)
         strcpy(string_var[n], "----------------------------");
       
       Serial.println("pushover USER=" + String(pushover_api_user));
       Serial.println("pushover TOKEN=" + String(pushover_api_token));
       rv = OK_STATUS_32b;
     }
     else
     {
       Serial.println("[NOTOK] string variables are too long");
       pushover_api_cerdentials_ready = false;
       rv = NOTOK_STATUS_32b;
     }
     return rv;
  } // uint32_t set_pushover_credentials(void)

  uint16_t set_push_notification_text(uint8_t string_var_index)
  {
    // return zero for error, or text length upon success
    if(string_var_index >= STRING_VARS_COUNT)
    {
      debug_error("string index " + String(string_var_index) + " out of bounds");
      return 0;
    }

    if(strlen(string_var[string_var_index]) >= PUSH_NOTIFICATION_STRING_SIZE)
    {
      debug_error("text too large (" + String(strlen(string_var[string_var_index])) + ") characters");
      return 0;
    }

    strcpy(push_notification_buffer, string_var[string_var_index]);
    debug_ok("push notification text set to '" + String(push_notification_buffer) + "'");
    return strlen(string_var[string_var_index]);
  } // uint16_t set_push_notification_text(uint8_t string_var_index)

  void purge_gpio_event_log(void)
  {
    gpel_write_ptr = 0;
    gpel_read_ptr = 0;
    gpel_overrun_count = 0;
  } // void purge_gpio_event_log(void)

  void write_to_gpio_event_log(uint32_t _millis)
  {
    gpel_event_list[gpel_write_ptr++] = _millis;
    gpel_write_ptr %= GPEL_SIZE;
    if(gpel_write_ptr == gpel_read_ptr)
    {
      gpel_overrun_count++;
      gpel_read_ptr++;
      gpel_read_ptr %= GPEL_SIZE;
    }
  } // void write_to_gpio_event_log(uint_32t _millis)

  uint32_t read_from_gpio_event_log(void)
  {
    // !!! UNSAFE !!!
    // this function advanced read pointer unconditionally, which may cause list corruption.
    uint32_t read_value = gpel_event_list[gpel_read_ptr];
    if(gpel_read_ptr != gpel_write_ptr)
    {
      gpel_read_ptr++;
      gpel_read_ptr %= GPEL_SIZE;
    }
    return read_value;
  } // uint32_t read_from_gpio_event_log(void)

  uint8_t gpio_event_log_event_count(void)
  {
    if(gpel_write_ptr >= gpel_read_ptr)
      return gpel_write_ptr - gpel_read_ptr;
    else
      return GPEL_SIZE + gpel_write_ptr - gpel_read_ptr;
  } // uint8_t gpio_event_log_event_count(void)

  void initialize_push_on_gpio(void)
  {
    push_on_gpio_enabled = false;
    push_on_gpio_pin_number = 0;
    push_on_gpio_true_rising_false_falling = true;
    push_on_gpio_previous_read_value = LOW;
    push_on_gpio_backoff_seconds = 30;
    push_on_gpio_backoff_wraparound_flag = false;
    push_on_gpio_supressed_events = 0;
    push_on_gpio_last_edge_detect_millis = 0;
  } // void initialize_push_on_gpio(void)

  uint32_t enable_push_on_gpio(uint8_t pin_number, bool edge_polarity, uint16_t backoff_seconds)
  {
    unsigned long _millis = millis();
    if(pin_number == 0)
    {
      Serial.println("enable_push_on_gpio: DISABLED");
      push_on_gpio_enabled = false;
      push_on_gpio_pin_number = 0;
      push_on_gpio_backoff_window_millis = 0;
      led_off();
    }
    else
    {
      push_on_gpio_enabled = true;
      push_on_gpio_pin_number = pin_number;
      push_on_gpio_true_rising_false_falling = edge_polarity;
      push_on_gpio_backoff_seconds = backoff_seconds;
      pinMode(pin_number, INPUT);
      push_on_gpio_previous_read_value = digitalRead(pin_number);
      push_on_gpio_next_push_millis = _millis;
      push_on_gpio_backoff_window_millis = 1000*MAX(push_on_gpio_backoff_seconds, push_on_gpio_minimum_backoff);
      Serial.println("enable_push_on_gpio: enabled, pin " + String(push_on_gpio_pin_number) + String(edge_polarity ? " rising" : " falling") + 
                     " edge, " + String(push_on_gpio_backoff_seconds) + " seconds backoff");
    }
    push_on_gpio_backoff_flag = false;
    push_on_gpio_backoff_wraparound_flag = false;
    return push_on_gpio_backoff_window_millis;
  } // uint32_t enable_push_on_gpio(uint8_t pin_number, bool edge_polarity, uint16_t backoff_seconds)

  void update_push_on_gpio_next_push(unsigned long _millis)
  {
    // this method keeps 'xxxx' BEHIND _millis when _millis goes through wrap-around (each 50 days or so)
    // without this, we are at risk of being blocked from sending push notification (on GPIO) for many days.

    // logic here is as follows:
    // if millis, is smaller than the backoff window size, and the next push marker is HIGHER than the maximal possible
    // window size, than we know, for sure, that the wrap-around millis isn't within a backoff window, and we can safely
    // reset the next push marker to zero millis

    if((_millis < push_on_gpio_backoff_window_millis) && (push_on_gpio_next_push_millis > 0x10000*1000))
      push_on_gpio_next_push_millis = 0;   
  }

  void check_gpio(unsigned long _millis)
  {
    if(push_on_gpio_enabled)
    {
      int read_value = digitalRead(push_on_gpio_pin_number);
      bool rising = (read_value == HIGH) && (push_on_gpio_previous_read_value == LOW);
      bool falling = (read_value == LOW) && (push_on_gpio_previous_read_value == HIGH);
      if((rising && push_on_gpio_true_rising_false_falling) || (falling && !push_on_gpio_true_rising_false_falling))
      {
        gpio_event_handler(_millis);
      } // send rising/falling edge push notification
      else
      {
        update_push_on_gpio_next_push(_millis);
      }
      push_on_gpio_previous_read_value = read_value;
    }

    // clear wrap-around flag if required
    if(push_on_gpio_backoff_wraparound_flag && (_millis <= push_on_gpio_backoff_window_millis))
      push_on_gpio_backoff_wraparound_flag = false;

    if(push_on_gpio_backoff_flag && !push_on_gpio_backoff_wraparound_flag)
    {
      if(_millis >= push_on_gpio_next_push_millis)
      {
        push_on_gpio_backoff_flag = false;
        Serial.println("push_on_gpio: backoff period (" + String(push_on_gpio_backoff_window_millis) + " milliseconds) ended");
        Serial.println("push_on_gpio: " + String(push_on_gpio_supressed_events) + " events suppressed");
        led_off();
        print_gpio_event_list();
        purge_gpio_event_log();
      }
    } // backoff end logic
  } // void check_gpio(unsigned long _millis)

  void gpio_event_handler(unsigned long _millis)
  {
    // supress notification during backoff window.
    // how do we protect agains wraparound error?
    // 
    // if((_millis > push_on_gpio_next_push_millis) && !push_on_gpio_backoff_wraparound_flag)
    if(!push_on_gpio_backoff_flag)
    {
      // we are out of the backoff window, HTTP API request should be issued
      Serial.println("GPIO edge push notification " + String(_millis - push_on_gpio_next_push_millis));
      unsigned long next_next = _millis + push_on_gpio_backoff_window_millis;
      // if(next_next < push_on_gpio_backoff_window_millis)
      if(next_next < _millis)
        push_on_gpio_backoff_wraparound_flag = true;
      push_on_gpio_next_push_millis = next_next;
      push_on_gpio_supressed_events = 0;
      push_on_gpio_backoff_flag = true;
      led_on(true);
      write_to_gpio_event_log(_millis);
    }
    else
    {
      // we are still within the backoff window: apply debunce and notification suppression logic.
      // - debounce: any adjascent events less than one second apart from each other, are considered mechanical jitter, and therefore
      //             ignored.
      // - suppression: any event passing the debounce logic within the backoff window is simply registered by means of suppressed
      //                events counter. push notification, however, is not being sent out.

      Serial.println("GPIO edge ignored (backoff ends in " + String(push_on_gpio_next_push_millis-_millis) + " milliseconds)");
      if(_millis > push_on_gpio_last_edge_detect_millis + 1000) // simple debounce logic - ignore edges less than one second apart
      {
        push_on_gpio_supressed_events++;
        write_to_gpio_event_log(_millis);
        Serial.println("GPIO Event: supressed (" + String(push_on_gpio_supressed_events) + ") so far");
      }
      else
      {
        Serial.println("GPIO Event: filtered out by debounce logic");
      }            
    }
    push_on_gpio_last_edge_detect_millis = _millis;
  } // void gpio_event_handler(unsigned long _millis)

  void erase_credentials(void)
  {
    pushover_api_cerdentials_ready = false;
    strcpy(pushover_api_token, "");
    strcpy(pushover_api_user, "");
    initialize_push_on_gpio();
    purge_gpio_event_log();
  } // void erase_credentials(void)

} // namespace my_PN

uint32_t pn_get_request_test(void){ return my_PN::get_request_test(); }
uint32_t pn_openweather_get_request(void){ return my_PN::openweather_get_request(); }
void pn_parse_json_test(void){ my_PN::parse_json_test(); }
uint32_t pn_test_post_request(void){ return my_PN::test_post_request(); }
uint32_t pn_push_notification(void){ return my_PN::push_notification(); }
uint32_t pn_set_pushover_credentials(void){ return my_PN::set_pushover_credentials(); }
uint16_t pn_set_push_notification_text(uint8_t string_var_index){ return my_PN::set_push_notification_text(string_var_index); }
void pn_erase_credentials(void){ my_PN::erase_credentials(); }

void pn_check_gpio(unsigned long _millis){ my_PN::check_gpio(_millis); }
uint32_t pn_gpio_enable(uint8_t pin_number, bool edge_polarity, uint16_t backoff_seconds)
{ 
  return my_PN::enable_push_on_gpio(pin_number, edge_polarity, backoff_seconds); 
}
