// #define DEBUGGING                               // enable this line to include debugging print statements
// #define SERIALPRINT                             // include print statement for commissioning - comment this line to exclude
#include <math.h>
#include <Time.h>
#include <SPI.h>
//#include <Ethernet.h>
//#include <EthernetUdp.h>
//#include <Event.h>
#include <Timer.h>
#include <SoftwareSerial.h>
#include <string.h>
#include <stdlib.h>
#include <EEPROM.h>

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#define FILTERSETTLETIME 5000                    //  Time (ms) to allow the filters to settle before sending data

#define PHASE2 8                                 //  Number of samples delay for L2
#define PHASE3 18                                //  Number  of samples delay for L3, also size of array
//  These can be adjusted if the phase correction is not adequate
//  Suggested starting values for 4 ct's:
//    PHASE2  6
//    PHASE3 15
//    Phasecal2 = 0.57
//    Phasecal3 = 0.97

const int UNO = 1;                               // Set to 0 if you are not using the UNO bootloader 
// (i.e using Duemilanove) - All Atmega's shipped from
// OpenEnergyMonitor come with Arduino Uno bootloader

#include <avr/wdt.h>                             // the UNO bootloader 

//Set Voltage and current input pins
int inPinV = 0;
int inPinI1 = 4;
int inPinI2 = 3;
int inPinI3 = 2;
//Calibration coefficients
//These need to be set in order to obtain accurate results
double Vcal = 244.35;                           // Calibration constant for voltage input
double Ical1 = 77.7;                            // Calibration constant for current transformer 1
double Ical2 = 17.78;                           // Calibration constant for current transformer 2
double Ical3 = 3.84;                            // Calibration constant for current transformer 3

double Phasecal1 = 1.0;                         // Calibration constant for phase shift L1
double Phasecal2 = 1.35;                         // Calibration constant for phase shift L2
double Phasecal3 = 1.37;                         // Calibration constant for phase shift L3

uint8_t skipEarlies = 0;

//--------------------------------------------------------------------------------------
// Variable declaration for filters, phase shift, voltages, currents & powers
//--------------------------------------------------------------------------------------
double realPower1,                               // The final data
apparentPower1,
powerFactor1,
Irms1,
realPower2,
apparentPower2,
powerFactor2,
Irms2,
realPower3,
apparentPower3,
powerFactor3,
Irms3,
realPower4,
apparentPower4,
powerFactor4,
Irms4,
Vrms;       

typedef struct { 
  int power1, power2, power3, power4, Vrms; 
} 
PayloadTX;        // neat way of packaging data for RF comms
// (Include all the variables that are desired,
// ensure the same struct is used to receive)

PayloadTX emontx;                                // create an instance

const int LEDpin = 9;                            // On-board emonTx LED 

boolean settled = false;

//uint8_t state=0;                                 // state of SimCom sequence
//#define pwrPin 5

//--------------------------------------------------------------------------------------
// Variable declaration for timer and ethernet
//--------------------------------------------------------------------------------------
Timer t;
byte mac[] = { 
  0xDE, 0xAD, 0xEB, 0xEF, 0xEF, 0xDE };
const int updateThingSpeakInterval = 30000;      // Time interval in milliseconds to update ThingSpeak (number of seconds * 1000 = interval)
long lastConnectionTime = 0; 
boolean lastConnected = false;
// Initialize Arduino Ethernet Client
uint8_t failedCounter = 0;
uint8_t resetCounter = 0;
//uint8_t gprsAvailable = 0;
//EthernetClient client;
//SoftwareSerial SerialSim(7, 8);
char buffer[64]; // buffer array for data recieve over serial port
uint8_t count=0; // counter for buffer array
char smeasure[10];

