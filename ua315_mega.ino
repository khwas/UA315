// UTFT_Demo_480x272 (C)2013 Henning Karlsen
// web: http://www.henningkarlsen.com/electronics
//
// This program is a demo of how to use most of the functions
// of the library with a supported display modules.
//
// This demo was made for modules with a screen resolution
// of 480x272 pixels.
//
// This program requires the UTFT library.
//

#include <UTFT.h>
#include <Wire.h> // Use Wire according to arcticle https://garretlabs.wordpress.com/2014/03/18/the-dark-side-of-porting-arduino-sketches-on-intel-galileo-part-one/
#include <Math.h>
#include "BigNumber.h" // https://github.com/nickgammon/BigNumber by Nick Gammon

// Declare which fonts we will be using
extern uint8_t SmallFont[];
extern uint8_t BigFont[];

UTFT myGLCD(SSD1963_480, 38, 39, 40, 41); // Remember to change the model parameter to suit your display module!

double TimeBaseFrequencyHz = (double)16384000.0;

void setup()
{
  // Setup the LCD
  myGLCD.InitLCD();
  // reset pin is high by default
  pinMode(9, OUTPUT);
  analogWrite(9, 255);

  // Frequency mux pins
  pinMode(11, OUTPUT); // PB5 Digital 11 Mux A
  pinMode(12, OUTPUT); // PB6 Digital 12 Mux B
  pinMode(13, OUTPUT); // PB7 Digital 13 Mux C
  selectDiv1Frequency();

  // relays
  pinMode(44, OUTPUT); // PL5 relay reset HIGH=Energized
  pinMode(45, OUTPUT); // PL4 range 4 HIGH=Energized
  pinMode(46, OUTPUT); // PL3 range 3 HIGH=Energized
  pinMode(47, OUTPUT); // PL2 range 2 HIGH=Energized
  pinMode(48, OUTPUT); // PL1 range 1 HIGH=Energized
  digitalWrite(44, LOW);
  digitalWrite(45, LOW);
  digitalWrite(46, LOW);
  digitalWrite(47, LOW);
  digitalWrite(48, LOW);

  // channell switch
  pinMode(49, OUTPUT); // PL0 channel switch. LOW=V, HIGH=I
  digitalWrite(49, HIGH); // switch. LOW=V, HIGH=I

  // unused serial pins
  pinMode(50, OUTPUT);  // PB3 MISO
  pinMode(51, OUTPUT);  // PB2 MOSI
  pinMode(52, OUTPUT);  // PB1 SCK
  pinMode(53, OUTPUT);  // PB0 not connected

  // Gain pins
  pinMode(16,  OUTPUT); // PH1 LO G0 Digital 16
  pinMode(17,  OUTPUT); // PH0 LO G1 Digital 17
  pinMode(18,  OUTPUT); // PD3 LO G2 Digital 18
  pinMode(19,  OUTPUT); // PD2 LO G3 Digital 19
  pinMode(A15, OUTPUT); // PK7 LO G4 Analog 15

  pinMode(A14, OUTPUT); // PK6 HI G0 Analog 14
  pinMode(A13, OUTPUT); // PK5 HI G1 Analog 13
  pinMode(A12, OUTPUT); // PK4 HI G2 Analog 12
  pinMode(A11, OUTPUT); // PK3 HI G3 Analog 11
  pinMode(A10, OUTPUT); // PK2 HI G4 Analog 10

  // set LO G3..0=0011, G4=0 to set gain 1:1   // High Gain -> greater phase lag ?
  // set LO G3..0=0100, G4=0 to set gain 1:2
  // set LO G3..0=0101, G4=0 to set gain 1:4
  // set LO G3..0=0110, G4=0 to set gain 1:8
  // set LO G3..0=0111, G4=0 to set gain 1:16
  // set LO G3..0=1000, G4=0 to set gain 1:32
  // set LO G3..0=1001, G4=0 to set gain 1:64
  // set LO G3..0=1010, G4=0 to set gain 1:128
  digitalWrite(16, HIGH);
  digitalWrite(17, LOW);
  digitalWrite(18, LOW);
  digitalWrite(19, HIGH);
  analogWrite(A15, 0);

  // set HI G3..0=0000, G4=0 to set gain 1:0.125
  // set HI G3..0=0001, G4=0 to set gain 1:0.25
  // set HI G3..0=0011, G4=0 to set gain 1:1
  // set HI G3..0=0100, G4=0 to set gain 1:2
  // set HI G3..0=0101, G4=0 to set gain 1:4
  // set HI G3..0=0110, G4=0 to set gain 1:8
  // set HI G3..0=0111, G4=0 to set gain 1:16
  // set HI G3..0=1001, G4=0 to set gain 1:64
  // set HI G3..0=1010, G4=0 to set gain 1:128
  analogWrite(A14, 255);
  analogWrite(A13, 0);
  analogWrite(A12, 255);
  analogWrite(A11, 0);
  analogWrite(A10, 0);

  Wire.begin();
  //select200KOhmRange();
  //select10KOhmRange();
  //select50ohmRange();
  selectOhm24Range();
  myGLCD.clrScr();
  displayGrid();
}

