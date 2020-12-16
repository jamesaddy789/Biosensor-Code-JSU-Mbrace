/* Written by James Curtis Addy
 * 
 * This code makes an Esp8266 act as an i2c master that will
 * request readings 10 times per second from the slave device. The 
 * readings will be saved to an SD card and sent over the internet 
 * using WiFi when the data buffer is full.
 */

#include <Wire.h>
#include <ESP8266WiFi.h>
#include "Base64Encode.h"
#include <SD.h>

#define DATA_SIZE 68 //60 bytes for readings and 8 for timestamp {{TTTT}}
const unsigned int BYTES_PER_DAY = DATA_SIZE * 3600 * 24; //Day's worth of bytes
#define I2C_ID 1
#define NUMBER_OF_SENSORS 6

//Change this stuff////
const char* ssid = "jsumobilenet";//network name 
const char* wifi_password =  ""; //password
const char* experiment_start_date = "2020-11-30"; //This date will be used for the SD file name
const char* server_directory = "GCRL_1130"; 
const char* sd_folder_name = "/GCRL_1130"; //Must start with "/" 
///////////////////////

WiFiClient client;
volatile byte data_array[DATA_SIZE];
volatile size_t data_index = 8; //Start after the timestamp
char server[] = "mbrace.xyz";
String file_name; //This will be the mac address acquired from WiFi.macAddress()

int port = 80;

//SD
unsigned int sd_file_counter = 0;
String sd_file_path_string;
String sd_file_suffix;
File sd_file;
unsigned long bytes_written = 0;

unsigned long get_readings_time = 0;

uint8_t reconnect_count = 0;

void setup()
{
  Serial.begin(9600);
  Wire.begin();

  client.setTimeout(1);
  while (!check_connection())
  {
    delay(5000);
  }
  
  establish_server_connection();
  data_array[0] = '{';
  data_array[1] = '{';
  data_array[6] = '}';
  data_array[7] = '}';

  file_name = String(WiFi.macAddress());
  file_name.replace(":", "_");
  
  //Begin SD object (mount to SD card)
  while (!SD.begin(5)) {}
  Serial.println(F("SD Card Mount Succeeded"));
  initialize_sd_file();
}

void loop()
{
  if (data_index >= DATA_SIZE)
  {
    add_timestamp();
    update_sd_file();

    int encoded_length = base64_encoded_length(DATA_SIZE);
    char encoded_data[encoded_length];
    base64_encode(encoded_data, (char*)data_array, DATA_SIZE);
    data_index = 8; //Once the data is encoded the original data buffer is free to use.
    if(reconnect_count == 0 || WiFi.status() == WL_CONNECTED)
    {
      if (check_connection())
      {
        publish_post(encoded_data, encoded_length);
      }
      else
      {
        reconnect_count = 4;//Try again in 5 seconds.
      }
    }
    else
    {
      reconnect_count--;  
    }
  }

  if((millis() - get_readings_time) >= 100)
  {
    get_readings_time = millis();
    request_readings();
  }
}

bool request_readings()
{   
    if(data_index >= DATA_SIZE) return false; //Don't read if the buffer is full
    Wire.requestFrom(I2C_ID, NUMBER_OF_SENSORS);  
   
    while (Wire.available())
    { 
      for (int i = 0; i < NUMBER_OF_SENSORS; i++)
      {
        data_array[data_index] = Wire.read();
        data_index++;
      }
    }  
}

bool check_connection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(F("Connecting to network: "));
    Serial.println(ssid);
    WiFi.begin(ssid, wifi_password);
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    if (establish_server_connection())
    {      
      return true;
    }
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

bool establish_server_connection()
{
  /*Clear read buffer because connected() returns true
    even with no server connection if there is still data
    in the read buffer*/
  while (client.available())
  {
    client.read();
  }
  client.flush(); //Clears the client buffer
  
  if (!client.connected())
  {
    if (client.connect(server, port))
    {
      return true;
    }
    else
    {
      Serial.println(F("Unable to connect to server"));
      return false;
    }
  }
  return true;
}

bool valid_send(size_t size_from_send)
{
  return size_from_send > 0;
}

bool publish_post(char* encoded_data, int encoded_length)
{
  //JSON structure:
  //{"n":"NAME", "d":"DATA"}
  // [(4 quotations/parameter + 1 colon/parameter) x #parameters] + 2 brackets + [(#parameters - 1) x ( 1 comma + 1 space)] = (5x2) + 2 + 2 = 14 characters
  // 14 characters + 2 for 'n' and 'd' = 16 total characters
  int content_length = 16 + file_name.length() + encoded_length;
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
                      valid_send(client.print(file_name)) &&
                      valid_send(client.print(F("\", \"d\":\""))) &&
                      valid_send(client.print(encoded_data)) &&
                      valid_send(client.println(F("\"}")));
  return good_publish;
}

//SD functions
unsigned int get_file_count(const char* folder_path)
{
  File root = SD.open(folder_path);
  if (!root)
  {
    Serial.print(F("Failed to open "));
    Serial.println(folder_path);
    return 0;
  }
  if (!root.isDirectory())
  {
    Serial.print(folder_path);
    Serial.println(F(" is not a directory"));
    return 0;
  }
  int count = 0;
  File current_file = root.openNextFile();

  while (current_file)
  {
    if (!current_file.isDirectory())
    {
      count++;
    }
    current_file.close();
    current_file = root.openNextFile();    
  }
  return count;
}

void update_sd_file()
{
  if (!sd_file)
  {
    //Initialize file
    update_sd_file_path_string();
    sd_file = SD.open(sd_file_path_string.c_str(), FILE_WRITE);
  }

  sd_file.write((uint8_t*)data_array, DATA_SIZE);
  sd_file.flush();
  bytes_written += DATA_SIZE;
  
  //Create a new file when bytes written is a day's amount.
  if (bytes_written >= BYTES_PER_DAY)
  {
    bytes_written = 0;
    sd_file_counter += 1;
    sd_file.close();
    sd_file_suffix = String(sd_file_counter);
    update_sd_file_path_string();
    sd_file = SD.open(sd_file_path_string.c_str(), FILE_WRITE);
  }
}

void initialize_sd_file()
{
  //Create SD folder if it doesn't exist on the card
  if (!SD.exists(sd_folder_name))
  {
    SD.mkdir(sd_folder_name);
  }
  //Set counter to the most recent file in the folder.
  sd_file_counter = get_file_count(sd_folder_name) + 1;
  if (sd_file_counter != 1)
  {
    //If there are already files in the folder, this must be a reset.
    sd_file_suffix = String(String(sd_file_counter) + "_Reset");
  }
  else
  {
    //Otherwise, start the count at 1.
    sd_file_suffix = String(sd_file_counter);
  }
  bytes_written = 0;
}

void update_sd_file_path_string()
{
  //The mac address is already acquired in the server file name.
  sd_file_path_string = String(String(sd_folder_name) + "/" + file_name + "__" + String(experiment_start_date) + "_"  + sd_file_suffix + ".bin");
}
