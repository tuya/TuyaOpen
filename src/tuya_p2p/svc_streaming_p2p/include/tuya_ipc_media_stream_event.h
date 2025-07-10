#ifndef _TUYA_IPC_MEDIA_STREAM_EVENT_H_
#define _TUYA_IPC_MEDIA_STREAM_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/**************************enum define***************************************/

// Media stream related events
typedef enum {
    // Enum 0~29 reserved for hardware related events
    MEDIA_STREAM_NULL = 0,
    MEDIA_STREAM_SPEAKER_START = 1,
    MEDIA_STREAM_SPEAKER_STOP = 2,
    MEDIA_STREAM_DISPLAY_START = 3,
    MEDIA_STREAM_DISPLAY_STOP = 4,

    MEDIA_STREAM_LIVE_VIDEO_START = 30,
    MEDIA_STREAM_LIVE_VIDEO_STOP = 31,
    MEDIA_STREAM_LIVE_AUDIO_START = 32,
    MEDIA_STREAM_LIVE_AUDIO_STOP = 33,
    MEDIA_STREAM_LIVE_VIDEO_CLARITY_SET = 34,
    MEDIA_STREAM_LIVE_VIDEO_CLARITY_QUERY = 35, /* query clarity informations*/
    MEDIA_STREAM_LIVE_LOAD_ADJUST = 36,
    MEDIA_STREAM_PLAYBACK_LOAD_ADJUST = 37,
    MEDIA_STREAM_PLAYBACK_QUERY_MONTH_SIMPLIFY = 38, /* query storage info of month  */
    MEDIA_STREAM_PLAYBACK_QUERY_DAY_TS = 39,         /* query storage info of day */
    MEDIA_STREAM_PLAYBACK_START_TS = 40,             /* start playback */
    MEDIA_STREAM_PLAYBACK_PAUSE = 41,                /* pause playback */
    MEDIA_STREAM_PLAYBACK_RESUME = 42,               /* resume playback */
    MEDIA_STREAM_PLAYBACK_MUTE = 43,                 /* mute playback */
    MEDIA_STREAM_PLAYBACK_UNMUTE = 44,               /* unmute playback */
    MEDIA_STREAM_PLAYBACK_STOP = 45,                 /* stop playback */
    MEDIA_STREAM_PLAYBACK_SET_SPEED = 46,            /*set playback speed*/
    MEDIA_STREAM_ABILITY_QUERY = 47,                 /* query the alibity of audion video strraming */
    MEDIA_STREAM_DOWNLOAD_START = 48,                /* start to download */
    MEDIA_STREAM_DOWNLOAD_STOP = 49,                 /* abondoned */
    MEDIA_STREAM_DOWNLOAD_PAUSE = 50,
    MEDIA_STREAM_DOWNLOAD_RESUME = 51,
    MEDIA_STREAM_DOWNLOAD_CANCLE = 52,

    MEDIA_STREAM_PLAYBACK_QUERY_DAY_TS_WITH_ENCRYPT = 53, /* query storage info of day */
    MEDIA_STREAM_DOWNLOAD_START_WITH_ENCRYPT = 54,

    /*Related to interconnection*/
    MEDIA_STREAM_LIVE_VIDEO_SEND_START = 60,  // Remote requests to pull video stream
    MEDIA_STREAM_LIVE_VIDEO_SEND_STOP = 61,   // Remote requests to stop pulling video stream
    MEDIA_STREAM_LIVE_AUDIO_SEND_START = 62,  // Remote requests to pull audio stream
    MEDIA_STREAM_LIVE_AUDIO_SEND_STOP = 63,   // Remote requests to stop pulling audio stream
    MEDIA_STREAM_LIVE_VIDEO_SEND_PAUSE = 64,  // Remote pauses video sending
    MEDIA_STREAM_LIVE_VIDEO_SEND_RESUME = 65, // Remote resumes video sending

    MEDIA_STREAM_STREAMING_VIDEO_START = 100,
    MEDIA_STREAM_STREAMING_VIDEO_STOP = 101,

    MEDIA_STREAM_DOWNLOAD_IMAGE = 201,  /* download image */
    MEDIA_STREAM_PLAYBACK_DELETE = 202, /* delete video */
    MEDIA_STREAM_ALBUM_QUERY = 203,
    MEDIA_STREAM_ALBUM_DOWNLOAD_START = 204,
    MEDIA_STREAM_ALBUM_DOWNLOAD_CANCEL = 205,
    MEDIA_STREAM_ALBUM_DELETE = 206,
    MEDIA_STREAM_ALBUM_PLAY_CTRL = 207,

    // XVR related
    MEDIA_STREAM_VIDEO_START_GW = 300,             /**< Live video start, parameter is C2C_TRANS_CTRL_VIDEO_START*/
    MEDIA_STREAM_VIDEO_STOP_GW,                    /**< Live video stop, parameter is C2C_TRANS_CTRL_VIDEO_STOP*/
    MEDIA_STREAM_AUDIO_START_GW,                   /**< Live audio start, parameter is C2C_TRANS_CTRL_AUDIO_START*/
    MEDIA_STREAM_AUDIO_STOP_GW,                    /**< Live audio stop, parameter is C2C_TRANS_CTRL_AUDIO_STOP*/
    MEDIA_STREAM_VIDEO_CLARITY_SET_GW,             /**< Set video live clarity, parameter is*/
    MEDIA_STREAM_VIDEO_CLARITY_QUERY_GW,           /**< Query video live clarity, parameter is*/
    MEDIA_STREAM_LOAD_ADJUST_GW,                   /**< Live load change, parameter is*/
    MEDIA_STREAM_PLAYBACK_LOAD_ADJUST_GW,          /**< Start playback, parameter is*/
    MEDIA_STREAM_PLAYBACK_QUERY_MONTH_SIMPLIFY_GW, /* Query local video info by month, parameter is  */
    MEDIA_STREAM_PLAYBACK_QUERY_DAY_TS_GW,         /* Query local video info by day, parameter is  */

    MEDIA_STREAM_PLAYBACK_START_TS_GW, /* Start playback video, parameter is  */
    MEDIA_STREAM_PLAYBACK_PAUSE_GW,    /**< Pause playback video, parameter is  */
    MEDIA_STREAM_PLAYBACK_RESUME_GW,   /**< Resume playback video, parameter is  */
    MEDIA_STREAM_PLAYBACK_MUTE_GW,     /**< Mute, parameter is  */
    MEDIA_STREAM_PLAYBACK_UNMUTE_GW,   /**< Unmute, parameter is  */
    MEDIA_STREAM_PLAYBACK_STOP_GW,     /**< Stop playback video, parameter is  */

    MEDIA_STREAM_PLAYBACK_SPEED_GW,  /**< Set playback speed, parameter is  */
    MEDIA_STREAM_DOWNLOAD_START_GW,  /**< Download start*/
    MEDIA_STREAM_DOWNLOAD_PAUSE_GW,  /**< Download pause  */
    MEDIA_STREAM_DOWNLOAD_RESUME_GW, /**< Download resume*/
    MEDIA_STREAM_DOWNLOAD_CANCLE_GW, /**< Download stop*/

    MEDIA_STREAM_SPEAKER_START_GW,   /**< Start intercom, no parameters */
    MEDIA_STREAM_SPEAKER_STOP_GW,    /**< Stop intercom, no parameters */
    MEDIA_STREAM_ABILITY_QUERY_GW,   /**< Ability query C2C_MEDIA_STREAM_QUERY_FIXED_ABI_REQ*/
    MEDIA_STREAM_CONN_START_GW,      /**< Start connection */
    MEDIA_STREAM_PLAYBACK_DELETE_GW, /* delete video */

    // for page mode play back enum
    MEDIA_STREAM_PLAYBACK_QUERY_DAY_TS_PAGE_MODE,       /* query storage info of day with page id*/
    MEDIA_STREAM_PLAYBACK_QUERY_EVENT_DAY_TS_PAGE_MODE, /* query storage evnet info of day with page id */

} MEDIA_STREAM_EVENT_E;

