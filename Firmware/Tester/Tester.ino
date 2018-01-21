/*
  Using the [device name]
  By: Nathan Seidle
  SparkFun Electronics
  Date: December 21st, 2017
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/[product ID]

  Hardware Connections:
  Attach the Qwiic Shield to your Arduino/Photon/ESP32 or other
  Plug the sensor onto the shield
  Serial.print it out at 9600 baud to serial monitor.

  Available:

  We want rotation vector! 3.5degree dynamic max, 2 degree static max error in heading, in practice it's 5 degrees?
  400Hz is max available rate of rotation vector
  Approx 11mA at 100Hz

  Stability classifier, is it on table, stable(in hand), or in motion
  Tap detector: Single or double tap and the axis that tap was detected.
  Step detector: either on wrist or all other uses (in pocket),
  Step counter: same thing just more analyzation along the way, more accurate
  Activity classification: are you running, walking, or still?
  Shake detector

  Calibrate sensor, orientations, sit still, rotate 180

  Set feature command = page 27 is 17 bytes, 21 total bytes with header

*/

#include <Wire.h>
#include <SoftwareSerial.h>
SoftwareSerial mySerial(A4, A5); // RX, TX
#include <SPI.h>

byte deviceAddress = 0x4B; //0x4A if you close the jumper

byte shtpHeader[4]; //Each packet has a header of 4 bytes

#define MAX_PACKET_SIZE 512
byte shtpData[MAX_PACKET_SIZE]; //Packets can be up to 32k but we don't have that much RAM.
int dataSpot = 0;

//Test jig pinouts
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
const byte PS0_WAKE = 5;
const byte PS1 = 4;
const byte INT = 3;
const byte RST = 2;

const byte SPI_CS = 10;
const byte ADR_MOSI = 11;
const byte SPI_MISO = 12;
const byte SPI_SCK = 13;

const byte TEST_BUTTON = A0;
const byte TEST_LED = A1;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void setup()
{
  pinMode(PS0_WAKE, OUTPUT);
  pinMode(PS1, OUTPUT);
  pinMode(INT, INPUT_PULLUP);
  pinMode(RST, OUTPUT);
  
  pinMode(SPI_CS, INPUT); //High impedance for now
  pinMode(ADR_MOSI, OUTPUT);
  pinMode(SPI_MISO, INPUT); //High impedance for now
  pinMode(SPI_SCK, INPUT); //High impedance for now

  pinMode(TEST_BUTTON, INPUT_PULLUP);
  pinMode(TEST_LED, OUTPUT);

  Serial.begin(9600);
  Serial.println();
  Serial.println("BNO080 Tester");
}

void loop()
{
  boolean testI2CA = false;
  boolean testI2CB = false;
  boolean testSPI = false;
  boolean testUART = false;
  boolean testINT = false;
  
  //Test I2C
  //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  digitalWrite(ADR_MOSI, LOW); //Set I2C address to 0x4A
  deviceAddress = 0x4A;

  setMode(0b00); //Reset IC and set mode to I2C

  Wire.begin();

  //The BNO080 comes out of reset with 3 init packet announcements. Look for one.
  if (receivePacket() == true)
    testI2CA = true;

  digitalWrite(ADR_MOSI, HIGH); //Set I2C address to 0x4B
  deviceAddress = 0x4B;

  setMode(0b00); //Reset IC and set mode to I2C

  //The BNO080 comes out of reset with 3 init packet announcements. Look for one.
  if (receivePacket() == true)
    testI2CB = true;

  if (digitalRead(INT) == LOW) //Sensor is trying to tell us data is available
    testINT = true;
    
  //Test UART
  //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  mySerial.begin(115200);

  setMode(0b01); //Reset IC and set mode to UART-RVC

  //As unit comes out of reset it transmits:
  //%Hillcrest Labs 10003608
  //%SW Ver 3.2.x
  //%(c) 2017 Hillcrest Laboratories, Inc.

  //Look for %Hillcrest
  if(mySerial.available())
  {
    if(mySerial.read() == '%')
    {
      if(mySerial.read() == 'H')
        //Good enough for me
        testUART = true;
    }
  }

  //Test SPI
  //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  pinMode(SPI_CS, OUTPUT);
  
  SPI.begin();

  setMode(0b11); //Reset IC and set mode to SPI

  //The BNO080 comes out of reset with 3 init packet announcements. Look for one.
  if (receivePacketSPI() == true)
    testSPI = true;


  if(testI2CA == true && testI2CB == true) 
  {
    if(testUART == true)
    {
      if(testSPI == true)
      {
        if(testINT == true)
        {
          Serial.println("Board is good!");
        }
      }
    }
  }
  
  if(testI2CA == false) 
    Serial.println("I2CA fail");
  if(testI2CB == false) 
    Serial.println("I2CB fail");
  if(testUART == false) 
    Serial.println("UART fail");
  if(testSPI == false) 
    Serial.println("SPI fail");
  if(testINT == false) 
    Serial.println("INT fail");

    while(1);
}

//Pull WAKE pin low to indicate we are ready to talk to the sensor
//Wait for sensor to pull INT pin low indicating it's ready for our talking
void startTransfer()
{
  digitalWrite(PS0_WAKE, LOW); //Tell sensor we are ready to talk

  int counter = 0;
  while (digitalRead(INT) == HIGH) //Wait for sensor to confirm
  {
    delay(1);
    if (counter++ > 500)
    {
      Serial.println(F("Sensor never pulled INT low"));
      return;
    }
  }

  digitalWrite(PS0_WAKE, HIGH); //Release WAKE

  Serial.println(F("Sensor is ready to talk!"));
}

//Resets the IC and puts it into one of four modes: UART, UART-RVC, SPI, I2C
void setMode(byte mode)
{
  //Begin test with unit in reset
  digitalWrite(RST, LOW);
  
  delay(10);

  if(mode == 0) //I2C
  {
    digitalWrite(PS0_WAKE, LOW);
    digitalWrite(PS1, LOW);
  }
  else if(mode == 1) //UART-RVC
  {
    digitalWrite(PS0_WAKE, HIGH);
    digitalWrite(PS1, LOW);
  }
  else if(mode == 2) //UART
  {
    digitalWrite(PS0_WAKE, LOW);
    digitalWrite(PS1, HIGH);
  }
  else if(mode == 3) //SPI
  {
    digitalWrite(PS0_WAKE, HIGH);
    digitalWrite(PS1, HIGH);
  }

  //Then release reset
  digitalWrite(RST, HIGH);

  delay(10);
}

