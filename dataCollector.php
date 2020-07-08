<?php

/*
  ESP8266: send data to your Domain, Hosted web site 

  Uses POST command to send DHT data to a designated website
  The circuit:
  * BME280
  * Post to Domain 
  
  Original version of this code by Stephan Borsay
  Modified by William Lucid to use BME280 along with ESP8266 sketch:  "NTP_Time-sycned_Web_Interface.ino."
*/

date_default_timezone_set("America/Indianapolis"); 
$TimeStamp = date("Y-m-d   h:i:sa");

   if( $_REQUEST["fahren"] ||  $_REQUEST["heat"] ||  $_REQUEST["humd"] ||  $_REQUEST["dewpt"]
       ||  $_REQUEST["cpressure"] ||  $_REQUEST["bars"]  || $_REQUEST["last"]) 
   {
      echo " The Date and Time is: ". $_REQUEST['last']."" ;
      echo " The Temperature is: ". $_REQUEST['fahren']. " F. ";
      echo " The Heat Index is: ". $_REQUEST['heat']. " F. ";
	  echo " The Humidity is: ". $_REQUEST['humd']. " %  ";
	  echo " The Dew Point is: ". $_REQUEST['dewpt']. " F. ";
      echo " The Barometric Pressure is: ". $_REQUEST['cpressure']. " inHG.  ";
      echo " The Barometric Pressure is: ". $_REQUEST['bars']. " millibars  ";
	  echo " The Elevation is: 824 Feet  ";

	 
   }
	  
	
$var1 = $_REQUEST['fahren']; 
$var2 = $_REQUEST['heat'];
$var3 = $_REQUEST['humd'];
$var4 = $_REQUEST['dewpt'];
$var5 = $_REQUEST['cpressure'];
$var6 = $_REQUEST['bars'];
$var7 = $_REQUEST['altit'];
$var8 = $_REQUEST['last'];

$WriteMyRequest= 


"<!DOCTYPE html>".
"<html><head><body><div class='centered'>".
"<p>".
"<p><h2>Treyburn Lakes<br>".
"Indianapolis, IN, US".
"</h2>".
"<p><h3>Last update:   " . $var8 . "</p>".
"<p> The Temperature is : "   		. $var1 . " F. </p>".
"<p>And the Heat Index is : " 		. $var2 . " F. </p>".
"<p>And the Humidity : "   		     . $var3 . " % </p>".
"<p>And The Dew Point is : "  		. $var4 . " F. </p>".
"<p>And The Barometric Pressure is : "  . $var5 . " inHG. </p>".
"<p>And The Barometric Pressure is : "  . $var6 . " mb</p>".
"<p>And The Elevation is :  824 Feet</p>".
"<p></p>".
"<p>".
"<a href='https://forum.arduino.cc/index.php?topic=466867.0' style='color: #ffffff' >Project Discussion</a></h3></p>".
"</div></body></head></html>";

file_put_contents('dataDisplayer.html', $WriteMyRequest);

?>