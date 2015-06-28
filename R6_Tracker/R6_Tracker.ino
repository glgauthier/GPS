/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 This code is based off of an example from https://learn.adafruit.com/gps-dog-collar/code
 I originally intended to use it to make a homemade GPS watch,
 but now I'm planning on using it for a distance tracker for when I'm kayaking.
 I'm looking at adding in support for writing waypoints to the EEPROM and getting rid of the "goal"
 feature, as well as writing a matlab GUI to take the waypoints and overlay them on a map
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

//Connect GPS TX to Arduino Uno Pin D3 
//Connect GPS RX to Arduino Uno Pin D2 
//Connect momentary switches between the 5V rail and pins D9 and D8, with 10K pulldowns to GND
//Connect your 16x2 LCD to digital pins 12, 11, 7, 6, 5, and 4

// Link the following libraries
#include <SoftwareSerial.h> 
#include <TinyGPS.h> // This library can be found at http://arduiniana.org/libraries/tinygps/
#include <Wire.h>
#include <LiquidCrystal.h>


LiquidCrystal LCD(12, 11, 7, 6, 5, 4); // Define which pins the LCD is connected to 

TinyGPS gps; // Create a name for the GPS module (used for sending commands to it)
SoftwareSerial nss(3,2); // Set up a software serial connection to the GPS module

// Function Prototypes
static void gpsdump(TinyGPS &gps);
static bool feedgps();
static void changemode();
void printLCDFloat(double number, int digits);
void timekeep();

// Set the desired goal to 3 miles (this can be changed using the goal button)
//float GOAL = 3; // Distances can include decimal points

// Global variables for maximum speed and last known position (not all are used)
float maxSpeed = 0;
float lastFlat = 0;
float lastFlon = 0;
long totalDistance = 0;
float fDist; 

// Global variables for saving elapsed time
int seconds;
int tseconds;
int minutes;
int tminutes;
int hours;

boolean start = 1;
int i = 0; // Counting variable for loops
int run = 0; // Variable used to switch between stop/start modes

// Setup loop: Runs once at boot
void setup() {
  nss.begin(9600); // Start a software serial connection with the GPS module, WAS 9600
  LCD.begin(16, 2); // Initialize the interface to the 16x2 lcd screen...
  LCD.clear(); //... and clear whatever's on the screen
  
 // Serial.begin(9600);
  
  // Set up digital pins 8 and 9 for button inputs (Start/Stop and Goal)
  pinMode(8, INPUT);
  //pinMode(9, INPUT);
  
  // Print to the LCD that the GPS module is acquiring satellites
  LCD.setCursor(0,1);
  LCD.print("Getting A Fix...");
}

// Main loop: Repeats endlessly after the Setup loop has finished
void loop() {
  
 //-------------------------------------When time is stopped---------------------------------
  while(run == 0){ // Do the following things while the run vairable is set to FALSE

    // If the Start/Stop button is pressed call the changemode() function
    if(digitalRead(8) == HIGH) changemode(); // changemode toggles the 'run' variable
    
    // If there is new data available from the GPS module ...
    if(feedgps()) {
      // On the second line of the display...
      LCD.setCursor(0, 1);
      // Print the number of satellites that the module is connected to 
      // (i.e. that it's ready to use for tracking)
      LCD.print(gps.satellites() == TinyGPS::GPS_INVALID_SATELLITES ? 0 : gps.satellites());
      LCD.print(" ");
      LCD.print("Sats   "); 
      // Also print the current runtime, this is 0:00:00 at boot
      LCD.print(hours);
      LCD.print(":");
      LCD.print(tminutes);
      LCD.print(minutes);
      LCD.print(":");
      LCD.print(tseconds);
      LCD.print(seconds);
    }
    
    LCD.setCursor(0,0); // On the first line of the display...
    LCD.print("Paused - "); // Indicate that the time/tracking is stopped
    fDist = totalDistance; // Create a temporary variable for the total distance traveled
    fDist *= 0.000621371192; // Then convert that distance from meters to miles ... 
    printLCDFloat(fDist, 2); // ... And print it to the LCD to two decimal places ...
    LCD.print(" Mi"); // ... Indicating that it's a distance in miles
  }
  
  //-------------------------------------When time is running---------------------------------
  while(run == 1) { // Do the following things while the run variable is set to TRUE
                    // this while loop is the outer loop (see block comment)
    
    /* This loop works by incrememnting the time that it has been excecuting for in millis.
    * Every 1000 millis (1 second), the code in the outside of the loop excecutes.
    * During the time in between each second, the inner loop excecutes 
    */
    
    if(digitalRead(8)== HIGH)  changemode(); // Check on the start/stop button
   
    
    bool newdata = false; // Indicate that any new data has been processesd
    unsigned long start = millis(); // Create a timing variable 
  
    // This while loop is the inner loop, and excecutes in the time between each second
    while (millis() - start < 1000) { // while the time is between 0 and 1 seconds
      if(digitalRead(8)== HIGH) changemode(); // Check on the start/stop button
      if (feedgps()) newdata = true; // if there is new data available, set boolean newdata to TRUE
    }
    
    seconds++; // Every time the inner loop is broken out of, increment the time in seconds ...
    timekeep(); // ... and update the global time in H:MM:SS
    gpsdump(gps); // Then send the current gps data to the gpsdump() loop
    
    // On the second line of the display, print out the current time
    LCD.setCursor(0,1);
    String timestring;
    timestring = String(hours)+":"+String(tminutes)+String(minutes)+":"+String(tseconds)+String(seconds)+"  ";
    LCD.print(timestring);
   
    //and the number of sats
    LCD.print(gps.satellites() == TinyGPS::GPS_INVALID_SATELLITES ? 0 : gps.satellites());
    LCD.print(" ");
    LCD.print("Sats   ");
  
  }
}