double totalReI;
double totalImI;
double totalReV;
double totalImV;

double ReferenceOhm = 0;

// Calibration variables
unsigned long totalWait;
int settlingCount = 0;

void loop()
{
  BigNumber::begin(40);
  BigNumber::setScale(10);
  commandAD5933Reset();            // reset command
  displayTemperature();
  commandAD5933ExternalClock();    // external clock
  commandAD5933GainX1();
  commandAD5933Voltage3V();
  //commandAD5933Voltage1500mV();
  //commandAD5933Voltage600mV();
  //commandAD5933Voltage300mV();

  writeAD5933FrequencyHz((double)10000.0);
  //writeAD5933FrequencyHz((double)492.25*64);
  //writeAD5933FrequencyHz((double)500.0*64);
  //writeAD5933FrequencyHz((double)30000.0);
  //writeAD5933FrequencyHz((double)100.0);
  writeAD5933IncrementHz((double)0);
  writeAD5933IncrementsN((word) 0);
  writeAD5933SettlingCnt(15);

  totalWait = 0;
  
  // I-cycle
  totalReI = 0.0;
  totalImI = 0.0;

  // V-cycle
  totalReV = 0.0;
  totalImV = 0.0;
  
  commandAD5933Reset();       // reset command
  commandAD5933StanbyMode();  // standby mode command. STANDBY->INITIALIZE->DELAY->SWEEP->INCREMENT is very stable !
  commandAD5933InitializeWithStartFrequency();   // initialize with start frequency
  for (byte i = 0; i < 1; i++) {
    // V-Cycle. Start V-Cycle as early as possible to let the switching to settle
    digitalWrite(49, LOW);      // switch. LOW=V, HIGH=I
    
    myGLCD.setFont(SmallFont);
    myGLCD.setColor(255, 255, 255);
    myGLCD.print("H", 461, 1);
    do {
    } while (analogRead(8) > 128);
    myGLCD.setFont(SmallFont);
    myGLCD.setColor(128, 128, 128);
    myGLCD.print("L", 461, 1);
    do {
    } while (analogRead(8) <= 128);
    
    //commandAD5933StanbyMode();  // standby mode command. STANDBY->INITIALIZE->DELAY->SWEEP->INCREMENT is very stable !
    //commandAD5933InitializeWithStartFrequency();   // initialize with start frequency
    //commandAD5933StartFrequencySweep();
    delay(20);

    if (i==0) { 
      commandAD5933StartFrequencySweep(); } 
    else {
      commandAD5933IncrementFrequency();
    }
    totalWait += delay2msAD5933UntilReady();
    
    // I-Cycle. Start I-Cycle as early as possible to let the switching to settle
    digitalWrite(49, HIGH);     // switch. LOW=V, HIGH=I
    delay(20);
    
    double VReOnce = readAD5933RealData();
    double VImOnce = readAD5933ImaginaryData();
    
    /*
    myGLCD.setFont(SmallFont);
    myGLCD.setColor(255, 255, 255);
    myGLCD.print("H", 461, 1);
    do {
    } while (analogRead(8) > 128);
    myGLCD.setFont(SmallFont);
    myGLCD.setColor(128, 128, 128);
    myGLCD.print("L", 461, 1);
    do {
    } while (analogRead(8) <= 128);
    */
    //commandAD5933StanbyMode();  // standby mode command. STANDBY->INITIALIZE->DELAY->SWEEP->INCREMENT is very stable !
    //commandAD5933InitializeWithStartFrequency();   // initialize with start frequency
    //commandAD5933StartFrequencySweep();
    //commandAD5933StanbyMode();  // standby mode command. STANDBY->INITIALIZE->DELAY->SWEEP->INCREMENT is very stable !
    //commandAD5933InitializeWithStartFrequency();   // initialize with start frequency
    //commandAD5933StartFrequencySweep();
    
    commandAD5933RepeatFrequency(); 
    totalWait += delay2msAD5933UntilReady();
    double IReOnce = readAD5933RealData();
    double IImOnce = readAD5933ImaginaryData();
    
    totalReV = totalReV + VReOnce;
    totalImV = totalImV + VImOnce;

    totalReI = totalReI + IReOnce;
    totalImI = totalImI + IImOnce;
  }
  //myGLCD.print("mcs: ", 240, 1);  myGLCD.printNumI(totalWait, 317, 1);
  // FIXED CALIBRATION VALUES
  totalReI = totalReI - 16.0;
  totalImI = totalImI -  4.0;

  totalReI = totalReI / ((double)1.0 * double(1.0));
  totalImI = totalImI / ((double)1.0 * double(1.0));

/*
  digitalWrite(49, LOW);      // switch. LOW=V, HIGH=I
  commandAD5933Reset();       // reset command
  commandAD5933StanbyMode();  // standby mode command. STANDBY->INITIALIZE->DELAY->SWEEP->INCREMENT is very stable !
  commandAD5933InitializeWithStartFrequency();   // initialize with start frequency
  for (byte i = 0; i < 6; i++) {
    myGLCD.setFont(SmallFont);
    myGLCD.setColor(255, 255, 255);
    myGLCD.print("H", 461, 1);
    do {
    } while (analogRead(8) > 128);
    myGLCD.setFont(SmallFont);
    myGLCD.setColor(128, 128, 128);
    myGLCD.print("L", 461, 1);
    do {
    } while (analogRead(8) <= 128);
    //commandAD5933StanbyMode();  // standby mode command. STANDBY->INITIALIZE->DELAY->SWEEP->INCREMENT is very stable !
    //commandAD5933InitializeWithStartFrequency();   // initialize with start frequency
    //commandAD5933StartFrequencySweep();
    //commandAD5933StanbyMode();  // standby mode command. STANDBY->INITIALIZE->DELAY->SWEEP->INCREMENT is very stable !
    //commandAD5933InitializeWithStartFrequency();   // initialize with start frequency
    //commandAD5933StartFrequencySweep();
    if (i==0) { 
      commandAD5933StartFrequencySweep(); } 
    else {
      commandAD5933IncrementFrequency();
    }
    totalWait += delay2msAD5933UntilReady();
    double ReOnce = readAD5933RealData();
    double ImOnce = readAD5933ImaginaryData();
    totalReV = totalReV + ReOnce;
    totalImV = totalImV + ImOnce;
  }
  //myGLCD.print("mcs: ", 240, 1);  myGLCD.printNumI(totalWait, 317, 1);

  // FIXED CALIBRATION VALUES
  totalReV = totalReV - 16.0;
  totalImV = totalImV -  4.0;
*/
  // Look at V channel gain setting!
  totalReV = totalReV * (double)16.0/(double)1.0; //0.25;
  totalImV = totalImV * (double)16.0/(double)1.0; //0.25;

  writeAD5933FrequencyHz((double)10000.0);

  displayData();
  BigNumber::finish();

  // walk through all settling counts
  settlingCount++;
  if (settlingCount > 2044) settlingCount = 0;
}

