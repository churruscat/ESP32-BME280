#define LOCATION "Denia"
#define DEVICE_ID "Salon"
#define TOKEN "Token-deniainterior"
#undef CON_UV      //UV sensor
#undef CON_SUELO
#undef CON_LLUVIA  // con pluviómetro
#define PRESSURE_CORRECTION (1.0)  // HPAo/HPHh 0m
#define HUMEDAD_MIN  150  /* valores de A0 para suelo seco y empapado*/
#define HUMEDAD_MAX  850
#define INTERVALO_CONEX 58000 // 1 min en milisecs
