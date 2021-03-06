
// 
// Platformio is using SpenceKonde's ATTinyCore
// https://github.com/SpenceKonde/ATTinyCore
// 
// 
// ATMEL ATTINY 25/45/85 / ARDUINO
//
//                   +-\/-+
//  Ain0 (D 5) PB5  1|    |8  Vcc
//  Ain3 (D 3) PB3  2|    |7  PB2 (D 2) Ain1
//  Ain2 (D 4) PB4  3|    |6  PB1 (D 1) pwm1
//             GND  4|    |5  PB0 (D 0) pwm0
//                   +----+
// 
// 
// Power Saving Tips
// 
// - add a capacitor of 100-200µF in parallel with your battery. 
//   Ceramic is better, but I have no problem with nodes using electrolytic capacitors. 
//   This will help when there is a peak power consumption from the radio. 
//   If you do not put one, voltage will drop quickly and that's probably what is triggering reboot loop 
//   on one of your nodes: maybe radio is less efficient and needs to resend more messages. 
// 
// - I've noticed that power usage on the nRF modules shoots up if I leave any inputs (CSN, SCK, MOSI) floating.  
//   With my circuit above CSN will never float, but SCK and MOSI should be pulled high or low when not in use, 
//   especially for battery-powered devices.
// 
// - For low power applications, before entering sleep, remember to turn off the ADC 
//   (ADCSRA&=(~(1<<ADEN))) - otherwise it will waste ~270uA
// 
// Gotchas:
// 
// - You cannot use the Pxn notation (ie, PB2, PA1, etc) to refer to pins 
//   these are defined by the compiler-supplied headers, and not to what an arduino user would expect. 
//   To refer to pins by port and bit, use PIN_xn (ex, PIN_B2); these are #defined to the Arduino pin number 
//   for the pin in question, and can be used wherever digital pin numbers can be used
// 
// - All ATTiny chips (as well as the vast majority of digital integrated circuits) require a 0.1uF ceramic capacitor 
//   between Vcc and Gnd for decoupling; this should be located as close to the chip as possible 
//   (minimize length of wires to cap). Devices with multiple Vcc pins, or an AVcc pin, 
//   should use a cap on those pins too. Do not be fooled by poorly written tutorials or guides that omit these. 
//   Yes, I know that in some cases (ex, the x5 series) the datasheet doesn't mention these 
//   but other users as well as myself have had problems when it was omitted on a t85.
// 
// - When using I2C on anything other than the ATTiny48/88 you must use an I2C pullup resistor 
//   on SCL and SDA (if there isn't already one on the I2C device you're working with 
//   many breakout boards include them). 4.7k or 10k is a good default value. 
//   On parts with real hardware I2C, the internal pullups are used, and this is sometimes good enough 
//   to work without external pullups; this is not the case for devices without hardware I2C 
//   (all devices supported by this core except 48/88) - the internal pullups can't be used here, 
//   so you must use external ones. That said, for maximum reliability, you should always use external pullups,
//   even on the t48/88, as the internal pullups are not as strong as the specification requires.
// 
// 
//  Pinout ATTINY85
//                   +-\/-+
//  BME280 CS   PB5  1|    |8  Vcc
//  RF24 CE     PB3  2|    |7  PB2  SCK
//  RF24 CSN    PB4  3|    |6  PB1  MISO
//              GND  4|    |5  PB0  MOSI
//                    +----+
// 
//  ATtiny25/45/85 Pin map with CE_PIN 3 and CSN_PIN 3 => PB3 and PB4 are free to use for application
//  Circuit idea from http://nerdralph.blogspot.ca/2014/01/nrf24l01-control-with-3-attiny85-pins.html
//  Original RC combination was 1K/100nF. 22K/10nF combination worked better.
//  For best settletime delay value in RF24::csn() the timingSearch3pin.ino scatch can be used.
//  This configuration is enabled when CE_PIN and CSN_PIN are equal, e.g. both 3
//  Because CE is always high the power consumption is higher than for 5 pins solution
//                                                                                          ^^
//                               +-\/-+           nRF24L01   CE, pin3 ------|              //
//                         PB5  1|o   |8  Vcc --- nRF24L01  VCC, pin2 ------x----------x--|<|-- 5V
//                 NC      PB3  2|    |7  PB2 --- nRF24L01  SCK, pin5 --|<|---x-[22k]--|  LED
//                 NC      PB4  3|    |6  PB1 --- nRF24L01 MOSI, pin6  1n4148 |
//  nRF24L01 GND, pin1 -x- GND  4|    |5  PB0 --- nRF24L01 MISO, pin7         |
//                      |        +----+                                       |
//                      |-----------------------------------------------||----x-- nRF24L01 CSN, pin4 
//                                                                     10nF

