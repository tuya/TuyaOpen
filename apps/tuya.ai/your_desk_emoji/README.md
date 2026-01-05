English | [简体中文](./RAEDME_zh.md)

# your_desk_emoji

[your_desk_emoji](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_desk_emoji) is an intelligent desk emoji robot based on tuya.ai. It combines AI conversation, gesture recognition, servo control, emoji display, and weather information to create an interactive desk companion.

## Supported Features

1. **AI Intelligent Conversation**
   - Multiple conversation modes: button press, voice wake-up, free conversation
   - Support for different wake-up keywords
   - Real-time chat display on LCD screen

2. **Gesture Recognition**
   - Based on PAJ7620 sensor
   - Supports directional gestures (up, down, left, right, forward, backward)
   - Gesture-triggered servo movements and emoji expressions

3. **Servo Motor Control**
   - Dual servo control (vertical and horizontal movement)
   - Smooth movement animations
   - Interactive actions triggered by gestures and conversations

4. **Emoji Display System**
   - Multiple emoji expressions with animations
   - Blink animations for enhanced interaction
   - Real-time emoji switching based on user interactions

5. **Weather Clock Display**
   - Real-time weather information from Tuya Cloud
   - Time synchronization and display
   - Date and weather information on LCD

6. **Display Interface**
   - Multiple UI styles: WeChat-like, Chatbot, OLED
   - Real-time chat content display
   - Weather and time information display

7. **Network Configuration**
   - Bluetooth network configuration
   - WiFi connection management
   - Network status monitoring

## Hardware Dependencies

1. **Audio System**
   - Audio capture capability
   - Audio playback capability
   - Speaker control

2. **Sensor Interface**
   - I2C interface for PAJ7620 gesture sensor
   - GPIO for gesture sensor interrupt

3. **Motor Control**
   - PWM interface for servo motor control
   - Dual servo support (head and body movement)

4. **Display Interface**
   - LCD display for emoji animations and information
   - Support for different screen sizes and types

5. **User Interface**
   - Button for conversation control
   - LED indicator for status display

## Supported Hardware

| Model | Config File | Description | Reset Method |
| --- | --- | --- | --- |
| TUYA T5AI Core | TUYA_T5AI_CORE.config | T5AI Core development board with 13565LCD display | Restart 3 times (press RST button) |
| T5AI Mini (ST7735S) | app_default.config | T5AI Mini with ST7735S color LCD display | Restart 3 times (press RST button) |
| T5AI Mini (13565LCD) | T5AI_MINI_LCD.config | T5AI Mini with 13565LCD display | Restart 3 times (press RST button) |


## Compilation

1. **Select Configuration**: Choose the appropriate config file based on your hardware:
   - For TUYA T5AI Core with 13565LCD: Use `TUYA_T5AI_CORE.config`
   - For T5AI Mini with ST7735S LCD: Use `app_default.config`
   - For T5AI Mini with 13565LCD: Use `T5AI_MINI_LCD.config`

2. **Apply Configuration**: Run `tos.py config choice` command to select the current development board.

3. **Customize Settings**: If you need to modify the configuration, run `tos.py config menu` command to make changes.

4. **Build Project**: Run `tos.py build` command to compile the project.

## Configuration

### Default Configuration
- Free conversation mode, AEC disabled, no interruption support
- Wake-up keywords:
  - T5AI version: "你好涂鸦" (Hello Tuya)
  - ESP32 version: "你好小智" (Hello Xiaozhi)

### General Configuration

#### Conversation Modes

- **Press and Hold Mode**
  - `ENABLE_CHAT_MODE_KEY_PRESS_HOLD_SINGEL`: Press and hold button to speak, release after finishing

- **Button Trigger Mode**
  - `ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE`: Press button to enter/exit listening state with VAD detection

- **Wake-up Single Mode**
  - `ENABLE_CHAT_MODE_ASR_WAKEUP_SINGEL`: Say wake-up word to start single conversation

- **Wake-up Free Mode**
  - `ENABLE_CHAT_MODE_ASR_WAKEUP_FREE`: Say wake-up word for continuous conversation (30s timeout)

#### Wake-up Keywords
Available when using wake-up conversation modes:

| Macro | Type | Description |
| --- | --- | --- |
| `ENABLE_WAKEUP_KEYWORD_NIHAO_TUYA` | Boolean | Wake-up word: "你好涂鸦" |
| `ENABLE_WAKEUP_KEYWORD_NIHAO_XIAOZHI` | Boolean | Wake-up word: "你好小智" |
| `ENABLE_WAKEUP_KEYWORD_XIAOZHI_TONGXUE` | Boolean | Wake-up word: "小智同学" |
| `ENABLE_WAKEUP_KEYWORD_XIAOZHI_GUANJIA` | Boolean | Wake-up word: "小智管家" |

#### Audio Configuration

- **AEC Support**
  - `ENABLE_AEC`: Enable/disable Acoustic Echo Cancellation based on hardware capability

- **Speaker Enable Pin**
  - `SPEAKER_EN_PIN`: Pin number for speaker control (0-64)

- **Chat Button Pin**
  - `CHAT_BUTTON_PIN`: Pin number for conversation button (0-64)

- **Status LED Pin**
  - `CHAT_INDICATE_LED_PIN`: Pin number for status indicator LED (0-64)

#### Display Configuration

- **Enable Display**
  - `ENABLE_CHAT_DISPLAY`: Enable display functionality for boards with screens

- **UI Style Selection**
  - `ENABLE_GUI_WECHAT`: WeChat-like chat interface style
  - `ENABLE_GUI_CHATBOT`: Chatbot interface style
  - `ENABLE_GUI_OLED`: Scrolling text style for OLED screens

- **Streaming Text Display**
  - `ENABLE_GUI_STREAM_AI_TEXT`: Enable streaming display of AI responses

- **OLED Screen Type** (when OLED UI is selected)
  - `OLED_SSD1306_128X32`: OLED screen size 128x32
  - `OLED_SSD1306_128X64`: OLED screen size 128x64

## Project Structure

```
src/
├── app_chat_bot.c          # AI conversation management
├── app_gesture.c           # PAJ7620 gesture recognition
├── app_servo.c             # Servo motor control
├── app_weather.c           # Weather service and data
├── app_system_info.c       # System information display
├── tuya_main.c             # Main application entry
├── display/                # Display system
│   ├── app_display.c       # Display management
│   ├── ui/                 # UI components
│   │   ├── ui_emoji.c      # Emoji display
│   │   ├── ui_weather_clock.c # Weather clock UI
│   │   ├── ui_chatbot.c    # Chatbot UI
│   │   ├── ui_wechat.c     # WeChat-style UI
│   │   └── ui_oled.c       # OLED display UI
│   └── font/               # Font resources
└── media/                  # Media resources
```

## Usage

1. **Initial Setup**: Configure the development board and compile the project
2. **Network Configuration**: Use Bluetooth or button reset for WiFi setup
3. **Interaction**: Use voice wake-up or button press to start conversations
4. **Gesture Control**: Wave hands in different directions to trigger actions
5. **Weather Display**: View real-time weather and time information on the display

## Development Notes

- The project supports multiple conversation modes and can be configured based on hardware capabilities
- Gesture recognition requires PAJ7620 sensor connected via I2C
- Servo control supports dual-axis movement for head and body
- Weather information is fetched from Tuya Cloud every 30 minutes
- Display system supports multiple UI styles for different screen types