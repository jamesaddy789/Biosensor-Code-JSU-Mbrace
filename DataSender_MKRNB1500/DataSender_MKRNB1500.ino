/*Written by James Curtis Addy

*/

#include <MKRNB.h>
#include <Wire.h>
#include "Base64Encode.h"

NBClient client;
GPRS gprs;
NB nbAccess;

#define DATA_SIZE 7208 //7200 for data + 8 for timestamp
#define I2C_ID 0

///Change this stuff///////////////
String server_file_name = "I2CTest1";
//////////////////

volatile byte data_array[DATA_SIZE];
volatile size_t data_index = 8; //The first 8 bytes are reserved for the timestamp
char server[] = "mbrace.xyz";
const char* server_directory = "OceanData";
int port = 80;

void setup()
{
  Wire.begin(I2C_ID);
  Wire.onReceive(receive_data_event);
  //Serial.begin(9600);
  connect_nb();
  attach_gprs();
  establish_server_connection();
  //These are dilimiters for the timestamp
  data_array[0] = '{';
  data_array[1] = '{';
  data_array[6] = '}';
  data_array[7] = '}';
}

void loop()
{
  if (data_index >= DATA_SIZE)
  {
    add_timestamp();
    int encoded_length = base64_encoded_length(DATA_SIZE);
    char encoded_data[encoded_length];
    base64_encode(encoded_data, (char*)data_array, DATA_SIZE);
    data_index = 8; //Once the data is encoded the original data buffer is free to use.
    if(check_connection())
    {
        publish_post(encoded_data, encoded_length);
    }    
  }
}

void add_timestamp()
{
  unsigned long current_millis = millis();
  data_array[2] = (current_millis >> 24) & 255;
  data_array[3] = (current_millis >> 16) & 255;
  data_array[4] = (current_millis >> 8) & 255;
  data_array[5] = current_millis & 255;
}

void receive_data_event(int amount_received)
{
   while(Wire.available())
   {
      if(data_index >= DATA_SIZE) return; //Don't write if the buffer is full
      byte received_byte = Wire.read();
      data_array[data_index++] = received_byte;
   }
}

bool check_connection()
{
  if (nbAccess.status() != NB_READY)
  {
    //Serial.println(F("Not connected to NB Modem"));
    connect_nb();
  }
  if (gprs.status() != GPRS_READY)
  {
    //Serial.println(F("Not attached with GPRS"));
    attach_gprs();
  }
  return establish_server_connection() && nbAccess.status() == NB_READY && gprs.status() == GPRS_READY;
}

bool establish_server_connection()
{
  client.flush(); //Clears the write buffer
  /*Clear read buffer because connected() returns true
    even with no server connection if there is still data
    in the read buffer*/
  while (client.available())
  {
    char received_byte = (char)client.read();
    //Serial.print(received_byte);
  }
  bool is_connected = client.connected();
  if (!is_connected)
  {
    is_connected = client.connect(server, port);
  }
  return is_connected;
}

void attach_gprs()
{
  while (gprs.attachGPRS() != GPRS_READY)
  {
    delay(1000);
  }
}

void connect_nb()
{
  while (nbAccess.begin() != NB_READY)
  {
    //Serial.println(F("Failed to connect to NB modem"));
    delay(1000);
  }
  //Serial.println(F("Connected to NB modem"));
}

void publish_post(char* encoded_data, int encoded_length)
{
  client.print(F("POST /"));
  client.print(server_directory);
  client.println(F("/index.php HTTP/1.1"));
  client.println(F("HOST: mbrace.xyz"));
  client.println(F("Content-Type: application/json"));
  //JSON structure:
  //{"n":"NAME", "d":"DATA"}
  // [(4 quotations/parameter + 1 colon/parameter) x #parameters] + 2 brackets + [(#parameters - 1) x ( 1 comma + 1 space)] = (5x2) + 2 + 2 = 14 characters
  // 14 characters + 2 for 'n' and 'd' = 16 total characters
  int content_length = 16 + server_file_name.length() + encoded_length;
  client.print(F("Content-Length: "));
  client.println(content_length);
  client.println();
  client.print(F("{\"n\":\""));
  client.print(server_file_name);
  client.print(F("\", \"d\":\""));
  client.print(encoded_data);
  client.println(F("\"}"));
}
