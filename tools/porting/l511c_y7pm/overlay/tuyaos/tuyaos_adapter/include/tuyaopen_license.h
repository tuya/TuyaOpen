#ifndef L511C_TUYAOPEN_LICENSE_H
#define L511C_TUYAOPEN_LICENSE_H

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int tuyaopen_license_write(const char *data, uint32_t data_len);
int tuyaopen_license_read(char **data, uint32_t *data_len);

#ifdef __cplusplus
}
#endif

#endif /* L511C_TUYAOPEN_LICENSE_H */
