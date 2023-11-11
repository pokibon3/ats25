/*
  This sketch SHOULD work with the Chinese KIT sold on AliExpress, eBay and Amazon
  The author of this sketch and Arduino Library does not know the seller of this kit and does not have a commercial relationship with any commercial product that uses the Arduino Library.
  It is important you understand that there is no guarantee that this sketch will work correctly in your current product.
  SO, DO NOT TRY IT IF YOU DON'T KNOW WHAT ARE YOU DOING. YOU MUST BE ABLE TO GO BACK TO THE PREVIOUS VERSION IF THIS SKETCH DOES NOT WORK FOR YOU.

  The user interface and commands are different if compared with the original sketch.
  Please, read the user_manual.txt for more details about this sketch before updating your receiver.
  PLEASE, READ THE user_manual.txt FOR MORE DETAILS ABOUT THIS SKETCH BEFORE UPDATING YOUR RECEIVER.

  ATTENTION: Turn your receiver on with the encoder push button pressed at first time to RESET the eeprom content.
  ARDUINO LIBRARIES:
  1) This sketch uses the Rotary Encoder Class implementation from Ben Buxton (the source code is included together with this sketch). You do not need to install it;
  2) Tiny4kOLED Library and TinyOLED-Fonts (on your Arduino IDE, look for this library on Tools->Manage Libraries).
  3) PU2CLR SI4735 Arduino Library (on your Arduino IDE look for this library on Tools->Manage Libraries).

  ABOUT THE EEPROM:

  ATMEL says the lifetime of an EEPROM memory position is about 100,000 writes.
  For this reason, this sketch tries to avoid save unnecessary writes into the eeprom.
  So, the condition to store any status of the receiver is changing the frequency,  bandwidth, volume, band or step  and 10 seconds of inactivity.
  For example, if you switch the band and turn the receiver off immediately, no new information will be written into the eeprom.
  But you wait 10 seconds after changing anything, all new information will be written.

  ABOUT SSB PATCH:

  First of all, it is important to say that the SSB patch content is not part of this library. The paches used here were made available by Mr.
  Vadim Afonkin on his Dropbox repository. It is important to note that the author of this library does not encourage anyone to use the SSB patches
  content for commercial purposes. In other words, this library only supports SSB patches, the patches themselves are not part of this library.

  In this context, a patch is a piece of software used to change the behavior of the SI4735 device.
  There is little information available about patching the SI4735 or Si4732 devices. The following information is the understanding of the author of
  this project and it is not necessarily correct. A patch is executed internally (run by internal MCU) of the device.
  Usually, patches are used to fixes bugs or add improvements and new features of the firmware installed in the internal ROM of the device.
  Patches to the SI473X are distributed in binary form and have to be transferred to the internal RAM of the device by
  the host MCU (in this case Arduino). Since the RAM is volatile memory, the patch stored into the device gets lost when you turn off the system.
  Consequently, the content of the patch has to be transferred again to the device each time after turn on the system or reset the device.

  Wire up on Arduino UNO, Pro mini and SI4735-D60

  | Device name               | Device Pin / Description      |  Arduino Pin  |
  | ----------------          | ----------------------------- | ------------  |
  | Display OLED              |                               |               |
  |                           | SDA                           |     A4        |
  |                           | CLK                           |     A5        |
  |     (*1) SI4735           |                               |               |
  |                           | RESET (pin 15)                |     12        |
  |                           | SDIO (pin 18)                 |     A4        |
  |                           | SCLK (pin 17)                 |     A5        |
  |                           | SEN (pin 16)                  |    GND        |
  |     (*2) Buttons          |                               |               |
  |                           | Switch MODE (AM/LSB/AM)       |      4        |
  |                           | Banddwith                     |      5        |
  |                           | Volume                        |      6        |
  |                           | Custom button 1 (*3)          |      7        |
  |                           | Band                          |      8        |
  |                           | Custom button 2 (*3)          |      9        |
  |                           | Step                          |     10        |
  |                           | AGC / Attentuation            |     11        |
  |                           | VFO/VFO Switch (Encoder)      |     14 / A0   |
  |    Encoder (*4)           |                               |               |
  |                           | A                             |       2       |
  |                           | B                             |       3       |

   1 - You can use the SI4732-A10. Check on the SI4732 package the pins: RESET, SDIO, SCLK and SEN.
   2 - Please, read the file user_manual.txt for more detail.
   3 - You can remove this buttons from your circuit and sketch if you dont want to use them.
   4 - Some encoder devices have pins A and B inverted. So, if the clockwise and counterclockwise directions
       are not correct for you, please, invert the settings for pins A and B.

  Prototype documentation: https://pu2clr.github.io/SI4735/
  PU2CLR Si47XX API documentation: https://pu2clr.github.io/SI4735/extras/apidoc/html/

  Donate 
  If you find this project useful, consider making a donation so that the author of this library can purchase components
  and modules for improvements and testing of this library. 
  Click here to donate: https://www.paypal.com/donate/?business=LLV4PHKTXC4JW&no_recurring=0&item_name=Consider+making+a+donation.+So%2C+I+can+purchase+components+and+modules+for+improvements+and+testing+of+this+library.&currency_code=USD

  By Ricardo Lima Caratti, April  2021.
  Improved by EFWob (https://github.com/EFWob) Jan 2022.

*/

#define DEBUG  // Comment/uncomment for disabling/enabling debug output on Serial
//#define DEBUG_BUTTONS_ONLY  // If defined (in addition to DEBUG), just do DEBUG output for Buttons (radio will not play at all)

#include <SI4735.h>
#include <EEPROM.h>
#include <Tiny4kOLED.h>
#include <font8x16atari.h>  // Please, install the TinyOLED-Fonts library
#include "Rotary.h"
#include "SimpleButton.h"

#include <patch_ssb_compressed.h>  // Compressed SSB patch version (saving almost 1KB)

const uint16_t size_content = sizeof ssb_patch_content;  // See ssb_patch_content.h
const uint16_t cmd_0x15_size = sizeof cmd_0x15;          // Array of lines where the 0x15 command occurs in the patch content.

#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3

// OLED Diaplay constants
#define RST_PIN -1  // Define proper RST_PIN if required.

#define RESET_PIN 12

// Enconder PINs - if the clockwise and counterclockwise directions are not correct for you, please, invert this settings.
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3

// Buttons controllers
#define MODE_SWITCH 4       // Switch MODE (Am/LSB/USB)
#define BANDWIDTH_BUTTON 5  // Used to select the banddwith.
#define VOLUME_BUTTON 6     // Volume Up
#define AVC_BUTTON 7        // **** Use thi button to implement a new function
#define BAND_BUTTON 8       // Next band
#define SOFTMUTE_BUTTON 9   // **** Use thi button to implement a new function
#define AGC_BUTTON 11       // Switch AGC ON/OF
#define STEP_BUTTON 10      // Used to select the increment or decrement frequency step (see tabStep)
#define ENCODER_BUTTON 14   // Used to select the enconder control (BFO or VFO) and SEEK function on AM and FM modes

#define MIN_ELAPSED_TIME 100
#define MIN_ELAPSED_RSSI_TIME 150

