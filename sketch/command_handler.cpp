#include "esp32-hal-gpio.h"
#include "string.h"
#include "sys/_types.h"
#include <sys/stat.h>
#include "stdint.h"
#include <WiFi.h>

#include "shared_vars.h"
extern char string_var[][STRING_VARS_SIZE];
extern unsigned long count;
extern String  wired_lan_mac_address;
extern bool    wired_lan_connection;
extern uint8_t wifi_reconnect_count;
extern bool    display_ok;

extern uint8_t led_pin_number;
extern bool    led_pin_enabled;
extern bool    led_polarity;

// extern
void debug_error(String);
void reset_loop_counter(void); // external function prototype
uint32_t pn_get_request_test(void);
uint32_t pn_openweather_get_request(void);
void pn_parse_json_test(void);
uint32_t pn_test_post_request(void);
uint32_t pn_push_notification(void);
uint32_t pn_set_pushover_credentials(void);
uint16_t pn_set_push_notification_text(uint8_t string_var_index);
uint32_t pn_gpio_enable(uint8_t pin_number, bool edge_polarity, uint16_t backoff_seconds);
void oled_text(String, String);
void led_on(bool turn_it_on);

namespace my_CH
{
  const int VERSION1 = 1;
  const int OK_STATUS = 0;
  const int NOTOK_STATUS = 1;
  const int BAD_COMMAND_STATUS = 2;

  const int V1_CMD_NOP                        = 0;
  const int V1_CMD_RESET_LOOP_COUNTER         = 1;
  const int V1_CMD_READ_UINT16                = 2;
  const int V1_CMD_GET_RANDOM_32BIT           = 3;
  const int V1_CMD_TEST_HTTP_GET              = 4;
  const int V1_CMD_WEATHER_REQUEST            = 5;
  const int V1_CMD_QUICK_BROWN_FOX            = 6;
  const int V1_CMD_READ_STRING_VAR            = 7;
  const int V1_CMD_WRITE_STRING_VAR           = 8;
  const int V1_CMD_PARSE_JSON_TEST            = 9;
  const int V1_CMD_POST_REQUEST_TEST          = 10;
  const int V1_CMD_PUSH_NOTIFICATION          = 11;
  const int V1_CMD_GET_MAC_ADDRESS            = 12;
  const int V1_CMD_SET_PUSHOVER_CREDENTIALS   = 13;
  const int V1_CMD_READ_UINT32                = 14;
  const int V1_CMD_SET_PUSH_NOTIFICATION_TEXT = 15;
  const int V1_CMD_OLED_TEXT                  = 16;
  const int V1_CMD_SET_LED_PIN                = 17;
  const int V1_CMD_READ_OVER_STRING_VAR       = 18;
  const int V1_CMD_CONFIG_PUSHOVER_GPIO       = 19;
  const int V1_CMD_RESERVED                   = 32;  // for extended commands. if first octet is 0x20, additional three octets build the opcode, making for 16777216 (2^24) additional opcodes

  const int V1_UINT16_VARIABLE_ZERO_CONST           = 0;
  const int V1_UINT16_VARIABLE_LOOP_COUNTER         = 1;
  const int V1_UINT16_VARIABLE_STR_VAR_ATTRIBUTES   = 2;
  const int V1_UINT16_VARIABLE_SKETCH_DATE_CODE     = 3;
  const int V1_UINT16_VARIABLE_MAX_RESP_SIZE        = 4;
  const int V1_UINT16_VARIABLE_WIFI_RSSI            = 5;
  const int V1_UINT16_VARIABLE_WIFI_RECONNECT_COUNT = 6;
  const int V1_UINT16_VARIABLE_GPIO_INPUT_VALUE     = 7;
  // const int V1_UINT16_VARIABLE_WIFI_BSSID           = 7; // need to remove this!! replaced by 'V1_CMD_READ_BSSID_AS_STRING'

  const int V1_READ_OVER_STRVAR_QBF                 = 0x0000;
  const int V1_READ_OVER_STRVAR_WIFIBSSID           = 0x0001;

  const int V1_UINT32_VARIABLE_ZERO_CONST         = 0;
  const int V1_UINT32_VARIABLE_SKETCH_DATE_CODE   = 1;

  char* cmd_request_buffer;
  int   cmd_request_length;

  const int MAX_RESPONSE_SIZE = 128;
  uint8_t response_buffer[MAX_RESPONSE_SIZE];
  unsigned response_length;

