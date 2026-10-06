#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

//servo test: moves one PCA9685 channel at a time and prints the I2C status

const uint8_t PCA9685_ADDR = 0x40;

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADDR);

//pulse length in microseconds for 0 and 180 degrees, same as the Arduino Servo library
const int SERVO_MIN_US = 544;
const int SERVO_MAX_US = 2400;
const int SERVO_FREQ = 50;

//channels to test, 0-11 are the legs, 12-15 are spare
const int FIRST_CHANNEL = 0;
const int LAST_CHANNEL = 15;

void channel_write(int channel, int angle)
{
  angle = constrain(angle, 0, 180);
  int us = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  pwm.writeMicroseconds(channel, us);
}

//true if the PCA9685 answers on the I2C bus
bool pca_present()
{
  Wire.beginTransmission(PCA9685_ADDR);
  return Wire.endTransmission() == 0;
}

//read one register of the PCA9685, -1 if the read failed
int pca_read(uint8_t reg)
{
  Wire.beginTransmission(PCA9685_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0)
    return -1;
  if (Wire.requestFrom(PCA9685_ADDR, (uint8_t)1) != 1)
    return -1;
  return Wire.read();
}

void print_status()
{
  int mode1 = pca_read(0x00);
  int prescale = pca_read(0xFE);
  Serial.printf("PCA9685: %s, MODE1 = ", pca_present() ? "answering" : "NOT ANSWERING");
  if (mode1 < 0)
    Serial.print("read failed");
  else
    Serial.printf("0x%02X (%s)", mode1, (mode1 & 0x10) ? "ASLEEP - outputs off" : "awake");
  Serial.printf(", prescale = %d (expected about 131)\n", prescale);
}

//print the pulse stored in the board for every channel (-1 = read failed)
void print_all_channels()
{
  Serial.print("  stored pulses:");
  for (int channel = 0; channel < 16; channel++)
  {
    int off_l = pca_read(0x08 + 4 * channel);
    int off_h = pca_read(0x09 + 4 * channel);
    Serial.printf(" %d", (off_l < 0 || off_h < 0) ? -1 : (off_l | (off_h << 8)));
  }
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== servo test ===");

  //initialize the PCA9685 (SDA = 21, SCL = 22)
  Wire.begin();
  //slow bus: the link to the PCA9685 is unreliable at the default 100 kHz
  Wire.setClock(10000);
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);
  delay(10);
  print_status();
}

//channel being tested, -1 = all channels together
int selected = 0;
String input = "";

//switch one channel's output off, the servo goes limp
void channel_off(int channel)
{
  pwm.setPWM(channel, 0, 4096);
}

//move the selected channel(s) to an angle
void selected_write(int angle)
{
  for (int channel = FIRST_CHANNEL; channel <= LAST_CHANNEL; channel++)
  {
    if (selected < 0 || channel == selected)
      channel_write(channel, angle);
  }
}

void select_channel(int channel)
{
  selected = channel;
  //everything off, then only the selected channel(s) get driven
  for (int i = FIRST_CHANNEL; i <= LAST_CHANNEL; i++)
    channel_off(i);
  if (selected < 0)
    Serial.println("Testing ALL channels together");
  else
    Serial.printf("Testing channel %d\n", selected);
}

void print_help()
{
  Serial.println("Type a channel number (0-15) and press Enter to test that channel");
  Serial.println("  n = next channel, p = previous channel, a = all channels together");
}

//read commands typed in the serial monitor
void read_commands()
{
  while (Serial.available())
  {
    char c = Serial.read();
    if (c == 'n')
      select_channel(selected < LAST_CHANNEL ? selected + 1 : FIRST_CHANNEL);
    else if (c == 'p')
      select_channel(selected > FIRST_CHANNEL ? selected - 1 : LAST_CHANNEL);
    else if (c == 'a')
      select_channel(-1);
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
}

//wait while still reacting to typed commands, true if the selection changed
bool wait_and_read(unsigned long ms)
{
  int before = selected;
  unsigned long start = millis();
  while (millis() - start < ms)
  {
    read_commands();
    if (selected != before)
      return true;
    delay(10);
  }
  return false;
}

void loop(void)
{
  static bool started = false;
  if (!started)
  {
    started = true;
    print_help();
    select_channel(selected);
  }

  //the selected channel swings between 60 and 120 degrees
  selected_write(60);
  if (wait_and_read(1000))
    return;
  selected_write(120);
  wait_and_read(1000);
}