#define DEFAULT_VOLUME 36  // change it for your favorite sound volume


#define FM 0
#define LSB 1
#define USB 2
#define AM 3
#define LW 4

#define SSB 1

#define STORE_TIME 10000  // Time of inactivity to make the current receiver status writable (10s / 10000 milliseconds).

const uint8_t app_id = 43;  // Useful to check the EEPROM content before processing useful data
const int eeprom_address = 0;
long storeTime = millis();

const char *bandModeDesc[] = { "FM ", "LSB", "USB", "AM " };
uint8_t currentMode = FM;
uint8_t seekDirection = 1;

bool bfoOn = false;

bool ssbLoaded = false;
bool fmStereo = true;

bool cmdVolume = false;    // if true, the encoder will control the volume.
bool cmdAgcAtt = false;    // if true, the encoder will control the AGC / Attenuation
bool cmdStep = false;      // if true, the encoder will control the step frequency
bool cmdBw = false;        // if true, the encoder will control the bandwidth
bool cmdBand = false;      // if true, the encoder will control the band
bool cmdSoftMute = false;  // if true, the encoder will control the Soft Mute attenuation
bool cmdAvc = false;       // if true, the encoder will control Automatic Volume Control

uint8_t muteVolume = 0;  // restore volume for "un-mute"

long countRSSI = 0;

int currentBFO = 0;

// Button handlers for push buttons/encoder
SimpleButton btn_Bandwidth(BANDWIDTH_BUTTON);
SimpleButton btn_BandUp(BAND_BUTTON);
SimpleButton btn_BandDn(SOFTMUTE_BUTTON);
SimpleButton btn_VolumeUp(VOLUME_BUTTON);
SimpleButton btn_VolumeDn(AVC_BUTTON);
SimpleButton btn_Encoder(ENCODER_BUTTON);
SimpleButton btn_AGC(AGC_BUTTON);
SimpleButton btn_Step(STEP_BUTTON);
SimpleButton btn_Mode(MODE_SWITCH);



long elapsedRSSI = millis();
//TODO long elapsedButton = millis();

// Encoder control variables
volatile int encoderCount = 0;

// Some variables to check the SI4735 status
uint16_t currentFrequency;
uint16_t previousFrequency;
// uint8_t currentStep = 1;
uint8_t currentBFOStep = 25;

// Datatype to deal with bandwidth on AM, SSB and FM in numerical order.
// Ordering by bandwidth values.
typedef struct
{
  uint8_t idx;       // SI473X device bandwidth index value
  const char *desc;  // bandwidth description
} Bandwidth;

int8_t bwIdxSSB = 4;
Bandwidth bandwidthSSB[] = {
  { 4, "0.5" },  // 0
  { 5, "1.0" },  // 1
  { 0, "1.2" },  // 2
  { 1, "2.2" },  // 3
  { 2, "3.0" },  // 4  - default
  { 3, "4.0" }   // 5
};               // 3 = 4kHz

int8_t bwIdxAM = 4;
const int maxFilterAM = 6;
Bandwidth bandwidthAM[] = {
  { 4, "1.0" },  // 0
  { 5, "1.8" },  // 1
  { 3, "2.0" },  // 2
  { 6, "2.5" },  // 3
  { 2, "3.0" },  // 4 - default
  { 1, "4.0" },  // 5
  { 0, "6.0" }   // 6
};

int8_t bwIdxFM = 0;
Bandwidth bandwidthFM[] = {
  { 0, "AUT" },  // Automatic - default
  { 1, "110" },  // Force wide (110 kHz) channel filter.
  { 2, " 84" },
  { 3, " 60" },
  { 4, " 40" }
};

// Atenuação and AGC
int8_t agcIdx = 0;
uint8_t disableAgc = 0;
uint8_t agcNdx = 0;
int8_t smIdx = 8;
int8_t avcIdx = 38;

int tabStep[] = { 1,      // 0
                  5,      // 1
                  9,      // 2
                  10,     // 3
                  50,     // 4
                  100 };  // 5

const int lastStep = (sizeof tabStep / sizeof(int)) - 1;
int idxStep = 3;

/*
   Band data structure
*/
typedef struct
{
  uint8_t bandType;         // Band type (FM, MW or SW)
  uint16_t minimumFreq;     // Minimum frequency of the band
  uint16_t maximumFreq;     // Maximum frequency of the band
  uint16_t currentFreq;     // Default frequency or current frequency
  uint16_t currentStepIdx;  // Idex of tabStep:  Defeult frequency step (See tabStep)
  int8_t bandwidthIdx;      // Index of the table bandwidthFM, bandwidthAM or bandwidthSSB;
} Band;

/*
   Band table
   YOU CAN CONFIGURE YOUR OWN BAND PLAN. Be guided by the comments.
   To add a new band, all you have to do is insert a new line in the table below. No extra code will be needed.
   You can remove a band by deleting a line if you do not want a given band.
   Also, you can change the parameters of the band.
   ATTENTION: You have to RESET the eeprom after adding or removing a line of this table.
              Turn your receiver on with the encoder push button pressed at first time to RESET the eeprom content.
*/
Band band[] = {
  { LW_BAND_TYPE, 100, 510, 300, 0, 4 },
  { MW_BAND_TYPE, 520, 1720, 810, 3, 4 },    // AM/MW from 520 to 1720kHz; default 810kHz; default step frequency index is 3 (10kHz); default bandwidth index is 4 (3kHz)
  { MW_BAND_TYPE, 531, 1701, 783, 2, 4 },    // MW for Europe, Africa and Asia
  { SW_BAND_TYPE, 1700, 3500, 1900, 0, 4 },  // 160 meters
  { SW_BAND_TYPE, 3500, 4500, 3700, 0, 5 },  // 80 meters
  { SW_BAND_TYPE, 4500, 5600, 4850, 1, 4 },
  { SW_BAND_TYPE, 5600, 6800, 6000, 1, 4 },
  { SW_BAND_TYPE, 6800, 7300, 7100, 0, 4 },  // 40 meters
  { SW_BAND_TYPE, 7200, 8500, 7200, 1, 4 },  // 41 meters
  { SW_BAND_TYPE, 8500, 10000, 9600, 1, 4 },
  { SW_BAND_TYPE, 10000, 11200, 10100, 0, 4 },  // 30 meters
  { SW_BAND_TYPE, 11200, 12500, 11940, 1, 4 },
  { SW_BAND_TYPE, 13400, 13900, 13600, 1, 4 },
  { SW_BAND_TYPE, 14000, 14500, 14200, 0, 4 },  // 20 meters
  { SW_BAND_TYPE, 15000, 15900, 15300, 1, 4 },
  { SW_BAND_TYPE, 17200, 17900, 17600, 1, 4 },
  { SW_BAND_TYPE, 18000, 18300, 18100, 0, 4 },  // 17 meters
  { SW_BAND_TYPE, 21000, 21400, 21200, 0, 4 },  // 15 mters
  { SW_BAND_TYPE, 21400, 21900, 21500, 1, 4 },  // 13 mters
  { SW_BAND_TYPE, 24890, 26200, 24940, 0, 4 },  // 12 meters
  { SW_BAND_TYPE, 26200, 28000, 27500, 0, 4 },  // CB band (11 meters)
  { SW_BAND_TYPE, 28000, 30000, 28400, 0, 4 },  // 10 meters
  { FM_BAND_TYPE, 6400, 8400, 7000, 3, 0 },     // FM from 64 to 84MHz; default 70MHz; default step frequency index is 3; default bandwidth index AUTO
  { FM_BAND_TYPE, 8400, 10800, 10570, 3, 0 }
};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int bandIdx = 1;

