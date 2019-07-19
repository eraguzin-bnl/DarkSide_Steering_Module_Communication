//#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <fstream>
#include "ftd2xx.h"
#include "SPI_Sequence.h"
using namespace std;

//Just a stock length when making strings
#define CHAR_BUFFER_LENGTH 100

//When sending a message to print_save, these are just definitions for "mode" to make it more readable
#define ONLY_SAVE 1
#define ONLY_PRINT 2
#define PRINT_AND_SAVE 3

//Making it a global variable makes it easer for print_save
char debug_filename[CHAR_BUFFER_LENGTH];

///////////////////////////////////////////////////////
// CLASS DECLARATIONS
///////////////////////////////////////////////////////
/*
print_save is because when calling the DLL through C++, you can see print statements, but not when
you call it through Python.  So my solution for debugging when you call it through Python is to always
write what you would have printed to file.  Rather than clutter up the main code, this allows any debug 
information to simply be outputted with one line, with the additional option of being something you only
want to see through print (if you want to do a quick little test through the C++ exe) or through the text
output or both.
*/

class print_save_instance {
	public:
		//I need a default constructor without arguments or else the class can't be passed as a parameter
		print_save_instance() {
		}

		print_save_instance(wchar_t *save_directory_wchar_t) {
			char save_directory_string[CHAR_BUFFER_LENGTH];
			int ret = wcstombs (save_directory_string, save_directory_wchar_t, sizeof(save_directory_string));
			sprintf_s(debug_filename,sizeof(debug_filename), "%sC Argument Info.txt", save_directory_string);
			has_written = false;
		}

		void print_save (char *message, int mode){
			if (mode & 0x1){
				FILE* argument_file;
				char debug_text[CHAR_BUFFER_LENGTH * 10];
				int num_of_chars = sprintf_s(debug_text, sizeof(debug_text), message);
				char *read_write;
				if (!has_written){
					read_write = "w";
				}
				else{
					read_write = "a";
				}
				int resp = fopen_s(&argument_file,debug_filename, read_write);

				if ((!argument_file) ^ (resp != 0))
				{
					printf("Unable to open file!");
				}
				else{
					fwrite(debug_text, 1, num_of_chars, argument_file);
					fclose(argument_file);
					has_written = true;
				}
			}
			if (mode & 0x2){
				printf(message);
			}
		}
	private:
		bool has_written;
		char debug_filename[CHAR_BUFFER_LENGTH];
};

class SPI {
	public:
      // Constructor definition.  Pass in the same print_save so that the original file doesn't get overwritten (can't use global variables)
		SPI(int CS, int CK, int DOUT, int DIN, bool data[96], wchar_t *save_directory, print_save_instance record) {
			CS_pin = CS;
			CK_pin = CK;
			DOUT_pin = DOUT;
			DIN_pin = DIN;

			record.print_save("Initial write array is: ", PRINT_AND_SAVE);
			for (int i = 0; i < (sizeof(write_array)/sizeof(*write_array)); i++) 
			{
				write_array[i] = data[i];
				num_of_chars = sprintf_s(debug_text, sizeof(debug_text), "%d", data[i]);
				record.print_save(debug_text, PRINT_AND_SAVE);
			}
			record.print_save("\n", PRINT_AND_SAVE);
			record_local = record;
		}

		//The way the FT chip works is that you send an 8-bit byte with the values you want for the pins that you've deemed outputs
		//This is an easy way to use the already given pin locations and update them with the desired output values
		char createRegister(bool CS_val, bool CK_val, bool DOUT_val)
		{
			char sendRegister = 0;
			sendRegister = sendRegister | (CS_val << CS_pin);
			sendRegister = sendRegister | (CK_val << CK_pin);
			sendRegister = sendRegister | (DOUT_val << DOUT_pin);
			return sendRegister;
		}

		void check_errors(FT_STATUS status, char *step){
			if(status != FT_OK) 
			{
				num_of_chars = sprintf_s(debug_text, sizeof(debug_text), "FT Device failed step %c!\n", step);
				record_local.print_save(debug_text, PRINT_AND_SAVE);
				num_of_chars = sprintf_s(debug_text, sizeof(debug_text), "Status: %d\n", status);
				record_local.print_save(debug_text, PRINT_AND_SAVE);
			}
		}
		
