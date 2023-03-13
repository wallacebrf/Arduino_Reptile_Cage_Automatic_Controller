/*changelog
3.0: 3/8/2023 (experimental)
updated code to use latest ethernet library
-> this library has built in DHCP and DNS. the old DHCP and DNS lookup features in prior code versions have been replaced
->time server changed from NIST servers to google time servers
*/

/* system requires an SD card to load images off of, and to save logging data. copy the following files into the root of the SD card:
red.png
green.png
on.png
off.png

without these files, page1 "System Main" will not display correctly. */

/*********************************************************************************************************************
                                                      Add required include files and libraries
*********************************************************************************************************************/

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <SPI.h>
#include <Ethernet.h>
#include "WebServer.h"
#include "htmldata.h"
#include <EEPROM.h>
#include <OneWire.h>
#include <avr/wdt.h>
#include <Wire.h>
#include <EthernetUdp.h>
#include "Wire.h"
#include <SD.h>
#include <Dns.h>

/*********************************************************************************************************************
                                                      Create Global System Variables
*********************************************************************************************************************/
IPAddress ip;//byte ip[4];            //ip address of NTP server after the server URL is resolved into IP address. This is so an IP address does not need to be hard coded as recommended by the NTP servers
byte localip[4];        //the local system IP, either local or DHCP generated depending on user preference
byte dnsServerIp[4];      //system variable for the DNS server to use to resolve the NTP server URL. 
#define DS1307_I2C_ADDRESS 0x68 //address of the RTC on the SPI bus
byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };    //MAC address of the local system

// Http header token delimiters
//the purpose of these delimiters is so the code knows when to insert system variables into the outputted HTML code. any time these delimiters are found
//the system will call the substitution function and determine what to output to the web-page
const char *pSpDelimiters = " \r\n";
const char *pStxDelimiter = "\002";    // STX - ASCII start of text character


#define localPort 8888      // local port to listen for UDP packets. needed for the NTP servers for time updating
#define NTP_PACKET_SIZE 48 // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
EthernetUDP Udp;// A UDP instance to let us send and receive packets over UDP

const char* ip_to_str(const uint8_t*);

// declare tables for the images
const char * const image_header PROGMEM = content_image_header;
const int * const data_for_images [] PROGMEM = { content_favicon_data}; // real data
const int   size_for_images [] PROGMEM = { sizeof(content_favicon_data)};
// declare table for all URIs
const char * const http_uris[] PROGMEM = { http_uri1, http_uri2, http_uri3, http_uri4, http_uri5, http_uri6, http_uri7, http_uri8, http_uri9, http_uri10, http_uri11, http_uri12 }; // URIs

#define NUM_PAGES  sizeof(contents_pages)  / sizeof(const char *)    //how many pages does the web-server contain?
#define NUM_IMAGES sizeof(data_for_images) / sizeof(const char *)    // favicon or png format
#define NUM_URIS  NUM_PAGES + NUM_IMAGES  // Pages URIs + favicon URI, etc


EthernetServer server(80);                          //start the instance of server on port 80

EthernetClient mailclient;

DNSClient dnClient;

byte middle_temp_whole;                            //middle heater in tank whole temperature
byte cold_side_temp_whole;                            //cold heater in tank whole temperature
byte hot_side_temp_whole;                            //hot heater in tank whole temperature
byte ambient_temp_1_whole;                          
byte ambient_temp_2_whole;
byte middle_temp_fract;
byte cold_side_temp_fract;
byte hot_side_temp_fract;
byte ambient_temp_1_fract;
byte ambient_temp_2_fract;
byte cold_side_status;                              //the status of the relay controlling the cold side heater. 1 = on, 0 = off
byte hot_side_status;                              //the status of the relay controlling the hot side heater. 1 = on, 0 = off
byte middle_status;                              //the status of the relay controlling the middle side heater. 1 = on, 0 = off
byte middle_temp_status;                              //the status of the the middle side thermal sesnor. 1 = good, 0 = bad. sensor can be bad if it is not connected or if the CRC of the received data is bad
byte cold_side_temp_status;                              //the status of the the cold side thermal sesnor. 1 = good, 0 = bad. sensor can be bad if it is not connected or if the CRC of the received data is bad
byte hot_side_temp_status;                              //the status of the the hot side thermal sesnor. 1 = good, 0 = bad. sensor can be bad if it is not connected or if the CRC of the received data is bad
byte ambient_temp_1_status;                              //the status of the the ambient thermal sesnor. 1 = good, 0 = bad. sensor can be bad if it is not connected or if the CRC of the received data is bad
byte ambient_temp_2_status;
byte heat_lamp_status;                              //the status of the relay controlling the heat lamp. 1 = on, 0 = off
byte UV_Light_status;                              //the status of the relay controlling the UV Light. 1 = on, 0 = off
byte humidifier_status;                              //the status of the relay controlling the humidifier. 1 = on, 0 = off
byte INCSGDTON;                                        //"cold side ground day time on temperature
#define INCSGDTONEEPROMADDR 1                          //eeprom address of the user defined cold side ground day time on temperature
bool INCSGDTON_incorrect;                              //if the user types in an invalid entry into the web-page, this will let the system know if the entry was invalid
byte CSGDTOFF;                                          //cold side ground day time off temerature
#define CSGDTOFFEEPROMADDR 2
bool CSGDTOFF_incorrect;
byte CSGNTON;                                           //cold side ground night time on temperature
#define CSGNTONEEPROMADDR 3
bool CSGNTON_incorrect;
byte CSGNOFF;                                          //cold side ground night time off temperature
#define CSGNOFFEEPROMADDR 4
bool CSGNOFF_incorrect;
byte MSGDTON;                                          //middle side ground day time on temperture
#define MSGDTONEEPROMADDR 5
bool MSGDTON_incorrect;
byte MSGDTOFF;                                        //middle side ground day time off temperature
#define MSGDTOFFEEPROMADDR 6
bool MSGDTOFF_incorrect;
byte MSGNTON;                                         //middle side ground night time on temperature
#define MSGNTONEEPROMADDR 7
bool MSGNTON_incorrect;
byte MSGNTOFF;                                        //middle side ground night time off temperature
#define MSGNTOFFEEPROMADDR 8
bool MSGNTOFF_incorrect;
byte HSGDTON;                                          //hot side ground day time on temperature
#define HSGDTONEEPROMADDR 9
bool HSGDTON_incorrect;
byte HSGDTOFF;                                          //hot side ground day time off temperature
#define HSGDTOFFEEPROMADDR 10
bool HSGDTOFF_incorrect;
byte HSGNTON;                                          //hot side ground night time on temperature
#define HSGNTONEEPROMADDR 11
bool HSGNTON_incorrect;
byte HSGNTOFF;                                        //hot side ground night time off temperature
#define HSGNTOFFEEPROMADDR 12
bool HSGNTOFF_incorrect;
byte AADTON;                                          //average ambient day time on temperature
#define AADTONEEPROMADDR 13
bool AADTON_incorrect;
byte AADTOFF;                                        //average ambient day time off temperature
#define AADTOFFEEPROMADDR 14
bool AADTOFF_incorrect;
byte AANTON;                                        //average ambient nigt time on temperature
#define AANTONEEPROMADDR 15
bool AANTON_incorrect;
byte AANTOFF;                                       //average ambient night time off temperature
#define AANTOFFEEPROMADDR 16
bool AANTOFF_incorrect;
bool DTHUMON_incorrect;
byte DTHUMON;                                      //day time humidifier on level
#define DTHUMONEEPROMADDR 17
bool DTHUMOFF_incorrect; 
byte DTHUMOFF;                                      //day time humidifier off level
#define DTHUMOFFEEPROMADDR 18
bool NTHUMON_incorrect;
byte NTHUMON;                                        //night time humidifier on level
#define NTHUMONEEPROMADDR 19
bool NTHUMOFF_incorrect;
byte NTHUMOFF;                                      //night time humidifier off level
#define NTHUMOFFEEPROMADDR 20
//used to set the time, either from an online NTP server or from the user web-page
byte SETDAYOFWEEK;
byte SETMONTH;
byte SETDAY;
byte SETYEAR;
int TIMEZONE;
byte SETHOUR;
byte SETMINUTE;
byte SETSECONDS;
bool UVLIGHTONHOUR_MINUTE_SECONDS_incorrect;
byte UVLIGHTONHOUR;                                        //when will the UV liht turn on - hour of day
#define UVLIGHTONHOUREEPROMADDR 21
byte UVLIGHTONMINUTE;                                      //when will the UV light turn on - minute of the hour
#define UVLIGHTONMINUTEEEPROMADDR 22
byte UVLIGHTONSECOND;                                      //when will the UV light turn on - second of the minute
#define UVLIGHTONSECONDEEPROMADDR 23
bool UVLIGHTOFFHOUR_MINUTE_SECONDS_incorrect;
byte UVLIGHTOFFHOUR;                                        //when will the UV light turn off?
#define UVLIGHTOFFHOUREEPROMADDR 24
byte UVLIGHTOFFMINUTE;
#define UVLIGHTOFFMINUTEEEPROMADDR 25
byte UVLIGHTOFFSECOND;
#define UVLIGHTOFFSECONDEEPROMADDR 26
byte temp_scale;                                            //system temp scale - degrees F or degrees C? 1 = F, 0 = C
#define TEMPSCALEEEPROMADDR 27
byte max_temp_setting;                                      //depending on the temp scale, the maximum allowable entry on the web-page before it will reject the entry
byte min_temp_setting;                                      //depending on the temp scale, the minimum allowable entry on the web-page before it will reject the entry
#define TIMEZONEEEPROMADDR 28
#define TIMEZONEEEPROMADDRSIGN 29
#define LOCALIPADDREEPROMADDRPART1 30
#define LOCALIPADDREEPROMADDRPART2 31
#define LOCALIPADDREEPROMADDRPART3 32
#define LOCALIPADDREEPROMADDRPART4 33
#define SUBNETMASKEEPROMADDRPART1 34
#define SUBNETMASKEEPROMADDRPART2 35
#define SUBNETMASKEEPROMADDRPART3 36
#define SUBNETMASKEEPROMADDRPART4 37
#define GATEWAYEEPROMADDRPART1 38
#define GATEWAYEEPROMADDRPART2 39
#define GATEWAYEEPROMADDRPART3 40
#define GATEWAYEEPROMADDRPART4 41
#define DNSSERVEREEPROMADDRPART1 42
#define DNSSERVEREEPROMADDRPART2 43
#define DNSSERVEREEPROMADDRPART3 44
#define DNSSERVEREEPROMADDRPART4 45
#define USEDHCPEEPROMADDR 46
byte subnetmask[4];
byte gateway[4];
byte usedhcp;
bool  localip_incorrect;
bool subnetmask_incorrect;
bool gateway_incorrect;
bool dns_incorrect;
bool timeupdated;
//what pins are the 6 different relays connected to?
#define cold_side_ground_pin 31
#define middle_side_ground_pin 33
#define hot_side_ground_pin 35
#define heat_lamp_pin 37
#define UV_light_pin 39
#define humidifier_pin 41
float RH;//whole part of the average ambient humidity
byte CSGMANUAL = 0;    //does the user want the cold side ground relay to be manually controlled or automatically controlled by the syste?
byte MSGMANUAL = 0;    //does the user want the middle ground relay to be manually controlled or automatically controlled by the syste?
byte HSGMANUAL = 0;     //does the user want the hot side ground relay to be manually controlled or automatically controlled by the syste?
byte HEATLAMPMANUAL = 0;  //does the user want the heat lamp relay to be manually controlled or automatically controlled by the syste?
byte UVLIGHTMANUAL = 0;    //does the user want the UV light relay to be manually controlled or automatically controlled by the syste?
byte HUMIDIFIERMANUAL = 0;    //does the user want the humidifier relay to be manually controlled or automatically controlled by the syste?
//SD card required variabled
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;
byte SD_init_OK;    //did the SD card initilzie ok? 1 = yes, 0 = no
byte SDFAT_init_OK;  //does the SD have an active FAT file system? yes = 1, no = 0
byte FAT_Type;        //what kind of file system does the card have? FAT32? FAT16?
byte root_OK;        //cold we open the root of the SD card? yes = 1, no = 0
byte SD_Type;      //is the SD card a SDHC or something else?
double volumesize;  //how much space is on the card in bytes?
double SD_USED_SPACE = 0;  //how many bytes of space is used on the card?
byte data_log_enabled = 0;  //does the user want data logging on or off? 1 = on, 0 = off
byte data_log_period = 10;  //how often does a sample get taken if data logging is enabled? 1 per 10, 20, 30, 40, 50, or 60 seconds?
byte counter;                //msic variable used by data logging
#define CSG1wirepin 38
#define MSG1wirepin 44
#define HSG1wirepin 42
#define AA11wirepin 46
#define AA21wirepin 48
#define humidity1pin 14
#define humidity2pin 15
double last_time_data_saved = 0;
TimeStamp systemstarttime = {0};
int cold_badsensorcount =0;
int middle_badsensorcount =0;
int hot_badsensorcount =0;
int ambient1_badsensorcount =0;
int ambient2_badsensorcount =0;
TimeStamp cold_badsensordate = {0};
TimeStamp middle_badsensordate = {0};
TimeStamp hot_badsensordate = {0};
TimeStamp ambient1_badsensordate = {0};
TimeStamp ambient2_badsensordate = {0};
unsigned long  last_time_email_sent = 0;

//**************************************************
//code that may not be needed for watch dog timer, but it is here just in case
uint8_t mcusr_mirror __attribute__ ((section (".noinit")));
void get_mcusr(void) \
__attribute__((naked)) \
__attribute__((section(".init3")));

void get_mcusr(void)
{
mcusr_mirror = MCUSR;
MCUSR = 0;
// We always need to make sure the WDT is disabled immediately after a
// reset, otherwise it will continue to operate with default values.
//
wdt_disable();
}
//***********************************************/


void GET_NTP_TIME(int timezone)
{
  //get the IP address of the time server
  if(dnClient.getHostByName("time.google.com",ip) == 1) {
    Serial.print(F("time.google.com = "));
    Serial.println(ip);
  }else{ 
    Serial.print(F("dns lookup failed"));
  }
 
  wdt_enable(WDTO_4S);
  
  sendNTPpacket(); // send an NTP packet to a time server
  //wdt_reset();
    // wait to see if a reply is available
  delay(1000);  
  wdt_reset();
  if ( Udp.parsePacket() ) {  
    // We've received a packet, read the data from it
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;  

    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;     
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;                                
    

    // print the hour, minute and second:    
    SETHOUR = int(((epoch+(timezone*3600))  % 86400)/3600);      
    SETMINUTE = int((epoch  % 3600)/60); 
    SETSECONDS = int(epoch %60);
  }
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(void)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:            
  Udp.beginPacket(ip, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket();
}

// Just a utility function to nicely format an IP address.
const char* ip_to_str(const uint8_t* ipAddr)
{
  static char buf[16];
  sprintf(buf, "%d.%d.%d.%d\0", ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
  return buf;
}

// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return ( (val/10*16) + (val%10) );
}

// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return ( (val/16*10) + (val%16) );
}

// 1) Sets the date and time on the ds1307
// 2) Starts the clock
// 3) Sets hour mode to 24 hour clock

// Assumes you're passing in valid numbers

void setDateDs1307(byte set_second,        // 0-59
byte set_minute,        // 0-59
byte set_hour,          // 1-23
byte set_dayOfWeek,     // 1-7
byte set_dayOfMonth,    // 1-28/29/30/31
byte set_month,         // 1-12
byte set_year)          // 0-99
{
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0);
  Wire.write(decToBcd(set_second));    // 0 to bit 7 starts the clock
  Wire.write(decToBcd(set_minute));
  Wire.write(decToBcd(set_hour));     
  Wire.write(decToBcd(set_dayOfWeek));
  Wire.write(decToBcd(set_dayOfMonth));
  Wire.write(decToBcd(set_month));
  Wire.write(decToBcd(set_year));
  Wire.write(0x10); // sends 0x10 (hex) 00010000 (binary) to control register - turns on square wave
  Wire.endTransmission();
}

// Gets the date and time from the ds1307
void getDateDs1307(byte *get_second,
byte *get_minute,
byte *get_hour,
byte *get_dayOfWeek,
byte *get_dayOfMonth,
byte *get_month,
byte *get_year)
{
  // Reset the register pointer
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_I2C_ADDRESS, 7);

  // A few of these need masks because certain bits are control bits
  *get_second     = bcdToDec(Wire.read() & 0x7f);
  *get_minute     = bcdToDec(Wire.read());
  *get_hour       = bcdToDec(Wire.read() & 0x3f);  // Need to change this if 12 hour am/pm
  *get_dayOfWeek  = bcdToDec(Wire.read());
  *get_dayOfMonth = bcdToDec(Wire.read());
  *get_month      = bcdToDec(Wire.read());
  *get_year       = bcdToDec(Wire.read());
}

float GET_HUMIDITY(byte analogPin){
/*[Voltage output (2nd order curve fit)]    Vout=0.00003(sensor RH)^2+0.0281(sensor RH)+0.820, typical @ 25 ºC and 5VDC supply


Solving the above for (sensor RH) = 182.57418583506*(sqrt(Vout + 5.76008333333335) – 2.5651673109825)


Vout = (float)(analogRead(pin)) * vRef / 1023 where vRef = 5 volts

Vout = (float)(analogRead(pin)) * 5 / 1023

Vout = (float)(analogRead(pin)) * 0.004888*/


  float Percent_RH = 182.57418583506*(sqrt(((float)(analogRead(analogPin)) * 0.004888) + 5.76008333333335) - 2.5615673109825);
  return Percent_RH;
}

void ListFiles(EthernetClient client, uint8_t flags) {
  // This code is just copied from SdFile.cpp in the SDFat library
  // and tweaked to print to the client output in html!
  dir_t p;
  
  root.rewind();
  sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[43])));  //client.print("<table width=\"50%\" height=\"14%\" border=\"0\">");
  while (root.readDir(p) > 0) {
    //wdt_reset();
    // done if past last used entry
    if (p.name[0] == DIR_NAME_FREE) break;

    // skip deleted entry and entries for . and  ..
    if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.') continue;

    // only list subdirectories and files
    if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;
    
    //skip image files, as we do not want to modify them
    if (p.name[0] == 'O' || p.name[0] == 'N' || p.name[0] == 'F' || p.name[0] == 'G' || p.name[0] == 'R' || p.name[0] == 'E' || p.name[0] == 'R' || p.name[0] == 'D' || p.name[0] == 'P') continue;

    // print any indent spaces
    sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[44])));  //client.print("<tr><td align=\"center\"><a href=\"");
    for (uint8_t i = 0; i < 11; i++) {
      //wdt_reset();
      if (p.name[i] == ' ') continue;
      if (i == 8) {
        sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[31])));  //client.print(".");
      }
      client.print((char)p.name[i]);
    }
    sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[45])));  //client.print("\" target=\"blank\">");
    
    // print file name with possible blank fill
    for (uint8_t i = 0; i < 11; i++) {
      //wdt_reset();
      if (p.name[i] == ' ') continue;
      if (i == 8) {
        sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[31])));  //client.print(".");
      }
      client.print((char)p.name[i]);
    }
    
    sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[46])));  //client.print("</a> || File Size: ");
    client.print(p.fileSize / 1024);
    sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[47])));  //client.print("Kilobytes (KB)");
    sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[48])));  //client.print("</td></tr>");
  }
  sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[49])));  //client.print("</table>");
}

