# 六足机器人控制系统 - 使用说明

## 📋 项目简介

这是一个基于 React 和 Node.js 的六足机器人实时控制系统，通过 Web 界面控制物理六足机器人。系统使用 Socket.IO 实现前后端实时通信，通过 Johnny-Five 控制 Arduino 兼容板上的舵机。

## 项目地址：
- [适配了总线舵机控制的项目地址hexapod-irl](https://github.com/robeortZ/hexapod-irl)
- [原始项目](https://github.com/mithi/hexapod-irl) - Mithi's Hexapod IRL

### 主要功能

- **正运动学控制**：直接控制每条腿的三个关节角度（alpha, beta, gamma）
- **逆运动学控制**：通过目标位置（x, y, z）自动计算关节角度
- **步态控制**：支持 Tripod Gait（三脚架步态）和 Ripple Gait（波浪步态）
- **行走模式**：支持前进/后退行走和原地旋转两种模式
- **实时3D可视化**：实时显示机器人姿态和运动轨迹
- **腿部模式**：预设的腿部动作模式

## 🏗️ 系统架构

```
┌─────────────────┐
│  React 前端应用  │  (端口 3000)
│  - 用户界面      │
│  - 3D可视化      │
│  - 步态生成      │
└────────┬────────┘
         │ Socket.IO
         │ (端口 4001)
         ▼
┌─────────────────┐
│  Node.js 服务器  │
│  - 接收命令      │
│  - 角度转换      │
│  - 舵机控制      │
└────────┬────────┘
         │ Johnny-Five
         │ Serial/USB
         ▼
┌─────────────────┐
│  Arduino 板     │
│  - 舵机驱动     │
│  - PCA9685      │
└────────┬────────┘
         │
         ▼
    物理六足机器人
```

## 📦 安装和配置

### 1. 环境要求

- Node.js (建议 v14 或更高版本)
- npm 或 yarn
- Arduino 兼容板（如 Adafruit Metro Mini 328）
- 已刷入 StandardFirmata 固件

### 2. 安装依赖

```bash
npm install
```

### 3. 配置舵机参数

编辑 `src/_SERVO_CONFIG.js`，根据你的硬件配置设置舵机引脚：

```javascript
// 示例：使用 PCA9685 的配置
const servoConfig = {
    leftFront: {
        alpha: { controller: "PCA9685", pin: 0 },
        beta: { controller: "PCA9685", pin: 1 },
        gamma: { controller: "PCA9685", pin: 2 },
    },
    // ... 其他腿的配置
}
```

### 4. 配置网络和端口

编辑 `src/_VAR_CONFIG.js`：

```javascript
// Socket.IO 服务器端口
const SOCKET_SERVER_PORT = 4001

// 允许连接的客户端 URL
const SOCKET_CLIENT_URLS = [
    "http://localhost:3000",
    "http://192.168.x.x:3000"  // 你的局域网 IP
]
```

## 🚀 使用方法

### 启动系统

需要同时运行前端和后端两个服务：

**终端 1 - 启动后端服务器：**
```bash
npm run run:server
```

**终端 2 - 构建并启动前端：**
```bash
# 开发模式
npm run dev:client

# 或生产模式（推荐）
npm run build:client
npx serve -s build -l 3000
```

### 访问界面

打开浏览器访问：`http://localhost:3000`

### 页面功能说明

#### 1. 正运动学页面 (`/forward-kinematics`)
- 直接控制每条腿的三个关节角度
- 使用滑块调整 alpha、beta、gamma 角度
- **Reset 按钮**：将机器人重置到默认姿态（所有角度为 0，映射到舵机的 135 度）

#### 2. 逆运动学页面 (`/inverse-kinematics`)
- 通过目标位置（x, y, z）控制腿部
- 系统自动计算所需的关节角度

#### 3. 步态控制页面 (`/walking-gaits`)
- **步态类型**：
  - Tripod Gait（三脚架步态）：3 条腿同时抬起
  - Ripple Gait（波浪步态）：6 条腿依次抬起
- **运动模式**：
  - Walking（行走）：前进/后退
  - Rotating（旋转）：原地旋转
- **参数调整**：
  - Step Count：步数
  - Hip Swing：髋部摆动幅度
  - Lift Swing：抬腿幅度
  - Step Height：步高

#### 4. 腿部模式页面 (`/leg-patterns`)
- 预设的腿部动作模式
- 快速测试机器人基本动作

## ⚙️ 关键配置说明

### 角度映射系统

**重要**：本系统使用总线舵机，角度范围为 **0-270 度**，默认位置为 **135 度**。

#### 前端角度系统
- 前端使用**相对角度**，以 135 度为基准
- 例如：前端发送 `0` 表示默认位置（135 度）
- 前端发送 `-90` 表示 135 - 90 = 45 度
- 前端发送 `+90` 表示 135 + 90 = 225 度

#### 角度转换逻辑

转换在 `src/_TRANSFORM.js` 中完成：

```javascript
const clean = (x, shouldInvert) => {
    let directed = shouldInvert ? -1 * x : x
    // 总线舵机：角度范围0-270度，默认位置135度
    // 前端发送的角度是相对于135度的偏移量
    // 需要转换为舵机的绝对角度：舵机角度 = 前端角度 + 135
    return Math.max(Math.min(Math.round(directed) + 135, 270), 0)
}
```

**转换规则**：
- 前端角度 `0` → 舵机角度 `135`（中间位置）
- 前端角度 `-135` → 舵机角度 `0`（最小角度）
- 前端角度 `+135` → 舵机角度 `270`（最大角度）

#### 镜像处理

不同腿的关节需要镜像处理：
- `alpha`（髋部旋转）：左腿镜像，右腿不镜像
- `beta`（大腿）：左腿镜像，右腿不镜像
- `gamma`（小腿）：左腿不镜像，右腿镜像

### 默认姿态

在 `src/templates/hexapodParams.js` 中定义：

```javascript
const DEFAULT_POSE = {
    leftFront: { alpha: 0, beta: 0, gamma: 0 },
    rightFront: { alpha: 0, beta: 0, gamma: 0 },
    // ... 其他腿
}
```

这些 `0` 值会被转换为舵机的 `135` 度（中间位置）。

## 🔧 故障排除

### 1. 前端无法访问

**问题**：`http://localhost:3000` 无法打开

**解决方案**：
```bash
# 检查端口是否被占用
lsof -i :3000

# 重新启动前端服务
npm run build:client
npx serve -s build -l 3000
```

### 2. 构建错误：OpenSSL 错误

**问题**：`Error: error:0308010C:digital envelope routines::unsupported`

**解决方案**：
```bash
NODE_OPTIONS=--openssl-legacy-provider npm run build:client
```

### 3. 服务器无法连接 Arduino

**问题**：后端显示 "board connected" 但舵机不响应

**检查项**：
- Arduino 板是否正确连接 USB
- 是否已刷入 StandardFirmata 固件
- 串口权限是否正确（Linux/Mac 可能需要 `sudo`）
- 舵机配置是否正确（`_SERVO_CONFIG.js`）

### 4. 角度不正确

**问题**：Reset 按钮发送的角度不是 135 度

**解决方案**：
1. 确认 `src/_TRANSFORM.js` 中的 `clean` 函数包含 `+ 135` 偏移
2. 重新构建前端：`NODE_OPTIONS=--openssl-legacy-provider npm run build:client`
3. 清除浏览器缓存并强制刷新（Cmd+Shift+R 或 Ctrl+Shift+R）

### 5. Socket.IO 连接失败

**问题**：前端无法连接到后端

**检查项**：
- 后端服务器是否正在运行（端口 4001）
- `_VAR_CONFIG.js` 中的 `SOCKET_CLIENT_URLS` 是否包含前端 URL
- 防火墙是否阻止了端口 4001

## 📝 开发说明

### 项目结构

```
hexapod-irl/
├── src/
│   ├── _TRANSFORM.js          # 角度转换函数
│   ├── _VAR_CONFIG.js          # 配置变量
│   ├── _SERVO_CONFIG.js        # 舵机配置
│   ├── _ROBOT_SERVER.js        # 后端服务器
│   ├── _HOOK.js                # Socket.IO 通信 Hook
│   ├── App.js                  # React 主应用
│   ├── components/             # React 组件
│   │   ├── pages/              # 页面组件
│   │   │   ├── ForwardKinematicsPage.js
│   │   │   ├── InverseKinematicsPage.js
│   │   │   ├── WalkingGaitsPage.js
│   │   │   └── ...
│   │   └── ...
│   └── templates/              # 模板和默认值
├── public/                     # 静态资源
└── build/                      # 构建输出（运行 build:client 后生成）
```

### 修改角度映射

如果需要修改角度映射逻辑，编辑 `src/_TRANSFORM.js`：

1. 修改 `clean` 函数中的偏移量（当前为 `+ 135`）
2. 修改角度范围限制（当前为 `0-270`）
3. 重新构建前端

### 添加新的步态

1. 在 `src/components/pages/WalkingGaitsPage.js` 中添加步态生成逻辑
2. 使用 `hexapod-kinematics-library` 计算关节角度
3. 通过 `setWalkSequence` 设置步态序列

## 📚 相关资源

- [hexapod-kinematics-library](https://github.com/mithi/hexapod-kinematics-library) - 运动学计算库
- [Johnny-Five](http://johnny-five.io/) - Arduino 控制库
- [Socket.IO](https://socket.io/) - 实时通信库
- [原始项目](https://github.com/mithi/hexapod-irl) - Mithi's Hexapod IRL

## ⚠️ 注意事项

1. **安全**：首次使用时，建议先断开舵机电源，在可视化界面中测试角度是否正确
2. **电源**：确保舵机电源充足，18 个舵机同时工作需要较大电流
3. **串口**：确保 Arduino 板正确连接，且没有其他程序占用串口
4. **角度限制**：注意舵机的物理角度限制，避免过度旋转损坏舵机

## 📞 技术支持

如遇到问题，请检查：
1. 浏览器控制台（F12）的错误信息
2. 后端服务器的日志输出
3. Arduino 板的连接状态

---

**版本**：基于 hexapod-irl v0.2.0 (Socket.IO 版本)  
**最后更新**：2024

