# 表情动画生成器使用说明

## 概述
这个工具用于为桌面表情机器人生成160x80尺寸的表情动画GIF文件。

## 文件说明

### `emoji_generator_complete.py`
完整的表情动画生成器，包含所有基础表情和有趣表情的生成功能。

**功能特点：**
- 生成160x80尺寸的GIF动画
- 保持眼睛大小不变（60x60像素）
- 包含14种不同的表情动画
- 优化的动画速度（50ms每帧）

**包含的表情：**
- **基础表情**：happy, sad, anger, surprise, sleep, wakeup, left, right, center
- **有趣表情**：wink, heart_eyes, rolling, zigzag, rainbow

## 使用方法

### 生成表情动画
```bash
cd /home/luoben/TuyaOpen/apps/tuya.ai/your_desk_emoji
python3 emoji_generator_complete.py
```

### 编译项目
```bash
cd /home/luoben/TuyaOpen
tos.py build
```

## 技术参数

### 屏幕参数
- **屏幕尺寸**：160x80像素
- **眼睛尺寸**：60x60像素
- **眼睛间距**：15像素
- **圆角半径**：15像素

### 动画参数
- **帧率**：50ms每帧
- **循环**：无限循环
- **格式**：GIF动画

### 表情分类

#### 基础表情（9个）
1. **happy** - 开心表情（眯眼+弹跳+闪烁）
2. **sad** - 悲伤表情（向上三角形覆盖）
3. **anger** - 愤怒表情（对角三角形覆盖）
4. **surprise** - 惊讶表情（收缩动画）
5. **sleep** - 睡觉表情（闭眼+漂浮Z字）
6. **wakeup** - 醒来表情（从细线到正常）
7. **left** - 左看动画
8. **right** - 右看动画
9. **center** - 居中状态

#### 有趣表情（5个）
1. **wink** - 眨眼（单眼闭合）
2. **heart_eyes** - 爱心眼
3. **rolling** - 翻白眼（圆形旋转）
4. **zigzag** - 之字形移动
5. **rainbow** - 彩虹色变化

## 自定义修改

### 修改屏幕尺寸
在`emoji_generator_complete.py`中修改：
```python
SCREEN_WIDTH = 160  # 修改宽度
SCREEN_HEIGHT = 80  # 修改高度
```

### 修改眼睛大小
在`emoji_generator_complete.py`中修改：
```python
REF_EYE_HEIGHT = 60  # 修改眼睛高度
REF_EYE_WIDTH = 60   # 修改眼睛宽度
```

### 修改动画速度
在`save_gif`函数中修改：
```python
duration=50  # 修改帧间隔（毫秒）
```

### 添加新表情
1. 在`CompleteEmojiGenerator`类中添加新的生成函数
2. 在`animations`字典中添加新表情的映射

## 输出文件

### GIF文件
- 位置：`emoji_animations/`目录
- 格式：`{表情名}.gif`
- 尺寸：160x80像素

## 集成到项目

生成的GIF文件需要手动转换为C数组并集成到项目中：
1. 使用其他工具将GIF转换为C数组
2. 在`ui_emoji.c`中声明：`LV_IMG_DECLARE({表情名});`
3. 在`gif_emotion`数组中引用：`{&{表情名}, "{表情名}"}`
4. 通过手势或舵机控制触发显示

## 注意事项

1. **GIF文件大小**：生成的GIF文件会占用存储空间
2. **动画帧数**：帧数越多，文件越大，但动画越流畅
3. **屏幕尺寸**：确保屏幕尺寸设置正确
4. **眼睛参数**：眼睛大小和位置会影响动画效果

## 故障排除

### 生成问题
- 检查Python环境和PIL库是否正确安装
- 确认输出目录权限
- 验证屏幕尺寸设置

### 显示问题
- 检查屏幕尺寸设置
- 确认眼睛位置计算正确
- 验证动画帧数是否合理

### 性能问题
- 减少动画帧数
- 降低动画速度
- 优化GIF文件大小
