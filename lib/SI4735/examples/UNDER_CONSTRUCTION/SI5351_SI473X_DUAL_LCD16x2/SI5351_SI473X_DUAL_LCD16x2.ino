/*
  UNDER CONSTRUCTION... 
  This sketch was not tested on a hardware so far.

  IMPORTANT:
  This sketch is an EXPERIMENT with dual converter design.
  It can be compiled on Arduino ATmega328 and Arduino Mega 
  Please set the constants SI5351_CALIBRATE_VALUE and IF_OFFSET to the correct values. 

  Features: 
                1) Dual converter
                2) The receiver current status is stored into Arduino EEPROM;
                3) ***FM RDS*** Disabled for while;
                4) FM frequency step;
                5) FM Bandwidth control.


  On ATmega 328, please, use the MiniCore manager : "LTO enabled" and "No bootloader". 

  See user_manual.txt before operating the receiver. 

  This sketch was built to work with the project "DIY Si4730 All Band Radio (LW, MW, SW, FM)" receiver from Mirko Pavleski.
  The original project can be found on https://create.arduino.cc/projecthub/mircemk/diy-si4730-all-band-radio-lw-mw-sw-fm-1894d9
  Please, follow the circuit available on that link.

  If you are using a SI4735-D60 or SI4732-A10, you can also use this sketch to add the SSB functionalities to the
  original Pavleski's project. If you are using another SI4730-D60, the SSB wil not work. But you will still have
  the SW functionalities.

  It is important to say that this sketch was designed to work with the circuit implemented by Mirko Pavleski (see link above).
  The visual interface, control commands, band plan, and some functionalities are different if compared with the original
  sketch. Be sure you are using the SI4735 Arduino Library written by PU2CLR to run this sketch. The library used by the original
  sketch will not work here. Also, you have to install the LiquidCrystal library.

  It is  a  complete  radio  capable  to  tune  LW,  MW,  SW  on  AM  and  SSB  mode  and  also  receive  the
  regular  comercial  stations.

  Features:   AM; SSB; LW/MW/SW; external mute circuit control; AGC; Attenuation gain control;
              SSB filter; CW; AM filter; 1, 5, 10, 50 and 500kHz step on AM and 10Hhz sep on SSB

  Wire up on Arduino UNO, Pro mini and SI4735-D60
  | Device name               | Device Pin / Description      |  Arduino Pin  |
  | ----------------          | ----------------------------- | ------------  |
  |    LCD 16x2 or 20x4       |                               |               |
  |                           | D4                            |     D7        |
  |                           | D5                            |     D6        |
  |                           | D6                            |     D5        |
  |                           | D7                            |     D4        |
  |                           | RS                            |     D12       |
  |                           | E/ENA                         |     D13       |
  |                           | RW & VSS & K (16)             |    GND        |
  |                           | A (15) & VDD                  |    +Vcc       |
  |                           | VO (see 20K tripot connection)|   ---------   |
  |     SS473X                |                               |               |
  |                           | RESET (pin 15)                |      9        |
  |                           | SDIO (pin 18)                 |     A4        |
  |                           | SCLK (pin 17)                 |     A5        |
  |                           | (*1)SEN (pin 16)              |  +Vcc or GND  |
  |    Encoder                |                               |               |
  |                           | A                             |       2       |
  |                           | B                             |       3       |
  |                           | PUSH BUTTON (encoder)         |     A0/14     |

  (*1) If you are using the SI4732-A10, check the corresponding pin numbers.
  (*1) The PU2CLR SI4735 Arduino Library has resources to detect the I2C bus address automatically.
       It seems the original project connect the SEN pin to the +Vcc. By using this sketch, you do
       not need to worry about this setting.
  ATTENTION: Read the file user_manual.txt
  Prototype documentation: https://pu2clr.github.io/SI4735/
  PU2CLR Si47XX API documentation: https://pu2clr.github.io/SI4735/extras/apidoc/html/

  By PU2CLR, Ricardo, May  2021.
*/

#include <SI4735.h>
#include <si5351.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include "Rotary.h"

// #include <patch_init.h> // SSB patch for whole SSBRX initialization string
#include <patch_ssb_compressed.h> // Compressed SSB patch version (saving almost 1KB)

const uint16_t size_content = sizeof ssb_patch_content; // See ssb_patch_content.h
const uint16_t cmd_0x15_size = sizeof cmd_0x15;         // Array of lines where the 0x15 command occurs in the patch content.


#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3

#define RESET_PIN 9

// Enconder PINs
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3

// LCD 16x02 or LCD20x4 PINs
#define LCD_D7 4
#define LCD_D6 5
#define LCD_D5 6
#define LCD_D4 7
#define LCD_RS 12
#define LCD_E 13

// Buttons controllers
#define ENCODER_PUSH_BUTTON 14 // Pin A0/14
#define DUMMY_BUTTON 15

#define MIN_ELAPSED_TIME 300
#define MIN_ELAPSED_RSSI_TIME 500
#define ELAPSED_COMMAND 2000 // time to turn off the last command controlled by encoder. Time to goes back to the FVO control
#define ELAPSED_CLICK 1500   // time to check the double click commands
#define DEFAULT_VOLUME 35    // change it for your favorite sound volume

