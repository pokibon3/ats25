/*

  Test and validation of the SI4735 Arduino Library with SSB support.
  It is important to know the SSB support works on SI4735-D60 and SI4732-A10 devices. 

  ATTENTION:  Please, avoid using the computer connected to the mains during testing.

  The table below shows the Si4735-D60 and STM32F103C8 pin connections 
    
  | Si4735-D60 pin  |  Arduino Pin  |
  | ----------------| ------------  |
  | RESET (pin 15)  |     PA12      |
  | SDIO (pin 18)   |     PB7 (B7)  |
  | SCLK (pin 17)   |     PB6 (B6)  |


  ABOUT SSB PATCH:  
  This sketch will download a SSB patch to your SI4735 device (patch_init.h). It will take about 8KB of the Arduino memory.

  First of all, it is important to say that the SSB patch content is not part of this library. The paches used here were made available by Mr. 
  Vadim Afonkin on his Dropbox repository. It is important to note that the author of this library does not encourage anyone to use the SSB patches 
  content for commercial purposes. In other words, this library only supports SSB patches, the patches themselves are not part of this library.

  Read more about SSB patch documentation on https://pu2clr.github.io/SI4735/

  Features of this sketch:

  1) Only SSB (LSB and USB);
  2) Audio bandwidth filter 0.5, 1, 1.2, 2.2, 3 and 4kHz;
  4) BFO Control; and
  5) Frequency step switch (1, 5 and 10kHz);

  By Ricardo Lima Caratti, Feb 2019.
  
*/

#include <SI4735.h>
#include "Rotary.h"
#include <patch_init.h> // SSB patch for whole SSBRX initialization string

const uint16_t size_content = sizeof ssb_patch_content; // see ssb_patch_content in patch_full.h or patch_init.h

#define AM_FUNCTION 1
#define RESET_PIN PA12

#define LSB 1
#define USB 2

bool disableAgc = true;
bool avc_en = true;

int currentBFO = 0;

// Some variables to check the SI4735 status
uint16_t currentFrequency;
uint8_t currentStep = 1;
uint8_t currentBFOStep = 25;

uint8_t bandwidthIdx = 2;
const char *bandwidth[] = {"1.2", "2.2", "3.0", "4.0", "0.5", "1.0"};


long et1 = 0, et2 = 0;

typedef struct
{
  uint16_t minimumFreq;
  uint16_t maximumFreq;
  uint16_t currentFreq;
  uint16_t currentStep;
  uint8_t currentSSB;
} Band;

Band band[] = {
    {1800, 2000, 1900, 1, LSB},
    {3500, 4000, 3700, 1, LSB},
    {7000, 7500, 7100, 1, LSB},
    {10000, 10500, 10050, 1, USB},
    {14000, 14300, 14200, 1, USB},
    {18000, 18300, 18100, 1, USB},
    {21000, 21400, 21200, 1, USB},
    {24890, 25000, 24940, 1, USB},
    {27000, 27700, 27300, 1, USB},
    {28000, 28500, 28400, 1, USB}};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int currentFreqIdx = 9;

uint8_t currentAGCAtt = 0;

uint8_t rssi = 0;


SI4735 si4735;


void showSeparator()
{
  Serial.println("\n**************************");
}

void showHelp()
{
  showSeparator();
  Serial.println("Type: ");
  Serial.println("U to frequency up or D to frequency down");
  Serial.println("> to go to the next band or < to go to the previous band");
  Serial.println("W to sitch the filter bandwidth");
  Serial.println("B to go to increment the BFO or b decrement the BFO");
  Serial.println("G to switch on/off the Automatic Gain Control");
  Serial.println("A to switch the LNA Gain Index (0, 1, 5, 15 e 26");
  Serial.println("S to switch the frequency increment and decrement step");
  Serial.println("s to switch the BFO increment and decrement step");
  Serial.println("X Shows the current status");
  Serial.println("H to show this help");
}

// Show current frequency
void showFrequency()
{
  String freqDisplay;
  freqDisplay = String((float)currentFrequency / 1000, 3);
  showSeparator();
  Serial.print("Current Frequency: ");
  Serial.print(freqDisplay);
  Serial.print("kHz");
  Serial.print(" | Step: ");
  Serial.println(currentStep);
}

