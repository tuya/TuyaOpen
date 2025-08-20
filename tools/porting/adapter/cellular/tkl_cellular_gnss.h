/**
 * @file tkl_cellular_gnss.h
 * @brief
 * @version 0.1
 * @date 2022-09-15
 *
 * @copyright Copyright (c) 2021-2022 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */

#ifndef _TKL_CELLULAR_GNSS_H_
#define _TKL_CELLULAR_GNSS_H_
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TUYA_GNSS_NMEA_MAX_LENGTH  1823                 /**  NMEA string maximum length. 1023 ->1823*/

typedef uint32_t TUYA_CELLULAR_GNSS_DEV_HANDLE;
typedef enum{
    TY_CELL_GNSS_GPS_BDS = 0x00,
    TY_CELL_GNSS_GPS_GLONASS = 0x01,
    TY_CELL_GNSS_GPS = 0x02,
    TY_CELL_GNSS_BDS = 0x03,
    TY_CELL_GNSS_GLONASS = 0x04,
    TY_CELL_GNSS_GALILEO = 0x05,
}TUYA_CELLULAR_GNSS_SYS_MODE_E;

typedef enum
{
    TY_CELL_GNSS_STATUS_NONE            = 0, /**<  GPS status unknown.  */
    TY_CELL_GNSS_STATUS_SESSION_BEGIN   = 1, /**<  GPS has begun navigating.  */
    TY_CELL_GNSS_STATUS_SESSION_END     = 2, /**<  GPS has stopped navigating.  */
    TY_CELL_GNSS_STATUS_ENGINE_ON       = 3, /**<  GPS has powered on but is not navigating.  */
    TY_CELL_GNSS_STATUS_ENGINE_OFF      = 4, /**<  GPS is powered off.  */
}TUYA_CELLULAR_GNSS_STATUS_E;

typedef enum
{
    TY_CELL_GNSS_CAPABILITY_SCHEDULING      = 0x01, /**<  GPS HAL schedules fixes for GPS_POSITION_RECURRENCE_PERIODIC
mode.
                                                         If this is not set, the framework uses \n 1000 ms for
min_interval
                                                         and will call start() and stop() to schedule the GPS. */
    TY_CELL_GNSS_CAPABILITY_MSB             = 0x02, /**<  GPS supports MS-Based AGPS mode.  */
    TY_CELL_GNSS_CAPABILITY_MSA             = 0x04, /**<  GPS supports MS-Assisted AGPS mode.  */
    TY_CELL_GNSS_CAPABILITY_SINGLE_SHOT     = 0x08, /**<  GPS supports single-shot fixes.  */
    TY_CELL_GNSS_CAPABILITY_ON_DEMAND_TIME  = 0x10, /**<  GPS supports on-demand time injection.  */
}TUYA_CELLULAR_GNSS_CAPABILITY_T;

typedef enum
{
    TY_CELL_GNSS_NFY_MSG_ID_STATUS_INFO = 0,        /**<  pv_data = &TUYA_CELLULAR_GNSS_STATUS_E  */
    TY_CELL_GNSS_NFY_MSG_ID_LOCATION_INFO,          /**<  pv_data = &TUYA_CELLULAR_GNSS_LOCATION_INFO_T  */
    TY_CELL_GNSS_NFY_MSG_ID_SV_INFO,                /**<  pv_data = &TUYA_CELLULAR_GNSS_SV_STATUS_T  */
    TY_CELL_GNSS_NFY_MSG_ID_NMEA_INFO,              /**<  pv_data = &TUYA_CELLULAR_GNSS_NMEA_T  */
    TY_CELL_GNSS_NFY_MSG_ID_CAPABILITIES_INFO,      /**<  pv_data = &TUYA_CELLULAR_GNSS_CAPABILITY_T  */
    TY_CELL_GNSS_NFY_MSG_ID_AGPS_STATUS,            /**<  pv_data = &TUYA_CELLULAR_GNSS_AGPS_STATUS_T  */
    TY_CELL_GNSS_NFY_MSG_ID_NI_NOTIFICATION,
    TY_CELL_GNSS_NFY_MSG_ID_XTRA_REPORT_SERVER,     /**<  pv_data = &UYA_CELLULAR_GNSS_XRTA_REPT_SERVER_INFO_T  */
}TUYA_CELLULAR_GNSS_NOTIFY_MSG_ID_E;

