#include <SoftwareSerial.h>
SoftwareSerial mySerial(10, 11);
const int inPin = A0; // temperature sensor connected to analog pin 0
float celsius;
int pulsePin = A1; // Pulse Sensor connected to analog pin 1
int blinkPin = 13; // pin to blink led at each beat

// these variables are volatile because they are used during the interrupt service routine!
volatile int BPM;               // used to hold the pulse rate
volatile int Signal;            // holds the incoming raw data
volatile int IBI = 600;         // holds the time between beats, the Inter-Beat Interval
volatile boolean Pulse = false; // true when pulse wave is high, false when it's low
volatile boolean QS = false;    // becomes true when Arduoino finds a beat.
void setup()
{
  pinMode(blinkPin, OUTPUT); // pin that will blink to your heartbeat!
  mySerial.begin(1200);      // Setting the baud rate of GSM Module
  Serial.begin(1200);        // Setting the baud rate of Serial Monitor (Arduino)
  interruptSetup();
  pinMode(pulsePin, INPUT);
  delay(100);

  mySerial.println("AT+SAPBR=3,1,\"APN\",\"airtelgprs.com\"");
  mySerial.println("AT+SAPBR=1,1");
}

void loop()
{

  int value = analogRead(inPin);
  float millivolts = (value / 1024.0) * 5000;
  celsius = millivolts / 10;

  Serial.println(celsius);

  mySerial.println("AT+HTTPINIT");

  sendDataToProcessing('S', Signal); // send Processing the raw Pulse Sensor data

  if (QS == true)
  {                                 // Quantified Self flag is true when arduino finds a heartbeat
    sendDataToProcessing('B', BPM); // send heart rate with a 'B' prefix
    QS = false;                     // reset the Quantified Self flag for next time
  }
  mySerial.println("AT+HTTPACTION=0");

  mySerial.println("AT+HTTPTERM");
  delay(20);
  if (Serial.available() > 0)
    switch (Serial.read())
    {
    case 's':
      SendMessage();
      break;
    }

  if (mySerial.available() > 0)
    Serial.write(mySerial.read());
}

void SendMessage()
{
  mySerial.println("AT+CMGF=1");                   //Sets the GSM Module in Text Mode
  delay(1000);                                     // Delay of 1000 milli seconds or 1 second
  mySerial.println("AT+CMGS=\"+919769743488\"\r"); // Replace x with mobile number
  delay(1000);
  mySerial.println("I am SMS from GSM Module"); // The SMS text you want to send
  delay(100);
  int value = analogRead(inPin);
  float millivolts = (value / 1024.0) * 5000;
  celsius = millivolts / 10;
  Serial.print(celsius);
  mySerial.print("TEMPRATURE = ");
  mySerial.print(celsius);
  mySerial.println("*C");
  mySerial.print("heart beat =");
  mySerial.print(BPM);
  delay(100);
  mySerial.println((char)26); // ASCII code of CTRL+Z
  delay(1000);
}

void sendDataToProcessing(char symbol, int data)
{

  if (symbol == 'B')
  {
    Serial.println(data);
    mySerial.println("AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/update?key=&field1=" + celsius + "&field2=" + data + "\"");
  }
  //  Serial.print(symbol);                // symbol prefix tells Processing what type of data is coming
  // Serial.println(data);
}

volatile int rate[10];                    // used to hold last ten IBI values
volatile unsigned long sampleCounter = 0; // used to determine pulse timing
volatile unsigned long lastBeatTime = 0;  // used to find the inter beat interval
volatile int P = 512;                     // used to find peak in pulse wave
volatile int T = 512;                     // used to find trough in pulse wave
volatile int thresh = 512;                // used to find instant moment of heart beat
volatile int amp = 100;                   // used to hold amplitude of pulse waveform
volatile boolean firstBeat = true;        // used to seed rate array so we startup with reasonable BPM
volatile boolean secondBeat = true;       // used to seed rate array so we startup with reasonable BPM

