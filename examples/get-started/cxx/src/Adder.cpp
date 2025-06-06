/**
 * @file Adder.cpp
 * @brief Implementation of the Adder class and the func_adder function.
 *
 * This file contains the implementation of the Adder class, which provides a
 * simple addition operation, and the func_adder function, which demonstrates
 * the usage of the Adder class to perform addition and print the result using
 * the Tuya Abstract Layer (TAL) logging system. The func_adder function is
 * exposed with C linkage to ensure compatibility with other parts of the system
 * that may be written in C.
 *
 * The Adder class includes a default constructor and a method named add, which
 * takes two integer parameters and returns their sum. An instance of the Adder
 * class named adder is created and used within the func_adder function.
 *
 * @note This example is designed for educational purposes and may need to be
 * adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "Adder.hpp"

#include <tal_log.h>

Adder::Adder() {}

int Adder::add(int a, int b)
{
    return a + b;
}

Adder adder;

extern "C" {
void func_adder(int cnt)
{
    static int a = 1;
    int sum = adder.add(a, cnt);
    PR_INFO("Adder: %d + %d = %d", a, cnt, sum);
}
}