typedef enum {
    TRANS_EVENT_SUCCESS = 0,             /* Return success */
    TRANS_EVENT_SPEAKER_ISUSED = 10,     /* Speaker already in use, different TRANSFER_SOURCE_TYPE_E */
    TRANS_EVENT_SPEAKER_REPSTART = 11,   /* Speaker repeatedly started, same TRANSFER_SOURCE_TYPE_E */
    TRANS_EVENT_SPEAKER_STOPFAILED = 12, /* Speaker stop failed*/
    TRANS_EVENT_SPEAKER_INVALID = 99
} TRANSFER_EVENT_RETURN_E;

typedef enum {
    TRANSFER_SOURCE_TYPE_P2P = 1,
    TRANSFER_SOURCE_TYPE_WEBRTC = 2,
    TRANSFER_SOURCE_TYPE_STREAMER = 3,
} TRANSFER_SOURCE_TYPE_E;

/**
 * \brief P2P online status
 * \enum TRANSFER_ONLINE_E
 */
typedef enum {
    TY_DEVICE_OFFLINE,
    TY_DEVICE_ONLINE,
} TRANSFER_ONLINE_E;

typedef enum {
    TY_CMD_QUERY_IPC_FIXED_ABILITY_TYPE_VIDEO = 0x1,   // if support video
    TY_CMD_QUERY_IPC_FIXED_ABILITY_TYPE_SPEAKER = 0x2, // if support speaker
    TY_CMD_QUERY_IPC_FIXED_ABILITY_TYPE_MIC = 0x4,     // is support MIC
} TY_CMD_QUERY_IPC_FIXED_ABILITY_TYPE;