void ListFilesToDelete(EthernetClient client, uint8_t flags) {
  // This code is just copied from SdFile.cpp in the SDFat library
  // and tweaked to print to the client output in html!
  dir_t p;
  
  root.rewind();
  sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[50])));  //client.print("<select name = \"delete\"");
  while (root.readDir(p) > 0) {
    //wdt_reset();
    // done if past last used entry
    if (p.name[0] == DIR_NAME_FREE) break;

    // skip deleted entry and entries for . and  ..
    if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.') continue;

    // only list subdirectories and files
    if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;
    
    //skip image files, as we do not want to modify them
    if (p.name[0] == 'O' || p.name[0] == 'N' || p.name[0] == 'F' || p.name[0] == 'G' || p.name[0] == 'R' || p.name[0] == 'E' || p.name[0] == 'R' || p.name[0] == 'D' || p.name[0] == 'P') continue;

    // print any indent spaces
    sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[51])));  //client.print("<option value=\"");
    
    for (uint8_t i = 0; i < 11; i++) {
      //wdt_reset();
      if (p.name[i] == ' ') continue;
      if (i == 8) {
        sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[31])));  //client.print(".");
      }
      client.print((char)p.name[i]);
    }
    sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[52])));  //client.print("\">");
    
    for (uint8_t i = 0; i < 11; i++) {
      //wdt_reset();
      if (p.name[i] == ' ') continue;
      if (i == 8) {
        sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[31])));  //client.print(".");
      }
      client.print((char)p.name[i]);
    }
    sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[53])));  //client.print("</option>");
  
  }
}

double SDUSEDSPACE(void) {
  // This code is just copied from SdFile.cpp in the SDFat library
  // and tweaked to print to the client output in html!
  dir_t p;
  uint8_t flags = LS_SIZE;
  double SD_USED_SPACE = 0;
  
  root.rewind();
  while (root.readDir(p) > 0) {
    //wdt_reset();
    // done if past last used entry
    if (p.name[0] == DIR_NAME_FREE) break;

    // skip deleted entry and entries for . and  ..
    if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.') continue;

    // only list subdirectories and files
    if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;

    if (!DIR_IS_SUBDIR(&p) && (flags & LS_SIZE)) {
      SD_USED_SPACE += p.fileSize;
    }
  }
  root.rewind();
  return SD_USED_SPACE;
}

// Function to return a substring defined by a delimiter at an index
char* subStr (char* str, char* delim, int index) {
   char *act, *sub, *ptr;
   static char copy[12];
   int i;

   // Since strtok consumes the first arg, make a copy
   strcpy(copy, str);

   for (i = 1, act = copy; i <= index; i++, act = NULL) {
      //Serial.print(".");
        sub = strtok_r(act, delim, &ptr);
      if (sub == NULL) break;
   }
   return sub;

}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

byte PROCESSREQUESTCONTENT(byte index, BUFFER & requestContent, BUFFER & output){
 char * pch;
 byte beginning=0;
 byte ending=0;
 byte counter=0;
 //locate where "%" is located so we know where to stop
 pch=strchr(requestContent,'%');
 
 //"pch-4" is required because pch indicates where the "%" sign is with the first character starting at 1. however the char array starts at 0. in addition we do not care about the "&17=" part of the ending.
 //the length of the "5" itself is removed because of the offset from starting point 1 to starting point 0
 //the "-4" is so the code ignores the "&18=" characters
 for (byte x=0;x<= (pch-requestContent-1)-3;x++){
   if (strncmp(&requestContent[x],"=",1)==0){
     beginning = x+1; //starting character position of actual data entry
   }else if (strncmp(&requestContent[x],"&",1)==0){
     ending=x;//ending character position of actual data entry
     counter++;//increment counter so we can see if this is the data entry we care about
   }
   if (counter==index){//have we found the data antry we care about?
     counter = 0;
     for (byte i=0; i<=(ending-beginning);i++){//copy the data entry so we can use it
       output[i]=requestContent[beginning+i];
     }
     return 1;
   }     
 }
  return 0;
}

void startethernet(void){
  /*********************************************************************************************************************
                                                      DHCP or Static Network Settings
*********************************************************************************************************************/
  if (usedhcp == 1){//use DHCP
    wdt_reset();
    // start the Ethernet connection:

    Serial.println(F("Initialize Ethernet with DHCP:"));

    if (Ethernet.begin(mac) == 0) {

      Serial.println(F("Failed to configure Ethernet using DHCP"));

      if (Ethernet.hardwareStatus() == EthernetNoHardware) {

        Serial.println(F("Ethernet shield was not found.  Sorry, can't run without hardware. :("));

      } else if (Ethernet.linkStatus() == LinkOFF) {

        Serial.println(F("Ethernet cable is not connected. Please reset or power cycle"));

      }

      // no point in carrying on, so do nothing forevermore:

      while (true) {

        delay(10000);
        Serial.println(F("Ethernet cable is not connected. Please reset or power cycle"));

      }

    }

    wdt_reset();
    //now that we have a DHCP lease, we need to save all the lease information so the rest of the system can use it
    const byte* ipAddr = Ethernet.localIP();
    const byte* gatewayAddr = Ethernet.gatewayIP();
    const byte* dnsAddr = Ethernet.dnsServerIP();
    
    //local system IP address
    localip[0] = ipAddr[0];
    localip[1] = ipAddr[1];
    localip[2] = ipAddr[2];
    localip[3] = ipAddr[3];
    
    //the subnet mask is always user define, and not determined by DHCP
    subnetmask[0] = EEPROM.read(SUBNETMASKEEPROMADDRPART1);
    subnetmask[1] = EEPROM.read(SUBNETMASKEEPROMADDRPART2);
    subnetmask[2] = EEPROM.read(SUBNETMASKEEPROMADDRPART3);
    subnetmask[3] = EEPROM.read(SUBNETMASKEEPROMADDRPART4);
    
    //the default gate way IP address 
    gateway[0] = gatewayAddr[0];
    gateway[1] = gatewayAddr[1];
    gateway[2] = gatewayAddr[2];
    gateway[3] = gatewayAddr[3];
    
    //the DNS server IP address
    dnsServerIp[0] = dnsAddr[0];
    dnsServerIp[1] = dnsAddr[1];
    dnsServerIp[2] = dnsAddr[2];
    dnsServerIp[3] = dnsAddr[3];
    Serial.println(F("A DHCP lease has been obtained."));

    Serial.print(F("System IP address is: "));
    Serial.println(ip_to_str(ipAddr));
  
    Serial.print(F("Gateway address is: "));
    Serial.println(ip_to_str(gatewayAddr));
  
    Serial.print(F("DNS IP address is: "));
    Serial.println(ip_to_str(dnsAddr));
    
    Serial.print(F("Subnet Mask is: "));
    Serial.println(ip_to_str(subnetmask));
  }else{      //do not use DHCP but use static settings defined by the user, saved in EEPROM
    localip[0] = EEPROM.read(LOCALIPADDREEPROMADDRPART1);
    localip[1] = EEPROM.read(LOCALIPADDREEPROMADDRPART2);
    localip[2] = EEPROM.read(LOCALIPADDREEPROMADDRPART3);
    localip[3] = EEPROM.read(LOCALIPADDREEPROMADDRPART4);
    subnetmask[0] = EEPROM.read(SUBNETMASKEEPROMADDRPART1);
    subnetmask[1] = EEPROM.read(SUBNETMASKEEPROMADDRPART2);
    subnetmask[2] = EEPROM.read(SUBNETMASKEEPROMADDRPART3);
    subnetmask[3] = EEPROM.read(SUBNETMASKEEPROMADDRPART4);
    gateway[0] = EEPROM.read(GATEWAYEEPROMADDRPART1);
    gateway[1] = EEPROM.read(GATEWAYEEPROMADDRPART2);
    gateway[2] = EEPROM.read(GATEWAYEEPROMADDRPART3);
    gateway[3] = EEPROM.read(GATEWAYEEPROMADDRPART4);
    dnsServerIp[0] = EEPROM.read(DNSSERVEREEPROMADDRPART1);
    dnsServerIp[1] = EEPROM.read(DNSSERVEREEPROMADDRPART2);
    dnsServerIp[2] = EEPROM.read(DNSSERVEREEPROMADDRPART3);
    dnsServerIp[3] = EEPROM.read(DNSSERVEREEPROMADDRPART4);
    
    Ethernet.begin(mac, localip, dnsServerIp, gateway, subnetmask); 
    Serial.println(F("System is using Static Ethernet Settings."));

    Serial.print(F("System IP address is: "));
    Serial.println(ip_to_str(localip));
  
    Serial.print(F("Gateway address is: "));
    Serial.println(ip_to_str(gateway));
  
    Serial.print(F("DNS IP address is: "));
    Serial.println(ip_to_str(dnsServerIp));
    
    Serial.print(F("Subnet Mask is: "));
    Serial.println(ip_to_str(subnetmask));
  }
  dnClient.begin(Ethernet.dnsServerIP());
}

void sendemail(void){
  if(dnClient.getHostByName("smtp-server.wi.rr.com",ip) == 1) {
    Serial.print(F("smtp-server.wi.rr.com = "));
    Serial.println(ip);
  }else{ 
    Serial.print(F("dns lookup failed"));
  }
 
  wdt_enable(WDTO_4S);

  if (mailclient.connect(ip, 25)) {
    delay(1000); /* wait for a response */
    wdt_reset();
    mailclient.println(F("helo wi.rr.com")); /* say hello*/
    wdt_reset();
    delay(1000); /* wait for a response */
    wdt_reset();
    mailclient.println(F("mail from: <wallacebrf@hotmail.com>")); /* identify sender */
    wdt_reset();
    delay(1000); /* wait for a response */
    wdt_reset();
    mailclient.println(F("rcpt to: <chinmonitor@wi.rr.com>")); /* identify recipient */
    wdt_reset();
    delay(1000); /* wait for a response */
    wdt_reset();
    mailclient.println(F("data"));
    wdt_reset();
    delay(1000); /* wait for a response */
    wdt_reset();
    mailclient.println(F("to: <chin monitor>")); /* insert to */
    mailclient.println(F("from: <chin monitor>")); /* insert from */
    mailclient.println(F("subject: Rotti Sensor Malfunction")); /* insert subject */
    mailclient.print(F("A sensor has been detected to be malfunctioning. Please Check Sensors."));
    mailclient.println();
    mailclient.println(F("."));
    delay(1000); /* wait for a response */
    wdt_reset();
    mailclient.println(F("quit")); /* terminate connection */
    wdt_reset();
    delay(1000); /* wait for a response */
    wdt_reset();
    mailclient.println();
    wdt_reset();
    mailclient.stop();
 }
}

TimeStamp calcDiff(TimeStamp now, const TimeStamp &future)
{
    byte current_month = now.mm;
    unsigned long sec_now = now.ss + ((unsigned long)now.min * 60) + ((unsigned long)now.hh * 3600);
    unsigned long sec_future = future.ss + ((unsigned long)future.min * 60) + ((unsigned long)future.hh * 3600);

    if (sec_future < sec_now)
    {
        sec_future += 86400;
        ++now.dd;
    }

    if (future.dd < now.dd)
    {
        now.dd = DAYS_IN_MONTH[now.mm-1] - now.dd + future.dd;
        ++now.mm;
    } else {
        now.dd = future.dd - now.dd;
    }

    if (future.mm < now.mm)
    {        
        now.mm = future.mm + 12 - now.mm;
        if ((current_month + now.mm) > 12){
          now.dd -= (DAYS_IN_MONTH[current_month-1] - DAYS_IN_MONTH[(current_month + now.mm - 12)-1]);
        }else{
          now.dd -= (DAYS_IN_MONTH[current_month-1] - DAYS_IN_MONTH[(current_month + now.mm)-1]);
        }
        ++now.yy;
    } else {
        now.mm = future.mm - now.mm;
    }

    sec_now = sec_future - sec_now;
    now.yy = future.yy - now.yy;
    now.hh = sec_now / 3600;
    now.min = (sec_now - ((unsigned long)now.hh * 3600)) / 60;
    now.ss = sec_now % 60;
    return now;
}



/*********************************************************************************************************************
                                                      System Setup
*********************************************************************************************************************/

void setup()
{
  wdt_enable(WDTO_8S);        //enable the watchdog timer with an 8 second time out
  Serial.begin(115200); 
  /*********************************************************************************************************************
                                                      Recall all needed information from EEPROM on boot
*********************************************************************************************************************/
  INCSGDTON = EEPROM.read(INCSGDTONEEPROMADDR);
  CSGDTOFF = EEPROM.read(CSGDTOFFEEPROMADDR);
  CSGNTON = EEPROM.read(CSGNTONEEPROMADDR);
  CSGNOFF = EEPROM.read(CSGNOFFEEPROMADDR);
  MSGDTON = EEPROM.read(MSGDTONEEPROMADDR);
  MSGDTOFF = EEPROM.read(MSGDTOFFEEPROMADDR);
  MSGNTON = EEPROM.read(MSGNTONEEPROMADDR);
  MSGNTOFF = EEPROM.read(MSGNTOFFEEPROMADDR);
  HSGDTON = EEPROM.read(HSGDTONEEPROMADDR);
  HSGDTOFF = EEPROM.read(HSGDTOFFEEPROMADDR);
  HSGNTON = EEPROM.read(HSGNTONEEPROMADDR);
  HSGNTOFF = EEPROM.read(HSGNTOFFEEPROMADDR);
  AADTON = EEPROM.read(AADTONEEPROMADDR);
  AADTOFF = EEPROM.read(AADTOFFEEPROMADDR);
  AANTON = EEPROM.read(AANTONEEPROMADDR);
  AANTOFF = EEPROM.read(AANTOFFEEPROMADDR);
  DTHUMON = EEPROM.read(DTHUMONEEPROMADDR);
  DTHUMOFF = EEPROM.read(DTHUMOFFEEPROMADDR);
  NTHUMON = EEPROM.read(NTHUMONEEPROMADDR);
  NTHUMOFF = EEPROM.read(NTHUMOFFEEPROMADDR);
  UVLIGHTONHOUR = EEPROM.read(UVLIGHTONHOUREEPROMADDR);
  UVLIGHTONMINUTE = EEPROM.read(UVLIGHTONMINUTEEEPROMADDR);
  UVLIGHTONSECOND = EEPROM.read(UVLIGHTONSECONDEEPROMADDR);
  UVLIGHTOFFHOUR = EEPROM.read(UVLIGHTOFFHOUREEPROMADDR);
  UVLIGHTOFFMINUTE = EEPROM.read(UVLIGHTOFFMINUTEEEPROMADDR);
  UVLIGHTOFFSECOND = EEPROM.read(UVLIGHTOFFSECONDEEPROMADDR);
  temp_scale = EEPROM.read(TEMPSCALEEEPROMADDR);
  //since the EEPROM cannot save anything other than 0-255, we needed to save a "sign" bit. based on tha bit, we need to set the time zone positive or negative
  if (EEPROM.read(TIMEZONEEEPROMADDRSIGN) == 1){
    TIMEZONE = EEPROM.read(TIMEZONEEEPROMADDR)*-1;
  }else{
    TIMEZONE = EEPROM.read(TIMEZONEEEPROMADDR);
  }
  if (temp_scale == 1){ //if temp scale is in degrees F
    max_temp_setting = 120;
    min_temp_setting = 20;
  }else{                //if temp scale is in degrees C
    max_temp_setting = 49;
    min_temp_setting = 0;
  }
  wdt_reset();
  usedhcp = EEPROM.read(USEDHCPEEPROMADDR);
  startethernet();
    
    Wire.begin();//needed to comunicate with the real time clock
    server.begin();//needed to set up the web-server
  
    Udp.begin(localPort);//activate the UDP system needed to access the online NTP servers
    //EthernetDNS.setDNSServer(dnsServerIp);  //start the DNS subsystems
    wdt_reset();
    
    //**********************************************************
              //define output pins
    //**********************************************************
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    pinMode(cold_side_ground_pin, OUTPUT);
    pinMode(middle_side_ground_pin, OUTPUT);
    pinMode(hot_side_ground_pin, OUTPUT);
    pinMode(heat_lamp_pin, OUTPUT);
    pinMode(UV_light_pin, OUTPUT);
    pinMode(humidifier_pin, OUTPUT);
    digitalWrite(cold_side_ground_pin, HIGH);//relay 1 - cold side ground
    digitalWrite(middle_side_ground_pin, HIGH);//relay 2 - middle side ground
    digitalWrite(hot_side_ground_pin, HIGH);//relay 3 - hot side ground
    digitalWrite(heat_lamp_pin, HIGH);//relay 4 - heat lamp
    digitalWrite(UV_light_pin, HIGH);//relay 5 - UV Light
    digitalWrite(humidifier_pin, HIGH);//relay 6 - Humidifier
    timeupdated = false;
    analogReference(DEFAULT);//set the ADC to use 5 volts as a reference
    pinMode(53, OUTPUT);//required for SD library! 
  //because of an issue with the SD library, if NO SD card is present, the SPI bus runs very slow because of changes made to two registers.
  //we are going to make a backup of these registers now before we nitilize the SD card. 
  //that way, if no SD card is fond, we can re-initilize those registers back to their proper values  
  uint8_t spcr = SPCR;
  uint8_t spsr = SPSR;
  if (!card.init(0, 5)){ // SD chip select on pin 5
    SD_init_OK = 0;//SD card not initilized
    Serial.print(F("NO SD Card"));
    //no SD card found, restore the SPI bus settings for full speed
    SPCR = spcr;
    SPSR = spsr;
    data_log_enabled =0;
  }else{
    Serial.print(F("SD card found"));
    SD_init_OK = 1;//SD card initilized OK
    // initialize a FAT volume
    if (!volume.init(&card)){
      SDFAT_init_OK = 0;//FAT volume not initilized
      data_log_enabled =0;
    }else{
      SDFAT_init_OK = 1;//FAT volume initilized OK
      FAT_Type = volume.fatType();//what kind of fat is the card formatted with?
      if (!root.openRoot(&volume)){
        root_OK = 0;//the root of the SD card could not be opened!
        data_log_enabled =0;
      }else{
        root_OK = 1;
        SD_Type = card.type();                    //what kind of card was inserted?
        volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
        volumesize *= volume.clusterCount();       // we'll have a lot of clusters
        volumesize *= 512;                            // SD card blocks are always 512 bytes
      }
    }
  }

  getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);//get current time from RTC
  systemstarttime.yy = year+2000;
  systemstarttime.mm = month;
  systemstarttime.dd = dayOfMonth;
  systemstarttime.hh = hour;
  systemstarttime.min = minute;
  systemstarttime.ss = second;
}

