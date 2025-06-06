# Speaker

## Introduction

A speaker is a common output device used to convert electrical signals into sound signals. This demo demonstrates how to read MP3 files from code arrays and internal flash, and then play them every 3 seconds. The MP3 audio file used should match the `spk_sample` setting. You can use the website [https://convertio.co/zh/](https://convertio.co/zh/) to convert audio formats and frequencies online.

## Usage

### 1. Using the MP3 File in Code

Modify the macro `MP3_FILE_SOURCE` as follows:

```c
#define MP3_FILE_SOURCE     USE_C_ARRAY
```

### 2. Volume Adjustment

The volume adjustment function is:

```c
OPERATE_RET tkl_ao_set_vol(INT32_T card, TKL_AO_CHN_E chn, VOID *handle, INT32_T vol);
```

Modify the parameter `vol` to adjust the volume. For example, setting the volume to 30% is done as follows:

```c
tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, 30);
```