typedef enum
{
    TY_CELL_GNSS_POS_MODE_STANDALONE        = 0, /**<  Mode for running GPS standalone (no assistance).  */
    TY_CELL_GNSS_POS_MODE_MS_BASED          = 1, /**<  AGPS MS-Based mode.  */
    TY_CELL_GNSS_POS_MODE_MS_ASSISTED       = 2, /**<  AGPS MS-Assisted mode.  */
}TUYA_CELLULAR_GNSS_POS_MODE_E;

#pragma pack(1)
#define     TUYA_CELLULAR_GNSS_MAX_SEVER_ADDR_LENGTH    255 /**  Maximum generic server address length for the host name. */
typedef struct
{/** Indication Message; Indication with the reported XTRA server URLs. */
    char server1[TUYA_CELLULAR_GNSS_MAX_SEVER_ADDR_LENGTH + 1];  /**<   server1.*/
    char server2[TUYA_CELLULAR_GNSS_MAX_SEVER_ADDR_LENGTH + 1];  /**<   server2.*/
    char server3[TUYA_CELLULAR_GNSS_MAX_SEVER_ADDR_LENGTH + 1];  /**<   server3.*/
}TUYA_CELLULAR_GNSS_XRTA_REPT_SERVER_INFO_T;


#define TY_CELL_GNSS_GPS_SSID_BUF_SIZE    32          /**  Maximum SSID (Service Set Identifier) buffer size. */
#define TY_CELL_GNSS_IPV6_ADDR_LEN        16          /**  IPv6 address length. */
typedef enum
{
    TY_CELL_GNSS_AGPS_TYPE_INVALID          = -1,   /**<  Invalid.  */
    TY_CELL_GNSS_AGPS_TYPE_ANY              = 0,    /**<  Any.  */
    TY_CELL_GNSS_AGPS_TYPE_SUPL             = 1,    /**<  SUPL.  */
    TY_CELL_GNSS_AGPS_TYPE_C2K              = 2,    /**<  C2K.  */
    TY_CELL_GNSS_AGPS_TYPE_WWAN_ANY         = 3,    /**<  WWAN any.  */
    TY_CELL_GNSS_AGPS_TYPE_WIFI             = 4,    /**<  Wi-Fi.  */
    TY_CELL_GNSS_AGPS_TYPE_SUPL_ES          = 5,    /**<  SUPL_ES.  */
}TY_CELL_GNSS_AGPS_TYPE_E;

typedef enum
{
    TY_CELL_GNSS_REQUEST_AGPS_DATA_CONN     = 1,    /**<  GPS requests a data connection for AGPS.  */
    TY_CELL_GNSS_RELEASE_AGPS_DATA_CONN     = 2,    /**<  GPS releases the AGPS data connection.  */
    TY_CELL_GNSS_AGPS_DATA_CONNECTED        = 3,    /**<  AGPS data connection is initiated  */
    TY_CELL_GNSS_AGPS_DATA_CONN_DONE        = 4,    /**<  AGPS data connection is completed.  */
    TY_CELL_GNSS_AGPS_DATA_CONN_FAILED      = 5,    /**<  AGPS data connection failed.  */
}TY_CELL_GNSS_AGPS_STATUS_E;

typedef struct
{
    uint32_t                        size;       /**<   Set to the size of mcm_agps_status_t. */
    TY_CELL_GNSS_AGPS_TYPE_E        type;       /**<   Type. */
    TY_CELL_GNSS_AGPS_STATUS_E      status;     /**<   Status. */
    int                             ipv4_addr;  /**<   IPv4 address. */
    char                            ipv6_addr[TY_CELL_GNSS_IPV6_ADDR_LEN + 1];        /**<   IPv6 address. */
    char                            ssid[TY_CELL_GNSS_GPS_SSID_BUF_SIZE + 1];         /**<   SSID. */
    char                            password[TY_CELL_GNSS_GPS_SSID_BUF_SIZE + 1];     /**<   Password. */
}TUYA_CELLULAR_GNSS_AGPS_STATUS_T;