/* gpsdump function: consumes data from the GPS module, extracts the latitude, longitude, age, date,
* and time, and then calculates the total distance traveled and the percentage of the goal reached
* and prints it all to the LCD, returning nothing.
*/
static void gpsdump(TinyGPS &gps) { 
  float flat, flon; // Create floating point variables for the latitude and longitude
  unsigned long age, date, time; // and integer values for age, date, and time
  
  // Then write the current lat, long, and age to the newly created vars
  gps.f_get_position(&flat, &flon, &age); 
  
  // If movement has been detected 
  if (gps.f_speed_kmph() > 3.9) {
    if (start == 1) { //  If the boolean variable start is set to TRUE
      start = 0; // Set it to false
      lastFlat = flat; // And replace the last lat and long values with the current ones
      lastFlon = flon;
    }
    else { // If the boolean variable start is set to FALSE
      // Update the total distance traveled using the calc_dist function
      totalDistance = totalDistance + calc_dist(flat, flon, lastFlat, lastFlon);
      lastFlat = flat; // And then replace the last lat and long values with the current ones
      lastFlon = flon;
    }
  }
  
  // Clear the LCD, and prepare to write to the first line of it
  LCD.clear();
  LCD.setCursor(0,0);
  
  fDist = totalDistance; // Store the distance to temporary variable fDist
  //convert meters to miles
  fDist *= 0.000621371192; // Convert that stored number from meters to miles
  printLCDFloat(fDist, 2); // And then print it to the LCD, 
  LCD.print(" Miles "); // Indicating that it's in miles
  
  float speed =  gps.speed()/100; // Then, create a decimal number for the percent of the goal reached
  printLCDFloat(speed, 1); // Turn the decimal into a percent and print it to the LCD
  LCD.print("Kn");
  
  //Debug to make sure I'm not getting any overflow
  //Serial.println(nss.overflow() ? "YES!" : "No");
 
   
}

/* printLCDFloat function: consumes a decimal number (number) and the number of decimal places
* to print out (digits), and then prints that number at the specified length to the LCD screen,
* returning nothing
*/
void printLCDFloat(double number, int digits) {
  
  if (number < 0.0) { // If the number being passed is negative,
     LCD.print("-"); // print a negative sign before it
     number = -number; // and then set it to a positive number so that the rest of the function can use it
  }

  // Round correctly so that print(1.999, 2) prints as "2.00"
  double rounding = 0.5;
  for (uint8_t i=0; i<digits; ++i) rounding /= 10.0;
  number += rounding;

  // Extract the integer part of the number and print it
  unsigned long int_part = (unsigned long)number;
  double remainder = number - (double)int_part;
  char sTemp[10];
  ltoa(int_part, sTemp, 10);
  LCD.print(sTemp);

  // Print the decimal point, but only if there are digits beyond
  if (digits > 0) LCD.print("."); 

  // Extract digits from the remainder one at a time
  while (digits-- > 0) {
    remainder *= 10.0;
    int toPrint = int(remainder);
    ltoa(toPrint, sTemp, 10);
    LCD.print(sTemp); // and print them to the LCD
    remainder -= toPrint; 
  } 
}

