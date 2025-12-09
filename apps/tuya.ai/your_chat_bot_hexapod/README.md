English | [简体中文](./README_zh.md)

# AI Chat Hexapod Robot

![](image/image.png)

An AI-powered hexapod robot project based on T5-Core development board, combining Tuya AI chatbot with an 18-servo hexapod robot for voice-controlled robot movement.

## 📋 Project Overview

This project is an extended version of [your_chat_bot](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_chat_bot), adding hexapod robot motion control capabilities to the original AI intelligent dialogue functionality. Control the hexapod robot to perform actions like walking, turning, dancing, and waving through voice or APP.

### Key Features

- 🤖 **AI Intelligent Dialogue**: Supports voice recognition and intelligent conversation
- 🦿 **Hexapod Motion Control**: Supports multiple motion patterns (forward, backward, turn left, turn right, dance, wave, stand, sit)
- 📱 **APP Remote Control**: Real-time robot control via Tuya Smart APP
- 🎤 **Voice Wake-up**: Supports voice wake-up and button wake-up
- 🔊 **Voice Playback**: AI responses played through speaker

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Tuya Smart APP                          │
│           (Motion Control / AI Role Switch / Volume)         │
└───────────────────────────┬─────────────────────────────────┘
                            │ MQTT
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                    T5-Core Development Board                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │  AI Chat    │  │  Audio      │  │  Hexapod Motion     │  │
│  │  Module     │  │  Processing │  │  Control Module     │  │
│  └─────────────┘  └─────────────┘  └──────────┬──────────┘  │
│                                               │              │
└───────────────────────────────────────────────┼──────────────┘
                                                │ UART (115200)
                                                ▼
┌─────────────────────────────────────────────────────────────┐
│                    Bus Servo Control Board                   │
│                    (18-Channel Servo Control)                │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      Hexapod Robot                           │
│                   (18 Bus Servos)                            │
└─────────────────────────────────────────────────────────────┘
```

## 🔧 Hardware Requirements

### Required Hardware

| Hardware | Description |
| --- | --- |
| T5-Core Dev Board | [T5-E1-IPEX Documentation](https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) |
| Hexapod Robot Kit | 18-servo bus servo hexapod robot |
| Bus Servo Controller | Controller supporting bus servo protocol |
| Microphone | For voice capture |
| Speaker | For voice playback |

### Wiring Guide

| T5-Core Pin | Connected Device | Description |
| --- | --- | --- |
| UART0 TX | Servo Controller RX | Serial transmit |
| UART0 RX | Servo Controller TX | Serial receive |
| GND | Servo Controller GND | Common ground |

> ⚠️ **Note**: The servo controller requires independent power supply. 18 servos working simultaneously need substantial current.

## 📐 Motion Posture Data Generation

The motion posture data (`move_ctrl.h`) in this project is generated using the **hexapod-irl** hexapod robot control system.

### Posture Generation Tool

- **Project URL**: [hexapod-irl (Bus Servo Adapted Version)](https://github.com/robeortZ/hexapod-irl)
- **Original Project**: [Mithi's hexapod-irl](https://github.com/mithi/hexapod-irl)

### Generation Flow

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  React Frontend │ --> │  Gait Algorithm │ --> │  Servo Angle    │
│  (3D Visualize) │     │  (Kinematics)   │     │  (move_ctrl.h)  │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

### Angle System

The bus servos in this project have an angle range of **0-270 degrees**, with the default center position at **135 degrees** (corresponding to PWM value 1500).

**Angle to PWM Mapping**:
- PWM 500 → 0 degrees
- PWM 1500 → 135 degrees (center position)
- PWM 2500 → 270 degrees

### Servo Numbering Layout

The hexapod robot has 6 legs, each with 3 servos (hip, thigh, shin), totaling 18 servos:

```
        Front
    ┌─────────┐
  1-3         4-6
    │  Body   │
  7-9         10-12
    │         │
 13-15       16-18
    └─────────┘
