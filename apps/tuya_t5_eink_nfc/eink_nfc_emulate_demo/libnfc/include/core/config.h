#ifndef CONFIG_H
#define CONFIG_H

// Platform capabilities
#define HAVE_STDINT_H   1
#define HAVE_STDLIB_H   1
#define HAVE_STRING_H   1
#define HAVE_STRINGS_H  1
#define HAVE_INTTYPES_H 1

// #define HAVE_CONFIG_H 1

// Logging
// #define LOG 1
// #define DEBUG 1

// Driver support - only enable UART driver
#define DRIVER_PN532_UART_ENABLED 1

// Package info
#define PACKAGE_NAME    "libnfc"
#define PACKAGE_VERSION "1.8.0-tuya"

// Disable features not needed for embedded
#undef HAVE_SYS_SELECT_H
#undef HAVE_LIBUSB
#undef CONFFILES

#endif // CONFIG_H
