
```markdown
English | [简体中文](./RAEDME_zh.md)

# your_robot_dog
 [your_robot_dog](./img/robot_dog.png)(https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_robot_dog) is ported from the TuyaOS `tuyaos_demo_ai_toy` project onto TuyaOpen’s `your_char_bot` baseline. It adds vivid robot-dog facial expression changes and servo-controlled actions, bringing an open-source LLM-powered smart chat robot dog. It captures audio through the microphone, performs speech recognition, and enables conversations, interactions, and teasing. You can also see emotional changes on the screen, along with interactive behaviors.

## Supported Features
1. AI smart conversation
2. Key wake-up / voice wake-up, turn-based conversations, supports voice interruption (hardware support required)
3. Expression display
4. Supports LCD to display chat content in real time; supports viewing chat content in real time in the app
5. Switch AI agent roles in real time in the app
6. Voice control for robot-dog behaviors

## Required Hardware Capabilities
1. Audio capture
2. Audio playback
2. Servo drive

## Supported Hardware
|  Model  | config | 
| --- | --- | 
| TUYA_T5AI_ROBOT_DOG | TUYA_T5AI_ROBOT_DOG.config | 

## Hardware Flashing Method
Prepare a CH340 and connect as follows:
CH340            TUYA_T5AI_ROBOT_DOG                                   
TX  -------------- RX0                                                   
RX  -------------- TX0                                                   
RST -------------- RST                                                   

To view the serial log:
CH340            TUYA_T5AI_ROBOT_DOG                                   
TX  -------------- RX_L                                                   
RX  -------------- TX_L                                                  
GND -------------- GND   
You must share a common ground, otherwise the log may be garbled.

## Build
1. Run `tos.py config choice` and select `TUYA_T5AI_ROBOT_DOG.config`.
2. If you need to modify the configuration, run `tos.py config menu` first.
3. Run `tos.py build` to build the project.

## Configuration

### Default Configuration
- Free chat mode, AEC disabled, interruption not supported
- Wake word:
	- T5AI version: 你好涂鸦

### General Configuration

- **Select chat mode**

	- Press-and-hold chat mode

		| Macro                                   | Type    | Description                                   |
		| -------------------------------------- | ------- | --------------------------------------------- |
		| ENABLE_CHAT_MODE_KEY_PRESS_HOLD_SINGEL | Boolean | Hold the button while speaking; release it after finishing one sentence. |

	- Button-triggered chat mode

		| Macro                               | Type    | Description                                                         |
		| ---------------------------------- | ------- | ------------------------------------------------------------------- |
		| ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE | Boolean | Press the button once to enter/exit listening. When listening, VAD detection is enabled and you can talk. |

	- Wake-word chat mode

		| Macro                               | Type    | Description                                                         |
		| ---------------------------------- | ------- | ------------------------------------------------------------------- |
		| ENABLE_CHAT_MODE_ASR_WAKEUP_SINGEL | Boolean | You must say the wake word to wake the device. After waking, the device enters listening mode and you can talk. Each wake-up allows only one round of conversation. To continue, wake it again with the wake word. |

	- Free chat mode

		| Macro                             | Type    | Description                                                         |
		| -------------------------------- | ------- | ------------------------------------------------------------------- |
		| ENABLE_CHAT_MODE_ASR_WAKEUP_FREE | Boolean | You must say the wake word to wake the device. After waking, the device enters listening mode and you can chat freely. If no sound is detected for 30 seconds, you need to wake it again. |

- **Wake word**
	This option only appears when the chat mode is set to **Wake-word chat** or **Free chat**.
	| Macro                               | Type    | Description         |
	| ---------------------------------- | ------- | ------------------- |
	| ENABLE_WAKEUP_KEYWORD_NIHAO_TUYA   | Boolean | Wake word: “你好涂鸦” |

- **AEC support**

	| Macro      | Type    | Description                                                         |
	| ---------- | ------- | ------------------------------------------------------------------- |
	| ENABLE_AEC | Boolean | Configure this based on whether the board hardware supports echo cancellation.<br />Enable it if echo cancellation is supported. **If the board does not support echo cancellation, you must disable this option, otherwise it will affect the wake-word chat feature**.<br />If disabled, voice interruption is not supported. |

- **Speaker enable pin**

	| Macro          | Type   | Description                                               |
	| ------------- | ------ | --------------------------------------------------------- |
	| SPEAKER_EN_PIN | Number | Controls whether the speaker is enabled; range: 0-64.      |

- **Chat button pin**

	| Macro           | Type   | Description                                 |
	| -------------- | ------ | ------------------------------------------- |
	| CHAT_BUTTON_PIN | Number | Button GPIO for chat control; range: 0-64.  |

- **Enable display**

	| Option                     | Description                                                  |
	| -------------------------  | ------------------------------------------------------------ |
	| enable the display module  | Enables display. Turn this on if the board has a screen.     |
	| enable the dog action      | Enables the robot-dog actions.                               |

### Display Configuration

These options appear only after display is enabled.

- **Select display UI style**

	| Option             | Type    | Description                                         |
	| ------------------ | ------- | --------------------------------------------------- |
	| Use Robot Dog ui   | Boolean | Default: shows the top status bar and dog expressions |

### File System Configuration
	Required: some robot-dog expression GIFs have been packed into a LittleFS image at `./src/display/emotion/fs/fs.bin`. You must flash it to the specified address in FLASH.
	If not configured, the system may access an invalid address (symptom: continuous reboot) or the dog expressions may be incomplete.

	Steps:
	Download the flashing tool BKFIL. BKFIL stands for Beken FLASH Image Loader, an official Armino flashing and configuration tool.
	Open it, select the flashing serial port in “Select COM port”, and set the path to `fs.bin` in “Bin file path”. (./img/BKFIL_1.png)
	Go to the “Config” page, and in the `fs.bin` row set the start address to `0x6cb000` and the file length to `0x100000`. (./img/BKFIL_2.png)
	Return to the main page and click Flash.

## Notes
	`your_robot_dog` is a ported project. Compared with the regular T5AI development board, the baseboard of `TUYA_T5AI_ROBOT_DOG` differs significantly.
	Music playback and camera features are not supported yet.


```
