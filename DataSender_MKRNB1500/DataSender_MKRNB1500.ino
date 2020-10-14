/*Written by James Curtis Addy

*/

#include <MKRNB.h>
#include <Wire.h>
#include "Base64Encode.h"
#include <string.h>

NBClient client;
NB nbAccess;

#define DATA_SIZE 7208 //7200 for data + 8 for timestamp
#define I2C_ID 0
#define SEND_TIMEOUT 60000 //amount of time to attempt to send data

///Change this stuff///////////////
String server_file_name = "CloseTest";
//////////////////

volatile byte data_array[DATA_SIZE];
volatile size_t data_index = 8; //The first 8 bytes are reserved for the timestamp
char server[] = "mbrace.xyz";
const char* server_directory = "OceanData";
int port = 80;

//volatile bool data_received;
//volatile unsigned int receive_count = 0;

void setup()
{
  Wire.begin(I2C_ID);
  Wire.onReceive(receive_data_event);
//  Serial.begin(9600);
//  while (!Serial) {}
  client.set_connect_timeout(5000);
  client.set_write_timeout(5000);
  nbAccess.setTimeout(5000);
  //These are dilimiters for the timestamp
  data_array[0] = '{';
  data_array[1] = '{';
  data_array[6] = '}';
  data_array[7] = '}';
}

void loop()
{
//  if (data_received)
//  {
//    Serial.print(receive_count);
//    if ((receive_count) % 20 == 0) Serial.println();
//    else Serial.print(F(" "));
//    data_received = false;
//  }
  if (data_index >= DATA_SIZE)
  {
//    receive_count = 0;
    add_timestamp();
    int encoded_length = base64_encoded_length(DATA_SIZE);
    char encoded_data[encoded_length];
    base64_encode(encoded_data, (char*)data_array, DATA_SIZE);
    data_index = 8; //Once the data is encoded the original data buffer is free to use.
    unsigned long start_millis = millis();
//    Serial.println();
    //Keep trying to connect the modem if it's not ready.
    do {
//      Serial.println("Starting up modem.");
      nbAccess.begin(NULL, false, true);
    } while ((nbAccess.status() != NB_READY) && !get_is_timed_out(start_millis, SEND_TIMEOUT));
    if (nbAccess.status() == NB_READY)
    {
//      Serial.println("NB modem connected!");
      if (send_data_to_server(encoded_data, encoded_length, start_millis))
      {
//        Serial.println("Send successful!");
      }
      else
      {
//        Serial.println("Send failed.");
      }
    }
//    else Serial.println("Modem connection failed.");
    nbAccess.secureShutdown();
  }
}

bool get_is_timed_out(unsigned long start_millis, unsigned int timeout)
{
  return (millis() - start_millis) >= timeout;
}

bool send_data_to_server(char* encoded_data, int encoded_length, unsigned long start_millis)
{
  while (!get_is_timed_out(start_millis, SEND_TIMEOUT))
  {
    while (client.available()) client.read(); //Clear read buffer
//    Serial.println(F("Connecting to server"));
    if (client.connect(server, port))
    {      
//      Serial.println(F("Server connected! Publishing data"));
      if (publish_post(encoded_data, encoded_length))
      {
//        Serial.println(F("Publish successful. Waiting for response."));
        bool good_response = get_server_response();
        if (good_response)
        {
//          Serial.println(F("Good server response!"));
          return true;
        }
        else
        {
//          Serial.println(F("Bad response. Resending data."));
          delay(1000);
        }         
      }
      else 
      {
//        Serial.println(F("Publish failed"));
        return false;
      }
    }
    else
    {
//      Serial.println(F("Unable to connect to the server."));
      return false;
    }
  }
//  Serial.print(F("No successful send after "));
//  Serial.print(SEND_TIMEOUT);
//  Serial.println(F(" milliseconds."));
  return false;
}

bool get_server_response()
{
  unsigned int response_timeout = 10000;
  unsigned long start_millis = millis();
  while ((millis() - start_millis) < response_timeout && !client.available()) {}
  if (client.available())
  {
    char response[256] = "";
    for (int i = 0; i < 256; i++)
    {
      if (!client.available()) break;
      response[i] = client.read();
    }
//    Serial.println(F("Response from server:"));
//    Serial.println(response);
    char* good_response = strstr(response, "200 OK");
    if (good_response) return true;
  }
  else
  {
//    Serial.print(response_timeout);
//    Serial.println(F(" ms passed without a response"));
  }
  return false;
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
  while (Wire.available())
  {
    if (data_index >= DATA_SIZE) return; //Don't write if the buffer is full
    byte received_byte = Wire.read();
    data_array[data_index++] = received_byte;
  }
//  data_received = true;
//  receive_count++;
}

bool valid_send(size_t size_from_send)
{
//  Serial.print("Send size: ");
//  Serial.println(size_from_send);
  return size_from_send > 0;
}

bool publish_post(char* encoded_data, int encoded_length)
{
  client.beginWrite(true);
  //JSON structure:
  //{"n":"NAME", "d":"DATA"}
  // [(4 quotations/parameter + 1 colon/parameter) x #parameters] + 2 brackets + [(#parameters - 1) x ( 1 comma + 1 space)] = (5x2) + 2 + 2 = 14 characters
  // 14 characters + 2 for 'n' and 'd' = 16 total characters
  int content_length = 16 + server_file_name.length() + encoded_length;
  bool good_publish = valid_send(client.print(F("POST /"))) &&
                      valid_send(client.print(server_directory)) &&
                      valid_send(client.println(F("/index.php HTTP/1.1"))) &&
                      valid_send(client.println(F("HOST: mbrace.xyz"))) &&
                      valid_send(client.println(F("Content-Type: application/json"))) &&
                      valid_send(client.println(F("Connection: close"))) &&
                      valid_send(client.print(F("Content-Length: "))) &&
                      valid_send(client.println(content_length)) &&
                      valid_send(client.println()) &&
                      valid_send(client.print(F("{\"n\":\""))) &&
                      valid_send(client.print(server_file_name)) &&
                      valid_send(client.print(F("\", \"d\":\""))) &&
                      valid_send(client.print(encoded_data)) &&
                      valid_send(client.println(F("\"}")));
  client.endWrite();
  return good_publish;
}