#include <avr/pgmspace.h>
const char tspost[] PROGMEM = "POST /update HTTP/1.1\n";                                  // 0
const char tshost[] PROGMEM = "Host: api.thingspeak.com\n";
const char tsconn[] PROGMEM = "Connection: close\n";                                      // 2
const char tscont[] PROGMEM = "Content-Type: application/x-www-form-urlencoded\n";        // 3
const char tscontlen[] PROGMEM = "Content-Length: ";
const char key[] PROGMEM = "GET /update?api_key=";                                        // 5
const char field1[] PROGMEM = "&field1=";                                                 // 6
const char field2[] PROGMEM = "&field2=";
const char field3[] PROGMEM = "&field3=";
const char field4[] PROGMEM = "&field4=";                                                 // 9
const char apikey[] PROGMEM = "JDA5WN57DTFGUQQB"; // Building Dev                         // 10 // GI L14LZE95PARSWJUG
const char tsaddress[] PROGMEM = "api.thingspeak.com";
const char field5[] PROGMEM = "&field5=";                                                 // 12
const char field6[] PROGMEM = "&field6=";
const char field7[] PROGMEM = "&field7=";                                                 // 14
const char field8[] PROGMEM = "&field8=";                                                 // 15
const char createdat[] PROGMEM = "&created_at=";                                          // 16
const char wifiSSID[] PROGMEM = "GREENERATION  ID";                                       // 17
const char wifiPassword[] PROGMEM = "GOGIGAGE";                                           // 18

//const char cgatt[] PROGMEM = "AT+CGATT=1";                                              // 19
//const char cstt[] PROGMEM = "AT+CSTT=\"telkomsel\",\"wap\",\"wap123\"";                      
//const char ciicr[] PROGMEM = "AT+CIICR";                                                     
//const char cifsr[] PROGMEM = "AT+CIFSR";                                                     
//const char cdnscfg[] PROGMEM = "AT+CDNSCFG=\"8.8.8.8\",\"8.8.4.4\"";                    // 23
//const char sapbrcon[] PROGMEM = "AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"";
//const char sapbrapn[] PROGMEM = "AT+SAPBR=3,1,\"APN\",\"3gprs\"";   
//const char sapbr[] PROGMEM = "AT+SAPBR=1,1";
//const char httpinit[] PROGMEM = "AT+HTTPINIT";
//const char httppara[] PROGMEM = "AT+HTTPPARA=\"URL\",\"api.thingspeak.com/update\"";      // 28
//const char httpdata[] PROGMEM = "AT+HTTPDATA=";                                           // 29
//const char httpaction[] PROGMEM = "AT+HTTPACTION=1";
//const char httpapi[] PROGMEM = "AT+HTTPPARA=\"X-THINGSPEAKAPIKEY\",";
//const char httpcid[] PROGMEM = "AT+HTTPPARA=\"CID\",1";
//const char httpterm[] PROGMEM = "AT+HTTPTERM";                                            // 33

SoftwareSerial wifiPort(2,6);

const char * const string_table[19] PROGMEM = 
{
  tspost, tshost, tsconn, tscont, tscontlen, key, field1, field2, field3, field4,
  apikey, tsaddress, field5, field6, field7, field8, createdat, wifiSSID, wifiPassword
  //  , cgatt, cstt, ciicr, cifsr, cdnscfg, sapbrcon, sapbrapn, sapbr, httpinit, httppara, httpdata, httpaction, httpapi, httpcid, httpterm
};

char progbuffer[65];    // make sure this is large enough for the largest string it must hold

char* deblank(char* input)                                         
{
  uint8_t i,j;
  char *output=input;
  for (i = 0, j = 0; i<strlen(input); i++,j++)          
  {
    if (input[i]!=' ')                           
      output[j]=input[i];                     
    else
      j--;                                     
  }
  output[j]=0;
  return output;
} 

//void setupSimCom() {
//  pinMode(pwrPin, OUTPUT);
//  digitalWrite(pwrPin, LOW);
//  delay(1000);
//  digitalWrite(pwrPin, HIGH);
//  delay(1500);
//  digitalWrite(pwrPin, LOW);
//  delay(2500);
//}

