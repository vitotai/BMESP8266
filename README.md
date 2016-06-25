# BMESP8266
To connect BrewManiac with ESP8266 and make it a WiFi-enabled device.  

The build environment uses ESP8266/Arduino. A few additional libraries is necessary:
WiFiManager  https://github.com/tzapu/WiFiManager
ESPAsyncTCP https://github.com/me-no-dev/ESPAsyncTCP
ESPAsyncWebServer  https://github.com/me-no-dev/ESPAsyncWebServer
ESP8266HTTPUpdateServer (newer version is needed.)  https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266HTTPUpdateServer

To configurate the hardware settings, mainly the Serial port, open config.h and change the #define's.