// SI5351 PARAMETERS
#define SI5351_CALIBRATE_VALUE 80000 // See how to calibrate your Si5351A (0 if you do not want).
#define IF_OFFSET 0         // Set the IF offset you are using. For example: +1070000000LLU (increase 10.7MHz) or -1070000000LLU (decrease 10.7Mhz); +45500000LU (455kHz) or -45500000LU (-455kHz) 
#define FREQUENCY_UP 1
#define FREQUENCY_DOWN -1      


#define FM 0
#define LSB 1
#define USB 2
#define AM 3
#define LW 4

#define SSB 1

#define EEPROM_SIZE        512

#define STORE_TIME 10000 // Time of inactivity to make the current receiver status writable (10s / 10000 milliseconds).

// EEPROM - Stroring control variables
const uint8_t app_id = 31; // Useful to check the EEPROM content before processing useful data
const int eeprom_address = 0;
long storeTime = millis();

bool itIsTimeToSave = false;

bool bfoOn = false;
bool ssbLoaded = false;

int8_t agcIdx = 0;
uint8_t disableAgc = 0;
int8_t agcNdx = 0;
int8_t softMuteMaxAttIdx = 4;
int8_t avcIdx;   // min 12 and max 90 
uint8_t countClick = 0;

uint8_t seekDirection = 1;

bool cmdBand = false;
bool cmdVolume = false;
bool cmdAgc = false;
bool cmdBandwidth = false;
bool cmdStep = false;
bool cmdMode = false;
bool cmdMenu = false;
bool cmdRds  =  false;
bool cmdSoftMuteMaxAtt = false;
bool cmdAvc = false;


bool fmRDS = false;

int16_t currentBFO = 0;
long elapsedRSSI = millis();
long elapsedButton = millis();
long elapsedCommand = millis();
long elapsedClick = millis();
volatile int encoderCount = 0;
uint16_t currentFrequency;
uint16_t previousFrequency = 0;


const uint8_t currentBFOStep = 10;

const char * menu[] = {"Volume", "FM RDS", "Step", "Mode", "BFO", "BW", "AGC/Att", "SoftMute", "AVC", "Seek Up", "Seek Down"};
int8_t menuIdx = 0;
const int lastMenu = 10;
int8_t currentMenuCmd = -1;

typedef struct
{
  uint8_t idx;      // SI473X device bandwidth index
  const char *desc; // bandwidth description
} Bandwidth;

int8_t bwIdxSSB = 4;
const int8_t maxSsbBw = 5;
Bandwidth bandwidthSSB[] = {
  {4, "0.5"},
  {5, "1.0"},
  {0, "1.2"},
  {1, "2.2"},
  {2, "3.0"},
  {3, "4.0"}
};


int8_t bwIdxAM = 4;
const int8_t maxAmBw = 6;
Bandwidth bandwidthAM[] = {
  {4, "1.0"},
  {5, "1.8"},
  {3, "2.0"},
  {6, "2.5"},
  {2, "3.0"},
  {1, "4.0"},
  {0, "6.0"}
};

int8_t bwIdxFM = 0;
const int8_t maxFmBw = 4;

Bandwidth bandwidthFM[] = {
    {0, "AUT"}, // Automatic - default
    {1, "110"}, // Force wide (110 kHz) channel filter.
    {2, " 84"},
    {3, " 60"},
    {4, " 40"}};



int tabAmStep[] = {1,    // 0
                   5,    // 1
                   9,    // 2
                   10,   // 3
                   50,   // 4
                   100}; // 5

const int lastAmStep = (sizeof tabAmStep / sizeof(int)) - 1;
int idxAmStep = 3;

int tabFmStep[] = {5, 10, 20};
const int lastFmStep = (sizeof tabFmStep / sizeof(int)) - 1;
int idxFmStep = 1;

uint16_t currentStepIdx = 1;


const char *bandModeDesc[] = {"FM ", "LSB", "USB", "AM "};
uint8_t currentMode = FM;

/**
 *  Band data structure
 */
typedef struct
{
  const char *bandName;   // Band description
  uint8_t bandType;       // Band type (FM, MW or SW)
  uint16_t minimumFreq;   // Minimum frequency of the band
  uint16_t maximumFreq;   // maximum frequency of the band
  uint16_t currentFreq;   // Default frequency or current frequency
  int8_t currentStepIdx;  // Idex of tabStepAM:  Defeult frequency step (See tabStepAM)
  int8_t bandwidthIdx;    // Index of the table bandwidthFM, bandwidthAM or bandwidthSSB;
  uint8_t disableAgc;
  int8_t agcIdx;
  int8_t agcNdx;
  int8_t avcIdx; 
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
    {"VHF", FM_BAND_TYPE, 6400, 10800, 10390, 1, 0, 1, 0, 0, 0},
    {"MW1", MW_BAND_TYPE, 150, 1720, 810, 3, 4, 0, 0, 0, 32},
    {"80M", MW_BAND_TYPE, 1700, 4000, 3700, 0, 4, 1, 0, 0, 32},
    {"SW1", SW_BAND_TYPE, 4000, 6500, 6000, 1, 4, 1, 0, 0, 32},
    {"40M", SW_BAND_TYPE, 6500, 7300, 7100, 0, 4, 1, 0, 0, 40},
    {"SW2", SW_BAND_TYPE, 7200, 14000, 7200, 1, 4, 1, 0, 0, 40},
    {"20M", SW_BAND_TYPE, 14000, 15000, 14200, 0, 4, 1, 0, 0, 42},
    {"SW7", SW_BAND_TYPE, 15000, 18000, 15300, 1, 4, 1, 0, 0, 42},
    {"15M", SW_BAND_TYPE, 20000, 21400, 21100, 0, 4, 1, 0, 0, 44},
    {"SW9", SW_BAND_TYPE, 21400, 22800, 21500, 1, 4, 1, 0, 0, 44},
    {"CB ", SW_BAND_TYPE, 26000, 28000, 27500, 0, 4, 1, 0, 0, 44},
    {"10M", SW_BAND_TYPE, 28000, 30000, 28400, 0, 4, 1, 0, 0, 44},
    {"ALL", SW_BAND_TYPE, 150, 30000, 15000, 0, 4, 1, 0, 0, 48} // All band. LW, MW and SW (from 150kHz to 30MHz)
};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int bandIdx = 0;
int tabStep[] = {1, 5, 10, 50, 100, 500, 1000};
const int lastStep = (sizeof tabStep / sizeof(int)) - 1;