  const uint8_t V1_RESPONSE_STATUS_OK = 0;

  void quick_brown_fox_response(void);
  uint8_t build_v1_read_string_variable_response(uint8_t);
  bool perform_read_to_string_variable(uint8_t, uint16_t);

  uint8_t variable_index;
  uint16_t ros_index;             // read over string
  String mac_address_string;
  uint32_t rv32bit;

  void build_v1_response_uint32_t_payload(uint8_t status, uint32_t payload)
  {
    response_buffer[0] = VERSION1;
    response_buffer[1] = status;
    response_buffer[2] = (payload >> 0) & 0xFF;
    response_buffer[3] = (payload >> 8) & 0xFF;
    response_buffer[4] = (payload >> 16) & 0xFF;
    response_buffer[5] = (payload >> 24) & 0xFF;
    response_length = 6;
  } // void build_v1_response_uint32_t_payload


  void build_v1_response_uint16_t_payload(uint8_t status, uint16_t payload)
  {
    response_buffer[0] = 1;
    response_buffer[1] = status;
    response_buffer[2] = (payload >> 0) & 0xFF;
    response_buffer[3] = (payload >> 8) & 0xFF;
    response_length = 4;
  } // void build_v1_response_uint16_t_payload

  void read_v1_16bit_variable(int variable_id)
  {
    Serial.println("Read 16 bit variable "+String(variable_id));
    uint8_t* bssid_ptr;
    uint8_t  arg1;
    int      intvar1;

    switch(variable_id)
    {
      case V1_UINT16_VARIABLE_ZERO_CONST:
        build_v1_response_uint16_t_payload(OK_STATUS, 0);
        break;

      case V1_UINT16_VARIABLE_LOOP_COUNTER:
        build_v1_response_uint16_t_payload(OK_STATUS, count);
        break;

      case V1_UINT16_VARIABLE_STR_VAR_ATTRIBUTES:
        build_v1_response_uint16_t_payload(OK_STATUS, STRING_VARS_COUNT | (STRING_VARS_SIZE << 8));
        break;

      case V1_UINT16_VARIABLE_SKETCH_DATE_CODE:
        build_v1_response_uint16_t_payload(OK_STATUS, SKETCH_16BIT_DATE_CODE);
        break;

      case V1_UINT16_VARIABLE_MAX_RESP_SIZE:
        build_v1_response_uint16_t_payload(OK_STATUS, MAX_RESPONSE_SIZE);
        break;

      case V1_UINT16_VARIABLE_WIFI_RSSI:
        if(!wired_lan_connection)
          build_v1_response_uint16_t_payload(OK_STATUS, 256+WiFi.RSSI());
        else
          build_v1_response_uint16_t_payload(NOTOK_STATUS, 0xdead);
        break;

      case V1_UINT16_VARIABLE_WIFI_RECONNECT_COUNT:
        build_v1_response_uint16_t_payload(wired_lan_connection ? NOTOK_STATUS : OK_STATUS, wifi_reconnect_count);
        break;

      case V1_UINT16_VARIABLE_GPIO_INPUT_VALUE:
        if(cmd_request_length != 9)
        {
          Serial.println("[NOTOK] 'V1_UINT16_VARIABLE_GPIO_INPUT_VALUE' bad request length (expected 9 octets, received " + String(cmd_request_buffer) + ")");
          build_v1_response_uint16_t_payload(NOTOK_STATUS, 0xDEAD);
        }
        else
        {
          arg1 = cmd_request_buffer[3];            // GPIO pin number
          if((arg1 & 0x80) == 0x00)
          {
            pinMode(arg1, INPUT);
          }
          else
          {
            Serial.println("Note: reading pin " + String(arg1 & 0x7f) + " without setting pinMode(INPUT)");
          }
          intvar1 = digitalRead(arg1 & 0x7f);
          build_v1_response_uint16_t_payload(OK_STATUS, intvar1);
        }
        break;

      // case V1_UINT16_VARIABLE_WIFI_BSSID:
      //   build_v1_response_uint16_t_payload(NOTOK_STATUS, 0xDEAD);
      //   Serial.print("WiFi.BSSIDstr() : " + WiFi.BSSIDstr() + ", or using WiFi.BSSID() :");
      //   bssid_ptr = WiFi.BSSID();
      //   for(int i=0; i<6; i++)
      //     Serial.print(" " + String(bssid_ptr[i]));
      //   Serial.println("");
      //   break;

      default:
        build_v1_response_uint16_t_payload(NOTOK_STATUS, 0);
        break;      
    } // switch( variable_id )
  } // void read_v1_16bit_variable(int variable_id)