void selectDiv1Frequency()
{
  analogWrite(11, 255); // inverted by level shifting transistor
  analogWrite(12, 255); // inverted by level shifting transistor
  analogWrite(13, 255); // inverted by level shifting transistor
  TimeBaseFrequencyHz = (double)16384000.0;
}

void selectDiv2Frequency()
{
  analogWrite(11, 0);   // inverted by level shifting transistor
  analogWrite(12, 255); // inverted by level shifting transistor
  analogWrite(13, 255); // inverted by level shifting transistor
  TimeBaseFrequencyHz = (double)8192000.0;
}

void selectDiv16Frequency()
{
  analogWrite(11, 255); // inverted by level shifting transistor
  analogWrite(12, 0);   // inverted by level shifting transistor
  analogWrite(13, 255); // inverted by level shifting transistor
  TimeBaseFrequencyHz = (double)1024000.0;
}

void selectDiv64Frequency()
{
  analogWrite(11, 0);  // inverted by level shifting transistor
  analogWrite(12, 0);  // inverted by level shifting transistor
  analogWrite(13, 255); // inverted by level shifting transistor
  TimeBaseFrequencyHz = (double)256000.0;
}

void selectDiv256Frequency()
{
  analogWrite(11, 255); // inverted by level shifting transistor
  analogWrite(12, 255); // inverted by level shifting transistor
  analogWrite(13, 0);   // inverted by level shifting transistor
  TimeBaseFrequencyHz = (double)64000.0;
}

void selectDiv1024Frequency()
{
  analogWrite(11, 0);    // inverted by level shifting transistor
  analogWrite(12, 255);  // inverted by level shifting transistor
  analogWrite(13, 0);    // inverted by level shifting transistor
  TimeBaseFrequencyHz = (double)16000.0;
}

void selectDiv4096Frequency()
{
  analogWrite(11, 255);  // inverted by level shifting transistor
  analogWrite(12, 0);    // inverted by level shifting transistor
  analogWrite(13, 0);    // inverted by level shifting transistor
  TimeBaseFrequencyHz = (double)4000.0;
}

void selectDiv16384Frequency()
{
  analogWrite(11, 0);  // inverted by level shifting transistor
  analogWrite(12, 0);  // inverted by level shifting transistor
  analogWrite(13, 0);  // inverted by level shifting transistor
  TimeBaseFrequencyHz = (double)1000.0;
}

