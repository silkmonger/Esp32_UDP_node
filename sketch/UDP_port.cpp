#include "WString.h"
#include "stdint.h"
#include "IPAddress.h"
#include "esp32-hal.h"
#include <WiFiUdp.h>
#include <CRC32.h>
// #include <AsyncUDP.h>

String ip_address_string(IPAddress ip);
void oled_text(String first_line, String second_line);
// void ch_handle_command_buffer(char * buffer, int length);
unsigned ch_handle_char_buffer_command_message(char * buffer, int length);
uint8_t * ch_response_buffer(void);
unsigned ch_response_length(void);
// void ch_reset_response_length(void);
void led_on(bool);

namespace my_UDP
{
  WiFiUDP udp;
  // AsyncUDP upd;      - this does NOT work for current code !!! (2023/04/05)
  unsigned int port_number = 3333;
  const int PACKET_BUFFER_SIZE = 64;
  char packet_buffer[PACKET_BUFFER_SIZE];
  bool enabled = false;
  bool remote_machine_valid = false;
  IPAddress remote_ip_address;
  uint16_t remote_port_number;
  const uint8_t first_char_command_threshold = 10;
  int udp_read_length;
  uint32_t good_CRC_received_frames = 0;
  uint32_t bad_CRC_received_frames = 0;
  uint8_t receive_sequence_number;
  bool read_crc_ok;
  bool udp_write_error_flag;

  // internal function prototypes
  void send_to_remote_machine(String text_to_send);
  uint32_t string_crc32(String s);
  uint32_t bytes_crc32(char* c, int length);
  bool incoming_packet_check_crc32(void);
  void serial_print_message_content(bool command_message);

  void enable_port(void)
  {
    udp.begin(port_number);
    Serial.println("starting UDP port " + String(port_number));
    enabled = true;
  }

  void restart(void)
  {
    if(enabled)
    {
      udp.stop();
      udp.begin(port_number);
      Serial.println("restarting UDP port " + String(port_number));
    }
    else
    {
      Serial.println("[NOTOK] UDP not enabled, cannot restart");
    }
  } // void restart(void)

  void send_command_handler_response(void)
  {
    uint8_t length = ch_response_length();
    uint8_t * rp = ch_response_buffer();
    int offset = length;
    
    rp[offset] = receive_sequence_number;
    uint32_t crc = bytes_crc32((char *)ch_response_buffer(), length+1);
    rp[offset+1] = (crc >> 0) & 0xff;
    rp[offset+2] = (crc >> 8) & 0xff;
    rp[offset+3] = (crc >> 16) & 0xff;
    rp[offset+4] = (crc >> 24) & 0xff;
    if(remote_machine_valid)
    {
      udp_write_error_flag |= udp.beginPacket(remote_ip_address, remote_port_number) != 1;;
      udp.write(ch_response_buffer(), length+5);      
      udp_write_error_flag |= udp.endPacket() != 1;
      Serial.println("  send_command_handler_response(): sending " + String(length+5) + " octets (binary) response");
    }
    else
    {
      Serial.println("  send_command_handler_response(): would send " + String(length+5) + " octets (binary) response");
      Serial.println("  [NOTOK] remote machine not valid");
    }
  } // void send_command_handler_response(void)

  void receive_and_print_to_serial(bool acknowledge=false)
  {
    // ch_reset_response_length();
    unsigned command_response_length = 0;
    if(enabled)
    {
      udp.parsePacket();
      udp_read_length = udp.read(packet_buffer, PACKET_BUFFER_SIZE);
      if( udp_read_length > 1 )
      {
        read_crc_ok = incoming_packet_check_crc32();
        // oled_text("UDP transfer", "");
        // trim newline - carriage return characters
        int pointer = udp_read_length-1;
        int valid_byte_count = udp_read_length;
        bool command_received = false;
        if(int(packet_buffer[0]) >= first_char_command_threshold)
          while( (pointer > 1) && ( (packet_buffer[pointer] == '\r') or (packet_buffer[pointer] == '\n') ) )
          {
            packet_buffer[pointer--] = NULL;
            valid_byte_count--;
          }
        else
        {
          Serial.println("CMD " + String(int(packet_buffer[0])) + ", valid_byte_count: " + String(valid_byte_count));
        }
        
        if((valid_byte_count >= 2) and (int(packet_buffer[0]) > 0) and (int(packet_buffer[0]) < first_char_command_threshold))
        {
          command_received = true;
          if(read_crc_ok)
          {
            String line2 = "ver:" + String(int(packet_buffer[0])) + ", cmd:" + String(int(packet_buffer[1]));
            // oled_text("command", line2);
            command_response_length = ch_handle_char_buffer_command_message(packet_buffer, valid_byte_count);
          }
          else
          {
            Serial.println("  CMD ignored: inbound CRC ERROR");
          }
        } // if(valid_byte_count > 2)
        Serial.println("\nUDP incoming " + String(udp_read_length) + " characters long UDP packet received");
        // Serial.println("  message content: '" + String(packet_buffer) + "'" + (command_received ? " (CMD)" : ""));
        serial_print_message_content(command_received);
        remote_ip_address = udp.remoteIP();
        remote_port_number = udp.remotePort();
        remote_machine_valid = true;
        Serial.println("  remote machine: " + ip_address_string(remote_ip_address) + ":" + String(remote_port_number));
        if(acknowledge)
        {
          // if(ch_response_length() == 0){
          if(command_response_length == 0){
            // uint32_t crc = command_received ? bytes_crc32(packet_buffer, valid_byte_count) : string_crc32(String(packet_buffer));
            uint32_t crc = bytes_crc32(packet_buffer, valid_byte_count);  // 2021-01-24 attempt [bug20230124a] fix for string response CRC
            Serial.println("  CRC32 = 0x" + String(crc, HEX));
            // send_to_remote_machine(String(length) + " chars [CRC: 0x" + String(string_crc32(String(packet_buffer)), HEX) + "]");
            String ack_message = String(udp_read_length) + " chars [CRC: 0x" + String(crc, HEX) + "] (G:"
                              + String(good_CRC_received_frames) + ", B:" + String(bad_CRC_received_frames) + ")";
            send_to_remote_machine(ack_message);
            Serial.println("ack_message: " + ack_message);
          }
          else
          {
            udp_write_error_flag = false;
            send_command_handler_response();
            Serial.println("  command handler has built a " + String(ch_response_length()) + " bit binary response");
            if(udp_write_error_flag)
            {
              led_on(true);
            }
            // Serial.println("  Command handler want to tell us they have a " + String(ch_response_length()) + " bits response built for this command");
            // Serial.println("  - future implementation");
          }
        }
      }
    }
    else
    {
      Serial.println("*E: UDP has not been enabled, can't receive a thing...");
    }
  } // void receive_and_print_to_serial(void)

