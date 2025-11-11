#ifndef __EXAMPLE_CAMERA_H__
#define __EXAMPLE_CAMERA_H__

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"

#include "tdl_display_manage.h"
#include "tdl_camera_manage.h"

OPERATE_RET __dma2d_init(void);
OPERATE_RET __display_init(void);
OPERATE_RET __camera_init(void);

#endif