// request, response
typedef struct tagC2CCmdQueryFixedAbility {
    unsigned int channel;
    unsigned int ability_mask; // ability is assigned by bit
} C2C_TRANS_QUERY_FIXED_ABI_REQ, C2C_TRANS_QUERY_FIXED_ABI_RESP;

typedef enum {
    TY_VIDEO_CLARITY_STANDARD = 0,
    TY_VIDEO_CLARITY_HIGH,
    TY_VIDEO_CLARITY_THIRD,
    TY_VIDEO_CLARITY_FOURTH,
    TY_VIDEO_CLARITY_MAX
} TRANSFER_VIDEO_CLARITY_TYPE_E;

/**************************struct define***************************************/
typedef INT_T (*MEDIA_STREAM_EVENT_CB)(IN CONST INT_T device, IN CONST INT_T channel,
                                       IN CONST MEDIA_STREAM_EVENT_E event, IN PVOID_T args);

typedef struct {
    TRANSFER_VIDEO_CLARITY_TYPE_E clarity;
    VOID *pReserved;
} C2C_TRANS_LIVE_CLARITY_PARAM_S;

typedef struct tagC2C_TRANS_CTRL_LIVE_VIDEO {
    unsigned int channel;
    unsigned int type; // Stream type
} C2C_TRANS_CTRL_VIDEO_START, C2C_TRANS_CTRL_VIDEO_STOP;

typedef struct tagC2C_TRANS_CTRL_LIVE_AUDIO {
    unsigned int channel;
} C2C_TRANS_CTRL_AUDIO_START, C2C_TRANS_CTRL_AUDIO_STOP;

typedef struct {
    UINT_T start_timestamp; /* start timestamp in second of playback */
    UINT_T end_timestamp;   /* end timestamp in second of playback */
} PLAYBACK_TIME_S;

typedef struct tagPLAY_BACK_ALARM_FRAGMENT {
    unsigned short video_type; ///< 0: Regular recording, 1: AOV recording
    unsigned short type;       ///< event type
    PLAYBACK_TIME_S time_sect;
} PLAY_BACK_ALARM_FRAGMENT;

typedef struct {
    unsigned int file_count;              // file count of the day
    PLAY_BACK_ALARM_FRAGMENT file_arr[0]; // play back file array
} PLAY_BACK_ALARM_INFO_ARR;

#pragma pack(4)
typedef struct tagPLAY_BACK_FILE_INFOS_WITH_ENCRYPT {
    unsigned short video_type; ///< 0: Regular recording, 1: AOV recording
    unsigned short type;       ///< event type
    char uuid[32];
    PLAYBACK_TIME_S time_sect;
    int encrypt;
    unsigned char key_hash[16];
} PLAY_BACK_FILE_INFOS_WITH_ENCRYPT;
#pragma pack()

typedef struct {
    unsigned int file_count;                       // file count of the day
    PLAY_BACK_FILE_INFOS_WITH_ENCRYPT file_arr[0]; // play back file array
} PLAY_BACK_ALARM_INFO_WITH_ENCRYPT_ARR;

typedef struct {
    unsigned int channel;
    unsigned int year;
    unsigned int month;
    unsigned int day;
} C2C_TRANS_QUERY_PB_DAY_INNER_REQ;

typedef struct {
    unsigned int channel;
    unsigned int year;
    unsigned int month;
    unsigned int day;
    PLAY_BACK_ALARM_INFO_ARR *alarm_arr;
    unsigned int ipcChan;
} C2C_TRANS_QUERY_PB_DAY_RESP;

typedef struct {
    unsigned int channel;
    unsigned int year;
    unsigned int month;
    unsigned int day;
    unsigned int ipcChan; //??? todo position
    int allow_encrypt;
    PLAY_BACK_ALARM_INFO_WITH_ENCRYPT_ARR *alarm_arr;
} C2C_TRANS_QUERY_PB_DAY_WITH_ENCRYPT_RESP;

// Playback data deletion by day request
typedef struct tagC2C_TRANS_CTRL_PB_DELDATA_BYDAY_REQ {
    unsigned int channel;
    unsigned int year;  // Year to delete
    unsigned int month; // Month to delete
    unsigned int day;   // Day to delete
} C2C_TRANS_CTRL_PB_DELDATA_BYDAY_REQ;

typedef struct tagC2C_TRANS_CTRL_PB_DOWNLOAD_IMAGE_S {
    unsigned int channel;
    PLAYBACK_TIME_S time_sect; // Start download time point
    char reserved[32];
    int result;             // Result, can extend error code TY_C2C_CMD_IO_CTRL_STATUS_CODE
    int image_fileLength;   // File length followed by h file content
    unsigned char *pBuffer; // File content
} C2C_TRANS_CTRL_PB_DOWNLOAD_IMAGE_PARAM_S;

