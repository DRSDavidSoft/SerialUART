/**
 * ==================================================
 * SerialUART Project
 * David Refoua <David@Refoua.me>
 * ==================================================
 * 
 * Filename: SerialUART.h
 * Version:  v1.3-dev
 * 
 * Description: 
 * 
 */

// ------------------------------------------------------------------
// Declare variables and structures
// ------------------------------------------------------------------

#define MESSAGE_LENGTH 100

#ifdef OS_WIN
DCB dcbSerialParams = {0};
COMMTIMEOUTS timeouts = {0};
DWORD dwBytesWritten = 0;
#else
#define HANDLE int
#define INVALID_HANDLE_VALUE -1
#define DTR_CONTROL_ENABLE true
#define DTR_CONTROL_DISABLE false
#define RTS_CONTROL_ENABLE true
#define RTS_CONTROL_DISABLE false
#endif

HANDLE fdSerial = INVALID_HANDLE_VALUE;
bool dtr_control = DTR_CONTROL_ENABLE;
bool rts_control = RTS_CONTROL_DISABLE;

unsigned char argbuff[PATH_LENGTH];
unsigned char buffer[PATH_LENGTH];
unsigned char text_to_send[PATH_LENGTH];
unsigned char digits[PATH_LENGTH];

char dev_name[PATH_LENGTH] = "";
int dev_number = -1;
int baudrate = 57600;
int scan_max = 30;
int scan_min = 1;
int boot_wait_time = -1; //2;
int interval_time = -1; //0;
int read_timeout = 10;
int debug = 1; // print some info by default
int m, n;

int noExec = 0;

bool parse_hex_bytes = 0;
bool welcome_msg = 1;
bool verify_only = 0;

/*

	TODO: 

	// Arduino wiki:
	// https://playground.arduino.cc/Main/DisablingAutoResetOnSerialConnection

	// Win32 API resources:
	// https://github.com/xanthium-enterprises/Serial-Programming-Win32API-C
	// http://xanthium.in/Serial-Port-Programming-using-Win32-API
	// https://batchloaf.wordpress.com/serialsend/

	// https://www.ibm.com/support/knowledgecenter/SSLTBW_1.13.0/com.ibm.zos.r13.bpxbd00/rtcgis.htm
	// https://www.cs.fsu.edu/~baker/devices/lxr/source/2.6.25/linux/drivers/serial/suncore.c
	// https://raw.githubusercontent.com/WiringPi/WiringPi/master/examples/serialTest.c
	// http://www.i-programmer.info/programming/cc/10027-serial-c-and-the-raspberry-pi.html?start=2
	
	// Linux Serial resources:
	// https://github.com/todbot/arduino-serial/blob/master/arduino-serial.c
	// http://xanthium.in/Serial-Port-Programming-on-Linux

	// linux-serial-test is GOLD
	// https://github.com/cbrake/linux-serial-test
	
	// http://www.teuniz.net/RS-232/
	// http://www.tldp.org/HOWTO/Serial-HOWTO.html

	// DTR Config resources:
	// https://www.experts-exchange.com/questions/26929457/How-to-set-DTR-on-termios-in-Linux.html
	// http://xanthium.in/Controlling-RTS-and-DTR-pins-SerialPort-in-Linux
	// https://stackoverflow.com/questions/30143244/disable-dtr-with-termios-h-for-arduino
	// https://github.com/GuLinux/GalaxyHunter/blob/master/old/set-serial-signal/set-serial-signal.c
	// https://github.com/xanthium-enterprises/RTS-DTR-Pin-Control-Linux/blob/master/serial_rts_dtr.c

	// Termios on OS X resources:
	// https://stackoverflow.com/questions/28659045/termios-configuration

	// Makefile example:
	// https://raw.githubusercontent.com/todbot/arduino-serial/master/Makefile

	// NMake on Windows:
	// https://msdn.microsoft.com/en-us/library/f35ctcxw.aspx?f=255&MSPPError=-2147217396
	// https://stackoverflow.com/questions/45882757/how-to-write-makefilenmake-for-visual-studio-build-tool-cl-exe

	// Usage example:
	// http://todbot.com/blog/2006/12/06/arduino-serial-c-code-to-talk-to-arduino/
	// http://todbot.com/blog/2013/04/29/arduino-serial-updated/

	// How to find all serial devices (ttyS, ttyUSB, ...) on Linux without opening them?
	// https://stackoverflow.com/q/2530096/1454514



*/