uint8_t rssi = 0;
uint8_t stereo = 1;
uint8_t volume = DEFAULT_VOLUME;

// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);
SI4735 si4735;

void setup() {

#ifdef DEBUG
  Serial.begin(115200);
#ifdef DEBUG_BUTTONS_ONLY
  return;
#endif
#endif
  // Encoder pins
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);

  oled.begin();
  oled.clear();
  oled.on();
  oled.setFont(FONT6X8);

  // Splash - Change orit for your introduction text or remove the splash code.
  oled.setCursor(40, 0);
  oled.print("SI473X");
  oled.setCursor(20, 1);
  oled.print("Arduino Library");
  delay(100);
  oled.setCursor(15, 2);
  oled.print("All in One Radio");
  delay(100);
  oled.setCursor(10, 3);
  oled.print("V3.0.8-By PU2CLR");
  delay(1000);
  // end Splash

  // If you want to reset the eeprom, keep the VOLUME_UP button pressed during statup
  if (digitalRead(ENCODER_BUTTON) == LOW) {
    oled.clear();
    EEPROM.write(eeprom_address, 0);
    oled.setCursor(0, 0);
    oled.print("EEPROM RESETED");
    delay(2000);
    oled.clear();
  }

  // Encoder interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

  si4735.getDeviceI2CAddress(RESET_PIN);  // Looks for the I2C bus address and set it.  Returns 0 if error

  si4735.setup(RESET_PIN, MW_BAND_TYPE);  //
  si4735.setAvcAmMaxGain(48);             // Sets the maximum gain for automatic volume control on AM/SSB mode (between 12 and 90dB)

  delay(500);

  // Checking the EEPROM content
  if (EEPROM.read(eeprom_address) == app_id) {
    readAllReceiverInformation();
  }

  // Set up the radio for the current band (see index table variable bandIdx )
  useBand();

  currentFrequency = previousFrequency = si4735.getFrequency();

  si4735.setVolume(volume);
  oled.clear();
  showStatus();
}


#define ENCODER_MUTEDELAY 2  // Controls how long the encoder button needs to be longpressed for Mute functionality: \
                             //    -must be uint8_t (i. e. between 0 and 255) \
                             //    -if Zero, Encoder longpresses will be ignored (treated same as shortpress) \
                             //    -any other number 'x' specifies the timeout to be (roughly, in ms) \
                             //       BUTTONTIME_LONGPRESS1 + x * BUTTONTIME_LONGPRESSREPEAT i. e. \
                             //       320 + x * 48 if you did not change the default settings in "SimpleButton.h" \
                             //    - with a setting of 2, this is roughly 400 ms
#define BANDWIDTH_DELAY 9    // Controls how long the Bandwidth button needs to be longpressed for functionality: \
                             //    -must be uint8_t (i. e. between 0 and 255) \
                             //    -if Zero, longpresses will be ignored (will be same as shortpress) \
                             //    -any other number 'x' specifies the timeout (in ms) between value changes as \
                             //       x * BUTTONTIME_LONGPRESSREPEAT , with the first change happening after BUTTONTIME_LONGPRESS1 \
                             //    -with a setting of 9, this is roughly 450 ms \
                             //     (if you did not change the default settings in "SimpleButton.h")
#define STEP_DELAY 6         // Controls how long the Step button needs to be longpressed for functionality: \
                             //    -must be uint8_t (i. e. between 0 and 255) \
                             //    -if Zero, longpresses will be ignored (will be same as shortpress) \
                             //    -any other number 'x' specifies the timeout (in ms) between value changes as \
                             //       x * BUTTONTIME_LONGPRESSREPEAT , with the first change happening after BUTTONTIME_LONGPRESS1 \
                             //    -with a setting of 6, this is roughly 300 ms \
                             //     (if you did not change the default settings in "SimpleButton.h")

#define BAND_DELAY 2  // Controls how long the BAND+/- buttons need to be longpressed for functionality: \
                      //    -must be uint8_t (i. e. between 0 and 255) \
                      //    -if Zero, longpresses will be ignored (will be same as shortpress) \
                      //    -any other number 'x' specifies the timeout (in ms) between Band changes as \
                      //       x * BUTTONTIME_LONGPRESSREPEAT , with the first change happening after BUTTONTIME_LONGPRESS1 \
                      //    -with a setting of 2, this is roughly not below 100 ms \
                      //      (however, since band switching/scree updating consumes some time, it is OK to leave at 1) \
                      //     (if you did not change the default settings in "SimpleButton.h")

#define VOLUME_DELAY 1  // Controls how long the Vol+/- buttons need to be longpressed for functionality: \
                        //    -must be uint8_t (i. e. between 0 and 255) \
                        //    -if Zero, longpresses will be ignored (will be same as shortpress) \
                        //    -any other number 'x' specifies the timeout (in ms) between value changes as \
                        //       x * BUTTONTIME_LONGPRESSREPEAT , with the first change happening after BUTTONTIME_LONGPRESS1 \
                        //    -with a setting of 1 (the best IMHO), this is roughly 50 ms \
                        //     (if you did not change the default settings in "SimpleButton.h")


#define AGC_DELAY 1  // Controls how long the AGC button needs to be longpressed for functionality: \
                     //    -must be uint8_t (i. e. between 0 and 255) \
                     //    -if Zero, longpresses will be ignored (will be same as shortpress) \
                     //    -any other number 'x' specifies the timeout (in ms) between value changes as \
                     //       x * BUTTONTIME_LONGPRESSREPEAT , with the first change happening after BUTTONTIME_LONGPRESS1 \
                     //    -with a setting of 1, this is roughly 50 ms \
                     //     (if you did not change the default settings in "SimpleButton.h")


// Print information about detected button press events on Serial
// These events can be used to attach specific funtionality to a button
// DEBUG must be defined for the function to have any effect
#if defined(DEBUG)
uint8_t buttonEvent(uint8_t event, uint8_t pin) {
  Serial.print("Ev=");
  Serial.print(event);
  Serial.print(" Pin=");
  Serial.print(pin);
  Serial.print(">>");
  if (BUTTONEVENT_ISLONGPRESS(event)) {
    if (BUTTONEVENT_FIRSTLONGPRESS == event)
      Serial.println("LP started!");
    else if (BUTTONEVENT_LONGPRESS == event)
      Serial.println("LP repeat!");
    else if (BUTTONEVENT_LONGPRESSDONE == event)
      Serial.println("LP Done!");
    else
      Serial.println("LP: UNEXPECTED!!!!");
  } else if (event == BUTTONEVENT_SHORTPRESS)
    Serial.println("Click!");
  else
    Serial.println("UNEXPECTED!!!!");
  return event;
}

