

//Check to see if there is any new data available
//Read the contents of the incoming packet into the shtpData array
boolean receivePacketSPI(void)
{
  digitalWrite(SPI_CS, LOW); //Select sensor

  //Get the first four bytes, aka the packet header
  byte packetLSB = SPI.transfer(0xFF);
  byte packetMSB = SPI.transfer(0xFF);
  byte channelNumber = SPI.transfer(0xFF);
  byte sequenceNumber = SPI.transfer(0xFF); //Not sure if we need to store this or not

  digitalWrite(SPI_CS, HIGH); //Deselect sensor

  //Store the header info.
  shtpHeader[0] = packetLSB;
  shtpHeader[1] = packetMSB;
  shtpHeader[2] = channelNumber;
  shtpHeader[3] = sequenceNumber;

  uint16_t dataLength = ((uint16_t)packetMSB << 8 | packetLSB);
  dataLength &= ~(1 << 15); //Clear the MSbit.

  if (dataLength != 0xFF)
  {
    return (true); //We heard something
  }

  return (false); //We're done!
}

//Check to see if there is any new data available
//Read the contents of the incoming packet into the shtpData array
boolean receivePacket(void)
{
  Wire.requestFrom((uint8_t)deviceAddress, (uint8_t)4); //Ask for four bytes to find out how much data we need to read
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
//Arduino I2C read limit is 32 bytes. Header is 4 bytes, so max user can request is 28
void getBytes(byte numberOfBytes)
{
  if (numberOfBytes > 28)
    Serial.println(F("Too many bytes requested!"));

  Wire.requestFrom((uint8_t)deviceAddress, (uint8_t)(numberOfBytes + 4));
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
      Serial.println(F("I2C timeout"));
      return (0); //Error
    }
    delay(1);
  }

  return (counter);
}


