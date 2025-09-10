/* ############ WiFi CREDENTIALS ############
Self explanatory. ESP32 supports only 2.4GHz WiFi stations. */
String ssid = "YourSSID";
String password = "UltraSecurePassword";

/* ############ HTTP SERVERS ############ */
//Here you can put a link to your 960x540 BMP/JPG file (or script that generates it).
//30px white bar at the top would be great if you want top bar to be visible
String imageUrl = "https://yourdomain.com/jpg.php";

//As you probably know, this program supports sending data from DHT11 sensor to HTTP server. 
//There is a sample PHP script in the repository to receive and save that data on server.
//Put url adress here that points to location that supports the same data input as sample script.
String serverUrl = "https://yourdomain.com/sensor.php";
//Or disable it completely if you do not have DHT11.
bool enableDHT = false;

/* ############ FILE TYPE CHOICE ############ */
bool enableBMP = false;
bool enableJPG = true;

/* ############ TOP BAR ############ */
//change this to false to disable top bar (battery charge, wifi signal and room temperature readings) 
bool enableTopBar = true;

/* ############ SLEEP DURATION ############
Configure how long you want your esp to sleep between wake-ups (in minutes).
Sleep time is aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour. 
By default 60 -> wake up every hour. */
long sleepDuration = 60; 

/* ############ RUNTIME ESTIMATION ############ 
Enable or disable time until next charging on the top of the screen.*/
bool enableEstimation = true;
//put there your battery's capacity in mAh
long batCapacity = 2000;

/* ############ TIME CONFIG ############
Adjust those settings if your timezone is different than Europe/Warsaw. 
Reference: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv */
String Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
String ntpServer = "0.pl.pool.ntp.org";            
int gmtOffset_sec = 0;
int daylightOffset_sec = 3600;

/* ############ DEBUG INFO ############ */
//Change to true if you want to see debug information in serial monitor.
bool enableDebug = false;