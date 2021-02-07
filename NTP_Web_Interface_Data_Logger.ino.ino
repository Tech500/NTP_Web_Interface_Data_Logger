////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//    Version 2.3        LittleFS version  02//7/2021 @ 17:120 EST --WiFi Status updated
//
//                       ESP8266 --Internet Weather Datalogger and Dynamic Web Server   arduino.cc Topic
//
//                       "NTP_Time-synced_Web_Interface.ino" developed by William Lucid
//
//                       NTP time routines optimized for speed by schufti  --of ESP8266.com
//
//                       https://forum.arduino.cc/index.php?topic=466867.0     //Project discussion at arduino.cc
//
//                       Supports WeMos D1 R2 and RobotDyn WiFi D1 R2 32 MB   --ESP8266EX Baseds Developement Board
//
//                       https://wiki.wemos.cc/products:d1:d1
//
//                       http://robotdyn.com/wifi-d1-r2-esp8266-dev-board-32m-flash.html
//
//                       listFiles and readFile functions by martinayotte of ESP8266 Community Forum
//
//                       Previous project:  "SdBrose_CC3000_HTTPServer.ino" by tech500" on https://github.com/tech500
//
//                       Project is Open-Source, requires one Barometric Pressure sensor, BME280 and RobotDyn, ESP8266 based developement board.
//
//                       http://weather-1.ddns.net/Weather Project web page  --Servered from ESP8266, RobotDyn WiFi D1 development board
//
//                       https://bit.ly/2M5NBs0  Domain, Hosted web page
//
//
//  External            Note:  Required; use esp8266 by ESP8266 Community version 2.7.4 for "Arduino IDE."
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Some lines require editing with your data.  Such as YourSSID, YourPassword, Your ThinkSpeak ChannelNumber, Your Thinkspeak API Key, Public IP, Forwarded //  Port,
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ********************************************************************************
// ********************************************************************************
//
//   See invidual library downloads for each library license.
//
//   Following code was developed using the Adafruit CC300 library, "HTTPServer" example.
//   and by merging library examples, adding logic for Sketch flow.
//
// *********************************************************************************
// *********************************************************************************
#include <Wire.h>   //Part of the Arduino IDE software download --> Used for I2C Protocol
#include <ESP8266WiFi.h>   //Part of ESP8266 Board Manager install __> Used by WiFi to connect to network
#include <ESP8266FtpServer.h>  //https://github.com/nailbuster/esp8266FTPServer  -->Needed for ftp transfers
#include <ESP8266HTTPClient.h>   //Part of ESP8266 Board Manager install --> Used for Domain web interface
#include <WiFiUdp.h>
#include <sys/time.h>  // struct timeval --> Needed to sync time
#include <time.h>   // time() ctime() --> Needed to sync time       
#include <FS.h>   //Part of ESP8266-Arduino Core --> Needed for SPIFFS (file system of ESP8266.
#include <LittleFS.h>
#include <BME280I2C.h>   //Use the Arduino Library Manager, --> Get BME280 by Tyler Glenn tested for ESP8266.
#include <EnvironmentCalculations.h>  //Use the Arduino Library Manager, get BME280 by Tyler Glenn  
//#include <LiquidCrystal_I2C.h>   //https://github.com/esp8266/Basic/tree/master/libraries/LiquidCrystal --> Used for LCD Display
#include <ThingSpeak.h>   //https://github.com/mathworks/thingspeak-arduino  --> Used for ThingSpeak.com graphing. Get it using the Library Manager.  Requires an account
#include <Ticker.h>  //Part of version 1.0.3 ESP32 Board Manager install  -----> Used for watchdog ISR
#include "variableInput.h"

extern "C"
{
#include "user_interface.h"
}

////////////////////// FTP ////////////////////
// Add this extern
//extern bool FtpClientConnected;

//#define FTP_CTRL_PORT    21          // Command port on wich server is listening
//#define FTP_DATA_PORT_PASV 50009     // Data port in passive mode);

//////////////////////////////////////////////

#define SPIFFS LittleFS

// Replace with your network details
//const char* ssid
//const char* password

///Are we currently connected?
boolean connected = false;

Ticker secondTick;

volatile int watchdogCounter;
volatile int watchDog;

void ISRwatchdog()
{

  watchdogCounter++;

  if (watchdogCounter == 75)
  {

    watchDog = 1;

  }

}

WiFiUDP udp;
//Settings pertain to NTP
//const int udpPort
//NTP Time Servers
//const char * udpAddress1
//const char * udpAddress2
char incomingPacket[255];
char replyPacket[] = "Hi there! Got the message :-)";

#define TZ "EST+5EDT,M3.2.0/2,M11.1.0/2"

int DOW, MONTH, DATE, YEAR, HOUR, MINUTE, SECOND;

char strftime_buf[64];

String dtStamp(strftime_buf);
String lastUpdate;

int lc = 0;
time_t tnow = 0;

//BME280 ////////////////////////////////////////////////

// Assumed environmental values:
float referencePressure = 1021.6;  // hPa local QFF (official meteor-station reading) ->  KEYE, Indianapolis, IND
float outdoorTemp = 28.4;           // °F  measured local outdoor temp.
float barometerAltitude = 250.698;  // meters ... map readings + barometer position  -> 824 Feet  Garmin, GPS measured Altitude.


BME280I2C::Settings settings(
  BME280::OSR_X1,
  BME280::OSR_X1,
  BME280::OSR_X1,
  BME280::Mode_Forced,
  BME280::StandbyTime_1000ms,
  BME280::Filter_16,
  BME280::SpiEnable_False,
  BME280I2C::I2CAddr_0x76
);