/**********************************************************************************************************************
*                                                           Main loop
***********************************************************************************************************************/
void loop()
{
  if (usedhcp == 1){//use DHCP and maintain DHCP
    switch (Ethernet.maintain()) {
      case 1:
        Serial.println(F("Error: renewed fail"));
        break;
      case 2:
        Serial.println(F("Renewed success"));
        Serial.print(F("My IP address: "));
        Serial.println(Ethernet.localIP());
        break;
      case 3:
        Serial.println(F("Error: rebind fail"));
        break;
      case 4:
        Serial.println(F("Rebind success"));
        Serial.print(F("My IP address: "));
        Serial.println(Ethernet.localIP());
        break;
      default:
        break;
    }
  }
  float humidity1sample1, humidity1sample2, humidity1sample3, humidity1sample4, humidity1sample5;//we want 5 samples of the humidity so we can average them for better accuracy
  float humidity2sample1, humidity2sample2, humidity2sample3, humidity2sample4, humidity2sample5;//we want 5 samples of the humidity so we can average them for better accuracy
  byte temp_whole, temp_fract, temp_status;
   wdt_reset();
   GETETHERNET();//called to check if any incoming clients are trying to conenct to the server
   wdt_reset();
   CONVERT_TEMP(MSG1wirepin, temp_whole, temp_fract, temp_status);
   middle_temp_whole = temp_whole;
   middle_temp_fract = temp_fract;
   middle_temp_status = temp_status;
   humidity1sample1 = GET_HUMIDITY(humidity1pin);//convert humidity and save the result from the sensor attached to analog pin 14
   humidity2sample1 = GET_HUMIDITY(humidity2pin);//convert humidity and save the result from the sensor attached to analog pin 14
   wdt_reset();
   GETETHERNET();
   wdt_reset();
   CONVERT_TEMP(CSG1wirepin, temp_whole, temp_fract, temp_status);
   cold_side_temp_whole = temp_whole;
   cold_side_temp_fract = temp_fract;
   cold_side_temp_status = temp_status;
   humidity1sample2 = GET_HUMIDITY(humidity1pin);
   humidity2sample2 = GET_HUMIDITY(humidity2pin);
   wdt_reset();
   GETETHERNET();
   wdt_reset();
   CONVERT_TEMP(HSG1wirepin, temp_whole, temp_fract, temp_status);
   hot_side_temp_whole = temp_whole;
   hot_side_temp_fract = temp_fract;
   hot_side_temp_status = temp_status;
   humidity1sample3 = GET_HUMIDITY(humidity1pin);
   humidity2sample3 = GET_HUMIDITY(humidity2pin);
   wdt_reset();
   GETETHERNET();
   wdt_reset();
   CONVERT_TEMP(AA11wirepin, temp_whole, temp_fract, temp_status);
   ambient_temp_1_whole = temp_whole;
   ambient_temp_1_fract = temp_fract;
   ambient_temp_1_status = temp_status;
   humidity1sample4 = GET_HUMIDITY(humidity1pin);
   humidity2sample4 = GET_HUMIDITY(humidity1pin);
   wdt_reset();
   GETETHERNET();
   wdt_reset();
   CONVERT_TEMP(AA21wirepin, temp_whole, temp_fract, temp_status);
   ambient_temp_2_whole = temp_whole;
   ambient_temp_2_fract = temp_fract;
   ambient_temp_2_status = temp_status;
   humidity1sample5 = GET_HUMIDITY(humidity1pin);
   humidity2sample5 = GET_HUMIDITY(humidity1pin);
   wdt_reset();
   GETETHERNET();
   wdt_reset();
   getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
   //since the RTC is not perfectly accurate, the system is designed to conenct to an online NTP server every day between 4:00 and 4:01 hours
   if (timeupdated == false){//have we updated yet?
     if (((hour * 3600UL) + (minute * 60UL) + second) >= (4*3600UL) && ((hour * 3600UL) + (minute * 60UL) + second) < ((4*3600UL)+60UL)){//is the current system time between 4:00 and 4:01?
       wdt_reset();
       digitalWrite(cold_side_ground_pin, HIGH);
       digitalWrite(middle_side_ground_pin, HIGH);
       digitalWrite(hot_side_ground_pin, HIGH);//relay 1 - cold side ground
       cold_side_status = 0;
       hot_side_status = 0;
       middle_status = 0;
       GET_NTP_TIME(TIMEZONE);//conencts to the NTP server using the configures system time zone and the IP address resolved previously
       wdt_reset();
       if (SETSECONDS != 0 && SETMINUTE != 0 && SETHOUR != 0){
         getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);//gets the current time from the RTC
         setDateDs1307(SETSECONDS, SETMINUTE, SETHOUR, dayOfWeek, dayOfMonth, month, year);//re-sets only the hours, minutes, and seconds, as that is the only part that gets updated from the NTP server. all other time aspects have not changed
         getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);//now that the RTC is updated, get the new updated time back out of the clock
         timeupdated = true;//we have updated, do not want to updated again until the next day
       }
     }
   }else{
     //clears the updated flag between 1:00 and 1:01 hours so the system will be ready to update again at 4:00 hours
     if (((hour * 3600UL) + (minute * 60UL) + second) >= 3600UL && ((hour * 3600UL) + (minute * 60UL) + second) < (3600UL+60UL)){
       timeupdated = false;
     }
   }
  //RH = ((humidity1sample1 + humidity1sample2 + humidity1sample3 + humidity1sample4 + humidity1sample5) / 5);//because of the accuracy of the sensor used, we are taking 5 samples roughly 1 scond apart and taking the average
  RH = ((humidity1sample1 + humidity1sample2 + humidity1sample3 + humidity1sample4 + humidity1sample5 + humidity2sample1 + humidity2sample2 + humidity2sample3 + humidity2sample4 + humidity2sample5) / 10);//because of the accuracy of the sensor used, we are taking 5 samples roughly 1 scond apart and taking the average
  if ((((hour * 3600UL) + (minute * 60UL) + second) >= ((UVLIGHTONHOUR * 3600UL) + (UVLIGHTONMINUTE * 60UL) + UVLIGHTONSECOND)) && (((hour * 3600UL) + (minute * 60UL) + second) < ((UVLIGHTOFFHOUR * 3600UL) + (UVLIGHTOFFMINUTE * 60UL) + UVLIGHTOFFSECOND))){//if it is day time
       if (middle_temp_status == 1 && MSGMANUAL == 0){//as long as the temperature sensor is good, and the relay is set to be automatically controlled
         if (middle_temp_whole < MSGDTON){//is the current temperature reading below on the heater on threashold?
           digitalWrite(middle_side_ground_pin, LOW);//relay 2 - middle side ground
           middle_status = 1;//used by the web-server interface to tell the user if the relay is on or off, 1 means on, 0 means off
         }else if (middle_temp_whole >= MSGDTOFF){//is the current reading above the heater off threashold?
           digitalWrite(middle_side_ground_pin, HIGH);//relay 2 - middle side ground
           middle_status = 0;//used by the web-server interface to tell the user if the relay is on or off, 1 means on, 0 means off
         }
       }else if (middle_temp_status == 0 && MSGMANUAL == 0){//is the temperature sensor bad? then default the relay to the off position so the heater does not get stuck on and possibly overheat the snake
         digitalWrite(middle_side_ground_pin, HIGH);//relay 2 - middle side ground
         middle_status = 0;
       }
       if (cold_side_temp_status == 1 && CSGMANUAL == 0){
         if (cold_side_temp_whole < INCSGDTON){
           digitalWrite(cold_side_ground_pin, LOW);//relay 1 - cold side ground
           cold_side_status = 1;
         }else if (cold_side_temp_whole >= CSGDTOFF){
           digitalWrite(cold_side_ground_pin, HIGH);//relay 1 - cold side ground
           cold_side_status = 0;
         }
       }else if (cold_side_temp_status == 0 && CSGMANUAL == 0){
         digitalWrite(cold_side_ground_pin, HIGH);//relay 1 - cold side ground
         cold_side_status = 0;
       }
       if (hot_side_temp_status == 1 && HSGMANUAL ==0){
         if (hot_side_temp_whole < HSGDTON){
           digitalWrite(hot_side_ground_pin, LOW);//relay 3 - hot side ground
           hot_side_status = 1;
         }else if (hot_side_temp_whole >= HSGDTOFF){
           digitalWrite(hot_side_ground_pin, HIGH);//relay 3 - hot side ground
           hot_side_status = 0;
         }
       }else if (hot_side_temp_status == 0 && HSGMANUAL ==0){
         digitalWrite(hot_side_ground_pin, HIGH);//relay 3 - hot side ground
         hot_side_status = 0;
       }
       if ((((float)ambient_temp_2_whole + (float)ambient_temp_1_whole + ((float)ambient_temp_2_fract / 100.0) + ((float)ambient_temp_1_fract / 100.0))/2.0) < (AADTON*1.00) && HEATLAMPMANUAL == 0){
         digitalWrite(heat_lamp_pin, LOW);//relay 4 - heat lamp
         heat_lamp_status = 1;
       }else if ((((float)ambient_temp_2_whole + (float)ambient_temp_1_whole + ((float)ambient_temp_2_fract / 100.0) + ((float)ambient_temp_1_fract / 100.0))/2.0) >= (AADTOFF*1.00) && HEATLAMPMANUAL == 0){
         digitalWrite(heat_lamp_pin, HIGH);//relay 4 - heat lamp
         heat_lamp_status = 0;
       }
       if (RH < DTHUMON && HUMIDIFIERMANUAL ==0){//is the humidity below the humidifier on threashold?
         digitalWrite(humidifier_pin, LOW);//relay 3 - hot side ground
         humidifier_status = 1;
       }else if (RH >= DTHUMOFF && HUMIDIFIERMANUAL ==0){
         digitalWrite(humidifier_pin, HIGH);//relay 3 - hot side ground
         humidifier_status = 0;
       }
       if (UVLIGHTMANUAL == 0){
         //if it is day time, then the UV light must be on. the relays used by the system are active low
         digitalWrite(UV_light_pin, LOW);//relay 4 - UV Light
         UV_Light_status = 1;//used by the web-server interface to tell the user if the relay is on or off, 1 means on, 0 means off
       }
     }else{//it must be night time, use the night time user settings instead
       if (middle_temp_status == 1 && MSGMANUAL == 0){
         if (middle_temp_whole < MSGNTON){
           digitalWrite(middle_side_ground_pin, LOW);//relay 2 - middle side ground
           middle_status = 1;//used by the web-server interface to tell the user if the relay is on or off, 1 means on, 0 means off
         }else if (middle_temp_whole >= MSGNTOFF){
           digitalWrite(middle_side_ground_pin, HIGH);//relay 2 - middle side ground
           middle_status = 0;//used by the web-server interface to tell the user if the relay is on or off, 1 means on, 0 means off
         }
       }else if (middle_temp_status == 0 && MSGMANUAL == 0){
           digitalWrite(middle_side_ground_pin, HIGH);//relay 2 - middle side ground
           middle_status = 0;//used by the web-server interface to tell the user if the relay is on or off, 1 means on, 0 means off
       }
       if (cold_side_temp_status == 1 && CSGMANUAL == 0){
         if (cold_side_temp_whole < CSGNTON){
           digitalWrite(cold_side_ground_pin, LOW);//relay 1 - cold side ground
           cold_side_status = 1;
         }else if (cold_side_temp_whole >= CSGNOFF){
           digitalWrite(cold_side_ground_pin, HIGH);//relay 1 - cold side ground
           cold_side_status = 0;
         }
       }else if (cold_side_temp_status == 0 && CSGMANUAL == 0){
         digitalWrite(cold_side_ground_pin, HIGH);//relay 1 - cold side ground
         cold_side_status = 0;
       }
       if (hot_side_temp_status == 1 && HSGMANUAL == 0){
         if (hot_side_temp_whole < HSGNTON){
           digitalWrite(hot_side_ground_pin, LOW);//relay 3 - hot side ground
           hot_side_status = 1;
         }else if (hot_side_temp_whole >= HSGNTOFF){
           digitalWrite(hot_side_ground_pin, HIGH);//relay 3 - hot side ground
           hot_side_status = 0;
         }
       }else if (hot_side_temp_status == 0 && HSGMANUAL == 0){
         digitalWrite(hot_side_ground_pin, HIGH);//relay 3 - hot side ground
         hot_side_status = 0;
       }
      if ((((float)ambient_temp_2_whole + (float)ambient_temp_1_whole + ((float)ambient_temp_2_fract / 100.0) + ((float)ambient_temp_1_fract / 100.0))/2.0) < (AANTON*1.00) && HEATLAMPMANUAL == 0){
         digitalWrite(heat_lamp_pin, LOW);//relay 4 - heat lamp
         heat_lamp_status = 1;
       }else if ((((float)ambient_temp_2_whole + (float)ambient_temp_1_whole + ((float)ambient_temp_2_fract / 100.0) + ((float)ambient_temp_1_fract / 100.0))/2.0) >= (AANTOFF*1.00) && HEATLAMPMANUAL == 0){
         digitalWrite(heat_lamp_pin, HIGH);//relay 4 - heat lamp
         heat_lamp_status = 0;
       }
       if (RH < NTHUMON && HUMIDIFIERMANUAL == 0){
         digitalWrite(humidifier_pin, LOW);//relay 3 - hot side ground
         humidifier_status = 1;
       }else if (RH >= NTHUMOFF && HUMIDIFIERMANUAL == 0){
         digitalWrite(humidifier_pin, HIGH);//relay 3 - hot side ground
         humidifier_status = 0;
       }
       if (UVLIGHTMANUAL == 0){
         digitalWrite(UV_light_pin, HIGH);//relay 4 - UV Light
         UV_Light_status = 0;//used by the web-server interface to tell the user if the relay is on or off, 1 means on, 0 means off
       }
     } 
     wdt_reset();
     getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);//get the latest data from the RTC so the data file is named correctly
      root.rewind();
     if (data_log_enabled == 1 && ((SDUSEDSPACE() / volumesize) * 100) <99 && abs(((second + (minute*60) + (hour*3600)) - last_time_data_saved)) >= data_log_period){//is data logging enabled by the user and there is space available on the card?
       last_time_data_saved = (second + (minute*60) + (hour*3600));
       char newfile[12] = "";//variable for the log file name
       sprintf(newfile,"%.2d%.2d%.2d.htm", month, dayOfMonth, year);//generate the file name in the followin format "010513.htm" for a file created on January 13th, 2013
       if (counter != dayOfMonth){//if the day is a new day, then we need to add the header information to the log file
         counter = dayOfMonth;//set the counter to the current day so the system knows we have gon through this code before until the next day
         if (!file.open(root,newfile, O_READ)){//does the file already exist?
           file.open(root,newfile , O_CREAT | O_APPEND | O_WRITE); // no the file does not exist, lets create the file
           wdt_reset();
           //add the header information to the log file
           file.print(F("<Table border = \"1\">"
			"<tr>"
				"<td align = \"center\">Date</td>"
				"<td align = \"center\">Time</td>"
				"<td align = \"center\">Cold Side Ground Temperature</td>"
				"<td align = \"center\">Middle Ground Temperature</td>"
				"<td align = \"center\">Hot Side Ground Temperature</td>"
				"<td align = \"center\">Average Ambient Temperature</td>"
				"<td align = \"center\">Average Humidity</td>"
				"<td align = \"center\">Cold Side Ground Heater Status</td>"
				"<td align = \"center\">Middle Ground Heater Status</td>"
				"<td align = \"center\">Hot Side Ground Heater Status</td>"
				"<td align = \"center\">Humidifier Status</td>"
				"<td align = \"center\">Heat Lamp Status</td>"
				"<td align = \"center\">UV Light Status</td>"
			"</tr>"));
            file.close();
            wdt_reset();
         }else{
           //yes the file exists, close the file and move on
           file.close();
           wdt_reset();
         }
       }else{
         //we hae already created the file, now we need to append the log data to it as the headers are already written to it. 
         file.open(root,newfile , O_APPEND | O_WRITE); // Tested OK
         wdt_reset();
         file.print(F("<tr>"));
         file.print(F("<td align = \"center\">"));
         sprintf(newfile, "%.2d/%.2d/%.2d", month, dayOfMonth, year);
         file.print(newfile);
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         sprintf (newfile, "%.2d:%.2d:%.2d", hour, minute, second);
         file.print(newfile);
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         file.print(cold_side_temp_whole);
         file.print(".");
         file.print(cold_side_temp_fract);
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         file.print(middle_temp_whole);
         file.print(".");
         file.print(middle_temp_fract);
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         file.print(hot_side_temp_whole);
         wdt_reset();
         file.print(".");
         file.print(hot_side_temp_fract);
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         file.print((((float)ambient_temp_2_whole + (float)ambient_temp_1_whole + ((float)ambient_temp_2_fract / 100) + ((float)ambient_temp_1_fract / 100))/2));
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         file.print(RH);
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         if (cold_side_status == 1){
           file.print(F("ON"));
         }else{
           file.print(F("OFF"));
         }
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         wdt_reset();
         if (middle_status == 1){
           file.print(F("ON"));
         }else{
           file.print(F("OFF"));
         }
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         if (hot_side_status == 1){
           file.print(F("ON"));
         }else{
           file.print(F("OFF"));
         }
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         if (humidifier_status == 1){
           file.print(F("ON"));
         }else{
           file.print(F("OFF"));
         }
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         if (heat_lamp_status == 1){
           file.print(F("ON"));
         }else{
           file.print(F("OFF"));
         }
         file.print(F("</td>"));
         file.print(F("<td align = \"center\">"));
         if (UV_Light_status == 1){
           file.print(F("ON"));
         }else{
           file.print(F("OFF"));
         }
         file.print(F("</td>"));
         file.print("</tr>");
         file.close(); 
         wdt_reset();
       }
     } 
}

void GETETHERNET(void)
{
  getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);//get current time from RTC
  EthernetClient client = server.available();
  if (client)
  {
    // now client is connected to arduino we need to extract the
    // following fields from the HTTP request.
    int    nUriIndex;  // Gives the index into table of recognized URIs or -1 for not found.
    BUFFER requestContent;    // Request content as a null-terminated string.
    char * global_pUri = strtok(NULL, pSpDelimiters);
    MethodType eMethod = readHttpRequest(client, nUriIndex, requestContent, global_pUri);
    if (nUriIndex < 0)//is the URL NOT a system page (pages 1 through 8?) then we need to see if ther requested page is a file contained on the SD card
    {
       root.rewind();
       if (! file.open(root, strstr(global_pUri,"/")+1, O_READ)) {
         sendProgMemAsString(client, (char*)pgm_read_word(&(page_404[0])));//if there is no applicable file on the SD card, then we need to display a file/page not found error
       }else{
         //if the file is found, print out its contents to the web-browser
         int16_t c;
         byte clientBuf[128];
         byte clientCount = 0;
         if (strcmp(global_pUri,"/on.png")==0){
         }else if (strcmp(global_pUri,"/off.png")==0){
         }else if (strcmp(global_pUri,"/green.png")==0){
         }else if (strcmp(global_pUri,"/red.png")==0){
         }else{
           digitalWrite(cold_side_ground_pin, HIGH);
           digitalWrite(middle_side_ground_pin, HIGH);
           digitalWrite(hot_side_ground_pin, HIGH);//relay 1 - cold side ground
           cold_side_status = 0;
           hot_side_status = 0;
           middle_status = 0;
         }
         while ((c = file.read()) >= 0) {
           clientBuf[clientCount] = c;
           clientCount++;
           if(clientCount > 127)
           {
             client.write(clientBuf,128);
             clientCount = 0;
             wdt_reset();
           }
         }
         //final <128 byte cleanup packet
         if(clientCount > 0) client.write(clientBuf,clientCount);            
         // close the file:
         file.close();
       }
    }
    else if (nUriIndex < NUM_PAGES)
    {
      // Normal page request, may depend on content of the request;
      sendPage(client, nUriIndex, requestContent, global_pUri);
    }
    else
    {
      // Image request
      sendImage(client, nUriIndex, requestContent);
    }

    // give the web browser time to receive the data
    delay(1);
    client.stop();
  }
  //String temp="";
}

