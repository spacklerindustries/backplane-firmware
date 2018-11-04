/* Backplane unit ID (must be unique) */
int backplane_number = 1;
/* number of slots on backplane unit (would be either 3 or 4) */
const int numBackplaneSlots=3;

/* MQTT Configuration */
const char* mqtt_server = "10.1.1.58";
#define mqtt_tls true
const int mqtt_tls_port = 8883;
const int mqtt_port = 1883;
//TODO: add verification to mqtt reconnect
//define mqtt_server_fingerprint "60 B8 4C 1A 1C 26 BE 31 CF 6D 84 C3 79 8D BC F8 75 13 5B FA"
const char* mqtt_caddy_topic = "bp/power";
char cstr[16];
String client_name = "backplaneunit-" + String(backplane_number);
const char* mqtt_client_name = client_name.c_str();

/* MQTT Authentication if used */
bool mqtt_auth = true;
const char* mqtt_user = "user";
const char* mqtt_password = "password";

/* OTA Password */
bool ota_auth = true;
const char* ota_password = "password";

/* WIFI Configuration */
const char* ssid = "SSID";
const char* password = "SECRETPASS";