BME280I2C bme(settings);

float temp(NAN), temperature, hum(NAN), pres(NAN), heatIndex, dewPoint, absHum, altitude, seaLevel;

float currentPressure;
float millibars;
float pastPressure;
float difference;   //change in barometric pressure drop; greater than .020 inches of mercury.
float HeatIndex;   //Conversion of heatIndex to Farenheit
float DewPoint;    //Conversion of dewPoint to Farenheit
float altFeet;   //Convert of altitude to Feet
float atm;

void getWeatherData()
{
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);

  bme.read(pres, temp, hum, tempUnit, presUnit);

  EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;

  delay(300);

  /// To get correct local altitude/height (QNE) the reference Pressure
  ///    should be taken from meteorologic messages (QNH or QFF)
  /// To get correct seaLevel pressure (QNH, QFF)
  ///    the altitude value should be independent on measured pressure.
  /// It is necessary to use fixed altitude point e.g. the altitude of barometer read in a map
  absHum = EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit);
  altitude = EnvironmentCalculations::Altitude(pres, envAltUnit, referencePressure, outdoorTemp, envTempUnit);
  dewPoint = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
  heatIndex = EnvironmentCalculations::HeatIndex(temp, hum, envTempUnit);
  seaLevel = EnvironmentCalculations::EquivalentSeaLevelPressure(barometerAltitude, temp, pres, envAltUnit, envTempUnit);
  HeatIndex = (heatIndex * 1.8) + 32;
  DewPoint = (dewPoint * 1.8) + 32;
  atm = pres * 0.0009869233;  //Convert hPa to atm
  altFeet = 843;

  temperature = ((temp * 1.8) + 32);  //Convert to Fahrenheit

  currentPressure = seaLevel * 0.02953;   //Convert from hPa to inHg.

}

/////////////////////////////////////////////

int count = 0;
int error;
int started;

unsigned long delayTime;

//use I2Cscanner to find LCD display address, in this case 3F   //https://github.com/todbot/arduino-i2c-scanner/
//LiquidCrystal_I2C lcd(0x3F,16,2);  // set the LCD address to 0x3F for a 16 chars and 2 line display

#define sonalert 9  // pin for Piezo buzzer

#define online D6  //pin for online LED indicator

#define BUFSIZE 64  //Size of read buffer for file download  -optimized for CC3000.

//long int id = 1;  //Increments record number

char *filename;
char str[] = {0};

String fileRead;

char MyBuffer[13];

//String publicIP   //in-place of xxx.xxx.xxx.xxx put your Public IP address inside quotes

//#define LISTEN_PORT          // in-place of yyyy put your listening port number
// The HTTP protocol uses port 80 by default.

#define MAX_ACTION            10      // Maximum length of the HTTP action that can be parsed.

#define MAX_PATH              64      // Maximum length of the HTTP request path that can be parsed.
// There isn't much memory available so keep this short!

#define BUFFER_SIZE           MAX_ACTION + MAX_PATH + 20  // Size of buffer for incoming request data.
// Since only the first line is parsed this
// needs to be as large as the maximum action
// and path plus a little for whitespace and
// HTTP version.

#define TIMEOUT_MS           250  // Amount of time in milliseconds to wait for     /////////default 500/////
// an incoming request to finish.  Don't set this
// too high or your server could be slow to respond.

uint8_t buffer[BUFFER_SIZE + 1];
int bufindex = 0;
char action[MAX_ACTION + 1];
char path[MAX_PATH + 1];

//////////////////////////
// Web Server on port LISTEN_PORT
/////////////////////////
WiFiServer server(PORT);
WiFiClient client;

////////////////////////// FTP /////////////////////////
FtpServer ftpSrv;

//bool FtpClientConnected = true; // or true;
/////////////////////////////////////////////////////////
/*
     This is the ThingSpeak channel number for the MathwWorks weather station
     https://thingspeak.com/channels/YourChannelNumber.  It senses a number of things and puts them in the eight
     field of the channel:

     Field 1 - Temperature (Degrees F)
     Field 2 - Humidity (%RH)
     Field 3 - Barometric Pressure (in Hg)
     Field 4 - Dew Point  (Degree F)
*/

//edit ThingSpeak.com data in "variableInput.h""
//unsigned long myChannelNumber = 12345;
//const char * myWriteAPIKey = "E12345";

int reconnect;