// query playback data by month
typedef struct tagC2CCmdQueryPlaybackInfoByMonth {
    unsigned int channel;
    unsigned int year;
    unsigned int month;
    unsigned int day; // list days that have playback data. Use each bit for one day. For example day=26496=0110 0111
                      // 1000 0000 means day 7/8/9/19/13/14 have playback data.
    unsigned int ipcChan;
} C2C_TRANS_QUERY_PB_MONTH_REQ, C2C_TRANS_QUERY_PB_MONTH_RESP;

typedef struct tagC2CCmdQueryPlaybackInfoByMonthInner {
    unsigned int channel;
    unsigned int year;
    unsigned int month;
    unsigned int day; // list days that have playback data. Use each bit for one day. For example day=26496=0110 0111
                      // 1000 0000 means day 7/8/9/19/13/14 have playback data.
} C2C_TRANS_QUERY_PB_MONTH_INNER_REQ, C2C_TRANS_QUERY_PB_MONTH_INNER_RESP;

typedef struct tagC2C_TRANS_CTRL_PB_START {
    unsigned int channel;
    PLAYBACK_TIME_S time_sect;
    UINT_T playTime; /* the actual playback time, in second */
    TRANSFER_SOURCE_TYPE_E type;
    unsigned int reqId; /*  request ID, need by send frame api */
    int allow_encrypt;
} C2C_TRANS_CTRL_PB_START;

typedef struct tagC2C_TRANS_CTRL_PB_STOP {
    unsigned int channel;
} C2C_TRANS_CTRL_PB_STOP;

typedef struct tagC2C_TRANS_CTRL_PB_PAUSE {
    unsigned int channel;
} C2C_TRANS_CTRL_PB_PAUSE;

typedef struct tagC2C_TRANS_CTRL_PB_RESUME {
    unsigned int channel;
    unsigned int reqId;
} C2C_TRANS_CTRL_PB_RESUME;

typedef struct tagC2C_TRANS_CTRL_PB_MUTE {
    unsigned int channel;
} C2C_TRANS_CTRL_PB_MUTE;

typedef struct tagC2C_TRANS_CTRL_PB_UNMUTE {
    unsigned int channel;
    unsigned int reqId;
} C2C_TRANS_CTRL_PB_UNMUTE;

typedef struct tagC2C_TRANS_CTRL_PB_SET_SPEED {
    unsigned int channel;
    unsigned int speed;
    unsigned int reqId;
} C2C_TRANS_CTRL_PB_SET_SPEED;

/**
 * \brief network load change callback struct
 * \note NOT supported now
 */
typedef struct {
    INT_T client_index;
    INT_T curr_load_level; /**< 0:best 5:worst */
    INT_T new_load_level;  /**< 0:best 5:worst */

    VOID *pReserved;
} C2C_TRANS_PB_LOAD_PARAM_S;

typedef struct {
    INT_T client_index;
    INT_T curr_load_level; /**< 0:best 5:worst */
    INT_T new_load_level;  /**< 0:best 5:worst */

    VOID *pReserved;
} C2C_TRANS_LIVE_LOAD_PARAM_S;

typedef struct tagC2C_TRANS_CTRL_DL_START {
    unsigned int channel;
    unsigned int fileNum;
    unsigned int downloadStartTime;
    unsigned int downloadEndTime;
    PLAYBACK_TIME_S *pFileInfo;
} C2C_TRANS_CTRL_DL_START;

typedef struct tagC2C_TRANS_CTRL_DL_ENCRYPT_START {
    unsigned int channel;
    unsigned int fileNum;
    unsigned int downloadStartTime;
    unsigned int downloadEndTime;
    int allow_encrypt; // Whether to allow encrypted data
    int reqId;         // request ID, need by send frame api
    PLAYBACK_TIME_S *pFileInfo;
} C2C_TRANS_CTRL_DL_ENCRYPT_START;

typedef struct tagC2C_TRANS_CTRL_DL_STOP {
    unsigned int channel;
} C2C_TRANS_CTRL_DL_STOP, C2C_TRANS_CTRL_DL_PAUSE, C2C_TRANS_CTRL_DL_RESUME, C2C_TRANS_CTRL_DL_CANCLE;

typedef enum {
    TUYA_DOWNLOAD_VIDEO = 0,
    TUYA_DOWNLOAD_ALBUM,
    TUYA_DOWNLOAD_VIDEO_ALLOW_ENCRYPT,
    TUYA_DOWNLOAD_MAX,
} TUYA_DOWNLOAD_DATA_TYPE;

typedef struct {
    INT_T video_codec;
    UINT_T frame_rate;
    UINT_T video_width;
    UINT_T video_height;
} TRANSFER_IPC_VIDEO_INFO_S;

typedef struct {
    INT_T audio_codec;
    INT_T audio_sample;   // TUYA_AUDIO_SAMPLE_E
    INT_T audio_databits; // TUYA_AUDIO_DATABITS_E
    INT_T audio_channel;  // TUYA_AUDIO_CHANNEL_E
} TRANSFER_IPC_AUDIO_INFO_S;

