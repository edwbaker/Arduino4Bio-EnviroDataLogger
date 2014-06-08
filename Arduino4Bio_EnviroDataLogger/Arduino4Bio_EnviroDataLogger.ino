//Ecological Data Logger
//======================
//
//Ed Baker (ebaker.me.uk)

//Libraries
#include "DHT.h"
#include <SPI.h>
#include <SD.h>
#include "lta_Struct.h"
#include <DS1307RTC.h>
#include <Time.h>
#include <Wire.h>
#include <Sleep_n0m1.h>

//DHT (Digital Humidity and Temperature Sensor)
#define DHTPIN 2
#define DHTTYPE DHT22

//SD Card
#define SDPIN  4

//Global variables
int mode;  //The mode that the device is running in


DHT dht(DHTPIN, DHTTYPE);
Sleep sleep;

void setup() {
  //Serial connection
  Serial.begin(9600); 
  while(!Serial){
    ;
  }

  //Check for SD card
  if (mode == 2 || mode == 3 || mode == 4) { 
    Serial.println("Initalising SD card...");
    //Pin 53 on mega must be output (10 on other baords)
    pinMode(53, OUTPUT);
    int i=0;
    boolean sd_check = false;
    while (sd_check == false && i < 10) {
      i++;
      sd_check = init_sd();
      delay(2000);
      if (sd_check == false) {
        String error_msg = ("SD card initialisation failed on attempt ");
        error_msg.concat(i);
        error error = {
          error_msg, 
          3 //WARNING
        };
        error_condition(error);
      }
    }
    if (sd_check == false) {
      int warning;
      if (mode == 2 || mode == 3) {
        warning = 1;  //FATAL IN THESE CASES
      } else {
        warning = 2;  //ERROR - USE OF ETHERNET IS EXPECTED
      }
      error error = {
        "SD card initialisation has failed.", 
        3 //WARNING
      };
      error_condition(error);
      Serial.println("SD card initalisation has failed.");
    }
  }

  //Start DHT
  Serial.println("Connecting to humidity and temperature sensor");
  dht.begin();
  int i =0;
  boolean connected = true;
  if (isnan(dht.readTemperature())) { connected = false; }
  if (isnan(dht.readHumidity())) { connected = false; }
  if (dht.readTemperature() == 0 && dht.readHumidity() == 0) { connected = false; }
  while (!connected) {
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
  Serial.println("Connected.");
  Serial.println();
  Serial.println("Starting program loop.");
}

void loop() {
  lta data = lta_get_data();
  data_post(data);
  error_blink(1); //Should move to data_post function
  Serial.println("Starting sleep.");
  delay(100);
  sleep.pwrDownMode();
  sleep.sleepDelay(600000);
  Serial.println("Ending sleep.");
}

boolean sd_post(lta data) {
  //Generate new line of data
  char humidity[6];
  char temperature[6];
  dtostrf(data.humidity, 1, 2, humidity);
  dtostrf(data.temperature, 1, 2, temperature);
  String request;
  request.concat(data.time);
  request += ",";
  request +=  humidity;
  request += ",";
  request +=  temperature;
  request += ",";
  request +=  data.ethernet;
  request += ",";
  request += data.fail;

  //Append to file
  File dataFile = SD.open("data.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println(request);
    dataFile.close();
    Serial.println("Data saved to data.txt");
    return true;
  } 
  else {
   error error = {
     "Failed to open data.txt.",
     2    };
    error_condition(error);
    return false;
  }
}

boolean data_post(lta data) {
    boolean sd = false;
    int i = 0;
    while (sd == false && i < 5) {
      sd = sd_post(data);
      i++;
    }
  }
}


boolean init_sd() {
  // make sure that the default chip select pin is set to output, even if you don't use it:
  pinMode(SDPIN, OUTPUT);
  // see if the card is present and can be initialized:
  if (!SD.begin(SDPIN)) {
    return false;
  } 
  return true;
}

lta lta_get_data(){
  uint32_t time     = RTC.get();
  float    h        = dht.readHumidity();
  float    t        = dht.readTemperature();
  boolean  ethernet = false;
  boolean  fail     = false;
  
  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h) || (t == 0 && h == 0)) {
    String error_msg = "Failed to read from DHT";
    error error =  {
      error_msg, 
      2
    };
    error_condition(error);
    fail = true;
  } 
  lta data = {
    time,
    h,
    t,
    ethernet,
    fail
  };
  return data;
}

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
      error_blink(5);
      softReset();
      break;
    case 2:
      error_blink(4);
      break;
    case 3:
      error_blink(3);
      break;
    case 4:
      error_blink(2);
      break;
  }
}

void error_blink(int repeat) {
  pinMode(9, OUTPUT);
  for (int i =0 ; i < repeat; i++) {
    digitalWrite(9, HIGH);
    delay(50);
    digitalWrite(9, LOW);
    delay(50);
  }
}
