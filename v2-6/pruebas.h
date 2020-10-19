#define LOCATION "LugarDePrueba"
#define DEVICE_ID "PruebasESP32"
#define TOKEN "Token-de-Pruebas"
#define CON_LLUVIA
#define CON_UV
#undef CON_SUELO   // con sensor de humedad del suelo
#define HUMEDAD_MIN  50  // valores de A0 para suelo seco y empapado
#define HUMEDAD_MAX  450
#define INTERVALO_CONEX 10000 //30 secs
