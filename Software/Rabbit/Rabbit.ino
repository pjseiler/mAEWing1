// TAYLOR, BRIAN R.
// brtaylor@umn.edu
// 2015-07-21
// 
// v0.7
//

#include <ADC.h>            // ADC library
#include <Servo.h>          // Servo library
#include <TinyGPS++.h>      // TinyGPS++ library for NMEA parsing
#include <SoftwareSerial.h> // Software serial library for additional serial lines
#include <i2c_t3.h>         // I2C library

byte pack[59];    // array of bytes for sending
byte unpack[18];    // array of bytes for receiving

ADC *adc = new ADC(); // an ADC object
int ain[6];           // array to store ADC values
int ain_buff[6];      // array to store ADC values

static double u_a[2] = {0,0}; // input of low pass filter {u(k), u(k-1)}
static double y_a[2] = {0,0}; // output of low pass filter {y(k), y(k-1)}
static double u_b[2] = {0,0}; // input of low pass filter {u(k), u(k-1)}
static double y_b[2] = {0,0}; // output of low pass filter {y(k), y(k-1)}
static double u_c[2] = {0,0}; // input of low pass filter {u(k), u(k-1)}
static double y_c[2] = {0,0}; // output of low pass filter {y(k), y(k-1)}
static double u_d[2] = {0,0}; // input of low pass filter {u(k), u(k-1)}
static double y_d[2] = {0,0}; // output of low pass filter {y(k), y(k-1)}
static double u_e[2] = {0,0}; // input of low pass filter {u(k), u(k-1)}
static double y_e[2] = {0,0}; // output of low pass filter {y(k), y(k-1)}
static double u_f[2] = {0,0}; // input of low pass filter {u(k), u(k-1)}
static double y_f[2] = {0,0}; // output of low pass filter {y(k), y(k-1)}

TinyGPSPlus gps;                            // GPS object
static const uint8_t RXPin = 9, TXPin = 10; // software serial pins
static const uint32_t GPSBaud = 9600;       // GPS baud rate
SoftwareSerial GPSSERIAL(RXPin, TXPin);     // software serial to GPS
static uint8_t satVisible;                  // number of satellites visible 
union{                                      // latitude
  double val;
  byte b[8];
}latitude;

union{                                      // longitude
  double val;
  byte b[8];
}longitude;

union{                                      // altitude
  double val;
  byte b[8];
}alt;

union{                                      // ground track
  float val;
  byte b[4];
}track;

union{                                      // ground speed
  float val;
  byte b[4];
}gspeed;

uint32_t gpstime;                           // raw GPS time, used to see if we got an update
uint32_t oldgpstime = 0;                    // raw GPS time, used to see if we got an update
byte isupdated;                             // byte to store if update [1] or not [0]

int incomingByte; // byte received from Goldy

/* Timing variables to stabilize frame rate */
unsigned long start_time;
unsigned long end_time;
unsigned long frame_time = 1000; // 1 kHz frame rate, us

byte ps_byte[2];  // air data bytes
byte pd_byte[2];  // air data bytes

byte pwm_byte[8]; // pwm input bytes

// manual / auto GPIO mode
int modePin = 2;
byte modeVal;

// light blinking stuff
int j = 0;

// servo objects
Servo tinyservo_a;
Servo tinyservo_b;
Servo tinyservo_c;
Servo tinyservo_d;
Servo tinyservo_e;
Servo tinyservo_f;
Servo tinyservo_g;
Servo tinyservo_h;
Servo tinyservo_i;

uint16_t pwmcmd[9]; // array of microsecond commands