//boolean waitOk() {
//  bool isOk = false;
//  bool isAll = false;
//  uint8_t i = 0;
//  while ( (!isOk) && (!isAll) ) {
//    if ( buffer[i] == 'O' ) {
//      if (( buffer[i+1] == 'K' ) && ( buffer[i+2] == '\r' )) { // OK response
//        isOk = true;
//      }
//    }
//    if (i == 63) {
//      isAll = true;
//    }
//    i++;
//  } 
//  if (isOk) return true;
//  if (isAll) return false;
//}
//
//boolean waitDownload() {
//  bool isDownload = false;
//  bool isAll = false;
//  uint8_t i = 0;
//  while ( (!isDownload) && (!isAll) ) {
//    if ( buffer[i] == 'D' ) {
//      if (( buffer[i+1] == 'O' ) && ( buffer[i+2] == 'W' ) && ( buffer[i+3] == 'N' ) && ( buffer[i+4] == 'L' )) { // DOWNLOAD response
//        isDownload = true;
//      }
//    }
//    if (i == 63) {
//      isAll = true;
//    }
//    i++;
//  }
//  if (isDownload) return true;
//  if (isAll) return false;
//}
//
//boolean waitIp() {
//  bool isAll = false;
//  uint8_t numDot = 0;
//  uint8_t i = 0;
//  while ( (numDot<3) && (!isAll) ) {
//    if ( buffer[i] == '.' ) {
//      numDot++;      
//    }
//    if (i == 63) {
//      isAll = true;
//    }
//    i++;
//  }
//  if (numDot == 3) return true;
//  if (isAll) return false;
//}

//void initialiseEthernet() {
//  client.stop();
//  if (Ethernet.begin(mac) == 0) {
//    Serial.println("Failed using DHCP");
//  }
//  else { 
//    Serial.println("DHCP Ok"); 
//  }
//  delay(1000);
//}

void initialiseWifi() {
  wifiPort.println(F("AT+RST"));
  delay(1000);
  while(!wifiPort.available());
  if(wifiPort.find("OK")){
    Serial.println(F("rdy"));
  }
  else{
    Serial.println(F("no rspn"));
    //indicator
  }
  delay(1000);
}

void setup() 
{
  Serial.begin(9600);
  Serial.println("EMMA");
  wifiPort.begin(9600);
  wifiPort.setTimeout(5000);
//  initialiseEthernet();
  initialiseWifi();

  t.every(updateThingSpeakInterval, httpGetCommand);
}

void httpGetCommand() { 
  
  uint8_t incomingByte = 0;
//  readTime();
  // Outer loop - Reads Voltages & Currents - Sends results
  calcVI3Ph(11,2000);                              // Calculate all. No.of complete cycles, time-out  
  // Removing these print statements is recommended for normal use (if not required).
#ifdef SERIALPRINT

  Serial.print(" Voltage: "); 
  Serial.println(Vrms);
  Serial.print(" Current 1: "); 
  Serial.print(Irms1);
  Serial.print(" Power 1: "); 
  Serial.print(realPower1);
  Serial.print(" VA 1: "); 
  Serial.print(apparentPower1);
  Serial.print(" PF 1: "); 
  Serial.println(powerFactor1);
  Serial.print(" Current 2: "); 
  Serial.print(Irms2);
  Serial.print(" Power 2: "); 
  Serial.print(realPower2);
  Serial.print(" VA 2: "); 
  Serial.print(apparentPower2);
  Serial.print(" PF 2: "); 
  Serial.println(powerFactor2);
  Serial.print(" Current 3: "); 
  Serial.print(Irms3);
  Serial.print(" Power 3: "); 
  Serial.print(realPower3);
  Serial.print(" VA 3: "); 
  Serial.print(apparentPower3);
  Serial.print(" PF 3: "); 
  Serial.println(powerFactor3);
#ifdef CT4
  Serial.print(" Current 4: "); 
  Serial.print(Irms4);
  Serial.print(" Power 4: "); 
  Serial.print(realPower4);
  Serial.print(" VA 4: "); 
  Serial.print(apparentPower4);
  Serial.print(" PF 4: "); 
  Serial.println(powerFactor4);
#endif

  Serial.println(); 
  delay(100);

#endif 

  emontx.power1 = realPower1;                      // Copy the desired variables ready for transmision
  emontx.power2 = realPower2;
  emontx.power3 = realPower3;
  emontx.power4 = realPower4;
  emontx.Vrms   = Vrms;

  if (!settled && millis() > FILTERSETTLETIME)     // because millis() returns to zero after 50 days ! 
    settled = true;

  if (settled)                                     // send data only after filters have settled
  {  
    //    send_rf_data();                              // *SEND RF DATA* - see emontx_lib
    digitalWrite(LEDpin, HIGH); 
    delay(2); 
    digitalWrite(LEDpin, LOW);      // flash LED
    //    emontx_sleep(3);    
  }

  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[5])));
  String tsData = progbuffer;
  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[10]))); // writeApiKey
  tsData += progbuffer;

  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[6])));
  tsData += progbuffer;
  dtostrf(Vrms,6,2,smeasure);
  tsData += deblank(smeasure);

  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[7])));
  tsData += progbuffer;
  dtostrf(Irms1,6,2,smeasure);
  tsData += deblank(smeasure);

  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[8])));
  tsData += progbuffer;
  Irms2 = 0;
  dtostrf(Irms2,6,2,smeasure);
  tsData += deblank(smeasure);

  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[9])));
  tsData += progbuffer;
  Irms3 = 0;
  dtostrf(Irms3,6,2,smeasure);
  tsData += deblank(smeasure);

  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[12])));
  tsData += progbuffer;
  dtostrf(apparentPower1,6,2,smeasure);
  tsData += deblank(smeasure);

  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[13])));
  tsData += progbuffer;
  apparentPower2 = 0;
  dtostrf(apparentPower2,6,2,smeasure);
  tsData += deblank(smeasure);

  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[14])));
  tsData += progbuffer;
  apparentPower3 = 0;
  dtostrf(apparentPower3,6,2,smeasure);
  tsData += deblank(smeasure);
  tsData +="\r\n\r\n";
  
  Serial.print(tsData);
  // Serial.println("Sending...");

  // Disconnect from ThingSpeak
