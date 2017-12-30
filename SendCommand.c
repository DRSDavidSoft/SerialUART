/**------------------------------------------------------------------
 *	SerialUART.c
 *	version: 1.3-dev
 *
 *	Send data via serial port, wait for reply from UART device to read
 *	and prints it to the console.
 *
 *	Author: David Refoua <David@Refoua.me>
 *------------------------------------------------------------------*/

#define AUTHOR "David Refoua <David@Refoua.me>"
static const char VERSION[] = "1.3-dev";


// ------------------------------------------------------------------
// Include the required header files
// ------------------------------------------------------------------
#include "lib/os_macros.h"   /* Detect which platform we're currently running on */
#include "lib/SerialUART.h"  /* Include platform specific libraries */
#include "lib/SerialUART.c"


// ------------------------------------------------------------------
// Program display messages
// ------------------------------------------------------------------

void WelcomeMsg(char* app_name)
{
	if (debug > 0 && welcome_msg)
	{
		welcome_msg = 0;
		fprintf(stderr, "%s v%s\n", app_name, VERSION);
		fprintf(stderr, "%s\n\n", AUTHOR);
	}
}

void ExitMessage(const char* error_message, int error)
{
	if (debug > 0) {
		// Print an error message
		fprintf(stderr, error_message);
		fprintf(stderr, "\n");
	}

	// Exit the program
	exit(error);
}

// ------------------------------------------------------------------
// 
// ------------------------------------------------------------------
void SetConfig(void)
{
	// Set device parameters:

	#ifdef OS_WIN
		// Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/aa363214(v=vs.85).aspx
		//	baudrate (default 57600), 1 start bit, 1 stop bit, no parity
		dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
		if (GetCommState(fdSerial, &dcbSerialParams) == 0)
			ExitMessage("Error getting device state", EXIT_FAILURE);
		dcbSerialParams.BaudRate = baudrate;
		dcbSerialParams.ByteSize = 8; //bytesize
		dcbSerialParams.StopBits = ONESTOPBIT;
		dcbSerialParams.Parity = NOPARITY;
		dcbSerialParams.fDtrControl = dtr_control; //DTR_CONTROL_DISABLE;
		dcbSerialParams.fRtsControl = rts_control; //RTS_CONTROL_DISABLE;
		//dcbSerialParams.fOutxCtsFlow = FALSE;
		//dcbSerialParams.fOutxDsrFlow = FALSE;
		//dcbSerialParams.fDsrSensitivity = FALSE;
		//dcbSerialParams.fAbortOnError = TRUE;
		if(SetCommState(fdSerial, &dcbSerialParams) == 0)
			ExitMessage("Error setting device parameters", EXIT_FAILURE);

		// Set COM port timeout settings
		timeouts.ReadIntervalTimeout = 50;
		timeouts.ReadTotalTimeoutConstant = 50;
		timeouts.ReadTotalTimeoutMultiplier = 10;
		timeouts.WriteTotalTimeoutConstant = 50;
		timeouts.WriteTotalTimeoutMultiplier = 10;
		if(SetCommTimeouts(fdSerial, &timeouts) == 0)
			ExitMessage("Error setting timeouts", EXIT_FAILURE);
	#endif

}

void CloseSerialPort(void)
{
	#ifdef OS_WIN
	if (fdSerial != INVALID_HANDLE_VALUE)
	{
		// Close serial port
		if (debug > 0) fprintf(stderr, "\nClosing serial port... ");
		if (CloseHandle(fdSerial) == 0)
		{
			if (debug > 0) fprintf(stderr, "Error\n");
			_exit(EXIT_FAILURE);
		}
		else if (debug > 0) fprintf(stderr, "OK\n");
	}
	#endif
	
	_exit(EXIT_SUCCESS); // exit normally
}


// ------------------------------------------------------------------
// Main program structure and definitions
// ------------------------------------------------------------------