#include <Arduino.h>
#include <SPI.h>

#include <avr/eeprom.h>
#include <avr/power.h>      // Power management
#include <avr/sleep.h>      // Sleep Modes
#include <avr/wdt.h>        // Watchdog timer

#include "RF24.h"

const char FIRMWARE_VERSION[6] = "1.0.9";

// #define DEBUG       
#define EEPROM_SETTINGS_VERSION 1
#define WATCHDOG_WAKEUPS_TARGET 1                

#ifndef DEVICE_ID
#define DEVICE_ID               0x12
#endif


#ifdef USE_BATTERY_CHECK_INTERVAL
    #define BAT_CHECK_INTERVAL      5        // after how many data collections we should get the battery status
    uint16_t batteryVoltage = 0;
    uint8_t batteryCheckCounter = BAT_CHECK_INTERVAL;
#endif


// 
// RF24
// 
#ifndef RF24_ADDRESS
    #define RF24_ADDRESS        "HOMEKIT_RF24"
#endif 

#ifndef RF24_ADDRESS_SIZE
    #define RF24_ADDRESS_SIZE   13
#endif

#ifndef RF24_PA_LEVEL
    #define RF24_PA_LEVEL       RF24_PA_HIGH
#endif

#ifndef RF24_DATA_RATE
    #define RF24_DATA_RATE      RF24_1MBPS
#endif

// RF24 Address
#ifndef RF24_ID
    #define RF24_ID DEVICE_ID
#endif


// 
// Pins
// 

#if ATTINY

#include <SoftwareSerial.h>
SoftwareSerial softSerial(99, PIN_B4); // RX, TX


// 
// BME280
// 
#if ATTINY_USE_BME280
#define TINY_BME280_SPI
#include <TinyBME280.h>
#define BME280_CS_PIN   PIN_B5  // PB5 = RESET
#endif /* ATTINY_USE_BME280 */


#ifndef WDTCSR
#define WDTCSR WDTCR
#endif


#define RF24_CE_PIN     PIN_B5  // PB3
#define RF24_CSN_PIN    PIN_B3  // NOPE -> Since we are using 3 pin configuration we will use same pin for both CE and CSN

#else

#ifndef UNO
#define UNO 1
#endif

#define RF24_CE_PIN     9  
#define RF24_CSN_PIN    10  

#endif /* ATTINY */


// 
// EEPRom Macros
// 
#define eepromBegin() eeprom_busy_wait(); noInterrupts() // Details on https://youtu.be/_yOcKwu7mQA
#define eepromEnd()   interrupts()



// Utility macro
#define adc_enable()  (ADCSRA |= (1 << ADEN))
#define adc_disable() (ADCSRA &= ~(1 << ADEN)) // disable ADC (before power-off)


// *****************************************************************************************************************************
// 
// Structs
// 
// *****************************************************************************************************************************

struct __attribute__((__packed__)) EepromSettings
{
    uint16_t    radioId;
    uint8_t     sleepInterval;
    uint8_t     measureMode;
    uint8_t     version;
    char        firmware_version[6];
};

struct __attribute__((__packed__)) RadioPacket
{
    uint16_t    radioId;
    uint8_t     type;
    
    int32_t     temperature;    // temperature
    uint32_t    humidity;       // humidity
    uint32_t    pressure;       // pressure
    
    uint16_t    voltage;        // voltage * 100 , e.g. 330 for 3.3 V
};


