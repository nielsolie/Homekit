//
// HAPPluginRF24DeviceDHT.cpp
// Homekit
//
//  Created on: 31.05.2020
//      Author: michael
//

#include "HAPPluginRF24DeviceDHT.hpp"
#include "HAPServer.hpp"
#include "HAPLogger.hpp"
#include "HAPPluginRF24.hpp"


HAPPluginRF24DeviceDHT::HAPPluginRF24DeviceDHT(){   
    name    = "";    
    id 		= 0;
    type    = RemoteDeviceTypeDHT;

    _accessory          = nullptr;
    _eventManager       = nullptr;  
    _fakegatoFactory    = nullptr;

	_batteryLevel       = nullptr;
    _batteryStatus      = nullptr;
}

HAPPluginRF24DeviceDHT::HAPPluginRF24DeviceDHT(uint16_t id_, String name_){
    
    name    = name_;    
    id 		= id_;
    type    = RemoteDeviceTypeDHT;

    _accessory          = nullptr;
    _eventManager       = nullptr;      
    _fakegatoFactory    = nullptr;

	_batteryLevel       = nullptr;
    _batteryStatus      = nullptr;
}




HAPAccessory* HAPPluginRF24DeviceDHT::initAccessory(){
    // Create accessory if not already created
    _accessory = new HAPAccessory();
    //HAPAccessory::addInfoServiceToAccessory(_accessory, "Builtin LED", "ACME", "LED", "123123123", &identify);
    auto callbackIdentify = std::bind(&HAPPluginRF24Device::identify, this, std::placeholders::_1, std::placeholders::_2);
    _accessory->addInfoService(name, "ACME", "RF24", String(id, HEX), callbackIdentify, "1.0");


    //
    // Battery service
    // 
    HAPService* batteryService = new HAPService(HAP_SERVICE_BATTERY_SERVICE);
    _accessory->addService(batteryService);

    // 
    // Battery level
    // 
    _batteryLevel = new intCharacteristics(HAP_CHARACTERISTIC_BATTERY_LEVEL, permission_read|permission_notify, 0, 100, 1, unit_percentage);
    _batteryLevel->setValue("0");
    auto callbackChangeBatLevel = std::bind(&HAPPluginRF24DeviceDHT::changeBatteryLevel, this, std::placeholders::_1, std::placeholders::_2);
    _batteryLevel->valueChangeFunctionCall = callbackChangeBatLevel;
    _accessory->addCharacteristics(batteryService, _batteryLevel);

    // 
    // Battery status
    // 
    _batteryStatus = new intCharacteristics(HAP_CHARACTERISTIC_STATUS_LOW_BATTERY, permission_read|permission_notify, 0, 1, 1, unit_none);
    _batteryStatus->setValue("0");
    auto callbackChangeBatStatus = std::bind(&HAPPluginRF24DeviceDHT::changeBatteryStatus, this, std::placeholders::_1, std::placeholders::_2);
    _batteryStatus->valueChangeFunctionCall = callbackChangeBatStatus;
    _accessory->addCharacteristics(batteryService, _batteryStatus);

    // 
    // Charging State
    // 
    intCharacteristics *chargingState = new intCharacteristics(HAP_CHARACTERISTIC_CHARGING_STATE, permission_read, 0, 2, 1, unit_none);
    chargingState->setValue("2");
    _accessory->addCharacteristics(batteryService, chargingState);


	//
	// Temperature
	//
	HAPService* temperatureService = new HAPService(HAP_SERVICE_TEMPERATURE_SENSOR);
	_accessory->addService(temperatureService);

	stringCharacteristics *tempServiceName = new stringCharacteristics(HAP_CHARACTERISTIC_NAME, permission_read, 0);
	tempServiceName->setValue("Temperature Sensor");

	_accessory->addCharacteristics(temperatureService, tempServiceName);

	//floatCharacteristics(uint8_t _type, int _permission, float minVal, float maxVal, float step, unit charUnit): characteristics(_type, _permission), _minVal(minVal), _maxVal(maxVal), _step(step), _unit(charUnit)
	_temperatureValue = new floatCharacteristics(HAP_CHARACTERISTIC_CURRENT_TEMPERATURE, permission_read|permission_notify, -50, 100, 0.1, unit_celsius);
	_temperatureValue->setValue("0.0");
	auto callbackChangeTemp = std::bind(&HAPPluginRF24DeviceDHT::changeTemp, this, std::placeholders::_1, std::placeholders::_2);
	//_temperatureValue->valueChangeFunctionCall = std::bind(&changeTemp);
	_temperatureValue->valueChangeFunctionCall = callbackChangeTemp;
	_accessory->addCharacteristics(temperatureService, _temperatureValue);

	//
	// Humidity
	//
	HAPService* humidityService = new HAPService(HAP_SERVICE_HUMIDITY_SENSOR);
	_accessory->addService(humidityService);

	stringCharacteristics *humServiceName = new stringCharacteristics(HAP_CHARACTERISTIC_NAME, permission_read, 0);
	humServiceName->setValue("Humidity Sensor");
	_accessory->addCharacteristics(humidityService, humServiceName);

	_humidityValue = new floatCharacteristics(HAP_CHARACTERISTIC_CURRENT_RELATIVE_HUMIDITY, permission_read|permission_notify, 0, 100, 0.1, unit_percentage);
	_humidityValue->setValue("0.0");

	auto callbackChangeHum = std::bind(&HAPPluginRF24DeviceDHT::changeHum, this, std::placeholders::_1, std::placeholders::_2);
	//_humidityValue->valueChangeFunctionCall = std::bind(&changeHum);
	_humidityValue->valueChangeFunctionCall = callbackChangeHum;
	_accessory->addCharacteristics(humidityService, _humidityValue);

	//
	// FakeGato
	// 		
	_fakegato.registerFakeGatoService(_accessory, name);    
	auto callbackAddEntry = std::bind(&HAPPluginRF24DeviceDHT::fakeGatoCallback, this);
	_fakegatoFactory->registerFakeGato(&_fakegato,  String("RF24 ") + String(id, HEX), callbackAddEntry);

    return _accessory;
}