uint8_t rssi = 0;
uint8_t volume = DEFAULT_VOLUME;

// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
SI4735 rx;
Si5351 vfo;

void setup()
{
  // Encoder pins
  pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  lcd.begin(16, 2);

  // Splash - Remove or Change the splash message
  lcd.setCursor(0, 0);
  lcd.print("PU2CLR-SI4735");
  lcd.setCursor(0, 1);
  lcd.print("Arduino Library");
  Flash(1500);
  lcd.setCursor(0, 0);
  lcd.print("DIY Mirko Radio");
  lcd.setCursor(0, 1);
  lcd.print("By RICARDO/2021");
  Flash(2000);
  // End splash

  EEPROM.begin();

  // If you want to reset the eeprom, keep the VOLUME_UP button pressed during statup
  if (digitalRead(ENCODER_PUSH_BUTTON) == LOW)
  {
    EEPROM.update(eeprom_address, 0);
    lcd.setCursor(0,0);
    lcd.print("EEPROM RESETED");
    delay(2000);
    lcd.clear();
  }

  vfo.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  vfo.set_correction(SI5351_CALIBRATE_VALUE, SI5351_PLL_INPUT_XO);
  vfo.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  vfo.set_freq(10700, SI5351_CLK0); // Start CLK0 (VFO)
  vfo.update_status();
  delay(100);

  // controlling encoder via interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

  rx.setI2CFastModeCustom(100000);
  
  rx.getDeviceI2CAddress(RESET_PIN); // Looks for the I2C bus address and set it.  Returns 0 if error
  
  rx.setup(RESET_PIN, MW_BAND_TYPE);
  // Comment the line above and uncomment the three lines below if you are using external ref clock (active crystal or signal generator)
  // rx.setRefClock(32768);
  // rx.setRefClockPrescaler(1);   // will work with 32768  
  // rx.setup(RESET_PIN, 0, MW_BAND_TYPE, SI473X_ANALOG_AUDIO, XOSCEN_RCLK);

  
  delay(300);
  rx.setAvcAmMaxGain(48); // Sets the maximum gain for automatic volume control on AM/SSB mode (you can use values between 12 and 90dB).


  // Checking the EEPROM content
  if (EEPROM.read(eeprom_address) == app_id)
  {
    readAllReceiverInformation();
  } else 
    rx.setVolume(volume);
  
  useBand();
  showStatus();
}

/**
 *  Cleans the screen with a visual effect.
 */
void Flash(int d)
{
  delay(d);
  lcd.clear();
  lcd.noDisplay();
  delay(500);
  lcd.display();
}

/*
   writes the conrrent receiver information into the eeprom.
   The EEPROM.update avoid write the same data in the same memory position. It will save unnecessary recording.
*/
void saveAllReceiverInformation()
{
  int addr_offset;

  EEPROM.begin();

  EEPROM.update(eeprom_address, app_id);                 // stores the app id;
  EEPROM.update(eeprom_address + 1, rx.getVolume()); // stores the current Volume
  EEPROM.update(eeprom_address + 2, bandIdx);            // Stores the current band
  EEPROM.update(eeprom_address + 3, fmRDS);
  EEPROM.update(eeprom_address + 4, currentMode); // Stores the current Mode (FM / AM / SSB)
  EEPROM.update(eeprom_address + 5, currentBFO >> 8);
  EEPROM.update(eeprom_address + 6, currentBFO & 0XFF);

  addr_offset = 7;
  band[bandIdx].currentFreq = currentFrequency;

  for (int i = 0; i <= lastBand; i++)
  {
    EEPROM.update(addr_offset++, (band[i].currentFreq >> 8));   // stores the current Frequency HIGH byte for the band
    EEPROM.update(addr_offset++, (band[i].currentFreq & 0xFF)); // stores the current Frequency LOW byte for the band
    EEPROM.update(addr_offset++, band[i].currentStepIdx);       // Stores current step of the band
    EEPROM.update(addr_offset++, band[i].bandwidthIdx);         // table index (direct position) of bandwidth
    EEPROM.update(addr_offset++, band[i].disableAgc );
    EEPROM.update(addr_offset++, band[i].agcIdx);
    EEPROM.update(addr_offset++, band[i].agcNdx);
    EEPROM.update(addr_offset++, band[i].avcIdx);

  }

  EEPROM.end();
}