void showStatus()
{
  showSeparator();
  Serial.print("SSB | ");

  si4735.getAutomaticGainControl();
  si4735.getCurrentReceivedSignalQuality();
    
  Serial.print((si4735.isAgcEnabled()) ? "AGC ON " : "AGC OFF");
  Serial.print(" | LNA GAIN index: ");
  Serial.print(si4735.getAgcGainIndex());
  Serial.print("/");
  Serial.print(currentAGCAtt);
  
  Serial.print(" | BW :");
  Serial.print(String(bandwidth[bandwidthIdx]));
  Serial.print("kHz");
  Serial.print(" | SNR: ");
  Serial.print(si4735.getCurrentSNR());
  Serial.print(" | RSSI: ");
  Serial.print(si4735.getCurrentRSSI());
  Serial.print(" dBuV");
  Serial.print(" | Volume: ");
  Serial.println(si4735.getVolume());
  showFrequency();
}

void showBFO()
{
  String bfo;
  
  if (currentBFO > 0)
    bfo = "+" + String(currentBFO);
  else
    bfo = String(currentBFO);

  showSeparator();

  Serial.print("BFO: ");
  Serial.print(bfo);
  Serial.print("Hz ");
  
  Serial.print(" | Step: ");
  Serial.print(currentBFOStep);
  Serial.print("Hz ");  

}

void bandUp()
{
  // save the current frequency for the band
  band[currentFreqIdx].currentFreq = currentFrequency;
  if (currentFreqIdx < lastBand)
  {
    currentFreqIdx++;
  }
  else
  {
    currentFreqIdx = 0;
  }
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentSSB);
  currentStep = band[currentFreqIdx].currentStep;
  delay(250);
  currentFrequency = si4735.getCurrentFrequency();
  showStatus();
}

void bandDown()
{
  // save the current frequency for the band
  band[currentFreqIdx].currentFreq = currentFrequency;
  if (currentFreqIdx > 0)
  {
    currentFreqIdx--;
  }
  else
  {
    currentFreqIdx = lastBand;
  }
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentSSB);
  currentStep = band[currentFreqIdx].currentStep;
  delay(250);
  currentFrequency = si4735.getCurrentFrequency();
  showStatus();
}

/*
 * This function loads the contents of the ssb_patch_content array into the CI (Si4735) and starts the radio on
 * SSB mode.
 */
void loadSSB()
{
  si4735.queryLibraryId(); // Is it really necessary here? I will check it.
  si4735.patchPowerUp();
  delay(50);
  si4735.downloadPatch(ssb_patch_content, size_content);
  // Parameters
  // AUDIOBW - SSB Audio bandwidth; 0 = 1.2kHz (default); 1=2.2kHz; 2=3kHz; 3=4kHz; 4=500Hz; 5=1kHz;
  // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
  // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
  // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
  // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
  // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
  si4735.setSSBConfig(bandwidthIdx, 1, 0, 1, 0, 1);
}

/*
   Main
*/

void setup()
{

  Serial.begin(9600);
  while(!Serial);

  
  Serial.println("Si4735 Arduino Library");
  Serial.println("SSB TEST");
  Serial.println("By PU2CLR");


  // Gets and sets the Si47XX I2C bus address
  int16_t si4735Addr = si4735.getDeviceI2CAddress(RESET_PIN);
  if ( si4735Addr == 0 ) {
    Serial.println("Si473X not found!");
    Serial.flush();
    while (1);
  } else {
    Serial.print("The Si473X I2C address is 0x");
    Serial.println(si4735Addr, HEX);
  }


  si4735.setup(RESET_PIN, AM_FUNCTION);

  // Testing I2C clock speed and SSB behaviour
  // si4735.setI2CLowSpeedMode();     //  10000 (10kHz)
  // si4735.setI2CStandardMode();        // 100000 (100kHz)
  si4735.setI2CFastMode();         // 400000 (400kHz)
  // si4735.setI2CFastModeCustom(500000); // It is not safe and can crash.
  delay(10);
  Serial.println("SSB patch is loading...");
  et1 = millis();
  loadSSB();
  et2 = millis();
  Serial.print("SSB patch was loaded in: ");
  Serial.print( (et2 - et1) );
  Serial.println("ms");
  delay(100);
  si4735.setTuneFrequencyAntennaCapacitor(1); // Set antenna tuning capacitor for SW.
  si4735.setSSB(band[currentFreqIdx].minimumFreq, band[currentFreqIdx].maximumFreq, band[currentFreqIdx].currentFreq, band[currentFreqIdx].currentStep, band[currentFreqIdx].currentSSB);
  delay(100);
  currentFrequency = si4735.getFrequency();
  si4735.setVolume(60);
  showHelp();
  showStatus();
}