char* getProgramName( char* path ) {
	
	char *result = malloc(128);
	
	for ( int i = 0; path[i] != '\0'; i++ ) {
		if ( path[i] == '\\' ) path[i] = '/';
	}
	
	char *pch = strrchr(path, '/');
	if ( pch != NULL ) strcpy( result, pch + 1 );
	else strcpy( result, path );
	
	return result;
	
}

bool matchArgument( char *arg, char *shortarg, char *longarg, bool display ) {
	
	bool result = false;
	
	if ( strlen(arg) == 0 ) result = false;
	else {
		
		if ( strlen(longarg) > 0 ) {
		
			if ( strncmp(arg, "/", 1) == 0 )
				result |= ( strcmp(arg + 1, longarg) == 0 );
			
			if ( strncmp(arg, "--", 2) == 0 )
				result |= ( strcmp(arg + 2, longarg) == 0 );
		}
		
		if ( strlen(shortarg) > 0 ) {
			
			if ( strncmp(arg, "-", 1) == 0 )
				result |= ( strcmp(arg + 1, shortarg) == 0 );
			
		}
		
		if ( display == true ) {
			// TODO: 
			//char message[] = "Does something.";
			//fprintf(stderr, "  -%s --%s\t%s\n", shortarg, longarg, message);
		}
		
	}
	
	return result;
	
}

void parseArguments( int argc, char *argv[], bool show_usage ) {
	
	strcpy(buffer, "");
	
	// Parse command line arguments
	int argn = 1;
	while (argn < argc)
	{
		
		// Process next command line argument
		if ( matchArgument( argv[argn], "h", "hello", show_usage ) == true )
		{
			ExitMessage("Hello, world!", 1);
		}
		
		// Process next command line argument
		else if ( matchArgument( argv[argn], "p", "device", show_usage ) == true )
		{
			if (++argn >= argc) ExitMessage("Error: no serial port specified", 1);
			//else if (debug) fprintf(stderr, "Device number %d specified\n", dev_number);

			// Set device number to specified value
			dev_number = atoi(argv[argn]);
		}
		/*
		else if (strcmp(argv[argn], "/id") == 0)
		{
			if (++argn >= argc) ExitMessage("Error: no id number specified", 1);

			// Set id to specified value
			id = atoi(argv[argn]);
		}
		*/
		else if ( matchArgument( argv[argn], "b", "baudrate", show_usage ) == true )
		{
			// Parse baud rate
			if (++argn >= argc) ExitMessage("Error: no baudrate value specified", 1);
			//else if (debug && ((baudrate = atoi(argv[argn])) > 0)) fprintf(stderr, "%d baud specified\n", baudrate);

			// Set baudrate to specified value
			baudrate = atoi(argv[argn]);
		}
		else if ( matchArgument( argv[argn], "H", "hex", show_usage ) == true )
		{
			// Parse flag for hex byte parsing.
			// If this flag is set, then arbitrary byte values can be
			// included in the string to send using '\x' notation.
			// For example, the command "SerialSend /hex Hello\x0D"
			// sends six bytes in total, the last being the carriage
			// return character, '\r' which has hex value 0x0D.
			parse_hex_bytes = 1;
		}
		else if ( matchArgument( argv[argn], "s", "command", show_usage ) == true )
		{
			if (++argn >= argc) ExitMessage("Error: no command specified", 1);
			//else if (debug) fprintf(stderr, "Specified Command:\n%d\n", argv[argn]);
			
			// This command line argument is the text to send
			strcpy(buffer, argv[argn]);
		}
		/*
		else if (strcmp(argv[argn], "/dothething") == 0)
		{
			option_dothething = 1;
		}
		*/
		else if ( matchArgument( argv[argn], "", "dtr", show_usage ) == true )
		{
			if (++argn >= argc) ExitMessage("Error: DTR control parameter was not specified", 1);

			if (strcmp(argv[argn], "off") == 0) dtr_control = DTR_CONTROL_DISABLE; else
			if (strcmp(argv[argn], "on" ) == 0) dtr_control = DTR_CONTROL_ENABLE;  else
			ExitMessage("Error: DTR control parameter invalid", 1);
		}
		else if ( matchArgument( argv[argn], "", "rts", show_usage ) == true )
		{
			if (++argn >= argc) ExitMessage("Error: RTS control parameter was not specified", 1);

			if (strcmp(argv[argn], "off") == 0) rts_control = RTS_CONTROL_DISABLE; else
			if (strcmp(argv[argn], "on" ) == 0) rts_control = RTS_CONTROL_ENABLE;  else
			ExitMessage("Error: RTS control parameter invalid", 1);
		}
		else if ( matchArgument( argv[argn], "W", "wait", show_usage ) == true )
		{
			if (++argn >= argc) ExitMessage("Error: no boot wait delay value specified", 1);

			boot_wait_time = atoi(argv[argn]);
		}
		else if ( matchArgument( argv[argn], "i", "interval", show_usage ) == true )
		{
			if (++argn >= argc) ExitMessage("Error: no interval time value specified", 1);

			interval_time = atoi(argv[argn]);
		}
		else if ( matchArgument( argv[argn], "t", "timeout", show_usage ) == true )
		{
			if (++argn >= argc) ExitMessage("Error: no read timeout value specified", 1);

			read_timeout = atoi(argv[argn]);
		}
		else if ( matchArgument( argv[argn], "", "verifyonly", show_usage ) == true )
		{
			verify_only = 1;
		}
		else if ( matchArgument( argv[argn], "", "debug", show_usage ) == true )
		{
			debug = 2;
		}
		else if ( matchArgument( argv[argn], "q", "quiet", show_usage ) == true )
		{
			debug = 0;
		}
		else if ( matchArgument( argv[argn], "v", "version", show_usage ) == true )
		{
			noExec = 1;
		}
		else
		{
			// Be sure the welcome msg is displayed first
			WelcomeMsg( argv[0] );
			
			// Unknown command line argument
			if (debug > 0) fprintf(stderr, "Unrecognized option: %s\n", argv[argn]);
			// TODO: move out of this funtion
		}

		argn++; // Increment command line argument counter
	}
	
}