void selectOhm24Range()
{
  // Deenergize all range relays
  digitalWrite(45, LOW);   // PL4 range 4 HIGH=Energized
  digitalWrite(46, LOW);   // PL3 range 3 HIGH=Energized
  digitalWrite(47, LOW);   // PL2 range 2 HIGH=Energized
  digitalWrite(48, LOW);   // PL1 range 1 HIGH=Energized
  // Energize reset coils
  digitalWrite(44, HIGH);  // PL5 relay reset HIGH=Energized
  delay(2);
  digitalWrite(44, LOW);   // PL5 relay reset HIGH=Energized
  delay(2);
  // Energize one range relay
  digitalWrite(48, HIGH);   // PL1 range 1 HIGH=Energized
  delay(2);
  digitalWrite(48, LOW);    // PL1 range 1 HIGH=Energized
  ReferenceOhm = (double)0.24;
  ReferenceOhm = (double)0.45610274114131552523016247474564;
  ReferenceOhm = (double)0.48460673121136234408836303366844;
}

void select50ohmRange()
{
  // Deenergize all range relays
  digitalWrite(45, LOW);   // PL4 range 4 HIGH=Energized
  digitalWrite(46, LOW);   // PL3 range 3 HIGH=Energized
  digitalWrite(47, LOW);   // PL2 range 2 HIGH=Energized
  digitalWrite(48, LOW);   // PL1 range 1 HIGH=Energized
  // Energize reset coils
  digitalWrite(44, HIGH);  // PL5 relay reset HIGH=Energized
  delay(2);
  digitalWrite(44, LOW);   // PL5 relay reset HIGH=Energized
  delay(2);
  // Energize one range relay
  digitalWrite(47, HIGH);   // PL2 range 2 HIGH=Energized
  delay(2);
  digitalWrite(47, LOW);   // PL2 range 2 HIGH=Energized
  ReferenceOhm = (double)50.0;
}

void select10KOhmRange()
{
  // Deenergize all range relays
  digitalWrite(45, LOW);   // PL4 range 4 HIGH=Energized
  digitalWrite(46, LOW);   // PL3 range 3 HIGH=Energized
  digitalWrite(47, LOW);   // PL2 range 2 HIGH=Energized
  digitalWrite(48, LOW);   // PL1 range 1 HIGH=Energized
  // Energize reset coils
  digitalWrite(44, HIGH);  // PL5 relay reset HIGH=Energized
  delay(2);
  digitalWrite(44, LOW);   // PL5 relay reset HIGH=Energized
  delay(2);
  // Energize one range relay
  digitalWrite(46, HIGH);   // PL3 range 3 HIGH=Energized
  delay(2);
  digitalWrite(46, LOW);   // PL3 range 3 HIGH=Energized
  //ReferenceOhm = (double)10333.3333;
  ReferenceOhm = (double)10025.531726282725800284844512745;
}

void select200KOhmRange()
{
  // Deenergize all range relays
  digitalWrite(45, LOW);   // PL4 range 4 HIGH=Energized
  digitalWrite(46, LOW);   // PL3 range 3 HIGH=Energized
  digitalWrite(47, LOW);   // PL2 range 2 HIGH=Energized
  digitalWrite(48, LOW);   // PL1 range 1 HIGH=Energized
  // Energize reset coils
  digitalWrite(44, HIGH);  // PL5 relay reset HIGH=Energized
  delay(2);
  digitalWrite(44, LOW);   // PL5 relay reset HIGH=Energized
  delay(2);
  // Energize one range relay
  digitalWrite(45, HIGH);   // PL4 range 4 HIGH=Energized
  delay(2);
  digitalWrite(45, LOW);   // PL4 range 4 HIGH=Energized
  ReferenceOhm = (double)200000.0;
}

unsigned long delay2msAD5933UntilReady()
{
  unsigned long start = millis();
  byte r8F;
  myGLCD.setFont(SmallFont);
  myGLCD.setColor(255, 128, 128);
  myGLCD.print(" ", 461, 1);
  for (int i = 0; i < 100; i++) {
    delay(2);
    r8F = readAD5933(0x8F);
    if ((r8F & 0x02) == 0) {
      myGLCD.print("?", 461, 1);
    }
    else
    {
      myGLCD.print("R", 461, 1);
      break;
    }
  }
  return millis() - start;
}

void displayR80()
{
  myGLCD.setFont(SmallFont);
  myGLCD.setColor(128, 128, 255);
  myGLCD.print(formatHEX8(readAD5933(0x80)), 81, 1);
}

void displayR81()
{
  myGLCD.setFont(SmallFont);
  myGLCD.setColor(128, 128, 255);
  myGLCD.print(formatHEX8(readAD5933(0x81)), 97, 1);
}

void commandAD5933StanbyMode()
{
  byte r80 = readAD5933(0x80);
  r80 = 0xB0 | (r80 & 0x0F);
  writeAD5933(0x80, r80);
  displayR80();
}

void commandAD5933Reset()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x80 | (r80 & 0x0F);
  writeAD5933(0x80, r80);
  displayR80();
}

void commandAD5933ExternalClock()
{
  byte r81 = readAD5933(0x81);
  r81 = 0x08 | (r81 & 0xF7);
  writeAD5933(0x81, r81);
  displayR81();
}

void commandAD5933InitializeWithStartFrequency()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x10 | (r80 & 0x0F);
  writeAD5933(0x80, r80);
  displayR80();
}

