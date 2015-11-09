
// http://forums.adafruit.com/viewtopic.php?f=8&t=36132
// SYSTEM_MODE(SEMI_AUTOMATIC);

#include "neopixel.h"
#define	PIXEL_COUNT 100
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, D2, WS2812B);

#include "elapsedMillis.h"
elapsedMillis elapsedCycle;
elapsedMillis elapsedPub;
elapsedMillis elapsedSecondary;

#define BLYNK_PRINT Serial
#include "BlynkSimpleParticle.h"

static const char blynk_auth[] = "5abe10a558a947f7a83a797799c19aef";

uint8_t CYCLE_DELAY = 20;
uint16_t PUB_DELAY = 60*1000;
uint8_t BLYNK_DELAY = 5*1000;
uint32_t nextBlinkPush = 2000;
uint32_t nextEepromSave = 0;
uint32_t eepromSaveDelay = 5*60*1000;
uint8_t intervalSecondary = 100;

static const uint8_t status_pixel = 0;
uint8_t status_color[3] = { 0, 0, 0 };

uint8_t COLOR[3] = { 255, 255, 255 };
uint8_t BRIGHTNESS_MIN = 32;
uint8_t BRIGHTNESS_MAX = 127;
uint8_t BRIGHTNESS_INDEX = BRIGHTNESS_MIN;
bool BRIGHTNESS_DIR = 0;
bool BLYNK_UPDATED = false;
static const uint8_t BRIGHTNESS_ABSOLUTE_MAX = 127;

static const char DEVICE_NAME[] = "porch";
static const char EVENT_NAME[] = "statsd";
String pub = String(DEVICE_NAME)+";";


// EEPROM
// 0 = COLOR[0]
// 1 = COLOR[1]
// 2 = COLOR[3]
// 3 = BRIGHTNESS_MIN
// 4 = BRIGHTNESS_MAX
// 5 = CYCLE_DELAY


// 0 = All on
// 1 = Breathe
uint8_t EFFECT_MODE = 1;
// uint8_t LAST_EFFECT_MODE = EFFECT_MODE;


void setup() {
    strip.begin();
    strip.setBrightness(BRIGHTNESS_MIN);
    strip.show();

    eepromLoad();

    elapsedCycle = 0;
    elapsedPub = 0;

    Blynk.begin(blynk_auth);

    elapsedPub = PUB_DELAY-5*1000;
    elapsedSecondary = 0;
}


void loop() {
    if(BRIGHTNESS_MAX>BRIGHTNESS_ABSOLUTE_MAX) BRIGHTNESS_MAX = BRIGHTNESS_ABSOLUTE_MAX;

    Blynk.run();

    // Process the current effect mode
    doEffectMode();

    // Show the updated colors
	strip.show();


	if(elapsedSecondary>intervalSecondary) {
        // Check if we need to push data back to Blynk
        if(nextBlinkPush>0 && millis()>nextBlinkPush)
            doBlynkPush();

        // Publish data
        doPub();

        // See if we have anything to save
    	if(nextEepromSave>0 && millis()>nextEepromSave)
    	    eepromSave();

    	elapsedSecondary = 0;
    }
}


// Load saved values from EEPROM
void eepromLoad() {
    COLOR[0] = EEPROM.read(0);
    COLOR[1] = EEPROM.read(1);
    COLOR[2] = EEPROM.read(2);
    BRIGHTNESS_MIN = EEPROM.read(3);
    BRIGHTNESS_MAX = EEPROM.read(4);
    CYCLE_DELAY = EEPROM.read(5);

    BRIGHTNESS_INDEX = BRIGHTNESS_MIN;
}


// Save values to EEPROM
void eepromSave() {
    EEPROM.update(0, COLOR[0]);
    EEPROM.update(1, COLOR[1]);
    EEPROM.update(2, COLOR[2]);
    EEPROM.update(3, BRIGHTNESS_MIN);
    EEPROM.update(4, BRIGHTNESS_MAX);
    EEPROM.update(5, CYCLE_DELAY);

    addPub("eesave:1|c");

    doPubSooner(1000);

    nextEepromSave = 0;
}


