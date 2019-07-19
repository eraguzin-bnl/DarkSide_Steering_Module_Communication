//Eric Raguzin
//Brookhaven National Laboratory
//Control for the IO-1733-1 PCB serial chain for Darkside test with LTC6820 interface
//12/28/18
#include <Wire.h>
//Datasheets available here: 
//http://ww1.microchip.com/downloads/en/DeviceDoc/hv5523.pdf
//http://ww1.microchip.com/downloads/en/DeviceDoc/20005843A.pdf
//https://www.analog.com/media/en/technical-documentation/data-sheets/6820fb.pdf

//LTC6820 must be in the POL=0, PHA=1 positions.  Full explanation at bottom of file

//Adjust speed of SPI write (to compensate for any issues in cryo)/////////
int SPI_delay = 1;

//User parameters for long term cold test///////////////////////////////////
//Pins to cycle
int cycle_pins_HV[] = {1,41,42,43,44,45,46,47,48,49,50};
int cycle_pins_LV[] = {};

//Pause between cycling chips
unsigned int pause = 3000;

//Duration of the test - no longer than 50 days worth
unsigned long hours = 3;
////////////////////////////////////////////////////////////////////////////
//To translate to LTC6820 language, MOSI is D_IN and DATA, MISO is D_OUT and READBACK

#define D_IN  10  //Data out
#define D_OUT 4  //Readback coming in
#define LE    3
#define CLK   9

//I use a char array for the 64+32 bit stream instead of a 64 bit int or something because of this : https://forum.arduino.cc/index.php?topic=496719.0
//It's 12 arrays of 8 bits each, so it's a little more complicated to access, but is essentially the same.
//It's organized so that the first 4 arrays are for the low voltage amplifier bias, the last 8 are for the high voltage SiPM bias
//When printed, the arrays go from Channel 1 on the left to Channel 32 or 64 on the right
//This defaults to all outputs NOT allowing high voltage (the two chips require opposite polarities for that).
//All off
byte array_to_set[12] = {255,255,255,255,0,0,0,0,0,0,0,0};
//All on
//byte array_to_set[12] = {0,0,0,0,255,255,255,255,255,255,255,255};
byte array_readback[12] = {0};

const byte numChars = 32;
char receivedChars[numChars]; // an array to store the received data
boolean newData = false;

struct argument {
   byte pin;
   bool value;
};

void setup() {
  pinMode(D_IN, OUTPUT);
  pinMode(D_OUT, INPUT);
  pinMode(LE, OUTPUT);
  pinMode(CLK, OUTPUT);

  //Latch off
  //POL 0, PHA 0 requires CLK to idle low
  digitalWrite(LE, HIGH);
  digitalWrite(CLK, LOW);
  digitalWrite(D_IN, LOW);

  //Writes the initial array to the chips so that all outputs are off.  It has a habit of starting up in a state that allows HV through.
  write_chip(false);
  
  Serial.begin (9600);
  delay(100); //It helps with the weird characters you get at the beginning
  Serial.println(F("Brookhaven National Laboratory HV5523/HV3418 Chip Tester"));
  Serial.print(F("The available commands are:\n'lv': This sets one channel of the low voltage amplifier bias array of 32 bits, but does not yet write it to the chip.  For example, to prepare pin 15 to be 1, "));
  Serial.print(F("use the syntax 'lv 15 1'.  To prepare pin 31 to be 0, use 'lv 31 0'\n'hv': This sets one channel of the high voltage SiPM bias array of 64 bits, but does not yet write it to the chip.  "));
  Serial.print(F("For example, to prepare pin 52 to be 1, use the syntax 'hv 52 1'.  To prepare pin 40 to be 0, use 'hv 40 0'\n'write': This will write the prepared array to the HV3418 chip.  At the end of the "));
  Serial.print(F("write sequence, you will receive either a 'SPI Write Successful' or an error message\n'test': This will ask for parameters and start the long term switching test in liquid nitrogen.\n\n"));
  Serial.print(F("NOTE: With the low voltage chip, a 0 means that channel will be at the control voltage, and a 1 will mean that channel will be at ground potential.\n"));
  Serial.print(F("NOTE: With the high voltage chip, a 0 means that channel is at ground potential and a 1 means that channel will be at the high voltage.\n"));
  Serial.print(F("NOTE: When each array is printed, they display from Channel 0 on the left to Channel 32 or 64 on the right\n\n"));

  Serial.println("Initial settings:");
  Serial.print(F("Low Voltage Amplifier Bias Array (hex) is: "));
  for (int i = 0; i < 12; i++){
    if (i == 4){
      Serial.print("\nHigh Voltage SiPM Bias Array (hex) is: ");
    }
    Serial.print(array_to_set[i] >> 4, HEX);
    Serial.print(array_to_set[i] & 0xF, HEX);
  }
  Serial.print("\n\n");
}