void commandAD5933StartFrequencySweep()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x20 | (r80 & 0x0F);
  writeAD5933(0x80, r80);
  displayR80();
}

void commandAD5933RepeatFrequency()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x40 | (r80 & 0x0F);
  writeAD5933(0x80, r80);
  displayR80();
}

void commandAD5933IncrementFrequency()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x30 | (r80 & 0x0F);
  writeAD5933(0x80, r80);
  displayR80();
}

void commandAD5933MeasureTemperature()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x90 | (r80 & 0x0F);
  writeAD5933(0x80, r80);
  displayR80();
}

void commandAD5933Voltage3V()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x00 | (r80 & 0xF9);
  writeAD5933(0x80, r80);
  displayR80();
  myGLCD.print("3000mV", 217, 12);
}

void commandAD5933Voltage1500mV()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x06 | (r80 & 0xF9);
  writeAD5933(0x80, r80);
  displayR80();
  myGLCD.print("1500mV", 217, 12);
}

void commandAD5933Voltage600mV()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x04 | (r80 & 0xF9);
  writeAD5933(0x80, r80);
  displayR80();
  myGLCD.print("600mV ", 217, 12);
}

void commandAD5933Voltage300mV()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x02 | (r80 & 0xF9);
  writeAD5933(0x80, r80);
  displayR80();
  myGLCD.print("300mV ", 217, 12);
}

void commandAD5933GainX1()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x01 | (r80 & 0xFE);
  writeAD5933(0x80, r80);
  displayR80();
  myGLCD.print("x1", 313, 1);
}

void commandAD5933GainX5()
{
  byte r80 = readAD5933(0x80);
  r80 = 0x00 | (r80 & 0xFE);
  writeAD5933(0x80, r80);
  displayR80();
  myGLCD.print("x5", 313, 1);
}

void displayGrid()
{
  //myGLCD.setFont(BigFont);
  myGLCD.setFont(SmallFont);
  myGLCD.setColor(128, 128, 255);
  myGLCD.print("  Command", 1,   1);  myGLCD.print("Temperature", 121,   1);  myGLCD.print("    Gain", 241,   1);  myGLCD.print("    Status", 361,   1);
  myGLCD.print(" Settling", 1,  12);  myGLCD.print("P-P Voltage", 121,  12);  myGLCD.print("    Step", 241,  12);  myGLCD.print("Increments", 361,  12);
  myGLCD.print("Frequency", 1,  24);

  myGLCD.print("ReI", 1, 48);  myGLCD.print("ReV", 121, 48);  myGLCD.print("Absolute V", 241, 48);
  myGLCD.print("ImI", 1, 60);  myGLCD.print("ImV", 121, 60);  myGLCD.print("Angle Volt", 241, 60);
  myGLCD.print("AbI", 1, 72);  myGLCD.print("AbV", 121, 72);  myGLCD.print("Angle Curr", 241, 72);
  myGLCD.print("AnI", 1, 84);  myGLCD.print("AnV", 121, 84);  myGLCD.print("Angle Loss", 241, 84);

  myGLCD.setFont(BigFont);
  myGLCD.setColor(255, 255, 0);
  myGLCD.print("D    ", 1, 100);
  myGLCD.print("Q    ", 1, 120);
  myGLCD.print("Z,Ohm", 1, 140);
  myGLCD.print("R,Ohm", 1, 160);
  myGLCD.print("C,nF ", 1, 180);
  myGLCD.print("L,uH ", 1, 200);

}

void displayTemperature()
{
  commandAD5933MeasureTemperature(); // measure temperature command
  byte r8F;
  do {
    r8F = readAD5933(0x8F) & 0x07;
    if ((r8F & 0x01) == 0) {
      delay(10);
    }
  } while ((r8F & 0x01) == 0);
  double TempC = readAD5933TemperatureC();
  myGLCD.setFont(SmallFont);
  myGLCD.setColor(128, 128, 255);
  myGLCD.printNumF(TempC, 0, 209, 1, '.', 5);
}

void display14BitData(double value, short X, short Y)
{
  if (abs(value) > 16383) {
    myGLCD.setBackColor(255, 255, 255);
    myGLCD.setColor(255, 0, 0);
    myGLCD.printNumF(value, 3, X, Y, '.', 9);
    myGLCD.setBackColor(0, 0, 0);
    myGLCD.setColor(128, 128, 255);
  } else
  {
    myGLCD.printNumF(value, 3, X, Y, '.', 9);
  }
}

void display9PosData(double value, double maxabs, short X, short Y)
{
  byte usedlen = 0;
  if (value < 0) usedlen++; // 1 position is used for sign

}