/**
 * reads the last receiver status from eeprom. 
 */
void readAllReceiverInformation()
{
  uint8_t volume;
  int addr_offset;
  int bwIdx;

  volume = EEPROM.read(eeprom_address + 1); // Gets the stored volume;
  bandIdx = EEPROM.read(eeprom_address + 2);
  fmRDS = EEPROM.read(eeprom_address + 3);
  currentMode = EEPROM.read(eeprom_address + 4);
  currentBFO = EEPROM.read(eeprom_address + 5) << 8;
  currentBFO |= EEPROM.read(eeprom_address + 6);

  addr_offset = 7;
  for (int i = 0; i <= lastBand; i++)
  {
    band[i].currentFreq = EEPROM.read(addr_offset++) << 8;
    band[i].currentFreq |= EEPROM.read(addr_offset++);
    band[i].currentStepIdx = EEPROM.read(addr_offset++);
    band[i].bandwidthIdx = EEPROM.read(addr_offset++);
    band[i].disableAgc = EEPROM.read(addr_offset++);
    band[i].agcIdx = EEPROM.read(addr_offset++);
    band[i].agcNdx = EEPROM.read(addr_offset++);
    band[i].avcIdx = EEPROM.read(addr_offset++);
  }


  currentFrequency = band[bandIdx].currentFreq;

  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    currentStepIdx = idxFmStep = band[bandIdx].currentStepIdx;
    rx.setFrequencyStep(tabFmStep[currentStepIdx]);
  }
  else
  {
    currentStepIdx = idxAmStep = band[bandIdx].currentStepIdx;
    rx.setFrequencyStep(tabAmStep[currentStepIdx]);
  }

  bwIdx = band[bandIdx].bandwidthIdx;

  if (currentMode == LSB || currentMode == USB)
  {
    loadSSB();
    bwIdxSSB = (bwIdx > 5) ? 5 : bwIdx;
    rx.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
    // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
    if (bandwidthSSB[bwIdxSSB].idx == 0 || bandwidthSSB[bwIdxSSB].idx == 4 || bandwidthSSB[bwIdxSSB].idx == 5)
      rx.setSSBSidebandCutoffFilter(0);
    else
      rx.setSSBSidebandCutoffFilter(1);
  }
  else if (currentMode == AM)
  {
    bwIdxAM = bwIdx;
    rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
  }
  else
  {
    bwIdxFM = bwIdx;
    rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
  }

  delay(50);
  rx.setVolume(volume);
}

/*
 * To store any change into the EEPROM, it is needed at least STORE_TIME  milliseconds of inactivity.
 */
void resetEepromDelay()
{
  elapsedCommand = storeTime = millis();
  itIsTimeToSave = true;
}

/**
    Set all command flags to false
    When all flags are disabled (false), the encoder controls the frequency
*/
void disableCommands()
{
  cmdBand = false;
  bfoOn = false;
  cmdVolume = false;
  cmdAgc = false;
  cmdBandwidth = false;
  cmdStep = false;
  cmdMode = false;
  cmdMenu = false;
  cmdSoftMuteMaxAtt = false;
  cmdRds = false;
  cmdAvc =  false; 
  countClick = 0;

  showCommandStatus((char *) "VFO ");
}



/**
 * Reads encoder via interrupt
 * Use Rotary.h and  Rotary.cpp implementation to process encoder via interrupt
 * if you do not add ICACHE_RAM_ATTR declaration, the system will reboot during attachInterrupt call. 
 * With ICACHE_RAM_ATTR macro you put the function on the RAM.
 */
void  rotaryEncoder()
{ // rotary encoder events
  uint8_t encoderStatus = encoder.process();
  if (encoderStatus)
    encoderCount = (encoderStatus == DIR_CW) ? 1 : -1;
}

/**
 * Shows frequency information on Display
 */
void showFrequency()
{
  char bufferDisplay[15];
  char *unit;
  int col = 4;
  if (rx.isCurrentTuneFM())
  {
    rx.convertToChar(currentFrequency, bufferDisplay, 5, 3, ',');
    if ( fmRDS ) { 
      col = 0;
      unit = (char *)" ";
    } else {
      unit = (char *)"MHz";
    }
  }
  else
  {
    if (currentFrequency < 1000)
      rx.convertToChar(currentFrequency, bufferDisplay, 5, 0, '.');
    else
      rx.convertToChar(currentFrequency, bufferDisplay, 5, 2, '.');
    unit = (char *)"kHz";
  }
  strcat(bufferDisplay, unit);
  lcd.setCursor(col, 1);
  lcd.print(bufferDisplay);
  showMode();
}

/**
 * Shows the current mode
 */
void showMode()
{
  char *bandMode;

   
  if (currentFrequency < 520)
    bandMode = (char *)"LW  ";
  else
    bandMode = (char *)bandModeDesc[currentMode];
  lcd.setCursor(0, 0);
  lcd.print(bandMode);

  if ( currentMode == FM && fmRDS ) return;
  
  lcd.setCursor(0, 1);
  lcd.print(band[bandIdx].bandName);
}

/**
 * Shows some basic information on display
 */
