/*
  Using the [device name]
  By: Nathan Seidle
  SparkFun Electronics
  Date: December 21st, 2017
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/[product ID]

  This example shows how to output the i/j/k/real parts of the rotation vector.
  https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation

  It takes about 1ms at 400kHz I2C to read a record from the sensor, but we are polling the sensor continually
  between updates from the sensor. Use the interrupt pin on the BNO080 breakout to avoid polling.

  Hardware Connections:
  Attach the Qwiic Shield to your Arduino/Photon/ESP32 or other
  Plug the sensor onto the shield
  Serial.print it out at 9600 baud to serial monitor.
*/

#include <Wire.h>

byte deviceAddress = 0x4B; //0x4A if you close the jumper

byte shtpHeader[4]; //Each packet has a header of 4 bytes

#define MAX_PACKET_SIZE 128
byte shtpData[MAX_PACKET_SIZE]; //Packets can be up to 32k but we don't have that much RAM.

//There are 6 com channels. Each channel has its own seqnum
byte sequenceNumber[6] = {0, 0, 0, 0, 0, 0};

const byte CHANNEL_COMMAND = 0;
const byte CHANNEL_EXECUTABLE = 1;
const byte CHANNEL_CONTROL = 2;
const byte CHANNEL_REPORTS = 3;
const byte CHANNEL_WAKE_REPORTS = 4;
const byte CHANNEL_GYRO = 5;

//All the ways we can configure or talk to the BNO080, figure 34, page 36 reference manual
//These are used for low level communication with the sensor
#define SHTP_REPORT_FRS_READ_RESPONSE 0xF3
#define SHTP_REPORT_FRS_READ_REQUEST 0xF4
#define SHTP_REPORT_PRODUCT_ID_RESPONSE 0xF8
#define SHTP_REPORT_PRODUCT_ID_REQUEST 0xF9
#define SHTP_REPORT_BASE_TIMESTAMP 0xFB
#define SHTP_REPORT_SET_FEATURE_COMMAND 0xFD

//All the different sensors and features we can get reports from
//These are used when enabling a given sensor
#define SENSOR_REPORTID_ACCELEROMETER 0x01
#define SENSOR_REPORTID_GYROSCOPE 0x02
#define SENSOR_REPORTID_MAGNETIC_FIELD 0x03
#define SENSOR_REPORTID_LINEAR_ACCELERATION 0x04
#define SENSOR_REPORTID_ROTATION_VECTOR 0x05
#define SENSOR_REPORTID_GRAVITY 0x06
#define SENSOR_REPORTID_VECTOR_REPORTID_GAME_ROTATION 0x08
#define SENSOR_REPORTID_GEOMAGNETIC_ROTATION_VECTOR 0x09
#define SENSOR_REPORTID_TAP_DETECTOR 0x10
#define SENSOR_REPORTID_STEP_COUNTER 0x11
#define SENSOR_REPORTID_STABILITY_CLASSIFIER 0x13

//Record IDs from figure 29, page 29 reference manual
//These are used to read the metadata for each sensor type
#define FRS_RECORDID_ACCELEROMETER 0xE302
#define FRS_RECORDID_ROTATION_VECTOR 0xE30B

//These are defined in the datasheet but can also be obtained by querying the meta data records
//See the read metadata example for more info
uint16_t rotationVector_Q1 = 14;
uint16_t rotationVector_Q2 = 12;
uint16_t rotationVector_Q3 = 13;

void setup()
{
  Serial.begin(9600);

  Wire.begin();
  Wire.setClock(400000); //Increase I2C data rate to 400kHz

  softReset(); //Send command to reset IC

  getProductID();

  enableRotationVector(50); //Send data update every 500ms
}

void loop()
{
  //Look for reports from the enabled sensor
  if (receivePacket() == true)
  {
    //Check to see if this packet is a sensor reporting in
    if (shtpHeader[2] == CHANNEL_REPORTS && shtpData[0] == SHTP_REPORT_BASE_TIMESTAMP) {
      printDataReport();
    }
  }
}

