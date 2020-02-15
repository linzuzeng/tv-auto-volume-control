#include <IRremote.h>
#include <TM1638lite.h>


#define NOISE_PRESS  1 // how many times to transmit the IR remote code
#define REMOTE_DOWN_CODE     0x4FB10EF  // remote code to transmit. This is for my TV. Replace with yours.
#define REMOTE_UP_CODE 0x4FB08F7 
#define REMOTE_BIT   4*8

#define SOUND_SENSOR_PIN     A0 // sound sensor connected to this analog pin
#define LED        13      // LED used to blink when volume too high

#define FILTER_SIZE 150
#define NOISE_CONTRAST 15
#define DESIRED_LEVEL 11 // level on TV

#define RMS_SIZE 5
#define TOTAL_LEVELS 24
#define REALMODE true
IRsend irsend; // instantiate IR object
TM1638lite tm(4, 7, 8);

long runningsum = 0;
float runningsqr = 0;
int filter[FILTER_SIZE];
long filter_sqr[FILTER_SIZE];


int hist[TOTAL_LEVELS];
void setup()
{
  pinMode(LED, OUTPUT);
  Serial.begin(9600);
  //analogReference(DEFAULT);
  analogReference(INTERNAL);
  
  for (int i=0;i<FILTER_SIZE;i++){
    filter[i]=0;
    filter_sqr[i]=0;
  }
  for (int i=0;i<TOTAL_LEVELS;i++){
    hist[i]=0;
    
  }
  tm.reset();
  if (REALMODE){
     digitalWrite(LED,HIGH);  // LED on
      
      //reset volume
      for (uint8_t presstimes=0; presstimes<15;presstimes++){
          irsend.sendNEC(REMOTE_DOWN_CODE , REMOTE_BIT); // Change to match your remote protocol
          delay(1000);
      }
      //reset volume
      for (uint8_t presstimes=0; presstimes<55;presstimes++){
          irsend.sendNEC(REMOTE_DOWN_CODE , REMOTE_BIT); // Change to match your remote protocol
          delay(100);
      }
      delay(500);
      for (uint8_t presstimes=0; presstimes<50;presstimes++){
          irsend.sendNEC(REMOTE_UP_CODE , REMOTE_BIT); // Change to match your remote protocol
          delay(150);
      }
      
      digitalWrite(LED,LOW);  // LED on
  }
 
}


uint8_t level = TOTAL_LEVELS-1; 
int filter_i = 0;
int samples_i = 1;
unsigned long last_shown = 0;

uint8_t level_new=0;


int samples_j = 1;
void loop()
{
  samples_i=(samples_i+1)%10000;
  samples_j=(samples_j+1)%RMS_SIZE;
  
  //filter
  filter_i=(filter_i+1)%FILTER_SIZE;
  //dequeue
  runningsum -= (long)filter[filter_i];
  runningsqr -= (float)filter_sqr[filter_i];
  // read the sound sensor
  const int A0value = analogRead(SOUND_SENSOR_PIN);
  filter[filter_i]= A0value; 
  filter_sqr[filter_i]= (long)A0value*(long)A0value; 
  //enqueue
  runningsum += (long)filter[filter_i];
  runningsqr += (float)filter_sqr[filter_i];
  

  
  // generate proposal 
  if (samples_j==0){
    const float SquaredSum =  (float)runningsqr-((float)runningsum*(float)runningsum)/(float)FILTER_SIZE;
    //Serial.print(SquaredSum/ (float)FILTER_SIZE);
    //Serial.print("\n");
    const float equalTVlevel = (10*log10(SquaredSum/ (float)FILTER_SIZE)-14.0);
    if (abs(millis()-last_shown)>300){
      tm.displayText(String((int)level, HEX) +" "+ String((int)(0.79*equalTVlevel), DEC) );
      last_shown=millis();
    }
    const int targetlevel = TOTAL_LEVELS+(DESIRED_LEVEL-equalTVlevel);
    //Serial.print(targetlevel);
    //Serial.print("\n");
    int level_proposal = max(min(  targetlevel  ,TOTAL_LEVELS-1),0);
    hist[level_proposal]++; //vote
    //choose a new level
   
  }
  
  // lookup hist and send
  if (REALMODE && samples_i==0 ){
    for (level_new=0;level_new<TOTAL_LEVELS-1;level_new++){
      if (hist[level_new]>NOISE_CONTRAST){    
        break;
      }
    }
    //clear histogram
    for (uint8_t i=0;i<TOTAL_LEVELS-1;i++){
      /*
      Serial.print(hist[i]);
      Serial.print("\t");
      */
      hist[i]=0;
    }
    /*
    Serial.println("\t");
    Serial.println(level_new);
    Serial.println("\t");
     */
    //adjust volume
    for (uint8_t lowering_count =0; lowering_count<3;lowering_count++){
      if (level<level_new){
        digitalWrite(LED,HIGH);  // LED on
        for (uint8_t presstimes=0; presstimes<NOISE_PRESS;presstimes++){
          irsend.sendNEC(REMOTE_UP_CODE , REMOTE_BIT); // Change to match your remote protocol
          delay(150);
        }
        digitalWrite(LED,LOW); // LED off
        level++;
     
      }
    }
    for (uint8_t lowering_count =0; lowering_count<10;lowering_count++){
      if (level>level_new){
        digitalWrite(LED,HIGH);  // LED on
        for (uint8_t presstimes=0; presstimes<NOISE_PRESS;presstimes++){
          irsend.sendNEC(REMOTE_DOWN_CODE , REMOTE_BIT); // Change to match your remote protocol
          delay(150);
        }
        digitalWrite(LED,LOW); // LED off
        level--;

      }
    }
 
    //refresh display
    for (uint8_t position = 0; position < 8; position++) {
         tm.setLED(position, (level-(TOTAL_LEVELS-8)) >= position);
    }
    
       
  }
}