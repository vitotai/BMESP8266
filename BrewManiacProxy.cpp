#include "BrewManiacProxy.h"


#if DEBUG_ERROR == true
#define DPRINT_ERROR(s) DebugPort.print(s)
#define DPRINTLN_ERROR(s) DebugPort.println(s)
#else
#define DPRINT_ERROR(s) NULL
#define DPRINTLN_ERROR(s) NULL
#endif

// debug output
#if DEBUG_BMPROXY == true
#define DPRINT_BMPROXY(s) DebugPort.print(s)
#define DPRINTLN_BMPROXY(s) DebugPort.println(s)

void BMhex(unsigned char b)
{
	DebugPort.print(b>>4,HEX); 
	DebugPort.print(b&0xF,HEX);
}

#define DPRINT_BMPROXY_Hex(s) BMhex(s)
#else
#define DPRINT_BMPROXY(s) NULL
#define DPRINTLN_BMPROXY(s) NULL
#define DPRINT_BMPROXY_Hex(s) NULL
#endif

#if DEBUG_WIFIL2 == true
void WIFI2_hex(unsigned char b)
{
	DebugPort.print(b>>4,HEX); 
	DebugPort.print(b&0xF,HEX);
}


#define DPRINT_WIFIL2(s) DebugPort.print(s)
#define DPRINTLN_WIFIL2(s) DebugPort.println(s)
#define DPRINT_WIFIL2_Hex(s) WIFI2_hex(s)

#else
#define DPRINT_WIFIL2(s) NULL
#define DPRINT_WIFIL2_Hex(s) NULL
#define DPRINTLN_WIFIL2(s) NULL
#endif

#define CommInteger(hi,lo)  (((int)(hi))<<8) | ( (int)(lo) & 0xFF) 

#define StartRetransmissionTimer  _timerStartTime = millis()
#define StopRetransmissionTimer  _timerStartTime=0
#define IsRetransmissionTimeout(p) (_timerStartTime !=0 &&  (millis() - _timerStartTime) > p)
#define IsRetransmissionTimerRunning (_timerStartTime !=0)


#define TIME_KEEP_ALIVE 12000



BrewManiacProxy::BrewManiacProxy(void)
{
	_l2Init();
}

#if LambdaSupported == true
void BrewManiacProxy::begin(EventHandlerFunc handler,WriteFunc wfunc)
#else
void BrewManiacProxy::begin(void (*handler)(byte),void (*wfunc)(byte))
#endif
{
	_frameError=false; _frameErrorCount=0;
	
	_eventHandler=handler; 
	_write=wfunc;

	_bmState=BMSNull; 
	_parseState=WSWaitSOF;
	#if TEST_VALUE == true
	initTestValue();
	#endif
}


