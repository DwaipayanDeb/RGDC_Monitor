#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Preferences.h> // Library for storing data in flash memory
#include <BluetoothSerial.h> // Library for Bluetooth Serial
#include <SPIFFS.h>
#include "time.h"
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// WiFi credentials
#define WIFI_SSID "mySSID"
#define WIFI_PASSWORD "myPassword"
#define API_KEY "AIz***********************e1iJ4"
#define DATABASE_URL "https://at**************db.firebaseio.com"

//Pin definitions
#define TdsSensorPin 34
#define wifiLEDpin 5 
#define uploadLEDpin 23

#define VREF 3.3      // analog reference voltage(Volt) of the ADC
#define SCOUNT  1000           // sum of sample point
float analogBuffer[SCOUNT]={0};    // store the analog value in the array, read from ADC
int avgBuffer[10]={0};
int tdsValue = 0, rawCount=0, sumMode=0, avMode = 0, finalMode=0;

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Bluetooth Serial object
BluetoothSerial SerialBT;

// Flash memory storage
Preferences preferences;

unsigned long sendDataPrevMillis = 0;
unsigned long restartPrevMillis=0;
bool signupOK = false;

// NTP configuration
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;         // Adjust for your timezone (e.g., 19800 for IST)
const int   daylightOffset_sec = 0;    // Daylight savings (if any)


String ssid = "";     // WiFi SSID
String password = ""; // WiFi Password
String NWssid = "";     // New WiFi SSID
String NWpassword = ""; // New WiFi Password

int tdsThreshold=0;
int errorCount=0;  // Counter for Firebase upload errors


// Path of the file to write
const char* filePath = "/AtmData.txt";

// Configure Firebase

void saveCredentials(const String &newSSID, const String &newPassword) {
  preferences.begin("wifi-config", false); // Open namespace "wifi-config" in flash memory
  preferences.putString("ssid", newSSID);
  preferences.putString("password", newPassword);
  preferences.end();
  Serial.println("Wi-Fi credentials saved!");
}

void loadCredentials() {
  preferences.begin("wifi-config", true); // Open namespace "wifi-config" in read-only mode
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();
  if (ssid.isEmpty() || password.isEmpty()) {
    Serial.println("No saved Wi-Fi credentials found!");
  } else {
    Serial.println("Loaded Wi-Fi credentials:");
    Serial.println("SSID: " + ssid);
    Serial.println("Password: " + password);
  }
}

void connectToWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid.c_str(), password.c_str());
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    Serial.print(".");
    delay(500);
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
  }
}

void blueConnect(){
    Serial.println("Connecting bluetooth...");
    // Read and print the file content
    if (SerialBT.available()) {
    while (NWssid.isEmpty()||NWpassword.isEmpty()){
      String input = SerialBT.readStringUntil('\n');
      input.trim(); // Remove any trailing or leading whitespace
      Serial.println("Received via Bluetooth: " + input);
      int ssIndex = input.indexOf("SS:");       // Find "SS:"
      int pwIndex = input.indexOf("PW:");       // Find "PW:"

      if (ssIndex != -1 && pwIndex != -1 && pwIndex > ssIndex) {
        NWssid = input.substring(ssIndex + 3, pwIndex);  // between SS: and PW:
        NWpassword = input.substring(pwIndex + 3);           // after PW:
      }
    }

    
    // Save credentials and restart if both SSID and password are set
    if (!NWssid.isEmpty() && !NWpassword.isEmpty()) {
      Serial.println("Saving new Wi-Fi credentials and restarting...");
      saveCredentials(NWssid, NWpassword);
      SerialBT.print("OK");
      delay(1000);
      ESP.restart(); // Restart the ESP32 to apply the new credentials
      }
    }
  }