//This function pretty prints data reports by cutting off the header and timestamps
//Unit responds with packet that contains the following
//shtpHeader[0:3]: First, a 4 byte header
//shtpData[0:4]: Then a 5 byte timestamp of microsecond clicks since reading was taken
//shtpData[5 + 0]: Then a feature report ID (0x01 for Accel, 0x05 for Rotation Vector)
//shtpData[5 + 1]: Sequence number (See 6.5.18.2)
//shtpData[5 + 2]: Status
//shtpData[3]: Delay
//shtpData[4:5]: i
//shtpData[6:7]: j
//shtpData[8:9]: k
//shtpData[10:11]: real
//shtpData[12:13]: Accuracy estimate
void printDataReport(void)
{
  if (shtpData[5 + 0] == SENSOR_REPORTID_ROTATION_VECTOR)
  {
    int16_t quatI_raw = (uint16_t)shtpData[5 + 5] << 8 | shtpData[5 + 4];
    int16_t quatJ_raw = (uint16_t)shtpData[5 + 7] << 8 | shtpData[5 + 6];
    int16_t quatK_raw = (uint16_t)shtpData[5 + 9] << 8 | shtpData[5 + 8];
    int16_t quatReal_raw = (uint16_t)shtpData[5 + 11] << 8 | shtpData[5 + 10];
    int16_t accuracy_raw = (uint16_t)shtpData[5 + 13] << 8 | shtpData[5 + 12];

    float quatI = qToFloat(quatI_raw, rotationVector_Q1);
    float quatJ = qToFloat(quatJ_raw, rotationVector_Q1);
    float quatK = qToFloat(quatK_raw, rotationVector_Q1);
    float quatReal = qToFloat(quatReal_raw, rotationVector_Q1);
    float accuracyEstimate = qToFloat(accuracy_raw, rotationVector_Q1);

    Serial.print(quatReal, 2);
    Serial.print(F(","));    
    Serial.print(quatI, 2);
    Serial.print(F(","));
    Serial.print(quatJ, 2);
    Serial.print(F(","));
    Serial.print(quatK, 2);



  }
  else
  {
    Serial.print(F("[Unknown Sensor]"));
  }

  Serial.println();
}

//Send command to reset IC
//Read all advertisement packets from sensor
//The sensor has been seen to reset twice if we attempt too much too quickly.
//This seems to work reliably.
void softReset(void)
{
  shtpData[0] = 1; //Reset

  //Transmit packet on channel 1, 1 byte
  sendPacket(CHANNEL_EXECUTABLE, 1);

  //Read all incoming data and flush it
  delay(50);
  while (receivePacket() == true) ;
  delay(50);
  while (receivePacket() == true) ;
}

//Get the reset cause as well as product ID information
void getProductID()
{
  shtpData[0] = SHTP_REPORT_PRODUCT_ID_REQUEST; //Request the product ID and reset info
  shtpData[1] = 0; //Reserved

  //Transmit packet on channel 2, 2 bytes
  sendPacket(CHANNEL_CONTROL, 2);

  //Now we wait for response
  while (1)
  {
    while (receivePacket() == false) delay(1); //TODO - Remove blocking

    //We have the packet, inspect it for the right contents
    //See page 23. Report ID should be 0xF8
    if (shtpData[0] == SHTP_REPORT_PRODUCT_ID_RESPONSE)
    {/*
      if (shtpData[1] == 1) Serial.println(F("Power on reset"));
      else if (shtpData[1] == 2) Serial.println(F("Internal system reset"));
      else if (shtpData[1] == 3) Serial.println(F("Watchdog timeout"));
      else if (shtpData[1] == 4) Serial.println(F("External reset"));
      else
      {
        Serial.print(F("Reset: "));
        Serial.println(shtpData[1]);
      }
      */
      //printPacket();
      //while(1);
      break; //This packet is one we are looking for
    }
  }
}

//Pretty prints the contents of the current shtp packet
void printPacket(void)
{
  uint16_t packetLength = (uint16_t)shtpHeader[1] << 8 | shtpHeader[0];

  if (packetLength & 1 << 15)
  {
    Serial.println(F("Continued packet"));
    packetLength &= ~(1 << 15);
  }

  Serial.print(F("Packet Length: "));
  Serial.println(packetLength);

  //Attempt to pre-print known events
  if (shtpHeader[2] == CHANNEL_EXECUTABLE && shtpData[0] == 0x01) {
    Serial.println(F("Known: Reset complete"));
    return;
  }
  if (shtpHeader[2] == CHANNEL_CONTROL && shtpData[0] == 0xF1) {
    Serial.println(F("Known: Command response"));
  }
  if (shtpHeader[2] == CHANNEL_COMMAND && shtpData[0] == 0x00) {
    Serial.println(F("Known: Init Advertisement"));
    return;
  }

  if (shtpHeader[2] == CHANNEL_REPORTS && shtpData[0] == 0xFB) {
    Serial.println(F("Known: Data report"));
    printDataReport();
    return;
  }

  Serial.print(F("Channel: "));
  if (shtpHeader[2] == 0) Serial.println(F("Command"));
  else if (shtpHeader[2] == 1) Serial.println(F("Executable"));
  else if (shtpHeader[2] == 2) Serial.println(F("Control"));
  else Serial.println(shtpHeader[2]);
  Serial.print("SeqNum: ");
  Serial.println(shtpHeader[3]);

  int dataLength = packetLength - 4;
  if (dataLength > 10) dataLength = 10; //Arbitrary limiter. We don't want the phonebook.
  for (int i = 0 ; i < dataLength ; i++)
  {
    Serial.print(i);
    Serial.print(F(") 0x"));
    if (shtpData[i] < 0x10) Serial.print("0");
    Serial.print(shtpData[i], HEX);
    Serial.println();
  }
}