////////////////********************************************************************************
void setup(void)
{

  Serial.begin(115200);

  while (!Serial) {}

  delay(1000);

  Serial.println("");
  Serial.println("");
  Serial.println("Starting...");
  Serial.print("Version 2.3 'NTP_Time-synced_Web_Interface.ino' 02//7/2021 @ 17:12 EST");
  Serial.print("\n");

  wifi_Start();

  secondTick.attach(1, ISRwatchdog);  //watchdog  ISR triggers every 1 second

  pinMode(online, OUTPUT); //Set pinMode to OUTPUT for online LED

  pinMode(4, INPUT_PULLUP); //Set input (SDA) pull-up resistor on GPIO_0 // Change this *! if you don't use a Wemos


  Wire.setClock(2000000);
  Wire.begin(SDA, SCL); //Wire.begin(0, 2); //Wire.begin(sda,scl) // Change this *! if you don't use a Wemos


  while (!bme.begin())
  {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }



  configTime(0, 0, udpAddress1, udpAddress2);
  setenv("TZ", "EST+5EDT,M3.2.0/2,M11.1.0/2", 3);   // this sets TZ to Indianapolis, Indiana
  tzset();

  Serial.print("wait for first valid timestamp ");

  while (time(nullptr) < 100000ul)
  {
    Serial.print(".");
    delay(5000);
  }

  Serial.println(" time synced");

  // Starting the web server
  server.begin();
  Serial.println("Web server running. Waiting for the ESP IP...");

  delay(500);

  started = 1;   //Server started

  // Printing the ESP IP address
  Serial.print("Server IP:  ");
  Serial.println(WiFi.localIP());
  Serial.print("Port:  ");
  Serial.print(LISTEN_PORT);
  Serial.println("\n");

  delay(500);

  ////////////////////////// FTP /////////////////////////////////
  ///////////FTP Setup, ensure SPIFFS is started before ftp;  /////////
  LittleFS.begin();
  Serial.println("LittleFS opened!");
  ftpSrv.begin(ftpUser, ftpPassword);   //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 2121, 50009 for PASV)
  /////////////////////////////////////////////////////////



  //LittleFS.format();
  //LittleFS.remove("/SERVER.TXT");
  //LittleFS.remove("/WIFI.TXT");
  //LittleFS.remove("/LOG.TXT");
  //LittleFS.rename("/LOG715 TXT", "/LOG715.TXT");


  //lcdDisplay();      //   LCD 1602 Display function --used for inital display

  ThingSpeak.begin(client);

  //WiFi.disconnect();  //For testing wifiStart function in setup and listen functions.

  //delay(50 * 1000);  //Uncomment to test watchdog

  //Serial.println("Delay elapsed");

  Serial.println("*** Ready ***");

}



// How big our line buffer should be. 100 is plenty!
#define BUFFER 100

///////////
void loop()
{

  //udp only send data when connected
  if (connected)
  {
    //Send a packet
    udp.beginPacket(udpAddress1, udpPort);
    udp.printf("Seconds since boot: %u", millis() / 1000);
    udp.endPacket();
  }

  listen();

    if(WiFi.status() != WL_CONNECTED) 
  {

      getDateTime();
  
      //Open a "WIFI.TXT" for appended writing.   Client access ip address logged.
      File logFile = LittleFS.open("/WIFI.TXT", "a");
  
      if (!logFile)
      {
        Serial.println("File: '/WIFI.TXT' failed to open");
      }
  
      logFile.print("WiFi Disconnected:    ");
  
      logFile.print(dtStamp);
   
      logFile.print("   Connection result: ");   //Diagnostic test
  
      logFile.println(WiFi.waitForConnectResult());   //Diagnostic test

      logFile.close();
   
      reconnect = 0;

      wifi_Start();

  }

     if((WiFi.status() != WL_CONNECTED))  
     {

          exit;
                    
     }
     else if(reconnect == 1)
     {

          watchdogCounter = 0;  //Resets the watchdogCount

          //Open "WIFI.TXT" for appended writing.   Client access ip address logged.
          File logFile = LittleFS.open("/WIFI.TXT", "a");

          if (!logFile)
          {
               Serial.println("File: '/WIFI.TXT' failed to open");
          }

          getDateTime();

          logFile.print("WiFi Reconnected:     ");

          logFile.print(dtStamp);
    
          logFile.print("   Connection result: ");   //Diagnostic test

          logFile.println(WiFi.waitForConnectResult());   //Diagnostic test

          logFile.println("");

          logFile.close();

          reconnect = 0;

     }


  delay(1);

  if (started == 1)
  {

    // Open "/SERVER.TXT" for appended writing
    File log = LittleFS.open("/SERVER.TXT", "a");



    if (!log)
    {
      Serial.println("file:  '/SERVER.TXT' open failed");
    }

    getDateTime();

    log.print("Started Server:  ");
    log.println(dtStamp) + "  ";
    log.close();

    getWeatherData();

  }

  started = 0;   //only time started = 1 is when Server starts in setup

  if ( watchDog == 1)
  {

    logWatchdog();

    watchDog = 0;

  }

  ///////////////////////////////////////////////////// FTP ///////////////////////////////////
  for (int x = 1; x < 5000; x++)
  {
    ftpSrv.handleFTP();
  }
  ///////////////////////////////////////////////////////////////////////////////////////////////

  getDateTime();

  if ((MINUTE % 15 == 0) && (SECOND == 10))   //Write Data to "LOG.TXT" every 15 MINUTES.
  {

    getWeatherData();

    if (YEAR > 2019)
    {
      lastUpdate = dtStamp;   //store dtstamp for use on dynamic web page
      updateDifference();  //Get Barometric Pressure difference
      logtoSD();   //Output to LittleFS  --Log to LittleFS on 15 minute interval.
      delay(10);  //Be sure there is enough LittleFS write time
      pastPressure = currentPressure;  //Store last 15 MINUTE, currentPressure
      webInterface();
      speak();
    }
    else
    {
      listen;
    }

  }



  //Collect  "LOG.TXT" Data for one day; do it early (before 00:00:00) so day of week still equals 6.
  if ((HOUR == 23 )  &&
      (MINUTE == 57) &&
      (SECOND == 0))
  {

    newDay();
  }

  watchdogCounter = 0;  //Resets the watchdogCount

}

