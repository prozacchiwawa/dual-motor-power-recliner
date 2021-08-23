/*
   A controller for a dual motor recliner. (C) art yerkes

   This is GPL software.

   This is a program for an arduino UNO and the official quad relay shield, A000110.
   I make no representation as to the safety or reliability of using this program
   to control a dual motor power recliner.  Please use official hardware only unless
   experimenting and aware of any potential risks.

   The recliner has switches for up and down on a controller that connects via Din5
   connector as well as a left and right limit switch which I've connected with spade
   connector terminals, since the switches themselves use spade connectors).

   Our goal is a minimal program that keeps the motors from running too far out of
   balance without taking actions that are themselves potentially dangerous to the
   chair mechanism or the motors.

   I therefore run the motors only when a switch is pressed, and only run a motor in
   the IN direction (reducing the chair's height slightly) to be maximally conservative
   if the mechanism spends 1/5 of a second trying to correct a limit switch and the
   limit switch is still on, it enters a safe fault mode.

   If at any time both limit switches are pressed, it also enters fault mode.

     DIAGRAM FOR ONE MOTOR (NC == Normally Connected, NO == Normally Open)

                             ____
   +24v -----------+---- NO | R1 |                STOP (GND,GND) -> R1(0), R2(0)
                   |        |    | COM --------   FWD  (+24,GND) -> R1(1), R2(0)
   GND  --------+------- NC |____|                REV  (GND,+24) -> R1(0), R1(1)
                |  |
                |  |         ____
                |  +---- NO | R2 |
                |           |    | COM --------
                +------- NC |____|

   Din Plug Wiring

         Up  3
 Down  2     |     4------- Green (unused)
COM 1  |     |        5 --- Blue  (unused)
    |  |     + Yellow
    |  + Orange
    Red

   Up switch - Red to Yellow
   Down switch - Red to Orange

   Device terminals   Internal Wire  (except USB)

   DIG_GND            GND
   CONTROLLER_Pin1(R) GND
   CONTROLLER_Pin2(O) DIG_3
   CONTROLLER_Pin3(Y) DIG_2
   Left switch spade  DIG_9
   Right switch spade DIG_10

   24v                R1_NO R2_NO R3_NO R4_NO
   24GND              R1_NC R2_NC R3_NC R4_NC
   LeftIn             R1_COM
   LeftOut            R2_COM
   RightIn            R3_COM
   RightOut           R4_COM


   The chair has two limit switches on a rocker:


           Left Motor                   Right Motor
             || ||                        || ||
             ||=||                        ||=||
             || ||                        || ||
               H                            H
               H                            H
            +----------------------------------+
            |  O             O              O  |
            +----------------------------------+
          ----o                            o----
      ___/____                              ____\___
      Left Switch                           Right Switch

      So that if the motors are desynchronized too far, then one switch
      will be pressed, letting the controller know which direction to
      correct the imbalance in.

   It winds up looking like this:
                    
                                  Left limit ---+      +--- Down switch (Din5, pin 2)
                                                |      |
                                 Right limit --+|      |+-- Up switch (Din5, pin 3)
                                               ||      ||
           Din5, pin 1 ---+                    ||      ||
                          |                    ||      ||
Return from right limit --+--------- GND --+   ||      ||
                          |                |   ||      ||
Return from Left limit ---+                |   ||      ||
                                           GDCBA9X876543210

                +----------------------------+
                |                            |
            +---+                            +----+------------- +24V
            |   |                            |    |
            |  NO    R1               R4    NO    |
  Motor GND-|--NC                           NC----|-- Motor GND
Left--------|--COM                          COM---|--------------Right
Motor       |                                     |              Motor
Red         +--NO    R2               R3    NO----+              Blue
  Motor GND----NC                           NC------- Motor GND
Left-----------COM                          COM------------------Right
Motor                                                            Motor
Blue                                                             Red



*/

#define DEBUG 0

#define motorLeftIn 4
#define motorLeftOut 7
#define motorRightIn 8
#define motorRightOut 12

