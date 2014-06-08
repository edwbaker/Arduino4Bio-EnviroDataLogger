//Ecological Data Logger
//======================
//
//by Ed Baker (ebaker.me.uk)
//for Arduino for Biologists by Pelagic Publishing

//Libraries
#include <SPI.h>          //Standard Arduino
#include <SD.h>           //
#include <Wire.h>         //

#include "DHT.h"          //Library for reading the DHT-22 sensor (get from: https://github.com/adafruit/DHT-sensor-library )
#include <Time.h>         //Generic functions for managing date/time data (get from: http://www.pjrc.com/teensy/td_libs_Time.html )
#include <DS1307RTC.h>    //Tools for manipulating the DS1307 real time clock (get from https://bitbucket.org/johnmccombs/arduino-libraries/downloads )
#include <Sleep_n0m1.h>   //Easy-to-use way of putting Arduino sleetp (get from https://github.com/n0m1/Sleep_n0m1 )

#include "EnviroStruct.h" //We define the struct 'reading' to handle time, temperature and humidity together

//Configure the libraries 
#define DHTPIN 2          //The DHT-22 data will be accessed via pin 2
#define DHTTYPE DHT22     //Set model: we use DHT-22 but the library also supports DHT-11
#define SDPIN  4          //We will access the SD card via pin 4 (Ethernet shield default)

//Create instances
DHT dht(DHTPIN, DHTTYPE);
Sleep sleep;

void setup() {
  int i=0;                  //We use this as a counter in a few loops
  boolean sd_check = false; //Set to TRUE once we have checked we can access the SD card
  pinMode(10, OUTPUT);      //Pin 53 on mega must be output (10 on other baords)
  
  //Start Serial connection
  Serial.begin(9600); 
  while(!Serial){
    ;
  }

  //Check for SD card 
  Serial.println("Initalising SD card...");
  
  //Sometimes this doesn't work on first attempt, so we will attempt up to 3 times
  while (sd_check == false && i < 3) {
    i++;
    sd_check = init_sd();
    delay(2000);
  }
  //Check whether SD card is ready. If not create a fatal error
  if (sd_check == false) {
    error error = {
      "SD card initialisation has failed.", 
      1 //FATAL
    };
    error_condition(error);
    Serial.println("SD card initalisation has failed.");
  }
  
  //Start DHT
  Serial.println("Connecting to humidity and temperature sensor");
  dht.begin();
  i =0;
  boolean connected = true;
  //The DHT-22 will return a not-a-number (NaN) error if a reading failed
  //In some instances it will also return a value of 0 for both temperature and humidity if there is an error
  //We check for any of these and don't count them as valid if they occur
  
  //TODO: Rewrite this to avoid repetition
  if (isnan(dht.readTemperature())) { connected = false; }
  if (isnan(dht.readHumidity())) { connected = false; }
  if (dht.readTemperature() == 0 && dht.readHumidity() == 0) { connected = false; }
  //We keep trying to connect up to 10 times
  while (!connected & i < 10) {
    i++;
    int warning;
    if (i < 10) {
      warning = 3;
    } else {
      warning = 1;
    }
    String error_msg = "Cannot connect to sensor. Try ";
    error_msg.concat(i);
    error error = {
      error_msg,
      warning
    };
    error_condition(error);
    connected = true;
    if (isnan(dht.readTemperature())) { connected = false; }
    if (isnan(dht.readHumidity())) { connected = false; }
    if (dht.readTemperature() == 0 && dht.readHumidity() == 0) { connected = false; }
  }
  
  //We're good to go! 
  Serial.println("Connected.");
  Serial.println();
  Serial.println("Starting program loop.");
}

void loop() {
  //This rather spartan looking loop delegates a lot of the action to spearate functions.
  
  //Read the data from DHT-22 and DS1307
  reading data = reading_get_data();

  //We then send the data to a fucntion to handle saving it
  data_post(data);
  
  data_blink(1); //Should move to data_post function
  
  //Move into separate function
  Serial.println("Starting sleep.");
  delay(100);
  sleep.pwrDownMode();
  sleep.sleepDelay(60000);
  Serial.println("Ending sleep.");
}

//The data_post function is a wrapper function around sd_post (the function that saves the data to the SD card).
//Sometimes wrapper functions like this are used to make code more portable, for instance we could easily add
//functionality to this function to submit data over the internet and still keep the sd_post function only doing
//a single task.
boolean data_post(reading data) {
    boolean sd = false;
    //This loop tries to write the data up to five times, after that it gives an error.
    int i = 0;
    while (sd == false && i < 5) {
      sd = sd_post(data);
      i++;
    }
    //TODO: error!
}

//This function takes the reading struct and forms a row of a CSV file
//to append to the file data.txt on the SD card
boolean sd_post(reading data) {
  //Converting floats to strings is a bit of a faff - use dtostrf with some
  //char* buffers
  char humidity[6];
  char temperature[6];
  dtostrf(data.humidity, 1, 2, humidity);
  dtostrf(data.temperature, 1, 2, temperature);
  //The request string will contain the line of dat we request to add to the file
  String request;
  request.concat(data.time);
  request += ",";
  request +=  humidity;
  request += ",";
  request +=  temperature;

  //Try and open the file data.txt. This will create it if it doesn't already exist.
  File dataFile = SD.open("data.txt", FILE_WRITE);
  //Check we have opened the file
  if (dataFile) {
    //Apend our request to the end of the file and close it
    dataFile.println(request);
    dataFile.close();
    Serial.println("Data saved to data.txt");
    return true;
  } 
  else {
   //If we can't open the file raise an error.
   error error = {
     "Failed to open data.txt.",
     2    };
    error_condition(error);
    //Returning false will indicate to data_post() that the write failed
    return false;
  }
}

//This finction separates out the initialisiation of the SD card
boolean init_sd() {
  // make sure that the default chip select pin is set to output, even if you don't use it:
  pinMode(SDPIN, OUTPUT);
  // see if the card is present and can be initialized:
  if (!SD.begin(SDPIN)) {
    return false;
  } 
  return true;
}

//Function to read the sensors and clock and return a struct if successful
reading reading_get_data(){
  uint32_t time     = RTC.get();
  float    h        = dht.readHumidity();
  float    t        = dht.readTemperature();
  
  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h) || (t == 0 && h == 0)) {
    String error_msg = "Failed to read from DHT";
    error error =  {
      error_msg, 
      2
    };
    error_condition(error);
  } 
  reading data = {
    time,
    h,
    t,
  };
  return data;
}

//This function responds to errors raised elsewhere in the code
void error_condition(error error) {
  Serial.println("Error condition encountered:");
  Serial.println(error.condition);
  Serial.print("Error level ");
  Serial.println(error.level);

  //Log error to SD card
  String request = "";
  request += error.condition;
  request += ",";
  request += error.level;

  File errorFile = SD.open("error.log", FILE_WRITE);
  if (errorFile) {
    errorFile.println(request);
    errorFile.close();
  }
  
  //Respond to error condition as appropriate
  switch (error.level) {
    case 1:
      data_blink(5);
      break;
    case 2:
      data_blink(4);
      break;
    case 3:
      data_blink(3);
      break;
    case 4:
      data_blink(2);
      break;
  }
}

//Function to generate an LED blink (pin 9) for info and error,
void data_blink(int repeat) {
  pinMode(9, OUTPUT);
  for (int i =0 ; i < repeat; i++) {
    digitalWrite(9, HIGH);
    delay(50);
    digitalWrite(9, LOW);
    delay(50);
  }
}