void CONVERT_TEMP(byte Sensor_PIN, byte & temp_whole, byte & temp_fract, byte & temp_status) {
  OneWire  dSensor1(Sensor_PIN); 
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  int HighByte, LowByte, TReading, SignBit, Tc_100;
  
  
  if ( !dSensor1.search(addr)) {
    dSensor1.reset_search();
    delay(250);
    temp_whole = 0;
    temp_fract = 0;
    temp_status = 0;
    if (Sensor_PIN==CSG1wirepin){
      cold_badsensorcount++;
      cold_badsensordate.yy=year+2000;
      cold_badsensordate.mm=month;
      cold_badsensordate.dd=dayOfMonth;
      cold_badsensordate.hh=hour;
      cold_badsensordate.min=minute;
      cold_badsensordate.ss=second;
    }else if (Sensor_PIN==MSG1wirepin){
      middle_badsensorcount++;
      middle_badsensordate.yy=year+2000;
      middle_badsensordate.mm=month;
      middle_badsensordate.dd=dayOfMonth;
      middle_badsensordate.hh=hour;
      middle_badsensordate.min=minute;
      middle_badsensordate.ss=second;
    }else if (Sensor_PIN==HSG1wirepin){
      hot_badsensorcount++;
      hot_badsensordate.yy=year+2000;
      hot_badsensordate.mm=month;
      hot_badsensordate.dd=dayOfMonth;
      hot_badsensordate.hh=hour;
      hot_badsensordate.min=minute;
      hot_badsensordate.ss=second;
    }else if (Sensor_PIN==AA11wirepin){
      ambient1_badsensorcount++;
      ambient1_badsensordate.yy=year+2000;
      ambient1_badsensordate.mm=month;
      ambient1_badsensordate.dd=dayOfMonth;
      ambient1_badsensordate.hh=hour;
      ambient1_badsensordate.min=minute;
      ambient1_badsensordate.ss=second;
    }else if (Sensor_PIN==AA21wirepin){
      ambient2_badsensorcount++;
      ambient2_badsensordate.yy=year+2000;
      ambient2_badsensordate.mm=month;
      ambient2_badsensordate.dd=dayOfMonth;
      ambient2_badsensordate.hh=hour;
      ambient2_badsensordate.min=minute;
      ambient2_badsensordate.ss=second;
    }
    if (abs(((second + (minute*60UL) + (hour*3600UL) + (dayOfMonth*86400UL) + (month*2629743UL) + (year*31556926UL)) - last_time_email_sent)) >= 3600){
           sendemail();
           wdt_reset();
           last_time_email_sent = second + (minute*60UL) + (hour*3600UL) + (dayOfMonth*86400UL) + (month*2629743UL) + (year*31556926UL);
         }
    return;
  }

  if ( OneWire::crc8( addr, 7) != addr[7]) {
      temp_whole = 0;
    temp_fract = 0;
    temp_status = 0;
    if (Sensor_PIN==CSG1wirepin){
      cold_badsensorcount++;
      cold_badsensordate.yy=year+2000;
      cold_badsensordate.mm=month;
      cold_badsensordate.dd=dayOfMonth;
      cold_badsensordate.hh=hour;
      cold_badsensordate.min=minute;
      cold_badsensordate.ss=second;
    }else if (Sensor_PIN==MSG1wirepin){
      middle_badsensorcount++;
      middle_badsensordate.yy=year+2000;
      middle_badsensordate.mm=month;
      middle_badsensordate.dd=dayOfMonth;
      middle_badsensordate.hh=hour;
      middle_badsensordate.min=minute;
      middle_badsensordate.ss=second;
    }else if (Sensor_PIN==HSG1wirepin){
      hot_badsensorcount++;
      hot_badsensordate.yy=year+2000;
      hot_badsensordate.mm=month;
      hot_badsensordate.dd=dayOfMonth;
      hot_badsensordate.hh=hour;
      hot_badsensordate.min=minute;
      hot_badsensordate.ss=second;
    }else if (Sensor_PIN==AA11wirepin){
      ambient1_badsensorcount++;
      ambient1_badsensordate.yy=year+2000;
      ambient1_badsensordate.mm=month;
      ambient1_badsensordate.dd=dayOfMonth;
      ambient1_badsensordate.hh=hour;
      ambient1_badsensordate.min=minute;
      ambient1_badsensordate.ss=second;
    }else if (Sensor_PIN==AA21wirepin){
      ambient2_badsensorcount++;
      ambient2_badsensordate.yy=year+2000;
      ambient2_badsensordate.mm=month;
      ambient2_badsensordate.dd=dayOfMonth;
      ambient2_badsensordate.hh=hour;
      ambient2_badsensordate.min=minute;
      ambient2_badsensordate.ss=second;
    }
    if (abs(((second + (minute*60UL) + (hour*3600UL) + (dayOfMonth*86400UL) + (month*2629743UL) + (year*31556926UL)) - last_time_email_sent)) >= 3600){
           sendemail();
           wdt_reset();
           last_time_email_sent = second + (minute*60UL) + (hour*3600UL) + (dayOfMonth*86400UL) + (month*2629743UL) + (year*31556926UL);
         }
    return;
  }

  // The DallasTemperature library can do all this work for you!

  dSensor1.reset();
  dSensor1.select(addr);
  dSensor1.write(0x44,0);         // start conversion, without parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a dSensor1.depower() here, but the reset will take care of it.
  
  present = dSensor1.reset();
  dSensor1.select(addr);    
  dSensor1.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = dSensor1.read();
  }

  LowByte = data[0];
  HighByte = data[1];
  TReading = (HighByte << 8) + LowByte;
  SignBit = TReading & 0x8000;  // test most sig bit
  if (SignBit) // negative
  {
    TReading = (TReading ^ 0xffff) + 1; // 2's comp
  }
  Tc_100 = (6 * TReading) + TReading / 4;    // multiply by (100 * 0.0625) or 6.25
  
  if (temp_scale == 1){//if using degrees F, the celcius reading from the sensor must be converted
     temp_whole = (int)(((Tc_100)*(1.8))+3200)/100;
     temp_fract = (int)(((Tc_100)*(1.8))+3200)%100;
     if (temp_whole > 32){
       temp_status = 1;//if the temperature is above 32 degrees F, then the sensor is OK
     }else{
       temp_status = 0;//the sensor must be bad as the temperature must never get down as low as 32 degrees F, the snake would die!!
     }
   }else{//if using degrees C, the temp reading is good as is.
     temp_whole = Tc_100/100;
     temp_fract = Tc_100%100;
     if (temp_whole > 0){
       temp_status = 1;//if the temperature is above 0 degrees C, then the sensor is OK
     }else{
       temp_status = 0;//the sensor must be bad as the temperature must never get down as low as 0 degrees C
     }
   }
}

/**********************************************************************************************************************
*                                              Method for read HTTP Header Request from web client
*
* The HTTP request format is defined at http://www.w3.org/Protocols/HTTP/1.0/spec.html#Message-Types
* and shows the following structure:
*  Full-Request and Full-Response use the generic message format of RFC 822 [7] for transferring entities. Both messages may include optional header fields
*  (also known as "headers") and an entity body. The entity body is separated from the headers by a null line (i.e., a line with nothing preceding the CRLF).
*      Full-Request   = Request-Line       
*                       *( General-Header 
*                        | Request-Header  
*                        | Entity-Header ) 
*                       CRLF
*                       [ Entity-Body ]    
*
* The Request-Line begins with a method token, followed by the Request-URI and the protocol version, and ending with CRLF. The elements are separated by SP characters.
* No CR or LF are allowed except in the final CRLF sequence.
*      Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
* HTTP header fields, which include General-Header, Request-Header, Response-Header, and Entity-Header fields, follow the same generic format.
* Each header field consists of a name followed immediately by a colon (":"), a single space (SP) character, and the field value.
* Field names are case-insensitive. Header fields can be extended over multiple lines by preceding each extra line with at least one SP or HT, though this is not recommended.     
*      HTTP-header    = field-name ":" [ field-value ] CRLF
***********************************************************************************************************************/
// Read HTTP request, setting Uri Index, the requestContent and returning the method type.
MethodType readHttpRequest(EthernetClient & client, int & nUriIndex, BUFFER & requestContent, char * & global_pUri)
{
  BUFFER readBuffer;    // Just a work buffer into which we can read records
  int nContentLength = 0;
  bool bIsUrlEncoded;

  requestContent[0] = 0;    // Initialize as an empty string
  global_pUri = strtok(NULL, pSpDelimiters);
  // Read the first line: Request-Line setting Uri Index and returning the method type.
  MethodType eMethod = readRequestLine(client, readBuffer, nUriIndex, requestContent, global_pUri);
  // Read any following, non-empty headers setting content length.
  readRequestHeaders(client, readBuffer, nContentLength, bIsUrlEncoded);

  if (nContentLength > 0)
  {
  // If there is some content then read it and do an elementary decode.
    readEntityBody(client, nContentLength, requestContent);
    if (bIsUrlEncoded)
    {
    // The '+' encodes for a space, so decode it within the string
    for (char * pChar = requestContent; (pChar = strchr(pChar, '+')) != NULL; )
      *pChar = ' ';    // Found a '+' so replace with a space
    }
  }

  return eMethod;
}

// Read the first line of the HTTP request, setting Uri Index and returning the method type.
// If it is a GET method then we set the requestContent to whatever follows the '?'. For a other
// methods there is no content except it may get set later, after the headers for a POST method.
MethodType readRequestLine(EthernetClient & client, BUFFER & readBuffer, int & nUriIndex, BUFFER & requestContent, char * & global_pUri)
{
  MethodType eMethod;
  // Get first line of request:
  // Request-Line = Method SP Request-URI SP HTTP-Version CRLF
  getNextHttpLine(client, readBuffer);
  // Split it into the 3 tokens
  char * pMethod  = strtok(readBuffer, pSpDelimiters);
  char * pUri     = strtok(NULL, pSpDelimiters);
  char * pVersion = strtok(NULL, pSpDelimiters);
  global_pUri     = strtok(NULL, pSpDelimiters);
  // URI may optionally comprise the URI of a queryable object a '?' and a query
  // see http://www.ietf.org/rfc/rfc1630.txt
  strtok(pUri, "?");
  char * pQuery   = strtok(NULL, "?");
  if (pQuery != NULL)
  {
    strcpy(requestContent, pQuery);
    // The '+' encodes for a space, so decode it within the string
    for (pQuery = requestContent; (pQuery = strchr(pQuery, '+')) != NULL; )
      *pQuery = ' ';    // Found a '+' so replace with a space


  }
  if (strcmp(pMethod, "GET") == 0){
    eMethod = MethodGet;
    global_pUri = subStr(pUri, (char *)" ", 1);
  }else if (strcmp(pMethod, "POST") == 0){
    eMethod = MethodPost;
  }else if (strcmp(pMethod, "HEAD") == 0){
    eMethod = MethodHead;
  }else{
    eMethod = MethodUnknown;
  }

  // See if we recognize the URI and get its index
  nUriIndex = GetUriIndex(pUri);
  return eMethod;
}

// Read each header of the request till we get the terminating CRLF
void readRequestHeaders(EthernetClient & client, BUFFER & readBuffer, int & nContentLength, bool & bIsUrlEncoded)
{
  nContentLength = 0;      // Default is zero in cate there is no content length.
  bIsUrlEncoded  = true;   // Default encoding
  // Read various headers, each terminated by CRLF.
  // The CRLF gets removed and the buffer holds each header as a string.
  // An empty header of zero length terminates the list.
  do
  {
    getNextHttpLine(client, readBuffer);
    // Process a header. We only need to extract the (optionl) content
    // length for the binary content that follows all these headers.
    // General-Header = Date | Pragma
    // Request-Header = Authorization | From | If-Modified-Since | Referer | User-Agent
    // Entity-Header  = Allow | Content-Encoding | Content-Length | Content-Type
    //                | Expires | Last-Modified | extension-header
    // extension-header = HTTP-header
    //       HTTP-header    = field-name ":" [ field-value ] CRLF
    //       field-name     = token
    //       field-value    = *( field-content | LWS )
    //       field-content  = <the OCTETs making up the field-value
    //                        and consisting of either *TEXT or combinations
    //                        of token, tspecials, and quoted-string>
    char * pFieldName  = strtok(readBuffer, pSpDelimiters);
    char * pFieldValue = strtok(NULL, pSpDelimiters);

    if (strcmp(pFieldName, "Content-Length:") == 0)
    {
      nContentLength = atoi(pFieldValue);
    }
    else if (strcmp(pFieldName, "Content-Type:") == 0)
    {
      if (strcmp(pFieldValue, "application/x-www-form-urlencoded") != 0)
        bIsUrlEncoded = false;
    }
    //wdt_reset();
  } while (strlen(readBuffer) > 0);    // empty string terminates
}

// Read the entity body of given length (after all the headers) into the buffer.
void readEntityBody(EthernetClient & client, int nContentLength, BUFFER & content)
{
  int i;
  char c;

  if (nContentLength >= sizeof(content))
    nContentLength = sizeof(content) - 1;  // Should never happen!

  for (i = 0; i < nContentLength; ++i)
  {
    c = client.read();
    content[i] = c;
  }

  content[nContentLength] = 0;  // Null string terminator
}

// See if we recognize the URI and get its index; or -1 if we don't recognize it.
int GetUriIndex(char * pUri)
{
  int nUriIndex = -1;

  // select the page from the buffer (GET and POST) [start]
  for (int i = 0; i < NUM_URIS; i++)
  {
    if (strcmp_P(pUri, (const char *)pgm_read_word(&(http_uris[i]))) == 0)
    {
      nUriIndex = i;
      break;
    }
  }

  return nUriIndex;
}

/**********************************************************************************************************************
* Read the next HTTP header record which is CRLF delimited.  We replace CRLF with string terminating null.
***********************************************************************************************************************/
void getNextHttpLine(EthernetClient & client, BUFFER & readBuffer)
{
  char c;
  int bufindex = 0; // reset buffer

  // reading next header of HTTP request
  if (client.connected() && client.available())
  {
    // read a line terminated by CRLF
    readBuffer[0] = client.read();
    readBuffer[1] = client.read();
    bufindex = 2;
    for (int i = 2; readBuffer[i - 2] != '\r' && readBuffer[i - 1] != '\n'; ++i)
    {
      // read full line and save it in buffer, up to the buffer size
      c = client.read();
      if (bufindex < sizeof(readBuffer)){
        readBuffer[bufindex++] = c;
      }
        
    }
    readBuffer[bufindex - 2] = 0;  // Null string terminator overwrites '\r'
    
  }
}

