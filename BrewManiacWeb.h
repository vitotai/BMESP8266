#ifndef BrewManiacWeb_H
#define BrewManiacWeb_H

#include <Arduino.h>
#include <functional>
#include <ArduinoJson.h>
#include "BrewManiacProxy.h"

#include "config.h"

class BrewManiacWeb;

typedef enum _BmwEventType{
	BmwEventTargetDisconnected,
	BmwEventTargetConnected,
	BmwEventAutomationChanged,
	BmwEventSettingChanged,
	BmwEventStatusUpdate,
	BmwEventBrewEvent,
	BmwEventPwmChanged,
	BmwEventSettingTemperatureChanged,
	BmwEventButtonLabel
}BmwEventType;

typedef std::function<void(BrewManiacWeb* bmw, BmwEventType event)> BmwEventHandler;

class BrewManiacWeb
{
protected:
	BrewManiacProxy _bmproxy;
	BmwEventHandler _eventHandler;
	
	void _bmProxyEventHandler(uint8_t event);

	unsigned long _reconnectTimer;
public:

	BrewManiacWeb(WriteFunc wf);
	void connectTarget(void);
	void processData(uint8_t ch){_bmproxy.read(ch);}
	bool targetConnected(void);
	void setIp(uint8_t ip[]);
	
	void loop(void);
	// get data
	void getSettings(String& json);
	void getAutomation(String& json);	
	// update data
	bool updateSettings(String& json);
	bool updateAutomation(String& json);
	void sendButton(byte mask,bool longPressed);
	// get current status
	void getCurrentStatus(String& json,bool initial=false);
	
	// 'push' data
	void getLastEvent(String& json);
	void getSettingPwm(String& json);
	void getSettingTemperature(String& json);
	void getButtonLabel(String& json);
	// event handler
	void onEvent(BmwEventHandler handler){_eventHandler=handler;}
	
};

#endif