void setup() {
  // port for debugging
  Serial.begin(9600);

  // serial connection with GPS
  GPSSERIAL.begin(GPSBaud);

  // I2C for air data and PWM input
  Wire.begin(I2C_MASTER, 0, I2C_PINS_18_19, I2C_PULLUP_EXT, I2C_RATE_400);

   // I2C for data transfer with MPC5200
  Wire1.begin(I2C_SLAVE, 0x03, I2C_PINS_29_30, I2C_PULLUP_EXT, I2C_RATE_400);
  Wire1.onRequest(requestEvent);
  Wire1.onReceive(receiveEvent);

  // sets the digital mode pin as input
  pinMode(modePin, INPUT);      

  // sets the LED pin as an output
  
  pinMode(13, OUTPUT);

  // setting up ADC0
  adc->setAveraging(1);
  adc->setResolution(16);
  adc->setConversionSpeed(ADC_HIGH_SPEED_16BITS);
  adc->setSamplingSpeed(ADC_HIGH_SPEED_16BITS);

  // setting up ADC1
  adc->setAveraging(1,ADC_1);
  adc->setResolution(16,ADC_1);
  adc->setConversionSpeed(ADC_HIGH_SPEED_16BITS,ADC_1);
  adc->setSamplingSpeed(ADC_HIGH_SPEED_16BITS,ADC_1);

  // attaching servos to pins
  tinyservo_a.attach(23);
  tinyservo_b.attach(22);
  tinyservo_c.attach(21);
  tinyservo_d.attach(3);
  tinyservo_e.attach(4);
  tinyservo_f.attach(5);
  tinyservo_g.attach(6);
  tinyservo_h.attach(7);
  tinyservo_i.attach(8);

  // send the servos to zero initially
  tinyservo_a.writeMicroseconds(1500);
  tinyservo_b.writeMicroseconds(1500);
  tinyservo_c.writeMicroseconds(1500);
  tinyservo_d.writeMicroseconds(1500);
  tinyservo_e.writeMicroseconds(1500);
  tinyservo_f.writeMicroseconds(1500);
  tinyservo_g.writeMicroseconds(1500);
  tinyservo_h.writeMicroseconds(1500);
  tinyservo_i.writeMicroseconds(1500);
}

void loop() {

  // get the frame start time
  start_time = micros();

  // read ADC
  ain[0] = adc->analogRead(A0, ADC_0);
  ain[1] = adc->analogRead(A2, ADC_1);
  ain[2] = adc->analogRead(A1, ADC_0);
  ain[3] = adc->analogRead(A3, ADC_1);
  ain[4] = adc->analogRead(A6, ADC_0);
  ain[5] = adc->analogRead(A15, ADC_1);

  // low pass filter
  ain_buff[0] = lp_filter(ain[0], u_a, y_a);
  ain_buff[1] = lp_filter(ain[1], u_b, y_b);
  ain_buff[2] = lp_filter(ain[2], u_c, y_c);
  ain_buff[3] = lp_filter(ain[3], u_d, y_d);
  ain_buff[4] = lp_filter(ain[4], u_e, y_e);
  ain_buff[5] = lp_filter(ain[5], u_f, y_f);

  // make the light blink at roughly 1 Hz
  if(j > 1000){
    digitalWrite(13,LOW);
  }
  else{
    digitalWrite(13,HIGH);
  }
  if(j > 2000){
    j = 0;
  }
  
  // increment counter for LED
  j++;

  // get the frame end time
  end_time = micros();
  
  // delay to maintain frame rate
  smartDelay(frame_time - (end_time - start_time));
}

static void smartDelay(unsigned long us)
{
  unsigned long start = micros();
  do 
  {
    while (GPSSERIAL.available())
      gps.encode(GPSSERIAL.read());
  } while (micros() - start < us);
}