typedef enum
{
    TY_CELL_GNSS_LOCATION_LAT_LONG_VALID   = 0x0001, /**<  GPS location has valid latitude and longitude.  */
    TY_CELL_GNSS_LOCATION_ALTITUDE_VALID   = 0x0002, /**<  GPS location has a valid altitude.  */
    TY_CELL_GNSS_LOCATION_SPEED_VALID      = 0x0004, /**<  GPS location has a valid speed.  */
    TY_CELL_GNSS_LOCATION_BEARING_VALID    = 0x0008, /**<  GPS location has a valid bearing.  */
    TY_CELL_GNSS_LOCATION_ACCURACY_VALID   = 0x0010, /**<  GPS location has valid accuracy.  */
    TY_CELL_GNSS_LOCATION_SOURCE_INFO_VALID= 0x0020, /**<  GPS location has valid source information.  */
    TY_CELL_GNSS_LOCATION_IS_INDOOR_VALID  = 0x0040, /**<  GPS location has a valid "is indoor?" flag.  */
    TY_CELL_GNSS_LOCATION_FLOOR_NUMBE_VALID= 0x0080, /**<  GPS location has a valid floor number.  */
    TY_CELL_GNSS_LOCATION_MAP_URL_VALID    = 0x0100, /**<  GPS location has a valid map URL.  */
    TY_CELL_GNSS_LOCATION_MAP_INDEX_VALID  = 0x0200, /**<  GPS location has a valid map index.  */
}TY_CELL_GNSS_LOACTION_VAILD_FLAG_E;

typedef struct
{
    int64_t     timestamp;                              /**<   System Timestamp, marked for when got the nmea data */
    char        nmea[TUYA_GNSS_NMEA_MAX_LENGTH + 1];    /**<   NMEA string.*/
    int         length;                                 /**<   NMEA string length. */
}TUYA_CELLULAR_GNSS_NMEA_T;  /* Message */

typedef struct
{
    int         flags;                    /**<   Contains GPS location flags bits. TY_CELL_GNSS_LOACTION_VAILD_FLAG_E */
    double      latitude;               /**<   Latitude in degrees. */
    double      longitude;              /**<   Longitude in degrees. */
    double      altitude;               /**<   Altitude in meters above the WGS 84 reference ellipsoid. */
    float       speed;                  /**<   Speed in meters per second. */
    float       bearing;                /**<   Heading in degrees. */
    float       accuracy;               /**<   Expected accuracy in meters. */
}TUYA_CELLULAR_GNSS_LOCATION_INFO_T;

typedef struct
{
    uint32_t    size;                   /**<   Set to the size of mcm_gps_sv_info_t. */
    int         prn;                    /**<   Pseudo-random number for the SV. */
    float       snr;                    /**<   Signal-to-noise ratio. */
    float       elevation;              /**<   Elevation of the SV in degrees. */
    float       azimuth;                /**<   Azimuth of the SV in degrees. */
}TUYA_CELLLULAR_GNSS_SV_INFO_T;                      /* Type */

#define     TUYA_GPS_SUPPORT_SVS_MAX   80  /**  Maximum number of satellites in view. */
typedef struct
{
    uint32_t                        size;                                   /**<   Set to the size of mcm_gps_sv_status_t. */
    int                             num_svs;                                /**<   Number of SVs currently visible. */
    TUYA_CELLLULAR_GNSS_SV_INFO_T    sv_list[TUYA_GPS_SUPPORT_SVS_MAX];    /**<   Contains an array of SV information. */
    uint32_t                         ephemeris_mask;                         /**<   Bitmask indicating which SVs have ephemerisdata.  */
    uint32_t                         almanac_mask;                           /**<   Bitmask indicating which SVs have almanac data
.   */
    uint32_t                         used_in_fix_mask;                       /**<   Bitmask indicating which SVs were used for
computing the most recent position fix. */
}TUYA_CELLULAR_GNSS_SV_STATUS_T; /* Type */

typedef struct
{
    int64_t time;               /**<   Inject time.*/
    int64_t time_reference;     /**<   Time reference.*/
    int32_t uncertainty;        /**<   Uncertainty.*/
}TUYA_CELLULAR_GNSS_INJECT_TIME_INFO_T;  /* Message */

typedef struct
{
    double  latitude;   /**<   Latitude.*/
    double  longitude;  /**<   Longitude.*/
    float   accuracy;   /**<   Accuracy.*/
}TUYA_CELLULAR_GNSS_INJECT_LOCATION_INFO_T;

