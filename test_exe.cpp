using namespace std;
#include <stdio.h>
#include <iostream>
#include "SPI_Sequence.h"

/*
This is a test bench for starting up the SPI communication C++ DLL.  This allows us to run the DLL without going through
Python and Ctypes, and lets us see print statements as well.  If you want to use it, have Visual Studio run this exe upon compile time.
If not, just uncheck it (in Build->Configuration Manager).  It uses default values for the pins and the data it actually sends.
*/

int main()
{
  printf("Starting up test bench: Project is FT_Chip_Test\n");
  bool* resp;
  int CS = 2;
  int CK = 3;
  int DOUT = 1;
  int DIN = 0;
  wchar_t save_directory[] = L"D:\\OneDrive - Brookhaven National Laboratory\\DarkSide\\FT2232H\\C Program\\";
  bool data[96] = { 0 };
  for (int i = 0; i < 96; i++) {
	  if (i < 64){
		  data[i] = false;}
	  else{
		  data[i] = true;
	  }
  }
  resp = SPI_Sequence(CS, CK, DOUT, DIN, data, save_directory);
  cout<<"\n\nPress any key to end test bench...";
  cin.ignore();
}