  uint32_t string_crc32(String s)
  {
    CRC32 calc;

    for(int i=0; i<s.length(); i++)
    {
      calc.update(s[i]);
    }
    return calc.finalize();
  } // uint32_t string_crc32(String s)

  uint32_t bytes_crc32(char* c, int length)
  {
    CRC32 calc;
    char* d=c;
    String update_bytes = "";

    for(int i=0; i<length; i++)
    {
      calc.update(*d);
      update_bytes += String(int(*d)) + ",";
      d++;
    }
    Serial.print("calculated bytes CRC32 for " + update_bytes);
    return calc.finalize();
  } // uint32_t bytes_crc32(char* c, int length)

  void send_to_remote_machine(String text_to_send)
  {
    if(remote_machine_valid)
    {
      udp.beginPacket(remote_ip_address, remote_port_number);
      udp.println(text_to_send);
      udp.endPacket();
    }
    else
    {
      Serial.println("[NOTOK] remote machine information is missing");
    }
  }

  bool incoming_packet_check_crc32(void)
  {
    char hex_string[9];
    if(udp_read_length > 4)
    {
      uint32_t calculated_crc = bytes_crc32(packet_buffer, udp_read_length-4);
      uint32_t received_crc = uint8_t(packet_buffer[udp_read_length-1]) + (uint8_t(packet_buffer[udp_read_length-2]) << 8)
                            + (uint8_t(packet_buffer[udp_read_length-3]) << 16) + (uint8_t(packet_buffer[udp_read_length-4]) << 24);

      if(calculated_crc == received_crc)
        good_CRC_received_frames++;
      else
        bad_CRC_received_frames++;      

      if(udp_read_length > 5)
      {
        receive_sequence_number = uint8_t(packet_buffer[udp_read_length-5]);
      }

      itoa(received_crc, hex_string, 16);
      Serial.print("  Inbound CRC32: 0x" + String(hex_string));
      itoa(calculated_crc, hex_string, 16);
      Serial.print(", calculated: 0x" + String(hex_string) + " sqe.no: " + String(receive_sequence_number));

      if (calculated_crc == received_crc)
        Serial.println(" [CRC OK]");
      else
        Serial.println(" [CRC ERROR]");

      Serial.println("   " + String(good_CRC_received_frames) + " good frames, " + String(bad_CRC_received_frames) + " CRC errors");
      return (calculated_crc == received_crc);
    } // if(udp_read_length > 4)
    else
    {
      Serial.println("*W: incoming_packet_check_crc32: message too short");
      return false;
    }
  }

  void serial_print_message_content(bool command_message)
  {
    char print_buffer[PACKET_BUFFER_SIZE];

    if(command_message)
    {
      Serial.print("  command received ");
      for(int i=0; i<(read_crc_ok ? udp_read_length-5 : udp_read_length); i++)
      {
        Serial.print(String(int(packet_buffer[i])) + ", ");
      }
      Serial.println(read_crc_ok ? " [CRC OK]" : " [no CRC or CRC ERROR]");
    }
    else
    {
      strncpy(print_buffer, packet_buffer, PACKET_BUFFER_SIZE);
      char * crlf = strchr(print_buffer, '\n');
      if(crlf != NULL){ *crlf = NULL; }
      Serial.println("  received message: " + String(print_buffer) + (read_crc_ok ? " [CRC OK]" : ""));
    }
  } // void serial_print_message_content()

} // namespace my_UDP

// void UDP_wait_1(void) { my_UDP::wait_x(); }
void UDP_enable(void) { my_UDP::enable_port(); }
bool UDP_enabled(void) { return my_UDP::enabled; }
void UDP_receive_and_print_to_serial(void) { my_UDP::receive_and_print_to_serial(); }
void UDP_receive_and_acknowledge(void) { my_UDP::receive_and_print_to_serial(true); }
void UDP_restart(void){ my_UDP::restart(); }