typedef struct {
    INT_T encrypt;        // Whether to encrypt
    INT_T security_level; // Security level
    CHAR_T uuid[32];      // Device UUID
    BYTE_T iv[16];        // Encryption vector
} TRANSFER_MEDIA_ENCRYPT_INFO_T;

typedef struct {
    INT_T frame_type; // MEDIA_FRAME_TYPE_E
    BYTE_T *p_buf;
    UINT_T size;
    UINT64_T pts;
    UINT64_T timestamp;
    union {
        TRANSFER_IPC_VIDEO_INFO_S video;
        TRANSFER_IPC_AUDIO_INFO_S audio;
    } media;

    TRANSFER_MEDIA_ENCRYPT_INFO_T encrypt_info;
} TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T; // Used for playback

typedef struct {
    INT_T type; // MEDIA_FRAME_TYPE_E
    UINT_T size;
    UINT64_T timestamp;
    UINT64_T pts;
    union {
        TRANSFER_IPC_VIDEO_INFO_S video;
        TRANSFER_IPC_AUDIO_INFO_S audio;
    } media;
    TRANSFER_MEDIA_ENCRYPT_INFO_T encrypt_info;
} TUYA_DOWNLOAD_FRAME_HEAD_ENCRYPT_T;

typedef struct {
    INT_T type; // See MEDIA_FRAME_TYPE_E
    UINT_T size;
    UINT64_T timestamp;
    UINT64_T pts;
    union {
        TRANSFER_IPC_VIDEO_INFO_S video;
        TRANSFER_IPC_AUDIO_INFO_S audio;
    } media;
    TRANSFER_MEDIA_ENCRYPT_INFO_T encrypt_info;
    BYTE_T *p_buf; // frame data
} TUYA_ALBUM_PLAY_FRAME_T;

/***********************************album protocol ****************************************/
#define TUYA_ALBUM_APP_FILE_NAME_MAX_LEN (48)
#define IPC_SWEEPER_ROBOT                "ipc_sweeper_robot"
typedef struct {
    unsigned int channel; // Currently not needed, reserved
    char albumName[48];
    int fileLen;
    void *pIndexFile;
} C2C_QUERY_ALBUM_REQ; // Query request header
typedef struct tagC2C_ALBUM_INDEX_ITEM {
    int idx;           // Provided by device and guaranteed uniqueness
    char valid;        // 0 invalid, 1 valid
    char channel;      // 0  1 Channel number
    char type;         // 0 Reserved, 1 pic, 2 mp4, 3 panoramic image (folder), 4 binary file, 5 stream file
    char dir;          // 0 file 1 dir
    char filename[48]; // 123456789_1.mp4 123456789_1.jpg  xxx.xxx
    int createTime;    // File creation time
    short duration;    // Video file duration
    char reserved[18];
} C2C_ALBUM_INDEX_ITEM; // Index Item
typedef struct {
    unsigned int crc;
    int version; // Album function version (>2 supports online file playback)
    char magic[16];
    unsigned long long min_idx;
    unsigned long long max_idx;
    char reserved[512 - 44];
    int itemCount; // include invalid items
    C2C_ALBUM_INDEX_ITEM itemArr[0];
} C2C_ALBUM_INDEX_HEAD; // Query return: 520 = 8 + 512, index file header + item

typedef struct {
    unsigned int channel;   // Currently not needed for business, reserved
    int result;             // Query return result
    char reserved[512 - 4]; // Reserved, total 512
    int itemCount;          // include invalid items
    C2C_ALBUM_INDEX_ITEM itemArr[0];
} C2C_CMD_IO_CTRL_ALBUM_QUERY_RESP; // Query return: 520 = 8 + 512, index file header + item

typedef struct tagC2C_CMD_IO_CTRL_ALBUM_fileInfo {
    char filename[48]; // File name, without absolute path
} C2C_CMD_IO_CTRL_ALBUM_fileInfo;
typedef struct tagC2C_CMD_IO_CTRL_ALBUM_DOWNLOAD_START {
    unsigned int channel; // Currently unused, reserved
    int operation;        // See TY_CMD_IO_CTRL_DOWNLOAD_OP
    char albumName[48];
    int thumbnail;    // 0 Original image, 1 Thumbnail
    int fileTotalCnt; // max 50
    C2C_CMD_IO_CTRL_ALBUM_fileInfo pFileInfoArr[0];
} C2C_CMD_IO_CTRL_ALBUM_DOWNLOAD_START;
typedef struct tagC2C_ALBUM_DOWNLOAD_CANCEL {
    unsigned int channel; // Currently unused, reserved
    char albumName[48];
} C2C_ALBUM_DOWNLOAD_CANCEL;

