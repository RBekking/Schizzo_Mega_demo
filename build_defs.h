#ifndef BUILD_DEFS_H
#define BUILD_DEFS_H

#include <Arduino.h>

// For printing hard-coded strings over the serial line. Using this saves memory.
void termprint(const char* s);

// Some hard-coded strings used for debug output
const char STR_DBGFAIL[]      PROGMEM = {"[ FAIL ] "};
const char STR_DBGOK[]        PROGMEM = {"[  OK  ] "};
const char STR_DBGACTION[]    PROGMEM = {"  ===>   "};

#define D_FAIL(X)       termprint(STR_DBGFAIL); termprint(X); Serial.println();
#define D_FAILV(X,Y)    termprint(STR_DBGFAIL); termprint(X); Serial.println(Y);
#define D_OK(X)         termprint(STR_DBGOK); termprint(X); Serial.println();
#define D_OKV(X,Y)      termprint(STR_DBGOK); termprint(X); Serial.println(Y);
#define D_ACTION(X)     termprint(STR_DBGACTION); termprint(X); Serial.println();
#define D_PRINT(X)      termprint(X);
#define D_PRINTLN(X)    termprint(X); Serial.println();
#define D_PRINTI(X)     Serial.print(X);
#define D_PRINTILN(X)   Serial.println(X);
#define D_PRINTV(X,Y)   termprint(X); Serial.print(Y);
#define D_PRINTVLN(X,Y) termprint(X); Serial.println(Y);

#define BUILD_YEAR ((__DATE__[7] - '0') * 1000 +  (__DATE__[8] - '0') * 100 + (__DATE__[9] - '0') * 10 + __DATE__[10] - '0')
#define BUILD_DATE ((__DATE__[4] - '0') * 10 + __DATE__[5] - '0')

#define DIGIT(s, no) ((s)[no] - '0')

const int BUILD_MONTH = (__DATE__[2] == 'b' ? 2 :
                         (__DATE__[2] == 'y' ? 5 :
                          (__DATE__[2] == 'l' ? 7 :
                           (__DATE__[2] == 'g' ? 8 :
                            (__DATE__[2] == 'p' ? 9 :
                             (__DATE__[2] == 't' ? 10 :
                              (__DATE__[2] == 'v' ? 11 :
                               (__DATE__[2] == 'c' ? 12 :
                                (__DATE__[2] == 'n' ?
                                 (__DATE__[1] == 'a' ? 1 : 6) :
                                 /* Implicit "r" */
                                 (__DATE__[1] == 'a' ? 3 : 4))))))))));

const int BUILD_DAY = ( 10 * (__DATE__[4] == ' ' ? 0 : DIGIT(__DATE__, 4))
                        + DIGIT(__DATE__, 5));

#define BUILD_HOUR ((__TIME__[0] - '0') * 10 + __TIME__[1] - '0')
#define BUILD_MIN ((__TIME__[3] - '0') * 10 + __TIME__[4] - '0')
#define BUILD_SEC ((__TIME__[6] - '0') * 10 + __TIME__[7] - '0')

#endif // BUILD_DEFS_H
