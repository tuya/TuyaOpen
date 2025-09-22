# Speaker

## 简介

Speaker（扬声器）是一种常见的输出设备，用于将电信号转换为声音信号。本 demo 演示了如何从多种音频源读取 MP3 文件并进行播放，支持以下三种音频源：

1. **代码数组**：MP3 文件直接嵌入到代码中
2. **内部 Flash**：从设备内部 Flash 存储器读取
3. **SD 卡**：从外部 SD 卡读取音频文件

程序会每隔 3 秒进行一次 MP3 解码播放。使用的 MP3 音频文件应和设置的 `spk_sample` 采样率一致，可以使用 https://convertio.co/zh/ 网站在线进行音频格式和频率转换。


## 使用方法

### 1. 使用代码中的 MP3 文件

将 `MP3_FILE_SOURCE` 宏修改如下：

```c
#define MP3_FILE_SOURCE     USE_C_ARRAY
```

### 2. 使用内部 Flash 中的 MP3 文件

将 `MP3_FILE_SOURCE` 宏修改如下：

```c
#define MP3_FILE_SOURCE     USE_INTERNAL_FLASH
```

系统会从内部 Flash 的 `/media/hello_tuya.mp3` 路径读取 MP3 文件进行播放。

### 3. 使用 SD 卡中的 MP3 文件

将 `MP3_FILE_SOURCE` 宏修改如下：

```c
#define MP3_FILE_SOURCE     USE_SD_CARD
```

**使用 SD 卡音频的步骤：**

1. **准备 MP3 文件**：将您的 MP3 音频文件重命名为 `hello_tuya.mp3`
2. **音频格式要求**：确保 MP3 文件的采样率与代码中设置的 `AUDIO_SAMPLE_RATE` 一致（默认为 16kHz）
3. **插入 SD 卡**：将准备好的 MP3 文件复制到 SD 卡根目录
4. **插入设备**：将 SD 卡插入开发板的 SD 卡槽
5. **文件路径**：系统会自动从 `/sdcard/hello_tuya.mp3` 路径读取文件

**注意事项：**
- SD 卡必须正确格式化（建议使用 FAT32 格式）
- MP3 文件大小建议不超过 SD 卡容量的 80%
- 确保 SD 卡正常工作且文件系统完整
- 如果播放失败，请检查 SD 卡是否正确插入和文件是否存在

### 4. 声音调节

声音调节函数为：

```c
OPERATE_RET tkl_ao_set_vol(INT32_T card, TKL_AO_CHN_E chn, VOID *handle, INT32_T vol);
```

通过设置 `vol` 进行参数修改。如：30% 音量配置如下：

```c
tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, 30);
```

### 5. 音频格式转换

无论使用哪种音频源，都需要确保 MP3 文件的采样率与代码中设置的采样率一致。如果您的音频文件格式或采样率不匹配，可以使用以下方法进行转换：

**在线转换（推荐）：**
- 访问 https://convertio.co/zh/ 网站
- 上传您的音频文件
- 选择输出格式为 MP3
- 设置采样率为 16kHz（或根据代码中的 `AUDIO_SAMPLE_RATE` 设置）
- 下载转换后的文件

**参数说明：**
- 设置采样率为 16kHz
- 设置为单声道（MONO）

## 故障排除

### SD 卡音频播放问题

如果使用 SD 卡音频时遇到问题，请按以下步骤排查：

1. **检查 SD 卡格式**
   - 确保 SD 卡使用 FAT32 文件系统
   - 建议使用容量不超过 32GB 的 SD 卡

2. **检查文件名和路径**
   - 确认 MP3 文件名为 `hello_tuya.mp3`
   - 文件应放置在 SD 卡根目录，而非子文件夹中

3. **检查硬件连接**
   - 确认 SD 卡正确插入设备
   - 检查 SD 卡插槽接触是否良好

4. **查看日志输出**
   ```
   mount fs failed     // SD 卡挂载失败
   mp3 file not exist! // MP3 文件不存在
   open mp3 file xxx failed! // 文件打开失败
   ```

5. **音频格式检查**
   - 确认 MP3 文件未损坏，可在电脑上正常播放
   - 检查采样率是否与代码设置一致

### 音频质量问题

- 如果音频播放有杂音，尝试调整音量设置
- 如果播放速度不正常，检查 MP3 文件的采样率设置