enum RemoteDeviceType {
    RemoteDeviceTypeWeather    = 0x01,
    RemoteDeviceTypeDHT        = 0x02,
};


enum MeasureMode
{
    MeasureModeWeatherStation   = 0x00, // = measureWeather 0x01,
    MeasureModeIndoor           = 0x01, // = measureIndoor  0x11,
};


enum ChangeType
{
    ChangeTypeNone              = 0x00,
    ChangeRadioId               = 0x01,
    ChangeSleepInterval         = 0x02,
    ChangeMeasureType           = 0x03,
};


struct NewSettingsPacket
{
    uint8_t         changeType; // enum ChangeType
    uint16_t        forRadioId;
    uint16_t        newRadioId;
    uint8_t         newSleepInterval;
    uint8_t         newMeasureMode;
};




// *****************************************************************************************************************************
// 
// Prototyps
// 
// *****************************************************************************************************************************

uint16_t readVcc();

void enterSleep();
void setupWatchDogTimer();
void resetWatchDog();


bool setupRadio();

void processSettingsChange(NewSettingsPacket newSettings); 
void saveSettings();


// *****************************************************************************************************************************
// 
// Variables
// 
// *****************************************************************************************************************************

// uint8_t saveADCSRA;                 // variable to save the content of the ADC for later. if needed.
volatile uint8_t counterWD = 0;     // Count how many times WDog has fired. Used in the timing of the 
                                    // loop to increase the delay before the LED is illuminated. For example,
                                    // if WDog is set to 1 second TimeOut, and the counterWD loop to 10, the delay
                                    // between LED illuminations is increased to 1 x 10 = 10 seconds


RF24            _radio(RF24_CE_PIN, RF24_CSN_PIN);
uint8_t         address[RF24_ADDRESS_SIZE] = RF24_ADDRESS;
EepromSettings  _settings;

#if ATTINY
#if ATTINY_USE_BME280
tiny::BME280    _bme280;
#endif
#endif

// *****************************************************************************************************************************
// 
// Read VCC
// 
// *****************************************************************************************************************************

uint16_t readVcc()
{
    adc_enable();  // enable ADC
    // Details on http://provideyourown.com/2012/secret-arduino-voltmeter-measure-battery-voltage/

    ADMUX = _BV(MUX3) | _BV(MUX2); // Select internal 1.1V reference for measurement.
    delay(2);                      // Let voltage stabilize.
    ADCSRA |= _BV(ADSC);           // Start measuring.
    while (ADCSRA & _BV(ADSC));    // Wait for measurement to complete.
    uint16_t adcReading = ADC;
    uint16_t vcc = (1.1 * 1024.0 / adcReading) * 100; // Note the 1.1V reference can be off +/- 10%, so calibration is needed.

    adc_disable();  // disable ADC

#if ATTINY
    return vcc;
#else    
    return 3440;
#endif    

}


// *****************************************************************************************************************************
// 
// Setup
// 
// *****************************************************************************************************************************
    
void setup() {

#if UNO
    Serial.begin(9600);
    Serial.println("Starting NRF24 Test...");
#else 
    softSerial.begin(9600);
    softSerial.println(F("Starting ATTiny Sleep Test"));       
#endif
    // Disable Analog Digital converter
    adc_disable();  // disable ADC

    // Load settings from eeprom.
    eepromBegin();

#if RESET_EEPROM    
    eeprom_write_block(0xFF, 0, sizeof(_settings));
#endif // RESET_EEPROM

    
    eeprom_read_block(&_settings, 0, sizeof(_settings));
    eepromEnd();

    if (_settings.version != EEPROM_SETTINGS_VERSION) {
        // The settings version in eeprom is not what we expect, so assign default values.
        _settings.radioId = RF24_ID;        
        _settings.sleepInterval = WATCHDOG_WAKEUPS_TARGET;        
        _settings.measureMode = MeasureModeWeatherStation;        
        _settings.version = EEPROM_SETTINGS_VERSION;
        strncpy(_settings.firmware_version, FIRMWARE_VERSION, 5);
        _settings.firmware_version[5] = '\0';

        saveSettings();
    }

#if ATTINY

#if ATTINY_USE_BME280
    // set slave select pins BME280 as outputs
    pinMode(BME280_CS_PIN, OUTPUT);
  
    // set BME280_CS_PIN 
    digitalWrite(BME280_CS_PIN, HIGH);


    // init BME280
    _bme280.beginSPI(BME280_CS_PIN); // Start using SPI and CS Pin 10

    _bme280.setTempOverSample(1);
    _bme280.setPressureOverSample(1);
    _bme280.setHumidityOverSample(1);

    _bme280.setMode(tiny::Mode::SLEEP);
    
    _bme280.setFilter(0);
#endif
#endif

#if UNO
    pinMode(LED_BUILTIN, OUTPUT);
#endif


    setupWatchDogTimer(); // approximately 8 seconds sleep
}