#if TEST_VALUE == true
static byte hops[10]={60,45,15,0,0,0,0,0,0,0};
static byte restTime[8]={1,0,0,0,0,0,60,10};
static float restTemp[8]={50,0,0,0,0,0,69,75};
static byte defaultEEPROM[EEPROMSize]=
{
//#define PS_UseGas   0  //	Use Gas
	0,
//#define PS_kP      1  // 	kP
  	100,
//#define PS_kI      2  //	kI
  	99,
//#define PS_kD      3  //     kD
  	0,
//#define PS_SampleTime      4  //     SampleTime
	10,
//#define PS_WindowSize      5  //     WindowSize
  	20,
//#define PS_BoilHeat      6    //   Boil Heat %
	88,
//#define PS_Offset     7      // Offset
  	0,
//#define PS_Hysteresi     8   //    Hysteresi 
	0,
	0, // 9 
//#define PS_TempUnit   10     //  Scale Temp
	0,
//#define PS_SensorType     11      // Sensor Type
  	0,
//#define PS_BoilTemp     12       //Temp Boil °C
	99,
//     13       Temp Boil °F
	0,
//#define PS_PumpCycle     14  //     Time Pump Cycle
	5,
//#define PS_PumpRest     15   //    Time Pump Rest
	0,
//#define PS_PumpPreMash     16  //     Pump PreMash
	0,
//#define PS_PumpOnMash     17   //    Pump on Mash
	1,
//#define PS_PumpOnMashOut     18  //     Pump on MashOut
	1,
//#define PS_PumpOnBoil      19     //  Pump on Boil
	0,
//#define PS_TempPumpRest     20    //   Temp Pump Rest °C
	105,
//     21       Temp Pump Rest °F
	0,
//#define PS_PidPipe     22     //  PID Pipe
	0,
//#define PS_SkipAddMalt     23  //     Skip Add Malt
	0,
//#define PS_SkipRemoveMalt     24  //     Skip Remove Malt
	0,
//#define PS_SkipIodineTest     25    //   Skip Iodine Test
	1,
//#define PS_IodineTime     26   //    Iodine Time
	0,
//#define PS_Whirlpool     27     //  Whirlpool  	
	0
};
void BrewManiacProxy::initTestValue(void)
{
	pumpStatus=1;
	heaterStatus=0;
 	isPwmOn=false;
	isTempReached=false;
	isPaused=false;
	isCountingUp=false;
	stage=101;
	currentTemperature=19.9;
	
	runningTimer=0;
	byte lastEvent=0;
	setPwm=80;
	setTemperature=33;
	_bmState=BMSConnected;


	automationRecipe.boilTime=66;
	automationRecipe.numberHops=3;
	for(byte i=0;i<10;i++)
		automationRecipe.hops[i]=hops[i];
	for(byte i=0;i<8;i++)
		automationRecipe.restTime[i]=restTime[i];
	for(byte i=0;i<8;i++)
		automationRecipe.restTemp[i]=restTemp[i];
	
	for(int i=0;i<EEPROMSize;i++)
		eeprom[i]=defaultEEPROM[i];


}
#endif

void BrewManiacProxy::handleConnectionConfirm(void)
{
	DPRINTLN_BMPROXY("handleConnectionConfirm");
	brewManiacVersion = _buffer[0];
	buttonLabel = _buffer[1];
	// connection done. get setting.
	sendPersistenceBlockRead();
	_bmState=BMSSyncSetting;

}
void BrewManiacProxy::handlePersistenceBlockData(void)
{
	DPRINTLN_BMPROXY("handlePersistenceBlockData");
	// copy content
	byte *p=_buffer;
	int size = _frameSize -1;
	int address = *p & 0x7F;
	if( *p & 0x80){
		size--;
		p++;
		address = address | (((int)*p) << 7);
	}
	p++;	

	DPRINT_BMPROXY("address:");
	DPRINT_BMPROXY(address);
	DPRINT_BMPROXY(" size:");
	DPRINTLN_BMPROXY(size);

	// for safety, check the data length
	if((address + size) > EEPROMSize) size = EEPROMSize -address;
	
	DPRINT_BMPROXY("DATA:");
	
	for(int i=0; i < size; i++){
		eeprom[address + i] = *p;
		DPRINT_BMPROXY_Hex(*p);
		p++;
	}
	DPRINTLN_BMPROXY("");

	if(_bmState == BMSSyncSetting){
		sendQueryRecipe();
		_bmState=BMSGetRecipe;
	}else{
	#if LambdaSupported == true
		if(_eventHandler) _eventHandler(BMNotifySettingChanged);
	#else
		if(_eventHandler) (*_eventHandler)(BMNotifySettingChanged);
	#endif
	}
}


bool BrewManiacProxy::isConnected(void)
{
	return (_bmState==BMSConnected);
}