/**********************************************************************************************************************
*                                                              Send Pages
       Full-Response  = Status-Line         
                        *( General-Header   
                         | Response-Header 
                         | Entity-Header ) 
                        CRLF
                        [ Entity-Body ]   

       Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
       General-Header = Date | Pragma
       Response-Header = Location | Server | WWW-Authenticate
       Entity-Header  = Allow | Content-Encoding | Content-Length | Content-Type
                      | Expires | Last-Modified | extension-header
*
***********************************************************************************************************************/
void sendPage(EthernetClient & client, int nUriIndex, BUFFER & requestContent, char * global_pUri)
{
  if (nUriIndex < NUM_PAGES){    
    /***************************************************************
    //PAGE 2 DATA PROCESSING
    ***************************************************************/
    
    if (nUriIndex == 1){//page 2
      if (strncmp(&requestContent[0],"1",1)==0 && strncmp(&requestContent[1],"=",1)==0){ //tests if the page has had a form submitted with data or not
          BUFFER valueArray;
          for (byte x = 1; x<18; x++){ //page 2 contains 17 data entry text boxes, so we want to run through all of them
            PROCESSREQUESTCONTENT(x, requestContent, valueArray);
              if (x == 1){      //if the currently processing entry is entry 1
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){    //is the entred entry within the allowable bounds?
                  INCSGDTON = atoi(valueArray);    //converts the string value of the entred data into a interger
                  if (EEPROM.read(INCSGDTONEEPROMADDR)!=INCSGDTON){    //if the user has not changed the entry from what is already in memory, then we do not want to save to EEPROM again as the EEPROM has a shorter life time
                    EEPROM.write(INCSGDTONEEPROMADDR, INCSGDTON);    //since the user has entered a new number, we now need to save this to memory
                    INCSGDTON_incorrect = false;    //because a valid entry was entered, we do not want the web-page to display "Invalid Entry!"
                  }
                }else{
                  INCSGDTON_incorrect = true;//if the user did enter an invalid value then we want the web-page to display "invalid entry!" when the substitution function runs
                }
              }else if (x == 2){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  CSGDTOFF = atoi(valueArray);
                  if (EEPROM.read(CSGDTOFFEEPROMADDR)!=CSGDTOFF){
                    EEPROM.write(CSGDTOFFEEPROMADDR, CSGDTOFF);
                    CSGDTOFF_incorrect = false;
                  }
                }else{
                  CSGDTOFF_incorrect = true;
                }
              }else if (x == 3){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  MSGDTON = atoi(valueArray);
                  if (EEPROM.read(MSGDTONEEPROMADDR)!=MSGDTON){
                    EEPROM.write(MSGDTONEEPROMADDR, MSGDTON);
                    MSGDTON_incorrect = false;
                  }
                }else{
                  MSGDTON_incorrect = true;
                }
              }else if (x == 4){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  MSGDTOFF = atoi(valueArray);
                  if (EEPROM.read(MSGDTOFFEEPROMADDR)!=MSGDTOFF){
                    EEPROM.write(MSGDTOFFEEPROMADDR, MSGDTOFF);
                    MSGDTOFF_incorrect = false;
                  }
                }else{
                  MSGDTOFF_incorrect = true;
                }
              }else if (x == 5){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  HSGDTON = atoi(valueArray);
                  if (EEPROM.read(HSGDTONEEPROMADDR)!=HSGDTON){
                    EEPROM.write(HSGDTONEEPROMADDR, HSGDTON);
                    HSGDTON_incorrect = false;
                  }
                }else{
                  HSGDTON_incorrect = true;
                }
              }else if (x == 6){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  HSGDTOFF = atoi(valueArray);
                  if (EEPROM.read(HSGDTOFFEEPROMADDR)!=HSGDTOFF){
                    EEPROM.write(HSGDTOFFEEPROMADDR, HSGDTOFF);
                    HSGDTOFF_incorrect = false;
                  }
                }else{
                  HSGDTOFF_incorrect = true;
                }
              }else if (x == 7){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  AADTON = atoi(valueArray);
                  if (EEPROM.read(AADTONEEPROMADDR)!=AADTON){
                    EEPROM.write(AADTONEEPROMADDR, AADTON);
                    AADTON_incorrect = false;
                  }
                }else{
                  AADTON_incorrect = true;
                }
              }else if (x == 8){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  AADTOFF = atoi(valueArray);
                  if (EEPROM.read(AADTOFFEEPROMADDR)!=AADTOFF){
                    EEPROM.write(AADTOFFEEPROMADDR, AADTOFF);
                    AADTOFF_incorrect = false;
                  }
                }else{
                  AADTOFF_incorrect = true;
                }
              }else if (x == 9){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  CSGNTON = atoi(valueArray);
                  if (EEPROM.read(CSGNTONEEPROMADDR)!=CSGNTON){
                    EEPROM.write(CSGNTONEEPROMADDR, CSGNTON);
                    CSGNTON_incorrect = false;
                  }
                }else{
                  CSGNTON_incorrect = true;
                }
              }else if (x == 10){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  CSGNOFF = atoi(valueArray);
                  if (EEPROM.read(CSGNOFFEEPROMADDR)!=CSGNOFF){
                    EEPROM.write(CSGNOFFEEPROMADDR, CSGNOFF);
                    CSGNOFF_incorrect = false;
                  }
                }else{
                  CSGNOFF_incorrect = true;
                }
               }else if (x == 11){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  MSGNTON = atoi(valueArray);
                  if (EEPROM.read(MSGNTONEEPROMADDR)!=MSGNTON){
                    EEPROM.write(MSGNTONEEPROMADDR, MSGNTON);
                    MSGNTON_incorrect = false;
                  }
                }else{
                  MSGNTON_incorrect = true;
                }
              }else if (x == 12){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  MSGNTOFF = atoi(valueArray);
                  if (EEPROM.read(MSGNTOFFEEPROMADDR)!=MSGNTOFF){
                    EEPROM.write(MSGNTOFFEEPROMADDR, MSGNTOFF);
                    MSGNTOFF_incorrect = false;
                  }
                }else{
                  MSGNTOFF_incorrect = true;
                }
              }else if (x == 13){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  HSGNTON = atoi(valueArray);
                  if (EEPROM.read(HSGNTONEEPROMADDR)!=HSGNTON){
                    EEPROM.write(HSGNTONEEPROMADDR, HSGNTON);
                    HSGNTON_incorrect = false;
                  }
                }else{
                  HSGNTON_incorrect = true;
                }
              }else if (x == 14){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  HSGNTOFF = atoi(valueArray);
                  if (EEPROM.read(HSGNTOFFEEPROMADDR)!=HSGNTOFF){
                    EEPROM.write(HSGNTOFFEEPROMADDR, HSGNTOFF);
                    HSGNTOFF_incorrect = false;
                  }
                }else{
                  HSGNTOFF_incorrect = true;
                }
              }else if (x == 15){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  AANTON = atoi(valueArray);
                  if (EEPROM.read(AANTONEEPROMADDR)!=AANTON){
                    EEPROM.write(AANTONEEPROMADDR, AANTON);
                    AANTON_incorrect = false;
                  }
                }else{
                  AANTON_incorrect = true;
                }
              }else if (x == 16){
                if (atoi(valueArray)>= min_temp_setting && atoi(valueArray) <= max_temp_setting){
                  AANTOFF = atoi(valueArray);
                  if (EEPROM.read(AANTOFFEEPROMADDR)!=AANTOFF){
                    EEPROM.write(AANTOFFEEPROMADDR, AANTOFF);
                    AANTOFF_incorrect = false;
                  }
                 }else{
                    AANTOFF_incorrect = true;
                 }
            }else if (x== 17){
              temp_scale = atoi(valueArray);
              if (EEPROM.read(TEMPSCALEEEPROMADDR)!=temp_scale){
                EEPROM.write(TEMPSCALEEEPROMADDR, temp_scale);
                  if (temp_scale == 1){
                    max_temp_setting = 120;
                    min_temp_setting = 20;
                  }else{
                    max_temp_setting = 49;
                    min_temp_setting = 0;
                  }
              }
            }
          }
      }
    }
   
    /***************************************************************
    //PAGE 3 DATA PROCESSING
    ***************************************************************/
    if (nUriIndex == 2){//page 3  
      if (strncmp(&requestContent[0],"1",1)==0 && strncmp(&requestContent[1],"=",1)==0){ //tests if the page has had a form submitted with data or not
      BUFFER valueArray;
          for (byte x = 1; x<5; x++){
            PROCESSREQUESTCONTENT(x, requestContent, valueArray);
            if (x<5){
              if (x == 1){
                if (atoi(valueArray)>9 && atoi(valueArray) <91){
                  DTHUMON = atoi(valueArray);
                  if (EEPROM.read(DTHUMONEEPROMADDR)!=DTHUMON){
                    EEPROM.write(DTHUMONEEPROMADDR, DTHUMON);
                    DTHUMON_incorrect = false;
                  }
                }else{
                  DTHUMON_incorrect = true;
                }
              }else if (x == 2){
                if (atoi(valueArray)>9 && atoi(valueArray) <91){
                  DTHUMOFF = atoi(valueArray);
                  if (EEPROM.read(DTHUMOFFEEPROMADDR)!=DTHUMOFF){
                    EEPROM.write(DTHUMOFFEEPROMADDR, DTHUMOFF);
                    DTHUMOFF_incorrect = false;
                  }
                }else{
                  DTHUMOFF_incorrect = true;
                }
              }else if (x == 3){
                if (atoi(valueArray)>9 && atoi(valueArray) <91){
                  NTHUMON = atoi(valueArray);
                  if (EEPROM.read(NTHUMONEEPROMADDR)!=NTHUMON){
                    EEPROM.write(NTHUMONEEPROMADDR, NTHUMON);
                    NTHUMON_incorrect = false;
                  }
                }else{
                  NTHUMON_incorrect = true;
                }
              }else if (x==4){
                if (atoi(valueArray)>9 && atoi(valueArray) <91){
                  NTHUMOFF = atoi(valueArray);
                  if (EEPROM.read(NTHUMOFFEEPROMADDR)!=NTHUMOFF){
                    EEPROM.write(NTHUMOFFEEPROMADDR, NTHUMOFF);
                    NTHUMOFF_incorrect = false;
                  }
                }else{
                  NTHUMOFF_incorrect = true;
                }
              }
            }
          }
      }
    }
   
    
    /***************************************************************
    //PAGE 4 DATA PROCESSING
    ***************************************************************/
    if (nUriIndex == 3){//page 4 
      if (strncmp(&requestContent[0],"1",1)==0 && strncmp(&requestContent[1],"=",1)==0){ //tests if the page has had a form submitted with data or not
          BUFFER valueArray;
          for (byte x = 1; x<= 12; x++){
            PROCESSREQUESTCONTENT(x, requestContent, valueArray);
              if (x == 1){
                SETMONTH = atoi(valueArray);
              }else if (x == 2){
                SETDAY = atoi(valueArray);
              }else if (x == 3){
                SETYEAR = atoi(valueArray);
              }else if (x == 4){
                SETDAYOFWEEK = atoi(valueArray); 
              }else if (x == 5){ 
                  EEPROM.write(TIMEZONEEEPROMADDR, abs(atoi(valueArray)));
                  if (atoi(valueArray) <0){
                    EEPROM.write(TIMEZONEEEPROMADDRSIGN, 1);
                  }else{
                    EEPROM.write(TIMEZONEEEPROMADDRSIGN, 0);
                  }
                  TIMEZONE = atoi(valueArray);
                  GET_NTP_TIME(TIMEZONE);
                  if (SETSECONDS != 0 && SETMINUTE != 0 && SETHOUR != 0){
                    setDateDs1307(SETSECONDS, SETMINUTE, SETHOUR, SETDAYOFWEEK, SETDAY, SETMONTH, SETYEAR);
                    getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);  
                  }              
              }else if (x == 6){
                if (atoi(valueArray) == 1){
                  getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
                  GET_NTP_TIME(TIMEZONE);
                }
                  if (SETSECONDS != 0 && SETMINUTE != 0 && SETHOUR != 0){
                    setDateDs1307(SETSECONDS, SETMINUTE, SETHOUR, SETDAYOFWEEK, SETDAY, SETMONTH, SETYEAR);
                    getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
                  }
              }else if (x == 7){
                 if (atoi(valueArray) >= 0 && atoi(valueArray) <= 24){ 
                   if (atoi(valueArray) != UVLIGHTONHOUR){
                     UVLIGHTONHOUR = atoi(valueArray);
                     EEPROM.write(UVLIGHTONHOUREEPROMADDR, UVLIGHTONHOUR);
                     UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = false;
                   }
                 }else{
                    UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = true;
                  }
              }else if (x == 8){
                if (atoi(valueArray) >= 0 && atoi(valueArray) <= 60){ 
                   if (atoi(valueArray) != UVLIGHTONHOUR){
                     UVLIGHTONMINUTE = atoi(valueArray);
                     EEPROM.write(UVLIGHTONMINUTEEEPROMADDR, UVLIGHTONMINUTE);
                     UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = false;
                   }
                 }else{
                    UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = true;
                  }
              }else if (x == 9){
                if (atoi(valueArray) >= 0 && atoi(valueArray) <= 60){
                  if (atoi(valueArray) != UVLIGHTONSECOND){
                     UVLIGHTONSECOND = atoi(valueArray);
                     EEPROM.write(UVLIGHTONSECONDEEPROMADDR, UVLIGHTONSECOND);
                     UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = false;
                  }
                }else{
                   UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = true;
                }
              }else if (x == 10){
                if (atoi(valueArray) >= 0 && atoi(valueArray) <= 24){
                  if (atoi(valueArray) != UVLIGHTOFFHOUR){
                    UVLIGHTOFFHOUR = atoi(valueArray);
                    EEPROM.write(UVLIGHTOFFHOUREEPROMADDR, UVLIGHTOFFHOUR);
                    UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = false;
                  }
                }else{
                   UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = true;
                }
              }else if (x == 11){
                if (atoi(valueArray) >= 0 && atoi(valueArray) <= 24){
                  if (atoi(valueArray) != UVLIGHTOFFMINUTE){
                     UVLIGHTOFFMINUTE = atoi(valueArray);
                     EEPROM.write(UVLIGHTOFFMINUTEEEPROMADDR, UVLIGHTOFFMINUTE);
                     UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = false;
                  }
                }else{
                   UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = true;
                }
              }else if (x == 12){
                if (atoi(valueArray) >= 0 && atoi(valueArray) <= 24){
                  if (atoi(valueArray) != UVLIGHTOFFSECOND){
                     UVLIGHTOFFSECOND = atoi(valueArray);
                     EEPROM.write(UVLIGHTOFFSECONDEEPROMADDR, UVLIGHTOFFSECOND);
                     UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = false;
                  }
                }else{
                   UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = true;
                }
              }
          }
      }
    }
      /***************************************************************
    //PAGE 5 DATA PROCESSING
    ***************************************************************/
    if (nUriIndex == 4){//page 5 
      if (strncmp(&requestContent[0],"d",1)==0 && strncmp(&requestContent[1],"e",1)==0){
        file.remove(root, strstr(requestContent,"=")+1);
      }else if (strncmp(&requestContent[0],"1",1)==0 && strncmp(&requestContent[1],"=",1)==0){ //tests if the page has had a form submitted with data or not
          BUFFER valueArray;
          for (byte x = 1; x<=2; x++){
            PROCESSREQUESTCONTENT(x, requestContent, valueArray);
            if (x<=9){
              if (x == 1){
                  data_log_enabled = atoi(valueArray);
                  counter = 0;
              }else if (x == 2){
                  data_log_period = atoi(valueArray);
                  counter = 0;
              }
            }
          }
      }
    }

  
    /***************************************************************
    //PAGE 7 DATA PROCESSING
    ***************************************************************/
    if (nUriIndex == 6){//page 7  
    
      if(strncmp(&requestContent[0],"C",1)==0 && strncmp(&requestContent[1],"S",1)==0 && strncmp(&requestContent[2],"G",1)==0
         && strncmp(&requestContent[3],"O",1)==0 && strncmp(&requestContent[4],"N",1)==0 && strncmp(&requestContent[5],"=",1)==0
         && strncmp(&requestContent[6],"O",1)==0 && strncmp(&requestContent[7],"n",1)==0){
         digitalWrite(cold_side_ground_pin, LOW);//relay 1 - cold side ground
         cold_side_status = 1;
      }else if(strncmp(&requestContent[0],"C",1)==0 && strncmp(&requestContent[1],"S",1)==0 && strncmp(&requestContent[2],"G",1)==0
         && strncmp(&requestContent[3],"O",1)==0 && strncmp(&requestContent[4],"N",1)==0 && strncmp(&requestContent[5],"=",1)==0
         && strncmp(&requestContent[6],"O",1)==0 && strncmp(&requestContent[7],"f",1)==0 && strncmp(&requestContent[8],"f",1)==0){
         digitalWrite(cold_side_ground_pin, HIGH);//relay 1 - cold side ground
         cold_side_status = 0;
      }
      if(strncmp(&requestContent[0],"M",1)==0 && strncmp(&requestContent[1],"S",1)==0 && strncmp(&requestContent[2],"G",1)==0
         && strncmp(&requestContent[3],"O",1)==0 && strncmp(&requestContent[4],"N",1)==0 && strncmp(&requestContent[5],"=",1)==0
         && strncmp(&requestContent[6],"O",1)==0 && strncmp(&requestContent[7],"n",1)==0){
         digitalWrite(middle_side_ground_pin, LOW);//relay 1 - cold side ground
         middle_status = 1;
      }else if(strncmp(&requestContent[0],"M",1)==0 && strncmp(&requestContent[1],"S",1)==0 && strncmp(&requestContent[2],"G",1)==0
         && strncmp(&requestContent[3],"O",1)==0 && strncmp(&requestContent[4],"N",1)==0 && strncmp(&requestContent[5],"=",1)==0
         && strncmp(&requestContent[6],"O",1)==0 && strncmp(&requestContent[7],"f",1)==0 && strncmp(&requestContent[8],"f",1)==0){
         digitalWrite(middle_side_ground_pin, HIGH);//relay 1 - cold side ground
         middle_status = 0;
      }
      if(strncmp(&requestContent[0],"H",1)==0 && strncmp(&requestContent[1],"S",1)==0 && strncmp(&requestContent[2],"G",1)==0
         && strncmp(&requestContent[3],"O",1)==0 && strncmp(&requestContent[4],"N",1)==0 && strncmp(&requestContent[5],"=",1)==0
         && strncmp(&requestContent[6],"O",1)==0 && strncmp(&requestContent[7],"n",1)==0){
         digitalWrite(hot_side_ground_pin, LOW);//relay 1 - cold side ground
         hot_side_status = 1;
      }else if(strncmp(&requestContent[0],"H",1)==0 && strncmp(&requestContent[1],"S",1)==0 && strncmp(&requestContent[2],"G",1)==0
         && strncmp(&requestContent[3],"O",1)==0 && strncmp(&requestContent[4],"N",1)==0 && strncmp(&requestContent[5],"=",1)==0
         && strncmp(&requestContent[6],"O",1)==0 && strncmp(&requestContent[7],"f",1)==0 && strncmp(&requestContent[8],"f",1)==0){
         digitalWrite(hot_side_ground_pin, HIGH);//relay 1 - cold side ground
         hot_side_status = 0;
      }
      if(strncmp(&requestContent[0],"H",1)==0 && strncmp(&requestContent[1],"L",1)==0 && strncmp(&requestContent[2],"O",1)==0
         && strncmp(&requestContent[3],"N",1)==0 && strncmp(&requestContent[4],"=",1)==0
         && strncmp(&requestContent[5],"O",1)==0 && strncmp(&requestContent[6],"n",1)==0){
         digitalWrite(heat_lamp_pin, LOW);//relay 1 - cold side ground
         heat_lamp_status = 1;
      }else if(strncmp(&requestContent[0],"H",1)==0 && strncmp(&requestContent[1],"L",1)==0 && strncmp(&requestContent[2],"O",1)==0
         && strncmp(&requestContent[3],"N",1)==0 && strncmp(&requestContent[4],"=",1)==0
         && strncmp(&requestContent[5],"O",1)==0 && strncmp(&requestContent[6],"f",1)==0 && strncmp(&requestContent[7],"f",1)==0){
         digitalWrite(heat_lamp_pin, HIGH);//relay 1 - cold side ground
         heat_lamp_status = 0;
      }
      if(strncmp(&requestContent[0],"U",1)==0 && strncmp(&requestContent[1],"V",1)==0 && strncmp(&requestContent[2],"L",1)==0
         && strncmp(&requestContent[3],"O",1)==0 && strncmp(&requestContent[4],"N",1)==0
         && strncmp(&requestContent[5],"=",1)==0 && strncmp(&requestContent[6],"O",1)==0 && strncmp(&requestContent[7],"n",1)==0){
         digitalWrite(UV_light_pin, LOW);//relay 1 - cold side ground
         UV_Light_status = 1;
      }else if(strncmp(&requestContent[0],"U",1)==0 && strncmp(&requestContent[1],"V",1)==0 && strncmp(&requestContent[2],"L",1)==0
         && strncmp(&requestContent[3],"O",1)==0 && strncmp(&requestContent[4],"N",1)==0 && strncmp(&requestContent[5],"=",1)==0
         && strncmp(&requestContent[6],"O",1)==0 && strncmp(&requestContent[7],"f",1)==0 && strncmp(&requestContent[8],"f",1)==0){
         digitalWrite(UV_light_pin, HIGH);//relay 1 - cold side ground
         UV_Light_status = 0;
      }
      if(strncmp(&requestContent[0],"H",1)==0 && strncmp(&requestContent[1],"U",1)==0 && strncmp(&requestContent[2],"M",1)==0
         && strncmp(&requestContent[3],"O",1)==0 && strncmp(&requestContent[4],"N",1)==0
         && strncmp(&requestContent[5],"=",1)==0 && strncmp(&requestContent[6],"O",1)==0 && strncmp(&requestContent[7],"n",1)==0){
         digitalWrite(humidifier_pin, LOW);//relay 1 - cold side ground
         humidifier_status = 1;
      }else if(strncmp(&requestContent[0],"H",1)==0 && strncmp(&requestContent[1],"U",1)==0 && strncmp(&requestContent[2],"M",1)==0
         && strncmp(&requestContent[3],"O",1)==0 && strncmp(&requestContent[4],"N",1)==0 && strncmp(&requestContent[5],"=",1)==0
         && strncmp(&requestContent[6],"O",1)==0 && strncmp(&requestContent[7],"f",1)==0 && strncmp(&requestContent[8],"f",1)==0){
         digitalWrite(humidifier_pin, HIGH);//relay 1 - cold side ground
         humidifier_status = 0;
         }
  
      if (strncmp(&requestContent[0],"1",1)==0 && strncmp(&requestContent[1],"=",1)==0){ //tests if the page has had a form submitted with data or not
          BUFFER valueArray;
          for (byte x = 1; x<=6; x++){
            PROCESSREQUESTCONTENT(x, requestContent, valueArray);
            if (x<=6){
              if (x == 1){
                CSGMANUAL = atoi(valueArray);
              }else if (x == 2){
                MSGMANUAL = atoi(valueArray);
              }else if (x == 3){
                HSGMANUAL = atoi(valueArray);
              }else if (x==4){
                HEATLAMPMANUAL = atoi(valueArray);
              }else if (x==5){
                UVLIGHTMANUAL = atoi(valueArray);
              }else if (x==6){
                HUMIDIFIERMANUAL = atoi(valueArray);
              }
            }
          }
      }
    }
   
  
    
      /***************************************************************
    //PAGE 8 DATA PROCESSING
    ***************************************************************/
    if (nUriIndex == 7){//page 8
      
      if (strncmp(&requestContent[0],"1",1)==0 && strncmp(&requestContent[1],"=",1)==0){ //tests if the page has had a form submitted with data or not
          byte set1, set2, set3, set4;
          BUFFER valueArray;
          for (byte x = 1; x<=17; x++){
            PROCESSREQUESTCONTENT(x, requestContent, valueArray); 
            if (x<=9){
              if (x == 1){
                if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  localip[0] = atoi(valueArray);
                  if (EEPROM.read(LOCALIPADDREEPROMADDRPART1)!=localip[0]){
                    EEPROM.write(LOCALIPADDREEPROMADDRPART1, localip[0]);
                    localip_incorrect = false;
                  }
                }else{
                  localip_incorrect = true;
                }
              }else if (x == 2){
                if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  localip[1] = atoi(valueArray);
                  if (EEPROM.read(LOCALIPADDREEPROMADDRPART2)!=localip[1]){
                    EEPROM.write(LOCALIPADDREEPROMADDRPART2, localip[1]);
                    localip_incorrect = false;
                  }
                }else{
                  localip_incorrect = true;
                }
              }else if (x == 3){
                if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  localip[2] = atoi(valueArray);
                  if (EEPROM.read(LOCALIPADDREEPROMADDRPART3)!=localip[2]){
                    EEPROM.write(LOCALIPADDREEPROMADDRPART3, localip[2]);
                    localip_incorrect = false;
                  }
                }else{
                  localip_incorrect = true;
                }
              }else if (x==4){
                if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  localip[3] = atoi(valueArray);
                  if (EEPROM.read(LOCALIPADDREEPROMADDRPART4)!=localip[3]){
                    EEPROM.write(LOCALIPADDREEPROMADDRPART4, localip[3]);
                    localip_incorrect = false;
                  }
                }else{
                  localip_incorrect = true;
                }
              }else if (x==5){
                if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  subnetmask[0] = atoi(valueArray);
                  if (EEPROM.read(SUBNETMASKEEPROMADDRPART1)!=subnetmask[0]){
                    EEPROM.write(SUBNETMASKEEPROMADDRPART1, subnetmask[0]);
                    subnetmask_incorrect = false;
                  }
                }else{
                  subnetmask_incorrect = true;
                }
              }else if (x==6){
                if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  subnetmask[1] = atoi(valueArray);
                  if (EEPROM.read(SUBNETMASKEEPROMADDRPART2)!=subnetmask[1]){
                    EEPROM.write(SUBNETMASKEEPROMADDRPART2, subnetmask[1]);
                    subnetmask_incorrect = false;
                  }
                }else{
                  subnetmask_incorrect = true;
                }
              }else if (x==7){
                if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  subnetmask[2] = atoi(valueArray);
                  if (EEPROM.read(SUBNETMASKEEPROMADDRPART3)!=subnetmask[2]){
                    EEPROM.write(SUBNETMASKEEPROMADDRPART3, subnetmask[2]);
                    subnetmask_incorrect = false;
                  }
                }else{
                  subnetmask_incorrect = true;
                }
              }else if (x==8){
                if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  subnetmask[3] = atoi(valueArray);
                  if (EEPROM.read(SUBNETMASKEEPROMADDRPART4)!=subnetmask[3]){
                    EEPROM.write(SUBNETMASKEEPROMADDRPART4, subnetmask[3]);
                    subnetmask_incorrect = false;
                  }
                }else{
                  subnetmask_incorrect = true;
                }
              }else if (x==9){
                if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  gateway[0] = atoi(valueArray);
                  if (EEPROM.read(GATEWAYEEPROMADDRPART1)!=gateway[0]){
                    EEPROM.write(GATEWAYEEPROMADDRPART1, gateway[0]);
                    gateway_incorrect = false;
                  }
                }else{
                  gateway_incorrect = true;
                }
              }
            }else if (x >= 10){
              if (x == 10){
                  if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  gateway[1] = atoi(valueArray);
                  if (EEPROM.read(GATEWAYEEPROMADDRPART2)!=gateway[1]){
                    EEPROM.write(GATEWAYEEPROMADDRPART2, gateway[1]);
                    gateway_incorrect = false;
                  }
                }else{
                  gateway_incorrect = true;
                }
              }else if (x == 11){
                  if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  gateway[2] = atoi(valueArray);
                  if (EEPROM.read(GATEWAYEEPROMADDRPART3)!=gateway[2]){
                    EEPROM.write(GATEWAYEEPROMADDRPART3, gateway[2]);
                    gateway_incorrect = false;
                  }
                }else{
                  gateway_incorrect = true;
                }
              }else if (x == 12){
                  if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  gateway[3] = atoi(valueArray);
                  if (EEPROM.read(GATEWAYEEPROMADDRPART4)!=gateway[3]){
                    EEPROM.write(GATEWAYEEPROMADDRPART4, gateway[3]);
                    gateway_incorrect = false;
                  }
                }else{
                  gateway_incorrect = true;
                }
              }else if (x == 13){
                  if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  dnsServerIp[0] = atoi(valueArray);
                  if (EEPROM.read(DNSSERVEREEPROMADDRPART1)!=dnsServerIp[0]){
                    EEPROM.write(DNSSERVEREEPROMADDRPART1, dnsServerIp[0]);
                    dns_incorrect = false;
                  }
                }else{
                  dns_incorrect = true;
                }
              }else if (x == 14){
                  if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  dnsServerIp[1] = atoi(valueArray);
                  if (EEPROM.read(DNSSERVEREEPROMADDRPART2)!=dnsServerIp[1]){
                    EEPROM.write(DNSSERVEREEPROMADDRPART2, dnsServerIp[1]);
                    dns_incorrect = false;
                  }
                }else{
                  dns_incorrect = true;
                }
              }else if (x == 15){
                  if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  dnsServerIp[2] = atoi(valueArray);
                  if (EEPROM.read(DNSSERVEREEPROMADDRPART3)!=dnsServerIp[2]){
                    EEPROM.write(DNSSERVEREEPROMADDRPART3, dnsServerIp[2]);
                    dns_incorrect = false;
                  }
                }else{
                  dns_incorrect = true;
                }
              }else if (x == 16){
                  if (atoi(valueArray)>= 0 && atoi(valueArray) <= 255){
                  dnsServerIp[3] = atoi(valueArray);
                  if (EEPROM.read(DNSSERVEREEPROMADDRPART4)!=dnsServerIp[3]){
                    EEPROM.write(DNSSERVEREEPROMADDRPART4, dnsServerIp[3]);
                    dns_incorrect = false;
                  }
                }else{
                  dns_incorrect = true;
                }
              }else if (x == 17){
                  usedhcp = atoi(valueArray);
                  if (EEPROM.read(USEDHCPEEPROMADDR)!=usedhcp){
                    EEPROM.write(USEDHCPEEPROMADDR, usedhcp);
                  }
              }
            }
          }
      }
    }
    // send response headers
    sendProgMemAsString(client, (char*)pgm_read_word(&(contents_main[CONT_HEADER])));
  
      // send HTML header
    sendProgMemAsString(client, (char*)pgm_read_word(&(contents_main[CONT_TOP])));
      
    // send menu
    sendProgMemAsString(client, (char*)pgm_read_word(&(contents_main[CONT_MENU])));
      
    // send title
    sendProgMemAsString(client, (char*)pgm_read_word(&(contents_titles[nUriIndex])));
     
    // send the body for the requested page
    sendUriContentByIndex(client, nUriIndex, requestContent);
    //wdt_reset();
  
   
    // send footer
    sendProgMemAsString(client,(char*)pgm_read_word(&(contents_main[CONT_FOOTER])));
  }
  //sendUriContentByIndex(client, nUriIndex, requestContent, 4, CONT_FOOTER);
  INCSGDTON_incorrect = false;
  CSGDTOFF_incorrect = false;
  CSGNTON_incorrect = false;
  CSGNOFF_incorrect = false;
  MSGDTON_incorrect = false;
  MSGDTOFF_incorrect = false;
  MSGNTON_incorrect = false;
  MSGNTOFF_incorrect = false;
  HSGDTON_incorrect = false;
  HSGDTOFF_incorrect = false;
  HSGNTON_incorrect = false;
  HSGNTOFF_incorrect = false;
  AADTON_incorrect = false;
  AADTOFF_incorrect = false;
  AANTON_incorrect = false;
  AANTOFF_incorrect = false;
  DTHUMON_incorrect = false;
  DTHUMOFF_incorrect = false;
  NTHUMON_incorrect = false;
  NTHUMOFF_incorrect = false;
  UVLIGHTONHOUR_MINUTE_SECONDS_incorrect = false;
  UVLIGHTOFFHOUR_MINUTE_SECONDS_incorrect = false;
  localip_incorrect = false;
  subnetmask_incorrect = false;
  gateway_incorrect= false;
  dns_incorrect= false;
}

