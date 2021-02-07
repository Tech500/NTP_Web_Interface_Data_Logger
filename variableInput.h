//
//   "NTP_Web_Interface_Data_Logger.ino" and  
//   variableInput.h place in sketch folder
//   William M. Lucid   07/06/2020 @ 11:32 EDT    
// 

// Replace with your network details  
//const char * host  = "esp8266"; 

// Replace with your network details
const char * ssid = "R2D2";
const char * password = "sissy4357";

//Settings pertain to NTP
const int udpPort = 123;
//NTP Time Servers

const char * udpAddress1 = "us.pool.ntp.org";
const char * udpAddress2 = "time.nist.gov";

//publicIP accessiable over Internet with Port Forwarding; know the risks!!!
//WAN IP Address.  Or use LAN IP Address --same as server ip; no Internet access. 
String publicIP = "73.102.122.239";  //Part of href link for "GET" requests
String LISTEN_PORT = "8010"; //Part of href link for "GET" requests

//String linkAddress = "xx.xx.xx.xx:8010";  //"xx.xx.xx.xx" = publicIP  "yyyy" = "PORT used as String"

String ip1String = "10.0.0.146";   //Host ip address  --prevents logging host computer

int PORT = 8010;  //Web Server port

//Graphing requires "FREE" "ThingSpeak.com" account..  
//Enter "ThingSpeak.com" data here....
//Example data; enter yout account data..
unsigned long myChannelNumber = 290421;
const char * myWriteAPIKey = "LYRTHG76RTS69V0Z";


//Server settings
#define ip {10,0,0,26}
#define subnet {255,255,255,0}
#define gateway {10,0,0,1}
#define dns {10,0,0,1}

//FTP Credentials
const char * ftpUser = "Trump";
const char * ftpPassword = "Reddog60$";
 
//Restricted Access
const char* Restricted = "/Reddog.TXT";  //Can be any filename.  
//Will be used for "GET" request path to pull up client ip list.

///////////////////////////////////////////////////////
