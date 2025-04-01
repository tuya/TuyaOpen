# Coding Style Guide

The purpose of this style guide is to encourage TuyaOpen developers to use a consistent coding style, thereby improving the readability and maintainability of the code.

The TuyaOpen coding style is based on the `Linux kernel coding style` [https://www.kernel.org/doc/html/latest/process/coding-style.html](https://www.kernel.org/doc/html/latest/process/coding-style.html), with some adjustments or deletions to specific guidelines.

## 1. Directories and Files
Both directories and files should be named in lowercase, with names reflecting the content they represent. For commands combining multiple meanings, use an underscore "_" to connect them. It is recommended that directory and file names do not exceed three combined components.

### Header Files
C language header files should define a symbol to avoid multiple inclusions. The definition style should be as follows:

```c
#ifndef __TCP_TRANSPORTER_H__
#define __TCP_TRANSPORTER_H__

#ifdef __cplusplus
extern "C" {
#endif

...

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __TCP_TRANSPORTER_H__ */
```

Macro names should be consistent with file names, using uppercase letters, starting and ending with double underscores "__", and converting all non-alphabetic symbols to underscores.

To ensure that C functions work correctly in a C++ environment, use the extern "C" macro in header files to ensure the determinism of function declarations.
```c
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
```

All content in header files must be placed within macro protection areas.

### File Header Descriptions

File headers should include descriptive comments describing the file's description, version, copyright, etc.
```c
/**
 * @file tcp_transporter.h
 * @brief Header file for TCP transporter functions.
 *
 * This file declares the interface for creating and destroying TCP transporters
 * within the Tuya AI+IoT SDK. A TCP transporter is responsible for establishing
 * and managing TCP connections, enabling the transmission of data between Tuya
 * devices and the Tuya cloud platform or other networked services. The
 * functions provided here allow for the creation of a TCP transporter instance
 * and its subsequent destruction, facilitating clean resource management and
 * termination of TCP connections.
 *
 * The creation function initializes a new TCP transporter and returns a handle
 * for further operations, while the destruction function releases any resources
 * allocated to the TCP transporter and properly closes the TCP connection.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
```

## 2. Functions

### Function Names
Function names should be composed of lowercase letters and underscores, with words connected by "_". Commands should follow the subject-verb structure, such as `set_xxx`, `get_xxx`.

Module internal function interfaces should start with double underscores "__" and are recommended to be declared as static;
```c
static void __function(void)
{
	...
｝
```

### Parameters
If the function entry parameters are empty, void must be used as the entry parameter.

### Return Values

- Functions must specify return values;

- For functions with clear return values, the return values of called functions must be checked, and exceptions must be handled;
```c
static OPERATE_RET function(void)
{
    char *out = NULL;

    out = tal_malloc(128);
    if (NULL == out) {
        PR_ERR("tal_malloc Fails %d", len);
        return OPRT_MALLOC_FAILED;
    }
    
    ...
    
    return OPRT_OK;
}
```

- Return values and names should be kept on the same line, and if a line break is needed, it should maintain a reasonable alignment.


### Function Declaration
- API interfaces provided for external use must be declared in the corresponding header file;

- Function declarations can be obtained through header file references, rather than using the extern way; it is advisable to avoid using extern for functions.


### Comments

- Good code should be self-descriptive, but not every coder can achieve this. It is recommended that code should include rich comments to help us record, understand, and track the code.

- External interfaces should be declared in the header file and provide detailed comments, describing the function's functionality, parameters, and return values.

- Function header comments should help understand the function's purpose, parameter usage, and how to handle return values, helping users understand how to use the function and understand the issues and risks it may have in special scenarios.
```c
/**
 * @brief Controls the TLS transporter.
 *
 * This function is used to control the TLS transporter by sending different
 * commands.
 *
 * @param t The TLS transporter to control.
 * @param cmd The command to send.
 * @param args The arguments for the command.
 *
 * @return The result of the operation.
 */
```
Where:

1. @brief + Briefly describe the function's purpose. In the description, emphasize the function's purpose, capitalize the first letter of each sentence, and end with an English period.
2. Function description. Points about the function's functionality or purpose that were not covered in the brief description can be explained, with each sentence starting with a capital letter and ending with an English period.
3. @param + Use the parameter as the subject + be verb + describe, explaining the meaning or source of the parameter.
4. @return + Enumerate return values + describe the meaning of the return value. If the return value is data, directly introduce the function of the data.
5. @warning + Points to note when using the function. In the use of the function, describe the points to note, such as the use environment and use method. Each sentence starts with a capital letter and ends with an English period.

## 3. Indentation
We use the K&R indentation style.

### Spaces and Parentheses
- Program blocks should be written in indentation style, with each level of indentation being 4 spaces, and consistency should be maintained.
- Add a space after keywords, and do not add spaces after left parentheses and before right parentheses:
- Left braces and conditions, loop conditions should be on the same line
- Right braces should be on a separate line (except for do-while and if-else if)
```c
if (condition) {
    action();
}
```

### Loops
- For loops should follow the indentation standard of functions
```c
int i = 0;
for (i = 0; i < MAX; i++) { 
	do_something();
}
```

- do-while statements
The left brace should immediately follow the do keyword and be indented by one space; the right brace and the while keyword should be on the same line and be indented by one space.
```c
do {
	body of do-loop
} while (condition);
```

### Conditional Statements
- When more than one statement is in a condition or loop, parentheses should also be used:
```c
if (condition) {
    if (test) {
        do_something();
    }
}
```

- if-else if-else statements
```c
    if (x == y) {
        ..
    } else if (x > y) {
        ...
    } else {
        ....
    }
```

### switch-case
- Each case should be on its own line, and cases should be aligned with the switch.
- Each case's execution body should be protected with braces, to avoid variables exceeding their scope.
- Do not forget the default.
```c
switch (suffix) { 
case 'A': 
case 'a': 
	{
    	printf("a"); 
	}
	break; 
case 'B': 
case 'b': 
	{
    	printf("b");  
	}
	break; 
/* fall through */ 
default: 
    break; 
}
```

## 4. Macros and Enums
Macro names and labels in enums should all be in uppercase letters, with words connected by "_".
```c
#define CONSTANT 0x12345
```

When defining multiple related constants, it is recommended to use enum definitions.

```c
typedef enum {
    STATE_IDLE,
    STATE_START,
    STATE_DATA_LOAD,
    STATE_ENDPOINT_GET,
    STATE_ENDPOINT_UPDATE,
    STATE_TOKEN_PENDING,
    STATE_ACTIVATING,
    STATE_NETWORK_CHECK,
    STATE_NETWORK_RECONNECT,
    STATE_STARTUP_UPDATE,
    STATE_MQTT_CONNECT_START,
    STATE_MQTT_CONNECTING,
    STATE_MQTT_RECONNECT,
    STATE_MQTT_YIELD,
    STATE_RESTART,
    STATE_RESET,
    STATE_STOP,
    STATE_EXIT,
} tuya_run_state_t;
```

## 5. Code Formatting
TuyaOpen supports code formatting with clang-format. Please install clang-format 14 or above, and manually or automatically format the code currently modified.

- Manual formatting:
```shell
$ clang-format -style=file -i <file>
```
Where <file> is the file to be formatted.

- Automatic formatting:

The TuyaOpen directory tools/hooks/pre-commit file will be automatically copied to the .git/hooks directory during compilation.

The pre-commit command will automatically call clang-format to format the files that have already been added with the `git add` command when committing code with the `git commit` command.
