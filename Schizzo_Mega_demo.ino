#include <Arduino.h>

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#define XBEE_SERIES 2

#include <avr/pgmspace.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <EEPROMWearLevel.h>
//#include <XBee.h>
#include <SerialCommands.h>
#include "Commands.h"
#include "build_defs.h"
#include "DS1307.h"

#define TOSTR(X) #X
///////////////////////////////////////////////////////////////////////////////
//  Watchdog constants                                                       //
///////////////////////////////////////////////////////////////////////////////
#ifndef WDTCSR
#define WDTCSR	(*((unsigned char*)0x21))
#endif // WDTCSR defined

//Predefined prescaler timeout flags.
#define AVR8_WDT_PRESCALER_16MS		((unsigned char)0x00)
#define AVR8_WDT_PRESCALER_32MS		((unsigned char)0x01)
#define AVR8_WDT_PRESCALER_64MS		((unsigned char)0x02)
#define AVR8_WDT_PRESCALER_125MS	((unsigned char)0x03)
#define AVR8_WDT_PRESCALER_250MS	((unsigned char)0x04)
#define AVR8_WDT_PRESCALER_500MS	((unsigned char)0x05)
#define AVR8_WDT_PRESCALER_1S		((unsigned char)0x06)
#define AVR8_WDT_PRESCALER_2S		((unsigned char)0x07)
#define AVR8_WDT_PRESCALER_4S		((unsigned char)0x20)
#define AVR8_WDT_PRESCALER_8S		((unsigned char)0x21)

///////////////////////////////////////////////////////////////////////////////
//  EEPROM variables                                                         //
///////////////////////////////////////////////////////////////////////////////
#define EEPROM_LAYOUT_VERSION 0
#define AMOUNT_OF_INDEXES 2

#define INDEX_LED_ENABLE 0
#define INDEX_INTERVAL_XMIT 1

uint8_t   LED_ENABLE;
uint16_t  INTERVAL_XMIT;

///////////////////////////////////////////////////////////////////////////////
//  Constants                                                                //
///////////////////////////////////////////////////////////////////////////////

// Firmware information
const uint8_t FIRMWARE_VERSION = 10;

//const uint16_t XBEE_PAYLOADSIZE = 10;

// Pin configuration
//  Serial Terminal                   Internal UART0
//  XBEE                              Internal UART1
const int PIN_CHIPSELECT = 4;
const int PIN_RUNLED     = 13;    //  On-board LED

// Interval times in ms
const unsigned long INTERVAL_RUNLED      = 500;

///////////////////////////////////////////////////////////////////////////////
//  String constants                                                         //
///////////////////////////////////////////////////////////////////////////////
const char STR_SETRTC[]       PROGMEM = {"time [YYYYMMDDhhmmssx]: Configure RTC date and time"};
const char STR_LEDENABLE[]    PROGMEM = {"led [on|off]: Configure LED visibility"};
const char STR_TXINTERVAL[]   PROGMEM = {"txinterval [n]: Configure transmit interval in seconds"};
const char STR_SHOWDATA[]     PROGMEM = {"showdata: Display sensor values and transmit"};

const char STR_YES[]          PROGMEM = {"YES"};
const char STR_NO[]           PROGMEM = {"NO"};
const char STR_FWVERSION[]    PROGMEM = {" Firmware version: "};
const char STR_COMPDATE[]     PROGMEM = {" Compile date: "};
const char STR_COMPTIME[]     PROGMEM = {" Compile time: "};
const char STR_LEDSTATUS[]    PROGMEM = {" LEDs enabled: "};
const char STR_DBGSAVE[]      PROGMEM = {"Saving..."};
const char STR_DBGDONE[]      PROGMEM = {"Done!"};
const char STR_RTCDATE[]      PROGMEM = {"RTC date:         "};
const char STR_RTCTIME[]      PROGMEM = {"RTC time:         "};
const char STR_LEDUPDATE[]    PROGMEM = {"Updating..."};
const char STR_WRITEFILE[]    PROGMEM = {"Data written to file: "};
const char STR_FILEERR[]      PROGMEM = {"Can't write to file: "};
const char STR_DBGRXERROR[]   PROGMEM = {"Receive error"};
const char STR_DBGSDFAIL[]    PROGMEM = {"SD Card failed, or not present"};
const char STR_DBGSDOK[]      PROGMEM = {"SD Card ready"};
const char STR_XBEERDY[]      PROGMEM = {"XBee ready"};
const char STR_XBEEFAIL[]     PROGMEM = {"XBee failed, or not installed"};
const char STR_ASCIIART[]     PROGMEM = {
  " ___     _    _             __  __\r\n"
  "/ __| __| |_ (_)_________  |  \\/  |___ __ _ __ _\r\n"
  "\\__ \\/ _| ' \\| |_ /_ / _ \\ | |\\/| / -_) _` / _` |\r\n"
  "|___/\\__|_||_|_/__/__\\___/ |_|  |_\\___\\__, \\__,_|\r\n"
  "                                      |___/\r\n"
};

