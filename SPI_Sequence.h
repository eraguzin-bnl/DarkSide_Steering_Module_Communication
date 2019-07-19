#ifndef SPI_Stuff
#define SPI_Stuff
#include <string>
using namespace std;
//extern "C" makes sure the function name doesn't get mangled
extern "C" __declspec(dllexport) bool * SPI_Sequence(int CS, int CK, int DOUT, int DIN, bool data[96], wchar_t *save_directory);
#endif