/**********************************************************************************************************************
*                                                              Send Images
***********************************************************************************************************************/
void sendImage(EthernetClient & client, int nUriIndex, BUFFER & requestContent)
{
  int nImageIndex = nUriIndex - NUM_PAGES;

  // send the header for the requested image
  //sendUriContentByIndex(client, nUriIndex, requestContent, 4, 0);
  sendUriContentByIndex(client, nUriIndex, requestContent);

  // send the image data
  sendProgMemAsBinary(client, (char *)pgm_read_word(&(data_for_images[nImageIndex])), (int)pgm_read_word(&(size_for_images[nImageIndex])));
}

/**********************************************************************************************************************
*                                                              Send content split by buffer size
***********************************************************************************************************************/
// If we provide string data then we don't need specify an explicit size and can do a string copy
void sendProgMemAsString(EthernetClient & client, const char *realword)
{
  sendProgMemAsBinary(client, realword, strlen_P(realword));
}

// Non-string data needs to provide an explicit size
void sendProgMemAsBinary(EthernetClient & client, const char* realword, int realLen)
{
  int remaining = realLen;
  const char * offsetPtr = realword;
  int nSize = sizeof(BUFFER);
  BUFFER buffer;

  while (remaining > 0)
  {
    //wdt_reset();
    // print content
    if (nSize > remaining)
      nSize = remaining;      // Partial buffer left to send

    memcpy_P(buffer, offsetPtr, nSize);

    if (client.write((const uint8_t *)buffer, nSize) != nSize);

    // more content to print?
    remaining -= nSize;
    offsetPtr += nSize;
  }
}

/**********************************************************************************************************************
*                                                              Send real page content
***********************************************************************************************************************/
// This method takes the contents page identified by nUriIndex, divides it up into buffer-sized
// strings, passes it on for STX substitution and finally sending to the client.
void sendUriContentByIndex(EthernetClient client, int nUriIndex, BUFFER & requestContent)
//void sendUriContentByIndex(EthernetClient client, int nUriIndex, BUFFER & requestContent, int page, int pageindex)
{
  // Locate the page data for the URI and prepare to process in buffer-sized chunks.
  const char * offsetPtr;               // Pointer to offset within URI for data to be copied to buffer and sent.
  char *pNextString;
  int nSubstituteIndex = -1;            // Count of substitutions so far for this URI
  int remaining;                        // Total bytes (of URI) remaining to be sent
  int nSize = sizeof(BUFFER) - 1;       // Effective size of buffer allowing last char as string terminator
  BUFFER buffer;

  if (nUriIndex < NUM_PAGES)
      offsetPtr = (char*)pgm_read_word(&(contents_pages[nUriIndex]));
  else
    offsetPtr = (char*)pgm_read_word(&(image_header));

  buffer[nSize] = 0;  // ensure there is always a string terminator
  remaining = strlen_P(offsetPtr);  // Set total bytes of URI remaining

  while (remaining > 0)
  {
    //wdt_reset();
    // print content
    if (nSize > remaining)
    {
      // Set whole buffer to string terminator before copying remainder.
      memset(buffer, 0, STRING_BUFFER_SIZE);
      nSize = remaining;      // Partial buffer left to send
    }
    memcpy_P(buffer, offsetPtr, nSize);
    offsetPtr += nSize;
    // We have a buffer's worth of page to check for substitution markers/delimiters.
    // Scan the buffer for markers, dividing it up into separate strings.
    if (buffer[0] == *pStxDelimiter)    // First char is delimiter
    {
      sendSubstitute(client, nUriIndex, ++nSubstituteIndex, requestContent);
      --remaining;
    }
    // First string is either terminated by the null at the end of the buffer
    // or by a substitution delimiter.  So simply send it to the client.
    pNextString = strtok(buffer, pStxDelimiter);
    client.print(pNextString);
    remaining -= strlen(pNextString);
    // Scan for strings between delimiters
    for (pNextString = strtok(NULL, pStxDelimiter); pNextString != NULL && remaining > 0; pNextString = strtok(NULL, pStxDelimiter))
    {
      // pNextString is pointing to the next string AFTER a delimiter
      sendSubstitute(client, nUriIndex, ++nSubstituteIndex, requestContent);
      --remaining;
      client.print(pNextString);
      remaining -= strlen(pNextString);
      //wdt_reset();
    }
  }
}

