/* Written by James Curtis Addy
 * 
 * This code makes an Esp32 or Esp8266 act as an i2c master that will
 * request readings 10 times per second from the slave device. The 
 * readings will be saved to an SD card and sent over the internet 
 * using WiFi when the data buffer is full. WPA2 Enterprise connection
 * is supported.
 */

#include <Wire.h>
#include "esp_wpa2.h" //wpa2 library for connections to Enterprise networks
#include <WiFi.h>
#include <WiFiClient.h>
#include "Base64Encode.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <string.h>

#define DATA_SIZE 68 //60 bytes for readings and 8 for timestamp {TTTT}
const unsigned int BYTES_PER_DAY = DATA_SIZE * 3600 * 24; //Day's worth of bytes
#define I2C_ID 1
#define NUMBER_OF_SENSORS 6

//Change this stuff////
const char* ssid = "jsumobilenet";//network name 
const char* wifi_username = "";//For WPA2-Enterprise connections only. Leave as empty string if not WPA2
const char* wifi_password =  ""; //password
String server_file_name; //This will be the mac address acquired from WiFi.macAddress()
const char* experiment_start_date = "2020-11-30"; //This date will be used for the SD file name
///////////////////////

WiFiClient client;
volatile byte data_array[DATA_SIZE];
volatile size_t data_index = 8; //Start after the timestamp
char server[] = "mbrace.xyz";
const char* server_directory = "GCRL_1130"; 
int port = 80;

//SD
const char* sd_folder_name = "/GCRL_1130";
unsigned int sd_file_counter = 0;
String sd_file_path_string;
String sd_file_suffix;
File sd_file;

hw_timer_t* timer = NULL;
volatile bool timer_interrupt_flagged = false;

void setup()
{
  Serial.begin(9600);
  Wire.begin();
  unsigned long start_millis = millis();
  while (WiFi.status() != WL_CONNECTED && !get_is_timed_out(start_millis, 30000))
  {
    establish_wifi_connection();
    delay(1000);
  }
  
  Serial.println("WiFi connected!");
  client.setTimeout(1);
  establish_server_connection();
  data_array[0] = '{';
  data_array[1] = '{';
  data_array[6] = '}';
  data_array[7] = '}';

  server_file_name = String(WiFi.macAddress());
  server_file_name.replace(":", "_");
  
  //Begin SD object (mount to SD card)
  while (!SD.begin(5)) {}
  Serial.println(F("SD Card Mount Succeeded"));
  initialize_sd_file();

  //Set timer to 10HZ
  timer = timerBegin(0, 8000, true); //80MZ / 8000 = 10000HZ
  timerAttachInterrupt(timer, &flag_timer_interrupt, true);
  timerAlarmWrite(timer, 1000, true); //10000HZ / 1000 = 10HZ
  timerAlarmEnable(timer);
}

void loop()
{
  if (data_index >= DATA_SIZE)
  {
    add_timestamp();
    update_sd_file(SD);
    yield();
    int encoded_length = base64_encoded_length(DATA_SIZE);
    char encoded_data[encoded_length];
    base64_encode(encoded_data, (char*)data_array, DATA_SIZE);
    data_index = 8; //Once the data is encoded the original data buffer is free to use.
    if (check_connection())
    {
      Serial.println("Ready to publish data");
      yield();
      publish_post(encoded_data, encoded_length);
    }
  }

  if(timer_interrupt_flagged)
  {
    request_readings();
    timer_interrupt_flagged = false; 
  }
}

void flag_timer_interrupt()
{
  timer_interrupt_flagged = true;
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

bool get_is_timed_out(unsigned long start_millis, unsigned int timeout)
{
  return (millis() - start_millis) >= timeout;
}

bool check_connection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    establish_wifi_connection();
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi status is connected");
    if (establish_server_connection())
    {      
      Serial.println("Connected to server");
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

void establish_wifi_connection()
{
  Serial.print(F("Connecting to network: "));
  Serial.println(ssid);
  
  if (String(wifi_username) != "")
  {
    Serial.println("Enterprise connection");
    //Enterprise setup
    WiFi.mode(WIFI_STA); //init wifi mode
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)wifi_username, strlen(wifi_username)); //provide identity
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)wifi_username, strlen(wifi_username)); //provide username --> identity and username is same
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)wifi_password, strlen(wifi_password)); //provide password
    esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT(); //set config settings to default
    esp_wifi_sta_wpa2_ent_enable(&config); //set config settings to enable function
  }
  WiFi.begin(ssid, wifi_password);
}

bool establish_server_connection()
{
  yield();
  while (client.available())
  {
    //Serial.print((char)client.read());
    client.read();
    //Maybe a yield here
    //yield()
  }
  client.flush(); //Clears the client buffer
  yield();
  /*Clear read buffer because connected() returns true
    even with no server connection if there is still data
    in the read buffer*/
  if (!client.connected())
  {
    Serial.println(F("Connecting to server"));
    if (client.connect(server, port))
    {
      Serial.println(F("Connected to server"));
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

//void publish_post(char* encoded_data, int encoded_length)
//{
//  client.print(F("POST /"));
//  client.print(server_directory);
//  client.println(F("/index.php HTTP/1.1"));
//  client.println(F("HOST: mbrace.xyz"));
//  client.println(F("Content-Type: application/json"));
//  //JSON structure:
//  //{"n":"NAME", "d":"DATA"}
//  // [(4 quotations/parameter + 1 colon/parameter) x #parameters] + 2 brackets + [(#parameters - 1) x ( 1 comma + 1 space)] = (5x2) + 2 + 2 = 14 characters
//  // 14 characters + 2 for 'n' and 'd' = 16 total characters
//  int content_length = 16 + server_file_name.length() + encoded_length;
//  client.print(F("Content-Length: "));
//  client.println(content_length);
//  client.println();
//  client.print(F("{\"n\":\""));
//  client.print(server_file_name);
//  client.print(F("\", \"d\":\""));
//  client.print(encoded_data);
//  client.println(F("\"}"));
//}

bool valid_send(size_t size_from_send)
{
//  Serial.print("Send size: ");
//  Serial.println(size_from_send);
  return size_from_send > 0;
}

bool publish_post(char* encoded_data, int encoded_length)
{
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
  return good_publish;
}

//SD functions
unsigned int get_file_count(fs::FS &fs, const char* folder_path)
{
  File root = fs.open(folder_path);
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

void update_sd_file(fs::FS &fs)
{
  if (!sd_file)
  {
    //Initialize file
    update_sd_file_path_string();
    sd_file = SD.open(sd_file_path_string.c_str(), FILE_APPEND);
  }

  sd_file.write((uint8_t*)data_array, DATA_SIZE);
  sd_file.flush();
  Serial.print(sd_file.name());
  Serial.print(F("'s file size: "));
  Serial.println(sd_file.size());
  Serial.print(F("The file pointer position is: "));
  Serial.println(sd_file.position());
  
  //Create a new file when current file is at the limit
  if (sd_file.size() >= BYTES_PER_DAY)
  {
    sd_file_counter += 1;
    sd_file.close();
    sd_file_suffix = String(sd_file_counter);
    update_sd_file_path_string();
    sd_file = SD.open(sd_file_path_string.c_str(), FILE_APPEND);
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
  sd_file_counter = get_file_count(SD, sd_folder_name) + 1;
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
}

void update_sd_file_path_string()
{
  //The mac address is already acquired in the server file name.
  sd_file_path_string = String(String(sd_folder_name) + "/" + server_file_name + "__" + String(experiment_start_date) + "_"  + sd_file_suffix + ".bin");
  Serial.println(sd_file_path_string);
}
