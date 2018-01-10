
//Reads from a given location
//Stores the result at the provided outputPointer
uint8_t readRegister(uint8_t addr)
{
  Wire.beginTransmission(deviceAddress);
  Wire.write(addr);
  if (Wire.endTransmission(false) != 0) //Send a restart command. Do not release bus.
  {
    //Sensor did not ACK
    Serial.println("Error: Sensor did not ack");
  }

  Wire.requestFrom((uint8_t)deviceAddress, (uint8_t)1);
  if (Wire.available())
  {
    return (Wire.read());
  }

  Serial.println("Error: Sensor did not respond");
  return(0);
}

//Reads two consecutive bytes from a given location
//Stores the result at the provided outputPointer
uint16_t readRegister16(uint16_t addr)
{
  Wire.beginTransmission(deviceAddress);
  Wire.write(addr >> 8); //MSB
  Wire.write(addr & 0xFF); //LSB
  if (Wire.endTransmission(false) != 0) //Send a restart command. Do not release bus.
  {
    //Sensor did not ACK
    Serial.println("Error: Sensor did not ack");
  }

  Wire.requestFrom((uint8_t)deviceAddress, (uint8_t)1);
  if (Wire.available())
  {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    return (Wire.read());
  }

  Serial.println("Error: Sensor did not respond");
  return(0);
}

//Write a value to a spot
void writeRegister(uint8_t addr, uint8_t val)
{
  Wire.beginTransmission(deviceAddress);
  Wire.write(addr);
  Wire.write(val);
  if (Wire.endTransmission() != 0)
  {
    //Sensor did not ACK
    Serial.println("Error: Sensor did not ack");
  }
}

//Write two bytes to a spot
void writeRegister16(uint16_t addr, uint16_t val)
{
  Wire.beginTransmission(deviceAddress);
  Wire.write(addr >> 8); //MSB
  Wire.write(addr & 0xFF); //LSB
  Wire.write(val >> 8); //MSB
  Wire.write(val & 0xFF); //LSB
  if (Wire.endTransmission() != 0)
  {
    //Sensor did not ACK
    Serial.println("Error: Sensor did not ack");
  }
}

