/**
  ESP-12E  (ESP8266) manejado desde IBM IoT
  en funcion de la libreria (mqtt_Mosquitto o mqtt_Bluemix)
  manda los datos a un servidor mqtt u otro
  Morrastronics -- by chuRRuscat
  v1.0 2017 initial version
  v2.0 2018 mqtt & connectivity  functions separated
  v2.1 2019 anyadido define CON_ LLUVIA y cambios en handleupdate
  v2.5 2020 reprogrammed handling of reconnections
*/

#define PRINT_SI
#ifdef PRINT_SI
#define DPRINT(...)    Serial.print(__VA_ARGS__)
#define DPRINTLN(...)  Serial.println(__VA_ARGS__)
#else
#define DPRINT(...)     //linea en blanco
#define DPRINTLN(...)   //linea en blanco
#endif
/*************************************************
 ** -------- Personalised values ----------- **
 * *****************************************/
/* select sensor and its values */ 
//#include "pruebas.h"  
//#include "salon.h"
//#include "jardin.h"
#include "terraza.h"
#include "mqtt_mosquitto.h"  /* mqtt values */
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

/*************************************************
 ** ----- End of Personalised values ------- **
 * ***********************************************/
#define AJUSTA_T 10000 // To adjust delay in some places along the program
#define SDA D5   // for BME280 I2C 
#define SCL D6
#define interruptPin D7 // PIN where I'll connect the rain gauge
#define sensorPin    A0  // analog PIN  of Soil humidity sensor
#define CONTROL_HUMEDAD D2  // Transistor base that switches on&off soil sensor
#define L_POR_BALANCEO 0.2794 // liter/m2 for evey rain gauge interrupt
#include <Wire.h>             //libraries for sensors and so on
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Pin_NodeMCU.h>
#define PRESSURE_CORRECTION (1.080)  // HPAo/HPHh 647m

#define BME280_ADDRESS   (0x76)   //IMPORTANT, sometimes it is 0x77
Adafruit_BME280 sensorBME280;     // this represents the sensor

#define JSONBUFFSIZE 250
#define DATOSJSONSIZE 250

volatile int contadorPluvi = 0; // must be 'volatile',for counting interrupt 
/* ********* these are the sensor variables that will be exposed **********/ 
float temperatura,humedadAire,presionHPa,lluvia=0,sensacion=20;
int humedadMin=HUMEDAD_MIN,
    humedadMax=HUMEDAD_MAX,
    humedadSuelo=0,humedadCrudo=HUMEDAD_MIN;
int humedadCrudo1,humedadCrudo2,
    intervaloConex=INTERVALO_CONEX;
char datosJson[DATOSJSONSIZE];
#ifdef CON_LLUVIA
  // Interrupt counter for rain gauge
  void ICACHE_RAM_ATTR balanceoPluviometro() {  
  contadorPluvi++;
}
#endif