void showStatus()
{
  lcd.clear();
  showFrequency();
  showRSSI();
}

/**
 *  Shows the current Bandwidth status
 */
void showBandwidth()
{
  char *bw;
  if (currentMode == LSB || currentMode == USB)
  {
    bw = (char *)bandwidthSSB[bwIdxSSB].desc;
    showBFO();
  }
  else if (currentMode == AM)
  {
    bw = (char *)bandwidthAM[bwIdxAM].desc;
  }
  else
  {
    bw = (char *)bandwidthFM[bwIdxFM].desc;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BW: ");
  lcd.print(bw);
}

/**
 *   Shows the current RSSI and SNR status
 */
void showRSSI()
{
  int rssiAux = 0;
  char sMeter[7];

  if (currentMode == FM)
  {
    lcd.setCursor(14, 0);
    lcd.print((rx.getCurrentPilot()) ? "ST" : "MO");
    lcd.setCursor(10, 0);
    if ( fmRDS ) {
      lcd.print("RDS");
      return;
    }
    else 
      lcd.print("   ");
  }

    
  if (rssi < 2)
    rssiAux = 4;
  else if (rssi < 4)
    rssiAux = 5;
  else if (rssi < 12)
    rssiAux = 6;
  else if (rssi < 25)
    rssiAux = 7;
  else if (rssi < 50)
    rssiAux = 8;
  else
    rssiAux = 9;

  sMeter[0] = 'S';
  sMeter[1] = rssiAux + 48; // Converts number to asc
  sMeter[2] = (rssi >= 60) ? '+' : '.';
  sMeter[3] = '\0'; 

  lcd.setCursor(13, 1);
  lcd.print(sMeter);
}

/**
 *    Shows the current AGC and Attenuation status
 */
void showAgcAtt()
{
  char sAgc[15];
  lcd.clear();
  rx.getAutomaticGainControl();
  if ( !disableAgc /*agcNdx == 0 && agcIdx == 0 */ )
    strcpy(sAgc, "AGC ON");
  else {
    strcpy(sAgc, "ATT: ");
    rx.convertToChar(agcNdx, &sAgc[5],2,0,'.');
  }

  lcd.setCursor(0, 0);
  lcd.print(sAgc);
}


/**
 *   Shows the current step
 */
void showStep()
{
  char sStep[15];
  int st = (currentMode == FM) ? (tabFmStep[currentStepIdx] * 10) : tabAmStep[currentStepIdx];
  strcpy(sStep, "Stp: ");
  rx.convertToChar(st, &sStep[5], 4, 0, '.');
  lcd.setCursor(0, 0);
  lcd.print(sStep);
}

/**
 *  Shows the current BFO value
 */
void showBFO()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BFO: ");
  lcd.print(currentBFO);
  elapsedCommand = millis();
}

/*
 *  Shows the volume level on LCD
 */
void showVolume()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("VOLUME: ");
  lcd.print(rx.getVolume());
}

/**
 * Show Soft Mute 
 */
void showSoftMute()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Soft Mute: ");
  lcd.print(softMuteMaxAttIdx);
}


/**
 * Show Soft Mute 
 */
void showAvc()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AVC: ");
  lcd.print(avcIdx);
}



/**
 * Shows RDS ON or OFF
 */
void showRdsSetup() 
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("RDS: ");
  lcd.print((fmRDS) ? "ON " : "OFF");
}

/***************  
 *   RDS
 *   
 */

char *stationName;
char bufferStatioName[20];

void clearRDS() {
  /*
   stationName = (char *) "           ";
   showRDSStation();
   */
}

void showRDSStation()
{
  /*
    int col = 8;
    for (int i = 0; i < 8; i++ ) {
      if (stationName[i] != bufferStatioName[i] ) {
        lcd.setCursor(col + i, 1);
        lcd.print(stationName[i]); 
        bufferStatioName[i] = stationName[i];
      }
    }
    delay(100);
  */
}

/*
 * Checks the station name is available
 */
void checkRDS()
{
  /*
  rx.getRdsStatus();
  if (rx.getRdsReceived())
  {
    if (rx.getRdsSync() && rx.getRdsSyncFound() && !rx.getRdsSyncLost() && !rx.getGroupLost() )
    {
      stationName = rx.getRdsText0A();
      if (stationName != NULL )
      {
        showRDSStation();
      }
    }
  }
  */
}


/**
 *   Sets Band up (1) or down (!1)
 */
void setBand(int8_t up_down)
{
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStepIdx = currentStepIdx;
  if (up_down == 1)
    bandIdx = (bandIdx < lastBand) ? (bandIdx + 1) : 0;
  else
    bandIdx = (bandIdx > 0) ? (bandIdx - 1) : lastBand;
  useBand();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
}

/**
 * Switch the radio to current band
 */
