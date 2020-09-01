/* Written by James Curtis Addy 
*/
#include <Wire.h>

#define DATA_SIZE 30 //Wire buffer size is 32
#define SAMPLE_INTERVAL 100
#define DEFAULT_ANALOG_MAX 1023
#define TARGET_MAX 100 
#define SLAVE_ID 0

unsigned long start_time = 0;
unsigned int reading_index = 0;
byte readings[DATA_SIZE];

void setup() {
  Serial.begin(9600);
  Wire.begin();
  start_time = millis();
}

void loop() {  
  if((millis() - start_time) >= SAMPLE_INTERVAL)
  {
    start_time = millis();   
    int analog_values[6];   
    analog_values[0] = scale_analog_value(analogRead(A0));
    analog_values[1] = scale_analog_value(analogRead(A1));
    analog_values[2] = scale_analog_value(analogRead(A2));
    analog_values[3] = scale_analog_value(analogRead(A3));
    analog_values[4] = scale_analog_value(analogRead(A6));
    analog_values[5] = scale_analog_value(analogRead(A7));
    for(int i = 0; i < 6; i++)
    {
//      Serial.print(analog_values[i]);
//      if(i < 5) Serial.print(",");
      readings[reading_index++] = analog_values[i];
    }
    Serial.println();
    if(reading_index == DATA_SIZE)
    {
      reading_index = 0;
      Wire.beginTransmission(SLAVE_ID);     
      Wire.write(readings, DATA_SIZE);
      Wire.endTransmission();  
    }
  }
}

byte scale_analog_value(int value)
{
  float ratio = (float)value/(float)DEFAULT_ANALOG_MAX;
  return ratio * TARGET_MAX;
}