void BrewManiacProxy::handleRecipeInformation(void) 
{
	DPRINTLN_BMPROXY("handleRecipeInformation");
	// copy mash information.
	DPRINT_BMPROXY("Data:");

	byte idx=0;
	byte stage;	
	do{
		byte stageByte= _buffer[idx];
		stage= stageByte & 0x0F;

		DPRINT_BMPROXY_Hex(_buffer[idx]);

		// assume stage <=7
		
		if((_buffer[idx] & 0xF0) == 0)
		{
			DPRINT_BMPROXY_Hex(_buffer[idx+1]);
			DPRINT_BMPROXY_Hex(_buffer[idx+2]);
			DPRINT_BMPROXY_Hex(_buffer[idx+3]);

			byte time=_buffer[idx+1];
			if(stage ==0) time=1;
			else if(stage >=6 && time==0) time=1;
			
			int temperature =CommInteger(_buffer[idx+2],_buffer[idx+3]);
  			automationRecipe.restTime[stage]=time;
  			automationRecipe.restTemp[stage]=TempFromStorage(temperature);
  			
  			idx +=4;
		}
		else
		{
			automationRecipe.restTime[stage]=0;
			automationRecipe.restTemp[stage]=0;
  			idx ++;
		}
	}while(idx < _frameSize && stage < 7);
		
	DPRINTLN_BMPROXY("$");
	//handleBoilHopInformation");
	// copy information.
	if(idx >= _frameSize) return; // for safety

	automationRecipe.boilTime= _buffer[idx];
	idx++;
  	byte hIdx=0;
  	while(idx < _frameSize)
  	{
    	automationRecipe.hops[hIdx]=_buffer[idx];
    	// write hop time
    	idx++;
    	hIdx++;
  	}
	automationRecipe.numberHops = hIdx;
	
	if(_bmState == BMSGetRecipe){
		_bmState=BMSConnected;
		#if LambdaSupported == true
		if(_eventHandler) _eventHandler(BMNotifyConnected);
		#else
		if(_eventHandler) (*_eventHandler)(BMNotifyConnected);
		#endif
	}else{
		#if LambdaSupported == true
		if(_eventHandler) _eventHandler(BMNotifyAutomationChanged);
		#else
		if(_eventHandler) (*_eventHandler)(BMNotifyAutomationChanged);
		#endif
	}

}

void BrewManiacProxy::handleStatus(void)
{
	//too much! DPRINTLN_BMPROXY("handleStatus");
	
	byte status = _buffer[0];
	pumpStatus =status & 0x3;
	heaterStatus = (status >>2) & 0x3;
 	isPwmOn=  (status >> 4) & 0x01;
	isTempReached=	(status >> 5) & 0x1;
	isPaused = (status >>6) & 0x1;
	isCountingUp =(status>>7) &0x1;
	
	stage = _buffer[1];
	currentTemperature =TempFromStorage(CommInteger(_buffer[2],_buffer[3]));
	runningTimer=CommInteger(_buffer[4],_buffer[5]);
	#if LambdaSupported == true
	if(_eventHandler) _eventHandler(BMNotifyStatusChanged);
	#else
	if(_eventHandler) (*_eventHandler)(BMNotifyStatusChanged);
	#endif
}

void BrewManiacProxy::handleEventNotification(void)
{
	DPRINTLN_BMPROXY("handleEventNotification");
	lastEvent=_buffer[0];
	#if LambdaSupported == true
	if(_eventHandler) _eventHandler(BMNotifyEvent);
	#else
	if(_eventHandler) (*_eventHandler)(BMNotifyEvent);
	#endif
}

void BrewManiacProxy::handleSetTemperature(void)
{
	DPRINTLN_BMPROXY("handleSetTemperature");
	setTemperature = TempFromStorage(CommInteger(_buffer[0], _buffer[1]));
	#if LambdaSupported == true
	if(_eventHandler) _eventHandler(BMNotifySetTemperature);
	#else
	if(_eventHandler) (*_eventHandler)(BMNotifySetTemperature);
	#endif
}

void BrewManiacProxy::handleSetPwm(void)
{
	DPRINTLN_BMPROXY("handleSetPwm");
	setPwm = _buffer[0];
	#if LambdaSupported == true
	if(_eventHandler) _eventHandler(BMNotifySetPwm);
	#else
	if(_eventHandler) (*_eventHandler)(BMNotifySetPwm);
	#endif
}

void BrewManiacProxy::handleButtonLabel(void)
{
	DPRINTLN_BMPROXY("handleButtonLabel");
	buttonLabel = _buffer[0];
	#if LambdaSupported == true
	if(_eventHandler) _eventHandler(BMNotifyButtonLabel);
	#else
	if(_eventHandler) (*_eventHandler)(BMNotifyButtonLabel);
	#endif
}

