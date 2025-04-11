# 编码风格指南

本风格指南目的是鼓励 TuyaOpen 开发人员使用统一的编码风格，提高代码的可读性和可维护性。

TuyaOpen 编码 以 `Linux kernel coding style` [https://www.kernel.org/doc/html/latest/process/coding-style.html](https://www.kernel.org/doc/html/latest/process/coding-style.html) 为基础，对部分规范进行了调整或者删除。

## 目录与文件
目录与文件命名均采用小写字母，命名应能够正确反映对应内容的含义；有多个不同含义组合的命令以下划线  "_"  连接，建议目录与文件名不超过 3 个以上组合。

###  头文件
C 语言头文件为了避免多次重复包含，需要定义一个符号。这个符号的定义形式请采用如下的风格：

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

宏的命名方式和文件名保持一致，采用大写字母，以双下划线开头、结尾，所有非字母符号转换成下划线。

同时为了保证在 c 函数在 c++ 环境下正常使用，应在头文件使用 extern “c” 宏来保证函数声明的确定性。
```c
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
```

所有的头文件内容都必须放在宏保护区域内。

### 文件头描述信息

文件头部需要增加描述性注释信息，用于描述该文件的描述、版本、版权等内容。
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


## 函数

### 函数名
函数命名由小写字母和下划线组成，单词之间使用 "_" 连接。命令采用动宾结构的方式，如 `set_xxx`，get_xxx。

模块内部函数接口应以双下划线 “__” 开头，并建议以声明为 static；

```c
static void __function(void)
{
	...
｝
```

### 入参
如果函数入口参数是空，必须使用 void 作为入口参数。

### 返回值

- 函数必须指定返回值；

- 函数内对于有明确返回值的调用函数的返回值，需要进行判断，并进行异常处理；
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

- 函数返回值、名称应保持在同一行，如需要换行，则需要保持合理的对其方式。


### 函数声明
- 提供给外部使用的 API 接口，必须在相应的头文件中声明；

- 通过头文件引用方式获得函数的声明，而不是使用 extern 方式；尽量避免使用 extern 作用与函数。


### 注释

- 好的代码应该有自描述性，但是并不是每个coder都可以做到，建议代码应该包含丰富的注释，帮助我们记录、理解、跟踪代码。

- 对外的接口应该在头文件声明，并提供详细注释，详细描述函数功能、参数、返回值。

- 函数头的注释应当具有帮助理解函数用途、参数使用方式，返回值如何处理等，能够帮助使用者了解如何使用该函数，并了解该函数在某些特殊的场景下具有的问题和风险。
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
其中

1. @brief + 简述函数作用。在描述中，着重说明该函数的作用，每句话首字母大写，句尾加英文句号。
2. 函数说明。在上述简述中未能体现到的函数功能或作用的一些点，可以做解释说明，每句话首字母大写，句尾加英文句号。
3. @param + 以参数为主语 + be 动词 + 描述，说明参数的意义或来源。
4. @return + 枚举返回值 + 返回值的意思，若返回值为数据，则直接介绍数据的功能。
5. @warning + 函数使用注意要点。在函数使用时，描述需要注意的事项，如使用环境、使用方式等。每句话首字母大写，句尾加英文句号。

## 缩进
我们采用 K&R 缩进风格。

### 空格与括号
- 程序块采用缩进风格编写，每级缩进为 4 个空格，并保持一致。
- 关键字后加空格，左小括号后、右小括号前不加空格：
- 左大括号和条件、循环条件一行
- 右大括号独占一行 (do-while 和 if-else if除外)
```c
if (condition) {
    action();
}
```
### 循环
- for 循环遵循函数的缩进标准
```c
int i = 0;
for (i = 0; i < MAX; i++) { 
	do_something();
}
```

- do-while 语句
左边大括号紧跟 do 关键字，并且空一格；右边大括号和while关键字一行，并且空一格。
```c
do {
	body of do-loop
} while (condition);
```

### 条件判断
- 在一个条件、循环中超过一个语句的情况也同样需要使用括号：
```c
if (condition) {
    if (test) {
        do_something();
    }
}
```

- if-else if-else 语句
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
- case 单独占一行，并且让 case 与 switch 对齐。
- 每个 case 的执行体建议使用大括号保护，避免变量的作用域超出范围。
- 不要遗漏 default。
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


## 宏与枚举
定义常量的宏名称和枚举中的标签均使用大写字母，单词之间使用 "_" 连接。
```c
#define CONSTANT 0x12345
```

定义多个相关常量时，推荐使用枚举定义。

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

## 格式化代码
TuyaOpen 支持 clang-format 格式化代码，请安装 clang-format 14 及以上版本，可手动格式化或自动格式化当前修改的代码。

- 手工格式化:
```shell
$ clang-format -style=file -i <file>
```
其中 <file>  为待格式化文件。

- 自动格式化：

TuyaOpen 目录 tools/hooks/pre-commit 文件会在编译时自动复制至 .git/hooks 目录。

通过 `git commit` 命令提交代码时，pre-commit 命令会自动调用 clang-format 格式化当前已经通过 `git add` 命令添加的文件。