		//Returns the incoming readback from the LTC chips in boolean array form
		bool * SPIwrite() {
			static const UCHAR Mask = ~(0x1 << DIN_pin) ;			   // If high, IO is output, if low, it's an input.  Only readback pin is an input.
			num_of_chars = sprintf_s(debug_text, sizeof(debug_text), "IO Mask is: %d\n", Mask);
			record_local.print_save(debug_text, PRINT_AND_SAVE);

			//Open device and initial setup
			status = FT_Open(0, &fthandle);
			check_errors(status, "1");

			status = FT_ResetDevice(fthandle);
			check_errors(status, "2");

			status = FT_SetBitMode(fthandle, Mask, Mode);
			check_errors(status, "3");

			//Might want to change baud rate in future, check documentation
			//status = FT_SetBaudRate(fthandle, 183);

			//Start SPI Sequence
			char toSend;

			//Function order is CS, CK, DOUT
			//Wake up sleeping LTC chip by toggling CS
			toSend = createRegister(false, false, false);
			status = FT_Write(fthandle,&toSend,1,&bytesWritten);
			//Assume that if it can write this one time, it can write all of the next times
			check_errors(status, "4");

			toSend = createRegister(true, false, false);
			status = FT_Write(fthandle,&toSend,1,&bytesWritten);

			//Cycles through this twice.  The first time shifts the settings in the latches, and the second time, the previous settings are outputted, so you can read and compare
			//Check the data sheets for the LTC6820, HV5523, and HV3418 so it all makes sense.  A full explanation is at the bottom of this file.
			for (int m = 0; m < 2; m++)
			{
				//CS must be low to shift in data
				toSend = createRegister(false, false, false);
				status = FT_Write(fthandle,&toSend,1,&bytesWritten);
				for (int i = 0; i < (sizeof(write_array)/sizeof(*write_array)); i++) 
				{
					//Output the given bit and toggle clock high and low to shift it in
					toSend = createRegister(false, false, write_array[i]);
					status = FT_Write(fthandle,&toSend,1,&bytesWritten);
					toSend = createRegister(false, true, write_array[i]);
					status = FT_Write(fthandle,&toSend,1,&bytesWritten);
					toSend = createRegister(false, false, write_array[i]);
					status = FT_Write(fthandle,&toSend,1,&bytesWritten);

					//You may or may not need this 1 ms delay.  I needed it when I hooked up the FT chip without the LTC in between
					//Sleep(1);
					//This function doubles as a simple way to read all the pins (only has a valid return for those deemed inputs)
					status = FT_GetBitMode(fthandle, &pucMode);
					check_errors(status, "5");

					read_array[i] = bool((pucMode >> DIN_pin) & 0x01);

					//After the last bit, you need to shift the CS a couple of times, so both chips finish latching them in
					if (i == (sizeof(write_array)/sizeof(*write_array) - 1))
					{
						toSend = createRegister(true, false, write_array[i]);
						status = FT_Write(fthandle,&toSend,1,&bytesWritten);
						toSend = createRegister(false, false, write_array[i]);
						status = FT_Write(fthandle,&toSend,1,&bytesWritten);
						toSend = createRegister(true, false, write_array[i]);
						status = FT_Write(fthandle,&toSend,1,&bytesWritten);
					}
				}
			}

			//Close the USB connection to the FT chip
			status = FT_Close(fthandle);
			check_errors(status, "6");

			//Check the readback array against the array that was written in, and print any relevant statements about
			bool match = true;
			for (int i = 0; i < (sizeof(write_array)/sizeof(*write_array)); i++) 
			{
				if (write_array[i] != read_array[i])
				{
					match = false;
				}
			}
	
			record_local.print_save("Read Info: ", PRINT_AND_SAVE);
			
			for (int i = 0; i < 96; i++) 
			{
				num_of_chars = sprintf_s(debug_text, sizeof(debug_text), "%d", read_array[i]);
				record_local.print_save(debug_text, PRINT_AND_SAVE);
			}
			
			record_local.print_save("\n", PRINT_AND_SAVE);

			if (match)
			{
				record_local.print_save("That's a match!\n", PRINT_AND_SAVE);
			}
			else
			{
				record_local.print_save("That's not a match :(\n", PRINT_AND_SAVE);
			}

			//Return that readback array so the GUI can reflect it
			return read_array;
		}

	private:
		int DIN_pin;
		int DOUT_pin;
		int CS_pin;
		int CK_pin;
		int num_of_chars;
		static const int length = 96;
		FT_HANDLE fthandle;
		FT_STATUS status;
		unsigned long bytesWritten;
		static const UCHAR Mode = 1;        //  Set Asynchronous Bit-Bang Mode
		UCHAR pucMode;
		bool write_array[length];
		bool read_array[length];
		char debug_text[CHAR_BUFFER_LENGTH * 10];
		print_save_instance record_local;
		
};

///////////////////////////////////////////////////////
// SCRIPT ENTRY POINT
///////////////////////////////////////////////////////
//We need this helper script because Ctypes can't talk directly to a C++ class.  And I want the class so I can have member functions.
//I couldn't get global functions to work in the DLL export with Ctypes
//https://stackoverflow.com/questions/1615813/how-to-use-c-classes-with-ctypes
bool * SPI_Sequence(int CS, int CK, int DOUT, int DIN, bool data[96], wchar_t *save_directory)
{
	int num_of_chars;
	char debug_text[CHAR_BUFFER_LENGTH * 10];

	print_save_instance record(save_directory);
	record.print_save("Inside DLL SPI_Sequence Function\n", PRINT_AND_SAVE);

	num_of_chars = sprintf_s(debug_text, sizeof(debug_text), "CS: %d\nCK: %d\nDOUT: %d\nDIN: %d\n", CS, CK, DOUT, DIN);
	record.print_save(debug_text, PRINT_AND_SAVE);

	record.print_save("Data: ", PRINT_AND_SAVE);
	for (int i = 0; i < 96; i++) 
	{
		num_of_chars = sprintf_s(debug_text, sizeof(debug_text), "%d", data[i]);
		record.print_save(debug_text, PRINT_AND_SAVE);
	}
	record.print_save("\n", PRINT_AND_SAVE);

	//Create the SPI object and call its SPI write.  Expect a boolean array return of the readback to give it to the Python GUI
	SPI SPIobject(CS, CK, DOUT, DIN, data, save_directory, record);
	bool * response = SPIobject.SPIwrite();
	return response;
}

//This cycles through the 96 bits in the correct order.  For each bit, it is "clocked" in, and the output bit is read.
//The HV5523 and HV3418 latch in the data on different clock edges, so the transient analysis in complicated, especially with the LTC6820 between them, but this was found experimentally to work
//Because of the register orientation, you want to put the last bit first
//With the LTC6820, a change of LE will send that across to the slave module
//With PHA = 0, POL = 0, a rising clock edge will sample D_IN and send that data to the slave module.  The slave module will adjust MOSI and then do a quick positive/negative clock pulse
//and then sample the readback on MISO and send that back.
//HOWEVER, the Master only adjusts its MISO (sampled as D_OUT here) AFTER the Arduino does the falling clock edge.  Thanks Linear for hiding that information in a figure.

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
