//
// HAPPluginHygrometer.hpp
// Homekit
//
//  Created on: 22.04.2018
//      Author: michael
//

#ifndef HAPPLUGINHYGROMETER_HPP_
#define HAPPLUGINHYGROMETER_HPP_

#include <Arduino.h>
#include "HAPPlugins.hpp"
#include "HAPLogger.hpp"
#include "HAPAccessory.hpp"

#include "HAPFakeGato.hpp"
#include "HAPFakeGatoWeather.hpp"
#include "HAPCustomCharacteristics+Services.hpp"

#ifndef HAP_PLUGIN_HYGROMETER_USE_DUMMY
#define HAP_PLUGIN_HYGROMETER_USE_DUMMY 	0
#endif



class HAPPluginHygrometer: public HAPPlugin {
public:

	HAPPluginHygrometer();

	bool begin();
	HAPAccessory* initAccessory() override;
	
	void setValue(int iid, String oldValue, String newValue);	
	

	void changeHum(float oldValue, float newValue);




	void identify(bool oldValue, bool newValue);
    void handleImpl(bool forced = false);	
	
	HAPConfigValidationResult validateConfig(JsonObject object);
	JsonObject getConfigImpl();
	void setConfigImpl(JsonObject root);
	// void handleEvents(int eventCode, struct HAPEvent eventParam);
private:

	floatCharacteristics*	_humidityValue;

#if HAP_HYGROMETER_LEAK_SENSOR_ENABLED	
	uint8Characteristics*	_leakSensor;	
#endif

	bool _leakSensorEnabled;
	
	bool fakeGatoCallback();

	HAPFakeGatoWeather _fakegato;

    uint16_t readSensor();

};

REGISTER_PLUGIN(HAPPluginHygrometer)

#endif /* HAPPLUGINHYGROMETER_HPP_ */ 