```

For detailed instructions, see: [六足运动姿态生成使用说明.md](./六足运动姿态生成使用说明.md)

## 🦿 Supported Motion Actions

| Action | Enum Value | Description |
| --- | --- | --- |
| Idle | `MOVE_STATE_IDLE` | Standby state |
| Forward | `MOVE_STATE_FORWARD` | Walk forward |
| Backward | `MOVE_STATE_BACKWARD` | Walk backward |
| Turn Left | `MOVE_STATE_TURN_LEFT` | Turn left in place |
| Turn Right | `MOVE_STATE_TURN_RIGHT` | Turn right in place |
| Dance | `MOVE_STATE_DANCE` | Dance motion |
| Wave | `MOVE_STATE_SHAKE_HANDS` | Waving motion |
| Stand | `MOVE_STATE_STAND` | Standing posture |
| Sit | `MOVE_STATE_SIT` | Sitting posture |
| Reset | `MOVE_STATE_RESET` | Return to initial posture |

## 📡 Servo Control Protocol

### Single Servo Control Command

```
#<ID>P<PWM>T<TIME>!

Parameters:
- ID: Servo number (001-018)
- PWM: PWM value (0500-2500)
- TIME: Execution time (0001-9999ms)

Example: #001P1500T0100!  // Control servo 1 to position 1500, execution time 100ms
```

### Multi-Servo Synchronous Control

```
{#001P1500T0100!#002P1500T0100!...#018P1500T0100!}

Use curly braces to wrap multiple servo commands for synchronous control
```

### Read Servo Status

```
#<ID>PRAD!

Return format: #<ID>P<PWM>!
```

### Release All Servos

```
#255PULK!
```

## 📦 Build and Flash

### 1. Environment Setup

Ensure TuyaOpen development environment is installed. See [TuyaOpen Documentation](https://github.com/tuya/TuyaOpen).

### 2. Configuration Selection

```bash
# Navigate to project directory
cd apps/tuya.ai/your_chat_bot_hexapod

# Select configuration (choose based on actual hardware)
tos.py config choice

# Modify configuration (optional)
tos.py config menu
```

### 3. Build

```bash
tos.py build
```

### 4. Flash

```bash
tos.py flash
```

## 📱 APP Control

### DP Point Description

| DP ID | Name | Type | Description |
| --- | --- | --- | --- |
| 3 | Volume | Value | Volume control (0-100) |
| 8 | Motion Status | Enum | Control robot motion actions |
| 101 | Motion Steps | Value | Set motion execution steps |

### Motion Status Enum Values

```
0 - Idle (none)
1 - Forward (forward)
2 - Backward (backward)
3 - Turn Left (left)
4 - Turn Right (right)
5 - Dance (dance)
6 - Wave (handshake)
7 - Stand (stand)
8 - Sit (sit)
9 - Reset (reset)
```

## 📁 Project Structure

```
your_chat_bot_hexapod/
├── src/                        # Main source code
│   ├── tuya_main.c            # Main entry
│   └── ...
├── robot_ctl_src/              # Hexapod robot control source
│   ├── uart_servo_ctrl.c      # Servo UART control
│   ├── uart_servo_ctrl.h      # Servo control header
│   └── move_ctrl.h            # Motion posture data table
├── include/                    # Header files
├── assets/                     # Resource files
├── config/                     # Configuration files
├── script/                     # Script files
├── CMakeLists.txt             # CMake configuration
├── Kconfig                    # Kconfig configuration
└── README_hexapod_en.md       # This document
```

## 🔗 Related Resources

- [TuyaOpen SDK](https://github.com/tuya/TuyaOpen)
- [hexapod-irl (Motion Posture Generation Tool)](https://github.com/robeortZ/hexapod-irl)
- [hexapod-kinematics-library (Kinematics Library)](https://github.com/mithi/hexapod-kinematics-library)
- [T5-Core Development Board Documentation](https://developer.tuya.com/en/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj)
- [Tuya IoT Development Platform](https://platform.tuya.com/)

## ⚠️ Cautions

1. **Power Supply**: 18 servos working simultaneously require sufficient current; recommend using independent power supply
2. **First Debug**: Recommend disconnecting servo power first, confirm angle data is correct before connecting
3. **Servo Protection**: Pay attention to servo angle limits to avoid damage from excessive rotation
4. **Serial Port**: Ensure UART0 is not occupied by other devices

## 📞 Technical Support

If you encounter problems, please check:
1. Whether serial connection is correct
2. Whether servo controller is properly powered
3. Error messages in build logs
4. Whether APP DP transmission is normal

---

**Version**: 1.0.1  
**Last Updated**: 2024