// Call this method in response to finding a substitution character '\002' within some
// URI content to send the appropriate replacement text, depending on the URI index and
// the substitution index within the content.
void sendSubstitute(EthernetClient client, int nUriIndex, int nSubstituteIndex, BUFFER & requestContent)
{
  if (nUriIndex < NUM_PAGES)
  {
    // Page request
    switch (nUriIndex)
    {
 /***************************************************************************
 //                  PAGE 1
 ***************************************************************************/
      case 0:
        switch (nSubstituteIndex)
        {
          case 0: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[30])));  //client.print("0.0.0.1");
            wdt_reset();
            break;
          case 1: 
            client.print(ip_to_str(localip));
            wdt_reset();
            break;
          case 2: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print(system_date);
            wdt_reset();
            break;
          case 3: 
            client.print(hour);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (minute<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(minute);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (second<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(second);
           
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[2])));  //client.print(" ");
            switch(dayOfWeek){
            case 1: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[3])));  //client.print("Sun");
              break;
            case 2: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[4])));  //client.print("Mon");
              break;
            case 3: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[5])));  //client.print("Tue");
              break;
            case 4: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[6])));  //client.print("Wed");
              break;
            case 5: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[7])));  //client.print("Thu");
              break;
            case 6: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[8])));  //client.print("Fri");
              break;
            case 7: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[9])));  //client.print("Sat");
              break;
            }
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print("  ");
            client.print(month);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[10])));  //client.print("/");
            client.print(dayOfMonth);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[11])));  //client.print("/20");
            client.println(year);
            wdt_reset();
            break;
          case 4: 
            if (cold_side_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[27])));  //client.print(/On.png");
            }else if (cold_side_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[28])));  //client.print("/Off.png");
            }
            wdt_reset();
            break;
          case 5: 
            if (cold_side_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("on");
            }else if (cold_side_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("off");
            }
            wdt_reset();
            break;
          case 6: 
            if (middle_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[27])));  //client.print("/On.png");
            }else if (middle_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[28])));  //client.print("/Off.png");
            }
            wdt_reset();
            break;
          case 7: 
            if (middle_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("on");
            }else if (middle_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("off");
            }
            wdt_reset();
            break;
          case 8: 
            if (hot_side_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[27])));  //client.print("/On.png");
            }else if (hot_side_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[28])));  //client.print("/Off.png");
            }
            wdt_reset();
            break;
          case 9: 
            if (hot_side_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("on");
            }else if (hot_side_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("off");
            }
            wdt_reset();
            break;
          case 10: 
            if (heat_lamp_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[27])));  //client.print("/On.png");
            }else if (heat_lamp_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[28])));  //client.print("/Off.png");
            }
            wdt_reset();
            break;
          case 11: 
            if (heat_lamp_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("on");
            }else if (heat_lamp_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("off");
            }
            wdt_reset();
            break;
          case 12: 
            if (UV_Light_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[27])));  //client.print("/On.png");
            }else if (UV_Light_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[28])));  //client.print("/Off.png");
            }
            wdt_reset();
            break;
          case 13: 
            if (UV_Light_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("on");
            }else if (UV_Light_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("off");
            }
            wdt_reset();
            break;
          case 14: 
            if (humidifier_status  == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[27])));  //client.print("/On.png");
            }else if (humidifier_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[28])));  //client.print("Off.png");
            }
            wdt_reset();
            break;
          case 15: 
            if (humidifier_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("on");
            }else if (humidifier_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("off");
            }
            wdt_reset();
            break;
          case 16: 
            if (temp_scale  == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[13])));  //client.print("F");
            }else{
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[14])));  //client.print("C");
            }
            wdt_reset();
            break;
          case 17: 
            client.print(cold_side_temp_whole);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[31])));  //client.print(".");
            client.print(cold_side_temp_fract);
            wdt_reset();
            break;
          case 18: 
            client.print(middle_temp_whole);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[31])));  //client.print(".");
            client.print(middle_temp_fract);
            wdt_reset();
            break;
          case 19: 
            client.print(hot_side_temp_whole);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[31])));  //client.print(".");
            client.print(hot_side_temp_fract);
            wdt_reset();
            break;
          case 20: 
            client.print(ambient_temp_1_whole);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[31])));  //client.print(".");
            client.print(ambient_temp_1_fract);
            wdt_reset();
            break;
          case 21: 
            client.print(ambient_temp_2_whole);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[31])));  //client.print(".");
            client.print(ambient_temp_2_fract);
            wdt_reset();
            break;
          case 22: 
            client.print((((float)ambient_temp_2_whole + (float)ambient_temp_1_whole + ((float)ambient_temp_2_fract / 100.0) + ((float)ambient_temp_1_fract / 100.0))/2.0));
            wdt_reset();
            break;
          case 23: 
            if (cold_side_temp_status  == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[25])));  //client.print("green.png");
            }else if (cold_side_temp_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[26])));  //client.print("red.png");
            }
            wdt_reset();
            break;
          case 24: 
            if (cold_side_temp_status  == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[54])));  //client.print("GOOD");
            }else if (cold_side_temp_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[55])));  //client.print("FAILED");
            }
            wdt_reset();
            break;
          case 25: 
            if (middle_temp_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[25])));  //client.print("green.png");
            }else if (middle_temp_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[26])));  //client.print("red.png");
            }
            wdt_reset();
            break;
          case 26: 
            if (middle_temp_status  == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[54])));  //client.print("GOOD");
            }else if (middle_temp_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[55])));  //client.print("FAILED");
            }
            wdt_reset();
            break;
          case 27: 
            if (hot_side_temp_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[25])));  //client.print("green.png");
            }else if (hot_side_temp_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[26])));  //client.print("red.png");
            }
            wdt_reset();
            break;
          case 28: 
            if (hot_side_temp_status  == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[54])));  //client.print("GOOD");
            }else if (hot_side_temp_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[55])));  //client.print("FAILED");
            }
            wdt_reset();
            break;
          case 29: 
            if (ambient_temp_1_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[25])));  //client.print("green.png");
            }else if (ambient_temp_1_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[26])));  //client.print("red.png");
            }
            wdt_reset();
            break;
          case 30: 
            if (ambient_temp_1_status  == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[54])));  //client.print("GOOD");
            }else if (ambient_temp_1_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[55])));  //client.print("FAILED");
            }
            wdt_reset();
            break;
          case 31: 
            if (ambient_temp_2_status == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[25])));  //client.print("green.png");
            }else if (ambient_temp_2_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[26])));  //client.print("red.png");
            }
            wdt_reset();
            break;
          case 32: 
            if (ambient_temp_2_status  == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[54])));  //client.print("GOOD");
            }else if (ambient_temp_2_status == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[55])));  //client.print("FAILED");
            }
            wdt_reset();
            break;
          case 33: 
            client.print(RH);
            wdt_reset();
            break;
        }
        break;
        
 /***************************************************************************
 //                  PAGE 2
 ***************************************************************************/
      case 1:  // page 2
        switch (nSubstituteIndex)
        {
          case 0: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[30])));  //client.print("0.0.0.1");
            wdt_reset();
            break;
          case 1: 
            client.print(ip_to_str(localip));
            wdt_reset();
            break;
          case 2: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print(system_date);
            wdt_reset();
            break;
          case 3: 
              client.print(hour);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (minute<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(minute);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (second<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(second);
           
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[2])));  //client.print(" ");
            switch(dayOfWeek){
            case 1: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[3])));  //client.print("Sun");
              break;
            case 2: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[4])));  //client.print("Mon");
              break;
            case 3: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[5])));  //client.print("Tue");
              break;
            case 4: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[6])));  //client.print("Wed");
              break;
            case 5: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[7])));  //client.print("Thu");
              break;
            case 6: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[8])));  //client.print("Fri");
              break;
            case 7: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[9])));  //client.print("Sat");
              break;
            }
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print("  ");
            client.print(month);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[10])));  //client.print("/");
            client.print(dayOfMonth);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[11])));  //client.print("/20");
            client.println(year);
            wdt_reset();
            break;
          case 4: 
            if (temp_scale == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[15])));  //client.print("20 and 120 degrees F");
            }else{
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[16])));  //client.print("0 and 49 degrees C");
            }
            wdt_reset();
            break;
          case 5: 
            if (INCSGDTON_incorrect == false)
            {
              client.print("");
            }else if (INCSGDTON_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 6: 
            client.print(INCSGDTON);
            wdt_reset();
            break;
          case 7: 
            if (CSGDTOFF_incorrect == false)
            {
              client.print("");
            }else if (CSGDTOFF_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 8: 
            client.print(CSGDTOFF);
            wdt_reset();
            break;
          case 9: 
            if (MSGDTON_incorrect == false)
            {
              client.print("");
            }else if (MSGDTON_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 10: 
            client.print(MSGDTON);
            wdt_reset();
            break;
          case 11: 
            if (MSGDTOFF_incorrect == false)
            {
              client.print("");
            }else if (MSGDTOFF_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 12: 
            client.print(MSGDTOFF);
            wdt_reset();
            break;
          case 13: 
            if (HSGDTON_incorrect == false)
            {
              client.print("");
            }else if (HSGDTON_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 14: 
            client.print(HSGDTON);
            wdt_reset();
            break;
          case 15: 
            if (HSGDTOFF_incorrect == false)
            {
              client.print("");
            }else if (HSGDTOFF_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 16: 
            client.print(HSGDTOFF);
            wdt_reset();
            break;
          case 17: 
            if (AADTON_incorrect == false)
            {
              client.print("");
            }else if (AADTON_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 18: 
            client.print(AADTON);
            wdt_reset();
            break;
          case 19: 
            if (AADTOFF_incorrect == false)
            {
              client.print("");
            }else if (AADTOFF_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 20: 
            client.print(AADTOFF);
            wdt_reset();
            break;
          case 21:	
	    if (CSGNTON_incorrect == false)
            {
              client.print("");
            }else if (CSGNTON_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 22: 
            client.print(CSGNTON);
            wdt_reset();
            break;
          case 23: 
            if (CSGNOFF_incorrect == false)
            {
              client.print("");
            }else if (CSGNOFF_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 24: 
            client.print(CSGNOFF);
            wdt_reset();
            break;
          case 25: 
            if (MSGNTON_incorrect == false)
            {
              client.print("");
            }else if (MSGNTON_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 26: 
            client.print(MSGNTON);
            wdt_reset();
            break;
          case 27: 
            if (MSGNTOFF_incorrect == false)
            {
              client.print("");
            }else if (MSGNTOFF_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 28: 
            client.print(MSGNTOFF);
            wdt_reset();
            break;
          case 29: 
            if (HSGNTON_incorrect == false)
            {
              client.print("");
            }else if (HSGNTON_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 30: 
            client.print(HSGNTON);
            wdt_reset();
            break;
          case 31: 
            if (HSGNTOFF_incorrect == false)
            {
              client.print("");
            }else if (HSGNTOFF_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 32: 
            client.print(HSGNTOFF);
            wdt_reset();
            break;
          case 33: 
            if (AANTON_incorrect == false)
            {
              client.print("");
            }else if (AANTON_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 34: 
            client.print(AANTON);
            wdt_reset();
            break;
          case 35: 
            if (AANTOFF_incorrect == false)
            {
              client.print("");
            }else if (AANTOFF_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 36: 
            client.print(AANTOFF);
            wdt_reset();
            break;
          case 37: 
            if (temp_scale == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
          case 38: 
            if (temp_scale == 2){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
        }
        break;
 /***************************************************************************
 //                  PAGE 3
 ***************************************************************************/
       case 2:  // page 3
        switch (nSubstituteIndex)
        {
          case 0: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[30])));  //client.print("0.0.0.1");
            wdt_reset();
            break;
          case 1: 
            client.print(ip_to_str(localip));
            wdt_reset();
            break;
          case 2: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print(system_date);
            wdt_reset();
            break;
          case 3: 
            client.print(hour);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (minute<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(minute);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (second<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(second);
           
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[2])));  //client.print(" ");
            switch(dayOfWeek){
            case 1: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[3])));  //client.print("Sun");
              break;
            case 2: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[4])));  //client.print("Mon");
              break;
            case 3: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[5])));  //client.print("Tue");
              break;
            case 4: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[6])));  //client.print("Wed");
              break;
            case 5: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[7])));  //client.print("Thu");
              break;
            case 6: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[8])));  //client.print("Fri");
              break;
            case 7: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[9])));  //client.print("Sat");
              break;
            }
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print("  ");
            client.print(month);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[10])));  //client.print("/");
            client.print(dayOfMonth);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[11])));  //client.print("/20");
            client.println(year);
            wdt_reset();
            break;
          case 4: 
            if (DTHUMON_incorrect == false)
            {
              client.print("");
            }else if (DTHUMON_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 5: 
            client.print(DTHUMON);
            wdt_reset();
            break;
          case 6: 
            if (DTHUMOFF_incorrect == false)
            {
              client.print("");
            }else if (DTHUMOFF_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 7: 
            client.print(DTHUMOFF);
            wdt_reset();
            break;
          case 8: 
            if (NTHUMON_incorrect == false)
            {
              client.print("");
            }else if (NTHUMON_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 9: 
            client.print(NTHUMON);
            wdt_reset();
            break;
          case 10: 
            if (NTHUMOFF_incorrect == false)
            {
              client.print("");
            }else if (NTHUMOFF_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 11: 
            client.print(NTHUMOFF);
            wdt_reset();
            break;
        }
        break;
/***************************************************************************
 //                  PAGE 4
 ***************************************************************************/
       case 3:  // page 4
        switch (nSubstituteIndex)
        {
          case 0: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[30])));  //client.print("0.0.0.1");
            wdt_reset();
            break;
          case 1: 
            client.print(ip_to_str(localip));
            wdt_reset();
            break;
          case 2: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print(system_date);
            wdt_reset();
            break;
          case 3: 
              client.print(hour);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (minute<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(minute);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (second<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(second);
           
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[2])));  //client.print(" ");
            switch(dayOfWeek){
            case 1: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[3])));  //client.print("Sun");
              break;
            case 2: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[4])));  //client.print("Mon");
              break;
            case 3: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[5])));  //client.print("Tue");
              break;
            case 4: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[6])));  //client.print("Wed");
              break;
            case 5: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[7])));  //client.print("Thu");
              break;
            case 6: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[8])));  //client.print("Fri");
              break;
            case 7: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[9])));  //client.print("Sat");
              break;
            }
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print("  ");
            client.print(month);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[10])));  //client.print("/");
            client.print(dayOfMonth);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[11])));  //client.print("/20");
            client.println(year);
            wdt_reset();
            break;
          case 4: 
            if (month == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 5: 
            if (month == 2)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 6: 
            if (month == 3)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 7: 
            if (month == 4)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 8: 
            if (month == 5)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 9: 
            if (month == 6)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 10: 
            if (month == 7)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 11: 
            if (month == 8)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 12: 
            if (month == 9)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 13: 
            if (month == 10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 14: 
            if (month == 11)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 15: 
            if (month == 12)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 16: 
            if (dayOfMonth == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 17: 
            if (dayOfMonth == 2)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 18: 
            if (dayOfMonth == 3)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 19: 
            if (dayOfMonth == 4)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 20: 
            if (dayOfMonth == 5)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 21: 
            if (dayOfMonth == 6)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 22: 
            if (dayOfMonth == 7)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 23: 
            if (dayOfMonth == 8)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 24: 
            if (dayOfMonth == 9)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 25: 
            if (dayOfMonth == 10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 26: 
            if (dayOfMonth == 11)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 27: 
            if (dayOfMonth == 12)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 28: 
            if (dayOfMonth == 13)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 29: 
            if (dayOfMonth == 14)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 30: 
            if (dayOfMonth == 15)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 31: 
            if (dayOfMonth == 16)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 32: 
            if (dayOfMonth == 17)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 33: 
            if (dayOfMonth == 18)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 34: 
            if (dayOfMonth == 19)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 35: 
            if (dayOfMonth == 20)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 36: 
            if (dayOfMonth == 21)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 37: 
            if (dayOfMonth == 22)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 38: 
            if (dayOfMonth == 23)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 39: 
            if (dayOfMonth == 24)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 40: 
            if (dayOfMonth == 25)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 41: 
            if (dayOfMonth == 26)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 42: 
            if (dayOfMonth == 27)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 43: 
            if (dayOfMonth == 28)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 44: 
            if (dayOfMonth == 29)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 45: 
            if (dayOfMonth == 30)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 46: 
            if (dayOfMonth == 31)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 47: 
            if (year == 12)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 48: 
            if (year == 13)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 49: 
            if (year == 14)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 50: 
            if (year == 15)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 51: 
            if (year == 16)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 52: 
            if (year == 17)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 53: 
            if (year == 18)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 54: 
            if (year == 19)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 55: 
            if (year == 20)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 56: 
            if (year == 21)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 57: 
            if (year == 22)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 58: 
            if (year == 23)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 59: 
            if (year == 24)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 60: 
            if (year == 25)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 61: 
            if (year == 26)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 62: 
            if (year == 27)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 63: 
            if (year == 28)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 64: 
            if (year == 29)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 65: 
            if (year == 30)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 66: 
            if (dayOfWeek == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 67: 
            if (dayOfWeek == 2)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 68: 
            if (dayOfWeek == 3)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 69: 
            if (dayOfWeek == 4)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 70: 
            if (dayOfWeek == 5)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 71: 
            if (dayOfWeek == 6)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 72: 
            if (dayOfWeek == 7)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 73: 
            if (TIMEZONE == -12)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 74: 
            if (TIMEZONE == -11)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 75: 
            if (TIMEZONE == -10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 76: 
            if (TIMEZONE == -9)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 77: 
            if (TIMEZONE == -8)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 78: 
            if (TIMEZONE == -7)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 79: 
            if (TIMEZONE == -6)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 80: 
            if (TIMEZONE == -5)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 81: 
            if (TIMEZONE == -4)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 82: 
            if (TIMEZONE == -2)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 83: 
            if (TIMEZONE == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 84: 
            if (TIMEZONE == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 85: 
            if (TIMEZONE == 2)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 86: 
            if (TIMEZONE == 3)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 87: 
            if (TIMEZONE == 4)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 88: 
            if (TIMEZONE == 5)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 89: 
            if (TIMEZONE == 6)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 90: 
            if (TIMEZONE == 7)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 91: 
            if (TIMEZONE == 8)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 92: 
            if (TIMEZONE == 9)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 93: 
            if (TIMEZONE == 10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 94: 
            if (TIMEZONE == 11)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 95: 
            if (TIMEZONE == 12)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 96: 
            if (TIMEZONE == 13)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("selected=\"selected\"");
            }else{
              client.print("");
            }
            wdt_reset();
            break;
          case 97: 
            if (UVLIGHTONHOUR_MINUTE_SECONDS_incorrect == false)
            {
              client.print("");
            }else if (UVLIGHTONHOUR_MINUTE_SECONDS_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 98: 
            client.print(UVLIGHTONHOUR);
            wdt_reset();
            break;
          case 99: 
            client.print(UVLIGHTONMINUTE);
            wdt_reset();
            break;
          case 100: 
            client.print(UVLIGHTONSECOND);
            wdt_reset();
            break;
          case 101: 
            if (UVLIGHTOFFHOUR_MINUTE_SECONDS_incorrect == false)
            {
              client.print("");
            }else if (UVLIGHTOFFHOUR_MINUTE_SECONDS_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 102: 
            client.print(UVLIGHTOFFHOUR);
            wdt_reset();
            break;
          case 103: 
            client.print(UVLIGHTOFFMINUTE);
            wdt_reset();
            break;
          case 104: 
            client.print(UVLIGHTOFFSECOND);
            wdt_reset();
            break;
        }
        break;

/***************************************************************************
 //                  PAGE 5
 ***************************************************************************/
       case 4:  // page 5
        switch (nSubstituteIndex)
        {
          case 0: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[30])));  //client.print("0.0.0.1");
            wdt_reset();
            break;
          case 1: 
            client.print(ip_to_str(localip));
            wdt_reset();
            break;
          case 2: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print(system_date);
            wdt_reset();
            break;
          case 3: 
              client.print(hour);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (minute<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(minute);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (second<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(second);
           
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[2])));  //client.print(" ");
            switch(dayOfWeek){
            case 1: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[3])));  //client.print("Sun");
              break;
            case 2: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[4])));  //client.print("Mon");
              break;
            case 3: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[5])));  //client.print("Tue");
              break;
            case 4: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[6])));  //client.print("Wed");
              break;
            case 5: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[7])));  //client.print("Thu");
              break;
            case 6: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[8])));  //client.print("Fri");
              break;
            case 7: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[9])));  //client.print("Sat");
              break;
            }
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print("  ");
            client.print(month);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[10])));  //client.print("/");
            client.print(dayOfMonth);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[11])));  //client.print("/20");
            client.println(year);
            wdt_reset();
            break;
          case 4:
            if (SD_init_OK == 1){
              client.print("");
              if (SDFAT_init_OK == 1){
                client.print("");
                if (root_OK == 1){
                  client.print("");
                }else{
                  sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[34])));  //client.print("<tr><td colspan = \"2\" align = \"center\"><font color = \"red\"><b>SD Card Found and formatted but not accessible - Please check SD Card</b></font></td></tr>");
                }
              }else{
                sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[35])));  //client.print("<tr><td colspan = \"2\" align = \"center\"><font color = \"red\"><b>Please check SD Card, SD Card Found but not functional... Is It Formatted?</b></font></td></tr>");
              }
            }else{
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[36])));  //client.print("<tr><td colspan = \"2\" align = \"center\"><font color = \"red\"><b>Please check SD Card, NO SD CARD FOUND!</b></font></td></tr>");
            }
            break;
          case 5:
            switch(SD_Type) {
            case SD_CARD_TYPE_SD1:
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[37])));  //client.print("SD1");
              break;
            case SD_CARD_TYPE_SD2:
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[38])));  //client.print("SD2");
              break;
            case SD_CARD_TYPE_SDHC:
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[39])));  //client.print("SDHC");
              break;
            default:
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[40])));  //client.print("N/A");
            }
            break;
          case 6:
            if (SDFAT_init_OK == 1){
              client.print(FAT_Type);
            }else{
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[40])));  //client.print("N/A");
            }
             break;
           case 7:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               client.print(volumesize / 1024);
             }else{
               sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[40])));  //client.print("N/A");
             }
             break;
           case 8:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               client.print((volumesize / 1024) / 1024);
             }else{
               sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[40])));  //client.print("N/A");
             }
             break;
           case 9:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               client.print((SDUSEDSPACE() / 1024));
             }else{
               sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[40])));  //client.print("N/A");
             }
             break;
           case 10:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               client.print((SDUSEDSPACE() / 1024) / 1024);
             }else{
               sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[40])));  //client.print("N/A");
             }
             break;
           case 11:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               byte SD_Percent = (SDUSEDSPACE() / volumesize) * 100;
               client.print(SD_Percent * 3);
               sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[41])));  //client.print("px;");
             }else{
               sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[42])));  //client.print("0px;");
             }
             break;
           case 12:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               byte SD_Percent = (SDUSEDSPACE() / volumesize) * 100;
               client.print(SD_Percent);
             }else{
               sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[40])));  //client.print("N/A");
             }
             break;
           case 13:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               if (data_log_enabled == 1){
                 sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("Selected=\"selected\"");
               }else{
                 client.print("");
               }
             }else{
               client.print("");
             }
             break;
           case 14:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               if (data_log_enabled == 0){
                 sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("Selected=\"selected\"");
               }else{
                 client.print("");
               }
             }else{
               client.print("");
             }
             break;
           case 15:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               if (data_log_period == 10){
                 sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("Selected=\"selected\"");
               }else{
                 client.print("");
               }
             }else{
               client.print("");
             }
             break;
           case 16:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               if (data_log_period == 20){
                 sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("Selected=\"selected\"");
               }else{
                 client.print("");
               }
             }else{
               client.print("");
             }
             break;
           case 17:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               if (data_log_period == 30){
                 sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("Selected=\"selected\"");
               }else{
                 client.print("");
               }
             }else{
               client.print("");
             }
             break;
           case 18:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               if (data_log_period == 40){
                 sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("Selected=\"selected\"");
               }else{
                 client.print("");
               }
             }else{
               client.print("");
             }
             break;
           case 19:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               if (data_log_period == 50){
                 sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("Selected=\"selected\"");
               }else{
                 client.print("");
               }
             }else{
               client.print("");
             }
             break;
           case 20:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               if (data_log_period == 60){
                 sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[17])));  //client.print("Selected=\"selected\"");
               }else{
                 client.print("");
               }
             }else{
               client.print("");
             }
             break;
           case 21:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               client.print("");
             }else{
               sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[20])));  //client.print("disabled");
             }
             break;
           case 22:
             if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               ListFilesToDelete(client, LS_DATE);
             }else{
               client.print("");
             }
             break;
           case 23:
            if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1 && data_log_enabled ==0){
               client.print("");
             }else{
               sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[20])));  //client.print("disabled");
             }
             break;
             
        }
        break;
        
 /***************************************************************************
 //                  PAGE 6
 ***************************************************************************/
       case 5:  // page 6
        switch (nSubstituteIndex)
        {
          case 0: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[30])));  //client.print("0.0.0.1");
            wdt_reset();
            break;
          case 1: 
            client.print(ip_to_str(localip));
            wdt_reset();
            break;
          case 2: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print(system_date);
            wdt_reset();
            break;
          case 3: 
            client.print(hour);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (minute<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(minute);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (second<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(second);
           
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[2])));  //client.print(" ");
            switch(dayOfWeek){
            case 1: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[3])));  //client.print("Sun");
              break;
            case 2: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[4])));  //client.print("Mon");
              break;
            case 3: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[5])));  //client.print("Tue");
              break;
            case 4: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[6])));  //client.print("Wed");
              break;
            case 5: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[7])));  //client.print("Thu");
              break;
            case 6: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[8])));  //client.print("Fri");
              break;
            case 7: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[9])));  //client.print("Sat");
              break;
            }
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print("  ");
            client.print(month);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[10])));  //client.print("/");
            client.print(dayOfMonth);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[11])));  //client.print("/20");
            client.println(year);
            wdt_reset();
            break;
          case 4: 
            if (SD_init_OK ==1 && SDFAT_init_OK == 1 && root_OK ==1){
               ListFiles(client, LS_DATE);
             }else{
               client.print("");
             }
             break;
        }
        break;


 /***************************************************************************
 //                  PAGE 7
 ***************************************************************************/
       case 6:  // page 7
        switch (nSubstituteIndex)
        {
          case 0: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[30])));  //client.print("0.0.0.1");
            wdt_reset();
            break;
          case 1: 
            client.print(ip_to_str(localip));
            wdt_reset();
            break;
          case 2: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print(system_date);
            wdt_reset();
            break;
          case 3: 
            client.print(hour);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (minute<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(minute);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (second<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(second);
           
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[2])));  //client.print(" ");
            switch(dayOfWeek){
            case 1: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[3])));  //client.print("Sun");
              break;
            case 2: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[4])));  //client.print("Mon");
              break;
            case 3: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[5])));  //client.print("Tue");
              break;
            case 4: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[6])));  //client.print("Wed");
              break;
            case 5: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[7])));  //client.print("Thu");
              break;
            case 6: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[8])));  //client.print("Fri");
              break;
            case 7: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[9])));  //client.print("Sat");
              break;
            }
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print("  ");
            client.print(month);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[10])));  //client.print("/");
            client.print(dayOfMonth);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[11])));  //client.print("/20");
            client.println(year);
            wdt_reset();
            break;
          case 4:
            if (CSGMANUAL == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }else if (CSGMANUAL == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 5:
            if (CSGMANUAL == 1){
              client.print("");
            }else if (CSGMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }
            wdt_reset();
            break;
          case 6:
            if (MSGMANUAL == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }else if (MSGMANUAL == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 7:
            if (MSGMANUAL == 1){
              client.print("");
            }else if (MSGMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }
            wdt_reset();
            break;
          case 8:
            if (HSGMANUAL == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }else if (HSGMANUAL == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 9:
            if (HSGMANUAL == 1){
              client.print("");
            }else if (HSGMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }
            wdt_reset();
            break;
          case 10:
            if (HEATLAMPMANUAL == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }else if (HEATLAMPMANUAL == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 11:
            if (HEATLAMPMANUAL == 1){
              client.print("");
            }else if (HEATLAMPMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }
            wdt_reset();
            break;
          case 12:
            if (UVLIGHTMANUAL == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }else if (UVLIGHTMANUAL == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 13:
            if (UVLIGHTMANUAL == 1){
              client.print("");
            }else if (UVLIGHTMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }
            wdt_reset();
            break;
          case 14:
            if (HUMIDIFIERMANUAL == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }else if (HUMIDIFIERMANUAL == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 15:
            if (HUMIDIFIERMANUAL == 1){
              client.print("");
            }else if (HUMIDIFIERMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[12])));  //client.print("selected");
            }
            wdt_reset();
            break;
          case 16: 
            if (cold_side_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (cold_side_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 17: 
            if (CSGMANUAL == 1)
            {
              client.print("");
            }else if (CSGMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[20])));  //client.print("disabled");
            }
            wdt_reset();
            break;
          case 18: 
            if (cold_side_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (cold_side_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 19: 
            if (middle_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (middle_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 20: 
            if (MSGMANUAL == 1)
            {
              client.print("");
            }else if (MSGMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[20])));  //client.print("disabled");
            }
            wdt_reset();
            break;
          case 21: 
            if (middle_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (middle_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 22: 
            if (hot_side_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (hot_side_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 23: 
            if (HSGMANUAL == 1)
            {
              client.print("");
            }else if (HSGMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[20])));  //client.print("disabled");
            }
            wdt_reset();
            break;
          case 24: 
            if (hot_side_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (hot_side_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 25: 
            if (heat_lamp_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (heat_lamp_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 26: 
            if (HEATLAMPMANUAL == 1)
            {
              client.print("");
            }else if (HEATLAMPMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[20])));  //client.print("disabled");
            }
            wdt_reset();
            break;
          case 27: 
            if (heat_lamp_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (heat_lamp_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 28: 
            if (UV_Light_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (UV_Light_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 29: 
            if (UVLIGHTMANUAL == 1)
            {
              client.print("");
            }else if (UVLIGHTMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[20])));  //client.print("disabled");
            }
            wdt_reset();
            break;
          case 30: 
            if (UV_Light_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (UV_Light_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 31: 
            if (humidifier_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (humidifier_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
          case 32: 
            if (HUMIDIFIERMANUAL == 1)
            {
              client.print("");
            }else if (HUMIDIFIERMANUAL == 0){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[20])));  //client.print("disabled");
            }
            wdt_reset();
            break;
          case 33: 
            if (humidifier_status == 0)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[18])));  //client.print("On");
            }else if (humidifier_status == 1){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[19])));  //client.print("Off");
            }
            wdt_reset();
            break;
        }
        break;