typedef enum
{
    TY_CELL_GNSS_POS_RECURRENCE_PERIODIC    = 0, /**<  Receive GPS fixes on a recurring basis at a specified period.  */
    TY_CELL_GNSS_POS_RECURRENCE_SINGLE      = 1, /**<  Request a single-shot GPS fix.  */
}TUYA_CELLULAR_GNSS_POS_RECURRENCE_T;

typedef struct
{
  TUYA_CELLULAR_GNSS_POS_MODE_E       mode;               /*  Position mode.      */
  TUYA_CELLULAR_GNSS_POS_RECURRENCE_T recurrence;         /*  Recurrence          */
  uint32_t                  min_interval;       /*  Minimum Interval, NMEA report frequency, 1000 means 1Hz, 100 means 10Hz    */
  uint32_t                  preferred_accuracy; /*  Preferred Accuracy , 30m or more, the less it takes longer timer. */
  uint32_t                  preferred_time;     /*  Preferred Time, first cold-boot may take 100s or more, hot boot may take 2s      */
}TUYA_CELLULAR_GNSS_MODE_INFO_T;

#pragma pack()
typedef void (*TKL_GNSS_REGISTION_NOTIFY)(TUYA_CELLULAR_GNSS_NOTIFY_MSG_ID_E msg, void *data,int len);

/**
 * @brief Initialize cellular GNSS service
 * @param handle Return cellular GNSS device service handle
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_init(TUYA_CELLULAR_GNSS_DEV_HANDLE *handle);

/**
 * @brief Deinitialize cellular GNSS service
 * @param handle Cellular GNSS device service handle
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_deinit(TUYA_CELLULAR_GNSS_DEV_HANDLE handle);

/**
 * @brief Register cellular GNSS service message callback function
 * @param notify TKL_GNSS_REGISTION_NOTIFY callback function
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_register_notify(TKL_GNSS_REGISTION_NOTIFY notify);

/**
 * @brief Set cellular GNSS positioning mode
 * @param handle Cellular GNSS device service handle
 * @param pt_mode Positioning mode
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_set_position_mode(TUYA_CELLULAR_GNSS_DEV_HANDLE nohandletify, TUYA_CELLULAR_GNSS_MODE_INFO_T pt_mode);

/**
 * @brief Set cellular GNSS positioning system mode
 * @param handle Cellular GNSS device service handle
 * @param pt_mode Positioning mode
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_set_system_mode(TUYA_CELLULAR_GNSS_DEV_HANDLE nohandletify, TUYA_CELLULAR_GNSS_SYS_MODE_E pt_mode);

/**
 * @brief Start navigation
 * @param handle Cellular GNSS device service handle
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_start_navigation(TUYA_CELLULAR_GNSS_DEV_HANDLE handle);

/**
 * @brief Stop navigation
 * @param handle Cellular GNSS device service handle
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_stop_navigation(TUYA_CELLULAR_GNSS_DEV_HANDLE handle);

/**
 * @brief Get current location information
 * @param handle Cellular GNSS device service handle
 * @param location Location information
 * @param timeout_sec Timeout for getting location information
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_get_location(TUYA_CELLULAR_GNSS_DEV_HANDLE handle,TUYA_CELLULAR_GNSS_LOCATION_INFO_T *location,int timeout_sec);

/**
 * @brief Directly inject time data
 * @param handle Cellular GNSS device service handle
 * @param pt_info Time data
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_injecttime(TUYA_CELLULAR_GNSS_DEV_HANDLE handle,TUYA_CELLULAR_GNSS_INJECT_TIME_INFO_T *pt_info);

/**
 * @brief Directly inject location information
 * @param handle Cellular GNSS device service handle
 * @param pt_info Location data
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_injectlocation(TUYA_CELLULAR_GNSS_DEV_HANDLE handle, TUYA_CELLULAR_GNSS_INJECT_LOCATION_INFO_T *pt_info);

/**
 * @brief GNSS service custom interface
 * @param handle Cellular GNSS device service handle
 * @param cmd GNSS command
 * @param pdata Data pointer
 * @param datalen pdata size
 * @return 0 success, others failure
 */
OPERATE_RET tkl_cellular_gnss_ioctl(TUYA_CELLULAR_GNSS_DEV_HANDLE handle,int cmd,void *pdata,int datalen);

#ifdef __cplusplus
}
#endif
#endif