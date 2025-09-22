# Speaker

## Introduction

A speaker is a common output device used to convert electrical signals into sound signals. This demo demonstrates how to read MP3 files from multiple audio sources and play them back, supporting the following three audio sources:

1. **Code Array**: MP3 files directly embedded in the code
2. **Internal Flash**: Reading from the device's internal flash memory
3. **SD Card**: Reading audio files from an external SD card

The program performs MP3 decoding and playback every 3 seconds. The MP3 audio files used should match the configured `spk_sample` sampling rate. You can use the https://convertio.co/ website for online audio format and frequency conversion.


## Usage

### 1. Using MP3 Files in Code

Modify the `MP3_FILE_SOURCE` macro as follows:

```c
#define MP3_FILE_SOURCE     USE_C_ARRAY
```

### 2. Using MP3 Files from Internal Flash

Modify the `MP3_FILE_SOURCE` macro as follows:

```c
#define MP3_FILE_SOURCE     USE_INTERNAL_FLASH
```

The system will read MP3 files from the internal flash path `/media/hello_tuya.mp3` for playback.

### 3. Using MP3 Files from SD Card

Modify the `MP3_FILE_SOURCE` macro as follows:

```c
#define MP3_FILE_SOURCE     USE_SD_CARD
```

**Steps for using SD card audio:**

1. **Prepare MP3 file**: Rename your MP3 audio file to `hello_tuya.mp3`
2. **Audio format requirements**: Ensure the MP3 file's sampling rate matches the `AUDIO_SAMPLE_RATE` set in the code (default is 16kHz)
3. **Insert SD card**: Copy the prepared MP3 file to the SD card root directory
4. **Insert into device**: Insert the SD card into the development board's SD card slot
5. **File path**: The system will automatically read the file from `/sdcard/hello_tuya.mp3` path

**Important Notes:**
- SD card must be properly formatted (FAT32 format recommended)
- MP3 file size should not exceed 80% of SD card capacity
- Ensure SD card is working properly with an intact file system
- If playback fails, check if the SD card is properly inserted and the file exists

### 4. Volume Control

The volume control function is:

```c
OPERATE_RET tkl_ao_set_vol(INT32_T card, TKL_AO_CHN_E chn, VOID *handle, INT32_T vol);
```

Modify parameters by setting `vol`. For example, configure 30% volume as follows:

```c
tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, 30);
```

### 5. Audio Format Conversion

Regardless of which audio source you use, you need to ensure the MP3 file's sampling rate matches the sampling rate set in the code. If your audio file format or sampling rate doesn't match, you can use the following methods for conversion:

**Online Conversion (Recommended):**
- Visit https://convertio.co/ website
- Upload your audio file
- Select MP3 as output format
- Set sampling rate to 16kHz (or according to the `AUDIO_SAMPLE_RATE` setting in code)
- Download the converted file

**Using FFmpeg Command Line Tool:**
```bash
# Convert to 16kHz sampling rate MP3 file
ffmpeg -i input.wav -ar 16000 -ac 1 -b:a 128k hello_tuya.mp3
```

**Parameter Description:**
- `-ar 16000`: Set sampling rate to 16kHz
- `-ac 1`: Set to mono channel (MONO)
- `-b:a 128k`: Set audio bitrate to 128kbps

## Troubleshooting

### SD Card Audio Playback Issues

If you encounter problems when using SD card audio, please troubleshoot following these steps:

1. **Check SD Card Format**
   - Ensure SD card uses FAT32 file system
   - Recommend using SD cards with capacity not exceeding 32GB

2. **Check File Name and Path**
   - Confirm MP3 file is named `hello_tuya.mp3`
   - File should be placed in SD card root directory, not in subfolders

3. **Check Hardware Connection**
   - Confirm SD card is properly inserted into device
   - Check if SD card slot has good contact

4. **Check Log Output**
   ```
   mount fs failed     // SD card mount failed
   mp3 file not exist! // MP3 file does not exist
   open mp3 file xxx failed! // File open failed
   ```

5. **Audio Format Check**
   - Confirm MP3 file is not corrupted and can play normally on computer
   - Check if sampling rate matches code settings

### Audio Quality Issues

- If audio playback has noise, try adjusting volume settings
- If playback speed is abnormal, check MP3 file sampling rate settings