void BrewManiacProxy::handlePersistenceValue(void)
{
	DPRINTLN_BMPROXY("handlePersistenceValue");

	byte *p=_buffer;
	int address = *p & 0x7F;
	if( *p & 0x80){
		p++;
		address = address | (((int)*p) << 7);
	}
	p++;	

	// for safety, check the data length
	if(address  > EEPROMSize) return;
	eeprom[address ] = *p;
	#if LambdaSupported == true
	if(_eventHandler) _eventHandler(BMNotifySettingChanged);
	#else
	if(_eventHandler) (*_eventHandler)(BMNotifySettingChanged);
	#endif
}

void BrewManiacProxy::_l2SendAck(void)
{
	_txBuffer[0]=NullPDU(WiControllerAck);
	_sendFrame(1,false);
}

void BrewManiacProxy::BrewManiacProxy::processMsg(void)
{
	bool inSequence = false;
	if(_isResponseFrame){
		DPRINTLN_BMPROXY("response frame");
		StopRetransmissionTimer;
		_l2DiscardCurrentPdu();
		_l2SendNumber= RoundSequenceNumber( _l2SendNumber +1);
		// send next packet? if any?
	}else{
		DPRINTLN_BMPROXY("indication frame");
		_l2SendAck();
		if(_packetSequence == _l2RcvNumber){
			_l2RcvNumber= RoundSequenceNumber(_l2RcvNumber +1);
			inSequence=true;
		}
	}
	
	if(_messageCode == WiConectionConfirm && _isResponseFrame){
		_l2SendNumber=0;
		_l2RcvNumber =0;
		BrewManiacProxy::handleConnectionConfirm();
	}else if(_messageCode == WiPersistenceBlockData && _isResponseFrame){
		BrewManiacProxy::handlePersistenceBlockData();

	}else if(_messageCode == WiStatus){
		if(inSequence){
			BrewManiacProxy::handleStatus();
		}
	}else if(_messageCode == WiEventNotification){
		if(inSequence){
			BrewManiacProxy::handleEventNotification();
		}		
	}else if(_messageCode == WiSetTemperature){
		if(inSequence){
			BrewManiacProxy::handleSetTemperature();
		}
	}else if(_messageCode == WiSetPwm){
		if(inSequence){
			BrewManiacProxy::handleSetPwm();
		}
	}else if(_messageCode == WiPersistenceValue){
		if(!_isResponseFrame){
			if(inSequence){
				BrewManiacProxy::handlePersistenceValue();
			}
		}else{
			BrewManiacProxy::handlePersistenceValue();
		}
	}else if(_messageCode == WiRecipeInformation){
		if(!_isResponseFrame){
			if(inSequence){
				BrewManiacProxy::handleRecipeInformation();
			}
		}else{
			BrewManiacProxy::handleRecipeInformation();
		}
	}else if(_messageCode == WiButtonLabel){
		BrewManiacProxy::handleButtonLabel();
	}
}
	
void BrewManiacProxy::loop(void)
{
	bool timeout=false;
	unsigned long current=millis();
	if(IsRetransmissionTimeout(RetryTimer)){
		//timeout
		DPRINTLN_ERROR("Timeout!");
		timeout=true;
		StopRetransmissionTimer;
	}

	if(_bmState == 	BMSNull || _bmState ==BMSDisconnected){
		return;
	}else if(_bmState == 	BMSConnected
		|| _bmState == 	BMSConnecting
		|| _bmState == 	BMSSyncSetting
		|| _bmState == 	BMSGetRecipe){
		if(timeout){
			if(_retryCounter < MaxRetry){
				resendLastFrame();
				_retryCounter ++;
				StartRetransmissionTimer;
			}else{
				
				DPRINTLN_ERROR("failure on retry!");
				BMDisc();
			}
		}
		if(_bmState == 	BMSConnected 
			&&  (current - _lastRcvTime) > TIME_KEEP_ALIVE){
				DPRINT_ERROR("Too long without status, last update:");
				DPRINT_ERROR(_lastRcvTime);
				DPRINT_ERROR(" now:");
				DPRINTLN_ERROR(current);

				BMDisc();
		}
	}
}

