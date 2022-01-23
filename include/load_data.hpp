#ifndef LOADDATA
#define LOADDATA

#include <iostream>
#include "datastruct.hpp"
using namespace std;

bool load_data(char* filename, datastruct* datastate);
void close_data(datastruct* datastate);

#endif