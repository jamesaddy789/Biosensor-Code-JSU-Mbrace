/* Written by James Curtis Addy 
 *  
 *  This code makes the Nano behave as an i2c slave that
 *  collects readings on request and sends them to the
 *  master device.
*/

#include <Wire.h>

#define SENSOR_NUMBER 6
#define I2C_ID 0

byte readings[SENSOR_NUMBER];

void setup() {
  Wire.begin(I2C_ID);
  Wire.onRequest(read_and_send_on_request);
}

void loop() {  
}

void read_and_send_on_request() {
  readings[0] = scale_analog_value(analogRead(A0));
  readings[1] = scale_analog_value(analogRead(A1));
  readings[2] = scale_analog_value(analogRead(A2));
  readings[3] = scale_analog_value(analogRead(A3));
  readings[4] = scale_analog_value(analogRead(A6));
  readings[5] = scale_analog_value(analogRead(A7));
  Wire.write(readings, SENSOR_NUMBER); 
}

byte scale_analog_value(int value)
{
  //floor(1023 / 4) = 255
  return value / 4.0;
}