// *****************************************************************************************************************************
// 
// Loop
// 
// *****************************************************************************************************************************

void loop() {    
    

    if ( counterWD >= _settings.sleepInterval ) {

        counterWD = 0;

        RadioPacket radioData;
        
        radioData.type = RemoteDeviceTypeWeather;
        radioData.radioId = _settings.radioId;


#if ATTINY_USE_BME280
     
        _bme280.setMode(tiny::Mode::FORCED); //Wake up sensor and take reading

        radioData.temperature   = _bme280.readFixedTempC();
        radioData.pressure      = _bme280.readFixedPressure();
        radioData.humidity      = _bme280.readFixedHumidity();

        _bme280.setMode(tiny::Mode::SLEEP);
#else   
        radioData.temperature   = 1100;
        radioData.pressure      = 12000;
        radioData.humidity      = 1300;
#endif

#if USE_BATTERY_CHECK_INTERVAL
        batteryCheckCounter++;

        if (batteryCheckCounter >= 5) {
            batteryVoltage = readVcc();
            batteryCheckCounter = 0;
        }
        
        radioData.voltage = batteryVoltage;
#else
        radioData.voltage = readVcc();
#endif
        

        // _radio.powerUp();
        // delay(5);

#if UNO
        Serial.print("Setup Radio ...");
#else    
      softSerial.print(F("Setup Radio ..."));
           
#endif

        // Re-initialize the radio.               
        if (setupRadio()){

#if UNO            
            Serial.println("OK");
            Serial.print("Send values ...");
#else 
            softSerial.println("OK");
            softSerial.print("Send values ...");
#endif            
            if (!_radio.write( &radioData, sizeof(RadioPacket) ) ) { //Send data to 'Receiver' every x seconds                        
#if UNO          
                Serial.println("FAILED");
#else
                softSerial.println("FAILED");
#endif                
            } else {
#if UNO                
                Serial.println("OK");      
#else
                softSerial.println("OK");
#endif
                if ( _radio.isAckPayloadAvailable() ) {
#if UNO
                    Serial.print("Acknowledgement available for radio Id: ");
#else
                    softSerial.print("Acknowledgement available for radio Id: ");                    
#endif
                    NewSettingsPacket newSettingsData;
                    _radio.read(&newSettingsData, sizeof(NewSettingsPacket));

#if UNO                    
                    Serial.println(newSettingsData.forRadioId, HEX);
#else
                    softSerial.println(newSettingsData.forRadioId, HEX);
#endif
                    if (newSettingsData.forRadioId == _settings.radioId) {

#if UNO
                        Serial.print("Process settings ...");
#else
                        softSerial.print("Process settings ...");
#endif
                        processSettingsChange(newSettingsData);        
#if UNO                        
                        Serial.println("OK");
#else
                      softSerial.println("OK");
#endif
#if UNO
                        // Send new updated settings
                        Serial.print("Send updated settings ...");
#else
                      softSerial.print("Send updated settings ...");
#endif
                        if (!_radio.write( &_settings, sizeof(EepromSettings) ) ) { //Send data to 'Receiver' every x seconds                                
#if UNO                        
                            Serial.println("FAILED");
#else
                          softSerial.println("FAILED");
#endif                            
                        } 
#if UNO                        
                        else {
                            Serial.println("OK");                           
                        }

#else                   
                        else {
                          softSerial.println("OK");                         
                        }
#endif                        
                    }           
                } 
#if UNO
                else {
                    Serial.println("No ack available");
                }
#else 
                else {
                  softSerial.println("No ack available");
                }                
#endif                
            }
        } 
#if UNO        
        else {
            Serial.println("FAILED");
        }   
#else
        else {
          softSerial.println("FAILED");
        }                                
#endif    
        _radio.powerDown();                  // Put the radio into a low power state.        
        
        // Put the SPI pins to low for energy saving
        // SPI.end();              

        
    }

#if UNO   
	// Toggle the LED on
	digitalWrite(LED_BUILTIN, 1);
	// wait
	delay(20);
	// Toggle the LED off
	digitalWrite(LED_BUILTIN, 0);
#endif

    // deep sleep    
    enterSleep();

#if UNO
    Serial.print("CounterWD: ");
    Serial.println(counterWD);
    // delay(1000 * 8);
#else
  softSerial.print("CounterWD: ");
  softSerial.println(counterWD);
#endif    
        
}