#else
#define buttonEvent NULL
#endif

//Handle Longpress of VolumeUp/VolumeDn (shortpress is handled in loop())
uint8_t volumeEvent(uint8_t event, uint8_t pin) {
#ifdef DEBUG
  buttonEvent(event, pin);
#endif
  if (muteVolume)                    // currently muted?
    if (!BUTTONEVENT_ISDONE(event))  // Any event to change the volume?
      if ((BUTTONEVENT_SHORTPRESS != event) || (VOLUME_BUTTON == pin))
        doVolume(1);  // yes-->Unmute
#if (0 != VOLUME_DELAY)
#if (VOLUME_DELAY > 1)
  static uint8_t count;
  if (BUTTONEVENT_FIRSTLONGPRESS == event) {
    count = 0;
  }
#endif
  if (BUTTONEVENT_ISLONGPRESS(event))        // longpress-Event?
    if (BUTTONEVENT_LONGPRESSDONE != event)  // but not the final release of button?
    {
#if (VOLUME_DELAY > 1)
      if (count++ == 0)
#endif
        doVolume(VOLUME_BUTTON == pin ? 1 : -1);
#if (VOLUME_DELAY > 1)
      count = count % VOLUME_DELAY;
#endif
    }
#else
  if (BUTTONEVENT_FIRSTLONGPRESS == event)
    event = BUTTONEVENT_SHORTPRESS;
#endif
  return event;
}

//Handle Longpress of Encoder (shortpress is handled in loop()) to mute/Unmute
uint8_t encoderEvent(uint8_t event, uint8_t pin) {
#ifdef DEBUG
  buttonEvent(event, pin);
#endif
#if (0 != ENCODER_MUTEDELAY)
  static uint8_t waitBeforeMute;
  if (BUTTONEVENT_FIRSTLONGPRESS == event) {
    waitBeforeMute = ENCODER_MUTEDELAY;
  } else if ((BUTTONEVENT_LONGPRESS == event) && (0 != waitBeforeMute))
    if (0 == --waitBeforeMute) {
      uint8_t x = muteVolume;
      muteVolume = si4735.getVolume();
      si4735.setVolume(x);
      showVolume();
    }
#else
  if (BUTTONEVENT_FIRSTLONGPRESS == event)
    event = BUTTONEVENT_SHORTPRESS;
#endif
  return event;
}

uint8_t modeEvent(uint8_t event, uint8_t pin) {
#ifdef DEBUG
  buttonEvent(event, pin);
#endif
  if (BUTTONEVENT_FIRSTLONGPRESS == event)
    event = BUTTONEVENT_SHORTPRESS;
  return event;
}


uint8_t bandwidthEvent(uint8_t event, uint8_t pin) {
#ifdef DEBUG
  buttonEvent(event, pin);
#endif
#if (0 != BANDWIDTH_DELAY)
  if (BUTTONEVENT_ISLONGPRESS(event)) {
    static uint8_t direction = 1;
    if (BUTTONEVENT_ISDONE(event))
      direction = 1 - direction;
    else {
      static uint8_t count;
      static uint8_t *bwFlag;
      static uint8_t bwMax;
      if (BUTTONEVENT_FIRSTLONGPRESS == event) {
        count = 0;
        if ((AM == currentMode) || (LW == currentMode)) {
          bwFlag = (unsigned char *)&bwIdxAM;
          bwMax = 6;
        } else if (FM == currentMode) {
          bwFlag = (unsigned char *)&bwIdxFM;
          bwMax = 4;
        } else {
          bwFlag = (unsigned char *)&bwIdxSSB;
          bwMax = 5;
        }
        if (0 == *bwFlag)
          direction = 1;
        else if (*bwFlag == bwMax)
          direction = 0;
      }
      if (0 == count) {
        if (((direction == 0) && (*bwFlag > 0)) || ((1 == direction) && (*bwFlag < bwMax))) {
          doBandwidth(direction);
        }
      }
      count = (count + 1) % BANDWIDTH_DELAY;
    }
  }
#else
  if (BUTTONEVENT_FIRSTLONGPRESS == event)
    event = BUTTONEVENT_SHORTPRESS;
#endif
  return event;
}

uint8_t stepEvent(uint8_t event, uint8_t pin) {
#ifdef DEBUG
  buttonEvent(event, pin);
#endif
#if (0 != STEP_DELAY)
  if (FM != currentMode)
    if (BUTTONEVENT_ISLONGPRESS(event)) {
      static uint8_t direction = 1;
      static uint8_t count;
      if (BUTTONEVENT_LONGPRESSDONE == event)
        direction = 1 - direction;
      else {
        if (BUTTONEVENT_FIRSTLONGPRESS == event) {
          count = 0;
          if (0 == idxStep)
            direction = 1;
          else if (lastStep == idxStep)
            direction = 0;
        }
        if (0 == count++)
          if (bfoOn || (idxStep != (direction ? lastStep : 0)))
            doStep(direction);
        count = count % STEP_DELAY;
      }
    }
#else
  if (BUTTONEVENT_FIRSTLONGPRESS == event)
    event = BUTTONEVENT_SHORTPRESS;
#endif
  return event;
}

uint8_t agcEvent(uint8_t event, uint8_t pin) {
#ifdef DEBUG
  buttonEvent(event, pin);
#endif
#if (0 != AGC_DELAY)
  if (FM != currentMode)
    if (BUTTONEVENT_ISLONGPRESS(event)) {
      static uint8_t direction = 1;
      static uint8_t count;
      if (BUTTONEVENT_LONGPRESSDONE == event)
        direction = 1 - direction;
      else {
        if (BUTTONEVENT_FIRSTLONGPRESS == event) {
          count = 0;
          if (0 == agcIdx)
            direction = 1;
          else if (37 == agcIdx)
            direction = 0;
        }
        if (0 == count++)
          if ((!direction && agcIdx) || (direction && (agcIdx < 37)))
            doAttenuation(direction);
        count = count % AGC_DELAY;
      }
    }
#else
  if (BUTTONEVENT_FIRSTLONGPRESS == event)
    event = BUTTONEVENT_SHORTPRESS;
#endif
  return event;
}



//Handle Longpress of BandUp/BandDn (shortpress is handled in loop())
uint8_t bandEvent(uint8_t event, uint8_t pin) {
#ifdef DEBUG
  buttonEvent(event, pin);
#endif
#if (0 != BAND_DELAY)
  static uint8_t count;
  if (BUTTONEVENT_ISLONGPRESS(event))  // longpress-Event?
    if (BUTTONEVENT_LONGPRESSDONE != event) {
      if (BUTTONEVENT_FIRSTLONGPRESS == event) {
        count = 0;
      }
      if (count++ == 0) {
        if (BAND_BUTTON == pin) {
          if (bandIdx < lastBand)
            bandUp();
        } else {
          if (bandIdx)
            bandDown();
        }
      }
      count = count % BAND_DELAY;
    }
#else
  if (BUTTONEVENT_FIRSTLONGPRESS == event)
    event = BUTTONEVENT_SHORTPRESS;
#endif
  return event;
}