void accessLog()
{

  getDateTime();

  //String ip1String    //Host ip address
  String ip2String = client.remoteIP().toString();   //client remote IP address

  //Open a "access.txt" for appended writing.   Client access ip address logged.
  File logFile = LittleFS.open("/ACCESS.TXT", "a");

  if (!logFile)
  {
    Serial.println("File failed to open");
  }

  if ((ip1String == ip2String) || (ip2String == "0.0.0.0"))
  {

    //Serial.println("IP Address match");
    logFile.close();

  }
  else
  {
    //Serial.println("IP address that do not match ->log client ip address");

    logFile.print("Accessed:  ");
    logFile.print(dtStamp + " -- ");
    logFile.print("Client IP:  ");
    logFile.print(client.remoteIP());
    logFile.print(" -- ");
    logFile.print("Action:  ");
    logFile.print(action);
    logFile.print(" -- ");
    logFile.print("Path:  ");
    logFile.print(path);

    if ((error) == 1)
    {

      //Serial.println("Error 404");
      logFile.println("  Error 404");
      logFile.close();

    }
    else if ((error) == 2)
    {

      //Serial.println("Error 405");
      logFile.println("  Error 405");
      logFile.close();

    }
    else
    {

      logFile.println("");
      logFile.close();

    }

  }

  error = 0;

}

void beep(unsigned char delayms)
{

  // wait for a delayms ms
  digitalWrite(sonalert, HIGH);
  delay(3000);
  digitalWrite(sonalert, LOW);

}

void end()
{

  // Wait a short period to make sure the response had time to send before
  // the connection is closed .

  delay(1000);

  //Client flush buffers
  client.flush();
  // Close the connection when done.
  client.stop();

  digitalWrite(online, LOW);   //turn-off online LED indicator

  getDateTime();

  Serial.println("Client closed:  " + dtStamp);

  delay(1);   //Delay for changing too quickly to new browser tab.

}

void fileStore()   //If 6th day of week, rename "log.txt" to ("log" + month + day + ".txt") and create new, empty "log.txt"
{

  // Open file for appended writing
  File log = LittleFS.open("/LOG.TXT", "a");

  if (!log)
  {
    Serial.println("file open failed");
  }

  // rename the file "LOG.TXT"
  String logname;
  logname = "/LOG";
  logname += MONTH; ////logname += Clock.getMonth(Century);
  logname += DATE;   ///logname += Clock.getDate();
  logname += ".TXT";
  LittleFS.rename("/LOG.TXT", logname.c_str());
  log.close();

  //For troubleshooting
  //Serial.println(logname.c_str());

}

String getDateTime()
{
  struct tm *ti;

  tnow = time(nullptr) + 1;
  //strftime(strftime_buf, sizeof(strftime_buf), "%c", localtime(&tnow));
  ti = localtime(&tnow);
  DOW = ti->tm_wday;
  YEAR = ti->tm_year + 1900;
  MONTH = ti->tm_mon + 1;
  DATE = ti->tm_mday;
  HOUR  = ti->tm_hour;
  MINUTE  = ti->tm_min;
  SECOND = ti->tm_sec;

  strftime(strftime_buf, sizeof(strftime_buf), "%a , %m/%d/%Y , %H:%M:%S %Z", localtime(&tnow));
  dtStamp = strftime_buf;
  return (dtStamp);

}

void links()
{

  client.println("<br>");
  client.println("<a href=http://" + publicIP + ":" + LISTEN_PORT + "/LOG.TXT download>Current Week Observations</a><br>");
  client.println("<br>");
  client.println("<a href=http://" + publicIP + ":" + LISTEN_PORT + "/SdBrowse >File Browser</a><br>");
  client.println("<br>");
  client.println("<a href=http://" + publicIP + ":" + LISTEN_PORT + "/Graphs >Graphed Weather Observations</a><br>");
  client.println("<br>");
  client.println("<a href=http://" + publicIP + ":" + LISTEN_PORT + "/README.TXT download>Server:  README</a><br>");
  client.println("<br>");
  client.println("<a href=https://forum.arduino.cc/index.php?topic=466867.0 >Project Discussion</a><br>");
  client.println("<br>");
  //Show IP Adress on client screen
  client.print("Client IP: ");
  client.print(client.remoteIP().toString().c_str());
  client.println("</body>");
  client.println("</html>");
}

