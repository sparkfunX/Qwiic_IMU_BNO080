
//Given a register value and a Q point, convert to float
//See https://en.wikipedia.org/wiki/Q_(number_format)
float qToFloat(int16_t fixedPointValue, uint8_t qPoint)
{
  float qFloat = fixedPointValue;
  qFloat *= pow(2, qPoint * -1);
  return (qFloat);
}

//Sends the packet to enable the accelerometer
void enableAccelerometer(int timeBetweenReports)
{
  setFeatureCommand(SENSOR_REPORTID_ACCELEROMETER, timeBetweenReports);
}

//Sends the packet to enable the rotation vector
void enableRotationVector(int timeBetweenReports)
{
  setFeatureCommand(SENSOR_REPORTID_ROTATION_VECTOR, timeBetweenReports);
}

//Given a sensor's report ID, this tells the BNO080 to begin reporting the values
void setFeatureCommand(byte reportID, int timeBetweenReports)
{
  long microsBetweenReports = (long)timeBetweenReports * 1000L;

  shtpData[0] = SHTP_REPORT_SET_FEATURE_COMMAND; //Set feature command. Reference page 55
  shtpData[1] = reportID; //Feature Report ID. 0x01 = Accelerometer, 0x05 = Rotation vector
  shtpData[2] = 0; //Feature flags
  shtpData[3] = 0; //Change sensitivity (LSB)
  shtpData[4] = 0; //Change sensitivity (MSB)
  shtpData[5] = (microsBetweenReports >> 0) & 0xFF; //Report interval (LSB) in microseconds. 0x7A120 = 500ms
  shtpData[6] = (microsBetweenReports >> 8) & 0xFF; //Report interval
  shtpData[7] = (microsBetweenReports >> 16) & 0xFF; //Report interval
  shtpData[8] = (microsBetweenReports >> 24) & 0xFF; //Report interval (MSB)
  shtpData[9] = 0; //Batch Interval (LSB)
  shtpData[10] = 0; //Batch Interval
  shtpData[11] = 0; //Batch Interval
  shtpData[12] = 0; //Batch Interval (MSB)
  shtpData[13] = 0; //Sensor-specific config (LSB)
  shtpData[14] = 0; //Sensor-specific config
  shtpData[15] = 0; //Sensor-specific config
  shtpData[16] = 0; //Sensor-specific config (MSB)

  //Transmit packet on channel 2, 17 bytes
  sendPacket(CHANNEL_CONTROL, 17);
}

//Check to see if there is any new data available
//Read the contents of the incoming packet into the shtpData array
boolean receivePacket(void)
{
  Wire.requestFrom((uint8_t)deviceAddress, (uint8_t)4); //Ask for four bytes to find out how much data we need to read
  if (waitForI2C() == false) return (false); //Error

  //Serial.println("Getting packet");

  //Get the first four bytes, aka the packet header
  uint8_t packetLSB = Wire.read();
  uint8_t packetMSB = Wire.read();
  uint8_t channelNumber = Wire.read();
  uint8_t sequenceNumber = Wire.read(); //Not sure if we need to store this or not

  //Store the header info.
  shtpHeader[0] = packetLSB;
  shtpHeader[1] = packetMSB;
  shtpHeader[2] = channelNumber;
  shtpHeader[3] = sequenceNumber;

  //Calculate the number of data bytes in this packet
  int16_t dataLength = ((uint16_t)packetMSB << 8 | packetLSB);
  dataLength &= ~(1 << 15); //Clear the MSbit.
  //This bit indicates if this package is a continuation of the last. Ignore it for now.
  //TODO catch this as an error and exit
  if (dataLength == 0)
  {
    //Packet is empty
    return (false); //All done
  }
  dataLength -= 4; //Remove the header bytes from the data count

  getDataFromSensor(dataLength);

  return (true); //We're done!
}

//Sends multiple requests to sensor until all data bytes are received from sensor
//The shtpData buffer has max capacity of MAX_PACKET_SIZE. Any bytes over this amount will be lost.
//Arduino I2C read limit is 32 bytes. Header is 4 bytes, so max data we can read per interation is 28 bytes
boolean getDataFromSensor(uint16_t bytesRemaining)
{
  uint16_t dataSpot = 0; //Start at the beginning of shtpData array

  //Setup a series of chunked 32 byte reads
  while (bytesRemaining > 0)
  {
    uint16_t numberOfBytesToRead = bytesRemaining;
    if (numberOfBytesToRead > 28) numberOfBytesToRead = 28;

    Wire.requestFrom((uint8_t)deviceAddress, (uint8_t)(numberOfBytesToRead + 4));
    if (waitForI2C() == false) return (0); //Error

    //The first four bytes are header bytes and are throw away
    Wire.read();
    Wire.read();
    Wire.read();
    Wire.read();

    for (uint8_t x = 0 ; x < numberOfBytesToRead ; x++)
    {
      uint8_t incoming = Wire.read();
      if (dataSpot < MAX_PACKET_SIZE)
      {
        shtpData[dataSpot++] = incoming; //Store data into the shtpData array
      }
      else
      {
        //Do nothing with the data
      }
    }

    bytesRemaining -= numberOfBytesToRead;
  }
  return (true); //Done!
}

//Wait a certain time for incoming I2C bytes before giving up
//Returns false if failed
boolean waitForI2C()
{
  for (uint8_t counter = 0 ; counter < 100 ; counter++) //Don't got more than 255
  {
    if (Wire.available() > 0) return (true);
    delay(1);
  }

  //Serial.println(F("I2C timeout"));
  return (false);
}

//Given the data packet, send the header then the data
//TODO - Arduino has a max 32 byte send. Break sending into multi packets if needed.
boolean sendPacket(uint8_t channelNumber, uint8_t dataLength)
{
  uint8_t packetLength = dataLength + 4; //Add four bytes for the header
  //if(packetLength > 32) return(false); //Arduino can't transmit more than 32 bytes

  Wire.beginTransmission(deviceAddress);

  //Send the 4 byte packet header
  Wire.write(packetLength & 0xFF); //Packet length LSB
  Wire.write(packetLength >> 8); //Packet length MSB
  Wire.write(channelNumber); //Channel number
  Wire.write(sequenceNumber[channelNumber]++); //Send the sequence number, increments with each packet sent, different counter for each channel

  //Send the user's data packet
  for (uint8_t i = 0 ; i < dataLength ; i++)
  {
    Wire.write(shtpData[i]);
  }
  if (Wire.endTransmission() != 0)
  {
    //Serial.println(F("Error: Sensor did not ack - sendPacket"));
    return (false);
  }
  return (true);
}

