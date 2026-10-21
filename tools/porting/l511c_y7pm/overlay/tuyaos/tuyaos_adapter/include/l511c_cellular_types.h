#ifndef L511C_CELLULAR_TYPES_H
#define L511C_CELLULAR_TYPES_H

#include <stdint.h>

/* The current generic TAL cellular config carries these opaque vendor fields,
 * but the L511C on-chip modem does not consume either value. Keep their ABI
 * sized as the vendor's 32-bit enum fields without inventing interface values.
 */
typedef uint32_t TUYA_CELLULAR_IF_E;
typedef uint32_t TUYA_CELLULAR_PROTOCOL_E;

#endif /* L511C_CELLULAR_TYPES_H */