void loop()
{
  // Check if exist some command to execute
  if (Serial.available() > 0)
  {
    char key = Serial.read();
    if (key == 'U' || key == 'u')
    { // frequency up
      si4735.frequencyUp();
      delay(250);
      band[currentFreqIdx].currentFreq = currentFrequency = si4735.getCurrentFrequency();
      showFrequency();
    }
    else if (key == 'D' || key == 'd')
    { // frequency down
      si4735.frequencyDown();
      delay(250);
      band[currentFreqIdx].currentFreq = currentFrequency = si4735.getCurrentFrequency();
      showFrequency();
    }
    else if (key == 'W' || key == 'w') // sitches the filter bandwidth
    {
      bandwidthIdx++;
      if (bandwidthIdx > 5)
        bandwidthIdx = 0;
      si4735.setSSBAudioBandwidth(bandwidthIdx);
      // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
      if (bandwidthIdx == 0 || bandwidthIdx == 4 || bandwidthIdx == 5)
        si4735.setSSBSidebandCutoffFilter(0);
      else
        si4735.setSSBSidebandCutoffFilter(1);
      showStatus();
    }
    else if (key == '>' || key == '.') // goes to the next band
      bandUp();
    else if (key == '<' || key == ',') // goes to the previous band
      bandDown();
    else if (key == 'V')
    { // volume down
      si4735.volumeUp();
      showStatus();
    }
    else if (key == 'v')
    { // volume down
      si4735.volumeDown();
      showStatus();
    }
    else if (key == 'B') // increments the bfo
    {
      currentBFO += currentBFOStep;
      si4735.setSSBBfo(currentBFO);
      showBFO();
    }
    else if (key == 'b') // decrements the bfo
    {
      currentBFO -= currentBFOStep;
      si4735.setSSBBfo(currentBFO);
      showBFO();
    }
    else if (key == 'G' || key == 'g') // switches on/off the Automatic Gain Control
    {
      disableAgc = !disableAgc;
      // siwtch on/off ACG; AGC Index = 0. It means Minimum attenuation (max gain)
      si4735.setAutomaticGainControl(disableAgc, currentAGCAtt);
      showStatus();
    }
    else if (key == 'A' || key == 'a') // Switches the LNA Gain index  attenuation 
    {
      if (currentAGCAtt == 0)
        currentAGCAtt = 1;
      else if (currentAGCAtt == 1)
        currentAGCAtt = 5;
      else if (currentAGCAtt == 5)
        currentAGCAtt = 15;
      else if (currentAGCAtt == 15)
        currentAGCAtt = 26;
      else
        currentAGCAtt = 0;
      si4735.setAutomaticGainControl(1 /*disableAgc*/, currentAGCAtt);
      showStatus();
    }
    else if (key == 's') // switches the BFO increment and decrement step
    {
      currentBFOStep = (currentBFOStep == 50) ? 10 : 50;
      showBFO();
    }
    else if (key == 'S') // switches the frequency increment and decrement step
    {
      if (currentStep == 1)
        currentStep = 5;
      else if (currentStep == 5)
        currentStep = 10;
      else
        currentStep = 1;
      si4735.setFrequencyStep(currentStep);
      band[currentFreqIdx].currentStep = currentStep;
      showFrequency();
    }
    else if (key == 'C' || key == 'c') // switches on/off the Automatic Volume Control
    {
      avc_en = !avc_en;
      si4735.setSSBAutomaticVolumeControl(avc_en);
    }
    else if (key == 'X' || key == 'x') 
      showStatus();
    else if (key == 'H' || key == 'h')
      showHelp();
  }
  delay(200);
}
