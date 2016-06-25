#include "BrewManiacWeb.h"

String _jsonDisconnected="{\"code\":-1,\"result\":\"BM disc.\"}";

#if SerialDebug == true
#define DEBUGF(...)  DebugPort.printf(__VA_ARGS__)
#else
#define DEBUGF(...)
#endif

void BrewManiacWeb::_bmProxyEventHandler(uint8_t event)
{
	if(event == BMNotifyDisconnected){
			DEBUGF("BMNotifyDisconnected\n");
			if(_eventHandler) _eventHandler(this,BmwEventTargetDisconnected);
			// try reconnect in somthiem
			_reconnectTimer = millis();
	}else if(event == BMNotifyConnected){
			DEBUGF("**BM Connected**\n");
			if(_eventHandler) _eventHandler(this,BmwEventTargetConnected);	
	}else if(event == BMNotifyAutomationChanged){
			DEBUGF("BMNotifyAutomationChanged\n");
			if(_eventHandler) _eventHandler(this,BmwEventAutomationChanged);
		
	}else if(event == BMNotifySettingChanged){
			DEBUGF("BMNotifySettingChanged\n");
			if(_eventHandler) _eventHandler(this,BmwEventSettingChanged);
		
	}else if(event == BMNotifyStatusChanged){
			//DEBUGF("BMNotifyStatusChanged"); // too much
			if(_eventHandler) _eventHandler(this,BmwEventStatusUpdate);
		
	}else if(event == BMNotifyEvent){
			DEBUGF("Event:%d\n",_bmproxy.lastEvent);
			if(_eventHandler) _eventHandler(this,BmwEventBrewEvent);		
	}else if(event == BMNotifySetPwm){
			DEBUGF("setting PWM:%d\n",_bmproxy.setPwm);
			if(_eventHandler) _eventHandler(this,BmwEventPwmChanged);		
	}else if(event == BMNotifySetTemperature){
			DEBUGF("setting Temp:%f\n",_bmproxy.setTemperature);
			if(_eventHandler) _eventHandler(this,BmwEventSettingTemperatureChanged);
	}else if(event == BMNotifyButtonLabel){
			DEBUGF("button label:%d\n",_bmproxy.buttonLabel);
			if(_eventHandler) _eventHandler(this,BmwEventButtonLabel);
	}

}

void BrewManiacWeb::setIp(uint8_t ip[])
{
	_bmproxy.setIp(ip);
}

BrewManiacWeb::BrewManiacWeb(WriteFunc wf)
{
	_reconnectTimer=0;

	_eventHandler=NULL;
	// setup bmproxy
	_bmproxy.begin([&](uint8_t event){
		_bmProxyEventHandler(event);
		},wf);
}

bool BrewManiacWeb::targetConnected(void)
{
	return _bmproxy.isConnected();
}

static const char* SettingMap[]={
//#define PS_UseGas   0  //	Use Gas
	NULL,
//#define PS_kP      1  // 	kP
  	"s_kp",
//#define PS_kI      2  //	kI
  	"s_ki",
//#define PS_kD      3  //     kD
  	"s_kd",
//#define PS_SampleTime      4  //     SampleTime
	"s_sample_time",
//#define PS_WindowSize      5  //     WindowSize
  	"s_window",
//#define PS_BoilHeat      6    //   Boil Heat %
	"s_pwm",
//#define PS_Offset     7      // Offset
  	"s_cal",
//#define PS_Hysteresi     8   //    Hysteresi 
	NULL,
	NULL, // 9 
//#define PS_TempUnit   10     //  Scale Temp
	"s_unit",
//#define PS_SensorType     11      // Sensor Type
  	"s_sensor",
//#define PS_BoilTemp     12       //Temp Boil °C
	"s_boil",
//     13       Temp Boil °F
	NULL,
//#define PS_PumpCycle     14  //     Time Pump Cycle
	"s_pumpcycle",
//#define PS_PumpRest     15   //    Time Pump Rest
	"s_pumprest",
//#define PS_PumpPreMash     16  //     Pump PreMash
	"s_pumppremash",
//#define PS_PumpOnMash     17   //    Pump on Mash
	"s_pumpmash",
//#define PS_PumpOnMashOut     18  //     Pump on MashOut
	"s_pumpmashout",
//#define PS_PumpOnBoil      19     //  Pump on Boil
	"s_pumpboil",
//#define PS_TempPumpRest     20    //   Temp Pump Rest °C
	"s_pumpstop",
//     21       Temp Pump Rest °F
	NULL,
//#define PS_PidPipe     22     //  PID Pipe
	"s_pipe",
//#define PS_SkipAddMalt     23  //     Skip Add Malt
	"s_skipadd",
//#define PS_SkipRemoveMalt     24  //     Skip Remove Malt
	"s_skipremove",
//#define PS_SkipIodineTest     25    //   Skip Iodine Test
	"s_skipiodine",
//#define PS_IodineTime     26   //    Iodine Time
	"s_iodine",
//#define PS_Whirlpool     27     //  Whirlpool  	
	"s_whirlpool"
};

void BrewManiacWeb::getSettings(String& json)
{
	if(!_bmproxy.isConnected()){
		json = _jsonDisconnected;
		return;
	}
	
    json = "{\"code\":0,\"result\":\"OK\", \"data\":{";

	bool comma=false;
    for(int i=0;i< sizeof(SettingMap)/sizeof(const char*);i++)
    {
    	if(SettingMap[i]){
    		if(!comma){
    			comma=true; // don't append comma before the first object
    		}else{
    			json += ",";
    		}
    		json += "\"" + String(SettingMap[i])  +"\":"+String(_bmproxy.getSetting(i));
    	}
    }
    json += "}}";

}