//The main loop basically checks for new Serial data, and outputs a flag if it receives a new line.  Then that flag tells the program to parse it.  This seems to be the most reliable way to recieve strings of variable length
void loop() {
  recvWithEndMarker();
  if (newData == true) {
    newData = false;
    parse_input();
  }
}

//As long as there's data in the Serial buffer, it reads it one char at a time, and fills in the char array.  Once it sees the end marker, it finishes up and gives a null termination.
//The Arduino terminal needs to be told to give that end marker whenever you send a command.
void recvWithEndMarker() {
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;
  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();
    
    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    }
    else {
      receivedChars[ndx] = '\0'; // terminate the string
      ndx = 0;
      newData = true;
    }
  }
}

void parse_input(){
  struct argument b;
  
  char compare[15];
  int ret_hv;
  int ret_lv;
  int ret_write;
  int ret_test;
  int ret_on;
  int ret_off;

  const char s[2] = " ";
  char *command_token;
  char *reg_token;
  char *val_token;
   
  //get the token separated by each space character
  command_token = strtok(receivedChars, s);
  reg_token = strtok(NULL, s);
  val_token = strtok(NULL, s);

  //I think this is the simplest way to compare a string token and see if it matches any preset strings

  strcpy(compare, "hv");
  ret_hv = strcmp(compare, command_token);

  strcpy(compare, "lv");
  ret_lv = strcmp(compare, command_token);

  strcpy(compare, "write");
  ret_write = strcmp(compare, command_token);

  strcpy(compare, "test");
  ret_test = strcmp(compare, command_token);

  strcpy(compare, "on");
  ret_on = strcmp(compare, command_token);

  strcpy(compare, "off");
  ret_off = strcmp(compare, command_token);

  if(ret_hv == 0){
    if(reg_token){
      int reg = atoi(reg_token);
      if (reg < 1 || reg > 64) {
        Serial.println(F("ERROR: You typed 'hv' but the register is out of range.  For high voltage it should be between 1 and 64.  You typed:"));
        Serial.println(reg);
      }
      else if((atoi(val_token) == 0) or (atoi(val_token) == 1)){
        int val = atoi(val_token);
        //Because we want to subtract it by 1 to zero index, but also add 32 to offset it to the right part of the array
        b.pin = (reg+31);
        b.value = val;
        set_array(b, true);
      }
      else{
        Serial.println(F("ERROR: You typed 'hv' but no value was found.  The syntax needs to be 'set reg# value#'.  value# is a 1 or 0, you typed:"));
        Serial.print(command_token);
        Serial.print(" ");
        Serial.print(reg_token);
        Serial.print(" ");
        Serial.print(val_token);
      }
    }
    else{
      Serial.println(F("ERROR: You typed 'hv' but no register number was found.  The syntax needs to be 'hv reg# value#'."));
    }
  }
  else if(ret_lv == 0){
    if(reg_token){
      int reg = atoi(reg_token);
      if (reg < 1 || reg > 32) {
        Serial.println(F("ERROR: You typed 'lv', but the register is out of range.  For low voltage it should be between 1 and 32.  You typed:"));
        Serial.println(reg);
      }
      else if((atoi(val_token) == 0) or (atoi(val_token) == 1)){
        int val = atoi(val_token);
        //To zero index
        b.pin = (reg-1);
        b.value = val;
        set_array(b, true);
      }
      else{
        Serial.println(F("ERROR: You typed 'lv' but no value was found.  The syntax needs to be 'set reg# value#'.  value# is a 1 or 0, you typed:"));
        Serial.print(command_token);
        Serial.print(" ");
        Serial.print(reg_token);
        Serial.print(" ");
        Serial.print(val_token);
      }
    }
    else{
      Serial.println(F("ERROR: You typed 'lv' but no register number was found.  The syntax needs to be 'hv reg# value#'."));
    }
  }
  else if (ret_write == 0){
    write_chip(true);
    }
  else if (ret_test == 0){
    //write_chip(true);
    bool result = switch_test();
    if (result == true){
      Serial.println(F("Test was successful!"));
    }
    else{
      Serial.println(F("Test was not successful"));
    }
  }
  else{
    Serial.print(F("Incorrect syntax, no function detected!  Use the 'hv','lv','write','test','on' or 'off' command with proper arguments.  You typed: "));
    Serial.println(receivedChars);
  }
  Serial.print("\n");
}