/***************************************************************************
 //                  PAGE 8
 ***************************************************************************/
       case 7:  // page 8
        switch (nSubstituteIndex)
        {
          case 0: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[30])));  //client.print("0.0.0.1");
            wdt_reset();
            break;
          case 1: 
            client.print(ip_to_str(localip));
            wdt_reset();
            break;
          case 2: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print(system_date);
            wdt_reset();
            break;
          case 3: 
            client.print(hour);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (minute<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(minute);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (second<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(second);
           
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[2])));  //client.print(" ");
            switch(dayOfWeek){
            case 1: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[3])));  //client.print("Sun");
              break;
            case 2: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[4])));  //client.print("Mon");
              break;
            case 3: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[5])));  //client.print("Tue");
              break;
            case 4: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[6])));  //client.print("Wed");
              break;
            case 5: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[7])));  //client.print("Thu");
              break;
            case 6: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[8])));  //client.print("Fri");
              break;
            case 7: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[9])));  //client.print("Sat");
              break;
            }
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print("  ");
            client.print(month);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[10])));  //client.print("/");
            client.print(dayOfMonth);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[11])));  //client.print("/20");
            client.println(year);
            wdt_reset();
            break;
          case 4: 
            if (localip_incorrect == false)
            {
              client.print("");
            }else if (localip_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 5: 
            client.print(localip[0]);
            wdt_reset();
            break;
          case 6: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 7: 
            client.print(localip[1]);
            wdt_reset();
            break;
          case 8: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 9: 
            client.print(localip[2]);
            wdt_reset();
            break;
          case 10: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 11: 
            client.print(localip[3]);
            wdt_reset();
            break;
          case 12: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 13: 
            if (subnetmask_incorrect == false)
            {
              client.print("");
            }else if (subnetmask_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 14: 
            client.print(subnetmask[0]);
            wdt_reset();
            break;
          case 15: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 16: 
            client.print(subnetmask[1]);
            wdt_reset();
            break;
          case 17: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 18: 
            client.print(subnetmask[2]);
            wdt_reset();
            break;
          case 19: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 20: 
            client.print(subnetmask[3]);
            wdt_reset();
            break;
          case 21: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 22: 
            if (gateway_incorrect == false)
            {
              client.print("");
            }else if (gateway_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 23: 
            client.print(gateway[0]);
            wdt_reset();
            break;
          case 24: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 25: 
            client.print(gateway[1]);
            wdt_reset();
            break;
          case 26: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 27: 
            client.print(gateway[2]);
            wdt_reset();
            break;
          case 28: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 29: 
            client.print(gateway[3]);
            wdt_reset();
            break;
          case 30: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 31: 
            if (dns_incorrect == false)
            {
              client.print("");
            }else if (dns_incorrect == true){
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[29])));  //client.print("<tr><td colspan=\"2\"><font color = \"red\"><b><center>Invalid Entry!</center></b></font></td></tr>");
            }
            wdt_reset();
            break;
          case 32: 
            client.print(dnsServerIp[0]);
            wdt_reset();
            break;
          case 33: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 34: 
            client.print(dnsServerIp[1]);
            wdt_reset();
            break;
          case 35: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 36: 
            client.print(dnsServerIp[2]);
            wdt_reset();
            break;
          case 37: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 38: 
            client.print(dnsServerIp[3]);
            wdt_reset();
            break;
          case 39: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[21])));  //client.print("readonly");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
          case 40: 
            if (usedhcp == 1)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[22])));  //client.print("checked");
            }else if (usedhcp == 0){
              client.print("");
            }
            wdt_reset();
            break;
        }
        break;
/***************************************************************************
 //                  PAGE 9
 ***************************************************************************/
       case 8:  // page 9
       getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);//get current time from RTC
       TimeStamp now = {(unsigned int)year+2000, month,  dayOfMonth, hour,  minute,  second};
       TimeStamp diff = calcDiff(systemstarttime, now);
        switch (nSubstituteIndex)
        {
          case 0: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[30])));  //client.print("0.0.0.1");
            wdt_reset();
            break;
          case 1: 
            client.print(ip_to_str(localip));
            wdt_reset();
            break;
          case 2: 
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print(system_date);
            wdt_reset();
            break;
          case 3: 
            client.print(hour);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (minute<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(minute);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[0])));  //client.print(":");
            if (second<10)
            {
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[1])));  //client.print("0");
            }
            client.print(second);
           
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[2])));  //client.print(" ");
            switch(dayOfWeek){
            case 1: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[3])));  //client.print("Sun");
              break;
            case 2: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[4])));  //client.print("Mon");
              break;
            case 3: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[5])));  //client.print("Tue");
              break;
            case 4: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[6])));  //client.print("Wed");
              break;
            case 5: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[7])));  //client.print("Thu");
              break;
            case 6: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[8])));  //client.print("Fri");
              break;
            case 7: 
              sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[9])));  //client.print("Sat");
              break;
            }
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[32])));  //client.print("  ");
            client.print(month);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[10])));  //client.print("/");
            client.print(dayOfMonth);
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[11])));  //client.print("/20");
            client.println(year);
            wdt_reset();
            break;
          case 4: 
            client.print(diff.yy);//years
            wdt_reset();
            break;
          case 5: 
            client.print(diff.mm);//months
            wdt_reset();
            break;
          case 6: 
            client.print(diff.dd);//days
            wdt_reset();
            break;
          case 7: 
            client.print(diff.hh);//hours
            wdt_reset();
            break;
          case 8: 
            client.print(diff.min);//minutes
            wdt_reset();
            break;
          case 9: 
            client.print(diff.ss);//seconds
            wdt_reset();
            break;
          case 10: 
            client.print(mac[0],HEX);
            wdt_reset();
            break;
          case 11: 
            client.print(mac[1],HEX);
            wdt_reset();
            break;
          case 12: 
            client.print(mac[2],HEX);
            wdt_reset();
            break;
          case 13: 
            client.print(mac[3],HEX);
            wdt_reset();
            break;
          case 14: 
            client.print(mac[4],HEX);
            wdt_reset();
            break;
          case 15: 
            client.print(mac[5],HEX);
            wdt_reset();
            break;
          case 16: 
            client.print(F(
            "<table border=\"0\">"
            "<tr>"
              "<td align = \"right\">Cold Side Sensor:</td>"));
                if (cold_badsensordate.yy==0){
                  client.print(F("<td><font color = \"green\">No Errors Detected</font></td>"));
                }else{
                  client.print(F("<td><font color = \"red\"><b>Number of Errors: "));
                  client.print(cold_badsensorcount);
                  client.print(F("   ||   Last Error Detected on: "));
                  client.print(cold_badsensordate.hh);
                  client.print(F(":"));
                  client.print(cold_badsensordate.min);
                  client.print(F(":"));
                  client.print(cold_badsensordate.ss);
                  client.print(F(" -- "));
                  client.print(cold_badsensordate.mm);
                  client.print(F("/"));
                  client.print(cold_badsensordate.dd);
                  client.print(F("/"));
                  client.print(cold_badsensordate.yy);
                  client.print(F("</b></font></td>"));
                }
            client.print(F(
            "</tr><tr>"
              "<td align = \"right\">Middle Sensor:</td>"));
                if (middle_badsensordate.yy==0){
                  client.print(F("<td><font color = \"green\">No Errors Detected</font></td>"));
                }else{
                  client.print(F("<td><font color = \"red\"><b>Number of Errors: "));
                  client.print(middle_badsensorcount);
                  client.print(F("   ||   Last Error Detected on: "));
                  client.print(middle_badsensordate.hh);
                  client.print(F(":"));
                  client.print(middle_badsensordate.min);
                  client.print(F(":"));
                  client.print(middle_badsensordate.ss);
                  client.print(F(" -- "));
                  client.print(middle_badsensordate.mm);
                  client.print(F("/"));
                  client.print(middle_badsensordate.dd);
                  client.print(F("/"));
                  client.print(middle_badsensordate.yy);
                  client.print(F("</b></font></td>"));
                }
            client.print(F(
            "</tr><tr>"
              "<td align = \"right\">Hot Side Sensor:</td>"));
                if (hot_badsensordate.yy==0){
                  client.print(F("<td><font color = \"green\">No Errors Detected</font></td>"));
                }else{
                  client.print(F("<td><font color = \"red\"><b>Number of Errors: "));
                  client.print(hot_badsensorcount);
                  client.print(F("   ||   Last Error Detected on: "));
                  client.print(hot_badsensordate.hh);
                  client.print(F(":"));
                  client.print(hot_badsensordate.min);
                  client.print(F(":"));
                  client.print(hot_badsensordate.ss);
                  client.print(F(" -- "));
                  client.print(hot_badsensordate.mm);
                  client.print(F("/"));
                  client.print(hot_badsensordate.dd);
                  client.print(F("/"));
                  client.print(hot_badsensordate.yy);
                  client.print(F("</b></font></td>"));
                }
            client.print(F(
            "</tr><tr>"
              "<td align = \"right\">Ambient Sensor 1:</td>"));
                if (ambient1_badsensordate.yy==0){
                  client.print(F("<td><font color = \"green\">No Errors Detected</font></td>"));
                }else{
                  client.print(F("<td><font color = \"red\"><b>Number of Errors: "));
                  client.print(ambient1_badsensorcount);
                  client.print(F("   ||   Last Error Detected on: "));
                  client.print(ambient1_badsensordate.hh);
                  client.print(F(":"));
                  client.print(ambient1_badsensordate.min);
                  client.print(F(":"));
                  client.print(ambient1_badsensordate.ss);
                  client.print(F(" -- "));
                  client.print(ambient1_badsensordate.mm);
                  client.print(F("/"));
                  client.print(ambient1_badsensordate.dd);
                  client.print(F("/"));
                  client.print(ambient1_badsensordate.yy);
                  client.print(F("</b></font></td>"));
                }
            client.print(F(
            "</tr><tr>"
              "<td align = \"right\">Ambient Sensor 2:</td>"));
                if (ambient2_badsensordate.yy==0){
                  client.print(F("<td><font color = \"green\">No Errors Detected</font></td>"));
                }else{
                  client.print(F("<td><font color = \"red\"><b>Number of Errors: "));
                  client.print(ambient2_badsensorcount);
                  client.print(F("   ||   Last Error Detected on: "));
                  client.print(ambient2_badsensordate.hh);
                  client.print(F(":"));
                  client.print(ambient2_badsensordate.min);
                  client.print(F(":"));
                  client.print(ambient2_badsensordate.ss);
                  client.print(F(" -- "));
                  client.print(ambient2_badsensordate.mm);
                  client.print(F("/"));
                  client.print(ambient2_badsensordate.dd);
                  client.print(F("/"));
                  client.print(ambient2_badsensordate.yy);
                  client.print(F("</b></font></td>"));
                }
            client.print(F("</tr></table>"));
            wdt_reset();
            break;
          case 17: 
            client.print(freeRam());
            wdt_reset();
            break;
        }
        break;
    }
  }
  
  else
  {
    // Image request
    int nImageIndex = nUriIndex - NUM_PAGES;

    switch (nSubstituteIndex)
    {
      case 0:
        // Content-Length value - ie. image size
        char strSize[6];  // Up to 5 digits plus null terminator
        itoa((int)pgm_read_word(&(size_for_images[nImageIndex])), strSize, 10);
        client.print(strSize);
        break;
      case 1:
        // Content-Type partial value
        switch (nImageIndex)
        {
          case 0:  // favicon
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[23])));  //client.print("x-icon");
            break;
          case 1:  // led on image
          case 2:  // led off image
            sendProgMemAsString(client, (char*)pgm_read_word(&(basic_table[24])));  //client.print("bmp");
            break;
        }
    }
  }
}


/**********************************************************************************************************************
*                                                          END OF CODE! WELL DONE!
***********************************************************************************************************************/


