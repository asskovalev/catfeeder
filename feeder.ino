#define MAX_PORTION 4
#define FEEDING_INTERVAL 4*3600
#define FEEDING_WDT 90
#define SIGN(X) (X == 0) ? 0 : (X > 0 ? 1 : -1)
const int buttonPin = D3;
const int relayPin = D1;
volatile bool button_down = 0;
volatile bool button_up = 0;
volatile int portion = 0;
bool feeding = 0;
long tick_t = 0;
long tick_s = 0;
long next_feed_after = 3;

enum { FEED_WDT, NUM_SCHEDULES_S } schedules_s_t;
long schedules_s[NUM_SCHEDULES_S];

void schedule_wdt(){
  schedules_s[FEED_WDT] = tick_s + FEEDING_WDT;
  Serial.print("WDT: ");
  Serial.println(schedules_s[FEED_WDT]);
}

void motor(int on){
//  pinMode(relayPin, !on ? INPUT_PULLUP : OUTPUT);
  digitalWrite(relayPin, on ? HIGH : LOW);
}

void btn_down() {
  detachInterrupt(digitalPinToInterrupt(buttonPin));
  button_down = 1;
}

void btn_up() {
  detachInterrupt(digitalPinToInterrupt(buttonPin));
  button_up = 1;
}

void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  Serial.begin(115200);
  Serial.println("starting");
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
  next_feed_after = FEEDING_INTERVAL;
}

void onFeedingStart(){
  Serial.println("start feeding");
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
    Serial.println("feeding error");
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
    if (tick_s % 60 == 0){
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
  }
  tick = 0;
}


