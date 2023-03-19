# Esp32_UDP_node
This is an Arduino sketch, to be flashed onto an ESP32 dev board.
Wired LAN connection is optional - for LILYGO T-Internet-PoE

Functionality:
- UDP server
  - the Esp32 is a UDP 'server' (i.e. listens on a UDP port, processes incoming commands/requests
    and sends appropriate reponses.
  - this is mainly featured to let the ESP32 be managed from a PC (e.g. python console) on the h
    ome LAN
- HTTP client
  - Esp32 can send Http requests and process responses, over the Internet.
