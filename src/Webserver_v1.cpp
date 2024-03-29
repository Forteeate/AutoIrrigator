
// Import required libraries
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <AsyncJson.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Time.h>
#include <analogWrite.h>
#include <TimeAlarms.h>


#define ledPin 2

int previousLEDStatus;
unsigned long int lastStatusUpdateTime;
String currentStatus;
String currentTime;

String newJSON;
int newJSONFlag;


unsigned long int currentMs;
unsigned long int prevMs;
unsigned long int durationMs = 0;
int LEDstatus = 0;

const char* filename = "/data.json";


// Network credentials
const char* ssid = "VM1480773";
const char* password = "p7mTsjnqknxx";

TaskHandle_t Task1;
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

DynamicJsonDocument recoverDataFromFile(const char* filename){
  //Read JSON file
  File file = SPIFFS.open("/data.json", "r");
  if(!file){
  Serial.println("Failed to read file!");
  }
    //Deserialise JSON
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, file);

  Serial.print("Read data from file:");
  Serial.println(file);

  file.close();
  return doc;
  
  
}

void Task1code( void * parameter);

void setTimezone(String timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(String timezone){
  struct tm timeinfo;

  configTime(0, 0, "time.google.com");    // First connect to NTP server, with 0 TZ offset
  if(!getLocalTime(&timeinfo)){
    Serial.println("  Failed to obtain time");
    return;
  }
  // Now we can set the real timezone
  setTimezone(timezone);
  
  String minutes;
  if(timeinfo.tm_min < 10){
    minutes = String("0" + String(timeinfo.tm_min));
  }
  else{
    minutes = String(timeinfo.tm_min);
  }
  currentTime = String(String(timeinfo.tm_hour) + ":" + minutes);

  Serial.print("Successfully initialised time to:");
  Serial.println(currentTime);
}

void writeDataToFile(const char* filename, int outputData1, String dataLabel1,
 int outputData2, String dataLabel2, int outputData3, String dataLabel3){
  File outFile = SPIFFS.open(filename, "w");
  DynamicJsonDocument doc(1024);
  doc["interval"] = outputData1;
  doc["time"] = outputData2;
  doc["volume"] = outputData3;
  serializeJson(doc, outFile);

  Serial.print("Wrote data to file:");
  Serial.println(outFile);
  Serial.print(dataLabel1);
  Serial.println(outputData1);
  Serial.print(dataLabel2);
  Serial.println(outputData2);
  Serial.print(dataLabel3);
  Serial.println(outputData3);
  outFile.close();
  
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "application/json", "{\"message\":\"Not found\"}");
}

unsigned int generateStatus(){

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time :(");
  }
  String minutes;
  if(timeinfo.tm_min < 10){
    minutes = String("0" + String(timeinfo.tm_min));
  }
  else{
    minutes = String(timeinfo.tm_min);
  }
  currentTime = String(String(timeinfo.tm_hour) + ":" + minutes);
  
  return(0);
  
}

void updateStatus(int currentLEDStatus){
  unsigned long int clockMs = millis(); 
  if((currentLEDStatus != previousLEDStatus) || ((clockMs - lastStatusUpdateTime) > 30000)){ //Update currentStatus JSON if LED has changed or if 30secs have passed.
    currentStatus = generateStatus();    
    lastStatusUpdateTime = clockMs;
    previousLEDStatus = currentLEDStatus;
  }
  
}

class Motor
{
  public:
    Motor(int iPwmOut, int iDir1, int iDir2);
    void drive(int iSpeed);
    void pump(int volume);
    int iPwmOutPort, iDir1Port, iDir2Port, iCurrentSpeed;
};

Motor::Motor(int iPwmOut, int iDir1, int iDir2)
{ 
  iPwmOutPort = iPwmOut;
  iDir1Port = iDir1;
  iDir2Port = iDir2;
  iCurrentSpeed = 0;
  
  pinMode(iPwmOutPort, OUTPUT);  
  pinMode(iDir1Port, OUTPUT);
  pinMode(iDir2Port, OUTPUT);  
}

void Motor::drive(int iSpeed){

  if(iSpeed < 0){
    digitalWrite(iDir1Port, HIGH);
    digitalWrite(iDir2Port, LOW);
  }
  else{
    digitalWrite(iDir1Port, LOW);
    digitalWrite(iDir2Port, HIGH);
  }
  
  analogWrite(iPwmOutPort, iSpeed);
//  while(iCurrentSpeed != iSpeed){
//    if(iCurrentSpeed > iSpeed){
//      iCurrentSpeed--;
//      analogWrite(iPwmOutPort, iCurrentSpeed);
//     
//    }
//    if(iCurrentSpeed < iSpeed){
//      iCurrentSpeed++;
//      analogWrite(iPwmOutPort, iCurrentSpeed);
//     
//    }
//  }
}
  