void useBand()
{
  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    currentMode = FM;
    rx.setTuneFrequencyAntennaCapacitor(0);
    rx.setFM(IF_OFFSET - 1000, IF_OFFSET + 1000, IF_OFFSET, tabFmStep[band[bandIdx].currentStepIdx]);
    
    rx.setSeekFmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
    // rx.setRdsConfig(1, 2, 2, 2, 2);
    // rx.setFifoCount(1);
    
    bfoOn = ssbLoaded = false;
    bwIdxFM = band[bandIdx].bandwidthIdx;
    rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);    
  }
  else
  {

    disableAgc = band[bandIdx].disableAgc;
    agcIdx = band[bandIdx].agcIdx;
    agcNdx = band[bandIdx].agcNdx;
    avcIdx = band[bandIdx].avcIdx;
    
    // set the tuning capacitor for SW or MW/LW
    rx.setTuneFrequencyAntennaCapacitor((band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE) ? 0 : 1);
    if (ssbLoaded)
    {
      rx.setSSB(IF_OFFSET - 1000, IF_OFFSET + 1000, IF_OFFSET, tabAmStep[band[bandIdx].currentStepIdx], currentMode);
      rx.setSSBAutomaticVolumeControl(1);
      rx.setSsbSoftMuteMaxAttenuation(softMuteMaxAttIdx); // Disable Soft Mute for SSB
      bwIdxSSB = band[bandIdx].bandwidthIdx;
      rx.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
      delay(500);
      rx.setSsbAgcOverrite(disableAgc, agcNdx);
    }
    else
    {
      currentMode = AM;
      rx.setAM(IF_OFFSET - 1000, IF_OFFSET + 1000, IF_OFFSET, tabAmStep[band[bandIdx].currentStepIdx]);
      bfoOn = false;
      bwIdxAM = band[bandIdx].bandwidthIdx;
      rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
      rx.setAmSoftMuteMaxAttenuation(softMuteMaxAttIdx); // Soft Mute for AM or SSB
      rx.setAutomaticGainControl(disableAgc, agcNdx);

    }
    rx.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq); // Consider the range all defined current band
    rx.setSeekAmSpacing(5); // Max 10kHz for spacing
    rx.setAvcAmMaxGain(avcIdx);
  }
  delay(100);
  currentFrequency = band[bandIdx].currentFreq;
  currentStepIdx = band[bandIdx].currentStepIdx;

  vfo.set_freq(currentFrequency + IF_OFFSET, SI5351_CLK0); 

  rssi = 0;
  showStatus();
  showCommandStatus((char *) "Band");
}


void loadSSB() {
  // rx.setI2CFastModeCustom(700000); // It is working. Faster, but I'm not sure if it is safe.
  rx.setI2CFastModeCustom(400000);
  rx.queryLibraryId(); // Is it really necessary here? I will check it.
  rx.patchPowerUp();
  delay(50);
  rx.downloadCompressedPatch(ssb_patch_content, size_content, cmd_0x15, cmd_0x15_size);
  rx.setSSBConfig(bandwidthSSB[bwIdxSSB].idx, 1, 0, 1, 0, 1);
  rx.setI2CStandardMode();
  ssbLoaded = true;
   
}

/**
 *  Switches the Bandwidth
 */
void doBandwidth(int8_t v)
{
    if (currentMode == LSB || currentMode == USB)
    {
      bwIdxSSB = (v == 1) ? bwIdxSSB + 1 : bwIdxSSB - 1;

      if (bwIdxSSB > maxSsbBw)
        bwIdxSSB = 0;
      else if (bwIdxSSB < 0)
        bwIdxSSB = maxSsbBw;

      rx.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
      // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
      if (bandwidthSSB[bwIdxSSB].idx == 0 || bandwidthSSB[bwIdxSSB].idx == 4 || bandwidthSSB[bwIdxSSB].idx == 5)
        rx.setSSBSidebandCutoffFilter(0);
      else
        rx.setSSBSidebandCutoffFilter(1);

      band[bandIdx].bandwidthIdx = bwIdxSSB;
    }
    else if (currentMode == AM)
    {
      bwIdxAM = (v == 1) ? bwIdxAM + 1 : bwIdxAM - 1;

      if (bwIdxAM > maxAmBw)
        bwIdxAM = 0;
      else if (bwIdxAM < 0)
        bwIdxAM = maxAmBw;

      rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
      band[bandIdx].bandwidthIdx = bwIdxAM;
      
    } else {
    bwIdxFM = (v == 1) ? bwIdxFM + 1 : bwIdxFM - 1;
    if (bwIdxFM > maxFmBw)
      bwIdxFM = 0;
    else if (bwIdxFM < 0)
      bwIdxFM = maxFmBw;

    rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
    band[bandIdx].bandwidthIdx = bwIdxFM;
  }
  showBandwidth();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
}

/**
 * Show cmd on display. It means you are setting up something.  
 */
void showCommandStatus(char * currentCmd)
{
  lcd.setCursor(5, 0);
  lcd.print(currentCmd);
}

/**
 * Show menu options
 */
void showMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.setCursor(0, 1);
  lcd.print(menu[menuIdx]);
  showCommandStatus( (char *) "Menu");
}

/**
 *  AGC and attenuattion setup
 */
void doAgc(int8_t v) {
  agcIdx = (v == 1) ? agcIdx + 1 : agcIdx - 1;
  if (agcIdx < 0 )
    agcIdx = 37;
  else if ( agcIdx > 37)
    agcIdx = 0;
  disableAgc = (agcIdx > 0); // if true, disable AGC; esle, AGC is enable
  if (agcIdx > 1)
    agcNdx = agcIdx - 1;
  else
    agcNdx = 0;
  if ( currentMode == AM ) 
     rx.setAutomaticGainControl(disableAgc, agcNdx); // if agcNdx = 0, no attenuation
  else
    rx.setSsbAgcOverrite(disableAgc, agcNdx, 0B1111111);

  band[bandIdx].disableAgc = disableAgc;
  band[bandIdx].agcIdx = agcIdx;
  band[bandIdx].agcNdx = agcNdx;

  showAgcAtt();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();
}


