#ifndef CONFIG_H
#define CONFIG_H
/**************************************************************************************/
/*  Configuration: 																	  */
/*  Only one setting: the serial used to connect to.                                  */
/*   if SoftwareSerial is used. RX/TX PIN must be defined.                            */
/*   else, UART0(Serial) is used.                                                     */
/**************************************************************************************/

#define UseSoftwareSerial false

#if UseSoftwareSerial == true
#define SW_RX_PIN 4
#define SW_TX_PIN 5
#endif

#define SwapSerial false
// changes the UART pins to use GPIO13 and GPIO15 as RX and TX. 
//- See more at: http://www.esp8266.com/viewtopic.php?f=23&t=6394#sthash.5cMDbhuW.dpuf


#define SerialDebug false
#define DebugPort Serial


/**************************************************************************************/
/*  Advanced Configuration:  														  */
/*   URLs .										  									  */
/**************************************************************************************/

#define HOSTNAME "bm"
// this will be used as hostname, accessed as "bm.local", as well as SSID to configurate 
// Wireless network
// serving page will be like http://bm.local/

#define ONLINE_UPDATE_PATH "/update"
// this is the path to check online update.
// like http://bm.local/update

// for web interface update
#define UPDATE_SERVER_PORT 8008
#define FILE_MANAGEMENT_PATH "/filemanager"
// direct access to file system
// http://bm.local:8008/filemenager
// upload firmware by browser


// http://bm.local:8008/update 
#define SYSTEM_UPDATE_PATH "/systemupdate"
#define SYSTEM_UPDATE_USER "brewmaniac"
#define SYSTEM_UPDATE_PASS "rdwhahb"

/**************************************************************************************/
/*  Some other Configuration:  														  */
/*  Don't touch them if you don't know what they are.								  */
/**************************************************************************************/

#define BME8266_VERSION "0.9.2"

#define FIRMWARE_UPDATE_URL "http://brew.vito.tw/bmeupdate.php?info"
#define JS_UPDATE_URL  "http://brew.vito.tw/bmejsupdate.php?v="

// request status report period
#define REPORT_PERIOD 6
// the time to reconnect to Arduino when disconnected.
#define RECONNECT_TIME 12000
#if UseSoftwareSerial == true
#define BAUDRATE 38400
#else
#define BAUDRATE 115200
#endif


#define UseWebSocket false
#define UseServerSideEvent true


#endif

