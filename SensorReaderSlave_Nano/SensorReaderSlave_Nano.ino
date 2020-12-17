/* Written by James Curtis Addy 
 *  
 *  This code makes the Nano behave as an i2c slave that
 *  collects readings on request and sends them to the
 *  master device.
*/

#include <Wire.h>

#define DATA_SIZE 12
#define I2C_ID 1

byte readings[DATA_SIZE];

void setup() {
  Wire.begin(I2C_ID);
  Wire.onRequest(read_and_send_on_request);
  DDRD = DDRD | 0xFC;    //Declare D2 to D7 as OUTPUTS
  PORTD = PORTD | 0xFC;  //Set D2 to D7 HIGH
  pinMode(10, OUTPUT);
}

void loop() {  
}

void read_and_send_on_request() {
  readings[0] = get_MSB(analogRead(A0));
  readings[1] = get_LSB(analogRead(A0));
  
  readings[2] = get_MSB(analogRead(A1));
  readings[3] = get_LSB(analogRead(A1));
  
  readings[4] = get_MSB(analogRead(A2));
  readings[5] = get_LSB(analogRead(A2));
  
  readings[6] = get_MSB(analogRead(A3));
  readings[7] = get_LSB(analogRead(A3));
  
  readings[8] = get_MSB(analogRead(A6));
  readings[9] = get_LSB(analogRead(A6));
  
  readings[10] = get_MSB(analogRead(A7));
  readings[11] = get_LSB(analogRead(A7));
  
  Wire.write(readings, DATA_SIZE); 
}

byte get_MSB(int value)
{
  return (value >> 8) & 255;
}

byte get_LSB(int value)
{
  return value & 255;
}