/**
 * Switches the current step
 */
void doStep(int8_t v)
{
    if ( currentMode == FM ) {
      idxFmStep = (v == 1) ? idxFmStep + 1 : idxFmStep - 1;
      if (idxFmStep > lastFmStep)
        idxFmStep = 0;
      else if (idxFmStep < 0)
        idxFmStep = lastFmStep;
        
      currentStepIdx = idxFmStep;
      rx.setFrequencyStep(tabFmStep[currentStepIdx]);
      
    } else {
      idxAmStep = (v == 1) ? idxAmStep + 1 : idxAmStep - 1;
      if (idxAmStep > lastAmStep)
        idxAmStep = 0;
      else if (idxAmStep < 0)
        idxAmStep = lastAmStep;

      currentStepIdx = idxAmStep;
      rx.setFrequencyStep(tabAmStep[currentStepIdx]);
      rx.setSeekAmSpacing(5); // Max 10kHz for spacing
    }
    band[bandIdx].currentStepIdx = currentStepIdx;
    showStep();
    delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
    elapsedCommand = millis();
}

/**
 * Switches to the AM, LSB or USB modes
 */
void doMode(int8_t v)
{
  if (currentMode != FM)
  {
    if (v == 1)  { // clockwise
      if (currentMode == AM)
      {
        // If you were in AM mode, it is necessary to load SSB patch (avery time)
        loadSSB();
        ssbLoaded = true;
        currentMode = LSB;
      }
      else if (currentMode == LSB)
        currentMode = USB;
      else if (currentMode == USB)
      {
        currentMode = AM;
        bfoOn = ssbLoaded = false;
      }
    } else { // and counterclockwise
      if (currentMode == AM)
      {
        // If you were in AM mode, it is necessary to load SSB patch (avery time)
        loadSSB();
        ssbLoaded = true;
        currentMode = USB;
      }
      else if (currentMode == USB)
        currentMode = LSB;
      else if (currentMode == LSB)
      {
        currentMode = AM;
        bfoOn = ssbLoaded = false;
      }
    }
    // Nothing to do if you are in FM mode
    band[bandIdx].currentFreq = currentFrequency;
    band[bandIdx].currentStepIdx = currentStepIdx;
    useBand();
  }
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();
}

/**
 * Sets the audio volume
 */
void doVolume( int8_t v ) {
  if ( v == 1)
    rx.volumeUp();
  else
    rx.volumeDown();

  showVolume();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
}

/**
 *  This function is called by the seek function process.
 */
void showFrequencySeek(uint16_t freq)
{
  currentFrequency = freq;
  showFrequency();
}

/**
 *  Find a station. The direction is based on the last encoder move clockwise or counterclockwise
 */
void doSeek()
{
  if ((currentMode == LSB || currentMode == USB)) return; // It does not work for SSB mode

  lcd.clear();
  rx.seekStationProgress(showFrequencySeek, seekDirection);
  showStatus();
  currentFrequency = rx.getFrequency();
  
}

/**
 * Sets the Soft Mute Parameter
 */
void doSoftMute(int8_t v)
{
  softMuteMaxAttIdx = (v == 1) ? softMuteMaxAttIdx + 1 : softMuteMaxAttIdx - 1;
  if (softMuteMaxAttIdx > 32)
    softMuteMaxAttIdx = 0;
  else if (softMuteMaxAttIdx < 0)
    softMuteMaxAttIdx = 32;

  rx.setAmSoftMuteMaxAttenuation(softMuteMaxAttIdx);
  showSoftMute();
  elapsedCommand = millis();
}


/**
 * Sets the Max gain for Automatic Volume Control. 
 */
void doAvc(int8_t v)
{
  avcIdx = (v == 1) ? avcIdx + 2 : avcIdx - 2;
  if (avcIdx > 90)
    avcIdx = 12;
  else if (avcIdx < 12)
    avcIdx = 90;

  rx.setAvcAmMaxGain(avcIdx);

  band[bandIdx].avcIdx = avcIdx;

  showAvc();
  elapsedCommand = millis();
}


/**
 * Turns RDS ON or OFF
 */
void doRdsSetup(int8_t v)
{
  fmRDS = (v == 1)? true:false;
  showRdsSetup();
  elapsedCommand = millis();
}


/**
 *  Menu options selection
 */
void doMenu( int8_t v) {
  menuIdx = (v == 1) ? menuIdx + 1 : menuIdx - 1;
  if (menuIdx > lastMenu)
    menuIdx = 0;
  else if (menuIdx < 0)
    menuIdx = lastMenu;

  showMenu();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();
}



/**
 * Return true if the current status is Menu command
 */
bool isMenuMode() {
  return (cmdMenu | cmdStep | cmdBandwidth | cmdAgc | cmdVolume | cmdSoftMuteMaxAtt | cmdMode | cmdRds | cmdAvc);
}


/**
 * Starts the MENU action process
 */