// Use Rotary.h and  Rotary.cpp implementation to process encoder via interrupt
void rotaryEncoder() {  // rotary encoder events
  uint8_t encoderStatus = encoder.process();
  if (encoderStatus) {
    if (encoderStatus == DIR_CW) {
      encoderCount = 1;
    } else {
      encoderCount = -1;
    }
  }
}

/*
   writes the conrrent receiver information into the eeprom.
   The EEPROM.update avoid write the same data in the same memory position. It will save unnecessary recording.
*/
void saveAllReceiverInformation() {
  int addr_offset;
  EEPROM.update(eeprom_address, app_id);                  // stores the app id;
  EEPROM.update(eeprom_address + 1, si4735.getVolume());  // stores the current Volume
  EEPROM.update(eeprom_address + 2, bandIdx);             // Stores the current band
  EEPROM.update(eeprom_address + 3, currentMode);         // Stores the current Mode (FM / AM / SSB)
  EEPROM.update(eeprom_address + 4, currentBFO >> 8);
  EEPROM.update(eeprom_address + 5, currentBFO & 0XFF);

  addr_offset = 6;
  band[bandIdx].currentFreq = currentFrequency;

  for (int i = 0; i <= lastBand; i++) {
    EEPROM.update(addr_offset++, (band[i].currentFreq >> 8));    // stores the current Frequency HIGH byte for the band
    EEPROM.update(addr_offset++, (band[i].currentFreq & 0xFF));  // stores the current Frequency LOW byte for the band
    EEPROM.update(addr_offset++, band[i].currentStepIdx);        // Stores current step of the band
    EEPROM.update(addr_offset++, band[i].bandwidthIdx);          // table index (direct position) of bandwidth
  }

  // Saves AVC and AGC/Att status
  EEPROM.update(addr_offset++, avcIdx);
  EEPROM.update(addr_offset++, agcIdx);
  EEPROM.update(addr_offset++, agcNdx);
}

/**
   reads the last receiver status from eeprom.
*/
void readAllReceiverInformation() {
  int addr_offset;
  int bwIdx;
  volume = EEPROM.read(eeprom_address + 1);  // Gets the stored volume;
  bandIdx = EEPROM.read(eeprom_address + 2);
  currentMode = EEPROM.read(eeprom_address + 3);
  currentBFO = EEPROM.read(eeprom_address + 4) << 8;
  currentBFO |= EEPROM.read(eeprom_address + 5);

  addr_offset = 6;
  for (int i = 0; i <= lastBand; i++) {
    band[i].currentFreq = EEPROM.read(addr_offset++) << 8;
    band[i].currentFreq |= EEPROM.read(addr_offset++);
    band[i].currentStepIdx = EEPROM.read(addr_offset++);
    band[i].bandwidthIdx = EEPROM.read(addr_offset++);
  }

  // Rescues the previous  AVC and AGC/Att status
  avcIdx = EEPROM.read(addr_offset++);
  agcIdx = EEPROM.read(addr_offset++);
  agcNdx = EEPROM.read(addr_offset++);


  previousFrequency = currentFrequency = band[bandIdx].currentFreq;
  idxStep = tabStep[band[bandIdx].currentStepIdx];
  bwIdx = band[bandIdx].bandwidthIdx;

  if (currentMode == LSB || currentMode == USB) {
    loadSSB();
    bwIdxSSB = (bwIdx > 5) ? 5 : bwIdx;
    si4735.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
    // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
    if (bandwidthSSB[bwIdxSSB].idx == 0 || bandwidthSSB[bwIdxSSB].idx == 4 || bandwidthSSB[bwIdxSSB].idx == 5)
      si4735.setSSBSidebandCutoffFilter(0);
    else
      si4735.setSSBSidebandCutoffFilter(1);
  } else if (currentMode == AM) {
    bwIdxAM = bwIdx;
    si4735.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
  } else {
    bwIdxFM = bwIdx;
    si4735.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
  }
}

/*
   To store any change into the EEPROM, it is needed at least STORE_TIME  milliseconds of inactivity.
*/
void resetEepromDelay() {
  storeTime = millis();
  previousFrequency = 0;
}

/**
   Converts a number to a char string and places leading zeros.
   It is useful to mitigate memory space used by sprintf or generic similar function

   value  - value to be converted
   strValue - the value will be receive the value converted
   len -  final string size (in bytes)
   dot - the decimal or tousand separator position
   separator -  symbol "." or ","
*/
void convertToChar(uint16_t value, char *strValue, uint8_t len, uint8_t dot, uint8_t separator) {
  char d;
  int i;
  for (i = (len - 1); i >= 0; i--) {
    d = value % 10;
    value = value / 10;
    strValue[i] = d + 48;
  }
  strValue[len] = '\0';
  if (dot > 0) {
    for (int i = len; i >= dot; i--) {
      strValue[i + 1] = strValue[i];
    }
    strValue[dot] = separator;
    len = dot;
  }
  i = 0;
  len--;
  while ((i < len) && ('0' == strValue[i])) {
    strValue[i++] = ' ';
  }
}

/**
  Show current frequency
*/
void showFrequency() {
  char *unit;
  char freqDisplay[10];

  if (band[bandIdx].bandType == FM_BAND_TYPE) {
    convertToChar(currentFrequency, freqDisplay, 5, 3, ',');
    unit = (char *)"MHz";
  } else {
    unit = (char *)"kHz";
    if (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE)
      convertToChar(currentFrequency, freqDisplay, 5, 0, '.');
    else
      convertToChar(currentFrequency, freqDisplay, 5, 2, '.');
  }

  oled.invertOutput(bfoOn);
  oled.setFont(FONT8X16ATARI);
  oled.setCursor(34, 0);
  oled.print("      ");
  oled.setCursor(34, 0);
  oled.print(freqDisplay);
  oled.setFont(FONT6X8);
  oled.invertOutput(false);

  oled.setCursor(95, 0);
  oled.print(unit);
}

/**
    This function is called by the seek function process.
*/
void showFrequencySeek(uint16_t freq) {
  currentFrequency = freq;
  showFrequency();
}

/**
   Checks the stop seeking criterias.
   Used to abort the seek process.
   Returns true if the user press the touch or rotates the encoder during the seek process.
*/
bool checkStopSeeking() {
  // Checks the touch and encoder
  return (bool)encoderCount || (digitalRead(ENCODER_BUTTON) == LOW);  // returns true if the user rotates the encoder or press the push button
}

/**
    Shows some basic information on display
*/
void showStatus() {
  showFrequency();
  showBandDesc();
  showStep();
  showBandwidth();
  showAttenuation();
  showRSSI();
  showVolume();
}

/**
   Shows band information
*/
void showBandDesc() {
  char *bandMode;
  if (currentFrequency < 520)
    bandMode = (char *)"LW  ";
  else
    bandMode = (char *)bandModeDesc[currentMode];

  oled.setCursor(0, 0);
  oled.print("    ");
  oled.setCursor(0, 0);
  oled.invertOutput(cmdBand);
  oled.print(bandMode);
  oled.invertOutput(false);
}