typedef struct tagC2C_CMD_IO_CTRL_ALBUM_DELETE {
    unsigned int channel;
    char albumName[48];
    int fileNum; // -1 All, others: number of files
    char res[64];
    C2C_CMD_IO_CTRL_ALBUM_fileInfo pFileInfoArr[0];
} C2C_CMD_IO_CTRL_ALBUM_DELETE; // Delete files

typedef struct {
    int reqId;
    int fileIndex;         // start from 0
    int fileCnt;           // max 50
    char fileName[48];     // File name
    int packageSize;       // Actual data length of current file segment
    int fileSize;          // File size
    int fileEnd;           // File end flag, last segment 10KB
} C2C_DOWNLOAD_ALBUM_HEAD; // Download data header

typedef struct {
    unsigned int channel;
    int result;    // See TY_C2C_CMD_IO_CTRL_STATUS_CODE_E
    int operation; // See TY_CMD_IO_CTRL_ALBUM_PLAY_OP_E, after online file playback ends it becomes
                   // TY_CMD_IO_CTRL_ALBUM_PLAY_OVER
} C2C_CMD_IO_CTRL_ALBUM_PLAY_RESULT_RESP_T;

typedef enum {
    TY_CMD_IO_CTRL_ALBUM_PLAY_START = 0, // Start playback
    TY_CMD_IO_CTRL_ALBUM_PLAY_STOP,      // Stop
    TY_CMD_IO_CTRL_ALBUM_PLAY_PAUSE,     // Pause
    TY_CMD_IO_CTRL_ALBUM_PLAY_RESUME,    // Resume
    TY_CMD_IO_CTRL_ALBUM_PLAY_CANCEL,    // Cancel
    TY_CMD_IO_CTRL_ALBUM_PLAY_OVER,      // Playback ended, device SDK actively sends
} TY_CMD_IO_CTRL_ALBUM_PLAY_OP_E;

typedef struct {
    unsigned int channel;                              // Currently unused, reserved
    int operation;                                     // See TY_CMD_IO_CTRL_ALBUM_PLAY_OP_E
    int thumbnail;                                     // 0 Original image, 1 Thumbnail
    unsigned int start_time;                           // Start playback time, unit: s
    char album_name[TUYA_ALBUM_APP_FILE_NAME_MAX_LEN]; // Album name
    char file_name[TUYA_ALBUM_APP_FILE_NAME_MAX_LEN];  // File name to play, without absolute path
} C2C_CMD_IO_CTRL_ALBUM_PLAY_CTRL_REQ_T;

typedef struct {
    unsigned int channel; // Currently unused, reserved (multi-camera channel)
    int user_idx;
    int req_id;
    int operation;                                     // See TY_CMD_IO_CTRL_ALBUM_PLAY_OP_E
    int thumbnail;                                     // 0 Original image, 1 Thumbnail
    unsigned int start_time;                           // Start playback time, unit: s
    char album_name[TUYA_ALBUM_APP_FILE_NAME_MAX_LEN]; // Album name
    char file_name[TUYA_ALBUM_APP_FILE_NAME_MAX_LEN];  // File name to play, without absolute path
} C2C_CMD_IO_CTRL_ALBUM_PLAY_CTRL_T;

typedef enum {
    E_FILE_TYPE_2_APP_PANORAMA = 1, // Panoramic image
} FILE_TYPE_2_APP_E;
typedef struct {
    FILE_TYPE_2_APP_E fileType;
    int param; // For panoramic images, total number of sub-images
} TUYA_IPC_BRIEF_FILE_INFO_4_APP;

/**
 * \fn tuya_ipc_start_send_file_to_app
 * \brief start send file to app by p2p
 * \param[in] strBriefInfo: brief file infomation
 * \return handle , >=0 valid, -1 err
 */
OPERATE_RET tuya_ipc_start_send_file_to_app(IN CONST TUYA_IPC_BRIEF_FILE_INFO_4_APP *pStrBriefInfo);

/**
 * \fn tuya_ipc_stop_send_file_to_app
 * \brief stop send file to app by p2p
 * \param[in] handle
 * \return ret
 */
OPERATE_RET tuya_ipc_stop_send_file_to_app(IN CONST INT_T handle);

typedef struct {
    CHAR_T *fileName; // Maximum 48 bytes, if null, use SDK internal naming
    INT_T len;
    CHAR_T *buff;
} TUYA_IPC_FILE_INFO_4_APP;
/**
 * \fn tuya_ipc_send_file_to_app
 * \brief start send file to app by p2p
 * \param[in] handle: handle
 * \param[in] strfileInfo: file infomation
 * \param[in] timeOut_s: suggest 30s, 0 no_block (current not support),
 * \return ret
 */
OPERATE_RET tuya_ipc_send_file_to_app(IN CONST INT_T handle, IN CONST TUYA_IPC_FILE_INFO_4_APP *pStrfileInfo,
                                      IN CONST INT_T timeOut_s);