void listen()   // Listen for client connection
{


  watchdogCounter = 0;  //Resets the watchdogCount



  // Check if a client has connected
  client = server.available();

  if (client)
  {

    accessLog();

    // Process this request until it completes or times out.
    // Note that this is explicitly limited to handling one request at a time!

    // Clear the incoming data buffer and point to the beginning of it.
    bufindex = 0;
    memset(&buffer, 0, sizeof(buffer));

    // Clear action and path strings.
    memset(&action, 0, sizeof(action));
    memset(&path,   0, sizeof(path));

    // Set a timeout for reading all the incoming data.
    unsigned long endtime = millis() + TIMEOUT_MS;

    // Read all the incoming data until it can be parsed or the timeout expires.
    bool parsed = false;

    while (!parsed && (millis() < endtime) && (bufindex < BUFFER_SIZE))
    {

      if (client.available())
      {

        buffer[bufindex++] = client.read();

      }

      parsed = parseRequest(buffer, bufindex, action, path);

    }

    if (parsed)
    {



      // Check the action to see if it was a GET request.
      if (strcmp(action, "GET") == 0)
      {

        digitalWrite(online, HIGH);  //turn on online LED indicator

        String ip1String = "10.0.0.146";   //Host ip address
        String ip2String = client.remoteIP().toString();   //client remote IP address

        Serial.print("\n");
        Serial.println("Client connected:  " + dtStamp);
        Serial.print("Client IP:  ");
        Serial.println(ip2String);
        Serial.println(F("Processing request"));
        Serial.print(F("Action: "));
        Serial.println(action);
        Serial.print(F("Path: "));
        Serial.println(path);

        // Check the action to see if it was a GET request.
        if (strncmp(path, "/favicon.ico", 12) == 0)  // Respond with the path that was accessed.
        {
          char *filename = "/FAVICON.ICO";
          strcpy(MyBuffer, filename);

          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: image/x-icon");
          client.println();

          readFile();

        }
        // Check the action to see if it was a GET request.
        if ((strcmp(path, "/Weather") == 0) || (strcmp(path, "/") == 0))   // Respond with the path that was accessed.
        {

          if (!isnan(temp))
          {
            delay(200);

            // First send the success response code.
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: html");
            client.println("Connnection: close");
            client.println("Server: Robotdyn WiFi D1 R2");
            // Send an empty line to signal start of body.
            client.println("");
            // Now send the response data.
            // output dynamic webpage
            client.println("<!DOCTYPE HTML>");
            client.println("<html lang='en'>");
            client.println("<head><title>Weather Observations</title></head>");
            client.println("<body>");
            // add a meta refresh tag, so the browser pulls again every 15 seconds:
            //client.println("<meta http-equiv=\"refresh\" content=\"15\">");
            client.println("<h2>Treyburn Lakes<br>");
            client.println("Indianapolis, IN 46239</h2><br>");

            if (lastUpdate != NULL)
            {
              client.println("Last Update:  ");
              client.println(lastUpdate);
              client.println("   <br>");
            }

            ////////////////////////////////////////////////////////////////////////////
            //
            //  Variables to be displayed in Client's Browser go here.
            //
            ////////////////////////////////////////////////////////////////////////////

            client.println("Humidity:  ");
            client.print(hum, 1);
            client.print(" %<br>");
            client.println("Dew point:  ");
            client.print(DewPoint, 1);
            client.print(" F. <br>");
            client.println("Temperature:  ");
            client.print(temperature, 1);
            client.print(" F.<br>");
            client.println("Heat Index:  ");
            client.print(HeatIndex, 1);
            client.print(" F. <br>");
            client.println("Barometric Pressure:  ");
            client.print(currentPressure, 3);   //Inches of Mercury
            client.print(" inHg.<br>");

            if (pastPressure == currentPressure)
            {
              client.println("0.000");
              client.print(" inHg --Difference since last update <br>");
            }
            else
            {
              client.println(difference, 3);
              client.print(" inHg --Difference since last update <br>");
            }

            client.println("Barometric Pressure:  ");
            client.println(seaLevel, 1);   //Convert hPA to millbars
            client.println(" mb.<br>");
            client.println("Atmosphere:  ");
            client.print(atm, 2);
            client.print(" atm <br>");
            client.println("Elevation:  843 Feet<br>");
            client.println("<h2>Weather Observations</h2>");
            client.println("<h3>" + dtStamp + "    </h3><br>");

            links();

            end();

          }
          else
          {

            // First send the success response code.
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: html");
            client.println("Connnection: close");
            client.println("Server: Robotdyn WiFi D1 R2");
            // Send an empty line to signal start of body.
            client.println("");
            // Now send the response data.
            // output dynamic webpage
            client.println("<!DOCTYPE HTML>");
            client.println("<html>");
            client.println("<head><title>Weather Observations</title>");
            client.println("<body>");
            // add a meta refresh tag, so the browser pulls again every 15 seconds:
            //client.println("<meta http-equiv=\"refresh\" content=\"15\">");
            client.println("<h2>Treyburn Lakes<br>");
            client.println("Indianapolis, IN 46239</h2></head><br>");

            if (lastUpdate != NULL)
            {
              //client.println("Last Update:  ");
              client.println(lastUpdate);
              client.println("   <br>");
            }

            client.println("<h2>Invalid Data --Sensor Failure</h2\n");
            client.println("</body>");
            client.println("</html>");

            links();

            end();

          }


        }
        // Check the action to see if it was a GET request.
        else if (strcmp(path, "/SdBrowse") == 0)   // Respond with the path that was accessed.
        {

          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<body>");
          client.println("<head><title>SDBrowse</title><head />");
          // print all the files, use a helper to keep it clean
          client.println("<h2>Collected Observations<br><br>");

          //////////////// Code to listFiles from martinayotte of the "ESP8266 Community Forum" ///////////////
          //////// Modified for LittleFS  /////////////////////////////////////////////////////////////////////

          String str;

          if (!LittleFS.begin())
          {
            Serial.println("LittleFS failed to mount !");
          }
          Dir dir = LittleFS.openDir("/");
          while (dir.next())
          {

            String file_name = dir.fileName();

            if (file_name.startsWith("LOG"))
            {

              str += "<a href=\"";
              str += dir.fileName();
              str += "\">";
              str += dir.fileName();
              str += "</a>";
              str += "    ";
              str += dir.fileSize();
              str += "<br>\r\n";
            }
          }

          client.print(str);

          ////////////////// End code by martinayotte //////////////////////////////////////////////////////
          client.println("<br><br>");
          client.println("<a href=http://" + publicIP + ":" + LISTEN_PORT + "/Weather    >Current Observations</a><br>");
          client.println("</body></h2>");
          client.println("</html>");

          end();

        }
        // Check the action to see if it was a GET request.
        else if (strcmp(path, "/Graphs") == 0)   // Respond with the path that was accessed.
        {

          //delayTime =1000;

          // First send the success response code.
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: html");
          client.println("Connnection: close");
          client.println("Server: Robotdyn D1 R2");
          // Send an empty line to signal start of body.
          client.println("");
          // Now send the response data.
          // output dynamic webpage
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<body>");
          client.println("<head><title>Graphed Weather Observations</title></head>");
          // add a meta refresh tag, so the browser pulls again every 15 seconds:
          //client.println("<meta http-equiv=\"refresh\" content=\"15\">");
          client.println("<br>");
          client.println("<h2>Graphed Weather Observations</h2><br>");
          client.println("<p>");
          client.println("<frameset rows='30%,70%' cols='33%,34%'>");
          client.println("<iframe width='450' height='260' 'border: 1px solid #cccccc;' src='https://thingspeak.com/channels/290421/charts/1?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=60&timescale=15&title=Temperature&type=line&xaxis=Date&yaxis=Temperature++++%C2%B0++F.'></iframe>");
          client.println("<iframe width='450' height='260' style='border: 1px solid #cccccc;' src='https://thingspeak.com/channels/290421/charts/2?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=60&title=Relative+Humidity&type=line&xaxis=Date+and+Time&yaxis=Relative+Humidity++%25'></iframe>");
          client.println("<br>");
          client.println("<br>");
          delayTime = 200;
          //client.println("</frameset>");
          //client.println("<frameset rows='30%,70%' cols='33%,34%'>");
          client.println("<iframe width='450' height='260' style='border: 1px solid #cccccc;' src='https://thingspeak.com/channels/290421/charts/3?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=60&title=Barometric+Pressure&type=line&xaxis=Date+and+Time&yaxis=Pressure+++inHg'></iframe>");
          client.println("<iframe width='450' height='260' style='border: 1px solid #cccccc;' src='https://thingspeak.com/channels/290421/charts/4?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=60&title=Dew+Point&type=line&xaxis=Date+and+Time&yaxis=Dew+Point++++%C2%B0++F.'></iframe>");
          client.println("</frameset>");
          client.println("<br><br>");
          client.println("<a href=http://" + publicIP + ":" +  LISTEN_PORT + "/Weather    >Current Observations</a><br>");
          client.println("<br>");
          client.println("</p>");
          client.println("</body>");
          client.println("</html>");

          end();

        }

        else if ((strncmp(path, "/LOG", 4) == 0) ||  (strcmp(path, "/WIFI.TXT") == 0) || (strcmp(path, "/DIFFER.TXT") == 0) || (strcmp(path, "/SERVER.TXT") == 0) || (strcmp(path, "/README.TXT") == 0))   // Respond with the path that was accessed.
        {

          char *filename;
          char name;
          strcpy( MyBuffer, path );
          filename = &MyBuffer[1];

          if ((strncmp(path, "/FAVICON.ICO", 12) == 0) || (strncmp(path, "/SYSTEM~1", 9) == 0) || (strncmp(path, "/ACCESS", 7) == 0))
          {

            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: text/html");
            client.println();
            client.println("<h2>404</h2>");
            delay(250);
            client.println("<h2>File Not Found!</h2>");
            client.println("<br><br>");
            client.println("<a href=http://" + publicIP + ":" + LISTEN_PORT + "/SdBrowse    >Return to File Browser</a><br>");

            Serial.println("Http 404 issued");

            error = 1;

            end();




          }
          else
          {

            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/plain");
            client.println("Content-Disposition: attachment");
            client.println("Content-Length:");
            client.println("Connnection: close");
            client.println();

            readFile();

            end();
          }

        }
        // Check the action to see if it was a GET request.
        else if (strncmp(path, Restricted, 14) == 0)  //replace "/zzzzzzz" with your choice.  Makes "ACCESS.TXT" a restricted file.  Use this for private access to remote IP file.
        {
          //Restricted file:  "ACCESS.TXT."  Attempted access from "Server Files:" results in
          //404 File not Found!

          char *filename = "/ACCESS.TXT";
          strcpy(MyBuffer, filename);

          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/plain");
          client.println("Content-Disposition: attachment");
          //client.println("Content-Length:");
          client.println();

          readFile();

          end();

        }
        else
        {

          delay(1000);

          // everything else is a 404
          client.println("HTTP 404 Not Found");
          client.println("Content-Type: text/html");
          client.println();
          client.println("<h2>404</h2>");
          delay(250);
          client.println("<h2>File Not Found!</h2>");
          client.println("<br><br>");
          client.println("<a href=http://" + publicIP + ":" + LISTEN_PORT + "/SdBrowse    >Return to File Browser</a><br>");

          Serial.println("Http 404 issued");

          error = 1;

          end();





        }

      }
      else
      {
        // Unsupported action, respond with an HTTP 405 method not allowed error.
        Serial.print(action);
        client.println("HTTP Method Not Allowed");
        client.println("");
        Serial.println("");
        Serial.println("Http 405 issued");
        Serial.println("");

        digitalWrite(online, HIGH);   //turn-on online LED indicator

        error = 2;

        end();

      }



    }



  }




}