void BrewManiacProxy::connect(void)
{
	if(_bmState==BMSConnecting) return;
	sendConnectRequest();
	_bmState=BMSConnecting;
}
	
void BrewManiacProxy::read(byte ch)
{
		DPRINT_WIFIL2("BrewManiacProxy::read ch=");
		DPRINT_WIFIL2_Hex(ch);
		DPRINT_WIFIL2(" _parseState=");
		DPRINTLN_WIFIL2(_parseState);
		
		if(_parseState == WSWaitSOF){
			if(ch == StartOfFrame){
				_parseState = WSWaitSequence;
				_frameError = false;
			}else{
				if(!_frameError){
					_frameError = true;
					_frameErrorCount ++;
					DPRINT_ERROR("non SOF:");
					DPRINTLN_ERROR(ch);
				}
			}
		}else if(_parseState == WSWaitSequence){
			_packetSequence=PacketSequence(ch);
			_isResponseFrame=IsAckResponse(ch);
			_parseState = WSMessageCode;
			_checksum = ch;
		}else if(_parseState == WSMessageCode){
			_checksum ^= ch;
			_messageCode = WiMessageCode(ch);
			if(IsNullPDU(ch)){
				_parseState=WSChecksum;
			}else if(IsOnePDU(ch)){
				_sizeToRead=_frameSize = 1;
				_parseState = WSData;
			}else if(IsVarPDU(ch)){
				_parseState = WSLength;
			}
			_ptr=_buffer;
			
		}else if(_parseState == WSLength){
			_checksum ^= ch;
			_sizeToRead=_frameSize = ch;
			if(_frameSize > BufferSize) _parseState=WSWaitSOF;
			else _parseState=WSData;
		}else if(_parseState == WSData){
			_checksum ^= ch;
			*_ptr = ch;
			_ptr ++;
			_sizeToRead --;
			if(_sizeToRead ==0){
				_parseState=WSChecksum;
			}
		}else /*if(_wiState == WSChecksum)*/{
			if(ch == _checksum){
				_parseState = WSWaitSOF;
				_lastRcvTime = millis();
				BrewManiacProxy::processMsg();
			}else{
				_parseState = WSWaitSOF;
				DPRINT_ERROR("!Checksum error\n");
				_frameErrorCount ++;
			}
		}
	
}

void BrewManiacProxy::sendPersistenceBlockRead(void)
{
	_txBuffer[0]=VarPDU(WiPersistenceBlockRead);
	_txBuffer[1]=2;
	_txBuffer[2]=0;
	_txBuffer[3]=EEPROMSize;
	_sendFrame(4,true);
}


void BrewManiacProxy::sendQueryRecipe(void)
{
	_txBuffer[0]=NullPDU(WiQueryRecipe);
	_sendFrame(1,true);
}



void BrewManiacProxy::sendConnectRequest(void)
{
	_txBuffer[0]=VarPDU(WiConnectRequest);
	_txBuffer[1]=1;
	_txBuffer[2]=REPORT_PERIOD;
	_sendFrame(3,true);
}


void BrewManiacProxy::sendButton(byte mask,bool longPressed)
{
	_txBuffer[0]=OnePDU(WiButtonPressed);
	_txBuffer[1] = (mask & 0xF) | ((longPressed)? 0x10:0);
	_sendFrame(2,true);
}

void BrewManiacProxy::sendAutomationRecipe(AutomationRecipe *recipe)
{
	DPRINTLN_BMPROXY("sendAutomationRecipe");
	
	_txBuffer[0]=VarPDU(WiSetRecipe);
	int  idx=2;	// index 1 is for length
	for(int i=0;i<8;i++){
		if(recipe->restTime[i] != 0){
			_txBuffer[idx++]=  i;
			_txBuffer[idx++]= recipe->restTime[i];
			int t=  ToTempInStorage(recipe->restTemp[i]);
			_txBuffer[idx++]= t>>8;
			_txBuffer[idx++]= t & 0xFF;
		}else{
			_txBuffer[idx++]= 0xF0 | i;
		}
	}
	_txBuffer[idx] = recipe->boilTime;
	
	idx++;
	for(int i=0;i < recipe->numberHops;i++){
		_txBuffer[idx++]=recipe->hops[i];
	}

	// length
	_txBuffer[1] = idx -2;
	_sendFrame(idx,true);
}