///////////////////////////////////////////////////////////////////////////////
//  Global variables                                                         //
///////////////////////////////////////////////////////////////////////////////

// Bitfields for 1-bit flags
struct {
  unsigned sdcard_present: 1;
  unsigned runled_state: 1;
  unsigned terminal_input_complete: 1;
  unsigned force_sensor_display_and_transmit: 1;
} global_flag;

// Interval keepup
uint16_t      seconds         = 0;
unsigned long runledMillis    = 0;
unsigned long secondMillis    = 0;

// RTC stuff
DS1307 clock;

// Serial command stuff
char serial_command_buffer_[32];
SerialCommands serial_commands_(
  &Serial,
  serial_command_buffer_,
  sizeof(serial_command_buffer_),
  "\r\n",
  " "
);

void cmd_unrecognized(SerialCommands*, const char*);
void cmd_showCommands(SerialCommands*);
void cmd_setRtc(SerialCommands*);
void cmd_setTxInterval(SerialCommands*);
void cmd_setLedVisibility(SerialCommands*);
void cmd_showData(SerialCommands*);

SerialCommand cmd_showCommands_     ("help",        cmd_showCommands);
SerialCommand cmd_setRtc_           ("time",        cmd_setRtc);           // parameter: YYYYMMDDhhmmssx
SerialCommand cmd_setLedVisibility_ ("led",         cmd_setLedVisibility); // parameter: [on | off]
SerialCommand cmd_setTxInterval_    ("txinterval",  cmd_setTxInterval);    // parameter: uint16 (seconds)
SerialCommand cmd_showData_         ("showdata",    cmd_showData);

// XBee / ZigBee stuff
//XBee xbee                     = XBee();   // XBee wireless comunication
//uint8_t* tx_payload           = new uint8_t[XBEE_PAYLOADSIZE];
//XBeeAddress64 addr64          = XBeeAddress64(0x00000000, 0x00000000);
//ZBTxRequest zbTx              = ZBTxRequest(addr64, tx_payload, XBEE_PAYLOADSIZE);
//ZBTxStatusResponse txStatus   = ZBTxStatusResponse();
//XBeeResponse response         = XBeeResponse();
//ZBRxResponse zbRx             = ZBRxResponse();
//ModemStatusResponse zbMsr     = ModemStatusResponse();

