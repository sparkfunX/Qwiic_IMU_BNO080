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

byte deviceAddress = 0x4B; //0x4A if you close the jumper

byte shtpHeader[4]; //Each packet has a header of 4 bytes

#define MAX_PACKET_SIZE 512
byte shtpData[MAX_PACKET_SIZE]; //Packets can be up to 32k but we don't have that much RAM.
int dataSpot = 0;

//There are 6 com channels. Each channel has its own seqnum
byte sequenceNumber[6] = {0, 0, 0, 0, 0, 0};

const byte WAKE = 8; //Connected through 2.2k resistor
const byte DATA_AVAILABLE = 12; //Aka the INT pin
const byte RST = 11;

const byte CHANNEL_COMMAND = 0;
const byte CHANNEL_EXECUTABLE = 1;
const byte CHANNEL_CONTROL = 2;
const byte CHANNEL_REPORTS = 3;
const byte CHANNEL_WAKE_REPORTS = 4;
const byte CHANNEL_GYRO = 5;

void setup()
{
  Serial.begin(9600);
  Serial.println("BNO080 Read Example");

  Wire.begin();

  //The BMO080 comes out of reset with 3 init packet announcements. Read them.
  while (receivePacket() == true)
  {
    printPacket();
    Serial.println();
  }

  //Enable accelerometer
  shtpData[0] = 0xFD;
  shtpData[1] = 0x01; //Report ID. Listed in SH2 reference manual, page 36
  shtpData[2] = 0;
  shtpData[3] = 0;
  shtpData[4] = 0;
  shtpData[5] = 0x60;
  shtpData[6] = 0xEA;
  shtpData[7] = 0;
  shtpData[8] = 0;
  shtpData[9] = 0;
  shtpData[10] = 0;
  shtpData[11] = 0;
  shtpData[12] = 0;
  shtpData[13] = 0;
  shtpData[14] = 0;
  shtpData[15] = 0;
  shtpData[16] = 0;

  //Transmit packet on channel 2
  sendPacket(CHANNEL_CONTROL, 17);
  Serial.println("Accel configured");
}

void loop()
{
  //The accelerometer should send update every 60ms
  while (receivePacket() == true)
  {
    printPacket();
    Serial.println();
  }
}

//Pretty prints the contents of the current shtp packet
void printPacket(void)
{
  uint16_t packetLength = (uint16_t)shtpHeader[1] << 8 | shtpHeader[0];

  if (packetLength & 1 << 15)
  {
    Serial.println("Continued packet");
    packetLength &= ~(1 << 15);
  }

  Serial.print("Packet Length: ");
  Serial.println(packetLength);

  //Attempt to pre-print known events
  if (shtpHeader[2] == 1 && shtpData[0] == 0x01) {
    Serial.println("Known: Reset complete");
    return;
  }
  if (shtpHeader[2] == 2 && shtpData[0] == 0xF1) {
    Serial.println("Known: Command response");
  }
  if (shtpHeader[2] == 0 && shtpData[0] == 0x00) {
    Serial.println("Known: Init Advertisement");
    return;
  }

  Serial.print("Channel: ");
  if (shtpHeader[2] == 0) Serial.println("Command");
  else if (shtpHeader[2] == 1) Serial.println("Executable");
  else if (shtpHeader[2] == 2) Serial.println("Control");
  else Serial.print(shtpHeader[2]);
  Serial.print("SeqNum: ");
  Serial.println(shtpHeader[3]);

  int dataLength = packetLength - 4;
  if (dataLength > 10) dataLength = 10; //Arbitrary limiter. We don't want the phonebook.
  for (int i = 0 ; i < dataLength ; i++)
  {
    Serial.print(i);
    Serial.print(") 0x");
    if (shtpData[i] < 0x10) Serial.print("0");
    Serial.print(shtpData[i], HEX);
    Serial.println();
  }
}