void displayData()
{
  myGLCD.setFont(SmallFont);
  myGLCD.setColor(128, 128, 255);
  byte r8F = readAD5933(0x8F);
  if ((r8F & 0x01) != 0) myGLCD.print("T", 449, 1); else myGLCD.print("?", 449, 1);
  if ((r8F & 0x02) != 0) myGLCD.print("D", 461, 1); else myGLCD.print("?", 461, 1);
  if ((r8F & 0x04) != 0) myGLCD.print("S", 473, 1); else myGLCD.print("?", 473, 1);

  display14BitData(totalReI,  41,  48);  display14BitData(totalReV, 161,  48);
  display14BitData(totalImI,  41,  60);  display14BitData(totalImV, 161,  60);
  double AbV = sqrt(sq(totalReV) + sq(totalImV));
  myGLCD.printNumF(AbV, 3, 161, 72, '.', 9);
  double AnV = (atan2(totalImV, totalReV) / M_PI) * (double)180.0;
  myGLCD.printNumF(AnV, 3, 161, 84, '.', 9);

  // get frequency
  double frequencyHz = readAD5933FrequencyHz();

  // subtract current sense voltage from high potential voltage to get voltage of DUT
  totalReV = totalReV - totalReI;
  totalImV = totalImV - totalImI;

  BigNumber pie("3.141592653589793238");
  char tmp[20];
  BigNumber bnTotalReV(dtostrf(totalReV, 9, 9, tmp));
  BigNumber bnTotalImV(dtostrf(totalImV, 9, 9, tmp));
  BigNumber bnAbV = bnTotalReV * bnTotalReV + bnTotalImV * bnTotalImV;
  bnAbV = bnAbV.sqrt();
  BigNumber bn180(180);
  AbV = sqrt(sq(totalReV) + sq(totalImV));
  AnV = (atan2(totalImV, totalReV) / M_PI) * (double)180.0;
  BigNumber bnAnV = (atan2(bnTotalImV, bnTotalReV) / pie) * bn180;

  // compute parasitic current of probe 28.3pF Xprobe = 1/2PifC
  //double Xprobe = (double)1.0/((double)2.0 * M_PI * frequencyHz * (double)0.0000283);
  BigNumber one(1);
  BigNumber two(2);
  BigNumber bnFrequencyHz(dtostrf(frequencyHz, 9, 9, tmp));
  //BigNumber bnProbe_pF("0.0000000000283");
  BigNumber bnProbe_pF("0.0000000000");
  BigNumber bnXprobe = one / (two * pie * bnFrequencyHz * bnProbe_pF);
  double probeImI = 0.0; /// AbV / Xprobe;
  BigNumber bnProbeImI = bnAbV / bnXprobe;
  BigNumber bnReferenceOhm(dtostrf(ReferenceOhm, 9, 9, tmp));
  bnProbeImI = bnProbeImI * bnReferenceOhm; // brought correction value to Volts units
  probeImI = probeImI / (double)1000000.0;

  double AbI = sqrt(sq(totalReI) + sq(totalImI));
  myGLCD.printNumF(AbI, 3, 41, 72, '.', 9);
  /*
  BigNumber bnTotalReI(dtostrf(totalReI, 9, 9, tmp));
  BigNumber bnTotalImI(dtostrf(totalImI, 9, 9, tmp));
  BigNumber bnAbI = bnTotalReI*bnTotalReI + bnTotalImI*bnTotalImI;
  bnAbI = bnAbI.sqrt();
  BigNumber bnAnI = (atan2(bnTotalImI, bnTotalReI)/pie)*bn180;
  char * buf = bnAbI.toString();
  myGLCD.print(buf, 41, 72);
  free(buf);
  */
  double AnI = (atan2(totalImI, totalReI) / M_PI) * (double)180.0;
  myGLCD.printNumF(AnI, 3, 41, 84, '.', 9);

  /*
  buf = bnAnI.toString();
  myGLCD.print(buf, 41, 84);
  free(buf);
  */

  AnI = AnI - AnV;
  if (AnI > (double) 180.0) AnI = AnI - (double)360.0;
  if (AnI < (double) - 180.0) AnI = AnI + (double)360.0;

  // Complex current components after making AnV=0
  totalReI = AbI * cos(AnI * M_PI / (double)180.0);
  totalImI = AbI * sin(AnI * M_PI / (double)180.0);
  // correct totalImI, AbI and AnI for probe current
  totalImI = totalImI - (-probeImI); // correction looks like addition, because we subtracting negative
  AbI = sqrt(sq(totalReI) + sq(totalImI));
  AnI = (atan2(totalImI, totalReI) / M_PI) * (double)180.0;

  // correct current angle for -0.41 degrees for known resistive DUT
  // correct current angle for +0.177 degrees for known resistive DUT
  //AnI = AnI - (double)0.360 + (double)0.125 + (double)0.213;
  //AnI = AnI - (double)0.177;
  //AnI = AnI + (double)13.900;
  //AnI = AnI + (double)0.300;
  AnI = AnI - (double)3.286; // 64 gain I, 1:1 gain V, @10KHz, 0.24Ohm range
  if (AnI > (double) 180.0) AnI = AnI - (double)360.0;
  if (AnI < (double) - 180.0) AnI = AnI + (double)360.0;
  totalReI = AbI * cos(AnI * M_PI / (double)180.0);
  totalImI = AbI * sin(AnI * M_PI / (double)180.0);

  // find current amplitude from current sensor voltage
  AbI = AbI / ReferenceOhm;
  totalReI = totalReI / ReferenceOhm;
  totalImI = totalImI / ReferenceOhm;
  {
    // Losses Angle
    double lossA = (double)90.0 + AnI;
    myGLCD.printNumF(AbV,   3, 329, 48, '.', 9);
    myGLCD.printNumF(AnV,   3, 329, 60, '.', 9);
    myGLCD.printNumF(AnI,   3, 329, 72, '.', 9);
    myGLCD.printNumF(lossA, 3, 329, 84, '.', 9);
  }
  // complex impedance in ohms iz complex division of real voltage by (real current + imaginary current)
  // (x+yi)/(u+vi)=((xu + yv)+(-xv+yu)i)/(u*u+v*v)
  // then, when y=0
  // x/(u+vi)=(xu-xvi)/(u*u+v*v)
  // then for real and imaginary resistance component
  // Rre = xu/(u*u+v*v)
  // Rim = -xvi/(u*u+v*v)
  //
  // AbI is a square root, so unsquareroot it by multiplying by itself
  // use AbI*AbI as ReI^2+ImI^2
  double Rre =  AbV * totalReI / (AbI * AbI);
  double Rim = -AbV * totalImI / (AbI * AbI);

  double tanLs = (Rre / Rim);
  // Capacitance
  double Cap = (double)(2.0) * M_PI * frequencyHz * Rim;
  Cap = Cap / (double)1000000.0;
  Cap = (double)1000.0 / Cap;
  myGLCD.setFont(BigFont);
  myGLCD.setColor(255, 255, 0);
  myGLCD.printNumF(tanLs, 6, 100, 100, '.', 10);
  double Q = abs(Rim / Rre);
  myGLCD.printNumF(Q,     6, 100, 120, '.', 10);
  double AbZ = sqrt(sq(Rre) + sq(Rim));
  myGLCD.print("                                       ", 100, 140);
  if (AbZ > 9999999.99) {
    myGLCD.print("INFINITY", 100, 140);
  } else {
    myGLCD.printNumF(AbZ, 6, 100, 140, '.', 10);
  }
  myGLCD.printNumF(Rre, 6, 100, 160, '.', 10);
  myGLCD.printNumF(Cap, 6, 100, 180, '.', 14);

  // Inductance
  double Ind = -Rim / ((double)(2.0) * M_PI * frequencyHz);
  Ind = Ind * (double)1000000.0;
  myGLCD.setColor(255, 255, 0);
  myGLCD.print("                                       ", 100, 200);
  if (abs(Ind) > 9999999.99) {
    myGLCD.print("INFINITY", 100, 200);
  } else {
    myGLCD.printNumF(Ind, 6, 100, 200, '.', 10);
  }
}