typedef enum {
    SWEEPER_ALBUM_FILE_TYPE_MIN = 0,
    SWEEPER_ALBUM_FILE_MAP = SWEEPER_ALBUM_FILE_TYPE_MIN, // map file
    SWEEPER_ALBUM_FILE_CLEAN_PATH = 1,
    SWEEPER_ALBUM_FILE_NAVPATH = 2,
    SWEEPER_ALBUM_FILE_TYPE_MAX = SWEEPER_ALBUM_FILE_NAVPATH,

    SWEEPER_ALBUM_STREAM_TYPE_MIN = 3,
    SWEEPER_ALBUM_STREAM_MAP =
        SWEEPER_ALBUM_STREAM_TYPE_MIN, // map stream , devcie should send map file to app continue
    SWEEPER_ALBUM_STREAM_CLEAN_PATH = 4,
    SWEEPER_ALBUM_STREAM_NAVPATH = 5,
    SWEEPER_ALBUM_STREAM_TYPE_MAX = SWEEPER_ALBUM_STREAM_NAVPATH,

    SWEEPER_ALBUM_FILE_ALL_TYPE_MAX = SWEEPER_ALBUM_STREAM_TYPE_MAX, // Maximum value 5
    SWEEPER_ALBUM_FILE_ALL_TYPE_COUNT,                               // Count 6
} SWEEPER_ALBUM_FILE_TYPE_E;

typedef enum {
    SWEEPER_TRANS_NULL,
    SWEEPER_TRANS_FILE,   // File transfer
    SWEEPER_TRANS_STREAM, // File stream transfer
} SWEEPER_TRANS_MODE_E;

// File transfer status
typedef enum {
    TY_DATA_TRANSFER_IDLE,
    TY_DATA_TRANSFER_START,
    TY_DATA_TRANSFER_PROCESS,
    TY_DATA_TRANSFER_END,
    TY_DATA_TRANSFER_ONCE,
    TY_DATA_TRANSFER_CANCEL,
    TY_DATA_TRANSFER_MAX
} TY_DATA_TRANSFER_STAT;

/***********************************album protocol end ****************************************/

/***********************************xvr  protocol start ****************************************/
typedef struct {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
    unsigned int year;  // Year to query
    unsigned int month; // Month to query
    unsigned int day;   // Day to query
} C2C_TRANS_QUERY_GW_PB_DAY_REQ;

typedef struct {
    unsigned int channel;
    unsigned int idx; // Session index
    unsigned int map_chan_index;
    ; // Channel bound in a session. Users can pass through transparently.
    char subdid[64];
    unsigned int year;                   // Year to query
    unsigned int month;                  // Month to query
    unsigned int day;                    // Day to query
    PLAY_BACK_ALARM_INFO_ARR *alarm_arr; // Query result returned by user
} C2C_TRANS_QUERY_GW_PB_DAY_RESP;

/**
UINT has 32 bits in total, each bit indicates whether the corresponding day has data, the rightmost bit represents day
0. For example, day = 26496 = B0110 0111 1000 0000 This indicates that days 7, 8, 9, 10, 13, 14 have playback data.
 */
// Query days with playback data by month request, response
typedef struct tagC2CCmdQueryGWPlaybackInfoByMonth {
    unsigned int channel;
    unsigned int idx;            // Session index
    unsigned int map_chan_index; // Channel bound in session. Users can pass through transparently.
    char subdid[64];
    unsigned int year;  // Year to query
    unsigned int month; // Month to query
    unsigned int day;   // Day with playback data
} C2C_TRANS_QUERY_GW_PB_MONTH_REQ, C2C_TRANS_QUERY_GW_PB_MONTH_RESP;

// request
// Playback-related operation structure
typedef struct tagC2C_TRANS_CTRL_GW_PB_START {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
    PLAYBACK_TIME_S time_sect;
    UINT_T playTime; /**< Actual playback start timestamp (in seconds) */
} C2C_TRANS_CTRL_GW_PB_START;

typedef struct tagC2C_TRANS_CTRL_GW_PB_STOP {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
} C2C_TRANS_CTRL_GW_PB_STOP;

typedef struct tagC2C_TRANS_CTRL_GW_PB_PAUSE {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
} C2C_TRANS_CTRL_GW_PB_PAUSE, C2C_TRANS_CTRL_GW_PB_RESUME;

typedef struct tagC2C_TRANS_CTRL_GW_PB_MUTE {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
} C2C_TRANS_CTRL_GW_PB_MUTE, C2C_TRANS_CTRL_GW_PB_UNMUTE;

// Capability set query C2C_CMD_QUERY_FIXED_ABILITY
// request, response
typedef struct tagC2CCmdQueryGWFixedAbility {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
    unsigned int ability_mask; // Capability result bit assignment
} C2C_TRANS_QUERY_GW_FIXED_ABI_REQ, C2C_TRANS_QUERY_GW_FIXED_ABI_RESP;