void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(wifiLEDpin,OUTPUT);
  pinMode(uploadLEDpin,OUTPUT);
  SerialBT.begin("RGDC_Monitor"); // Initialize Bluetooth Serial with the name "RGDC_Monitor"
  Serial.println("Bluetooth Serial started. Waiting for Wi-Fi credentials...");
  // Load stored credentials from flash memory
  loadCredentials();
  // Attempt to connect to Wi-Fi if credentials are available
  if (!ssid.isEmpty() && !password.isEmpty()) {
    connectToWiFi();
  }

   // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email.clear();
  auth.user.password.clear();
  // Provide your existing user credentials here
  auth.user.email = "****@****mail.com";
  auth.user.password = "********";

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(wifiLEDpin,HIGH);
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
  // Assign the callback function for the long running token generation task
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  // Assign the callback function for the long running token generation task
    config.token_status_callback = tokenStatusCallback;
  }
  else{
    digitalWrite(wifiLEDpin,LOW);
  }

  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to initialize SPIFFS!");
    return;
  }
  Serial.println("SPIFFS initialized!");
}


unsigned long int duration=30*60*1000; // 30 minute in milliseconds -- Upload duration
unsigned long int restartDur=60*60*1000; // 60 minute in milliseconds -- Restart duration
String timeStringFB;
int n=0, nn=0;

void loop() {
  if (WiFi.status() == WL_CONNECTED){
    digitalWrite(wifiLEDpin,HIGH);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo; 
    if (getLocalTime(&timeinfo)){
                char timeString[30];
                strftime(timeString, sizeof(timeString), "%Y-%m-%d_%H-%M-%S", &timeinfo);
                timeString[strlen(timeString)]='\0';
                timeStringFB=timeString;
                }   
      

    for (int i=0;i<1000;i++){
      rawCount=analogRead(TdsSensorPin);
      analogBuffer[i]=(rawCount*(3300/4095.0))+114;      // This is the raw voltage. The 114 was added because the pin 15 starts at this value. e.g. input 114 mV -> 0 count, 120 mV-> 120-114= 6 count, etc. 
    }  
    
    float modeValue=mode(analogBuffer);  // Here we get mode of 1000 raw voltages with binning (i.e. mode of frequencies for 20 bins from minimum to maximum)
    sumMode+=modeValue;
    n++;
    if(n>=100){
      avMode=round(sumMode/float(n)); //Average of 100 modes
      n=0;
      sumMode=0;
      Serial.println(avMode);
      digitalWrite(wifiLEDpin,LOW);
      delay(100);
      digitalWrite(wifiLEDpin,HIGH);

    //**********
    //avMode=333;// FOR TEST ONLY
    //*********

    if (avMode>=160){                 //  When TDS sensor is connected, 150 is the read by the ESP32 at pin 15 when leads are dry(i.e. 0 TDS). So 160 is given as threshold count so that 0 TDS can be avoided.  
    avgBuffer[nn]=avMode;
    nn+=1;
    if (nn>=10){
          nn=0;
          finalMode=modeDiscrete(avgBuffer);    // This gives the mode of 10 values of average modes as calculated in previous step (This mode is just gives the ineteger value that occurs maximum number of times without binning)
          Serial.println();
          Serial.print("Mode of last 10 Averages=");
          Serial.println(finalMode);
          Serial.println();
          tdsValue=ConvertToTDS(finalMode); // TDS Calibration function
          Serial.println();
          Serial.print("TDS=");
          Serial.println(tdsValue);
          Serial.println();
          uploadFirebase(timeStringFB,finalMode,tdsValue);
        }
      }
    } 
  } 
   else{
      digitalWrite(wifiLEDpin,LOW);
      blueConnect();
    } 
  if (((unsigned long)(millis() - restartPrevMillis) >= restartDur)) {
    restartPrevMillis = millis();
    Serial.println("Restarting ESP....");
    ESP.restart();
  }
}