  void read_v1_32bit_variable(int variable_id)
  {
    Serial.println("Read 32 bit variable " + String(variable_id));
    uint32_t payload_32bit;
    switch( variable_id )
    {
      case V1_UINT32_VARIABLE_ZERO_CONST:
        build_v1_response_uint32_t_payload(OK_STATUS, 0);
        break;

      case V1_UINT32_VARIABLE_SKETCH_DATE_CODE:
        // bits[31:16] - year
        // bits [15:8] - month
        //  bits [7:0] - day
        // new data code: [31:20][19:16][15:10][9:0] for {year, month, day, intraday_rev}, respectively
        //paylod_32bit = (SKETCH_YEAR_MONTH_DAY[0] << 16) | (SKETCH_YEAR_MONTH_DAY[1] << 8) | SKETCH_YEAR_MONTH_DAY[2];
        payload_32bit = (SKETCH_YEAR_MONTH_DAY[0] << 20) | (SKETCH_YEAR_MONTH_DAY[1] << 16)
                      | (SKETCH_YEAR_MONTH_DAY[2] << 10) | (SKETCH_INTRADAY_REV & 0x3FF);
        Serial.println("32bit date code : 0x" + String(payload_32bit, HEX));
        build_v1_response_uint32_t_payload(OK_STATUS, payload_32bit);
        break;

      default:
        Serial.println("[NOTOK] 32bit variable id 0x" + String(variable_id, HEX) + " is not defined");
        build_v1_response_uint32_t_payload(BAD_COMMAND_STATUS, 0);
        break;
    } // switch statement
  } // void read_v1_32bit_variable(int variable_id)

