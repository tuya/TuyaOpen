English | [简体中文](./RAEDME_zh.md)

# your_chat_bot_led_ctl

[your_chat_bot](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_chat_bot) is an open-source large model intelligent chatbot based on tuya.ai. It collects voice through a microphone, performs speech recognition, and enables conversation, interaction, and banter. You can also see real-time chat content on the screen.

**This version adds LED PWM control functionality**, allowing you to control LED lights through the cloud or local touch buttons.

**Note: Switching between TUYA AI V1.0 and V2.0 requires removing the device and clearing the data on the APP before use.**

## Supported Features

1. AI intelligent conversation
2. Button wake-up/Voice wake-up, turn-based dialogue, supports voice interruption (hardware support required)
3. Expression display
4. Supports LCD for displaying real-time chat content and supports viewing chat content in real-time on the APP side
5. Quick Bluetooth network connection to the router
6. Real-time switching of AI entity roles on the APP side
7. **LED PWM Control** - Control LED switch and brightness via cloud or local touch button

## LED PWM Control Features

### Cloud Control
- **Switch Control (DP ID: 20)**: Turn LED on/off through Tuya IoT cloud
- **Brightness Control (DP ID: 22)**: Adjust LED brightness (0-100%) through Tuya IoT cloud

### Local Touch Button Control
- **Short Press**: Toggle LED on/off
- **Long Press (400ms+)**: Increase brightness, automatically wraps to maximum when reaching the limit

### Technical Specifications
- **PWM Channel**: PWM5 (P9 pin)
- **PWM Frequency**: 100Hz (period: 10ms)
- **Duty Cycle Range**: 0-10000 (0% - 100%)
- **Duty Step**: 100 per adjustment
- **Touch Key Pin**: GPIO29
- **PWM Polarity**: Negative

## Hardware Dependencies

1. Audio capture
2. Audio playback
3. **PWM-capable GPIO pin (P9/PWM5)** for LED control
4. **GPIO pin (GPIO29)** for touch key control (optional, for local control)

## Supported Hardware

| Model | Description | Reset Method |
| --- | --- | --- |
| TUYA T5AI_Board Development Board | [https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj](https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) | Reset by restarting 3 times |
| TUYA T5AI_EVB Board | [https://oshwhub.com/flyingcys/t5ai_evb](https://oshwhub.com/flyingcys/t5ai_evb) | Reset by restarting 3 times |

## Compilation

1. Run the `tos config_choice` command to select the current development board in use.
2. If you need to modify the configuration, run the `tos menuconfig` command to make changes.
3. Run the `tos build` command to compile the project.

## Configuration

### LED PWM Configuration

You can modify the LED control parameters in `src/pwm_led_ctrl.c`:

| Parameter | Default Value | Description |
| --- | --- | --- |
| `LED_PWM_VERTICAL` | `TUYA_PWM_NUM_5` | PWM channel used for LED control |
| `LED_PWM_FREQ` | `100` | PWM frequency in Hz |
| `TOUCHE_KEY_PIN` | `TUYA_GPIO_NUM_29` | GPIO pin for touch key |
| `LONG_KEY_TIME` | `400` | Long press detection time in milliseconds |
| `PWM_DUTY_STEP` | `100` | Brightness adjustment step |

### Data Point IDs

The LED control uses the following data point IDs (defined in `include/pwm_led_ctrl.h`):

- **Switch Control**: DP ID 20 (BOOL type)
- **Brightness Control**: DP ID 22 (VALUE type, range: 0-100)

### Usage

1. **Cloud Control**: Use the Tuya Smart app or cloud API to send commands:
   - Switch: Send boolean value to DP ID 20 (true = on, false = off)
   - Brightness: Send integer value (0-100) to DP ID 22

2. **Local Touch Button Control**:
   - Short press the touch button (GPIO29) to toggle LED on/off
   - Long press (>400ms) to increase brightness step by step

3. **Initialization**: Call `app_led_contral_task()` during device initialization to start LED PWM control and touch key monitoring.

## File Structure

```
your_chat_bot_led_ctl/
├── src/
│   ├── pwm_led_ctrl.c      # LED PWM control implementation
│   └── tuya_main.c         # Main application with DP handlers
├── include/
│   └── pwm_led_ctrl.h      # LED control API definitions
└── README.md               # This file
```

## API Reference

### `int app_set_led_onoff(bool led_state)`
Set LED on or off.

**Parameters:**
- `led_state`: `true` to turn on, `false` to turn off

**Returns:** Operation result code

### `int app_set_led_brightness(INT_T value)`
Set LED brightness.

**Parameters:**
- `value`: Brightness value (0-100)

**Returns:** Operation result code

### `OPERATE_RET app_led_contral_task(VOID)`
Initialize LED PWM control and touch key monitoring. This function should be called during device initialization.

**Returns:** `OPRT_OK` on success

## Troubleshooting

1. **LED not responding**: Check if PWM channel and GPIO pins are correctly configured for your hardware
2. **Touch key not working**: Verify GPIO29 is available and not used by other peripherals
3. **Brightness control issues**: Ensure PWM frequency and duty cycle values are within hardware limits

## License

See LICENSE file in the repository root.
