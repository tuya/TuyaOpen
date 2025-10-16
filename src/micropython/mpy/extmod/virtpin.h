/*
 * Minimal virtpin.h for T5AI port
 * This file provides virtual pin interface definitions
 */

#ifndef MICROPY_INCLUDED_EXTMOD_VIRTPIN_H
#define MICROPY_INCLUDED_EXTMOD_VIRTPIN_H

#include "py/obj.h"

// Virtual pin protocol interface
// These functions are not used in minimal build but need to be declared
mp_obj_t mp_virtual_pin_read(mp_obj_t pin);
void mp_virtual_pin_write(mp_obj_t pin, int value);

// Virtual pin protocol structure
typedef struct _mp_pin_p_t {
    mp_obj_t (*ioctl)(mp_obj_t obj, mp_uint_t request, uintptr_t arg, int *errcode);
} mp_pin_p_t;

#endif // MICROPY_INCLUDED_EXTMOD_VIRTPIN_H