  unsigned handle_char_buffer_command_message(char * buffer, int length)
  {
    cmd_request_buffer = buffer;
    cmd_request_length = length;
    Serial.println("myCH::handle_char_buffer_command_message");
    response_length = 0;
    if(length >= 2)
      {
      if(int(buffer[0] == VERSION1))
      {
        int v1_command = int(buffer[1]);
        int variable_id;
        long r = 0;
        uint32_t http_result;
        uint16_t gpvar16bit;
        uint8_t line1_index;
        uint8_t line2_index;
        bool    bool_arg_1;
        Serial.println("myCH::handle_char_buffer_command_message -> V1 command " + String(v1_command));
        switch(v1_command)
        {
          case V1_CMD_NOP:
            break;

          case V1_CMD_RESET_LOOP_COUNTER:
            Serial.println("resetting loop counter");
            reset_loop_counter();
            break;

          case V1_CMD_READ_UINT16:
            Serial.println("'V1_CMD_READ_UINT16'");
            // expecting [version, opcode, register_id] + [seq.no] + [crc32]
            if(length >= 3+5)
            {
              variable_id = int(buffer[2]);
              read_v1_16bit_variable(variable_id);
            }
            else
            {
              Serial.println("*E 'V1_CMD_READ_UINT16' - length (" + String(length) + ") is less than 3+5");
            }
            break;

          case V1_CMD_GET_RANDOM_32BIT:
            r = random(0x10000);
            Serial.println("'V1_CMD_GET_RANDOM_32BIT': random value: " + String(r));
            build_v1_response_uint32_t_payload(V1_RESPONSE_STATUS_OK, uint32_t(r));
            break;
          
          case V1_CMD_TEST_HTTP_GET:
            Serial.println("'V1_CMD_TEST_HTTP_GET'");
            http_result = pn_get_request_test();
            build_v1_response_uint32_t_payload(V1_RESPONSE_STATUS_OK, http_result);
            break;
          
          case V1_CMD_WEATHER_REQUEST:
            Serial.println("'V1_CMD_WEATHER_REQUEST'");
            http_result = pn_openweather_get_request();
            build_v1_response_uint32_t_payload(V1_RESPONSE_STATUS_OK, http_result);
            break;

          case V1_CMD_QUICK_BROWN_FOX:
            Serial.println("'V1_CMD_QUICK_BROWN_FOX'");
            quick_brown_fox_response();
            break;

          case V1_CMD_READ_STRING_VAR:
            Serial.println("'V1_CMD_READ_STRING_VAR'");
            if(length == 3+5)
            {
              variable_index = buffer[2];
              build_v1_read_string_variable_response(variable_index);
            }
            else
            {
              Serial.println("*E: bad length");
              build_v1_response_uint32_t_payload(NOTOK_STATUS, 0xDEADBEEF);
            }
            break;

          /*---------------------------------------------------------------------------------------------
            Write String Variable command format:
            - version (one octet)
            - opcode (one octet)
            - variable index (one octet)
            - string characters (multiple octets)
            - CRC32 (four octets)
            ---------------------------------------------------------------------------------------------*/
          case V1_CMD_WRITE_STRING_VAR:
            Serial.println("'V1_CMD_WRITE_STRING_VAR' (length: " + String(length) + ")");
            // uint8_t variable_index;
            variable_index = buffer[2];
            
            if(variable_index < STRING_VARS_COUNT)
            {              
              uint8_t text_length = length-8;
              if(text_length < STRING_VARS_SIZE-1)
              {
                Serial.println("[OK] variable index = " +  String(variable_index) + ", text length: " + String(text_length));
                build_v1_response_uint32_t_payload(OK_STATUS, length);
                char * source_pointer = &(buffer[3]);
                for(uint8_t i=0; i<text_length; i++)
                  string_var[variable_index][i] = *(source_pointer++);
                string_var[variable_index][text_length] = NULL;
              }
              else
              {
                Serial.println("[NOTOK] text is too long (" + String(text_length) + ") chars");
                build_v1_response_uint32_t_payload(NOTOK_STATUS, 0xdeadbeef);
              }
            }
            else
            {
              Serial.println("[NOTOK] variable index = " + String(variable_index));
              build_v1_response_uint32_t_payload(NOTOK_STATUS, 0xdeadbeef);
            }
            break;

          case V1_CMD_PARSE_JSON_TEST:
            Serial.println("'V1_CMD_PARSE_JSON_TEST'");
            pn_parse_json_test();
            build_v1_response_uint32_t_payload(OK_STATUS, 0);
            break;

          case V1_CMD_POST_REQUEST_TEST:
            Serial.println("'V1_CMD_POST_REQUEST_TEST'");
            http_result = pn_test_post_request();
            build_v1_response_uint32_t_payload(V1_RESPONSE_STATUS_OK, http_result);
            break;

          case V1_CMD_PUSH_NOTIFICATION:
            Serial.println("'V1_CMD_PUSH_NOTIFICATION'");
            rv32bit = pn_push_notification(); // HTTP response character count, DEADBEEF for error
            build_v1_response_uint32_t_payload(rv32bit == 0xDEADBEEF ? NOTOK_STATUS : OK_STATUS, rv32bit);
            break;

          case V1_CMD_GET_MAC_ADDRESS:
            Serial.println("'V1_CMD_GET_MAC_ADDRESS'");
            // if(wired_lan_mac_address.length() > 1)
            if(wired_lan_connection)
              mac_address_string = wired_lan_mac_address + ",802.3";
            else
              mac_address_string = WiFi.macAddress() + ",802.11";
            Serial.println("MAC address: " + mac_address_string);
            if(mac_address_string.length() < STRING_VARS_SIZE)
            {
              strcpy(string_var[0], mac_address_string.c_str());
              build_v1_read_string_variable_response(0);
            }
            else
            {
              Serial.println("[NOTOK] MAC address string too long");
              build_v1_response_uint32_t_payload(NOTOK_STATUS, 0xdeadbeef);
            }
            break;

          case V1_CMD_SET_PUSHOVER_CREDENTIALS:
            Serial.println("'V1_CMD_SET_PUSHOVER_CREDENTIALS'");
            rv32bit = pn_set_pushover_credentials();
            build_v1_response_uint32_t_payload(rv32bit==OK_STATUS_32b ? OK_STATUS : NOTOK_STATUS, rv32bit);
            break;

          case V1_CMD_READ_UINT32:
            variable_id = int(buffer[2]);
            Serial.println("'V1_CMD_READ_UINT32' [0x" + String(variable_id, HEX) + "]");
            read_v1_32bit_variable(variable_id);
            break;

          case V1_CMD_SET_PUSH_NOTIFICATION_TEXT:
            // command copies specified string variable  to push notification test buffer
            variable_id = int(buffer[2]);
            Serial.println("'V1_CMD_SET_PUSH_NOTIFICATION_TEXT' (string var " + String(variable_id) + ")");
            // Serial.println("[NOTOK] command logic not implemented");
            gpvar16bit = pn_set_push_notification_text(variable_id);
            build_v1_response_uint16_t_payload(gpvar16bit>0 ? OK_STATUS : NOTOK_STATUS, gpvar16bit);
            if(gpvar16bit == 0) debug_error("failed to set push notification text");
            break;

          case V1_CMD_OLED_TEXT:
            // two octets following the opcode, determine string variables to be used for text lines 1 and 2
            line1_index = (uint8_t)buffer[2];
            line2_index = (uint8_t)buffer[3];
            Serial.println("'V1_CMD_OLED_TEXT(" + String(line1_index) + ", " + String(line2_index) + ")'");
            if(!display_ok)
            {
              debug_error("display device error");
              build_v1_response_uint16_t_payload(NOTOK_STATUS, 0);
            }
            else if(max(line1_index, line2_index) >= STRING_VARS_COUNT)
            {
              debug_error("string indices out of bounds");
              build_v1_response_uint16_t_payload(NOTOK_STATUS, 1);
            }              
            else if(max(strlen(string_var[line1_index]), strlen(string_var[line2_index])) < MIN(20, STRING_VARS_SIZE))
            {
              oled_text(String(string_var[line1_index]), String(string_var[line2_index]));
              build_v1_response_uint16_t_payload(OK_STATUS, strlen(string_var[line1_index]) | (strlen(string_var[line2_index]) << 8));
            }
            else
            {
              debug_error("string variables too long");
              build_v1_response_uint16_t_payload(NOTOK_STATUS, 2);
            }
            break;

          case V1_CMD_SET_LED_PIN:
            // command takes one octet as argument. bits [5:0] are pin number (0 to disable), bit [7] is
            // the GPIO digitalWrite value, which is written right after pinMode is set to OUTPUT.
            // bit [6] is LED polarity '1' (true) for HIGH=LED on, '0' for LOW=LED on.
            line1_index = (uint8_t)buffer[2];
            Serial.println("'V1_CMD_SET_LED_PIN' " + String(line1_index & 0x3F) + ", " + String(line1_index >> 7) + ", " + String((line1_index >> 6) & 0x01));
            if(led_pin_enabled && ((line1_index & 0x3F) != led_pin_number))
            {
              pinMode(led_pin_number, INPUT);
            }
            led_pin_number = line1_index & 0x3F;
            led_polarity = (line1_index & 0x40) == 0x40 ? true : false;
            if(led_pin_number == 0)
              led_pin_enabled = false;
            else
            {
              led_pin_enabled = true;
              pinMode(led_pin_number, OUTPUT);
              Serial.println("Turning LED " + String(((uint8_t)buffer[2] & 0x80) == 0x80 ? "ON" : "OFF") + " (GPIO " + String(led_pin_number) + ")");
              led_on( ((uint8_t)buffer[2] & 0x80) == 0x80 );
            }
            build_v1_response_uint16_t_payload(OK_STATUS, line1_index);
            break;

          case V1_CMD_READ_OVER_STRING_VAR:
            // this opcode copies BSSID string to a string variable selected by first octet argument.
            // response packet is a stadard 'read string variable' response xxxx
            variable_index = (uint8_t)buffer[2];
            ros_index = (uint8_t)buffer[3] + ((uint8_t)buffer[4] << 8);
            if(perform_read_to_string_variable(variable_index, ros_index))
            {
              build_v1_read_string_variable_response(variable_index);
            }
            else
            {
              build_v1_response_uint32_t_payload(NOTOK_STATUS, 0xdeadbeef);
            }
            // if(variable_index < STRING_VARS_COUNT)
            // {
            //   Serial.println("'V1_CMD_READ_OVER_STRING_VAR' (via string variable " + String(variable_index) + ")");
            //   // strcpy(string_var[variable_index], WiFi.BSSIDstr().toCharArray(char *buf, unsigned int bufsize));
            //   // WiFi.BSSIDstr().toCharArray(string_var[variable_index], STRING_VARS_SIZE);
            //   build_v1_read_string_variable_response(variable_index);
            // }
            // else
            // {
            //   Serial.println("[NOTOK] either variable index is too high (" + String(variable_index) + "), or we are on wired LAN");
            //   build_v1_response_uint32_t_payload(NOTOK_STATUS, 0xdeadbeef);
            // }
            break;

          case V1_CMD_CONFIG_PUSHOVER_GPIO:
            // 2023/04/12 - defined, not implemented
            // arguments:
            //   first octet: [6:0] - pin number, [7] - polarity
            //   second and third octets: beckoff seconds (0 to 2^16)
            // response payload (uint32_t):
            //   backoff window size, in milliseconds
            line1_index = (uint8_t)buffer[2] & 0x7F;                              // pin number
            bool_arg_1 = (uint8_t)buffer[2] & 0x08 == 0x08 ? false : true;        // polarity
            gpvar16bit = (uint8_t)buffer[3] + ((uint8_t)buffer[4] << 8);          // backoff window (seconds)
            Serial.println("'V1_CMD_CONFIG_PUSHOVER_GPIO' " + 
                           String(line1_index) + ", " + String(bool_arg_1) + ", " + 
                           String(gpvar16bit) + " seconds backoff");
            // pn_gpio_enable( line1_index,
            //                 bool_arg_1,
            //                 gpvar16bit );
            build_v1_response_uint32_t_payload(OK_STATUS, pn_gpio_enable(line1_index, bool_arg_1, gpvar16bit ));
            break;

          default:
            Serial.println("[NOTOK] no command handler for opcode " + String(v1_command));
            build_v1_response_uint32_t_payload(NOTOK_STATUS, 0xDEADBEEF);
            break;
        
        } // switch(v1_command)
      }
      else
      {
        Serial.println("myCH::handle_char_buffer_command_message -> *** NOT a V1 command ***");
      }
    }
    else // too short
    {
      Serial.println("myCH::handle_char_buffer_command_message -> length < 2");
    }
    return response_length;
  } // unsigned handle_char_buffer_command_message