void BrewManiacProxy::setIp(byte ip[])
{
	if(ip ==0 || (ip[0] == 0 && ip[1]==0 && ip[2] ==0  && ip[3] ==0))
	{
		_txBuffer[0]=OnePDU(WiDeviceAddress);
		_txBuffer[1]= 0; // clear
		_sendFrame(2,true);
	}
	else
	{
		_txBuffer[0]=VarPDU(WiDeviceAddress);
		_txBuffer[1]= 5;
		_txBuffer[2] = 0x1; // type
		byte idx=3;
		for(int i=0;i < 4;i++){
			_txBuffer[idx++]=ip[i];
		}
		_sendFrame(idx,true);
	}
}


void BrewManiacProxy::sendUpdateSetting(int number,int addresses[],byte values[])
{
	DPRINTLN_BMPROXY("sendUpdateSetting");
	_txBuffer[0]=VarPDU(WiPersistenceSet);
	int idx=2;
	
	for(int i=0;i<number;i++)
	{
		if(addresses[i] > 0x7F){
			_txBuffer[idx++] = (addresses[i] & 0x7F)|0x80;
			_txBuffer[idx++] = addresses[i] >>7;
		}else{
			_txBuffer[idx++] = addresses[i];
		}
		_txBuffer[idx++] = values[i];
	}
	_txBuffer[1] = idx-2;
	_sendFrame(idx,true);
}

byte BrewManiacProxy::getSetting(int addr)
{
	return eeprom[addr];
}

void BrewManiacProxy::_sendResponseFromBuffer(int len)
{
	DPRINT_WIFIL2("TX:");
	#if LambdaSupported == true
	_write(StartOfFrame);
	#else
	(*_write)(StartOfFrame);
	#endif

	byte checksum=0;

	// sequence number
	byte sqn=ResponseSequence(_l2RcvNumber);
	#if LambdaSupported == true
	_write(sqn);
	#else
	(*_write)(sqn);
	#endif
	checksum ^= sqn;

	for(int i=0;i<len;i++)
	{
		checksum ^= _txBuffer[i];
		#if LambdaSupported == true
		_write(_txBuffer[i]);
		#else
		(*_write)(_txBuffer[i]);
		#endif

		DPRINT_WIFIL2_Hex(_txBuffer[i]);
	}
	#if LambdaSupported == true
	_write(checksum);
	#else
	(*_write)(checksum);
	#endif
	DPRINT_WIFIL2_Hex(checksum);
	DPRINTLN_WIFIL2("");
}