void set_array(argument b, bool to_print){
  if (b.pin < 96 and b.pin > -1 and (b.value == 0 or b.value == 1)){
    if (to_print == true){
      if (b.pin > 31){
        Serial.print(F("Changing HV pin "));
        //Bring it back to user notation
        Serial.print(b.pin-31);
        Serial.print(F(" to "));
        Serial.println(b.value);
      }
      else{
        Serial.print(F("Changing LV pin "));
        //Bring it back to user notation
        Serial.print(b.pin+1);
        Serial.print(F(" to "));
        Serial.println(b.value);
      }
    }

      byte which_byte = b.pin/8;
      byte which_bit = b.pin%8;
      
      bitWrite(array_to_set[which_byte], which_bit, b.value);

    if (to_print == true){
      Serial.print(F("Low Voltage Amplifier Bias Array (binary) is: "));
      for (int i = 0; i < 12; i++){
        if (i == 4){
          Serial.print("\nHigh Voltage SiPM Bias Array (binary) is: ");
        }
        for (int k = 0; k < 8; k++){
          Serial.print((array_to_set[i] >> k) & 1);
        }
      }
      Serial.print("\n");

      Serial.print(F("Low Voltage Amplifier Bias Array (hex) is: "));
      for (int i = 0; i < 12; i++){
        if (i == 4){
          Serial.print("\nHigh Voltage SiPM Bias Array (hex) is: ");
        }
        Serial.print(array_to_set[i] >> 4, HEX);
        Serial.print(array_to_set[i] & 0xF, HEX);
      }
      Serial.print("\n");
    }
  }
  else{
    Serial.print(F("ERROR: Trying to change pin "));
    //Bring it back to user notation
    Serial.print(b.pin+1);
    Serial.print(F(" to "));
    Serial.println(b.value);
  }
}

void write_chip(bool to_print){
  //Cycles through this twice.  The first time shifts the settings in the latches, and the second time, the previous settings are outputted, so you can read and compare
  //This cycles through the 96 bits in the correct order.  For each bit, it is "clocked" in, and the output bit is read.
  //The HV 5523 and HV 3418 latch in the data on different clock edges, so the transient analysis in complicated, especially with the LTC6820 between them, but this was found experimentally to work
  //Because of the register orientation, you want to put the last bit first
  //With the LTC6820, a change of LE will send that across to the slave module
  //With PHA = 0, POL = 0, a rising clock edge will sample D_IN and send that data to the slave module.  The slave module will adjust MOSI and then do a quick positive/negative clock pulse
  //and then sample the readback on MISO and send that back.
  //HOWEVER, the Master only adjusts its MISO (sampled as D_OUT here) AFTER the Arduino does the falling clock edge.  Thanks Linear for hiding that information in a figure.

  //Wake up the sleeping module
  digitalWrite(LE, LOW);
  digitalWrite(LE, HIGH);
  
  for (int m = 0; m < 2; m++){
    digitalWrite(LE, LOW);
    
    for (int i = 11; i > -1; i--){
      for (int k = 7; k > -1; k--){
        bool output = (array_to_set[i] >> k) & 1;
        digitalWrite(D_IN, output);
        delay(SPI_delay);
        
        digitalWrite(CLK, HIGH);
        delay(SPI_delay);
        
        digitalWrite(CLK, LOW);
        delay(SPI_delay);
        
        boolean input = digitalRead(D_OUT);
        bitWrite(array_readback[i],k,input);
        
        if ((i == 0) and (k == 0)){
          digitalWrite(LE, HIGH);
          delay(SPI_delay);
          digitalWrite(LE, LOW);
          delay(SPI_delay);
          digitalWrite(LE, HIGH);
          delay(SPI_delay);
          digitalWrite(D_IN, LOW);
        }
      }
    }
  }

  //Check to make sure the chip was written correctly
  bool received = true;

  for (int i = 0; i < 12; i++){
    if (array_readback[i] != array_to_set[i]){
      received = false;
      Serial.print(F("For subarray "));
      Serial.print(i);
      Serial.print(F(", we wanted to see "));
      Serial.print(array_to_set[i]);
      Serial.print(F(", but instead we saw "));
      Serial.println(array_readback[i]);
    }
  }
  //Want it to always print SPI failures with timestamp (for during long term test), but only print successes if requested
  
  if (to_print == true){
    if (received == true){
      Serial.println(F("SPI Write Successful!"));
    }
  }
  
  if (received == false){
    Serial.println(millis());
    Serial.println(F("ERROR: SPI Write Unsuccessful"));
    }
}