int main(int argc, char *argv[])
{
	
	// Find the application's CLI name
	char *app_name = getProgramName( argv[0] );
	
	// Parse arguments
	parseArguments( argc, argv, false );

	// Welcome message
	WelcomeMsg( app_name );
	
	// __commentme__
	if ( noExec == 1 ) exit(0);

	// Debug messages
	if (debug > 1) fprintf(stderr, "dev_number = %d\n", dev_number);
	if (debug > 1) fprintf(stderr, "baudrate = %d\n\n", baudrate);

	// Register function to close serial port at exit time
	atexit(CloseSerialPort);
	
	// Check that some text to send was provided
	if (strlen(buffer) == 0)
	{
		//ExitMessage("Error: no command is specified", 1);
		fprintf(stderr, "No command is specified\n");
	}
	 
	// If hex parsing is enabled, modify text to send
	n = 0; m = 0;
	while(n < strlen(buffer))
	{
		if (parse_hex_bytes && buffer[n] == '\\')
		{
			n++;
			if (buffer[n] == '\\') text_to_send[m] = '\\';
			else if (buffer[n] == 'n') text_to_send[m] = '\n';
			else if (buffer[n] == 'r') text_to_send[m] = '\r';
			else if (buffer[n] == 'x')
			{
				digits[0] = buffer[++n];
				digits[1] = buffer[++n];
				digits[2] = '\0';
				text_to_send[m] = strtol(digits, NULL, 16);
			}
		}
		else
		{
			text_to_send[m] = buffer[n];
		}
		 
		m++; n++;
	}
	text_to_send[m] = '\0'; // Null character to terminate string	

	// If the device number is specified
	if (dev_number != -1)
	{
		// Limit scan range to specified COM port number
		scan_max = dev_number;
		scan_min = dev_number;
	}

	// Scan for the highest available COM port in _descending_ order
	for (n = scan_max ; n >= scan_min ; --n)
	{
		// Try next device
		if (debug > 1) {
			fprintf(stderr, "\r						");
			fprintf(stderr, "\rTrying %s...", dev_name);
		}
		sprintf(dev_name, "\\\\.\\COM%d", n);
		fdSerial = CreateFile(dev_name, GENERIC_READ|GENERIC_WRITE, 0, NULL,
								OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (fdSerial != INVALID_HANDLE_VALUE)
		{
			if (debug > 1) fprintf(stderr, "OK!\n");
			dev_number = n;
			break; // stop scanning COM ports
		}
		else if (debug > 1) fprintf(stderr, "FAILED!\n");
	}

	// Check in case no serial port could be opened
	if (fdSerial==INVALID_HANDLE_VALUE)
		ExitMessage("Error: could not open serial port", 1);

	// If we get this far, a serial port was successfully opened
	if (debug) fprintf(stderr, "Opening COM%d at %d baud\n\n", dev_number, baudrate);

	SetConfig();
	
	// Flush any remaining characters in the buffers
	// linux version is: tcflush(fd, TCIOFLUSH);
	PurgeComm(fdSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
	
	// Set up time events
	double timeout = 0.0;
	time_t start, stop;
	
	time(&start);
		
	// Wait while the device boots
	if (boot_wait_time > 0) while(1) {
		time(&stop);
		timeout = difftime(stop, start);
		if (timeout >= boot_wait_time) break;
	}
	
	// Send specified text
	if (strlen(buffer) > 0)
	{
		DWORD bytes_written, total_bytes_written = 0;
		if (debug) fprintf(stderr, "Sending text... ");
		while(total_bytes_written < m)
		{
			// Add a little bit of delay
			if (interval_time > 0) Sleep(interval_time);
			
			if(!WriteFile(fdSerial, text_to_send + total_bytes_written,
				interval_time > 0 ? 1 : m - total_bytes_written, &bytes_written, NULL))
			{
				ExitMessage("Error writing text", 1);
			}
			 
			total_bytes_written += bytes_written;
		}
		if (debug) fprintf(stderr, "\n%d bytes written to %s\n", total_bytes_written, dev_name);
	}
	
	if (debug) fprintf(stderr, (strlen(buffer) > 0) ? "\nReply: \n\n" : "");
	
	int state = 1;
	
	char c;
	char message_buffer[MESSAGE_LENGTH];
	DWORD bytes_read;
	
	if (verify_only == 1) {
		time(&start);
		while(1) {
			ReadFile(fdSerial, &c, 1, &bytes_read, NULL);
			if (bytes_read >= 1) exit(0);
			else if (read_timeout >= 0) {
				time(&stop);
				timeout = difftime(stop, start);
				if (timeout >= read_timeout) {
					if (debug) fprintf(stderr, "\nVerification timed out in %.f seconds", timeout);
					exit(EXIT_TIMEDOUT);
				}
			}
		}
	}

	// TODO: Win32 API: how to read the serial, or exit within a timeout if wasn't a data
	// https://stackoverflow.com/a/25428844/1454514

	if (true)
	{	
		time(&start);
		
		// print everything to console
		while(1)
		{
			ReadFile(fdSerial, &c, 1, &bytes_read, NULL);
			if (bytes_read == 1)
			{
				time(&start);
				
				if (c == 0 || c == 26) exit(0); // If a CTRL+Z is received
				else {
					printf("%c", c);
					// if (debug > 1) debug_log(c);
				}
			} else
			if (read_timeout >= 0) {
				time(&stop);
				timeout = difftime(stop, start);
				if (timeout >= read_timeout) {
					// Nothing to read for `timeout` seconds.
					if (debug > 1) fprintf(stderr, "\nThere was no reply for %.f seconds", timeout);
					exit(124); // Return code for timeout EXIT_TIMEDOUT
				}
			}
		}
	}

	// We should never get to this point because when the user
	// presses Ctrl-C, the atexit function will be called instead.
	return 0;
}