void requestEvent()
{
  // read air data
  Wire.requestFrom(2,2); // 2 bytes from address 0x02
  ps_byte[0] = Wire.read(); // put the data somewhere
  ps_byte[1] = Wire.read();

  Wire.requestFrom(3,2); // 2 bytes from address 0x03
  pd_byte[0] = Wire.read(); // put the data somewhere
  pd_byte[1] = Wire.read();
  
  // read PWM input
  Wire.requestFrom(0x79,8); // 8 bytes from address 0x79
  for(int i = 0; i<8; i++){
    pwm_byte[i] = Wire.read(); // put the data somewhere
  }
  
  // read GPS
  if(gps.time.isValid()){
    gpstime = gps.time.value();
  }
  if((gpstime - oldgpstime)){
    isupdated = 1;
    oldgpstime = gpstime;
    satVisible = gps.satellites.value();
    latitude.val = gps.location.lat();
    longitude.val = gps.location.lng();
    alt.val = gps.altitude.meters();
    gspeed.val = gps.speed.mps();
    track.val = gps.course.deg();
  }
  else{
    isupdated = 0;
  }
  
  // read GPIO, manual / auto mode
  if(digitalRead(modePin) == HIGH){
    modeVal = 1;
  }
  else{
    modeVal = 0;
  }

  /* Analog Data */
  pack[0]   = (byte) (ain_buff[0] & 0xff);
  pack[1]   = (byte) ((ain_buff[0] >> 8) & 0xff);
  pack[2]   = (byte) (ain_buff[1] & 0xff);
  pack[3]   = (byte) ((ain_buff[1] >> 8) & 0xff);
  pack[4]   = (byte) (ain_buff[2] & 0xff);
  pack[5]   = (byte) ((ain_buff[2] >> 8) & 0xff);
  pack[6]   = (byte) (ain_buff[3] & 0xff);
  pack[7]   = (byte) ((ain_buff[3] >> 8) & 0xff);
  pack[8]   = (byte) (ain_buff[4] & 0xff);
  pack[9]   = (byte) ((ain_buff[4] >> 8) & 0xff);
  pack[10]  = (byte) (ain_buff[5] & 0xff);
  pack[11]  = (byte) ((ain_buff[5] >> 8) & 0xff);

  /* Air Data */
  pack[12] = ps_byte[0];
  pack[13] = ps_byte[1];
  pack[14] = pd_byte[0];
  pack[15] = pd_byte[1];

  /* PWM Data */
  pack[16] = pwm_byte[0];
  pack[17] = pwm_byte[1];
  pack[18] = pwm_byte[2];
  pack[19] = pwm_byte[3];
  pack[20] = pwm_byte[4];
  pack[21] = pwm_byte[5];
  pack[22] = pwm_byte[6];
  pack[23] = pwm_byte[7];

  /* GPIO Data */
  pack[24] = modeVal;

  /* GPS Data */
  pack[25] = isupdated;
  pack[26] = satVisible;
  pack[27] = latitude.b[0];
  pack[28] = latitude.b[1];
  pack[29] = latitude.b[2];
  pack[30] = latitude.b[3];
  pack[31] = latitude.b[4];
  pack[32] = latitude.b[5];
  pack[33] = latitude.b[6];
  pack[34] = latitude.b[7];
  pack[35] = longitude.b[0];
  pack[36] = longitude.b[1];
  pack[37] = longitude.b[2];
  pack[38] = longitude.b[3];
  pack[39] = longitude.b[4];
  pack[40] = longitude.b[5];
  pack[41] = longitude.b[6];
  pack[42] = longitude.b[7];
  pack[43] = alt.b[0];
  pack[44] = alt.b[1];
  pack[45] = alt.b[2];
  pack[46] = alt.b[3];
  pack[47] = alt.b[4];
  pack[48] = alt.b[5];
  pack[49] = alt.b[6];
  pack[50] = alt.b[7];
  pack[51] = track.b[0];
  pack[52] = track.b[1];
  pack[53] = track.b[2];
  pack[54] = track.b[3];
  pack[55] = gspeed.b[0];
  pack[56] = gspeed.b[1];
  pack[57] = gspeed.b[2];
  pack[58] = gspeed.b[3];

  Wire1.write(pack, 59); 
}

void receiveEvent(size_t howMany)
{
  // read bytes off the i2c bus
  for(int k = 0; k<18; k++){
    unpack[k] = Wire1.read();
  }

  //assign bytes to microsecond commands
  pwmcmd[0] = ((unsigned int)unpack[1] << 8) + unpack[0];
  pwmcmd[1] = ((unsigned int)unpack[3] << 8) + unpack[2];
  pwmcmd[2] = ((unsigned int)unpack[5] << 8) + unpack[4];
  pwmcmd[3] = ((unsigned int)unpack[7] << 8) + unpack[6];
  pwmcmd[4] = ((unsigned int)unpack[9] << 8) + unpack[8];
  pwmcmd[5] = ((unsigned int)unpack[11] << 8) + unpack[10];
  pwmcmd[6] = ((unsigned int)unpack[13] << 8) + unpack[12];
  pwmcmd[7] = ((unsigned int)unpack[15] << 8) + unpack[14];
  pwmcmd[8] = ((unsigned int)unpack[17] << 8) + unpack[16];

  // write microsecond commands to pwm channels
  tinyservo_a.writeMicroseconds(pwmcmd[0]);
  tinyservo_b.writeMicroseconds(pwmcmd[1]);
  tinyservo_c.writeMicroseconds(pwmcmd[2]);
  tinyservo_d.writeMicroseconds(pwmcmd[3]);
  tinyservo_e.writeMicroseconds(pwmcmd[4]);
  tinyservo_f.writeMicroseconds(pwmcmd[5]);
  tinyservo_g.writeMicroseconds(pwmcmd[6]);
  tinyservo_h.writeMicroseconds(pwmcmd[7]);
  tinyservo_i.writeMicroseconds(pwmcmd[8]);
}

double lp_filter(double signal, double *u, double *y)
{
  const int m=1;  //m = order of denominator of low pass filter

  u[m] = signal;

  y[m] = 0.7304*y[m-1] + 0.2696*u[m-1]; // 200 Hz filter at 1000 Hz

  u[m-1] = u[m];    // initialize past values for next frame
  y[m-1] = y[m];

  return y[m];
}