void HAPPluginRF24DeviceDHT::setEventManager(EventManager* eventManager){
      
    _eventManager = eventManager;
    // Serial.printf("w event: %p\n", _eventManager);  
}


void HAPPluginRF24DeviceDHT::setFakeGatoFactory(HAPFakeGatoFactory* fakegatoFactory){
    
    _fakegatoFactory = fakegatoFactory;
    // Serial.printf("w fakegato: %p\n", _fakegatoFactory);
}   


void HAPPluginRF24DeviceDHT::changeTemp(float oldValue, float newValue) {
	Serial.printf("[RF24:%X] New temperature: %f\n", id, newValue);
}

void HAPPluginRF24DeviceDHT::changeHum(float oldValue, float newValue) {
	Serial.printf("[RF24:%X] New humidity: %f\n", id, newValue);
}



void HAPPluginRF24DeviceDHT::changeBatteryLevel(float oldValue, float newValue) {
	Serial.printf("[RF24:%X] New battery Level: %f\n", id, newValue);
}

void HAPPluginRF24DeviceDHT::changeBatteryStatus(float oldValue, float newValue) {
	Serial.printf("[RF24:%X] New battery status: %f\n", id, newValue);
}

// void HAPPluginRF24DeviceWeather::identify(bool oldValue, bool newValue) {
//     printf("Start Identify rf24: %d\n", id);
// }

bool HAPPluginRF24DeviceDHT::fakeGatoCallback(){	
	// return _fakegato.addEntry(_temperatureValue->value(), _humidityValue->value(), _pressureValue->value());
	return _fakegato.addEntry(_temperatureValue->value(), _humidityValue->value(), "0");
}



void HAPPluginRF24DeviceDHT::setValuesFromPayload(struct RadioPacket payload){

	LogD("Setting values for remote DHT device ...", false);
	_humidityValue->setValue(String(payload.humidity / 100.0));
	_temperatureValue->setValue(String(payload.temperature / 100.0));	

	_batteryLevel->setValue(String(payload.voltage));	
	_batteryStatus->setValue(payload.voltage < HAP_BATTERY_LEVEL_LOW_THRESHOLD ? "1" : "0" );


	{
		struct HAPEvent event = HAPEvent(nullptr, _accessory->aid, _humidityValue->iid, _humidityValue->value());		 					
		_eventManager->queueEvent( EventManager::kEventNotifyController, event);
	}

	{
		struct HAPEvent event = HAPEvent(nullptr, _accessory->aid, _temperatureValue->iid, _temperatureValue->value());							
		_eventManager->queueEvent( EventManager::kEventNotifyController, event);
	}

	{        
        struct HAPEvent event = HAPEvent(nullptr, _accessory->aid, _batteryLevel->iid, String(payload.voltage));
        _eventManager->queueEvent( EventManager::kEventNotifyController, event);        
    }
    
    {        
        struct HAPEvent event = HAPEvent(nullptr, _accessory->aid, _batteryStatus->iid, payload.voltage < HAP_BATTERY_LEVEL_LOW_THRESHOLD ? "1" : "0");
        _eventManager->queueEvent( EventManager::kEventNotifyController, event);        
    }


	LogD(" OK", true);
}