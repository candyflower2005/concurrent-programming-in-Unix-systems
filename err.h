#ifndef _ERR_
#define _ERR_
#include "shared_variables.h"

/* wypisuje informacje o blednym zakonczeniu funkcji systemowej 
i konczy dzialanie */
void syserr(const char *fmt, ...);

/* wypisuje informacje o bledzie i konczy dzialanie */
void fatal(const char *fmt, ...);

#endif
