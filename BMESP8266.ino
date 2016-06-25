#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SoftwareSerial.h>
//#include <GDBStub.h>
#include "WifiSetup.h"
#include "AsyncServerSideEvent.h"
#include "HttpUpdateHandler.h"
#include "BrewManiacProxy.h"
#include "BrewManiacWeb.h"
//#include "SPIFFSEditor.h"
#include "ESPUpdateServer.h"

#include "config.h"
/**************************************************************************************/
/* End of Configuration 																	  */
/**************************************************************************************/


const char* host = HOSTNAME;


#if (SerialDebug == true) && (UseSoftwareSerial != true) && (DebugPort == Serial)
#error "Hardware Serial is used to connect Arduino, cannot be used for debug at the same time"
#endif

#if SerialDebug == true
#define DebugOut(a) DebugPort.print(a)
#else
#define DebugOut(a) 
#endif

#if UseSoftwareSerial == true
SoftwareSerial wiSerial(SW_RX_PIN,SW_TX_PIN);
#else
#define wiSerial Serial
#endif

AsyncWebServer server(80);
BrewManiacWeb bmWeb([](uint8_t ch){wiSerial.write(ch);});
AsyncServerSideEventServer sse("/status.php");

typedef union _address{
                uint8_t bytes[4];  // IPv4 address
                uint32_t dword;
} IPV4Address;


class BmwHandler: public AsyncWebHandler 
{
public:
	BmwHandler(void){}
	void handleRequest(AsyncWebServerRequest *request){
	 	if(request->method() == HTTP_GET && request->url() == "/settings.php"){
	 		String json;
	 		bmWeb.getSettings(json);
	 		request->send(200, "text/json", json);
	 	}else if(request->method() == HTTP_GET && request->url() == "/automation.php"){
	 		String json;
	 		bmWeb.getAutomation(json);
	 		request->send(200, "text/json", json);
	 	}else if(request->method() == HTTP_GET && request->url() == "/button.php"){
			if(request->hasParam("code")){
				AsyncWebParameter* p = request->getParam("code");
				byte code=p->value().toInt();
	 			bmWeb.sendButton(code & 0xF, (code & 0xF0)!=0);
	 			request->send(200);
	 		}else{
	 			request->send(400);
	 		}
	 	}else if(request->method() == HTTP_POST && request->url() == "/saveauto.php"){
	 		String data=request->getParam("data", true, false)->value();
	 		DebugOut("saveauto.php:\n");
	 		DebugOut(data.c_str());
	 		if(bmWeb.updateAutomation(data)){
	 			request->send(200, "text/json", "{\"code\":0,\"result\":\"OK\"}");
	 		}else{
	 			request->send(400);
	 		}
	 		
	 	}else if(request->method() == HTTP_POST && request->url() == "/savesettings.php"){
	 		String data=request->getParam("data", true, false)->value();
	 		DebugOut("savesettings.php:\n");
	 		DebugOut(data.c_str());
	 		if(bmWeb.updateSettings(data)){
	 			request->send(200, "text/json", "{\"code\":0,\"result\":\"OK\"}");
	 		}else{
	 			request->send(400);
	 		}
	 	}	 	
	 }
	 
	bool canHandle(AsyncWebServerRequest *request){
	 	if(request->method() == HTTP_GET){
	 		if(request->url() == "/settings.php" || request->url() == "/automation.php" || request->url() == "/button.php" )
	 			return true;
	 	}else if(request->method() == HTTP_POST){
	 		if(request->url() == "/saveauto.php" || request->url() == "/savesettings.php")
	 			return true;	 	
	 	}
	 	return false;
	 }	 
};
BmwHandler bmwHandler;


uint8_t clientCount;
void sseEventHandler(AsyncServerSideEventServer * server, AsyncServerSideEventClient * client, SseEventType type)
{
//	DebugOut("eventHandler type:\n");
//	DebugOut(type);
	
	if(type ==SseEventConnected){
		clientCount ++;
		DebugOut("New connection, current client number:");
		DebugOut(clientCount);

		String json;
		
		bmWeb.getCurrentStatus(json,true);
		client->sendData(json);
		
	}else if (type ==SseEventDisconnected){
		clientCount --;
		DebugOut("Disconnected, current client number:");
		DebugOut(clientCount);
	}
}

void bmwEventHandler(BrewManiacWeb* bmw, BmwEventType event)
{
	if(event==BmwEventTargetConnected){
		IPV4Address ip;
		ip.dword = WiFi.localIP();
		bmWeb.setIp(ip.bytes);
	}else if(event==BmwEventAutomationChanged){
		// request reload automation
		sse.broadcastData("{\"update\":\"recipe\"}");
	}else if(event==BmwEventSettingChanged){
		// request reload setting
		sse.broadcastData("{\"update\":\"setting\"}");
	}else if(event==BmwEventStatusUpdate || event==BmwEventTargetDisconnected || event==BmwEventButtonLabel){
		String json;
		bmw->getCurrentStatus(json);
		sse.broadcastData(json);
	}else if(event==BmwEventBrewEvent){
		String json;
		bmw->getLastEvent(json);
		sse.broadcastData(json);
	}else if(event==BmwEventPwmChanged){
		String json;
		bmw->getSettingPwm(json);
		sse.broadcastData(json);

	}else if(event==BmwEventSettingTemperatureChanged){
		String json;
		bmw->getSettingTemperature(json);
		sse.broadcastData(json);
	}
}