//  if (!client.connected() && lastConnected) { client.stop();}

  // Update ThingSpeak
  if (skipEarlies > 0) { // we have skipped initial 10 samples
    if (isnan(Irms1) || isnan(Irms2) || isnan(Irms3)) {
      Serial.println("NaN detected");
    }
    else {
      //connect to wifi
      boolean isConnect = false;
      for(int i=0;i<3;i++){
        Serial.println("connecting...");
        if(connectWifi()){
          isConnect=true;
          break;
        }
      }
      if(isConnect){
        Serial.println(F("con"));
      }
      else{
        Serial.println(F("not con"));
        initialiseWifi();
      }
      delay(200);
      wifiPort.println(F("AT+CIPMUX=0"));
//        Serial.println("sending");
      sendData(tsData);
    }
  }
  else {
    Serial.println("skipped");
    skipEarlies++;
  }

  // FINISH SENDING DATA  
}
//*********************************************************************************************************************
void loop()
{
  t.update();
}

void calcVI3Ph(int cycles, int timeout)
{
  //--------------------------------------------------------------------------------------
  // Variable declaration for filters, phase shift, voltages, currents & powers
  //--------------------------------------------------------------------------------------

  static int lastSampleV,sampleV;                         // 'sample' holds the raw analog read value, 'lastSample' holds the last sample
  static int lastSampleI1,sampleI1;
  static int lastSampleI2,sampleI2;
  static int lastSampleI3,sampleI3;
  static int lastSampleI4,sampleI4;

  static double lastFilteredV,filteredV;      // 'Filtered' is the raw analog value minus the DC offset
  static double lastFilteredI1, filteredI1;
  static double lastFilteredI2, filteredI2;
  static double lastFilteredI3, filteredI3;
  static double lastFilteredI4, filteredI4;

  double phaseShiftedV1;                      // Holds the calibrated delayed & phase shifted voltage.
  double phaseShiftedV2;
  double phaseShiftedV3;
  double phaseShiftedV4;

  double sumV,sumI1,sumI2,sumI3,sumI4;
  double sumP1,sumP2,sumP3,sumP4;             // sq = squared, sum = Sum, inst = instantaneous

  int startV;                                 // Instantaneous voltage at start of sample window.

  int SupplyVoltage = readVcc();
  int crossCount = -2;                         // Used to measure number of times threshold is crossed.
  int numberOfSamples = 0;                     // This is now incremented  
  int numberOfPowerSamples = 0;                // Needed because 1 cycle of voltages needs to be stored before use
  boolean lastVCross, checkVCross;             // Used to measure number of times threshold is crossed.
  double storedV[PHASE3];                      // Array to store >240 degrees of voltage samples

  //-------------------------------------------------------------------------------------------------------------------------
  // 1) Waits for the waveform to be close to 'zero' (500 adc) part in sin curve.
  //-------------------------------------------------------------------------------------------------------------------------
  boolean st=false;                           // an indicator to exit the while loop

  unsigned long start = millis();             // millis()-start makes sure it doesnt get stuck in the loop if there is an error.

  while(st==false)                            // Wait for first zero crossing...
  {
    startV = analogRead(inPinV);            // using the voltage waveform
    if ((startV < 550) && (startV > 440)) st=true;        // check it's within range
    if ((millis()-start)>timeout) st = true;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 2) Main measurment loop
  //------------------------------------------------------------------------------------------------------------------------- 
  start = millis(); 

  while ((crossCount < cycles * 2) && ((millis()-start)<timeout)) 
  {
    lastSampleV=sampleV;                    // Used for digital high pass filter - offset removal
    lastSampleI1=sampleI1;
    lastSampleI2=sampleI2;
    lastSampleI3=sampleI3;
    lastSampleI4=sampleI4;

    lastFilteredV = filteredV;
    lastFilteredI1 = filteredI1;  
    lastFilteredI2 = filteredI2; 
    lastFilteredI3 = filteredI3;
    lastFilteredI4 = filteredI4;

    //-----------------------------------------------------------------------------
    // A) Read in raw voltage and current samples
    //-----------------------------------------------------------------------------
    sampleV = analogRead(inPinV);           // Read in raw voltage signal
    sampleI1 = analogRead(inPinI1);         // Read in raw current signal
    //Serial.print(" Analog I1: "); Serial.println(sampleI1);
    sampleI2 = analogRead(inPinI2);         // Read in raw current signal
    //Serial.print(" Analog I2: "); Serial.println(sampleI2);
    sampleI3 = analogRead(inPinI3);         // Read in raw current signal
    //Serial.print(" Analog I3: "); Serial.println(sampleI3);
#ifdef CT4
    sampleI4 = analogRead(inPinI4);          // Read in raw current signal
#endif
    //-----------------------------------------------------------------------------
    // B) Apply digital high pass filters to remove 2.5V DC offset (to centre wave on 0).
    //-----------------------------------------------------------------------------
    filteredV = 0.996*(lastFilteredV+(sampleV-lastSampleV));
    filteredI1 = 0.996*(lastFilteredI1+(sampleI1-lastSampleI1));
    filteredI2 = 0.996*(lastFilteredI2+(sampleI2-lastSampleI2));
    filteredI3 = 0.996*(lastFilteredI3+(sampleI3-lastSampleI3));
#ifdef CT4
    filteredI4 = 0.996*(lastFilteredI4+(sampleI4-lastSampleI4));
#endif

    storedV[numberOfSamples%PHASE3] = filteredV;        // store this voltage sample in circular buffer

    //-----------------------------------------------------------------------------

    // C)  Find the number of times the voltage has crossed the initial voltage
    //        - every 2 crosses we will have sampled 1 wavelength 
    //        - so this method allows us to sample an integer number of half wavelengths which increases accuracy
    //-----------------------------------------------------------------------------       


    lastVCross = checkVCross;                     

    checkVCross = (sampleV > startV)? true : false;
    if (numberOfSamples==1)
      lastVCross = checkVCross;                  

    if (lastVCross != checkVCross)
    {
      crossCount++;
      if (crossCount == 0)              // Started recording at -2 crossings so that one complete cycle has been stored before accumulating.
      {
        sumV  = 0;
        sumI1 = 0;
        sumI2 = 0;
        sumI3 = 0;
        sumI4 = 0;
        sumP1 = 0;                                    
        sumP2 = 0;
        sumP3 = 0;
        sumP4 = 0;
        numberOfPowerSamples = 0;
      }
    }

    //-----------------------------------------------------------------------------
    // D) Root-mean-square method voltage
    //-----------------------------------------------------------------------------  
    sumV += filteredV * filteredV;          // sum += square voltage values

    //-----------------------------------------------------------------------------
    // E) Root-mean-square method current
    //-----------------------------------------------------------------------------   
    sumI1 += filteredI1 * filteredI1;       // sum += square current values
    sumI2 += filteredI2 * filteredI2;
    sumI3 += filteredI3 * filteredI3;
#ifdef CT4
    sumI4 += filteredI4 * filteredI4;
#endif

    //-----------------------------------------------------------------------------
    // F) Phase calibration - for Phase 1: shifts V1 to correct transformer errors
    //    for phases 2 & 3 delays V1 by 120 degrees & 240 degrees respectively 
    //    and shifts for fine adjustment and to correct transformer errors.
    //-----------------------------------------------------------------------------
    phaseShiftedV1 = lastFilteredV + Phasecal1 * (filteredV - lastFilteredV);
    phaseShiftedV2 = storedV[(numberOfSamples-PHASE2-1)%PHASE3] 
      + Phasecal2 * (storedV[(numberOfSamples-PHASE2)%PHASE3] 
      - storedV[(numberOfSamples-PHASE2-1)%PHASE3]);
    phaseShiftedV3 = storedV[(numberOfSamples+1)%PHASE3] 
      + Phasecal3 * (storedV[(numberOfSamples+2)%PHASE3]
      - storedV[(numberOfSamples+1)%PHASE3]);


    //-----------------------------------------------------------------------------
    // G) Instantaneous power calc
    //-----------------------------------------------------------------------------   
    sumP1 += phaseShiftedV1 * filteredI1;   // Sum  += Instantaneous Power
    //        Serial.print(" sum I2: "); Serial.println(sumP2);
    //        Serial.print(" phase shift I2: "); Serial.println(phaseShiftedV2);
    //        Serial.print(" filtered I2: "); Serial.println(filteredI2);
    sumP2 += phaseShiftedV2 * filteredI2;
    sumP3 += phaseShiftedV3 * filteredI3;
#ifdef CT4
    sumP4 += phaseShiftedV1 * filteredI4;
#endif

    numberOfPowerSamples++;                  // Count number of times looped for Power averages.
    numberOfSamples++;                      //Count number of times looped.    

  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 3) Post loop calculations
  //------------------------------------------------------------------------------------------------------------------------- 
  //Calculation of the root of the mean of the voltage and current squared (rms)
  //Calibration coefficients applied. 

  double V_Ratio = Vcal *((SupplyVoltage/1000.0) / 1023.0);
  Vrms = V_Ratio * sqrt(sumV / numberOfPowerSamples); 

  double I_Ratio1 = Ical1 *((SupplyVoltage/1000.0) / 1023.0);
  Irms1 = I_Ratio1 * sqrt(sumI1 / numberOfPowerSamples); 
  double I_Ratio2 = Ical2 *((SupplyVoltage/1000.0) / 1023.0);
  Irms2 = I_Ratio2 * sqrt(sumI2 / numberOfPowerSamples); 
  double I_Ratio3 = Ical3 *((SupplyVoltage/1000.0) / 1023.0);
  Irms3 = I_Ratio3 * sqrt(sumI3 / numberOfPowerSamples); 
#ifdef CT4
  double I_Ratio4 = Ical4 *((SupplyVoltage/1000.0) / 1023.0);
  Irms4 = I_Ratio4 * sqrt(sumI4 / numberOfPowerSamples); 
#endif

  //Calculation power values
  realPower1 = V_Ratio * I_Ratio1 * sumP1 / numberOfPowerSamples;
  apparentPower1 = Vrms * Irms1;
  powerFactor1 = realPower1 / apparentPower1;

  realPower2 = V_Ratio * I_Ratio2 * sumP2 / numberOfPowerSamples;
  apparentPower2 = Vrms * Irms2;
  powerFactor2 = realPower2 / apparentPower2;

  realPower3 = V_Ratio * I_Ratio3 * sumP3 / numberOfPowerSamples;
  apparentPower3 = Vrms * Irms3;
  powerFactor3 = realPower3 / apparentPower3;

#ifdef CT4
  realPower4 = V_Ratio * I_Ratio4 * sumP4 / numberOfPowerSamples;
  apparentPower4 = Vrms * Irms4;
  powerFactor4 = realPower4 / apparentPower4;
#else
  realPower4 = 0.0;
  apparentPower4 = 0.0;
  powerFactor4 = 0.0;
#endif

  //Reset accumulators
  sumV = 0;
  sumI1 = 0;
  sumI2 = 0;
  sumI3 = 0;
  sumI4 = 0;
  sumP1 = 0;
  sumP2 = 0;
  sumP3 = 0;
  sumP4 = 0;
  //--------------------------------------------------------------------------------------       

#ifdef DEBUGGING
  // Include these statements for development/debugging only

  Serial.print("Total Samples: "); 
  Serial.print(numberOfSamples);
  Serial.print(" Power Samples: "); 
  Serial.print(numberOfPowerSamples);
  Serial.print(" Time: "); 
  Serial.print(millis() - start);
  Serial.print(" Crossings: "); 
  Serial.println(crossCount);

  for (int j=0; j<PHASE3; j++)
  {
    Serial.print(storedV[j]); 
    Serial.print(" ");
    Serial.println();
  }
#endif
}