//********************************************************************
//////////////////////////////////////////////////////////////////////
// Return true if the buffer contains an HTTP request.  Also returns the request
// path and action strings if the request was parsed.  This does not attempt to
// parse any HTTP headers because there really isn't enough memory to process
// them all.
// HTTP request looks like:
//  [method] [path] [version] \r\n
//  Header_key_1: Header_value_1 \r\n
//  ...
//  Header_key_n: Header_value_n \r\n
//  \r\n
bool parseRequest(uint8_t* buf, int bufSize, char* action, char* path)
{
  // Check if the request ends with \r\n to signal end of first line.
  if (bufSize < 2)
    return false;

  if (buf[bufSize - 2] == '\r' && buf[bufSize - 1] == '\n')
  {
    parseFirstLine((char*)buf, action, path);
    return true;
  }
  return false;
}


///////////////////////////////////////////////////////////////////
// Parse the action and path from the first line of an HTTP request.
void parseFirstLine(char* line, char* action, char* path)
{
  // Parse first word up to whitespace as action.
  char* lineaction = strtok(line, " ");

  if (lineaction != NULL)

    strncpy(action, lineaction, MAX_ACTION);
  // Parse second word up to whitespace as path.
  char* linepath = strtok(NULL, " ");

  if (linepath != NULL)

    strncpy(path, linepath, MAX_PATH);
}

