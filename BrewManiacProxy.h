#ifndef BrewManiacProxy_H
#define BrewManiacProxy_H

#include <Arduino.h>
#include <functional>
#include "config.h"

#define TEST_VALUE false


#if SerialDebug != true
#define DEBUG_BMPROXY false
#define DEBUG_WIFIL2 false
#define DEBUG_ERROR false
#else
#define DEBUG_BMPROXY true
#define DEBUG_WIFIL2 true
#define DEBUG_ERROR true
#endif

#define LambdaSupported true

#ifndef byte
typedef unsigned char byte;
#endif

#ifndef NULL
#define NULL 0
#endif

#define ButtonUpMask    0x01
#define ButtonDownMask  (0x01 << 1)
#define ButtonStartMask (0x01 << 2)
#define ButtonEnterMask (0x01 << 3)

#define StartOfFrame 0x81

typedef struct _autoRecipe{
	byte restTime[8];
	float restTemp[8];
	byte boilTime;
	byte hops[10];
	byte numberHops;
}AutomationRecipe;


#define ToTempInStorage(t) ((int)((t)*16))
#define TempFromStorage(t)  ((float)(t)/16.0)

#define EEPROMSize (28)

// message code
#define BT(c) ((c)|0x20)
#define WiControllerAck BT(1)
#define WiConnectRequest BT(2)
#define WiButtonPressed BT(3)
#define WiSetMashSchedule BT(4)
#define WiSetBoilHopsTime  BT(5)
#define WiPersistenceRead BT(6)
#define WiPersistenceBlockRead BT(7)
#define WiPersistenceSet BT(8)
#define WiDisconnect BT(9)
#define WiDeviceAddress BT(10)
#define WiSetRecipe BT(11)
#define WiQueryRecipe BT(12)

#define BO(c) (c)
#define WiBMAck BO(1)
#define WiConectionConfirm BO(2)
#define WiStatus BO(3)
#define WiEventNotification BO(4)
#define WiSetTemperature BO(5)
#define WiSetPwm BO(6)
#define WiPersistenceValue BO(7)
#define WiPersistenceBlockData BO(8)
#define WiRecipeInformation BO(9)
#define WiButtonLabel BO(10)
///
#define RequestIndicationSequence(n) (((n)&0xF))
#define ResponseSequence(n) (((n)&0xF)|0x10)
#define RoundSequenceNumber(n)  ((n) & 0xF)
#define IsAckResponse(n) (((n)&0xF0) == 0x10)
#define PacketSequence(n) ((n)&0xF)
//PDU format
#define WiMessageCode(c) ((c)&0x3F)
#define NullPDU(c) (c)
#define OnePDU(c)  ((c) | (0x1<<6))
#define VarPDU(c) ((c) |  (0x2<<6))

#define IsNullPDU(c) (((c)&0xC0) ==0)
#define IsOnePDU(c) (((c)&0xC0) ==0x40)
#define IsVarPDU(c) (((c)&0xC0) ==0x80)

#define RetryTimer 1500
#define MaxRetry 5
#define BufferSize 64
#define QueueBufferSize 128

typedef enum{
	WSWaitSOF= 0,
	WSWaitSequence=1,
	WSMessageCode= 2,
	WSLength= 3,
	WSData= 4,
	WSChecksum= 5
} BMParseState;

typedef enum{
	BMSNull,
	BMSConnecting,
	BMSConnected,
	BMSDisconnected,
	BMSSyncSetting,
	BMSGetRecipe
}BMStatus;

#define BMNotifyDisconnected 1
#define BMNotifyConnected 2
#define BMNotifyAutomationChanged 3
#define	BMNotifySettingChanged 4
#define	BMNotifyStatusChanged 5
#define	BMNotifyEvent 6
#define	BMNotifySetPwm 7
#define	BMNotifySetTemperature 8
#define	BMNotifyButtonLabel 9

#if LambdaSupported == true
typedef std::function<void(uint8_t event)> EventHandlerFunc;
typedef std::function<void(uint8_t ch)> WriteFunc;
#endif

class BrewManiacProxy{
	
public:
//	BrewManiacProxy(void (*handler)(byte),void (*wfunc)(byte));
	BrewManiacProxy(void);
	
#if LambdaSupported == true
	void begin(EventHandlerFunc handler,WriteFunc wfunc);
#else
	void begin(void (*handler)(byte),void (*wfunc)(byte));
#endif

	void loop(void);
	void connect(void);

//	void setDelegate(BrewManiacProxyDelegate *delegate);
	
	void sendButton(byte mask,bool longPressed);
	void sendAutomationRecipe(AutomationRecipe *recipe);
	void sendUpdateSetting(int number,int addresses[],byte values[]);
	void setIp(byte ip[]);
	byte getSetting(int addr);
	bool isConnected(void);
		
	byte pumpStatus;
	byte heaterStatus;
 	bool isPwmOn;
	bool isTempReached;
	bool isPaused;
	bool isCountingUp;
	byte stage;
	float currentTemperature;
	int  runningTimer;
	byte lastEvent;
	int   setPwm;
	float setTemperature;
	AutomationRecipe automationRecipe;

	void read(byte ch);
	
	int frameCount(void){return _frameErrorCount;}
	byte brewManiacVersion;
	byte buttonLabel;
private:
	byte _l2QueueBuffer[QueueBufferSize];
	byte _l2WritePtr;
	byte _l2CurrentPduPtr;
	byte _l2SendNumber;
	byte _l2RcvNumber;
	bool _isResponseFrame;
	byte _packetSequence;
	void _l2SendAck(void);
	void _l2DiscardCurrentPdu(void);
	void _l2Init(void){_l2WritePtr=_l2CurrentPduPtr=0;}
	void _sendResponseFromBuffer(int len);
	void _l2SendFromQueueBuffer(void);
	void _copyToQueueBuf(byte len);
	
	int  _frameErrorCount; // for statistics
	bool _frameError;
	byte _txBuffer[BufferSize];

	void resendLastFrame(void);

	BMStatus _bmState;

	unsigned long _timerStartTime;

	byte _retryCounter;
	byte eeprom[EEPROMSize];
	
#if LambdaSupported == true
	WriteFunc _write;	
	EventHandlerFunc _eventHandler;
#else
	void (*_write)(byte);	
	void (*_eventHandler)(byte);
#endif

	#if TEST_VALUE == true
	void initTestValue(void);
	#endif
	void _sendFromBuffer(void);
	// send msge
	void _sendFrame(int len,bool requestAck);
	void sendPersistenceBlockRead(void);
	void sendConnectRequest(void);
	void sendIpAddress(void);
	void sendQueryRecipe(void);
	//message handler
	void handleConnectionConfirm(void);
	void handleStatus(void);
	void handleEventNotification(void);
	void handleSetTemperature(void);
	void handleSetPwm(void);
	void handlePersistenceValue(void);
	void handlePersistenceBlockData(void);
	void handleRecipeInformation(void);
	void handleButtonLabel(void);
	// frame handling
	void processMsg(void);
	void BMDisc(void);
	// serial parsing related
	byte _buffer[BufferSize];

	BMParseState _parseState;
	byte _checksum;
	byte *_ptr;
	byte _frameSize;
	byte _sizeToRead;
	byte _messageCode;
	unsigned long _lastRcvTime;
	
	byte _ipAddress[4];
};

#endif