// *****************************************************************************************************************************
// 
// Watchdog
// 
// *****************************************************************************************************************************

// Setup the Watch Dog Timer (WDT)
void setupWatchDogTimer() {
	// The MCU Status Register (MCUSR) is used to tell the cause of the last
	// reset, such as brown-out reset, watchdog reset, etc.
	// NOTE: for security reasons, there is a timed sequence for clearing the
	// WDE and changing the time-out configuration. If you don't use this
	// sequence properly, you'll get unexpected results.

	// Clear the reset flag on the MCUSR, the WDRF bit (bit 3).
	MCUSR &= ~(1<<WDRF);

	// Configure the Watchdog timer Control Register (WDTCSR)
	// The WDTCSR is used for configuring the time-out, mode of operation, etc

	// In order to change WDE or the pre-scaler, we need to set WDCE (This will
	// allow updates for 4 clock cycles).

	// Set the WDCE bit (bit 4) and the WDE bit (bit 3) of the WDTCSR. The WDCE
	// bit must be set in order to change WDE or the watchdog pre-scalers.
	// Setting the WDCE bit will allow updates to the pre-scalers and WDE for 4
	// clock cycles then it will be reset by hardware.
	WDTCSR |= (1<<WDCE) | (1<<WDE);

	/**
	 *	Setting the watchdog pre-scaler value with VCC = 5.0V and 16mHZ
	 *	WDP3 WDP2 WDP1 WDP0 | Number of WDT | Typical Time-out at Oscillator Cycles
	 *	0    0    0    0    |   2K cycles   | 16 ms
	 *	0    0    0    1    |   4K cycles   | 32 ms
	 *	0    0    1    0    |   8K cycles   | 64 ms
	 *	0    0    1    1    |  16K cycles   | 0.125 s
	 *	0    1    0    0    |  32K cycles   | 0.25 s
	 *	0    1    0    1    |  64K cycles   | 0.5 s
	 *	0    1    1    0    |  128K cycles  | 1.0 s
	 *	0    1    1    1    |  256K cycles  | 2.0 s
	 *	1    0    0    0    |  512K cycles  | 4.0 s
	 *	1    0    0    1    | 1024K cycles  | 8.0 s
	*/
	WDTCSR  = (1<<WDP3) | (0<<WDP2) | (0<<WDP1) | (1<<WDP0);
	// Enable the WD interrupt (note: no reset).
	WDTCSR |= _BV(WDIE);
}


// *****************************************************************************************************************************
// 
// Interrupt
// 
// *****************************************************************************************************************************
// Watchdog Interrupt Service. This is executed when watchdog timed out.
ISR( WDT_vect ){    
    counterWD++;                            // increase the WDog firing counter. Used in the loop to time the flash
                                            // interval of the LED. If you only want the WDog to fire within the normal 
                                            // presets, say 2 seconds, then comment out this command and also the associated
                                            // commands in the if ( counterWD..... ) loop, except the 2 digitalWrites and the
                                            // delay () commands.
} // end of ISR




