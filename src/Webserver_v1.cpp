
// Import required libraries
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "AsyncJson.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "time.h"
#include <analogWrite.h>

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


// Replace with your network credentials
const char* ssid = "VM1480773";
const char* password = "p7mTsjnqknxx";

TaskHandle_t Task1;
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

int readDurationFromFile(const char* filename){
  //Read JSON file
  File file = SPIFFS.open(filename);
  if(file){
    //Deserialise JSON
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, file);
  auto duration = doc["duration"].as<int>();
  file.close();
  return duration;
  }
}

void setTimezone(String timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(String timezone){
  struct tm timeinfo;

  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
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

void writeDataToFile(const char* filename, int outputData, String dataLabel){
  File outFile = SPIFFS.open(filename, "w");
  DynamicJsonDocument doc(1024);
  doc[dataLabel] = outputData;
  serializeJson(doc,outFile);
  outFile.close();
}

void dataProcessor(String jsonData){

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, jsonData);

  int duration = doc["duration"];


    durationMs = duration*1000;
    writeDataToFile(filename, duration, "Duration");  
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
Motor waterPump(22, 32, 33);


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

durationMs = 1000*readDurationFromFile(filename); //initialise duration
initTime("GMT0BST,M3.5.0/1,M10.5.0"); //initialise timezone

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
     currentMs = millis();
  if((durationMs - (currentMs-prevMs)) > 0 && (durationMs - (currentMs-prevMs)) < 100000){
    durationMs = durationMs - (currentMs-prevMs);
    prevMs = currentMs;
    digitalWrite(ledPin, HIGH);
    waterPump.drive(255);
    LEDstatus = 1;
  }
  
  else{
    durationMs = 0;
    prevMs = currentMs;
    digitalWrite(ledPin, LOW);
    waterPump.drive(0);
    LEDstatus = 0;
  }
  
  updateStatus(LEDstatus);

  if(newJSONFlag == 1){
      dataProcessor(newJSON);
      newJSONFlag = 0;
  }
  
  }
}
  
void loop() {
  
}
