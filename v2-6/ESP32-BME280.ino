/**
  ESP-32E  (ESP32 DEVKIT V1) manejado desde IoT
  en funcion de la libreria (mqtt_Mosquitto)
  manda los datos a un servidor mqtt u otro
  Morrastronics -- by chuRRuscat
  v1.0 2017 initial version
  v2.0 2018 mqtt & connectivity  functions separated
  v2.1 2019 added define CON_ LLUVIA y cmodifications in handleupdate
  v2.5 2020 reprogrammed. ESP32 instead of ESP8266
  v2.6 2020 added CON_UV (UV sensor) and CON_VIENTO (wind sensor)

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
/*select sensor and its values */ 
#include "Dispositivo.h"
//#include "pruebas.h"  
//#include "terraza.h"
#include "mqtt_mosquitto.h"  /* mqtt values */
#define CON_OTA
#define CON_WIFI
//#undef CON_WIFI
//#undef CON_OTA
#ifdef CON_OTA
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#endif
/*************************************************
 ** ----- End of Personalised values ------- **
 * ***********************************************/

#define AJUSTA_T 10000 // To adjust delay in some places along the program
#define SCL 22
#define SDA 21
#define ADC1_CH0 36  // 
#define ADC1_CH3 39  // 
#define PIN_UV   36   
#define interruptPin 04 // PIN where I'll connect the rain gauge (GPIO04)
#define hSueloPin    34  // analog PIN  of Soil humidity sensor
#define CONTROL_HUMEDAD 02  // Transistor base that switches on&off soil sensor

#include <Wire.h>             //libraries for sensors and so on
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Pin_NodeMCU.h>
#include <ArduinoJson.h>
/* beware, I find that the sensors I buy are always alternate 
#define BME280_ADDRESS   (0x77)   //IMPORTANT, sometimes it is 0x77
#define BME280_ADDRESS_ALTERNATE (0x76)
*/
#define ACTUAL_BME280_ADDRESS BME280_ADDRESS_ALTERNATE 
Adafruit_BME280 sensorBME280;     // this represents the sensor

#define JSONBUFFSIZE 250
#define DATOSJSONSIZE 250

volatile int contadorPluvi = 0; // must be 'volatile',for counting interrupt 
volatile long lastTrigger=0;
/* ********* these are the sensor variables that will be exposed **********/ 
float temperatura,humedadAire,presionHPa,lluvia=0,sensacion=20;
#ifdef CON_SUELO
int humedadMin=HUMEDAD_MIN,
    humedadMax=HUMEDAD_MAX,
    humedadSuelo=0,humedadCrudo=HUMEDAD_MIN;
int humedadCrudo1,humedadCrudo2;
#endif
#ifdef CON_UV
  int lecturaUV;
  float UV_Watt;
  int   UV_Index;  
#endif  
#ifdef CON_LLUVIA
  // Interrupt counter for rain gauge
  void ICACHE_RAM_ATTR balanceoPluviometro() {  
    if ((millis()-lastTrigger) > 1000){
       lastTrigger=millis();
         contadorPluvi++;
    }
}
#endif
int  intervaloConex=INTERVALO_CONEX;
char datosJson[DATOSJSONSIZE];
StaticJsonDocument<DATOSJSONSIZE> docJson;
JsonObject valores=docJson.createNestedObject();  //Read values
JsonObject claves=docJson.createNestedObject();   // key values (location and deviceId)
// let's start, setup variables

void setup() {
boolean status;

  Serial.begin(115200);

   DPRINTLN("starting ... ");
   //pinMode(SDA,INPUT_PULLUP);
   //pinMode(SCL,INPUT_PULLUP); 
   Wire.begin(SDA,SCL);
   status = sensorBME280.begin(0x76);  
   if (!status) {
     DPRINT("Can't connect to BME Sensor!, Status  ");
     DPRINTLN(status);    
   }
   /* start PINs first soil Humidity, then Pluviometer */

   #ifdef CON_LLUVIA
    #define L_POR_BALANCEO 0.2794 // liter/m2 (=mm) for evey rain gauge interrupt
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptPin), balanceoPluviometro, RISING);
   #endif
   #ifdef CON_SUELO
      pinMode(CONTROL_HUMEDAD,OUTPUT);
     digitalWrite(CONTROL_HUMEDAD, HIGH); // prepare to read soil humidity sensor
     espera(1000);
     humedadCrudo1 = analogRead(hSueloPin); //first read to have date to get averages
     espera(1000);
     humedadCrudo2 = analogRead(hSueloPin);  //second read
     digitalWrite(CONTROL_HUMEDAD, LOW);
   #endif
   #ifdef CON_VIENTO
   #endif
   #ifdef CON_UV
    pinMode(PIN_UV, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_UV,ADC_11db) ;    
   #endif
   claves["deviceId"]=DEVICE_ID;
   claves["location"]=LOCATION;
   #ifdef CON_WIFI
   wifiConnect();
   mqttConnect();
   #endif
   #ifdef CON_OTA
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
   #endif
   delay(50);
   publicaDatos();      // and publish data. This is the function that gets and sends
}

