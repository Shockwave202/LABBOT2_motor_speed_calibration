
#include <SoftwareSerial.h>
#include <Servo.h>
#include <EEPROM.h>

#include "Board.h"
#include "PushButton_Simple.h"
#include "PushButton.h" //Use timer2
#include "Joystick.h"
#include "Motor_Regulator.h"
#include "pitches.h"

//Create objs.
Board LABBOT_Board;
//Input:
PushButton_Simple startButton;
PushButton pushButton1, pushButton2;
Joystick joystick;
//Output:
Motor_Regulator motor1, motor2;
// Create a Software serial object.
SoftwareSerial mySerial(9, 4); // RX, TX
Servo servo1;                  // create servo object to control a servo
// twelve servo objects can be created on most boards

//RF_module
int rfReading;
int rfDataCode;

//Beep
int PROMPT_TONE1_TAB[] = {
    //This table consists of the freqs to be displayed
    NOTE_E6, // 1318HZ,treble	3
    NOTE_A6, // 1760HZ,treble	6
    NOTE_G6, // 1568HZ,treble	5
};
int PROMPT_TONE1_DURATION_TAB[] = {8, 8, 8};

int PROMPT_TONE2_TAB[] = {
    //This table consists of the freqs to be displayed
    NOTE_A6, // 1760HZ,treble 6
    NOTE_E6, // 1568HZ,treble 5
    NOTE_E6, // 1397HZ,treble 4
    NOTE_E6, // 1318HZ,treble 3
    NOTE_A6, // 1760HZ,treble 6
};
int PROMPT_TONE2_DURATION_TAB[] = {8, 8, 8, 8, 8};

int thisNote;
int noteDuration;
int pauseBetweenNotes; // noteDuration * 1.30;
unsigned long durationCount;
bool beep_on;
bool enterSettingModePrompt;
bool exitSettingModePrompt;

//Motor
#define SPEED_CORRECTION 5
int motor1_speed;
int motor2_speed;
int motor1_forward_speed;
int motor1_backward_speed;
int motor2_forward_speed;
int motor2_backward_speed;
#define FORWARD 0
#define BACKWARD 1
int mode;
bool startMotor;

//EEPROM
byte checkingByte1, checkingByte2;

#define RUNNING_MODE 0
#define SETTING_MODE 1
int pgm_mode;
bool mode_switching;

void setup()
{
  // put your setup code here, to run once:
  //The startup time is approximate to 1.6S.
  LABBOT_Board.initial_all_ioports();
  //Serial.begin(9600);//For program test.

  /*----Common modules applied to all the subprograms.---*/
  //Input:
  startButton.create(START);

  //Output:
  //LED:
  pinMode(D1_BOARD, OUTPUT); //Attach LED1 to D1 pin
  digitalWrite(D1_BOARD, LOW);
  pinMode(D2_BOARD, OUTPUT); //Attach LED2 to D2 pin
  digitalWrite(D2_BOARD, LOW);
  //BEEP
  //Motor
  motor1.attach(MOTOR1_INA, MOTOR1_INB); //Attach INA to the PWM output port
  motor2.attach(MOTOR2_INA, MOTOR2_INB); //Attach INB to the general ioport for direction ctrl

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);

  //The first 2 bytes are used to check whether the EEPROM has been modified yet,
  //If the checking bytes don't match with the given values,which means the EEPROM hasn't
  //been modified,the servo associated variables should be assigned to the default values.
  //otherwise,read in the values from EEPROM.
  EEPROM.get(0, checkingByte1);
  EEPROM.get(1, checkingByte2);

  if (checkingByte1 != 0xaa || checkingByte2 != 0xaa)
  {

    //Assign the default value.
    motor1_forward_speed = 120;
    motor1_backward_speed = 120;
    motor2_forward_speed = 120;
    motor2_backward_speed = 120;

    //Update EEPROM
    EEPROM.put(0, 0xaa);
    EEPROM.put(1, 0xaa);
    EEPROM.put(2, motor1_forward_speed);
    EEPROM.put(4, motor1_backward_speed);
    EEPROM.put(6, motor2_forward_speed);
    EEPROM.put(8, motor2_backward_speed);
  }
  else
  { //checkingByte1=0x55 && checkingByte2==0xaa
    //The EEPROM has already been written,read in the values from the EEPROM
    Serial.println(EEPROM.get(2, motor1_forward_speed));
    Serial.println(EEPROM.get(4, motor1_backward_speed));
    Serial.println(EEPROM.get(6, motor2_forward_speed));
    Serial.println(EEPROM.get(8, motor2_backward_speed));
  }

  pgm_mode = RUNNING_MODE;
  mode_switching = 1;
}

