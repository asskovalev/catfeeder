#define MAX_PORTION 4
#define FEEDING_INTERVAL 4*3600
#define FEEDING_WDT 90
#define SIGN(X) (X == 0) ? 0 : (X > 0 ? 1 : -1)
#define TEST

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const int buttonPin = D3;
#ifdef TEST
const int relayPin = D4;
#else
const int relayPin = D1;
#endif
volatile bool button_down = 0;
volatile bool button_up = 0;
volatile int portion = 0;
bool feeding = 0;
long tick_t = 0;
long tick_s = 0;
long next_feed_after = 3;

enum { FEED_WDT, NUM_SCHEDULES_S } schedules_s_t;
long schedules_s[NUM_SCHEDULES_S];


const char* ssid     = "xtdp-link";
const char* password = "endless1987";

const char* mqtt_host = "m20.cloudmqtt.com";
const int   mqtt_port = 14034;
const char* mqtt_user = "t1";
const char* mqtt_pass = "4537";

WiFiClient wclient;
PubSubClient client(mqtt_host, mqtt_port, wclient);


void schedule_wdt(){
  schedules_s[FEED_WDT] = tick_s + FEEDING_WDT;
  Serial.print("WDT: ");
  Serial.println(schedules_s[FEED_WDT]);
}

void motor(int on){
  #ifdef TEST
  pinMode(relayPin, !on ? INPUT_PULLUP : OUTPUT);
  #else
  digitalWrite(relayPin, on ? HIGH : LOW);
  #endif
}

void btn_down() {
  detachInterrupt(digitalPinToInterrupt(buttonPin));
  button_down = 1;
}

void btn_up() {
  detachInterrupt(digitalPinToInterrupt(buttonPin));
  button_up = 1;
}

void motor_setup(){
  #ifdef TEST
  pinMode(buttonPin, INPUT_PULLUP);
  #else
  pinMode(relayPin, OUTPUT);
  #endif
}

void wifi_setup() {
  int max_attempts = 10;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to ssid ");
    Serial.print(ssid);
    
    WiFi.begin(ssid, password);
    
    while (--max_attempts > 0 && WiFi.status() != WL_CONNECTED ) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
      return;

    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}
void mqtt_callback(const char* topic, byte* payload, unsigned int length){
  Serial.print(topic); // выводим в сериал порт название топика
  Serial.print(" => ");
  Serial.write(payload, length); // выводим в сериал порт значение полученных данных
  Serial.println("");
  next_feed_after = 1;
  
//  String payload = pub.payload_string();
//  
//  if(String(pub.topic()) == "test/led"){ // проверяем из нужного ли нам топика пришли данные 
//    int stled = payload.toInt(); // преобразуем полученные данные в тип integer
//    digitalWrite(5,stled); // включаем или выключаем светодиод в зависимоти от полученных значений данных
//  }
}

void mqtt_setup(){
  if (!client.connected()) {
    Serial.print("Connecting to MQTT server "); 
    Serial.print(mqtt_user); Serial.print(":"); Serial.print(mqtt_pass); 
    Serial.print("@"); 
    Serial.print(mqtt_host); Serial.print(":"); Serial.print(mqtt_port);
    Serial.println("");
    if (client.connect("esp12e", mqtt_user, mqtt_pass)) {
      Serial.println("Connected to MQTT server");
      client.setCallback(mqtt_callback);
      client.subscribe("feeder/feed"); // подписывааемся по топик с данными для светодиода
    } else {
      Serial.print("Could not connect to MQTT server: "); 
      Serial.println(client.state());
    }
  }
}


void pub_int(const char* topic, int value){
  if (!client.connected())
    return;
  char _value[20];
  itoa(value, _value, 10);
  client.publish(topic, _value);
    
}

void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(buttonPin, INPUT_PULLUP);
  motor_setup();
  motor(0);
  Serial.begin(115200);
  Serial.println("starting");
  wifi_setup();
  mqtt_setup();
  next_feed_after = 5;
  tick_s = 0;
  tick_t = millis();
}


void onFeedingEnd(){
  detachInterrupt(digitalPinToInterrupt(buttonPin));
  schedules_s[FEED_WDT] = 0;
  button_down = 0;
  button_up = 0;
  portion = 0;
  motor(0);
  feeding = 0;
  Serial.println("feeding done");
  pub_int("feeder/feeding_end", tick_s);
  next_feed_after = FEEDING_INTERVAL;
}

void onFeedingStart(){
  Serial.println("start feeding");
  pub_int("feeder/feeding_start", tick_s);
  schedule_wdt();
  feeding = 1;
  portion = 0;
  attachInterrupt(digitalPinToInterrupt(buttonPin), btn_down, FALLING);
  motor(1);
  
}

void onButtonDown(){
  delay(40);
  attachInterrupt(digitalPinToInterrupt(buttonPin), btn_up, RISING);
}

void onButtonUp(){
  portion += 1;
  Serial.print("fed ");
  Serial.println(portion);
  if (portion == MAX_PORTION){
    onFeedingEnd();
  } else {
    delay(40);
    attachInterrupt(digitalPinToInterrupt(buttonPin), btn_down, FALLING);
  }
}

bool do_tick(long now){
  if (now - tick_t >= 1000){
    tick_t = now;
    int delta = tick_t % 1000;
    if (delta > 0){
      tick_t -= 1 + delta >> 1;
      Serial.print("time adj");
      Serial.println(tick_t);
    }
    return 1;
  }
  return 0;
}


// the loop function runs over and over again forever
void loop() {
  long now = millis();
  bool tick = do_tick(now);
  if (schedules_s[FEED_WDT] && schedules_s[FEED_WDT] <= tick_s){
    pub_int("feeder/feeding_wdt", tick_s);
    Serial.println("feeding error (timeout)");
    onFeedingEnd();
  }
  if (!feeding && !next_feed_after){
    onFeedingStart();
  }

  if(feeding && button_down){
    schedule_wdt();
    button_down = 0;
    onButtonDown();

  }
  if(feeding && button_up){
    button_up = 0;
    onButtonUp();

  }

  if (tick){
    tick_s = (tick_s + 1) % 864000;
    Serial.print(".");
    if (tick_s % 60 == 0){
      pub_int("feeder/uptime", tick_s);
      Serial.println("\n=======");
      Serial.print("Seconds: ");
      Serial.println(tick_s);
      Serial.print("WDT: ");
      Serial.println(schedules_s[FEED_WDT]);
      Serial.print("feeding: ");
      Serial.println(feeding);
      Serial.println("=======\n");
    }

    if (!feeding && next_feed_after){
      if(0 == next_feed_after % 60){
        Serial.print("nfa: ");
        Serial.println(next_feed_after);
      }
      next_feed_after --;
    }

    wifi_setup();
    mqtt_setup();
    if (client.connected()){
      client.loop();
    }
  }
  tick = 0;
}