char formatHEX8Result[3];
char* formatHEX8(byte value)
{
  sprintf(formatHEX8Result, "%02X", value);
  formatHEX8Result[2] = 0;
  return formatHEX8Result;
}

double readAD5933FrequencyHz()
{
  long triple = 0;
  long single = 0;
  single = readAD5933(0x82);
  triple = triple | (single << 16);
  single = readAD5933(0x83);
  triple = triple | (single << 8);
  single = readAD5933(0x84);
  triple = triple | (single << 0);
  double TimeBaseDivideBy4 = TimeBaseFrequencyHz / (double)4.0;
  double result = TimeBaseDivideBy4 * ((double)triple / (double)134217728.0); // 2^27;
  return result;
}

void writeAD5933FrequencyHz(double valueHz)
{
  double TimeBaseDivideBy4 = TimeBaseFrequencyHz / (double)4.0;
  double phaseSpeed = valueHz / TimeBaseDivideBy4;
  double controlDbl = phaseSpeed * (double)134217728.0;
  long triple = (long)controlDbl;
  byte single = 0;
  single = (triple >> 16) & 0xFF;
  writeAD5933(0x82, single);
  single = (triple >> 8) & 0xFF;
  writeAD5933(0x83, single);
  single = (triple >> 0) & 0xFF;
  writeAD5933(0x84, single);

  double frequencyHz = readAD5933FrequencyHz();
  myGLCD.setFont(SmallFont);
  if (frequencyHz < TimeBaseDivideBy4 / 800.0 || frequencyHz > TimeBaseDivideBy4 / 20.0)
  {
    myGLCD.setBackColor(255, 255, 255);
    myGLCD.setColor(255, 0, 0);
  } else
  {
    myGLCD.setBackColor(0, 0, 0);
    myGLCD.setColor(128, 128, 255);
  }
  myGLCD.setBackColor(0, 0, 0);
  myGLCD.print("      ", 81, 24);
  myGLCD.printNumI(round(frequencyHz), 81, 24, 5);
}