  void quick_brown_fox_response(void)
  {
    const char qbf_string[] = "the quick brown fox\0";
    response_buffer[0] = VERSION1;
    response_buffer[1] = OK_STATUS;
    strcpy((char *)(&response_buffer[2]), qbf_string);
    response_length = 2 + strlen(qbf_string);
  } // void quick_brown_fox_response(void)

  uint8_t build_v1_read_string_variable_response(uint8_t variable_index)
  {
    char * source_pointer;
    if(variable_index < STRING_VARS_COUNT)
    {
      source_pointer = string_var[variable_index];
      Serial.println("building response for read string variable(" + String(variable_index) + ")");
      response_buffer[0] = VERSION1;
      response_buffer[1] = OK_STATUS;
      strcpy((char *)(&response_buffer[2]), source_pointer);
      // strcpy((char *)(&response_buffer[2]), "abcdefg\0"); // xxxx
      response_length = 2 + strlen(source_pointer);
      // response_length = 2 + strlen("abcdefg\0");
      Serial.println("len(string variable[" + String(variable_index) + "]) = " + String(strlen(source_pointer)));
      return 0;
    }
    else
    {
      build_v1_response_uint32_t_payload(NOTOK_STATUS, 0xDEADBEEF);
      return 1;
    }
  } // uint8_t build_v1_read_string_variable_response(uint8_t variable_index)

