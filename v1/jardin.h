#define LOCATION "Pozuelo"
#define DEVICE_ID "Jardin"
#define TOKEN "Token-del-Jardin"
#undef CON_LLUVIA
#define CON_SUELO   // con sensor de humedad del suelo
#define HUMEDAD_MIN  50  // valores de A0 para suelo seco y empapado
#define HUMEDAD_MAX  450
#define INTERVALO_CONEX 288000 //5 min en milisecs
#define TRIEGO 4       // minutos
#define UMBRALRIEGO 70  // indice de suelo