void doCurrentMenuCmd() {
  disableCommands();
  switch (currentMenuCmd) {
     case 0:                 // VOLUME
      cmdVolume = true;
      showVolume();
      break;
    case 1: 
      cmdRds = true;
      showRdsSetup();
      break;
    case 2:                 // STEP
      cmdStep = true;
      showStep();
      break;
    case 3:                 // MODE
      cmdMode = true;
      lcd.clear();
      showMode();
      break;
    case 4:
        bfoOn = true;
        if ((currentMode == LSB || currentMode == USB)) {
          showBFO();
        }
      // showFrequency();
      break;      
    case 5:                 // BW
      cmdBandwidth = true;
      showBandwidth();
      break;
    case 6:                 // AGC/ATT
      cmdAgc = true;
      showAgcAtt();
      break;
    case 7: 
      cmdSoftMuteMaxAtt = true;
      showSoftMute();  
      break;
    case 8: 
      cmdAvc =  true; 
      showAvc();
      break;  
    case 9:
      seekDirection = 1;
      doSeek();
      break;  
    case 10:
      seekDirection = 0;
      doSeek();
      break;    
    default:
      showStatus();
      break;
  }
  currentMenuCmd = -1;
  elapsedCommand = millis();
}


void setVfoFrequency(int direction) {
  
    currentFrequency += band[bandIdx].currentStepIdx * direction;
    if (currentFrequency > band[bandIdx].maximumFreq) 
      currentFrequency = band[bandIdx].minimumFreq; 
    else if (currentFrequency < band[bandIdx].minimumFreq) 
      currentFrequency = band[bandIdx].maximumFreq; 
      
    vfo.set_freq(currentFrequency + IF_OFFSET, SI5351_CLK0);     
}

/**
 * Main loop
 */
void loop()
{
  // Check if the encoder has moved.
  if (encoderCount != 0)
  {
    if (bfoOn & (currentMode == LSB || currentMode == USB))
    {
      currentBFO = (encoderCount == 1) ? (currentBFO + currentBFOStep) : (currentBFO - currentBFOStep);
      rx.setSSBBfo(currentBFO);
      showBFO();
    }
    else if (cmdMenu)
      doMenu(encoderCount);
    else if (cmdMode)
      doMode(encoderCount);
    else if (cmdStep)
      doStep(encoderCount);
    else if (cmdAgc)
      doAgc(encoderCount);
    else if (cmdBandwidth)
      doBandwidth(encoderCount);
    else if (cmdVolume)
      doVolume(encoderCount);
    else if (cmdSoftMuteMaxAtt)
      doSoftMute(encoderCount);
    else if (cmdAvc)
      doAvc(encoderCount);      
    else if (cmdBand)
      setBand(encoderCount);
    else if (cmdRds ) 
      doRdsSetup(encoderCount);  
    else
    {
      if (encoderCount == 1)
      {
        setVfoFrequency(1);
      }
      else
      {
        setVfoFrequency(-1);
      }
      showFrequency();
    }
    encoderCount = 0;
    resetEepromDelay();
  }
  else
  {
    if (digitalRead(ENCODER_PUSH_BUTTON) == LOW)
    {
      countClick++;
      if (cmdMenu)
      {
        currentMenuCmd = menuIdx;
        doCurrentMenuCmd();
      }
      else if (countClick == 1)
      { // If just one click, you can select the band by rotating the encoder
        if (isMenuMode())
        {
          disableCommands();
          showStatus();
          showCommandStatus((char *)"VFO ");
        }
        else if ( bfoOn ) {
          bfoOn = false;
          showStatus();
        }
        else
        {
          cmdBand = !cmdBand;
          showCommandStatus((char *)"Band");
        }
      }
      else
      { // GO to MENU if more than one click in less than 1/2 seconds.
        cmdMenu = !cmdMenu;
        if (cmdMenu)
          showMenu();
      }
      delay(MIN_ELAPSED_TIME);
      elapsedCommand = millis();
    }
  }

  // Show RSSI status only if this condition has changed
  if ((millis() - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 6)
  {
    rx.getCurrentReceivedSignalQuality();
    int aux = rx.getCurrentRSSI();
    if (rssi != aux && !isMenuMode())
    {
      rssi = aux;
      showRSSI();
    }
    elapsedRSSI = millis();
  }

  // Disable commands control
  if ((millis() - elapsedCommand) > ELAPSED_COMMAND)
  {
    if ((currentMode == LSB || currentMode == USB) )
    {
      bfoOn = false;
      // showBFO();
      showStatus();
    } else if (isMenuMode()) 
      showStatus();
    disableCommands();
    elapsedCommand = millis();
  }

  if ((millis() - elapsedClick) > ELAPSED_CLICK)
  {
    countClick = 0;
    elapsedClick = millis();
  }

  // Show the current frequency only if it has changed
  if (itIsTimeToSave)
  {
    if ((millis() - storeTime) > STORE_TIME)
    {
      saveAllReceiverInformation();
      storeTime = millis();
      itIsTimeToSave = false;
    }
  }

  if (currentMode == FM && fmRDS && !isMenuMode() )
  {
      if (currentFrequency != previousFrequency)
      {
        clearRDS();
        previousFrequency = currentFrequency;
       }
      else
      {
        checkRDS();
      }
  }  

  delay(2);
}