  bool perform_read_to_string_variable(uint8_t var_index, uint16_t switch_index)
  {
    if(var_index >= STRING_VARS_COUNT)
    {
      Serial.println("[NOTOK] string variable index (" + String(var_index) + ") out of bounds");
      return false;
    }

    bool success = true;

    switch(switch_index)
    {
      case V1_READ_OVER_STRVAR_QBF:
        Serial.println("reading 'the quick brown fox' via string variable " + String(var_index));
        strcpy(string_var[var_index], "the quick brown fox");
        break;

      case V1_READ_OVER_STRVAR_WIFIBSSID:
        if(wired_lan_connection)
        {
          success = false;
        }
        else
        {
          WiFi.BSSIDstr().toCharArray(string_var[var_index], STRING_VARS_SIZE);
          Serial.println("writing BSSID string '" + String(string_var[var_index]) + "' to string variable " + String(var_index));
        }
        break;
        
      default:
        success = false;
        break;
    }
    return success;
  } // bool perform_read_to_string_variable(uint8_t var_index, uint16_t switch_index)

} // namespace myCH

unsigned ch_handle_char_buffer_command_message(char * buffer, int length){ return my_CH::handle_char_buffer_command_message(buffer, length); }
uint8_t * ch_response_buffer(void){ return my_CH::response_buffer; }
unsigned ch_response_length(void){ return my_CH::response_length; }
// void ch_reset_response_length(void){ my_CH::response_length=0; }