// *****************************************************************************************************************************
// 
// Sleep
// 
// *****************************************************************************************************************************



// Enters the arduino into sleep mode.
void enterSleep(void)
{

#if UNO
    
    Serial.println("Enter sleep");
#else
  softSerial.println("Enter sleep");
#endif  

	// There are five different sleep modes in order of power saving:
	// SLEEP_MODE_IDLE - the lowest power saving mode
	// SLEEP_MODE_ADC
	// SLEEP_MODE_PWR_SAVE
	// SLEEP_MODE_STANDBY
	// SLEEP_MODE_PWR_DOWN - the highest power saving mode
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    power_all_disable ();                   // turn power off to ADC, TIMER 1 and 2, Serial Interface

	sleep_enable();

	// Now enter sleep mode.
	sleep_mode();

	// The program will continue from here after the WDT timeout

	// First thing to do is disable sleep.
	sleep_disable();

	// Re-enable the peripherals.
	power_all_enable();
}




// *****************************************************************************************************************************
// 
// BME280
// 
// *****************************************************************************************************************************



// *****************************************************************************************************************************
// 
// Settings
// 
// *****************************************************************************************************************************

void processSettingsChange(NewSettingsPacket newSettings) {

    if (newSettings.changeType == ChangeTypeNone) {
        return;
    } else if (newSettings.changeType == ChangeRadioId) {
        _settings.radioId = newSettings.newRadioId;
        setupRadio();
    } else if (newSettings.changeType == ChangeSleepInterval) {
        _settings.sleepInterval = newSettings.newSleepInterval;
    } else if (newSettings.changeType == ChangeMeasureType) {       
        _settings.measureMode = newSettings.newMeasureMode;
      
        if (_settings.measureMode == MeasureModeWeatherStation){
#ifdef ATTINY_USE_BME280         
            _bme280.setTempOverSample(1);
            _bme280.setPressureOverSample(1);
            _bme280.setHumidityOverSample(1);
            _bme280.setMode(tiny::Mode::SLEEP);
            _bme280.setFilter(0);
#endif            
        } else if (_settings.measureMode == MeasureModeIndoor){
#ifdef ATTINY_USE_BME280          
            _bme280.setTempOverSample(2);
            _bme280.setPressureOverSample(16);
            _bme280.setHumidityOverSample(1);
            _bme280.setMode(tiny::Mode::NORMAL);
            _bme280.setFilter(16);
            _bme280.setStandbyTime(0);
#endif            
        }

        
    }

    saveSettings();
}


void saveSettings()
{
    EepromSettings settingsCurrentlyInEeprom;

    eepromBegin();
    eeprom_read_block(&settingsCurrentlyInEeprom, 0, sizeof(settingsCurrentlyInEeprom));
    eepromEnd();

    // Do not waste 1 of the 100,000 guaranteed eeprom writes if settings have not changed.
    if (memcmp(&settingsCurrentlyInEeprom, &_settings, sizeof(_settings)) == 0) {        
 
    } else {

        eepromBegin();
        eeprom_write_block(&_settings, 0, sizeof(_settings));
        eepromEnd();
    }
}





// *****************************************************************************************************************************
// 
// RF24
// 
// *****************************************************************************************************************************


bool setupRadio(){

    if (_radio.begin() ){  

        _radio.setPALevel(RF24_PA_LEVEL);   // You can set it as minimum or maximum depending on the distance between the transmitter and receiver.
        _radio.setDataRate(RF24_DATA_RATE);
        // _radio.setAutoAck(1);            // Ensure autoACK is enabled

        _radio.enableDynamicPayloads();
        _radio.enableAckPayload();
        _radio.setRetries(5,15);             // delay, count
                                            // 5 gives a 1500 µsec delay which is needed for a 32 byte ackPayload
        _radio.openWritingPipe(address);    // Write to device address 'HOMEKIT_RF24'

        return true;
    }  
    return false;    
}
