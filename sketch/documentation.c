/*

2023/04/13 16:55 - adding GPIO events log in 'push_notifications' module.
                 - later same day: added RCWL-0516 microwave motion sensor, and saw that it actually works!
                                   microwave sensor test was done on a S2 mini board, while the sensor is soldered
                                   to a short servo pigtail, to pins VBUS:GND:GPIO16 of the S2 mini board, 
                                   disconnectable with standard 100mil headers.

2023/04/13 13:25 - 'push on GPIO' largely written, and partially tested.
                 - the actual Pushover API request is not in the code yet, logic is still being under test using
                   mainly Serial.println() for debug.
                 - Config command (which is to be used for both enabling AND disabling push on GPIO) is fully 
                   coded in this sketch, and partially in the Python client, for testing.
                 - tests conducted by means of driving GPIO pin high and low, and observing how flow behaves on
                   transitions.
                 - ran into some issues with backoff window and millis() wrap-around logic, hopefully this is done
                   right by now, but I am not 100% sure.

2023/04/12 14:00 - enhancement planned: push notification upon GPIO pin change.
                   for motion detection, door alarms, etc.

2023/04/11 21:58 - adding read GPIO command. also removing 'V1_UINT16_VARIABLE_WIFI_BSSID' which is no longer
                   needed as reading the BSSID is done using the new 'V1_CMD_READ_OVER_STRING_VAR' command
                 - note that, the 'V1_CMD_READ_UINT16' command has been changed from being a fixed-length command
                   to a variale length command. all 16bit register reads did not need additional data after 
                   register identifier. the new 'read GPIO pin' read command (virtual register if you will) changes 
                   all that - pin number is required, as an additional octet.

2023/04/10 17:00 - adding WiFi BSSID read command. this allows better wifi diagnostics, in a multi-AP, or
                   AP + range extenders, wireless LAN environment.
                   essentially, BSSID is the access point MAC address. a single SSID network may consist of
                   multiple BSSIDs.

2023/04/09 17:00 - mofifying LED setup command.
                   rather than just using arguments' bit 7 for output set value, the API now uses bit 6 for 
                   polaruty config (i.e. some boards might turn the LED ON by writing HIGH, and others by writing
                   LOW). bit 7 now changes role, and instead of being used for direct write to GPIO out value, it
                   is refered to as 'On/Off' flag. the actual output value to the GPIO is now determined by 
                   combining the polarity (bit 6) and on/off (bit 7) fields. exact logic - in the code.
                   as a result, GPIO pin number is now restricted to 1 through 63.

2023/04/08 22:29 - while running an endless 'read RSSI' python test, the sketch seemd to hang up. unfortunately
                   I could not dump the log file. but I have a feeling this may have something to do with WiFiUdp
                   getting stuck.
                 - added 'V1_CMD_SET_LED_PIN' command, tested it and it seems to be working.

2023/04/08 15:50 - tested WiFi reconnect logic. seems to be working.

2023/04/08 12:57 - need to add disconnect/reconnect logic to sketch.
                 - to simplify, this should be disabled when connected via wired LAN.

2023/04/08 12:29 - adding 'V1_UINT16_VARIABLE_WIFI_RSSI' 16 bit integer read.
                   since ESP32 WiFi.RSSI() returns a signed integer (believed to be higher than -120 and lower
                   than zero), we will add 256 to return unsigned int.

2023/04/08 12:03 - merging '#ifndef __MINIMAL_HW__' conditional compilation clauses from 'experiment' fork back
                   to trunk.
                 - #define __MINIMAL_HW__ should produce an image that runs OK on the S2 mini, as well as on older
                   ESP32 WROOM boards, and the TTGO PoE module.

2023/04/07 23:18 - after commenting out GPIO code that essentially just configured GPIO pins 32 and 33, the
                   sketch ran successfully on the ESP32 S2 mini.

                   (further investigation suggests that use of GPIO pins 32 and 33 originate from a youtube reference
                   where they were just used to control LEDs. I.e. they had no functional role in operating the PoE 
                   hardware, or anything like that, and therefore, it is safe to assume they can be permanently 
                   commented out)

2023/04/07 23:00 - forked to 'LilygoPoE_s2_mini_experiment' (temporary) sketch
                   added #define __MINIMAL_HW__
                 - seems to still work with '__MINIMAL_HW__' defined, on a plain WROOM ESP32
                 - now time to try ESP32 S2 mini... no luck

2023/04/05 10:16 - trying to switch from WiFiUDP to AsyncUDP
                 - this doesn't work. numerous compilation errors.
                 - reverting to WiFiUdp

2023/04/04 23:59 - updating sketch date code, and reflashing plain ESP32 board (WiFi) to make sure it still works.

2023/04/04 23:00 - landed on this youtube video:
                   https://www.youtube.com/watch?v=XnqDEV21BSA&t=513s
                   at around 10:01, he explains how to set Arduino IDE for ESP32S2 with no Serial-USB converter
                   the board type he is using is 'ESP32S2 Dev Module'
                   -> this still did not work

                 - resorting to Adafruit guides at: https://learn.adafruit.com/adafruit-esp32-s2-feather/arduino-ide-setup

                 - after more trials and attempts I finally managed to get a simple 'Blink' sketch to the ESP32S2 mini board.
                   the sketch both blinked the LED, and sent characters to Serial interface, which were visible over the serial
                   monitor.
                 - the IDE settings were:
                   ESP32S2 Native USB board
                   and some more settings I tried copying to this project, and still the Blink sketch is working, and this
                   isn't. this sketch simply won't re-enumerate as a new Serial port after reset, and it seems that the
                   blink functionality simply remains on the board.

2023/04/04 14:30 - I am beginning to think the boards aren't flashed with Arduino bootloader, and I need to figure out
                   how to get bootloader onto the boards

2023/04/04 12:49 - trying cheap ESP2-S2 module
                 - sadly, getting this error message:
                   "A fatal error occurred: This chip is ESP32-S2 not ESP32. Wrong --chip argument?"
                   will try to change board model from 'ESP32 Dev Module' to ??? (found 'ESP32S2 Dev Module')
                 - well, the 'ESP32S2 Dev Module' yields the following error  
                   A serial exception error occurred: Cannot configure port, something went wrong. Original message: PermissionError(13, 'A device attached to the system is not functioning.', None, 31)
                   Note: This error originates from pySerial. It is likely not a problem with esptool, but with the hardware connection or drivers.
                   For troubleshooting steps visit: https://docs.espressif.com/projects/esptool/en/latest/troubleshooting.html
                   Failed uploading: uploading error: exit status 1
                 - trying 'ESP32S2 Native USB' board model
                   IDE loading made more progress, but still failed without launching the board. error message involved pressing reset.
                   the board does not enumerate automatically. must press reset and boot simulateously, and it then appears.
                   the post-load error message is:
                   Wrote 889040 bytes (583771 compressed) at 0x00010000 in 7.9 seconds (effective 899.6 kbit/s)...
                   Hash of data verified.

                   Leaving...
                   WARNING: ESP32-S2FH4 (revision v0.0) chip was placed into download mode using GPIO0.
                   esptool.py can not exit the download mode over USB. To run the app, reset the chip manually.
                   To suppress this note, set --after option to 'no_reset'.
                   Failed uploading: uploading error: exit status 1

                   

2023/03/24 19:00 - changing 'wired_lan_mac_address' string value setting logic.
                   from tests, it seemed that the 'V1_CMD_GET_MAC_ADDRESS' command was returning WiFi MAC info
                   even when connected to wired LAN (802.3).

2023/03/24 13:42 - Arduino IDE library update.
                   It seems as if I was using a pretty old esp32 library verions (1.0.6) until today, and this may have
                   been due to the fact that the package URL was outdated.
                   Today I added this URL: 
                   'https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json'
                   and on the following IDE startup, I got newer library versions (numbers 2.0.0 through 2.0.7)
                   according to github page 'https://github.com/espressif/arduino-esp32', the latest stable release
                   is 2.0.7
                 - library update takes a while, 5~10 minutes if I had to guess.
                 - promoting date code to 2023/3/24.1                   

2023/03/20 13:00 - expanding 'read 16bit variable' dictionary:
                   - added string variables attributes (count and size)
                   - adding sketch date code

2023/03/20 11:00 - removing hard coded pushover credentials.
                 - now credentials need to be provisioned over UDP command interface.

2023/03/19 20:50 - just flashed this sketch to a cheap ESP32 board, with no wired Ethernet port (WiFi only), and it
                   seems to be working just fine, with the exact same board settings as the more pricy LILYGO PoE ESP32
                   module.
                   granted, (#define __USE_WIRED_LAN__) needs to be commented out

2023/03/17 18:40 - 'V1_CMD_GET_MAC_ADDRESS' tested OK on both wired and wireless LANs.
                 - different MAC address strings were returned
                   - 'c0-49-ef-c8-f1-c0' (WiFi) and 'c0-49-ef-c8-f1-c3' (Ethernet)

2023/03/17 15:30 - tinkering with LAN address scanning.
                 - OS 'arp -a' command lists all nodes, and their MAC addresses
                 - it would help if we can know the ESP32 MAC address
                 - adding UDP command to get MAC address.

                 - it seems that WiFi.macAddress() always returns WiFi mac address (shocking ? :), even when connected
                   via wireles LAN. need to see how I can retreive ETH mac address
                   Note: by ARP it seems that the ETH mac address is 'c0-49-ef-c8-f1-c3' (WiFi: 'c0-49-ef-c8-f1-c0')

2023/03/17 12:24 - tests also pass on wired LAN (tests earlier today were on WiFi)
2023/03/17 12:13 - got push notifications to work with 'api.pushover.net' server
                 - entered the API account and created a new app with a new token.
                 - credentials are hard coded into the sketch (this isn't good, but will work for now)
                 - added UDP command mechanism to trigger the POST request
                   things are working :)

2023/03/17 22:06 - yet another attempt at ArduinoJson seems to work OK now.
                 - see 'pn_parse_json_test()', can be invoked by V1 command over UDP socket.

                 - EOD summary:
                   - basic JSON test parse OK
                   - openweather JSON response parsing fails. not sure yet why.

2023/03/16 21:05 - starting to add 'set string variable' command.
                 - command format:
                   - version (one octet)
                   - opcode (one octet)
                   - veriable index (one octet)
                   - string characters
                   - CRC32 (four octets)

2023/03/16 16:15 - trying and failing ArduinoJson for parsing JSON strings (even just a fixed string as a start)

2023/03/16 13:15 - working on read string variable
                 - mainly memory corruptions due to bad pointers use

2023/03/15 18:55 - slight change of plan
                 - since we do not have serial monitor when working over wired LAN, we need to first add a mechanism to pass
                   strings from the ESP32 to UDP client, for debug and diagnostics.
                 - as first POC, the 'quick brown fox' command has been added, which simply prepared a fixed string response
                   for the UDP client to read.

2023/03/15 17:55 - some HTTP get requests are working over WiFi connection.
                 - openweatherapi, and 'www.howsmyssl.com' which seems to be the canonical server for Arduino
                   HttpClient examples.
                 - next step is testing this over wired LAN and see if things still hold.
                 - we are using WiFiClientSecure for client (for non-secure connection as well, we just invoke 'client.setInsecure()'
                   before connection establishment attempt)


2023/03/14 20:21 - planning on adding HTTP requests, for 'pushover' push notifications

2023/01/29 10:44 - Adding good and bad frame counts to response string
                 - adding inbound message sequence number parsing
                 - added 'V1_CMD_GET_RANDOM_32BIT' opcode. this is intended to be the first one that responds
                   with a well formatted response datagram, rather than a text 'ack'
                   (scaffolding)

           17:55 - opcode 3 ('V1_CMD_GET_RANDOM_32BIT') now returns a binary formatted response.
                 - python script has been updated accordingly, and they both work together nicely.

           19:00 - successfully reading 'constant zero' and loop counter, as UINT16 variables, using binary
                   formatted response. 

2023/01/26 10:44 - compiling for Wired LAN

2023/01/24 18:11 - adding CRC at the end of all incmoming messagas.
                 - at this time, this does NOT change the outgoing (response) message at all.
                 - instead, for the time beeing, this sketch will only keep 'good_frames' and 'bad_frames'
                   counters, which will indicate how many incoming messages had good CRC and how many had
                   CRC errors.
                 - CRC used is CRC32, these four bytes will ALWAYS be the last four octets of the incoming
                   message.
                 - if incoming message is a string, CRC will come AFTER the CRLF characters.
           19:21 - inbound message CRC check is coded, tested, and works fine. 
           19:56 - [bug20230124a] in response CRC calculation - string handling is incorrect if inbound message
                   has CRC in it. the response CRC does not cover received CRC. it should.
                   see UDP tab 'uint32_t string_crc32(String s)' for why this happens this way.

2023/01/23 12:00 - need to consider adding CRC (and CRC check) on all incoming messages. as this is UDP.
                 - need to consider adding sequence number on incoming messages, for anti-replay attacks
                   as well as for packet loss detection.

2023/01/22 17:00 - adding 'command_handler.cpp' tab

2023/01/21 15:00 - adding 'command' transport (on receive flow)
                 - if first received byte value is 1 through 10, then incoming bytes are to be processed
                   as a message request

2023/01/17 10:40 - adding CRC32 in acknowledgement.
                 - for now, this is just to see if we have matching CRC32 calculation methods between the
                   arduino libraries and Python libraries.
                 - the python library currently used is zlib.crc32()

           11:35 - CRC32 cross-checks work fine on both wired and wireless connections.
                 - python script name : 'client_for_esp32.py'
                                 path : 'C:\Users\amit\PycharmProjects\Quick scripts\UDP_Socket'


2023/01/14 12:15 - added #ifdef __USE_WIRED_LAN__ clause, in an attempt to make this sketch compire for
                   either WiFi or Wired LAN convigurations.
                 - so far, this seems to be working  

*/

/*
 V1 inbound message format
 byte 0: 1 - version
 byte 1:     opcode
 ...         payload
 byte N-5:   sequence number
 last four bytes: CRC32

 v1 response format
 byte 0: 1 - version
 byte 1:     status (0: OK, 1, 2 ... - error codes)
 ...
 byte N-5:   sequence number
 last four bytes: CRC32
 */