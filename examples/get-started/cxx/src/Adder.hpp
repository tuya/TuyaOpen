/**
 * @file Adder.h
 * @brief Declaration of the Adder class and the func_adder function.
 *
 * This file contains the declaration of the Adder class, which provides a
 * simple addition operation, and the func_adder function prototype. The Adder
 * class includes a default constructor and a method named add, which takes
 * two integer parameters and returns their sum. The func_adder function is
 * declared with C linkage to ensure compatibility with other parts of the
 * system that may be written in C.
 *
 * @note This example is designed for educational purposes and may need to be
 * adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __ADDER_H__
#define __ADDER_H__

class Adder {
public:
    Adder();
    int add(int a, int b);
};

extern "C" {
    void func_adder(int cnt);
}

#endif // __ADDER_H__
