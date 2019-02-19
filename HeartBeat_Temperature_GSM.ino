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

  mySerial.begin(9600); // Setting the baud rate of GSM Module
  Serial.begin(9600);   // Setting the baud rate of Serial Monitor (Arduino)
  interruptSetup();
  pinMode(blinkPin, OUTPUT); // pin that will blink to your heartbeat!
  pinMode(pulsePin, INPUT);
  mySerial.println("AT+SAPBR=3,1,\"APN\",\"airtelgprs.com\"");
  delay(100);
  mySerial.println("AT+SAPBR=1,1");
  delay(1500);
}

void loop()
{

  int value = analogRead(inPin);
  float millivolts = (value / 1024.0) * 5000;
  celsius = millivolts / 10;

  Serial.println(celsius);
  //Serial.println(BPM);
  String temp = String(celsius);
  String beat = String(BPM);
  mySerial.println("AT+HTTPINIT");
  delay(100);
  mySerial.println("AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/update?key=TYNCY7N8NUYJHNF4&field1=" + temp + "&field2=" + beat + "\"");
  sendDataToProcessing('S', Signal); // send Processing the raw Pulse Sensor data

  if (QS == true)
  {                                 // Quantified Self flag is true when arduino finds a heartbeat
    sendDataToProcessing('B', BPM); // send heart rate with a 'B' prefix
    QS = false;                     // reset the Quantified Self flag for next time
  }
  delay(200);
  mySerial.println("AT+HTTPACTION=0");
  delay(4000);
  mySerial.println("AT+HTTPTERM");
  delay(54000);
}

void sendDataToProcessing(char symbol, int data)
{

  if (symbol == 'B')
  {
   // Serial.println("Hear Beat Detected" + String(data));
  //  mySerial.print("AT+HTTPPARA=\"URL\",\"https://api.thingspeak.com/update?api_key=TYNCY7N8NUYJHNF4&field1=" + String(celsius) + "&field2=" + String(data) + "\"");
  }
  /*Serial.print(symbol); 
  Serial.println(data); */
}