HttpUpdateHandler httpUpdateHandler(FIRMWARE_UPDATE_URL,JS_UPDATE_URL);
unsigned long _connectionTime;
byte _wifiState;

#define WiFiStateConnected 0
#define WiFiStateWaitToConnect 1
#define WiFiStateConnecting 2
#define TIME_WAIT_TO_CONNECT 10000
#define TIME_RECONNECT_TIMEOUT 10000

bool testSPIFFS(void)
{
	File vf=SPIFFS.open("/BME_TestWrite.t","w+");
	if(!vf){
  		DebugOut("Failed to open file for test\n");
		return false;
	}
	const char *str="test string\n";
	vf.print(str);
	vf.close();
	DebugOut("Close file writing\n");
	
	File rf=SPIFFS.open("/BME_TestWrite.t","r");
	if(!rf){
  		DebugOut("Failed to open file for test for reading\n");
		return false;
	}
//	String c=rf.readString();
	String c=rf.readStringUntil('\n');
	rf.close();
	
	DebugOut("Reading back data:");
	DebugOut(c.c_str());
	return true;
}

void setup(void){
	_wifiState=WiFiStateConnected;
	
	wiSerial.begin(38400);
	
	#if SerialDebug == true
  	DebugPort.begin(115200);
  	DebugOut("\n");
  	DebugPort.setDebugOutput(true);
  	#endif
  	
	#if SwapSerial == true && UseSoftwareSerial != true
	Serial.swap();
	#endif
  	
	//1. Start WiFi  
	WiFiSetup::begin(host);

  	DebugOut("Connected! IP address: ");
  	DebugOut(WiFi.localIP());

	if (!MDNS.begin(host)) {
		DebugOut("Error setting mDNS responder");
	}	
	// TODO: SSDP responder

	//2.Initialize file system
	//start SPI Filesystem
  	if(!SPIFFS.begin()){
  		// TO DO: what to do?
  		DebugOut("SPIFFS.being() failed");
  	}else{
  		DebugOut("SPIFFS.being() Success");
  	}

	// check version
	bool forcedUpdate;
	String jsVersion;
	File vf=SPIFFS.open("/version.txt","r");
	if(!vf){
		jsVersion="0";
  		DebugOut("Failed to open version.txt");
  		forcedUpdate=true;

	}else{
		jsVersion=vf.readString();
		DebugOut("Version.txt:");
		DebugOut(jsVersion.c_str());
		forcedUpdate=false;
	}
	
	//3. setup Web Server
	if(forcedUpdate){
		//3.1 forced to update
		httpUpdateHandler.setUrl("/");
		httpUpdateHandler.setVersion(BME8266_VERSION,jsVersion);
		server.addHandler(&httpUpdateHandler);
	}else{
		//3.1 HTTP Update page
		httpUpdateHandler.setUrl(ONLINE_UPDATE_PATH);
		httpUpdateHandler.setVersion(BME8266_VERSION,jsVersion);
		server.addHandler(&httpUpdateHandler);

		//3.2 Normal serving pages 
		//3.2.1 status report through SSE
		sse.onEvent(sseEventHandler);
		server.addHandler(&sse);
	
		server.addHandler(&bmwHandler);
		//3.2.2 SPIFFS is part of the serving pages
		server.serveStatic("/", SPIFFS, "/","public, max-age=259200"); // 3 days
	}
    
	server.on("/fs",[](AsyncWebServerRequest *request){
		FSInfo fs_info;
		SPIFFS.info(fs_info);
		request->send(200,"","totalBytes:" +String(fs_info.totalBytes) +
		" usedBytes:" + String(fs_info.usedBytes)+" blockSize:" + String(fs_info.blockSize)
		+" pageSize:" + String(fs_info.pageSize));
		//testSPIFFS();
	});
	
	// 404 NOT found.
  	//called when the url is not defined here
	server.onNotFound([](AsyncWebServerRequest *request){
		request->send(404);
	});
	
	//4. start Web server
	server.begin();
	DebugOut("HTTP server started");

	MDNS.addService("http", "tcp", 80);
	
	// 5. try to connnect Arduino
  	bmWeb.onEvent(bmwEventHandler);
	bmWeb.connectTarget();
	
	// 6. start WEB update pages.	
	ESPUpdateServer_setup();
}



void loop(void){
	ESPUpdateServer_loop();
  	
  	bmWeb.loop();
 
 	if(WiFi.status() != WL_CONNECTED)
 	{
 		if(_wifiState==WiFiStateConnected)
 		{
			byte nullIp[4]={0,0,0,0};
			bmWeb.setIp(nullIp);

			_connectionTime=millis();
			_wifiState = WiFiStateWaitToConnect;
		}
		else if(_wifiState==WiFiStateWaitToConnect)
		{
			if((millis() - _connectionTime) > TIME_WAIT_TO_CONNECT)
			{
				WiFi.begin();
				_connectionTime=millis();
				_wifiState = WiFiStateConnecting;
			}
		}
		else if(_wifiState==WiFiStateConnecting)
		{
			if((millis() - _connectionTime) > TIME_RECONNECT_TIMEOUT){
				ESP.restart();
			}
		}
 	}
 	else
 	{
 		_wifiState=WiFiStateConnected;
  	}
  	
  	httpUpdateHandler.runUpdate();
  	// read from UART and input to BM
  	while(wiSerial.available())
  		bmWeb.processData((uint8_t)wiSerial.read());
}




