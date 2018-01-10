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

void setup()
{
  Serial.begin(9600);
  Serial.println("Basic I2C Stub");

  //byte result = readRegister(0x3B);

  #define SHTP_HEADER_LENGTH_LSB 0
  #define SHTP_HEADER_LENGTH_MSB 1
  #define SHTP_HEADER_CHANNEL 2
  #define SHTP_HEADER_SEQNUM 3
  byte shtpPacket[21];

  //Each channel has its own seqnum
  byte sequenceNumber_Channel2 = 0;

  //Read the product ID
  shtpPacket[0] = 0x06; //Packet legnth
  shtpPacket[1] = 0; //Packet length MSB
  shtpPacket[2] = 0; //Channel, 2? for Product ID?
  shtpPacket[3] = sequenceNumber_Channel2; //Sequence number
  shtpPacket[4] = 0xF9; //Report ID, this is a configuration report
  shtpPacket[5] = 0; //Reserved

  Wire.beginTransmission(deviceAddress);
  for(int i = 0 ; i < 6 ; i++)
  {
    Wire.write(shtpPacket[i]);
  }

  if (Wire.endTransmission() != 0)
  {
    //Sensor did not ACK
    Serial.println("Error: Sensor did not ack");
  }

  //Ask for Product ID response
  Wire.beginTransmission(deviceAddress);
  Wire.write(0xF8); //Product ID response
  if (Wire.endTransmission(false) != 0) //Send a restart command. Do not release bus.
  {
    //Sensor did not ACK
    Serial.println("Error: Sensor did not ack");
  }

  Wire.requestFrom((uint8_t)deviceAddress, (uint8_t)16);
  int counter = 0;
  Serial.println("Product ID response (should be 16 bytes):");
  if (Wire.available())
  {
    byte response = Wire.read();
    Serial.print(counter);
    Serial.print(") 0x");
    Serial.print(response, HEX);
    Serial.println();

    counter++;
  }

  sequenceNumber_Channel2++;
  
  while(1);

  
  
  shtpPacket[0] = 0x15; //Packet is a total of 21 bytes
  shtpPacket[1] = 0;
  shtpPacket[2] = 2;
  shtpPacket[3] = sequenceNumber_Channel2;

  shtpPacket[4] = 0xFD;
  shtpPacket[5] = 0x01; //Report ID. Listed in SH2 reference manual, page 36
  shtpPacket[6] = 0;
  shtpPacket[7] = 0;
  shtpPacket[8] = 0;
  shtpPacket[9] = 0x60;
  shtpPacket[10] = 0xEA;
  shtpPacket[11] = 0;
  shtpPacket[12] = 0;
  shtpPacket[13] = 0;
  shtpPacket[14] = 0;
  shtpPacket[15] = 0;
  shtpPacket[16] = 0;
  shtpPacket[17] = 0;
  shtpPacket[18] = 0;
  shtpPacket[19] = 0;
  shtpPacket[20] = 0;

  while(1);


}

void loop()
{

}


