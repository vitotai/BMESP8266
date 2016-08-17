#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
//#include <GDBStub.h>
#include "WiFiSetup.h"

#define UseWebSocket true

#if UseWebSocket != true
#include "AsyncServerSideEvent.h"
#endif

#include "HttpUpdateHandler.h"
#include "BrewManiacProxy.h"
#include "BrewManiacWeb.h"
//#include "SPIFFSEditor.h"
#include "ESPUpdateServer.h"

#include "config.h"
/**************************************************************************************/
/* End of Configuration 																	  */
/**************************************************************************************/

#define WS_PATH "/ws"

const char* host = HOSTNAME;


#if (SerialDebug == true) && (UseSoftwareSerial != true) && (DebugPort == Serial)
#error "Hardware Serial is used to connect Arduino, cannot be used for debug at the same time"
#endif

#if SerialDebug == true
#define DebugOut(a) DebugPort.print(a)
#define DBG_PRINTF(...) DebugPort.printf(__VA_ARGS__)
#else
#define DebugOut(a) 
#define DBG_PRINTF(...)
#endif

#if UseSoftwareSerial == true
SoftwareSerial wiSerial(SW_RX_PIN,SW_TX_PIN);
#else
#define wiSerial Serial
#endif

AsyncWebServer server(80);
BrewManiacWeb bmWeb([](uint8_t ch){wiSerial.write(ch);});

#if UseWebSocket == true
AsyncWebSocket ws(WS_PATH);

#else
AsyncServerSideEventServer sse("/status.php");
#endif

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

#if UseWebSocket == true

void processRemoteCommand( uint8_t *data, size_t len)
{
	StaticJsonBuffer<128> jsonBuffer;
	char buf[128];
	int i;
	for(i=0;i< len && i<127;i++){
		buf[i]=data[i];
	}
	buf[i]='\0';
	JsonObject& root = jsonBuffer.parseObject(buf);

	if (root.success() && root.containsKey("btn") ){
		int code = root["btn"];
		bmWeb.sendButton(code & 0xF, (code & 0xF0)!=0);
	}
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
	if(type == WS_EVT_CONNECT){
    	DBG_PRINTF("ws[%s][%u] connect\n", server->url(), client->id());
		String json;
		bmWeb.getCurrentStatus(json);
		client->text(json);
  	} else if(type == WS_EVT_DISCONNECT){
    	DBG_PRINTF("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  	} else if(type == WS_EVT_ERROR){
    	DBG_PRINTF("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  	} else if(type == WS_EVT_PONG){
    	DBG_PRINTF("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  	} else if(type == WS_EVT_DATA){
    	AwsFrameInfo * info = (AwsFrameInfo*)arg;
    	String msg = "";
    	if(info->final && info->index == 0 && info->len == len){
      		//the whole message is in a single frame and we got all of it's data
      		DBG_PRINTF("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
			processRemoteCommand(data,info->len);

		} else {
      		//message is comprised of multiple frames or the frame is split into multiple packets
#if 0  // for current application, the data should not be segmented  
      		if(info->index == 0){
        		if(info->num == 0)
          		DBG_PRINTF("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        		DBG_PRINTF("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      		}

      		DBG_PRINTF("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

	        for(size_t i=0; i < info->len; i++) {
    	    	//msg += (char) data[i];
        	}
		
      		//DBG_PRINTF("%s\n",msg.c_str());

			if((info->index + len) == info->len){
				DBG_PRINTF("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        		if(info->final){
        			DBG_PRINTF("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        		}
      		}
#endif
      	}
    }
}
void broadcastMessage(String msg)
{
	ws.textAll(msg);
}
void broadcastMessage(const char* msg)
{
	ws.textAll(msg);
}
#else // #if UseWebSocket == true

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

void broadcastMessage(String msg)
{
	sse.broadcastData(msg);
}

void broadcastMessage(const char* msg)
{
	sse.broadcastData(msg);
}

#endif //#if UseWebSocket == true

void bmwEventHandler(BrewManiacWeb* bmw, BmwEventType event)
{
	if(event==BmwEventTargetConnected){
		IPV4Address ip;
		ip.dword = WiFi.localIP();
		bmWeb.setIp(ip.bytes);
	}else if(event==BmwEventAutomationChanged){
		// request reload automation
		broadcastMessage("{\"update\":\"recipe\"}");
	}else if(event==BmwEventSettingChanged){
		// request reload setting
		broadcastMessage("{\"update\":\"setting\"}");
	}else if(event==BmwEventStatusUpdate || event==BmwEventTargetDisconnected || event==BmwEventButtonLabel){
		String json;
		bmw->getCurrentStatus(json);
		broadcastMessage(json);
	}else if(event==BmwEventBrewEvent){
		String json;
		bmw->getLastEvent(json);
		broadcastMessage(json);
	}else if(event==BmwEventPwmChanged){
		String json;
		bmw->getSettingPwm(json);
		broadcastMessage(json);

	}else if(event==BmwEventSettingTemperatureChanged){
		String json;
		bmw->getSettingTemperature(json);
		broadcastMessage(json);
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
	
	wiSerial.begin(BAUDRATE);
	
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
#if UseWebSocket == true
	ws.onEvent(onWsEvent);
  	server.addHandler(&ws);
#else
		sse.onEvent(sseEventHandler);
		server.addHandler(&sse);
#endif 

	
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