double readAD5933IncrementHz()
{
  long triple = 0;
  long single = 0;
  single = readAD5933(0x85);
  triple = triple | (single << 16);
  single = readAD5933(0x86);
  triple = triple | (single << 8);
  single = readAD5933(0x87);
  triple = triple | (single << 0);
  double TimeBaseDivideBy4 = TimeBaseFrequencyHz / (double)4.0;
  double result = TimeBaseDivideBy4 * ((double)triple / (double)134217728.0); // 2^27;
  return result;
}

void writeAD5933IncrementHz(double valueHz)
{
  double TimeBaseDivideBy4 = TimeBaseFrequencyHz / (double)4.0;
  double phaseSpeed = valueHz / TimeBaseDivideBy4;
  double controlDbl = phaseSpeed * (double)134217728.0;
  long triple = (long)controlDbl;
  byte single = 0;
  single = (triple >> 16) & 0xFF;
  writeAD5933(0x85, single);
  single = (triple >> 8) & 0xFF;
  writeAD5933(0x86, single);
  single = (triple >> 0) & 0xFF;
  writeAD5933(0x87, single);
  myGLCD.setFont(SmallFont);
  myGLCD.setColor(128, 128, 255);
  double incrementHz = readAD5933IncrementHz();
  myGLCD.printNumI(round(incrementHz), 313, 12, 4);
}

word readAD5933IncrementsN()
{
  word result = 0;
  word single = 0;
  single = readAD5933(0x88);
  result = result | ((single & 0x01) << 8);
  single = readAD5933(0x89);
  result = result | ((single & 0xFF) << 0);
  return result;
}

void writeAD5933IncrementsN(word le511)
{
  byte single = 0;
  single = (le511 >> 8) & 0x01;
  writeAD5933(0x88, single);
  single = (le511 >> 0) & 0xFF;
  writeAD5933(0x89, single);
  myGLCD.setFont(SmallFont);
  myGLCD.setColor(128, 128, 255);
  le511 = readAD5933IncrementsN();
  myGLCD.printNumI(le511, 449, 12, 4);
}

word readAD5933SettlingCnt()
{
  word r8A = readAD5933(0x8A);
  word r8B = readAD5933(0x8B);
  word result = r8B | ((r8A & 0x01) << 8);
  if ((r8A & 0x06) == 0x06) {
    result = result * 4;
  } else if ((r8A & 0x06) == 0x02)
  {
    result = result * 2;
  };
  return result;
}

void writeAD5933SettlingCnt(word le2044)
{
  byte r8A = 0;
  byte r8B = 0;
  if (le2044 > 1022)
  {
    r8A = 0x06 | ((le2044 >> 10) & 0x01); // use x4 multiplier and occupy 1 MSb
    r8B = (le2044 >> 2) & 0xFF;           // occupy 8 LSb
  } else if (le2044 > 511)
  {
    r8A = 0x02 | ((le2044 >> 9) & 0x01); // use x2 multiplier and occupy 1 MSb
    r8B = (le2044 >> 1) & 0xFF;          // occupy 8 LSb
  } else
  {
    r8A = 0x00 | ((le2044 >> 8) & 0x01); // use x1 multiplier and occupy 1 MSb
    r8B = (le2044 >> 0) & 0xFF;          // occupy 8 LSb
  }
  writeAD5933(0x8A, r8A);
  writeAD5933(0x8B, r8B);
  myGLCD.setFont(SmallFont);
  myGLCD.setColor(128, 128, 255);
  le2044 = readAD5933SettlingCnt();
  myGLCD.printNumI(le2044, 81, 12, 4);
}

double readAD5933TemperatureC()
{
  long r92 = readAD5933(0x92);
  r92 = r92 << 26;
  long r93 = readAD5933(0x93);
  r93 = r93 << 18;
  long ival = r92 | r93;
  ival = ival / 262144;
  double result = (double)ival * (double)0.03125;
  return result;
}

double readAD5933RealData()
{
  int ival = (readAD5933(0x94) << 8) | readAD5933(0x95);
  double result = (double)ival;
  return result;
}

double readAD5933ImaginaryData()
{
  int ival = (readAD5933(0x96) << 8) | readAD5933(0x97);
  double result = (double)ival;
  return result;
}

byte readAD5933(byte registerNumber)
{
  byte twoBytes[2];
  twoBytes[0] = 0xB0; // Pointer Command
  twoBytes[1] = registerNumber; // Register 0x80..0x9X
  Wire.beginTransmission(0x0D); // 0x0D is I2C 7 bit address of AD5933
  Wire.write(twoBytes, 2);
  Wire.endTransmission();

  Wire.requestFrom(0x0D, 1, true); // 0x0D is I2C 7 bit address of AD5933
  byte result = Wire.read(); // read one byte
  return result;
}

void writeAD5933(byte registerNumber, byte value)
{
  byte twoBytes[2];
  twoBytes[0] = registerNumber; // Register 0x80..0x9X
  twoBytes[1] = value; // value
  Wire.beginTransmission(0x0D); // 0x0D is I2C 7 bit address of AD5933
  Wire.write(twoBytes, 2);
  Wire.endTransmission();
}
