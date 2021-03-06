/* Monitoramento de Água
 * PI-3 
 * Equipe: Ian Dannapel, Jhonatan Lang
 * Placa: NodeMCU 1.0 (ESP-12E Module)
 * https://github.com/Everton-LF-Santos/Projeto-Integrador-3-2018-1/tree/Uso-Sustent%C3%A1vel-%C3%81gua * 
 */

#include <PubSubClient.h>   // https://pubsubclient.knolleary.net/
#include <ESP8266WiFi.h>    // https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi
#include <HCSR04.h>         // https://github.com/gamgine/HCSR04-ultrasonic-sensor-lib

//-----------------Wifi/MQTT Define--------------------------
#define WIFI_AP "RedePI3"
#define WIFI_PASSWORD "noisqtapi3"

//-----------------HCSR04 Define---------------------------
#define TRIG_PIN D6
#define ECHO_PIN D5
#define d_min 3.29     //distancia (cm) do sensor até o topo da água
#define V_max 9.5      // volume útil da caixa dágua
float Volume_T;

//----------------------- Sensor de Fluxo -------------------------------
#define buttonPin D3      // variable pin
float P;                  //razão pulsos por segundo equivalentes ao fluxo
volatile int pulseCount;  //contador de pulsos
unsigned int flowRate;    //fluxo

//----------------------- MQTT/Wifi -------------------------------------
#define TOKEN "jr6lVU4AZAPJE5nVKtVO"
char thingsboardServer[] = "demo.thingsboard.io";
WiFiClient wifiClient;
PubSubClient client(wifiClient);
int status = WL_IDLE_STATUS;
unsigned long oldTime;
unsigned long deltaT;

//-------------------- ADC0 - Bateria -----------------------------------
#define analogPin A0
#define multiplicador 3.76   //R2/(R1+R2)
#define V_bat_max 9.3
#define V_bat_min 7.5
float v_bat;

//-------------------- Interrupção Sensor de Fluxo ----------------------
void pin_ISR(){
  pulseCount ++;
}

//-----------------------------------------------------------------------
HCSR04 hcsr04(TRIG_PIN,ECHO_PIN);//initialisation class HCSR04 (trig pin , echo pin)

void setup()
{
  Serial.begin(115200);
  delay(10);
  
  pulseCount        = 0;
  flowRate          = 0;
  oldTime           = 0;
  
  pinMode(buttonPin, INPUT_PULLUP);    
  InitWiFi();
  client.setServer(thingsboardServer, 1883);

  delay (1000);
  attachInterrupt(digitalPinToInterrupt(buttonPin), pin_ISR, RISING);

  // ADC bateria
  pinMode(analogPin, INPUT);
}
//-----------------------------------------------------------------------
void loop()
{  
  client.loop();
  if (!client.connected())
      reconnect();
  
  deltaT = millis() - oldTime;
  if(deltaT >= 5000){
      oldTime = millis();      
      
      //---------- Publica Fluxo -------------------------
      P = (pulseCount)/(deltaT/1000.0);
      flowRate = 126.23*P+416.8;     //y = 126.23*x + 416.8
      pulseCount=0;
      if (flowRate < 420)
        flowRate = 0;

      sendMQTT("Flow",flowRate);
      
      //---------- Publica Tensão Bateria-----------------
      v_bat = analogRead(analogPin);
      v_bat = (v_bat*3.1*multiplicador)/1024;

      sendMQTT("V_Bat",((v_bat-V_bat_min)/(V_bat_max- V_bat_min))* 100); 

      //---------- Publica Volume  ---------------------
      Volume_T = V_total(hcsr04.dist(),V_max, d_min);

      sendMQTT("Vol",Volume_T); 

  }  
  delay(2);     
}
//-----------------------------------------------------------------------
void InitWiFi()
{
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}
//-----------------------------------------------------------------------
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      WiFi.begin(WIFI_AP, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connected to AP");
    }
    Serial.print("Connecting to ThingsBoard node ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect("ESP8266 Device", TOKEN, NULL) ) {
      Serial.println( "[DONE]" );
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//-----------------------------------------------------------------------

float V_total(float dist_F, float v_maximo, float dist_minima){
  float delta_D = (dist_F - dist_minima);
  return(v_maximo - 0.7427 * delta_D + 0.5884) ;  // Aproximação linear
}

//-----------------------------------------------------------------------
void sendMQTT(String titulo, float dado){
      char attributes[100];
      String payload = "{\"";      
      payload += titulo;
      payload += "\":"; payload += dado; payload += "}";
      payload.toCharArray(attributes, 100);
      client.publish("v1/devices/me/telemetry", attributes);
}

