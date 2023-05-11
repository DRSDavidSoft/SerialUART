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

#ifndef __SERIALUART_H__
#define __SERIALUART_H__

// Detect which platform we're currently running on
#include "os_macros.h"

// Include required OS-specific headers and libraries
#ifdef OS_WIN
// Use SDKDDKVer instead of WINVER to make it automatically defined on MinGW and Cygwin compilation.
#include <SDKDDKVer.h>
#include <windows.h>
#endif

#ifdef OS_POSIX
// Use POSIX terminal control definitions for Linux-based distros
#include <errno.h>    // Error number definitions
#include <fcntl.h>    // File control definitions
#include <termios.h>  // POSIX terminal control definitions
#include <unistd.h>   // UNIX standard function definitions
#include <sys/ioctl.h>
#endif

// Include standard C libraries and headers
#include <stdio.h>    // Standard input/output definitions
#include <string.h>   // String function definitions
#include <stdlib.h>   // Standard library functions
#include <stdbool.h>  // Boolean type and values
#include <time.h>     // Time and date functions

// Define C values
#ifndef EXIT_SUCCESS // Return code for a successful operation
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE // Return code for a failure operation
#define EXIT_FAILURE 1
#endif

// Define the maximum length of a file path using PATH_MAX macro if available
#ifdef PATH_MAX
#define PATH_LENGTH PATH_MAX
#else
#warning "No `PATH_MAX` defined. Using a default value of 4096."
#define PATH_LENGTH 4096
#endif

// TODO: include defined from here, for example #define EXIT_TIMEDOUT 124
// https://github.com/wertarbyte/coreutils/blob/master/src/timeout.c

#endif // __SERIALUART_H__