void loop()
{                          // run over and over
  startButton.detection(); //Check if the button is pressed.
  motor1.regulation();
  motor2.regulation();

  if (beep_on)
  {
    if (millis() - durationCount > 200)
    {
      beep_on = 0;
      // stop the tone playing:
      noTone(BEEP);
    }
  }
  /*--------------RUNNING_MODE-------------------*/
  if (pgm_mode == RUNNING_MODE)
  {
    //Entrance:
    if (mode_switching)
    {
      mode_switching = 0;

      //Set the default state of the motor.
      //Forward:
      motor1.set(COUNTER_CLKWISE, 0);
      motor2.set(CLKWISE, 0);
    }

    if (startButton.isPress())
    {
      //do nothing
    }

    if (enterSettingModePrompt)
    {

      if (millis() - durationCount > pauseBetweenNotes)
      {
        durationCount = millis(); //reset the timer.

        if (thisNote < 2) //thisNote < NOTES_NBR-1
        {
          //the current note has asserted for long enough,
          //point to the next note.
          thisNote++;
          // to calculate the note duration, take one second divided by the note type.
          //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
          noteDuration = 1000 / PROMPT_TONE1_DURATION_TAB[thisNote];
          pauseBetweenNotes = noteDuration * 1.30;
          tone(BEEP, PROMPT_TONE1_TAB[thisNote], noteDuration);
        }
        else //The duration time of the last note has elapsed,
             //shutdown the tone and exit.
        {
          enterSettingModePrompt = 0;
          // stop the tone playing:
          noTone(BEEP);

          //After prompt,switch to setting mode
          pgm_mode = SETTING_MODE;
          mode_switching = 1;
        }
      }
    }

    //The processor receives the command from the RF_module,
    //and take the corresponding action.
    if (mySerial.available())
    {
      //Serial.println(mySerial.read());
      //Read the data from the RF module.
      rfReading = mySerial.read();

      //If the reading has changed,refresh the dataCode.
      if (rfReading != rfDataCode)
      {
        rfDataCode = rfReading;

        Serial.println(rfDataCode);

        if (rfDataCode == 15) //endCode.
        {
          //Motor stop:
          motor1.set(CLKWISE, 0);
          motor2.set(CLKWISE, 0);
          digitalWrite(D1_BOARD, LOW);
          digitalWrite(D2_BOARD, LOW);
        }
        else
        {
          //Prompt the user that the remote ctrller button
          //has been pressed.
          if (!beep_on)
          {
            beep_on = 1;
            durationCount = millis(); //reset the timer.
            tone(BEEP, NOTE_A6, 200);
            switch (rfDataCode) //Execute the command.
            {
            case 5: //KEY1
                    //Forward:
              motor1.set(COUNTER_CLKWISE, motor1_forward_speed);
              motor2.set(CLKWISE, motor2_forward_speed);
              digitalWrite(D1_BOARD, HIGH);
              digitalWrite(D2_BOARD, HIGH);
              break;
            case 7: //KEY2
                    //Backward:
              motor1.set(CLKWISE, motor1_backward_speed);
              motor2.set(COUNTER_CLKWISE, motor2_backward_speed);
              digitalWrite(D1_BOARD, HIGH);
              digitalWrite(D2_BOARD, HIGH);
              break;
            case 8: //KEY3
                    //Turn left
              motor1_speed = 200;
              motor2_speed = 200;
              motor1.set(CLKWISE, motor1_speed);
              motor2.set(CLKWISE, motor2_speed);
              digitalWrite(D1_BOARD, HIGH);
              digitalWrite(D2_BOARD, LOW);
              break;
            case 6: //KEY4
                    //Turn right
              motor1_speed = 200;
              motor2_speed = 200;
              motor1.set(COUNTER_CLKWISE, motor1_speed);
              motor2.set(COUNTER_CLKWISE, motor2_speed);
              digitalWrite(D1_BOARD, LOW);
              digitalWrite(D2_BOARD, HIGH);
              break;
            case 1: //KEY5
              break;
            case 3: //KEY6
              break;
            case 4: //KEY7
              break;
            case 2: //KEY8
              break;
            case 10: //KEY_A
              break;
            case 9: //KEY_B
              break;
            case 11: //KEY_C
              break;
            case 12: //KEY_D
              break;
            case 69: //KEY1+KEY7
              break;
            case 37: //KEY1+KEY8
              break;
            case 71: //KEY2+KEY7
              break;
            case 39: //KEY2+KEY8
              break;
            case 72: //KEY3+KEY7
              break;
            case 40: //KEY3+KEY8
              break;
            case 70: //KEY4+KEY7
              break;
            case 38: //KEY4+KEY8
              break;
            case 21: //KEY1+KEY5
              break;
            case 53: //KEY1+KEY6
              break;
            case 23: //KEY2+KEY5
              break;
            case 55: //KEY2+KEY6
              break;
            case 24: //KEY3+KEY5
              break;
            case 56: //KEY3+KEY6
              break;
            case 22: //KEY4+KEY5
              break;
            case 54: //KEY4+KEY6
              break;
            case 90: //KEY_A+KEY1
              //Prompt the user that the program is switching to the setting mode.
              if (!enterSettingModePrompt)
              {
                enterSettingModePrompt = 1;
                //Point to the first note.
                thisNote = 0;
                // to calculate the note duration, take one second divided by the note type.
                //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
                noteDuration = 1000 / PROMPT_TONE1_DURATION_TAB[thisNote];
                pauseBetweenNotes = noteDuration * 1.30;
                durationCount = millis(); //reset the timer.
                tone(BEEP, PROMPT_TONE1_TAB[thisNote], noteDuration);
              }
              //Set the setting mode:
              mode = FORWARD;
              break;
            case 122: //KEY_A+KEY2
              //Prompt the user that the program is switching to the setting mode.
              if (!enterSettingModePrompt)
              {
                enterSettingModePrompt = 1;
                //Point to the first note.
                thisNote = 0;
                // to calculate the note duration, take one second divided by the note type.
                //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
                noteDuration = 1000 / PROMPT_TONE1_DURATION_TAB[thisNote];
                pauseBetweenNotes = noteDuration * 1.30;
                durationCount = millis(); //reset the timer.
                tone(BEEP, PROMPT_TONE1_TAB[thisNote], noteDuration);
              }
              //Set the setting mode:
              mode = BACKWARD;
              break;
            }
          }
        }
      }
    }
  }
  /*--------------SETTING_MODE-----------------*/
  else if (pgm_mode == SETTING_MODE)
  {
    //Entrance:
    if (mode_switching)
    {
      mode_switching = 0;
      digitalWrite(D1_BOARD, HIGH);
      digitalWrite(D2_BOARD, HIGH);
    }

    if (startButton.isPress())
    {
      //do nothing.
    }

    //The processor receives the command from the RF_module,
    //and take the corresponding action.
    if (mySerial.available())
    {
      //Serial.println(mySerial.read());
      //Read the data from the RF module.
      rfReading = mySerial.read();

      //If the reading has changed,refresh the dataCode.
      if (rfReading != rfDataCode)
      {
        rfDataCode = rfReading;

        if (rfDataCode == 15) //endCode
        {
          //do nothing
        }
        else
        {
          //Prompt the user that the remote ctrller button
          //has been pressed.
          if (!beep_on)
          {
            beep_on = 1;
            durationCount = millis(); //reset the timer.
            tone(BEEP, NOTE_A6, 200);
            switch (rfDataCode) //Execute the command.
            {
            case 5: //KEY1
              if (startMotor)
              {
                if (mode == FORWARD)
                {
                  //Speed correction
                  if (motor1_forward_speed < 255)
                    motor1_forward_speed += SPEED_CORRECTION;
                  else
                    motor1_forward_speed = 255;
                  //Speed limitation
                  if (motor1_forward_speed > 255)
                    motor1_forward_speed = 255;
                  //Check
                  Serial.println(motor1_forward_speed);
                  motor1.set(COUNTER_CLKWISE, motor1_forward_speed);
                }
                else
                { //mode=BACKWARD
                  //Speed correction
                  if (motor1_backward_speed < 255)
                    motor1_backward_speed += SPEED_CORRECTION;
                  else
                    motor1_backward_speed = 255;
                  //Speed limitation
                  if (motor1_backward_speed > 255)
                    motor1_backward_speed = 255;
                  //Check
                  Serial.println(motor1_backward_speed);
                  motor1.set(CLKWISE, motor1_backward_speed);
                }
              }
              break;
            case 7: //KEY2
              if (startMotor)
              {
                if (mode == FORWARD)
                {
                  //Speed correction
                  if (motor1_forward_speed > 0)
                    motor1_forward_speed -= SPEED_CORRECTION;
                  else
                    motor1_forward_speed = 0;
                  //Speed limitation
                  if (motor1_forward_speed < 0)
                    motor1_forward_speed = 0;
                  //Check
                  Serial.println(motor1_forward_speed);
                  motor1.set(COUNTER_CLKWISE, motor1_forward_speed);
                }
                else
                { //mode=BACKWARD
                  //Speed correction
                  if (motor1_backward_speed > 0)
                    motor1_backward_speed -= SPEED_CORRECTION;
                  else
                    motor1_backward_speed = 0;
                  //Speed limitation
                  if (motor1_backward_speed < 0)
                    motor1_backward_speed = 0;
                  //Check
                  Serial.println(motor1_backward_speed);
                  motor1.set(CLKWISE, motor1_backward_speed);
                }
              }
              break;
            case 8: //KEY3
              break;
            case 6: //KEY4
              break;
            case 1: //KEY5
              if (startMotor)
              {
                if (mode == FORWARD)
                {
                  //Speed correction
                  if (motor2_forward_speed < 255)
                    motor2_forward_speed += SPEED_CORRECTION;
                  else
                    motor2_forward_speed = 255;
                  //Speed limitation
                  if (motor2_forward_speed > 255)
                    motor2_forward_speed = 255;
                  //Check
                  Serial.println(motor2_forward_speed);
                  motor2.set(CLKWISE, motor2_forward_speed);
                }
                else
                { //mode=BACKWARD
                  //Speed correction
                  if (motor2_backward_speed < 255)
                    motor2_backward_speed += SPEED_CORRECTION;
                  else
                    motor2_backward_speed = 255;
                  //Speed limitation
                  if (motor2_backward_speed > 255)
                    motor2_backward_speed = 255;
                  //Check
                  Serial.println(motor2_backward_speed);
                  motor2.set(COUNTER_CLKWISE, motor2_backward_speed);
                }
              }
              break;
            case 3: //KEY6
              if (startMotor)
              {
                if (mode == FORWARD)
                {
                  //Speed correction
                  if (motor2_forward_speed > 0)
                    motor2_forward_speed -= SPEED_CORRECTION;
                  else
                    motor2_forward_speed = 0;
                  //Speed limitation
                  if (motor2_forward_speed < 0)
                    motor2_forward_speed = 0;
                  //Check
                  Serial.println(motor2_forward_speed);
                  motor2.set(CLKWISE, motor2_forward_speed);
                }
                else
                { //mode=BACKWARD
                  //Speed correction
                  if (motor2_backward_speed > 0)
                    motor2_backward_speed -= SPEED_CORRECTION;
                  else
                    motor2_backward_speed = 0;
                  //Speed limitation
                  if (motor2_backward_speed < 0)
                    motor2_backward_speed = 0;
                  //Check
                  Serial.println(motor2_backward_speed);
                  motor2.set(COUNTER_CLKWISE, motor2_backward_speed);
                }
              }
              break;
            case 4: //KEY7
              break;
            case 2: //KEY8
              break;
            case 10: //KEY_A
              break;
            case 9: //KEY_B
              break;
            case 11: //KEY_C
              if (!startMotor)
              {
                startMotor = 1;
                if (mode == FORWARD)
                {
                  motor1.set(COUNTER_CLKWISE, motor1_forward_speed);
                  motor2.set(CLKWISE, motor2_forward_speed);
                }
                else //mode=BACKWARD;
                {
                  motor1.set(CLKWISE , motor1_backward_speed);
                  motor2.set(COUNTER_CLKWISE, motor2_backward_speed);
                }
              }
              break;
            case 12: //KEY_D
              //Stop the motor
              startMotor = 0;

              //Save
              if (mode == FORWARD)
              {
                EEPROM.put(2, motor1_forward_speed);
                EEPROM.put(6, motor2_forward_speed);
              }
              else //mode=BACKWARD
              {
                EEPROM.put(4, motor1_backward_speed);
                EEPROM.put(8, motor2_backward_speed);
              }

              //Prompt the user that the program is in the setting mode.
              if (!exitSettingModePrompt)
              {
                exitSettingModePrompt = 1;
                //Point to the first note.
                thisNote = 0;
                // to calculate the note duration, take one second divided by the note type.
                //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
                noteDuration = 1000 / PROMPT_TONE2_DURATION_TAB[thisNote];
                pauseBetweenNotes = noteDuration * 1.30;
                durationCount = millis(); //reset the timer.
                tone(BEEP, PROMPT_TONE2_TAB[thisNote], noteDuration);
              }
              break;
            case 69: //KEY1+KEY7
              break;
            case 37: //KEY1+KEY8
              break;
            case 71: //KEY2+KEY7
              break;
            case 39: //KEY2+KEY8
              break;
            case 72: //KEY3+KEY7
              break;
            case 40: //KEY3+KEY8
              break;
            case 70: //KEY4+KEY7
              break;
            case 38: //KEY4+KEY8
              break;
            case 21: //KEY1+KEY5
              break;
            case 53: //KEY1+KEY6
              break;
            case 23: //KEY2+KEY5
              break;
            case 55: //KEY2+KEY6
              break;
            case 24: //KEY3+KEY5
              break;
            case 56: //KEY3+KEY6
              break;
            case 22: //KEY4+KEY5
              break;
            case 54: //KEY4+KEY6
              break;
            }
          }
        }
      }
    }

    if (exitSettingModePrompt)
    {
      if (millis() - durationCount > pauseBetweenNotes)
      {
        durationCount = millis(); //reset the timer.

        if (thisNote < 4) //thisNote < NOTES_NBR-1
        {
          //the current note has asserted for long enough,
          //point to the next note.
          thisNote++;

          // to calculate the note duration, take one second divided by the note type.
          //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
          noteDuration = 1000 / PROMPT_TONE2_DURATION_TAB[thisNote];
          pauseBetweenNotes = noteDuration * 1.30;

          tone(BEEP, PROMPT_TONE2_TAB[thisNote], noteDuration);
        }
        else //The duration time of the last note has elapsed,
             //shutdown the tone and exit.
        {
          exitSettingModePrompt = 0;
          // stop the tone playing:
          noTone(BEEP);

          //Exit
          pgm_mode = RUNNING_MODE;
          mode_switching = 1;
          digitalWrite(D1_BOARD, LOW);
          digitalWrite(D2_BOARD, LOW);
        }
      }
    }
  }
}