void BrewManiacProxy::_l2SendFromQueueBuffer(void)
{
	DPRINT_WIFIL2("_l2SendFromQueueBuffer, _l2CurrentPduPtr= ");
	DPRINT_WIFIL2(_l2CurrentPduPtr);
	DPRINT_WIFIL2("_l2WritePtr=");
	DPRINTLN_WIFIL2(_l2WritePtr);
	
	if(_l2CurrentPduPtr == _l2WritePtr) return; // empty
	// check current pdu type
	byte ptr = _l2CurrentPduPtr;
	
	byte checksum;
	// start sending
	#if LambdaSupported == true
	_write(StartOfFrame);
	#else
	(*_write)(StartOfFrame);
	#endif
	// sequence number
	byte sqn=RequestIndicationSequence(_l2SendNumber);
	#if LambdaSupported == true
	_write(sqn);
	#else
	(*_write)(sqn);
	#endif
	checksum = sqn;
	
	DPRINT_WIFIL2_Hex(sqn);
	
	// message header
	byte ch = _l2QueueBuffer[ptr];
	ptr++;
	if(ptr == QueueBufferSize) ptr =0;	
	checksum ^= ch;
	#if LambdaSupported == true
	_write(ch);
	#else
	(*_write)(ch);
	#endif
	DPRINT_WIFIL2_Hex(ch);

	// length, if any
	byte len;
	if(IsNullPDU(ch)){
		len=0;
	}else if(IsOnePDU(ch)){
		len = 1;
	}else if(IsVarPDU(ch)){
		len = _l2QueueBuffer[ptr++];
		if(ptr == QueueBufferSize) ptr =0;

		#if LambdaSupported == true
		_write(len);
		#else
		(*_write)(len);
		#endif
		checksum ^= len;
		DPRINT_WIFIL2_Hex(len);
	}


	for(int i=0;i<len;i++)
	{
		byte data=_l2QueueBuffer[ptr];
		ptr ++; 
		if(ptr == QueueBufferSize) ptr =0;
		
		checksum ^= data;
		#if LambdaSupported == true
		_write(data);
		#else
		(*_write)(data);
		#endif

		DPRINT_WIFIL2_Hex(data);
	}
	#if LambdaSupported == true
	_write(checksum);
	#else
	(*_write)(checksum);
	#endif
	DPRINT_WIFIL2_Hex(checksum);
	DPRINTLN_WIFIL2("");

}


void BrewManiacProxy::_l2DiscardCurrentPdu(void)
{
	// check current pdu type
	byte ptr = _l2CurrentPduPtr;
	byte ch = _l2QueueBuffer[ptr++];
	if(ptr == QueueBufferSize) ptr =0;
	
	byte size;

	if(IsNullPDU(ch)){
		size=0;
	}else if(IsOnePDU(ch)){
		size = 1;
	}else if(IsVarPDU(ch)){
		size = _l2QueueBuffer[ptr++];
		// this is redundant
		if(ptr == QueueBufferSize) ptr =0;
	}
	ptr += size;
	if(ptr >= QueueBufferSize) ptr -=QueueBufferSize;
	_l2CurrentPduPtr=ptr;
}

void BrewManiacProxy::BMDisc(void)
{
	DPRINTLN_BMPROXY("Disconnected from BM");
	_bmState = BMSDisconnected;
	// reset the queuw
	_l2WritePtr = _l2CurrentPduPtr = 0;
	#if LambdaSupported == true
	if(_eventHandler) _eventHandler(BMNotifyDisconnected);
	#else
	if(_eventHandler) (*_eventHandler)(BMNotifyDisconnected);
	#endif
}

void BrewManiacProxy::_copyToQueueBuf(byte len)
{	
	DPRINT_WIFIL2("_copyToQueueBuf, _l2WritePtr:");
	DPRINTLN_WIFIL2(_l2WritePtr);
	
	for(int i=0;i<len;i++)
	{
		_l2QueueBuffer[_l2WritePtr] = _txBuffer[i];

		DPRINT_WIFIL2_Hex(_txBuffer[i]);		
		
		_l2WritePtr++;
		if(_l2WritePtr == QueueBufferSize) _l2WritePtr=0;
		if(_l2WritePtr == _l2CurrentPduPtr){
			// overlap. problem here
			// either NO response from peer
			// or the queue buffer is too small
			DPRINTLN_WIFIL2("Fatal Error: queue buffer overlap");
			BMDisc();
//			_l2DiscardCurrentPdu();
			// increase sendnumber
//			_l2SendNumber = RoundSequenceNumber(_l2SendNumber +1);
		}
	}
//	DPRINTLN_WIFIL2("End _copyToQueueBuf");
}


void BrewManiacProxy::_sendFrame(int len,bool requestAck)
{
	if(requestAck){
		_copyToQueueBuf(len);
		
		if(! IsRetransmissionTimerRunning)
		{
			_l2SendFromQueueBuffer();
			_retryCounter =0;
			StartRetransmissionTimer;
		}
	}else{
		_sendResponseFromBuffer(len);
	}
}

void BrewManiacProxy::resendLastFrame(void)
{
	_l2SendFromQueueBuffer();
}



