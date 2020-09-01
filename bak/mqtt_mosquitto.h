#include <WiFi.h>
#define MQTT_MAX_PACKET_SIZE 455 //cambialo antes de incluir docpatth\Arduino\libraries\pubsubclient-master\src\pubsubclient.h
#define MQTT_KEEP_ALIVE 60
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3
#include <Pin_NodeMCU.h>

/*************************************************
 ** -------- Valores Personalizados ----------- **
 * ***********************************************/
#define DEVICE_TYPE "ESP12E-Riego"
char* ssid;
char* password;
/*********** personal.h should include SSID and passwords  ***********
 *  something like: 
char ssid1[] = "ssid1";
char password1[] = "Password_ssid1";
char ssid2[] = "ssid2";
char password2[] = "Password_ssid2";
 */
#include "personal.h"   

/*************************************************
 ** ----- Fin de Valores Personalizados ------- **
 * ***********************************************/
#define ESPERA_NOCONEX 30000  // When no conection, wait 30 sec
char server[] = "192.168.1.20";
char * authMethod = NULL;
char * token = NULL;
char clientId[] = "d:" LOCATION":" DEVICE_TYPE ":" DEVICE_ID;

char publishTopic[] = "meteo/envia";  // device send data to mqtt
char metadataTopic[]= "meteo/envia/metadata"; //device send metadata to mqtt
char updateTopic[]  = "meteo/update";    // device metadata is to be updated
char responseTopic[]= "meteo/response";
char rebootTopic[]  = "meteo/reboot";