void Motor::pump(int volume){ //"volume" is volume of water to pump in ml
    int pumpDuration = volume * 1.31;
    this->drive(255);
    delay(pumpDuration*1000);
    this->drive(0);
}

Motor waterPump(22, 32, 33);

class Plant
{
  public:
    Plant(int interval, int time, int volume, int prevTime);
    //void dataProcessor(String jsonData);
    //void waterPlants();
    int wateringInterval, wateringTime, waterVolume, lastWaterTime, daysTillWater;
    AlarmID_t currentAlarm;
};

Plant::Plant(int interval, int time, int volume, int prevTime)
{ 
  wateringInterval = interval;
  wateringTime = time;
  waterVolume = volume;
  lastWaterTime = prevTime;
  
  
}



/* void Plant::waterPlants(){

  if(daysTillWater == 0){
    waterPump.pump(waterVolume);

    Serial.print("Watered Plants Succesfully: ");
    Serial.print(waterVolume);
    Serial.println("ml");

    currentAlarm = Alarm.alarmOnce(wateringTime,0,0, std::bind(waterPlants, this));  //Set alarm for next watering  
  }
  else{
    daysTillWater--;
  }

} */

Plant plant1(2, 12, 200, 0);

void waterPlants(){
     if(plant1.daysTillWater == 0){
    waterPump.pump(plant1.waterVolume);

    Serial.print("Watered Plants Succesfully: ");
    Serial.print(plant1.waterVolume);
    Serial.println("ml");

    plant1.currentAlarm = Alarm.alarmOnce(plant1.wateringTime,0,0, waterPlants);  //Set alarm for next watering  
    plant1.daysTillWater = (plant1.wateringInterval - 1);
  }
  else{
    plant1.daysTillWater--;
  }

  }

void dataProcessor(String jsonData){

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, jsonData);

  if(doc["waterNowvolume"] != 0){
    waterPump.pump(doc["waterNowvolume"]);
  }
  else if(doc["volume"] != 0){

    int waterInterval = doc["interval"]; //interval in days
    int waterTime = doc["time"]; //time of day in 24 hour format, eg "24" = midnight
    int waterVol = doc["volume"]; //volume in ml

    
    plant1.wateringInterval = waterInterval;
    plant1.wateringTime = waterTime;
    plant1.waterVolume = waterVol;
    plant1.daysTillWater = (waterInterval-1);
    
    Alarm.disable(plant1.currentAlarm);
    plant1.currentAlarm = Alarm.alarmOnce(plant1.wateringTime,0,0, waterPlants);  //Set alarm for next watering  

    writeDataToFile(filename, waterInterval, "wateringInterval",
      waterTime, "wateringTime", waterVol, "wateringVolume"); 
  }
}

void setup(){

  // Serial port for debugging purposes
  Serial.begin(115200);

  //Mount FileSystem
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());
  
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/index.html", String(), false);
  });
  server.on("/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/bootstrap.min.css","text/css");
  });
  server.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/jquery.min.js","text/css");
  });
  server.on("/bootstrap.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/bootstrap.min.js","text/css");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){

    StaticJsonDocument<200> doc;
    doc["LEDStatus"] = String(LEDstatus);
    doc["currentTime"] = currentTime;
    doc["lastWaterTime"] = plant1.lastWaterTime;
    doc["currentTimeInterval"] = plant1.wateringInterval;
    doc["currentWaterTime"] = plant1.wateringTime;
    doc["currentWaterVolume"] = plant1.waterVolume;
    

    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
  /*  Serial.print("STATUS: ");
    Serial.println(response);
  */
  });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/endpoint/", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<200> data;
    
      data = json.as<JsonObject>();
    
      String response;
      serializeJson(data, response);
      request->send(200, "application/json", response);

      newJSON = response;
      newJSONFlag = 1;
  });


  server.addHandler(handler);
  server.onNotFound(notFound);
  // Start server
  server.begin();


  initTime("GMT0BST,M3.5.0/1,M10.5.0"); //initialise timezone

  DynamicJsonDocument oldData = recoverDataFromFile(filename); //initialise values from file
    plant1.wateringInterval = oldData["interval"];
    plant1.wateringTime = oldData["time"];
    plant1.waterVolume = oldData["volume"];
    //plant1.lastWaterTime = oldData["prevTime"];
    plant1.currentAlarm = Alarm.alarmOnce(plant1.wateringTime,0,0, waterPlants);  //Set alarm for next watering  
    
  xTaskCreatePinnedToCore(
        Task1code, /* Function to implement the task */
        "Task1", /* Name of the task */
        10000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        0,  /* Priority of the task */
        &Task1,  /* Task handle. */
        0); /* Core where the task should run */
}




void Task1code( void * parameter) {
  for(;;) {
     
  updateStatus(LEDstatus);

  if(newJSONFlag == 1){
      dataProcessor(newJSON);
      newJSONFlag = 0;
  }
  
  }

  Alarm.delay(0);
}

void loop() {}