void interruptSetup()
{
  // Initializes Timer2 to throw an interrupt every 2mS.
  TCCR2A = 0x02; // DISABLE PWM ON DIGITAL PINS 3 AND 11, AND GO INTO CTC MODE
  TCCR2B = 0x06; // DON'T FORCE COMPARE, 256 PRESCALER
  OCR2A = 0X7C;  // SET THE TOP OF THE COUNT TO 124 FOR 500Hz SAMPLE RATE
  TIMSK2 = 0x02; // ENABLE INTERRUPT ON MATCH BETWEEN TIMER2 AND OCR2A
  sei();         // MAKE SURE GLOBAL INTERRUPTS ARE ENABLED
}

// THIS IS THE TIMER 2 INTERRUPT SERVICE ROUTINE.
// Timer 2 makes sure that we take a reading every 2 miliseconds
ISR(TIMER2_COMPA_vect)
{                                       // triggered when Timer2 counts to 124
  cli();                                // disable interrupts while we do this
  Signal = analogRead(pulsePin);        // read the Pulse Sensor
  sampleCounter += 2;                   // keep track of the time in mS with this variable
  int N = sampleCounter - lastBeatTime; // monitor the time since the last beat to avoid noise

  //  find the peak and trough of the pulse wave
  if (Signal < thresh && N > (IBI / 5) * 3)
  { // avoid dichrotic noise by waiting 3/5 of last IBI
    if (Signal < T)
    {             // T is the trough
      T = Signal; // keep track of lowest point in pulse wave
    }
  }

  if (Signal > thresh && Signal > P)
  {             // thresh condition helps avoid noise
    P = Signal; // P is the peak
  }             // keep track of highest point in pulse wave

  //  Cautare semnal de puls
  // signal surges up in value every time there is a pulse
  if (N > 250)
  { // avoid high frequency noise
    if ((Signal > thresh) && (Pulse == false) && (N > (IBI / 5) * 3))
    {
      Pulse = true;                       // set the Pulse flag when we think there is a pulse
      digitalWrite(blinkPin, HIGH);       // turn on pin 13 LED
      IBI = sampleCounter - lastBeatTime; // measure time between beats in mS
      lastBeatTime = sampleCounter;       // keep track of time for next pulse

      if (firstBeat)
      {                    // if it's the first time we found a beat, if firstBeat == TRUE
        firstBeat = false; // clear firstBeat flag
        return;            // IBI value is unreliable so discard it
      }
      if (secondBeat)
      {                     // if this is the second beat, if secondBeat == TRUE
        secondBeat = false; // clear secondBeat flag
        for (int i = 0; i <= 9; i++)
        { // seed the running total to get a realisitic BPM at startup
          rate[i] = IBI;
        }
      }

      // keep a running total of the last 10 IBI values
      word runningTotal = 0; // clear the runningTotal variable

      for (int i = 0; i <= 8; i++)
      {                          // shift data in the rate array
        rate[i] = rate[i + 1];   // and drop the oldest IBI value
        runningTotal += rate[i]; // add up the 9 oldest IBI values
      }

      rate[9] = IBI;              // add the latest IBI to the rate array
      runningTotal += rate[9];    // add the latest IBI to runningTotal
      runningTotal /= 10;         // average the last 10 IBI values
      BPM = 60000 / runningTotal; // how many beats can fit into a minute? that's BPM!

      /* Serial.print(BPM);
      Serial.print('\n'); */

      QS = true; // set Quantified Self flag
      // QS FLAG IS NOT CLEARED INSIDE THIS ISR
    }
  }

  if (Signal < thresh && Pulse == true)
  {                              // when the values are going down, the beat is over
    digitalWrite(blinkPin, LOW); // turn off pin 13 LED
    Pulse = false;               // reset the Pulse flag so we can do it again
    amp = P - T;                 // get amplitude of the pulse wave
    thresh = amp / 2 + T;        // set thresh at 50% of the amplitude
    P = thresh;                  // reset these for next time
    T = thresh;
  }

  if (N > 2500)
  {                               // if 2.5 seconds go by without a beat
    thresh = 512;                 // set thresh default
    P = 512;                      // set P default
    T = 512;                      // set T default
    lastBeatTime = sampleCounter; // bring the lastBeatTime up to date
    firstBeat = true;             // set these to avoid noise
    secondBeat = true;            // when we get the heartbeat back
  }

  sei(); // enable interrupts when youre done!
}