#define upSwitchPin 2
#define downSwitchPin 3

#define leftSwitchPin 9
#define rightSwitchPin 10

#define NO_MOTOR 0
#define LEFT_MOTOR 1
#define RIGHT_MOTOR 2
#define BOTH_MOTOR 3

#define RUN_OFF 0
#define RUN_IN 1
#define RUN_OUT 2

bool fault = false;

int fix_timer;
int fix_direction;

bool left_switch, right_switch;
bool up_switch, down_switch, prev_up, prev_down;

bool next_left, next_right;
int delay_next = 10;

void hw_setup() {
  Serial.begin(9600);       // setup Serial Monitor to display information
  pinMode(motorLeftIn, OUTPUT);  // connected to Relay 1
  pinMode(motorLeftOut, OUTPUT);  // connected to Relay 2
  pinMode(motorRightIn, OUTPUT);  // connected to Relay 3
  pinMode(motorRightOut, OUTPUT);  // connected to Relay 4

  pinMode(upSwitchPin, INPUT_PULLUP);
  pinMode(downSwitchPin, INPUT_PULLUP);
  pinMode(leftSwitchPin, INPUT_PULLUP);
  pinMode(rightSwitchPin, INPUT_PULLUP);
}

void set_motor_state(int motors, int dir) {
  if (motors & LEFT_MOTOR) {
    digitalWrite(motorLeftIn, dir == RUN_IN);
    digitalWrite(motorLeftOut, dir == RUN_OUT);
  } else {
    digitalWrite(motorLeftIn, 0);
    digitalWrite(motorLeftOut, 0);
  }

  if (motors & RIGHT_MOTOR) {
    digitalWrite(motorRightIn, dir == RUN_IN);
    digitalWrite(motorRightOut, dir == RUN_OUT);
  } else {
    digitalWrite(motorRightIn, 0);
    digitalWrite(motorRightOut, 0);
  }
}

void detect_switches() {
  up_switch = !digitalRead(upSwitchPin);
  down_switch = !digitalRead(downSwitchPin);
  left_switch = !digitalRead(leftSwitchPin);
  right_switch = !digitalRead(rightSwitchPin);
}

void setup() {
  hw_setup();
  detect_switches();
  set_motor_state(BOTH_MOTOR, RUN_OFF);
}

void loop() {
  detect_switches();
  if (left_switch && right_switch) {
    fault = true;
  }

  if (up_switch && down_switch) {
    fault = true;
  }

  if (fault) {
    Serial.print("fault!\n");
    delay(1000);
    return;
  }

  if (delay_next) {
    delay_next--;
    set_motor_state(BOTH_MOTOR, RUN_OFF);
    delay(1);
    return;
  }

  if (fix_timer && fix_direction) {
    if (!up_switch && !down_switch) {
      set_motor_state(BOTH_MOTOR, RUN_OFF);
      return;
    }
    fix_timer--;
    if (!fix_timer) {
      fix_timer = 0;
      fix_direction = NO_MOTOR;
      set_motor_state(BOTH_MOTOR, RUN_OFF);
      if (left_switch || right_switch) {
        fault = true;
      }
    } else {
      set_motor_state(fix_direction, RUN_IN);
    }
    delay(1);
    return;
  }

  if (left_switch) {
    delay_next = 10;
    fix_timer = 750;
    fix_direction = LEFT_MOTOR;
    Serial.println("fix left");
    return;
  }

  if (right_switch) {
    delay_next = 10;
    fix_timer = 750;
    fix_direction = RIGHT_MOTOR;
    Serial.println("fix right");
    return;
  }

  if (up_switch != prev_up || down_switch != prev_down) {
    prev_up = up_switch;
    prev_down = down_switch;
    delay_next = 10;
    return;
  }

  if (down_switch) {
    set_motor_state(BOTH_MOTOR, RUN_IN);
    delay(1);
    return;
  }
  if (up_switch) {
    set_motor_state(BOTH_MOTOR, RUN_OUT);
    delay(1);
    return;
  }
}