/* feedgps: consumes nothing and returns a boolean value indicating whether or not new
* data is available from the GPS module 
*/

static bool feedgps() {
  // while a connection to the GPS module has been established...
  while (nss.available()) {
    if (gps.encode(nss.read())) // If new data is available, 
      return true; // Exit the function and return TRUE
  }
  return false; // Otherwise, exit the function and return FALSE
}

/* calc_dist: consumes the current and previous latitude and longitude values and
* converts them into vectors, accounting for the curvature of the earth. The function
* returns the calculated distance between the current and previous lat/long points in 
* meters
*/
unsigned long calc_dist(float flat1, float flon1, float flat2, float flon2) {
  
  float dist_calc=0;
  float dist_calc2=0;
  float diflat=0;
  float diflon=0;

  diflat=radians(flat2-flat1);
  flat1=radians(flat1);
  flat2=radians(flat2);
  diflon=radians((flon2)-(flon1));

  dist_calc = (sin(diflat/2.0)*sin(diflat/2.0));
  dist_calc2= cos(flat1);
  dist_calc2*=cos(flat2);
  dist_calc2*=sin(diflon/2.0);
  dist_calc2*=sin(diflon/2.0);
  dist_calc +=dist_calc2;

  dist_calc=(2*atan2(sqrt(dist_calc),sqrt(1.0-dist_calc)));

  dist_calc*=6371000.0; //Converting to meters
  return dist_calc;
}

/* changemode: consumes nothing and toggles the run variable.  While the function is
* excecuting, it looks to see if the momentary switch for start/stop is being held down.  
* if so, it resets the elapsed time and current distance traveled.
*/
static void changemode() {
  
 LCD.clear(); // Clear the LCD screen
 static unsigned long last_interrupt_time = 0; // Used to debounce button press
 unsigned long interrupt_time = millis(); // Used to debounce button press
 if (interrupt_time - last_interrupt_time > 200) { // If the button has been pressed..
   // invert the value of run
   if (run==0) run++; 
   else run = 0;
 }
 
 if (digitalRead(8) == HIGH){ // If the button is being held down ...
   delay(500); // Wait to make sure it's not a glitch
   if(digitalRead(8) == HIGH){ // If the button is still being pressed down
     // Reset the total distance traveled and the current time
     totalDistance = 0;
     seconds = 0;
     tseconds = 0;
     minutes = 0;
     tminutes = 0;
     hours = 0;
     // Then clear the LCD and indicate that everything's been reset
     LCD.clear();
     LCD.setCursor(0,0);
     LCD.print("Total distance");
     LCD.setCursor(0,1);
     LCD.print("and time reset");
     delay(1000); // Wait a second so that the text has some time to stay on the screen
     run = 0; // Set the main loop into its stopped state (i.e. no tracking)
     LCD.clear(); // and clear the LCD
   }
 }
 last_interrupt_time = interrupt_time; 
}


/* timekeep: consumes and returns nothing; this function is used to convert the current
* time from seconds to H:MM:SS with a different variable for each digit. Note that I'm doing this
* using the main loop's one second interrupts, but I could do this more accurately with the RTC
* on the ultimate GPS module (Sounds like a future update to me ...)
*/
void timekeep(){
  if(seconds > 9){ // If the elapsed time goes from :09 to :10 seconds...
    tseconds++; // Increase the variable for tens of seconds by 1
    seconds = 0; // And reset the value for seconds 
  }
  
  if(seconds==0 & tseconds>5){ // If the elapsed time goes from :59 to 1:00...
    minutes++; // Increment the value for minutes
    seconds = 0; // and set the values for seconds and tens of seconds to :00
    tseconds = 0;
  }
  
   if(minutes>9){ // if the elapsed time goes from 9:59 to 10:00...
    tminutes++; // Increment the variable for tens of minutes
    seconds = 0; // and set the variables for X:XX to 0:00
    tseconds = 0;
    minutes = 0;
  }
  
  if(tminutes>5){ // if the elapsed time goes from 59:00 to 1:00:00
    hours++; // Increment the time in hours
    seconds = 0; // and set everything else to 00:00
    tseconds = 0;
    minutes = 0;
    tminutes = 0;
  }
  
  return; // break out of the function if none of the if loops are true
}
