/**
 * ==================================================
 * SerialUART Project
 * David Refoua <David@Refoua.me>
 * ==================================================
 * 
 * Filename: SerialUART.h
 * Version:  v1.3-dev
 * 
 * Description: This header file is used to include 
 *     platform specific libraries to communicate with
 *     the serial port.
 * 
 */

// ------------------------------------------------------------------
// Include required OS-specific headers and libraries
// ------------------------------------------------------------------
//
// NOTE: currently only Windows and Linux-based distros are supported.
//       contact me to add support for more platforms.
//

#ifndef __SERIALUART_H__
#define __SERIALUART_H__

#ifdef OS_WIN
// NOTE: used SDKDDKVer instead of WINVER to make it automatically defined on MinGW and Cygwin compilation.
//#define WINVER 0x0500
#include <SDKDDKVer.h>
#include <windows.h>
#endif

#ifdef OS_LINUX
#include <limits.h>   /* Used to derrive PATH_MAX length */
#include <errno.h>    /* Error number definitions */
#include <fcntl.h>    /* File control definitions */
#include <termios.h>  /* POSIX terminal control definitions */
#include <unistd.h>   /* UNIX standard function definitions */
#include <sys/ioctl.h>
#endif

// Platform independent headers
#include <stdio.h>    /* Standard input/output definitions */
#include <string.h>   /* String function definitions */
#include <stdlib.h>   /* */
#include <stdbool.h>  /* */
#include <time.h>     /* */

// TODO: include defined from here, for example #define EXIT_TIMEDOUT 124
// https://github.com/wertarbyte/coreutils/blob/master/src/timeout.c

/*
### DO NOT USE THIS
https://eklitzke.org/path-max-is-tricky
https://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html
#ifdef MAX_PATH
#define PATH_LENGTH MAX_PATH
#elif defined PATH_MAX
#define PATH_LENGTH PATH_MAX
#else
#error "No `MAX_PATH` or `PATH_MAX` defined."
#endif
*/

// Define C values
#ifndef EXIT_SUCCESS // Return code for a successful operation
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE // Return code for a failure operation
#define EXIT_FAILURE 1
#endif

#ifndef EXIT_TIMEDOUT // Return code for timeout
#define EXIT_TIMEDOUT 124
#endif

#endif