void uploadFirebase(String timeStr,int modeVal,int tdsVal){
            Serial.print("time-diff=");
            Serial.println((unsigned long)(millis() - sendDataPrevMillis));
            Serial.print("sendDataPrevMillis=");
            Serial.println(sendDataPrevMillis);
          if (Firebase.ready() && ((unsigned long)(millis() - sendDataPrevMillis) >= duration || sendDataPrevMillis == 0)) {            
            Serial.println("Uploading data to Firebase...");
 
            
            if (Firebase.setString(fbdo, "/AtmosphericData/TDS_value/"+timeStr, String(modeVal)+","+String(tdsVal))) {   
              Serial.println("TDS_value uploaded successfully!");
              sendDataPrevMillis = millis();
              digitalWrite(uploadLEDpin,LOW);
              delay(500);
              digitalWrite(uploadLEDpin,HIGH);
            } else {
              Serial.print("Error uploading TDS_value: ");
              digitalWrite(uploadLEDpin,LOW);
              Serial.println(fbdo.errorReason());
              //Firebase.setInt(fbdo, "/AtmosphericData/ErrorCount", errorCount);
              Firebase.reconnectWiFi(true);
              ESP.restart();
            }
          }
          else if (!Firebase.ready()){
                Serial.println("Firebase not ready. Restarting ESP....");
                ESP.restart();
            
          //sendDataPrevMillis=(long)(millis()-duration);
          }
      }

int getMedianNum(int bArray[], int iFilterLen) 
{
      int bTab[iFilterLen];
      for (byte i = 0; i<iFilterLen; i++)
      bTab[i] = bArray[i];
      int i, j, bTemp;
      for (j = 0; j < iFilterLen - 1; j++) 
      {
      for (i = 0; i < iFilterLen - j - 1; i++) 
          {
        if (bTab[i] > bTab[i + 1]) 
            {
        bTemp = bTab[i];
            bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
         }
      }
      }
      if ((iFilterLen & 1) > 0)
    bTemp = bTab[(iFilterLen - 1) / 2];
      else
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
      return bTemp;
}


float mode(float buffer[1000]) {      // Mode of float values with binning. The whole range of values from minimum tomaximum is divided in 20 sections (bins) to get frequency for each section
  float maxNum = buffer[0];
  float minNum = buffer[0];
  int freq[20] = {0}; // initialize frequency array

  for (int i = 1; i < 1000; i++) {
    if (buffer[i] > maxNum) maxNum = buffer[i];
    if (buffer[i] < minNum) minNum = buffer[i];
  }

  float binSize = (maxNum - minNum) / 20.0;

  for (int i = 0; i < 1000; i++) {
    for (int j = 0; j < 20; j++) {
      if ((buffer[i] >= minNum + j * binSize) && 
          (buffer[i] < minNum + (j + 1) * binSize)) {
        freq[j]++;
        break;
      }
      // handle edge case: exact match with maxNum
      if (j == 19 && buffer[i] == maxNum) {
        freq[j]++;
        break;
      }
    }
  }

  int max_j = 0;
  int max_freq = freq[0];
  for (int j = 1; j < 20; j++) {
    if (freq[j] > max_freq) {
      max_freq = freq[j];
      max_j = j;
    }
  }

  return minNum + max_j * binSize + (binSize / 2.0); // center of mode bin
}

int modeDiscrete(int values[10]){   // Mode of collection of inetgers without binning 
  int freq=0;
  int freqmax=0;
  int modeValue=0;
  for (int i=0;i<10;i++){
    if (values[i]!=0){
    int tempValue=values[i];
    freq=0;
     for (int j=0;j<10;j++){
      if(values[j]==tempValue){
        freq+=1;
        values[j]=0;
      }
      if (freq>=freqmax){
        freqmax=freq;
        modeValue=tempValue;
      }
    } 
    }
  }
return modeValue;
}

int ConvertToTDS(int sensorVoltage){   // This function was determined experimentally
  int tds=0;
  if (sensorVoltage<265){
    tds=round(((sensorVoltage-151)-3.55)/9.66);
  }
  else if (sensorVoltage<500){
    tds=round(((sensorVoltage-151)-30.22)/6.84);
  }
  else{
     tds=round(((sensorVoltage-151)-136.75)/4.89);
  }
  return tds;
}