// let's start, setup variables
void setup() {
boolean status;

  Serial.begin(115200);

   DPRINTLN("starting ... "); 
   Wire.begin(SDA,SCL);
   status = sensorBME280.begin();  
   if (!status) {
     DPRINTLN("Can't connect to BME Sensor!  ");    
   }
   /* start PINs first soil Humidity, then Pluviometer */
   pinMode(CONTROL_HUMEDAD,OUTPUT);
   #ifdef CON_LLUVIA
    pinMode(interruptPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(interruptPin), balanceoPluviometro, RISING);
   #endif
   digitalWrite(CONTROL_HUMEDAD, HIGH); // prepare to read soil humidity sensor
   espera(1000);
   humedadCrudo1 = analogRead(sensorPin); //first read to have date to get averages
   espera(1000);
   humedadCrudo2 = analogRead(sensorPin);  //second read
   digitalWrite(CONTROL_HUMEDAD, LOW);
   wifiConnect();
   mqttConnect();
   DPRINTLN(" los dos connect hechos, ahora OTA");
   ArduinoOTA.setHostname(DEVICE_ID); 
   ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else { // U_FS
        type = "filesystem";
      }

      // NOTE: if updating FS this would be the place to unmount FS using FS.end()
      DPRINTLN("Start updating " + type);
   });
   ArduinoOTA.onEnd([]() {
      DPRINTLN("\nEnd");
   });
   ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
   });
   ArduinoOTA.onError([](ota_error_t error) {
      #ifdef PRINT_SI
      Serial.printf("Error[%u]: ", error);
      #endif
      if (error == OTA_AUTH_ERROR) {
        DPRINTLN("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        DPRINTLN("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        DPRINTLN("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        DPRINTLN("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        DPRINTLN("End Failed");
      }
   });
   ArduinoOTA.begin(); 
   delay(50);
   publicaDatos();      // and publish data. This is the function that gets and sends
}

uint32_t ultima=0;

void loop() {
 DPRINT("*");
 if (!loopMQTT()) {  // Check if there are MQTT messages and if the device is connected  
   DPRINTLN("Connection lost; retrying");
   sinConectividad();        
   mqttConnect();
 }
 ArduinoOTA.handle(); 
 if ((millis()-ultima)>intervaloConex) {   // if it is time to send data, do it
   DPRINT("interval:");DPRINT(intervaloConex);
   DPRINT("\tmillis :");DPRINT(millis());
   DPRINT("\tultima :");DPRINTLN(ultima);
   while (!publicaDatos()) {     // repeat until publicadatos sends data
      espera(1000);        // publish data. This is the function that gets and sends
   }   
   ultima=millis();
 }
 espera(1000); //and wait
}

/****************************************** 
* this function sends data to MQTT broker, 
* first, it calls to tomaDatos() to read data 
*********************************************/
boolean publicaDatos() {
  int k=0;
  char signo;
  boolean pubresult=true;
     
  if (!tomaDatos()) { 
      sprintf(datosJson,"[{\"temp\":\"error\"},{\"deviceId\":\"%s\"}]",DEVICE_ID);
    } else {
      // Data is read an stored in global var. Prepare data in JSON mode
      if (temperatura<0) {  // to avoid probles with sign
        signo='-';          // if negative , set '-' character
        temperatura*=-1;  // if temp was negative, convert it positive 
      }  else signo=' ';
      // prepare the message
      #ifdef CON_LLUVIA
        #ifdef CON_SUELO
         sprintf(datosJson,"[{\"temp\":%c%d.%1d,\"hAire\":%d,\"hSuelo\":%d,\"hCrudo\":%d,\"HPa\":%d,\"l/m2\":%d.%03d},{\"deviceId\":\"%s\"}]",
                 signo,(int)temperatura, (int)(temperatura * 10.0) % 10,
                 (int) humedadAire, (int) humedadSuelo,(int) humedadCrudo,(int) presionHPa,
                 (int)lluvia, (int)(lluvia * 1000) % 1000,DEVICE_ID);
        #else
         sprintf(datosJson,"[{\"temp\":%c%d.%1d,\"hAire\":%d,\"HPa\":%d,\"l/m2\":%d.%03d},{\"deviceId\":\"%s\"}]",
                 signo,(int)temperatura, (int)(temperatura * 10.0) % 10,
                 (int) humedadAire,(int) presionHPa,
                 (int)lluvia, (int)(lluvia * 1000) % 1000,DEVICE_ID);
        #endif                  
      #else
          #ifdef CON_SUELO
            sprintf(datosJson,"[{\"temp\":%c%d.%1d,\"hAire\":%d,\"hSuelo\":%d,\"hCrudo\":%d,\"HPa\":%d},{\"deviceId\":\"%s\"}]",
                signo,(int)temperatura, (int)(temperatura * 10.0) % 10,\
                (int)humedadAire, (int) humedadSuelo,(int)humedadCrudo,(int)presionHPa,DEVICE_ID);  
          #else      
            sprintf(datosJson,"[{\"temp\":%c%d.%1d,\"hAire\":%d,\"HPa\":%d},{\"deviceId\":\"%s\"}]",
                signo,(int)temperatura, (int)(temperatura * 10.0) % 10,\
                (int)humedadAire, (int)presionHPa,DEVICE_ID);  
          #endif
      #endif     
  }
  // and publish them.
  DPRINTLN("preparing to send");
  pubresult = enviaDatos(publishTopic,datosJson);    
  if (pubresult){
    lluvia=0;      // I sent data was successful, set rain to zero 
  }
  return pubresult;
}

/* get data function. Read the sensors and set values in global variables */
/* get data function. Read the sensors and set values in global variables */
boolean tomaDatos (){
  float bufTemp,bufTemp1,bufHumedad,bufHumedad1,bufPresion,bufPresion1;
  boolean escorrecto=false;  //return value will be false unless it works
  int i=0;
  /* read and then get the mean */
  DPRINTLN("entro en tomadatos");
  while (!escorrecto) {
    bufHumedad= sensorBME280.readHumidity();   
    bufTemp= sensorBME280.readTemperature();
    bufPresion=sensorBME280.readPressure()/100.0F;
    #ifdef CON_SUELO
      /* activate soil sensor setting the transistor base */
      digitalWrite(CONTROL_HUMEDAD, HIGH);
      espera(10000);  
      humedadCrudo = analogRead(sensorPin); // and read soil moisture
      humedadCrudo=constrain(humedadCrudo,humedadMin,humedadMax); 
      digitalWrite(CONTROL_HUMEDAD, LOW);  // disconnect soil sensor
      // calculate the moving average of soil humidity of last three values 
      humedadCrudo=(humedadCrudo1+humedadCrudo2+humedadCrudo)/3;
      humedadSuelo = map(humedadCrudo,humedadMin,humedadMax,0,100);
      humedadCrudo2=humedadCrudo1;
      humedadCrudo1=humedadCrudo;
    #endif
    // read again from BME280 sensor
    bufHumedad1= sensorBME280.readHumidity();
    bufTemp1= sensorBME280.readTemperature();
    bufPresion1= sensorBME280.readPressure()/100.0F;
    DPRINTLN("Data read");
    #ifdef CON_LLUVIA 
      //detachInterrupt(digitalPinToInterrupt(interruptPin));
      lluvia+=contadorPluvi*L_POR_BALANCEO;
      contadorPluvi=0;
      //attachInterrupt(digitalPinToInterrupt(interruptPin), balanceoPluviometro, RISING);
    #endif
    if (humedadMin==humedadMax) humedadMax+=1; 
    /* if data could not be read for whatever reason, raise a message (in PRINT_SI mode) 
      Else calculate the mean */
    if (isnan(bufHumedad) || isnan(bufHumedad1) ||
        isnan(bufPresion) || isnan(bufPresion1) ||
        isnan(bufTemp)    || isnan(bufTemp1)    ) {       
       DPRINTLN("I could not read from BME280msensor!");       
       escorrecto=false;    // flag that BME280 could not read
    } else {
      temperatura=(bufTemp+bufTemp1)/2;
      humedadAire=(bufHumedad+bufHumedad1)/2;
      presionHPa=(bufPresion+bufPresion1)/2*PRESSURE_CORRECTION;
      escorrecto=true;
      if (temperatura>60) escorrecto=false;   //if temperature out of reasonable range
      if ((humedadAire>101)||(humedadAire<0)) escorrecto=false;    // or humidity      
    } 
    if (i++>30) {
      return escorrecto;
    }  
    espera(1000);
  }
  return escorrecto;
}
