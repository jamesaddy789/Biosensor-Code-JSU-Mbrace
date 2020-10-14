/* Written by James Curtis Addy 
*/
#include <Wire.h>

#define DATA_SIZE 30 //Wire buffer size is 32
#define SAMPLE_INTERVAL 100
#define DEFAULT_ANALOG_MAX 1023
#define TARGET_MAX 100 
#define I2C_ID 0

unsigned long start_time = 0;
unsigned int reading_index = 0;
byte readings[DATA_SIZE];

void setup() {
  Serial.begin(9600);
  Wire.begin();
  //delay(30000);
  start_time = millis();
}

void loop() {  
  if((millis() - start_time) >= SAMPLE_INTERVAL)
  {
    start_time = millis();      
    readings[reading_index++] = scale_analog_value(analogRead(A0));
    readings[reading_index++] = scale_analog_value(analogRead(A1));
    readings[reading_index++] = scale_analog_value(analogRead(A2));
    readings[reading_index++] = scale_analog_value(analogRead(A3));
    readings[reading_index++] = scale_analog_value(analogRead(A6));
    readings[reading_index++] = scale_analog_value(analogRead(A7));  
    if(reading_index >= DATA_SIZE)
    {
      reading_index = 0;
      Wire.beginTransmission(I2C_ID);     
      Wire.write(readings, DATA_SIZE);
      Serial.println("Calling Wire.endTransmission())");
      Wire.endTransmission();  
      Serial.println("Completed transmission!");
    }
  }
}

byte scale_analog_value(int value)
{
  float ratio = (float)value/(float)DEFAULT_ANALOG_MAX;
  return ratio * TARGET_MAX;
}
