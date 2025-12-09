# Hexapod Robot Control System - User Guide

## 📋 Project Introduction

This is a real-time hexapod robot control system based on React and Node.js, controlling physical hexapod robots through a Web interface. The system uses Socket.IO for real-time frontend-backend communication and Johnny-Five for servo control on Arduino-compatible boards.

## Project Links:
- [Adapted Bus Servo Control Project - hexapod-irl](https://github.com/robeortZ/hexapod-irl)
- [Original Project](https://github.com/mithi/hexapod-irl) - Mithi's Hexapod IRL

### Main Features

- **Forward Kinematics Control**: Directly control three joint angles (alpha, beta, gamma) for each leg
- **Inverse Kinematics Control**: Automatically calculate joint angles through target positions (x, y, z)
- **Gait Control**: Support for Tripod Gait and Ripple Gait
- **Walking Modes**: Support for forward/backward walking and in-place rotation
- **Real-time 3D Visualization**: Display robot pose and motion trajectory in real-time
- **Leg Patterns**: Preset leg action patterns

## 🏗️ System Architecture

```
┌─────────────────┐
│  React Frontend │  (Port 3000)
│  - User Interface│
│  - 3D Visualization│
│  - Gait Generation│
└────────┬────────┘
         │ Socket.IO
         │ (Port 4001)
         ▼
┌─────────────────┐
│  Node.js Server │
│  - Receive Commands│
│  - Angle Conversion│
│  - Servo Control│
└────────┬────────┘
         │ Johnny-Five
         │ Serial/USB
         ▼
┌─────────────────┐
│  Arduino Board  │
│  - Servo Driver │
│  - PCA9685      │
└────────┬────────┘
         │
         ▼
    Physical Hexapod Robot
```

## 📦 Installation and Configuration

### 1. Requirements

- Node.js (v14 or higher recommended)
- npm or yarn
- Arduino-compatible board (e.g., Adafruit Metro Mini 328)
- StandardFirmata firmware flashed

### 2. Install Dependencies

```bash
npm install
```

### 3. Configure Servo Parameters

Edit `src/_SERVO_CONFIG.js` to set servo pins according to your hardware configuration:

```javascript
// Example: Configuration using PCA9685
const servoConfig = {
    leftFront: {
        alpha: { controller: "PCA9685", pin: 0 },
        beta: { controller: "PCA9685", pin: 1 },
        gamma: { controller: "PCA9685", pin: 2 },
    },
    // ... configuration for other legs
}
```

### 4. Configure Network and Ports

Edit `src/_VAR_CONFIG.js`:

```javascript
// Socket.IO server port
const SOCKET_SERVER_PORT = 4001

// Allowed client URLs
const SOCKET_CLIENT_URLS = [
    "http://localhost:3000",
    "http://192.168.x.x:3000"  // Your LAN IP
]
```

## 🚀 Usage

### Starting the System

Run both frontend and backend services simultaneously:

**Terminal 1 - Start backend server:**
```bash
npm run run:server
```

**Terminal 2 - Build and start frontend:**
```bash
# Development mode
npm run dev:client

# Or production mode (recommended)
npm run build:client
npx serve -s build -l 3000
```

### Access the Interface

Open browser and visit: `http://localhost:3000`

### Page Function Description

#### 1. Forward Kinematics Page (`/forward-kinematics`)
- Directly control three joint angles for each leg
- Use sliders to adjust alpha, beta, gamma angles
- **Reset Button**: Reset robot to default pose (all angles at 0, mapped to servo's 135 degrees)

#### 2. Inverse Kinematics Page (`/inverse-kinematics`)
- Control leg position through target coordinates (x, y, z)
- System automatically calculates required joint angles

#### 3. Gait Control Page (`/walking-gaits`)
- **Gait Types**:
  - Tripod Gait: 3 legs lift simultaneously
  - Ripple Gait: 6 legs lift sequentially
- **Motion Modes**:
  - Walking: Forward/backward movement
  - Rotating: In-place rotation
- **Parameter Adjustment**:
  - Step Count: Number of steps
  - Hip Swing: Hip swing amplitude
  - Lift Swing: Leg lift amplitude
  - Step Height: Step height

#### 4. Leg Patterns Page (`/leg-patterns`)
- Preset leg action patterns
- Quick test of basic robot actions

## ⚙️ Key Configuration Details

### Angle Mapping System

**Important**: This system uses bus servos with an angle range of **0-270 degrees** and a default position of **135 degrees**.

#### Frontend Angle System
- Frontend uses **relative angles** based on 135 degrees
- Example: Frontend sends `0` for default position (135 degrees)
- Frontend sends `-90` for 135 - 90 = 45 degrees
- Frontend sends `+90` for 135 + 90 = 225 degrees

#### Angle Conversion Logic

Conversion is done in `src/_TRANSFORM.js`:

```javascript
const clean = (x, shouldInvert) => {
    let directed = shouldInvert ? -1 * x : x
    // Bus servo: angle range 0-270 degrees, default position 135 degrees
    // Frontend sends angles as offset from 135 degrees
    // Convert to servo absolute angle: servo angle = frontend angle + 135
    return Math.max(Math.min(Math.round(directed) + 135, 270), 0)
}
```

**Conversion Rules**:
- Frontend angle `0` → Servo angle `135` (center position)
- Frontend angle `-135` → Servo angle `0` (minimum angle)
- Frontend angle `+135` → Servo angle `270` (maximum angle)

#### Mirror Processing

Different legs require mirror processing for joints:
- `alpha` (hip rotation): Left legs mirrored, right legs not mirrored
- `beta` (thigh): Left legs mirrored, right legs not mirrored
- `gamma` (calf): Left legs not mirrored, right legs mirrored

### Default Pose

Defined in `src/templates/hexapodParams.js`:

```javascript
const DEFAULT_POSE = {
    leftFront: { alpha: 0, beta: 0, gamma: 0 },
    rightFront: { alpha: 0, beta: 0, gamma: 0 },
    // ... other legs
}
```

These `0` values are converted to servo's `135` degrees (center position).

## 🔧 Troubleshooting

### 1. Frontend Inaccessible

**Problem**: Cannot open `http://localhost:3000`

**Solution**:
```bash
# Check if port is occupied
lsof -i :3000

# Restart frontend service
npm run build:client
npx serve -s build -l 3000
```

### 2. Build Error: OpenSSL Error

**Problem**: `Error: error:0308010C:digital envelope routines::unsupported`

**Solution**:
```bash
NODE_OPTIONS=--openssl-legacy-provider npm run build:client
```

### 3. Server Cannot Connect to Arduino

**Problem**: Backend shows "board connected" but servos don't respond

**Check Items**:
- Is Arduino board correctly connected via USB
- Is StandardFirmata firmware flashed
- Are serial port permissions correct (Linux/Mac may need `sudo`)
- Is servo configuration correct (`_SERVO_CONFIG.js`)

### 4. Incorrect Angles

**Problem**: Reset button doesn't send 135 degrees

**Solution**:
1. Confirm `clean` function in `src/_TRANSFORM.js` includes `+ 135` offset
2. Rebuild frontend: `NODE_OPTIONS=--openssl-legacy-provider npm run build:client`
3. Clear browser cache and force refresh (Cmd+Shift+R or Ctrl+Shift+R)

### 5. Socket.IO Connection Failed

**Problem**: Frontend cannot connect to backend

**Check Items**:
- Is backend server running (port 4001)
- Does `SOCKET_CLIENT_URLS` in `_VAR_CONFIG.js` include frontend URL
- Is firewall blocking port 4001

## 📝 Development Notes

### Project Structure

```
hexapod-irl/
├── src/
│   ├── _TRANSFORM.js          # Angle conversion functions
│   ├── _VAR_CONFIG.js          # Configuration variables
│   ├── _SERVO_CONFIG.js        # Servo configuration
│   ├── _ROBOT_SERVER.js        # Backend server
│   ├── _HOOK.js                # Socket.IO communication Hook
│   ├── App.js                  # React main application
│   ├── components/             # React components
│   │   ├── pages/              # Page components
│   │   │   ├── ForwardKinematicsPage.js
│   │   │   ├── InverseKinematicsPage.js
│   │   │   ├── WalkingGaitsPage.js
│   │   │   └── ...
│   │   └── ...
│   └── templates/              # Templates and defaults
├── public/                     # Static resources
└── build/                      # Build output (generated after build:client)
```

### Modifying Angle Mapping

To modify angle mapping logic, edit `src/_TRANSFORM.js`:

1. Modify offset in `clean` function (currently `+ 135`)
2. Modify angle range limits (currently `0-270`)
3. Rebuild frontend

### Adding New Gaits

1. Add gait generation logic in `src/components/pages/WalkingGaitsPage.js`
2. Use `hexapod-kinematics-library` to calculate joint angles
3. Set gait sequence via `setWalkSequence`

## 📚 Related Resources

- [hexapod-kinematics-library](https://github.com/mithi/hexapod-kinematics-library) - Kinematics calculation library
- [Johnny-Five](http://johnny-five.io/) - Arduino control library
- [Socket.IO](https://socket.io/) - Real-time communication library
- [Original Project](https://github.com/mithi/hexapod-irl) - Mithi's Hexapod IRL

## ⚠️ Important Notes

1. **Safety**: For first-time use, disconnect servo power and test angles in visualization interface first
2. **Power**: Ensure sufficient servo power supply; 18 servos working simultaneously require high current
3. **Serial Port**: Ensure Arduino board is correctly connected and no other programs are using the serial port
4. **Angle Limits**: Be aware of servo physical angle limits to avoid damage from over-rotation

## 📞 Technical Support

If you encounter problems, please check:
1. Browser console (F12) error messages
2. Backend server log output
3. Arduino board connection status

---

**Version**: Based on hexapod-irl v0.2.0 (Socket.IO version)  
**Last Updated**: 2024

