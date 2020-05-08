#define LOCATION "Pozuelo"
#define DEVICE_ID "Salon"
#define TOKEN "Token-del-Salon"
#define CON_SUELO   // con sensor de humedad del suelo
#undef  CON_LLUVIA  // con pluviómetro
#define HUMEDAD_MIN  10  // valores de A0 para suelo seco y empapado
#define HUMEDAD_MAX  300
#define INTERVALO_CONEX 288000 // 5 min en milisecs
#define TRIEGO 4       // minutos
#define UMBRALRIEGO 70  // indice de suelo
