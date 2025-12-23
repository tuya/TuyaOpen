/**
 * @file platform_compat.c
 * @brief Platform compatibility layer implementation for embedded systems
 */

#include "platform_compat.h"

// pipe function stub (for pn532_uart abort mechanism)
// Returns 0 to indicate success, but doesn't actually create pipes
// Abort mechanism will not work, but basic NFC functionality is unaffected
int pipe(int pipefd[2])
{
    // Set dummy values to prevent undefined behavior
    if (pipefd) {
        pipefd[0] = -1;
        pipefd[1] = -1;
    }
    return 0; // Return success to allow initialization to continue
}