/**
 * \brief Parameter structure for requesting modification or querying clarity callback in live mode
 * \struct C2C_TRANS_LIVE_CLARITY_PARAM_S
 */
typedef struct {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
    TRANSFER_VIDEO_CLARITY_TYPE_E clarity; /**< Video clarity */
    VOID *pReserved;
} C2C_TRANS_LIVE_GW_CLARITY_PARAM_S;

// Preview-related operation structure
typedef struct tagC2C_TRANS_CTRL_GW_LIVE_VIDEO {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
} C2C_TRANS_CTRL_GW_VIDEO_START, C2C_TRANS_CTRL_GW_VIDEO_STOP;

typedef struct tagC2C_TRANS_CTRL_GW_LIVE_AUDIO {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
} C2C_TRANS_CTRL_GW_AUDIO_START, C2C_TRANS_CTRL_GW_AUDIO_STOP;

typedef struct tagC2C_TRANS_CTRL_GW_SPEAKER {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
} C2C_TRANS_CTRL_GW_SPEAKER_START, C2C_TRANS_CTRL_GW_SPEAKER_STOP;

typedef struct tagC2C_TRANS_CTRL_GW_DEV_CONN {
    unsigned int channel;
    unsigned int idx;
    char subdid[64];
} C2C_TRANS_CTRL_GW_DEV_CONN;

typedef struct tagC2C_TRANS_CTRL_PB_SET_SPEED_GW {
    char devid[64];
    unsigned int channel;
    unsigned int speed;
} C2C_TRANS_CTRL_PB_SET_SPEED_GW;

// Playback data deletion by day request
typedef struct tagC2C_TRANS_CTRL_PB_DELDATA_BYDAY_GW_REQ {
    char subdid[64];
    unsigned int channel;
    unsigned int year;  // Year to delete
    unsigned int month; // Month to delete
    unsigned int day;   // Day to delete
} C2C_TRANS_CTRL_PB_DELDATA_BYDAY_GW_REQ;

typedef struct tagC2C_CMD_PROTOCOL_VERSION {
    unsigned int version; // High bit main version number, low 16 bits sub-version number
} C2C_CMD_PROTOCOL_VERSION;
typedef struct {
    char subid[64];
    unsigned int channel;
    unsigned int year;
    unsigned int month;
    unsigned int day;
    int page_id;
} C2C_TRANS_QUERY_PB_DAY_V2_REQ, C2C_TRRANS_QUERY_EVENT_PB_DAY_REQ;
#pragma pack(4)
typedef struct {
    char subid[64];
    unsigned int channel;
    unsigned int year;
    unsigned int month;
    unsigned int day;
    int page_id;
    int total_cnt;
    int page_size;
    PLAY_BACK_ALARM_INFO_ARR *alarm_arr;
    int idx; // Session index
} C2C_TRANS_QUERY_PB_DAY_V2_RESP;
typedef struct {
    unsigned int start_timestamp; /* start timestamp in second of playback */
    unsigned int end_timestamp;
    unsigned short video_type; ///< 0: Regular recording, 1: AOV recording
    unsigned short type;       ///< event type
    char pic_id[20];
} C2C_PB_EVENT_INFO_S;
typedef struct {
    int version;
    int event_cnt;
    C2C_PB_EVENT_INFO_S event_info_arr[0];
} C2C_PB_EVENT_INFO_ARR_S;
typedef struct {
    char subid[64];
    unsigned int channel;
    unsigned int year;
    unsigned int month;
    unsigned int day;
    int page_id;
    int total_cnt;
    int page_size;
    C2C_PB_EVENT_INFO_ARR_S *event_arr;
    int idx;
} C2C_TRANS_QUERY_EVENT_PB_DAY_RESP;
#pragma pack()
typedef struct tagC2C_TRANS_CTRL_GW_DL_STOP {
    char devid[64];
    unsigned int channel;
} C2C_TRANS_CTRL_GW_DL_STOP, C2C_TRANS_CTRL_GW_DL_PAUSE, C2C_TRANS_CTRL_GW_DL_RESUME, C2C_TRANS_CTRL_GW_DL_CANCLE;
typedef struct tagC2C_TRANS_CTRL_GW_DL_START {
    char devid[64];
    unsigned int channel;
    unsigned int downloadStartTime;
    unsigned int downloadEndTime;
} C2C_TRANS_CTRL_GW_DL_START;
/***********************************xvr  protocol end ****************************************/

/**************************function define***************************************/

OPERATE_RET tuya_ipc_media_stream_register_event_cb(MEDIA_STREAM_EVENT_CB event_cb);

OPERATE_RET tuya_ipc_media_stream_event_call(INT_T device, INT_T channel, MEDIA_STREAM_EVENT_E event, PVOID_T args);

#ifdef __cplusplus
}
#endif

#endif /*_TUYA_IPC_MEDIA_STREAM_EVENT_H_*/