//*********************************************************************************************************************

long readVcc() 
{
  long result;
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result;
  return result;
}

void clearBufferArray()              // function to clear buffer array
{
  for (uint8_t i=0; i<count;i++)
  { 
    buffer[i]=NULL;
  }                  // clear all index of array with command NULL
}

void sendData(String tsData){
  wifiPort.println(F("AT+CIPSTART=\"TCP\",\"184.106.153.149\",80"));
  delay(2000);
  if(wifiPort.find("ERROR")) {
    return;
  }
  //delay(1000);

  wifiPort.print(F("AT+CIPSEND="));
  wifiPort.println(tsData.length()+2);
  if(wifiPort.find(">")){
    wifiPort.println(tsData);
  }
  else{
    wifiPort.println(F("AT+CIPCLOSE"));
    delay(1000);
    return;
  }
  
  if(wifiPort.find("OK")){
//    Serial.println(F("dt sent"));
    digitalWrite(13,HIGH);
    delay(250);
    digitalWrite(13,LOW);
    wifiPort.println(F("AT+CIPCLOSE"));
    delay(250);
  }
  else{
//    Serial.println(F("send err"));
    wifiPort.println(F("AT+CIPCLOSE"));
    delay(1000);
  }
}

boolean connectWifi(){
  wifiPort.println(F("AT+CWMODE=1"));
  delay(200);
  wifiPort.println(F("AT+CWJAP=\"Tritronik Mobile\",\"Tri12@11\"")); // setting SSID
//  wifiPort.println(F("AT+CWJAP=\"GREENERATION  ID\",\"GOGIGAGE\"")); // setting SSID
//    wifiPort.println(F("AT+CWJAP=\"Belkin_G_Plus_MIMO\",\"\"")); // setting SSID
//  wifiPort.print(F("AT+CWJAP=\""));
//  
//  //start read wifi ssid
//  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[17]))); // wifiSSID
//  wifiPort.print(progbuffer); //wifi ssid
//  Serial.println(progbuffer);
//  wifiPort.print(F("\",\""));
//  //end read wifi ssid
//
//  //start read wifi password
//  strcpy_P(progbuffer, (char*)pgm_read_word(&(string_table[18]))); // wifiPassword
//  wifiPort.print(progbuffer); //wifi password
//  Serial.println(progbuffer);
//  wifiPort.print(F("\""));
//  //end read wifi password
  
  if(wifiPort.find("OK")){
    return true;
  }
  else{
    return false;
  }
}