void BrewManiacWeb::getAutomation(String& json)
{
	AutomationRecipe* recipe=& _bmproxy.automationRecipe;
	if(!_bmproxy.isConnected()){
		json = _jsonDisconnected;
		return;
	}
	
    json = "{\"code\":0,\"result\":\"OK\", \"data\":{\"rest_tm\":[";

	for(int i=0;i<8;i++){
			json += String(recipe->restTime[i]);
			if(i!=7) json += ",";
	}
	json += "], \"rest_tp\":[";

	for(int i=0;i<8;i++){
			json += String(recipe->restTemp[i]);
			if(i!=7) json += ",";
	}
	
	json += "], \"boil\":";
	json += String(recipe->boilTime);
	json += ", \"hops\":[";
	for(int i=0;i<recipe->numberHops;i++){
		json +=String(recipe->hops[i]);
		if(i!= (recipe->numberHops-1)) json += ",";
	}	
	json += "] }}";
}

void BrewManiacWeb::getCurrentStatus(String& json,bool initial)
{
	if(!_bmproxy.isConnected()){
		json =  _jsonDisconnected;
		return;
	}

   	json = "{\"state\":";	
	json += String(_bmproxy.stage);
	
	if(initial){
		json += ",\"version\":";
		json += String(_bmproxy.brewManiacVersion);		
   	}
	json += ",\"btn\":";
	json += String(_bmproxy.buttonLabel);

	json += ",\"pump\":";
	json += String(_bmproxy.pumpStatus);
	json += ",\"heat\":";
	json += String(_bmproxy.heaterStatus);
	json += ",\"temp\":";
	json += String(_bmproxy.currentTemperature);
	json += ",\"tr\":";
	json += String((_bmproxy.isTempReached)? 1:0);
	json += ",\"pwmon\":";
	json += String((_bmproxy.isPwmOn)? 1:0);
	json += ",\"paused\":";
	json += String((_bmproxy.isPaused)? 1:0);
	json +=",\"counting\":";
	json += String((_bmproxy.isCountingUp)? 1:0);	
	json += ",\"timer\":";
	json += String(_bmproxy.runningTimer);
	json += "}";
}

void BrewManiacWeb::loop(void)
{
	// the loop function will handle retransmission.
	_bmproxy.loop();
	if(!_bmproxy.isConnected() 
		&& _reconnectTimer != 0 
		&& (millis() - _reconnectTimer > RECONNECT_TIME)){
		_reconnectTimer=0;
		connectTarget();
	}
}

void BrewManiacWeb::connectTarget(void)
{
	_bmproxy.connect();
}

void BrewManiacWeb::getLastEvent(String &json)
{
	if(!_bmproxy.isConnected()){
		json = _jsonDisconnected;
		return;
	}
	json = "{\"event\":" 
		+ String(_bmproxy.lastEvent)
		+ "}";
}

void BrewManiacWeb::getSettingPwm(String& json)
{
	if(!_bmproxy.isConnected()){
		json = _jsonDisconnected;
		return;
	}
	json = "{\"pwm\":" 
		+ String(_bmproxy.setPwm)
		+ "}";

}

void BrewManiacWeb::getSettingTemperature(String& json)
{
	if(!_bmproxy.isConnected()){
		json = _jsonDisconnected;
		return;
	}
	json = "{\"stemp\":" 
		+ String(_bmproxy.setTemperature)
		+ "}";
}
// need to parse JSON object
StaticJsonBuffer<1024> jsonBuffer;
char _strJsonBuffer[1024];

bool BrewManiacWeb::updateSettings(String& json)
{
	int addresses[sizeof(SettingMap)];
	byte values[sizeof(SettingMap)];
	byte count=0;
	
	memcpy(_strJsonBuffer,json.c_str(),json.length());
	JsonObject& root = jsonBuffer.parseObject(_strJsonBuffer);
	if (!root.success()){
		DEBUGF("wrong JSON string\n");
		return false;
	}
	
	for(int i=0;i< sizeof(SettingMap)/sizeof(const char*);i++){
		if(SettingMap[i] && root.containsKey(SettingMap[i])){
			addresses[count] = i;
			values[count] =  root[SettingMap[i]].as<byte>();
			count ++;
		}
	}
	_bmproxy.sendUpdateSetting(count,addresses,values);
	
	return true;
}

bool BrewManiacWeb::updateAutomation(String& json)
{
	memcpy(_strJsonBuffer,json.c_str(),json.length());
	JsonObject& root = jsonBuffer.parseObject(_strJsonBuffer);
	if (!root.success()){
		DEBUGF("wrong JSON string\n");
		return false;
	}
	
	AutomationRecipe recipe;

	for(byte i=0;i<8;i++){
		recipe.restTime[i]=root["rest_tm"][i];
		recipe.restTemp[i]=root["rest_tp"][i];
	}
	recipe.restTime[0]=1;
	
	recipe.boilTime=root["boil"];
	
	JsonArray& hopArray = root["hops"];

	byte idx=0;
	for(JsonArray::iterator it=hopArray.begin(); it!=hopArray.end(); ++it) 	
	{
    	recipe.hops[idx] = it->as<unsigned char>();
    	idx++;
	}
	recipe.numberHops = idx;
	_bmproxy.sendAutomationRecipe(&recipe);
}

void BrewManiacWeb::sendButton(byte mask,bool longPressed)
{
	_bmproxy.sendButton(mask,longPressed);
}