//Check to see if there is any new data available
//Read the contents of the incoming packet into the shtpData array
boolean receivePacket(void)
{
  Wire.requestFrom((uint8_t)deviceAddress, 4); //Ask for four bytes to find out how much data we need to read
  if (waitForI2C() == 0) return (0); //Error

  //Get the first four bytes, aka the packet header
  byte packetLSB = Wire.read();
  byte packetMSB = Wire.read();
  byte channelNumber = Wire.read();
  byte sequenceNumber = Wire.read(); //Not sure if we need to store this or not

  //Store the header info.
  shtpHeader[0] = packetLSB;
  shtpHeader[1] = packetMSB;
  shtpHeader[2] = channelNumber;
  shtpHeader[3] = sequenceNumber;

  uint16_t dataLength = ((uint16_t)packetMSB << 8 | packetLSB);
  dataLength &= ~(1 << 15); //Clear the MSbit.
  //This bit indicates if this package is a continuation of the last. Ignore it for now.
  //TODO catch this as an error and exit

  if (dataLength == 0)
  {
    //Serial.println("Packet is empty");
    return (false); //All done
  }

  //We have to limit the read length to 32 because of arduino limits - yay
  //Now that we have an idea of how much we need to read, setup a series of chunked 32 byte reads
  //to get the entire packet
  dataSpot = 0;
  dataLength -= 4; //Remove the header bytes from the data count
  while (dataLength > 28)
  {
    getBytes(28);
    dataLength -= 28; //Four bytes are the header so we're only getting 28 data bytes
  }

  getBytes(dataLength);
  return (true); //We're done!
}

//Gets the requested number of bytes
//Arduini I2C read limit is 32 bytes. Header is 4 bytes, so max user can request is 28
void getBytes(byte numberOfBytes)
{
  if (numberOfBytes > 28)
  {
    Serial.println("Too many bytes requested!");
  }

  Wire.requestFrom((uint8_t)deviceAddress, numberOfBytes + 4);
  if (waitForI2C() == 0) return (0); //Error

  //The first four bytes are header bytes and are throw away
  Wire.read();
  Wire.read();
  Wire.read();
  Wire.read();

  for (byte x = 0 ; x < numberOfBytes ; x++)
  {
    shtpData[dataSpot++] = Wire.read();

    if (dataSpot > MAX_PACKET_SIZE) Serial.println("Error: shtp overflow");
  }
}

//Wait a certain time for incoming I2C bytes before giving up
//Returns zero if failed
byte waitForI2C()
{
  byte counter = 1;
  while (Wire.available() == false)
  {
    if (counter++ > 100) //Don't go more than 254
    {
      Serial.println("I2C timeout");
      return (0); //Error
    }
    delay(1);
  }

  return (counter);
}

//Given the data packet, send the header then the data
//TODO - Does Arduino have a max 32 byte send as well? Break sending into multi packets if needed.
void sendPacket(byte channelNumber, int dataLength)
{
  int packetLength = dataLength + 4; //Add four bytes for the header

  Wire.beginTransmission(deviceAddress);

  //Send the 4 byte packet header
  Wire.write(packetLength & 0xFF); //Packet length LSB
  Wire.write(packetLength >> 8); //Packet length MSB
  Wire.write(channelNumber); //Channel number
  Wire.write(sequenceNumber[channelNumber]++); //Send the sequence number, increments with each packet sent, different counter for each channel

  //Send the user's data packet
  for (int i = 0 ; i < dataLength ; i++)
  {
    Wire.write(shtpData[i]);
  }
  if (Wire.endTransmission() != 0)
    Serial.println("Error: Sensor did not ack - sendPacket");
}

//Pull WAKE pin low to indicate we are ready to talk to the sensor
//Wait for sensor to pull INT pin low indicating it's ready for our talking
void startTransfer()
{
  digitalWrite(WAKE, LOW); //Tell sensor we are ready to talk

  int counter = 0;
  while (digitalRead(DATA_AVAILABLE) == HIGH) //Wait for sensor to confirm
  {
    delay(1);
    if (counter++ > 500)
    {
      Serial.println("Sensor never pulled INT low");
      return;
    }
  }

  digitalWrite(WAKE, HIGH); //Release WAKE

  Serial.println("Sensor is ready to talk!");
}