bool switch_test(){
  for (int i = 0; i < 100; i++){
    delay(1000);
    digitalWrite(LE, LOW);
    //delay(1);
    digitalWrite(D_IN, LOW);
    digitalWrite(CLK, LOW);
    //delay(1);
    digitalWrite(CLK, HIGH);
    digitalWrite(D_IN, HIGH);
    //delay(1);
    digitalWrite(CLK, LOW);
    //delay(1);
    digitalWrite(CLK, HIGH);
    digitalWrite(D_IN, LOW);
    //delay(1);
    digitalWrite(CLK, LOW);
    //delay(1);
    digitalWrite(CLK, HIGH);
    digitalWrite(D_IN, HIGH);
    //delay(1);
    //digitalWrite(LE, HIGH);
  }
  return true;
}

//Configuration explanation
//The IO-1733-1 board is designed with the SPI chain below with shared clock/LE line:

//input -> HV5523 -> HV3418 -> readback

//If you look at the data sheets, you'll see that the HV3418 shifts new bits in on a CLK rising edge and latches it (enables the output) on a LE falling edge
//while the HV5523 shifts new bits in on a CLK falling edge and latches on a LE rising edge.  I know, less than optimal, but these were the only chips that fit what we needed and worked at cryo.

//We have 4 options for the POL and PHA pins of the LTC6820.  
//The datasheet shows that the SCK pulse lasts for 100 ns or 1us depending on the SLOW pin.  Because we're going to be in cryo, I'd rather use SLOW to give us the margin if we need it, so we make sure the MOSI pin has settled.
//At room temp, the HV3418 requires the data line to be stable for ~30 ns before and after the clock, which must be >83 ns.  The output reflects the bit that you pushed 160 ns after the clock pulses.  The timing requirements are a little less
//strict for the HV5523, but still, because everything could completely change in cryo, it's better being safe.  For that reason POL=0, PHA=1 and POL=1, PHA=0 are out because I don't want a falling/rising SCK pulse. 
//That pattern would possibly create a race condition by shifting the HV5523 and immediately shifting it into the HV3418.  Because we have no control over the SCK pulse width, I'd much rather have a rising/falling SCK pulse.

//Now, with those options, POL=1, PHA=1 is more tricky to deal with.  The CS (or LE in our case) pin initiating the write also triggers SCK to fall and then rise at the end when LE comes back up.
//This shifts bits into either chip when we don't have control over the SPI data line.  It also makes things annoying at the end, where we need both LE edges to get both chips to latch, but in this case, that would actually keep shifting bits in.
//It would be just plain annoying to deal with all that.

//With POL=0, PHA=0, there are no suprise SCK edges from CS/LE, so I can just pulse those as much as I want to latch both chips.  The only downside is that the readback is shifted by one bit.  Because the first rising edge of the SCK pulse train
//Causes the HV3418 to shift in the last bit sent from the HV5523, the HV3418 that outputs the readback is one bit "behind".  I make up for this by having the random SCK pulse after the LE goes low in the write_chip() function