/* *******************************
   Shows RSSI status
*/
void showRSSI() {
  int bars = (rssi / 20.0) + 1;
  oled.setCursor(90, 3);
  oled.print("      ");
  oled.setCursor(90, 3);
  oled.print(".");
  for (int i = 0; i < bars; i++)
    oled.print('_');
  oled.print('|');

  if (currentMode == FM) {
    oled.setCursor(18, 0);
    oled.print("  ");
    oled.setCursor(18, 0);
    oled.invertOutput(true);
    if (si4735.getCurrentPilot()) {
      oled.invertOutput(true);
      oled.print("s");
    }
    oled.invertOutput(false);
  }
}

/*
   Shows the volume level on LCD
*/
void showVolume() {
  char s[3];
  oled.setCursor(58, 3);
  oled.print("  ");
  oled.setCursor(58, 3);
  oled.invertOutput(cmdVolume);
  oled.print(' ');
  oled.invertOutput(false);
  convertToChar(si4735.getCurrentVolume(), s, 2, 0, 0);
  oled.print(s);
}

void showStep() {
  if (bfoOn)
    return;
  oled.setCursor(93, 1);
  oled.print("     ");
  if (currentMode == FM)
    return;
  oled.setCursor(93, 1);
  oled.invertOutput(cmdStep);
  oled.print("S:");
  oled.invertOutput(false);
  oled.print(tabStep[idxStep]);
}

/**
   Shows bandwidth on AM,SSB and FM mode
*/
void showBandwidth() {
  char *bw;
  if (currentMode == LSB || currentMode == USB) {
    bw = (char *)bandwidthSSB[bwIdxSSB].desc;
    showBFO();
  } else if (currentMode == AM) {
    bw = (char *)bandwidthAM[bwIdxAM].desc;
  } else {
    bw = (char *)bandwidthFM[bwIdxFM].desc;
  }
  oled.setCursor(0, 3);
  oled.print("          ");
  oled.setCursor(0, 3);
  oled.invertOutput(cmdBw);
  oled.print("BW: ");
  oled.invertOutput(false);
  oled.print(bw);
}

/*
   Shows AGCC and Attenuation
*/
void showAttenuation() {
  // Show AGC Information
  oled.setCursor(0, 1);
  oled.print("     ");
  oled.setCursor(0, 1);
  if (currentMode != FM) {
    oled.invertOutput(cmdAgcAtt);
    if (agcIdx == 0) {
      oled.print("AGC");
      oled.invertOutput(false);
    } else {
      oled.print("At");
      oled.invertOutput(false);
      oled.print(agcNdx);
    }
  }
}

void showSoftMute() {
  if (currentMode != FM) {
    oled.setCursor(0, 1);
    oled.print("     ");
    oled.setCursor(0, 1);
    oled.invertOutput(cmdSoftMute);
    oled.print("SM");
    oled.invertOutput(false);
    oled.print(smIdx);
  }
}


void showAvc() {
  if (currentMode != FM) {
    oled.setCursor(0, 1);
    oled.print("     ");
    oled.setCursor(0, 1);
    oled.invertOutput(cmdAvc);
    oled.print("AVC");
    oled.invertOutput(false);
    oled.print(avcIdx);
  }
}

/*
   Shows the BFO current status.
   Must be called only on SSB mode (LSB or USB)
*/
void showBFO() {
  oled.setCursor(0, 2);
  oled.print("            ");
  oled.setCursor(0, 2);
  oled.print("BFO: ");
  oled.print(currentBFO);
  oled.print("Hz ");

  oled.setCursor(93, 2);
  oled.print("    ");
  oled.setCursor(93, 2);
  oled.invertOutput(cmdStep);
  oled.print("S:");
  oled.invertOutput(false);
  oled.print(currentBFOStep);
}


// RDS Begin

char *stationName;
char *programInfo;

long delayStationName = millis();
long delayProgramInfo = millis();
long delayMsgTurn = millis();
bool bShowStationName = true;
int progInfoIdx = 0;


/*
   Clean the content of the third line (line 2 - remember the first line is 0)
*/
void cleanBfoRdsInfo() {
  oled.setCursor(0, 2);
  oled.print("                    ");
}

/**
  showProgramInfo - Shows the Program Information
*/
void showProgramInfo() {
  char txtAux[21];

  if (programInfo == NULL || (millis() - delayProgramInfo) < 500) return;
  cleanBfoRdsInfo();
  programInfo[61] = '\0';  // Truncate the message to fit on display line
  strncpy(txtAux, &programInfo[progInfoIdx], sizeof(txtAux));
  txtAux[20] = '\0';
  progInfoIdx += 2;
  if (progInfoIdx > (int)(60 - sizeof(txtAux))) progInfoIdx = 0;
  oled.setCursor(0, 2);
  oled.print(txtAux);
  delayProgramInfo = millis();
}


void showStationName() {
  if (stationName == NULL || (millis() - delayStationName) < 3000) return;
  cleanBfoRdsInfo();
  oled.setCursor(0, 2);
  oled.print(stationName);
  delayStationName = millis();
}


/*
   Checks the station name is available
*/
void checkRDS() {
  if (currentFrequency != previousFrequency) {
    cleanBfoRdsInfo();
  } else {
    si4735.getRdsStatus();
    if (si4735.getRdsReceived()) {
      if (si4735.getRdsSync() && si4735.getNumRdsFifoUsed() > 0) {
        if ((millis() - delayMsgTurn) > 30000) {
          bShowStationName = !bShowStationName;
          progInfoIdx = 0;
          delayMsgTurn = millis();
        }
        if (bShowStationName) {  // time to show Station Name
          stationName = si4735.getRdsStationName();
          showStationName();
        } else {  // time to show Program Information
          programInfo = si4735.getRdsProgramInformation();
          showProgramInfo();
        }
      }
    }
  }
}


/*
   Goes to the next band (see Band table)
*/
void bandUp() {
  // save the current frequency for the band
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStepIdx = idxStep;  // currentStep;

  if (bandIdx < lastBand) {
    bandIdx++;
  } else {
    bandIdx = 0;
  }
  useBand();
}

/*
   Goes to the previous band (see Band table)
*/
void bandDown() {
  // save the current frequency for the band
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStepIdx = idxStep;
  if (bandIdx > 0) {
    bandIdx--;
  } else {
    bandIdx = lastBand;
  }
  useBand();
}

/*
   This function loads the contents of the ssb_patch_content array into the CI (Si4735) and starts the radio on
   SSB mode.
*/
void loadSSB() {
  oled.setCursor(0, 2);
  oled.print("  Switching to SSB  ");
  // si4735.setI2CFastModeCustom(700000); // It is working. Faster, but I'm not sure if it is safe.
  si4735.setI2CFastModeCustom(500000);
  si4735.queryLibraryId();  // Is it really necessary here? I will check it.
  si4735.patchPowerUp();
  delay(50);
  si4735.downloadCompressedPatch(ssb_patch_content, size_content, cmd_0x15, cmd_0x15_size);
  si4735.setSSBConfig(bandwidthSSB[bwIdxSSB].idx, 1, 0, 1, 0, 1);
  si4735.setI2CStandardMode();
  ssbLoaded = true;
  // oled.clear();
  cleanBfoRdsInfo();
}