void logToSDcard(float, float);
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//  Arduino's setup function                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
void setup() {
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  pinMode(PIN_CHIPSELECT, OUTPUT);
  pinMode(PIN_RUNLED, OUTPUT);

  // Disable the watchdog
  WDTCSR = (unsigned char)0x10;	//WDTCSR.WDE=0 and WDTCSR.WDCE=1 (if present) to disable watchdog
  EEPROMwl.begin(EEPROM_LAYOUT_VERSION, AMOUNT_OF_INDEXES);
  Wire.begin();
  clock.begin();
  Serial.begin(9600);   // for Terminal
  while (!Serial);      // wait for serial port to connect. Needed for native USB port only

//  Serial1.begin(9600);
//  xbee.begin(Serial1);

  global_flag.sdcard_present = 0;
  global_flag.runled_state = 0;
  global_flag.terminal_input_complete = 0;

  serial_commands_.SetDefaultHandler(cmd_unrecognized);
  serial_commands_.AddCommand(&cmd_showCommands_);
  serial_commands_.AddCommand(&cmd_setTxInterval_);
  serial_commands_.AddCommand(&cmd_setRtc_);
  serial_commands_.AddCommand(&cmd_setLedVisibility_);
  serial_commands_.AddCommand(&cmd_showData_);

  LoadConfiguration();

  if (LED_ENABLE == 0)
    digitalWrite(PIN_RUNLED, 0); // Turn the runled off when disabled

  // see if the card is present and can be initialized:
  if (!SD.begin(PIN_CHIPSELECT)) {
    global_flag.sdcard_present = 0;
    D_FAIL(STR_DBGSDFAIL);
  } else {
    global_flag.sdcard_present = 1;
    D_OK(STR_DBGSDOK);
  }
} /* void setup() */

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//  Arduino's loop function                                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
void loop()
{
  unsigned long currentMillis;

  serial_commands_.ReadSerial();

//  xbee.readPacket();
//  // Handle incoming traffic
//  if (xbee.getResponse().isAvailable()) {
//    // got something
//    if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
//      // got a zb rx packet
//      // now fill our zb rx class
//      xbee.getResponse().getZBRxResponse(zbRx);
//
//      if (zbRx.getOption() == ZB_PACKET_ACKNOWLEDGED) {
//        // the sender got an ACK
//      } else {
//        // we got it (obviously) but sender didn't get an ACK
//      }
//
//      //      if (rx.getDataLength() > 0) {
//      //        for (int i = 0; i < rx.getDataLength(); ++i) {
//      //          Serial.print(rx.getData()[i]);  // Display what we've got
//      //        }
//      //        Serial.println();
//      //        D_OK(STR_RXMESSAGE);
//      //      } else {
//      //        D_FAIL(STR_DBGRXERROR);
//      //      }
//
//    } else if (xbee.getResponse().getApiId() == MODEM_STATUS_RESPONSE) {
//      xbee.getResponse().getModemStatusResponse(zbMsr);
//      // the local XBee sends this response on certain events, like association/dissociation
//
//      if (zbMsr.getStatus() == ASSOCIATED) {
//        // yay this is great.
//      } else if (zbMsr.getStatus() == DISASSOCIATED) {
//        // this is awful..
//      } else {
//        // another status
//      }
//    } else {
//      // not something we were expecting
//    }
//  } else if (xbee.getResponse().isError()) {
//    D_FAIL(STR_DBGRXERROR);
//  }

  // RUNLED
  currentMillis = millis();
  if (currentMillis - runledMillis >= INTERVAL_RUNLED) {
    runledMillis = currentMillis;
    if (LED_ENABLE == 0) {
      digitalWrite(PIN_RUNLED, 0);
    }
    else {
      digitalWrite(PIN_RUNLED, global_flag.runled_state);
    }
    global_flag.runled_state ^= 1; // Toggles between 1 and 0
  }

  // SECONDS
  currentMillis = millis();
  if (currentMillis - secondMillis >= 1000) {
    secondMillis = currentMillis;
    seconds++;
  }

  if (seconds >= 2 || global_flag.force_sensor_display_and_transmit) {
    seconds = 0;

    Serial.print(F("Slider value: ")); Serial.println(analogRead(0));
    //if (global_flag.sdcard_present) {
    logToSDcard(analogRead(0), 0);
    //}
  }

  //  // XBEE
  //  if (seconds >= INTERVAL_XMIT || global_flag.force_sensor_display_and_transmit) {
  //    seconds = 0;
  //
  //    if (global_flag.sdcard_present) {
  //      logToSDcard();
  //    }
  //    xbee.send(zbTx);
  //  }

  global_flag.force_sensor_display_and_transmit = 0;
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
void logToSDcard(float x, float y = 0)
{
  // Compile filename for SD card logging (YYYY-MM-DD_HH)
  clock.getTime();
  String filename = String(clock.year);
  filename += ZeroPad(clock.month);
  filename += ZeroPad(clock.dayOfMonth);
  filename += ZeroPad(clock.hour);
  filename += F(".csv");

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.

  File dataFile = SD.open(filename.c_str(), FILE_WRITE);

  if (dataFile) {
    dataFile.print(clock.year + 2000, DEC);
    dataFile.print('-');
    dataFile.print(ZeroPad(clock.month));
    dataFile.print('-');
    dataFile.print(ZeroPad(clock.dayOfMonth));
    dataFile.print(',');
    dataFile.print(ZeroPad(clock.hour));
    dataFile.print(':');
    dataFile.print(ZeroPad(clock.minute));
    dataFile.print(':');
    dataFile.print(ZeroPad(clock.second));
    dataFile.print(',');

    //    for (int i = 0; i < XBEE_PAYLOADSIZE; i++) {
    //      dataFile.print(',');
    //      dataFile.print(0, DEC);
    //    }
    dataFile.print(x);
    dataFile.print(',');
    dataFile.print(y);
    dataFile.println();

    D_OKV(STR_WRITEFILE, filename.c_str());
  }
  else
  {
    D_FAILV(STR_FILEERR, filename.c_str());
  }

  dataFile.close();
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
void Display_Header()
{
  D_PRINTLN(STR_ASCIIART);
  D_PRINT(STR_FWVERSION); D_PRINTILN(FIRMWARE_VERSION);
  D_PRINT(STR_COMPDATE); D_PRINTILN(__DATE__);
  D_PRINT(STR_COMPTIME); D_PRINTILN(__TIME__);
  D_PRINT(STR_LEDSTATUS); D_PRINTLN(LED_ENABLE == 0 ? STR_NO : STR_YES);
  Serial.println();
  D_PRINTLN(STR_SETRTC);
  D_PRINTLN(STR_LEDENABLE);
  D_PRINTLN(STR_TXINTERVAL);
  D_PRINTLN(STR_SHOWDATA);
  Serial.println();
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Returns a zero padded string based on a two-digit input value             //
//                                                                           //
// Input:  a single- or double digit number                                  //
// Output: if the input was a single digit number, the output will have a    //
//         leading zero. Otherwise the double digit number will be returned  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
String ZeroPad(int value)
{
  String returnVal = (value > 9 ? "" : String(F("0"))) + String(value);
  return returnVal;
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
void DisplayRTCData(DS1307 rtcobject)
{
  rtcobject.getTime();
  D_PRINT(STR_RTCDATE);
  Serial.print(rtcobject.year + 2000); Serial.print("-");
  Serial.print(ZeroPad(rtcobject.month)); Serial.print("-");
  Serial.println(ZeroPad(rtcobject.dayOfMonth));
  D_PRINT(STR_RTCTIME);
  Serial.print(ZeroPad(rtcobject.hour)); Serial.print(":");
  Serial.print(ZeroPad(rtcobject.minute)); Serial.print(":");
  Serial.println(ZeroPad(rtcobject.second));
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
void LoadConfiguration()
{
  EEPROMwl.get(INDEX_INTERVAL_XMIT, INTERVAL_XMIT);   // 16 bits: use get
  LED_ENABLE = EEPROMwl.read(INDEX_LED_ENABLE);       // 8  bits: use read
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
void cmd_unrecognized(SerialCommands* sender, const char* cmd)
{
  sender->GetSerial()->print("Unrecognized command [");
  sender->GetSerial()->print(cmd);
  sender->GetSerial()->println("]");
}

void cmd_showCommands(SerialCommands* sender)
{
  Display_Header();
}

void cmd_setRtc(SerialCommands* sender)
{
  DisplayRTCData(clock);

  char* argument = sender->Next();
  if (argument == NULL || strlen(argument) != 15) {
    sender->GetSerial()->println("Expected datecode: YYYYMMDDhhmmssx (x = [1..7] day of week, 1 = monday)");
    return;
  }

  clock.fillByYMD(
    (argument[0] - '0') * 1000 + (argument[1] - '0') * 100 + (argument[2] - '0') * 10 + (argument[3] - '0') * 1,
    (argument[4] - '0') * 10 + (argument[5] - '0') * 1,
    (argument[6] - '0') * 10 + (argument[7] - '0') * 1
  );
  clock.fillByHMS(
    (argument[8] - '0') * 10 + (argument[9] - '0') * 1,
    (argument[10] - '0') * 10 + (argument[11] - '0') * 1,
    (argument[12] - '0') * 10 + (argument[13] - '0') * 1
  );
  clock.fillDayOfWeek(argument[14] - '0');
  clock.setTime();
  D_OK(STR_DBGDONE);
  DisplayRTCData(clock);
}

void cmd_setTxInterval(SerialCommands* sender)
{
  char* argument = sender->Next();
  if (argument == NULL) {
    sender->GetSerial()->println("Expected: number of seconds between transmissions");
    return;
  }
}

void cmd_setLedVisibility(SerialCommands* sender)
{
  char* argument = sender->Next();
  if (argument == NULL) {
    sender->GetSerial()->println("Usage: led [on|off]");
    return;
  }

  if (strncmp(argument, "on", 2) == 0) {
    D_PRINTLN(STR_LEDUPDATE);
    LED_ENABLE = 1;
    digitalWrite(PIN_RUNLED, 1);
  } else if (strncmp(argument, "off", 3) == 0) {
    D_PRINTLN(STR_LEDUPDATE);
    LED_ENABLE = 0;
    digitalWrite(PIN_RUNLED, 0);
  } else {
    sender->GetSerial()->println("Usage: led [on|off]");
    return;
  }

  EEPROMwl.update(INDEX_LED_ENABLE, LED_ENABLE);
}

void cmd_showData(SerialCommands* sender)
{
  global_flag.force_sensor_display_and_transmit = 1;
}