void doBlynkPush() {
    Blynk.virtualWrite(V0, BRIGHTNESS_MAX);
    Blynk.virtualWrite(V1, COLOR[0]);
    Blynk.virtualWrite(V2, COLOR[1]);
    Blynk.virtualWrite(V3, COLOR[2]);
    Blynk.virtualWrite(V4, BRIGHTNESS_MIN);
    Blynk.virtualWrite(V5, CYCLE_DELAY);
    Blynk.virtualWrite(V6, WiFi.RSSI());

    addPub("r:"+String(COLOR[0])+"|g");
    addPub("g:"+String(COLOR[1])+"|g");
    addPub("b:"+String(COLOR[2])+"|g");
    addPub("bmin:"+String(BRIGHTNESS_MIN)+"|g");
    addPub("bmax:"+String(BRIGHTNESS_MAX)+"|g");
    addPub("d:"+String(CYCLE_DELAY)+"|g");

    nextBlinkPush = 0;

    doPubSooner(1000);

    if(BLYNK_UPDATED)
        nextEepromSave = millis()+eepromSaveDelay;
}


// CYCLE_DELAY
BLYNK_WRITE(5) {
    CYCLE_DELAY = param.asInt();

    nextBlinkPush = millis()+2000;
    BLYNK_UPDATED = true;
}


// BRIGHTNESS_MIN
BLYNK_WRITE(4) {
    int x = param.asInt();

    if(x>BRIGHTNESS_MAX)
        x = BRIGHTNESS_MAX;

    BRIGHTNESS_MIN = x;

    nextBlinkPush = millis()+2000;
    BLYNK_UPDATED = true;
}


// BRIGHTNESS_MAX
BLYNK_WRITE(0) {
    int x = param.asInt();

    if(x<BRIGHTNESS_MIN)
        x = BRIGHTNESS_MIN;

    BRIGHTNESS_MAX = x;

    nextBlinkPush = millis()+2000;
    BLYNK_UPDATED = true;
}


// COLOR[0]
BLYNK_WRITE(1) {
    COLOR[0] = param.asInt();

    nextBlinkPush = millis()+2000;
    BLYNK_UPDATED = true;
}


// COLOR[1]
BLYNK_WRITE(2) {
    COLOR[1] = param.asInt();

    nextBlinkPush = millis()+2000;
    BLYNK_UPDATED = true;
}


// COLOR[2]
BLYNK_WRITE(3) {
    COLOR[2] = param.asInt();

    nextBlinkPush = millis()+2000;
    BLYNK_UPDATED = true;
}


// Apply color to all pixels at once
void lightAll(uint8_t r, uint8_t g, uint8_t b) {
    for(uint8_t i=0; i<PIXEL_COUNT; i++)
        strip.setPixelColor(i, Color(r, g, b));
}


void doEffectMode() {
    if(elapsedCycle<CYCLE_DELAY) return;

    switch(EFFECT_MODE) {
        case 0:
            lightAll(COLOR[0], COLOR[1], COLOR[2]);
            strip.setBrightness(BRIGHTNESS_INDEX);
            break;
        case 1:
        default:
            breathe();
    }

    elapsedCycle = 0;
}


void breathe() {
    // Apply colors
	lightAll(COLOR[0], COLOR[1], COLOR[2]);

    // Stay within bounds
    if(BRIGHTNESS_INDEX>=BRIGHTNESS_MAX)
        BRIGHTNESS_DIR = 0;

    if(BRIGHTNESS_INDEX<=BRIGHTNESS_MIN)
        BRIGHTNESS_DIR = 1;

    // Set the brightness
    if(BRIGHTNESS_DIR==1)
        BRIGHTNESS_INDEX++;
    else
        BRIGHTNESS_INDEX--;

    strip.setBrightness(BRIGHTNESS_INDEX);

    elapsedCycle = 0;
}


void addPub(String s) {
    if(pub.length()==0) pub += String(DEVICE_NAME)+";";
    if(!pub.endsWith(";")) pub += String(",");

    pub += s;
}


void doPubSooner(uint32_t t) {
    elapsedPub = PUB_DELAY-t;

    if(elapsedPub==0)
        elapsedPub = 1;
}


void doPub() {
    if(elapsedPub<PUB_DELAY)
        return;

    addPub("rssi:"+String(WiFi.RSSI())+"|g");
    Particle.publish(EVENT_NAME, pub, 60, PRIVATE);

    pub = String(DEVICE_NAME)+";";

    elapsedPub = 0;
}


// Because R<->G for some reason
uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return strip.Color(g, r, b);
}