/*
   Switch the radio to current band.
   The bandIdx variable points to the current band.
   This function change to the band referenced by bandIdx (see table band).
*/
void useBand() {
  cleanBfoRdsInfo();
  if (band[bandIdx].bandType == FM_BAND_TYPE) {
    currentMode = FM;
    si4735.setTuneFrequencyAntennaCapacitor(0);
    si4735.setFM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, tabStep[band[bandIdx].currentStepIdx]);
    si4735.setSeekFmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
    si4735.setSeekFmSpacing(1);
    bfoOn = ssbLoaded = false;
    si4735.setRdsConfig(3, 3, 3, 3, 3);
    si4735.setFifoCount(1);
    bwIdxFM = band[bandIdx].bandwidthIdx;
    si4735.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
  } else {
    if (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE)
      si4735.setTuneFrequencyAntennaCapacitor(0);
    else
      si4735.setTuneFrequencyAntennaCapacitor(1);

    if (ssbLoaded) {
      si4735.setSSB(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, tabStep[band[bandIdx].currentStepIdx], currentMode);
      si4735.setSSBAutomaticVolumeControl(1);
      si4735.setSsbSoftMuteMaxAttenuation(0);  // Disable Soft Mute for SSB
      bwIdxSSB = band[bandIdx].bandwidthIdx;
      si4735.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
      si4735.setSSBBfo(currentBFO);
    } else {
      currentMode = AM;
      si4735.setAM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, tabStep[band[bandIdx].currentStepIdx]);
      si4735.setAutomaticGainControl((disableAgc = (agcIdx > 0)), agcNdx);
      si4735.setAmSoftMuteMaxAttenuation(smIdx);  // // Disable Soft Mute for AM
      bwIdxAM = band[bandIdx].bandwidthIdx;
      si4735.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
      bfoOn = false;
    }
    si4735.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);                                        // Consider the range all defined current band
    si4735.setSeekAmSpacing((tabStep[band[bandIdx].currentStepIdx] > 10) ? 10 : tabStep[band[bandIdx].currentStepIdx]);  // Max 10kHz for spacing

    si4735.setAvcAmMaxGain(avcIdx);
  }
  //delay(100);
  //oled.clear();
  currentFrequency = band[bandIdx].currentFreq;
  idxStep = band[bandIdx].currentStepIdx;
  showStatus();
  resetEepromDelay();
}

/**
   Changes the step frequency value based on encoder rotation
*/
void doStep(int8_t v) {
  // This command should work only for SSB mode
  if ((currentMode == LSB || currentMode == USB) && bfoOn) {
    currentBFOStep = (currentBFOStep == 25) ? 10 : 25;
    showBFO();
  } else {
    idxStep = (v == 1) ? idxStep + 1 : idxStep - 1;
    if (idxStep > lastStep)
      idxStep = 0;
    else if (idxStep < 0)
      idxStep = lastStep;

    si4735.setFrequencyStep(tabStep[idxStep]);
    band[bandIdx].currentStepIdx = idxStep;
    si4735.setSeekAmSpacing((tabStep[idxStep] > 10) ? 10 : tabStep[idxStep]);  // Max 10kHz for spacing
    showStep();
  }
}

/**
   Changes the volume based on encoder rotation
*/
void doVolume(int8_t v) {
  if (muteVolume) {
    si4735.setVolume(muteVolume);
    muteVolume = 0;
  } else if (v == 1)
    si4735.volumeUp();
  else
    si4735.volumeDown();
  showVolume();
}

/**
   Switches the AGC/Attenuation based on encoder rotation
*/
void doAttenuation(int8_t v) {
  agcIdx = (v == 1) ? agcIdx + 1 : agcIdx - 1;
  if (agcIdx < 0)
    agcIdx = 37;
  else if (agcIdx > 37)
    agcIdx = 0;

  disableAgc = (agcIdx > 0);  // if true, disable AGC; esle, AGC is enable

  if (agcIdx > 1)
    agcNdx = agcIdx - 1;
  else
    agcNdx = 0;

  // Sets AGC on/off and gain
  si4735.setAutomaticGainControl(disableAgc, agcNdx);

  showAttenuation();
  //delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
}

/**
   SoftMute control
*/
void doSoftMute(int8_t v) {
  if (currentMode != FM) {
    smIdx = (v == 1) ? smIdx + 1 : smIdx - 1;
    if (smIdx > 32)
      smIdx = 0;
    else if (smIdx < 0)
      smIdx = 32;
    si4735.setAmSoftMuteMaxAttenuation(smIdx);

    showSoftMute();
    delay(MIN_ELAPSED_TIME);  // waits a little more for releasing the button.
  }
}

/**
   Automatic Volume Control
*/
void doAvc(int8_t v) {
  if (currentMode != FM) {
    avcIdx = (v == 1) ? avcIdx + 2 : avcIdx - 2;
    if (avcIdx > 90)
      avcIdx = 12;
    else if (avcIdx < 12)
      avcIdx = 90;

    si4735.setAvcAmMaxGain(avcIdx);
    showAvc();
    delay(MIN_ELAPSED_TIME);  // waits a little more for releasing the button.
  }
}


/**
   Switches the bandwidth based on encoder rotation
*/
void doBandwidth(uint8_t v) {
  if (currentMode == LSB || currentMode == USB) {
    bwIdxSSB = (v == 1) ? bwIdxSSB + 1 : bwIdxSSB - 1;

    if (bwIdxSSB > 5)
      bwIdxSSB = 0;
    else if (bwIdxSSB < 0)
      bwIdxSSB = 5;

    band[bandIdx].bandwidthIdx = bwIdxSSB;

    si4735.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
    // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
    if (bandwidthSSB[bwIdxSSB].idx == 0 || bandwidthSSB[bwIdxSSB].idx == 4 || bandwidthSSB[bwIdxSSB].idx == 5)
      si4735.setSSBSidebandCutoffFilter(0);
    else
      si4735.setSSBSidebandCutoffFilter(1);
  } else if (currentMode == AM) {
    bwIdxAM = (v == 1) ? bwIdxAM + 1 : bwIdxAM - 1;

    if (bwIdxAM > maxFilterAM)
      bwIdxAM = 0;
    else if (bwIdxAM < 0)
      bwIdxAM = maxFilterAM;

    band[bandIdx].bandwidthIdx = bwIdxAM;
    si4735.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
  } else {
    bwIdxFM = (v == 1) ? bwIdxFM + 1 : bwIdxFM - 1;
    if (bwIdxFM > 4)
      bwIdxFM = 0;
    else if (bwIdxFM < 0)
      bwIdxFM = 4;

    band[bandIdx].bandwidthIdx = bwIdxFM;
    si4735.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
  }
  showBandwidth();
}

