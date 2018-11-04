/* Backplane unit ID (must be unique) */
int backplane_number = 9;
/* number of slots on backplane unit (would be either 3 or 4) */
const int numBackplaneSlots=3;

/* MQTT Configuration */
#define mqtt_server "10.1.1.58"
#define mqtt_caddy_topic "bp/power"
#define mqtt_client_name "backplaneunit"

/* MQTT Authentication if used */
#define mqtt_auth true
#define mqtt_user "user"
#define mqtt_password "password"

/* OTA Password */
#define ota_auth true
#define ota_password "password"

/* WIFI Configuration */
const char* ssid = "SSID";
const char* password = "SECRETPASS";