uint32_t ultima=0;

void loop() {
 DPRINT("*");
 #ifdef CON_WIFI
 if (!loopMQTT()) {  // Check if there are MQTT messages and if the device is connected  
   DPRINTLN("Connection lost; retrying");
   sinConectividad();        
   mqttConnect();
 }
 #endif
 #ifdef CON_OTA
   ArduinoOTA.handle(); 
 #endif
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
     sprintf(datosJson,"[{\"temp\":\"NaN\"},{\"deviceId\":\"%s\",\"location\":\"%s\"}]",
            DEVICE_ID,LOCATION);
    } else {
      // Data is read an stored in global var. Prepare data in JSON mode
      if (temperatura<0) {  // to avoid probles with sign
        signo='-';          // if negative , set '-' character
        temperatura*=-1;  // if temp was negative, convert it positive 
      }  else signo=' ';
      // prepare the message
      serializeJson(docJson,datosJson);
  }
  // and publish them.
  DPRINTLN("preparing to send");
  #ifdef CON_WIFI
  pubresult = enviaDatos(publishTopic,datosJson);
  #else
  DPRINTLN(datosJson);
  #endif
      
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
  int i=0,pluvi=0;
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
      humedadCrudo = analogRead(hSueloPin); // and read soil moisture
      humedadCrudo=constrain(humedadCrudo,humedadMin,humedadMax); 
      digitalWrite(CONTROL_HUMEDAD, LOW);  // disconnect soil sensor
      // calculate the moving average of soil humidity of last three values 
      humedadCrudo=(humedadCrudo1+humedadCrudo2+humedadCrudo)/3;
      humedadSuelo = map(humedadCrudo,humedadMin,humedadMax,0,100);
      humedadCrudo2=humedadCrudo1;
      humedadCrudo1=humedadCrudo;
      if (humedadMin==humedadMax) humedadMax+=1;
      valores["hCrudo"]=humedadCrudo;
      valores["hSuelo"]=humedadsuelo; 
    #endif
    #ifdef CON_UV
      lecturaUV = analogRead(PIN_UV);
      UV_Watt=10/1.2*(4.1*lecturaUV/4095-1);  // mWatt/cm^2, at 2.2 V read there are 10mw/cm2
      UV_Index=(int) UV_Watt;
      DPRINT(" analog ");     DPRINTLN(lecturaUV);
      DPRINT("/t UV Watt  ");DPRINTLN(UV_Watt);
      DPRINT("/t UV index  ");DPRINTLN(UV_Index);
      valores["indexUV"]=UV_Watt;
    #endif
    // read again from BME280 sensor
    bufHumedad1= sensorBME280.readHumidity();
    bufTemp1= sensorBME280.readTemperature();
    bufPresion1= sensorBME280.readPressure()/100.0F;
    DPRINTLN("Data read");
    #ifdef CON_LLUVIA 
      pluvi=int(contadorPluvi/4);
      lluvia+=pluvi*L_POR_BALANCEO;
      contadorPluvi=0;
      valores["l/m2"]=lluvia;
    #endif
    /* if data could not be read for whatever reason, raise a message (in PRINT_SI mode) 
      Else calculate the mean */
    if (isnan(bufHumedad) || isnan(bufHumedad1) ||
        isnan(bufPresion) || isnan(bufPresion1) ||
        isnan(bufTemp)    || isnan(bufTemp1)    ) {       
       DPRINTLN("I could not read from BME28 sensor!");  
       DPRINT("humedad= \t");DPRINTLN(bufHumedad);
       DPRINT("Temperatura= \t");DPRINTLN(bufTemp);     
       escorrecto=false;    // flag that BME280 could not read
    } else {
      temperatura=(bufTemp+bufTemp1)/2;
      humedadAire=(bufHumedad+bufHumedad1)/2;
      presionHPa=(bufPresion+bufPresion1)/2*PRESSURE_CORRECTION;
      valores["temp"]=temperatura;      
      valores["HPa"]=(int)presionHPa;      
      valores["hAire"]=(int)humedadAire;
      escorrecto=true;  
    } 
    if (i++>30) {
      return false;
    }  
    espera(1000);
  }
  return escorrecto;
}