void logtoSD()   //Output to LittleFS Card every fifthteen minutes
{

  // Open a "log.txt" for appended writing
  File log = LittleFS.open("/LOG.TXT", "a");

  if (!log)
  {
    Serial.println("file open failed");
  }

  //log.print(id);
  //log.print(" , ");

  //////////////////////////////////////////////////////////////
  //
  // Variable to log go here.
  //
  //////////////////////////////////////////////////////////////

  log.print(lastUpdate);
  log.print(" , ");
  log.print("Humidity:  ");
  log.print(hum);
  log.print(" % , ");
  log.print("Dew Point:  ");
  log.print(DewPoint, 1);
  log.print(" F. , ");
  log.print(temperature);
  log.print("  F. , ");
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  log.print("Heat Index:  ");
  log.print(HeatIndex, 1);
  log.print(" F. ");
  log.print(" , ");
  log.print(currentPressure, 3);   //Inches of Mecury
  log.print(" in. Hg. ");
  log.print(" , ");

  if (pastPressure == currentPressure)
  {
    log.print("0.000");
    log.print(" Difference ");
    log.print(" ,");
  }
  else
  {
    log.print(difference, 3);
    log.print(" Difference ");
    log.print(", ");
  }

  log.print(seaLevel, 2);  //Convert hPA to millibars
  log.print(" millibars ");
  log.print(" , ");
  log.print(atm, 3);
  log.print(" atm ");
  log.print(" , ");
  log.print("Elevation:  843 Feet");
  log.println();
  //Increment Record ID number
  //id++;
  Serial.print("\n");
  Serial.println("Data written to 'LOG.TXT'  " + dtStamp);
  Serial.print("\n");

  log.close();

  pastPressure = currentPressure;

  if (abs(difference) >= .020)  //After testing and observations of Data; raised from .010 to .020 inches of Mecury
  {
    // Open a "DIFFER.TXT" for appended writing --records Barometric Pressure change difference and time stamps
    File diff = LittleFS.open("/DIFFER.TXT", "a");

    if (!diff)
    {
      Serial.println("file open /DIFFER.TXT  failed");
    }

    Serial.println("\n");
    Serial.println("Difference greater than .020 inches of Mecury ,  ");
    Serial.print(difference, 3);
    Serial.println("\n");
    Serial.print("  ,");
    Serial.print(dtStamp);
    Serial.println("");

    diff.println("");
    diff.print("Difference greater than .020 inches of Mecury,  ");
    diff.print(difference, 3);
    diff.print("  ,");
    diff.print(dtStamp);

    diff.close();




    beep(50);  //Duration of Sonalert tone

  }

}

void logWatchdog()
{

  yield();

  Serial.println("");
  Serial.println("Watchdog event triggered.");

  // Open a "log.txt" for appended writing
  File log = LittleFS.open("/WATCHDOG.TXT", "a");

  if (!log)
  {
    Serial.println("file 'WATCHDOG.TXT' open failed");
  }

  getDateTime();

  log.print("Watchdog Restart:  ");
  log.print("  ");
  log.print(dtStamp);
  log.println("");
  log.close();

  Serial.println("Watchdog Restart  " + dtStamp);
  Serial.println("\n");

  if (WiFi.status() != WL_CONNECTED)
  {

    reconnect = 2;
    
    watchdogCounter = 0;

    exit;

  }
  else
  {

    ESP.restart();

  }



}


void newDay()   //Collect Data for twenty-four hours; then start a new day
{

  //Do file maintence on 1st day of week at appointed time from RTC.  Assign new name to "log.txt."  Create new "log.txt."
  if ((DOW) == 6)
  {
    fileStore();
  }

  //id = 1;   //Reset id for start of new day
  //Write log Header

  // Open file for appended writing
  File log = LittleFS.open("/LOG.TXT", "a");

  if (!log)
  {
    Serial.println("file open failed");
  }
  else
  {

    delay(1000);
    log.println("......"); //Just a leading blank line, in case there was previous data
    log.println("Date, Time, Humidity, Dew Point, Temperature, Heat Index, in. Hg., Difference, millibars, Atm, Elevation");
    log.close();

    Serial.println("......");
    Serial.println("Date, Time, Humidity, Dew Point, Temperature, Heat Index, in. Hg., Difference, millibars, Atm, Elevation");
    Serial.println("");

  }
  Serial.println("\n");

}

