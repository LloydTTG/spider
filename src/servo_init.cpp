#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

//pulse length in microseconds for 0 and 180 degrees, same as the Arduino Servo library
const int SERVO_MIN_US = 544;
const int SERVO_MAX_US = 2400;
const int SERVO_FREQ = 50;

//channels with a servo: 1-11 and 12, channel 12 replaces the damaged channel 0
const int FIRST_CHANNEL = 1;
const int LAST_CHANNEL = 12;

//channel being held at 90 degrees, -1 = all channels
int selected = 12;
String input = "";

void channel_write(int channel, int angle)
{
  angle = constrain(angle, 0, 180);
  int us = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  //pulse in ticks of the 20ms period (4096 ticks)
  int ticks = (long)us * SERVO_FREQ * 4096 / 1000000;
  pwm.setPWM(channel, 0, ticks);
}

//switch one channel's output off, the servo goes limp
void channel_off(int channel)
{
  pwm.setPWM(channel, 0, 4096);
}

void print_status();

void select_channel(int channel)
{
  selected = channel;
  //set every servo once, the PCA9685 keeps the position by itself
  for (int i = FIRST_CHANNEL; i <= LAST_CHANNEL; i++)
  {
    if (selected < 0 || i == selected)
      channel_write(i, 90);
    else
      channel_off(i);
    delay(20);
  }
  if (selected < 0)
    Serial.println("All servos at 90 degrees");
  else
    Serial.printf("Channel %d at 90 degrees, other servos off\n", selected);
  print_status();
}

//read one register of the PCA9685, -1 if the read failed
int pca_read(uint8_t reg)
{
  Wire.beginTransmission(0x40);
  Wire.write(reg);
  if (Wire.endTransmission() != 0)
    return -1;
  if (Wire.requestFrom((uint8_t)0x40, (uint8_t)1) != 1)
    return -1;
  return Wire.read();
}

//print what the board has stored: 301 = 90 degrees, 4096 = off, -1 = read failed
void print_status()
{
  int mode1 = pca_read(0x00);
  Serial.print("PCA9685 MODE1 = ");
  if (mode1 < 0)
    Serial.print("read failed");
  else
    Serial.printf("0x%02X (%s)", mode1, (mode1 & 0x10) ? "ASLEEP - outputs off" : "awake");
  Serial.printf(", prescale = %d (expected about 131)\n", pca_read(0xFE));
  Serial.print("Stored pulses, channel 0-15:");
  for (int channel = 0; channel < 16; channel++)
  {
    int off_l = pca_read(0x08 + 4 * channel);
    int off_h = pca_read(0x09 + 4 * channel);
    Serial.printf(" %d", (off_l < 0 || off_h < 0) ? -1 : (off_l | (off_h << 8)));
  }
  Serial.println();
}

void print_help()
{
  Serial.printf("Type a channel number (%d-%d) and press Enter to set that servo to 90 degrees\n", FIRST_CHANNEL, LAST_CHANNEL);
  Serial.println("  n = next channel, p = previous channel, a = all servos, s = show board status");
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Servo initialization");
  print_help();

  //initialize the PCA9685 (SDA = 21, SCL = 22)
  Wire.begin();
  //slow bus: the link to the PCA9685 is unreliable at the default 100 kHz
  Wire.setClock(10000);
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);
  delay(10);
  //never drive the damaged channel 0
  channel_off(0);
  select_channel(selected);
}

void loop(void)
{
  //pick a servo from the serial monitor
  while (Serial.available())
  {
    char c = Serial.read();
    if (c == 'n')
      select_channel(selected < LAST_CHANNEL ? selected + 1 : FIRST_CHANNEL);
    else if (c == 'p')
      select_channel(selected > FIRST_CHANNEL ? selected - 1 : LAST_CHANNEL);
    else if (c == 'a')
      select_channel(-1);
    else if (c == 's')
      print_status();
    else if (c >= '0' && c <= '9')
    {
      input += c;
      Serial.print(c);
    }
    else if ((c == '\r' || c == '\n') && input.length() > 0)
    {
      Serial.println();
      int channel = input.toInt();
      input = "";
      if (channel >= FIRST_CHANNEL && channel <= LAST_CHANNEL)
        select_channel(channel);
      else
        print_help();
    }
  }
  delay(10);
}