/**
   disble command buttons and keep the current status of the last command button pressed
*/
void disableCommand(bool *b, bool value, void (*showFunction)()) {
  static bool anyOn = false;
  if (anyOn) {
    cmdVolume = false;
    cmdAgcAtt = false;
    cmdStep = false;
    cmdBw = false;
    cmdBand = false;
    cmdSoftMute = false;
    cmdAvc = false;
    showVolume();
    showStep();
    showAttenuation();
    showBandwidth();
    showBandDesc();

    anyOn = false;
  }
  if (b != NULL)  // rescues the last status of the last command only the parameter is not null
    *b = anyOn = value;
  if (showFunction != NULL)  //  show the desired status only if it is necessary.
    showFunction();

  elapsedRSSI = millis();
  countRSSI = 0;
}

void loop() {
#if defined(DEBUG) && defined(DEBUG_BUTTONS_ONLY)
  btn_BandUp.checkEvent(buttonEvent);
  btn_BandDn.checkEvent(buttonEvent);
  btn_VolumeUp.checkEvent(buttonEvent);
  btn_VolumeDn.checkEvent(buttonEvent);
  btn_Bandwidth.checkEvent(buttonEvent);
  btn_AGC.checkEvent(buttonEvent);
  btn_Step.checkEvent(buttonEvent);
  btn_Mode.checkEvent(buttonEvent);
  btn_Encoder.checkEvent(buttonEvent);
  return;
#endif
  // Check if the encoder has moved.
  if (encoderCount != 0) {
    if (cmdVolume)
      doVolume(encoderCount);
    else if (cmdAgcAtt)
      doAttenuation(encoderCount);
    else if (cmdStep)
      doStep(encoderCount);
    else if (cmdBw)
      doBandwidth(encoderCount);
    else if (cmdSoftMute)
      doSoftMute(encoderCount);
    else if (cmdAvc)
      doAvc(encoderCount);
    else if (cmdBand) {
      if (encoderCount == 1)
        bandUp();
      else
        bandDown();
    } else if (bfoOn) {
      currentBFO = (encoderCount == 1) ? (currentBFO + currentBFOStep) : (currentBFO - currentBFOStep);
      si4735.setSSBBfo(currentBFO);
      previousFrequency = 0;  // Forces eeprom update
      showBFO();
    } else {
      if (encoderCount == 1) {
        si4735.frequencyUp();
        seekDirection = 1;
      } else {
        si4735.frequencyDown();
        seekDirection = 0;
      }
      // Show the current frequency only if it has changed
      currentFrequency = si4735.getFrequency();
      showFrequency();
    }
    encoderCount = 0;
    resetEepromDelay();  // if you moved the encoder, something was changed
    elapsedRSSI = millis();
    countRSSI = 0;
  }

  // Check button commands
  // check if some button is pressed
  // Shortpress-Events are handled direct, longpress-Events by the specific "event-handler" (the callback function passed to
  // checkEvent
  // If buttonEvent is used as callback, there is no specific functionality attached to longpress, but info is logged on Serial
  // if the flag "DEBUG" is defined (see above)

  if (BUTTONEVENT_SHORTPRESS == btn_Bandwidth.checkEvent(bandwidthEvent)) {
    cmdBw = !cmdBw;
    disableCommand(&cmdBw, cmdBw, showBandwidth);
  }
  if (BUTTONEVENT_SHORTPRESS == btn_BandUp.checkEvent(bandEvent)) {
    cmdBand = !cmdBand;
    disableCommand(&cmdBand, cmdBand, showBandDesc);
  }
  if (BUTTONEVENT_SHORTPRESS == btn_BandDn.checkEvent(bandEvent)) {
    if (currentMode != FM) {
      cmdSoftMute = !cmdSoftMute;
      disableCommand(&cmdSoftMute, cmdSoftMute, showSoftMute);
    }
  }
  if (BUTTONEVENT_SHORTPRESS == btn_VolumeUp.checkEvent(volumeEvent)) {
    cmdVolume = !cmdVolume;
    disableCommand(&cmdVolume, cmdVolume, showVolume);
  }
  if (BUTTONEVENT_SHORTPRESS == btn_VolumeDn.checkEvent(volumeEvent)) {
    if (currentMode != FM) {
      cmdAvc = !cmdAvc;
      disableCommand(&cmdAvc, cmdAvc, showAvc);
    }
  }
  if (BUTTONEVENT_SHORTPRESS == btn_Encoder.checkEvent(encoderEvent)) {
    if (currentMode == LSB || currentMode == USB) {
      bfoOn = !bfoOn;
      if (bfoOn)
        showBFO();
      showStatus();
      disableCommand(NULL, false, NULL);  // disable all command buttons
    } else if (currentMode == FM || currentMode == AM) {
      // Jumps up or down one space
      if (seekDirection)
        si4735.frequencyUp();
      else
        si4735.frequencyDown();

      si4735.seekStationProgress(showFrequencySeek, checkStopSeeking, seekDirection);
      delay(30);
      if (currentMode == FM) {
        float f = round(si4735.getFrequency() / 10.0);
        currentFrequency = (uint16_t)f * 10;  // adjusts band space from 1 (10kHz) to 10 (100 kHz)
        si4735.setFrequency(currentFrequency);
      } else {
        currentFrequency = si4735.getFrequency();  //
      }
      showFrequency();
    }
  }
  if (BUTTONEVENT_SHORTPRESS == btn_AGC.checkEvent(agcEvent)) {
    if (currentMode != FM) {
      cmdAgcAtt = !cmdAgcAtt;
      disableCommand(&cmdAgcAtt, cmdAgcAtt, showAttenuation);
    }
  }
  if (BUTTONEVENT_SHORTPRESS == btn_Step.checkEvent(stepEvent)) {
    if (currentMode != FM) {
      cmdStep = !cmdStep;
      disableCommand(&cmdStep, cmdStep, showStep);
    }
  }
  if (BUTTONEVENT_SHORTPRESS == btn_Mode.checkEvent(modeEvent)) {
    if (currentMode != FM) {
      if (currentMode == AM) {
        // If you were in AM mode, it is necessary to load SSB patch (avery time)
        loadSSB();
        currentMode = LSB;
      } else if (currentMode == LSB) {
        currentMode = USB;
      } else if (currentMode == USB) {
        currentMode = AM;
        ssbLoaded = false;
        bfoOn = false;
      }
      // Nothing to do if you are in FM mode
      band[bandIdx].currentFreq = currentFrequency;
      band[bandIdx].currentStepIdx = idxStep;
      useBand();
    }
  }

  // Show RSSI status only if this condition has changed
  if ((millis() - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 9) {
    si4735.getCurrentReceivedSignalQuality();
    int aux = si4735.getCurrentRSSI();
    if (rssi != aux) {
      rssi = aux;
      showRSSI();
    }

    if (countRSSI++ > 3) {
      disableCommand(NULL, false, NULL);  // disable all command buttons
      countRSSI = 0;
    }
    elapsedRSSI = millis();
  }

  if (currentMode == FM) {
    checkRDS();
  }

  // Show the current frequency only if it has changed
  if (currentFrequency != previousFrequency) {
    if ((millis() - storeTime) > STORE_TIME) {
      saveAllReceiverInformation();
      storeTime = millis();
      previousFrequency = currentFrequency;
    }
  }
  delay(10);
}