void readFile()
{

  digitalWrite(online, HIGH);   //turn-on online LED indicator

  String filename = (const char *)&MyBuffer;

  Serial.print("File:  ");
  Serial.println(filename);

  File webFile = SPIFFS.open(filename, "r");

  if (!webFile)
  {
    Serial.println("File failed to open");
    Serial.println("\n");
  }
  else
  {
    char buf[1024];
    int siz = webFile.size();
    //.setContentLength(str.length() + siz);
    //webserver.send(200, "text/plain", str);
    while (siz > 0)
    {
      size_t len = std::min((int)(sizeof(buf) - 1), siz);
      webFile.read((uint8_t *)buf, len);
      client.write((const char*)buf, len);
      siz -= len;
    }
    webFile.close();
  }

  error = 0;

  delayTime = 1000;

  MyBuffer[0] = '\0';

  digitalWrite(online, LOW);   //turn-off online LED indicator

  listen();

}

void speak()
{

  char t_buffered1[14];
  dtostrf(temperature, 7, 1, t_buffered1);

  char t_buffered2[14];
  dtostrf(hum, 7, 1, t_buffered2);

  char t_buffered3[14];
  dtostrf(currentPressure, 7, 3, t_buffered3);

  char t_buffered4[14];
  dtostrf(DewPoint, 7, 1, t_buffered4);

  // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
  // pieces of information in a channel.  Here, we write to field 1.
  // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
  // pieces of information in a channel.  Here, we write to field 1.
  ThingSpeak.setField(1, t_buffered1);  //Temperature
  ThingSpeak.setField(2, t_buffered2);  //Humidity
  ThingSpeak.setField(3, t_buffered3);  //Barometric Pressure
  ThingSpeak.setField(4, t_buffered4);  //Dew Point F.

  // Write the fields that you've set all at once.
  ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  getDateTime();

  Serial.println("");
  Serial.println("Sent data to Thingspeak.com  " + dtStamp + "\n");

  //listen();  --needs to pass thru loop function

}

float updateDifference()  //Pressure difference for fifthteen minute interval
{
  //Function to find difference in Barometric Pressure
  //First loop pass pastPressure and currentPressure are equal resulting in an incorrect difference result.  Output "...Processing"
  //Future loop passes difference results are correct

  difference = currentPressure - pastPressure;  //This will be pressure from this pass thru loop, pressure1 will be new pressure reading next loop pass
  if (difference == currentPressure)
  {
    difference = 0;
  }
  return (difference); //Barometric pressure change in inches of Mecury

}

void webInterface()
{

  char fahren[9];// Buffer big enough for 9-character float
  dtostrf(temperature, 6, 1, fahren); // Leave room for too large numbers!

  char heat[9]; // Buffer big enough for 9-character float
  dtostrf(HeatIndex, 6, 1, heat); // Leave room for too large numbers!

  char humd[9]; // Buffer big enough for 9-character float
  dtostrf(hum, 6, 1, humd); // Leave room for too large numbers!

  char dewpt[9]; // Buffer big enough for 9-character float
  dtostrf(DewPoint, 6, 1, dewpt); // Leave room for too large numbers!

  char cpressure[9]; // Buffer big enough for 9-character float
  dtostrf(currentPressure, 6, 3, cpressure); // Leave room for too large numbers!

  char diff[9]; // Buffer big enough for 7-character float
  dtostrf(difference, 8, 3, diff); // Leave room for too large numbers!

  char bars[9]; // Buffer big enough for 9-character float
  dtostrf(seaLevel, 6, 2, bars); // Leave room for too large numbers!


  String data = "fahren="
                +  (String)fahren

                + "&heat="                +  heat

                + "&humd="                 +  humd

                + "&dewpt="               +  dewpt

                + "&cpressure="           +  cpressure

                + "&diff="                +  diff

                + "&bars="                +  bars

                + "&last="                +  lastUpdate;


  if (WiFi.status() == WL_CONNECTED)
  {
    //Check WiFi connection status

    HTTPClient http;    //Declare object of class HTTPClient

    http.begin("http://observations-weather.000webhostapp.com/dataCollector.php");      //Specify request destination
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //Specify content-type header

    int httpCode = http.POST(data);   //Send the request
    String payload = http.getString();                  //Get the response payload

    if (httpCode == 200)
    {
      Serial.print("");
      Serial.print("HttpCode:  ");
      Serial.print(httpCode);   //Print HTTP return code
      Serial.print("  Data echoed back from Hosted website  " );
      Serial.println("");
      Serial.println(payload);    //Print payload response

      http.end();  //Close HTTPClient http
    }
    else
    {
      Serial.print("");
      Serial.print("HttpCode:  ");
      Serial.print(httpCode);   //Print HTTP return code
      Serial.print("  Domain website data update failed.  ");
      Serial.println("");

      http.end();   //Close HTTPClient http
    }

    end();

  }
  else
  {

    Serial.println("Error in WiFi connection");

  }

}

void wifi_Start()
{

  WiFi.mode(WIFI_STA);

  //wifi_set_sleep_type(NONE_SLEEP_T);

  Serial.println();
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);


  //setting the static addresses in function "wifi_Start
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;


  WiFi.begin(ssid, password);

  WiFi.config(ip, gateway, subnet, dns);

  WiFi.waitForConnectResult();

  Serial.printf("Connection result: %d\n", WiFi.waitForConnectResult());

  if ((WiFi.status() != WL_CONNECTED))
  {

    delay(60 * 1000);
    
    wifi_Start();

    watchdogCounter = 0;
    
  }
  
  if((WiFi.status() == WL_CONNECTED))
  {
    
      reconnect = 1;  
    
      watchdogCounter = 0;
    
  }

}
