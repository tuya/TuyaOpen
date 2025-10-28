# 使用方式
```python
# 单频测试
python audio_test.py --mic=dump_mic.pcm --ref=dump_ref.pcm --test_case 1k_single
# 白噪声测试
python audio_test.py --mic=dump_mic.pcm --ref=dump_ref.pcm --test_case white_noise
# 静音测试
python audio_test.py --mic=dump_mic.pcm --ref=dump_ref.pcm --test_case silence
```
## 参数
- mic1: 测试音频文件
- ref: 参考音频文件

## 测试指标说明
- dc_offset: DC偏移量（通过/不通过）
- coherence:  mic 和 ref 相关性（越接近1越好）
- noise_floor: 底噪
- clip_detect: 音频截幅检测
- thd: 总谐波失真（通过/不通过）
- delay_stability: 音频延迟稳定性


