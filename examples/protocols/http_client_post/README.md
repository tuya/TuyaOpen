# HTTP Client POST Example

A demonstration of HTTP POST client functionality with interactive button control. Click the button on screen to send HTTP POST requests to a server and display the response.

## Features

- **Interactive Button**: Click button on screen to send HTTP POST request
- **HTTP POST Request**: Sends POST requests with JSON body to a server
- **Response Display**: Displays server response in a container box on screen
- **Wi-Fi Status Indicator**: Shows connection status with green/red dot in top-right corner
- **Raw Data Display**: Displays raw server response data directly
- **Code Separation**: UI code and network code are separated into different files

## Prerequisites

1. **Python Server**: You need to run a Python HTTP server on your computer (server.py provided)
2. **Network Connection**: Device must be connected to the same network as the server
3. **Display Board** (Optional): Board with LCD screen support for UI display (e.g., T5AI with LCD module)

## Configuration

### 1. Configure Server Information

Edit `src/tuya_main.c` and update these constants:

```c
// Server configuration - HTTP server settings
#define SERVER_HOST "192.168.1.181"  // Server IP address, modify according to your actual server
#define SERVER_PORT 8080               // Server port number
#define SERVER_PATH "/api/random"      // Server API endpoint path
```

### 2. Configure Wi-Fi (if using Wi-Fi)

Edit `src/tuya_main.c`:

```c
// Wi-Fi configuration - Wi-Fi network settings
#define DEFAULT_WIFI_SSID "your-ssid"        // Wi-Fi network name (SSID), modify according to your actual network
#define DEFAULT_WIFI_PSWD "your-passwd"    // Wi-Fi password, modify according to your actual network
```

### 3. Configure Display (Optional)

Edit `app_default.config`:

```config
CONFIG_BOARD_CHOICE_T5AI=y
CONFIG_TUYA_T5AI_BOARD_EX_MODULE_35565LCD=y
CONFIG_ENABLE_LIBLVGL=y
CONFIG_LVGL_ENABLE_TP=y
CONFIG_ENABLE_LVGL_DEMO=y
```

## Quick Start

### Step 1: Start the Python Server

On your computer, run the provided Python server:

```bash
cd examples/protocols/http_client_post
python3 server.py
```

The server will display:
```
============================================================
HTTP POST Server for TuyaOpen HTTP Client Example
============================================================
Server starting on 0.0.0.0:8080
Local IP address: 192.168.124.181
Access URL: http://192.168.124.181:8080
API Endpoint: http://192.168.124.181:8080/api/random
============================================================
```

**Note the IP address** - update `SERVER_HOST` in `src/tuya_main.c` with this IP.

### Step 2: Configure the Client

Update server IP and Wi-Fi credentials in `src/tuya_main.c` (see Configuration section above).

### Step 3: Build and Flash

```bash
# Configure project
tos config_choice  # Select your board

# Configure display (if using display)
tos menuconfig    # Configure display driver and pins

# Build
tos build

# Flash to device
```

### Step 4: Run

1. **Power on** the device
2. **Connect to network** (Wi-Fi or Wired)
3. **Click the button** on screen to send HTTP POST request
4. **View response** displayed in the container box

## Usage

### With Display

When display is enabled, the screen shows:

- **Wi-Fi Status**: "Wi-Fi" label with status dot (green = connected, red = disconnected) in top-right corner
- **Receive Label**: "Receive" label above the response container
- **Response Container**: Box with border containing the server response text
- **Send Button**: Button at bottom of screen to trigger POST request

**Workflow:**
1. Device connects to Wi-Fi (green dot appears)
2. Click "Send Request" button
3. Button shows "Sending..." status
4. Server response appears in the container box

### Without Display

The device will:
1. Connect to network automatically
2. Send POST request when network is connected (if auto-send is enabled)
3. Log response to console

## How It Works

1. **Display Initialization**: LVGL initializes and creates UI elements (button, response container, Wi-Fi indicator)
2. **Network Connection**: Device connects to Wi-Fi network
3. **Button Click**: When button is clicked:
   - Checks network connection status
   - Updates UI to show "Sending..." status
   - Sends HTTP POST request to server
4. **Response Handling**: When response is received:
   - Raw response data is copied to display buffer
   - Response is displayed in container box on screen
   - Response is logged to console

## Code Structure

```
http_client_post/
├── CMakeLists.txt          # Build configuration
├── app_default.config      # App configuration
├── server.py               # Python HTTP server for testing
├── include/
│   └── ui.h                # UI interface header
└── src/
    ├── tuya_main.c         # Main application (network, HTTP POST)
    └── ui.c                # UI implementation (LVGL display)
```

## Key Code Sections

### Button Click Handler

```c
static void __button_click_callback(void)
{
    // Check network status
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    
    if (status != NETMGR_LINK_UP) {
        ui_update_response_text("Network Not Connected", true);
        return;
    }
    
    // Update UI to show sending status
    ui_update_response_sending();
    
    // Send HTTP POST request
    __send_http_post_request();
}
```

### HTTP POST Request

```c
static void __send_http_post_request(void)
{
    http_client_response_t http_response = {0};
    
    http_client_header_t headers[] = {
        {.key = "Content-Type", .value = "application/json"},
        {.key = "User-Agent", .value = "TuyaOpen-HTTP-Client"}
    };
    
    const char *post_body = "{\"action\":\"get_random_string\"}";
    
    http_client_status_t http_status = http_client_request(
        &(const http_client_request_t){
            .host = SERVER_HOST,
            .port = SERVER_PORT,
            .method = "POST",
            .path = SERVER_PATH,
            .headers = headers,
            .headers_count = sizeof(headers) / sizeof(http_client_header_t),
            .body = (const uint8_t *)post_body,
            .body_length = strlen(post_body),
            .timeout_ms = HTTP_REQUEST_TIMEOUT},
        &http_response);
    
    // Process response...
}
```

### Response Display

```c
if (http_response.status_code == 200 && http_response.body != NULL) {
    // Copy raw response data directly
    size_t copy_len = http_response.body_length;
    if (copy_len > sizeof(server_response) - 1) {
        copy_len = sizeof(server_response) - 1;
    }
    memcpy(server_response, http_response.body, copy_len);
    server_response[copy_len] = '\0';
    
    // Update UI
    ui_update_response_text(server_response, false);
}
```

## Server API

### POST /api/random

**Request:**
```http
POST /api/random HTTP/1.1
Host: 192.168.124.181:8080
Content-Type: application/json
Content-Length: 32

{"action":"get_random_string"}
```

**Response:**
```json
{
  "status": "success",
  "message": "aB3xK9mP2qR7nT4v",
  "text": "aB3xK9mP2qR7nT4v",
  "data": "aB3xK9mP2qR7nT4v",
  "length": 16
}
```

The client displays the raw response data directly, including the full JSON response.

## Server Usage

### Basic Usage

```bash
python3 server.py
```

### Custom Host and Port

```bash
python3 server.py --host 0.0.0.0 --port 9000
```

### Get Help

```bash
python3 server.py --help
```

## Troubleshooting

### Button Not Displaying

- Check display configuration in `app_default.config`
- Verify display driver is configured in `tos menuconfig`
- Ensure `CONFIG_ENABLE_LIBLVGL=y` is set
- Check LVGL initialization logs

### Button Click Not Working

- Verify touch screen is configured if using touch input
- Check touch driver configuration in `tos menuconfig`
- Ensure `CONFIG_LVGL_ENABLE_TP=y` is set if using touch
- Verify GPIO pins for touch are correctly configured

### Server Not Found

1. **Check IP Address**: Make sure `SERVER_HOST` matches your computer's IP
2. **Check Network**: Ensure device and computer are on the same network
3. **Check Firewall**: Disable firewall or allow the configured port
4. **Check Server**: Verify Python server is running

### Connection Timeout

1. **Check Server**: Make sure Python server is running
2. **Check Port**: Verify port number matches configuration
3. **Check Network**: Test network connectivity (ping server IP)
4. **Check Logs**: Review device logs for connection errors

### Wi-Fi Not Connecting

- Verify Wi-Fi SSID and password in `src/tuya_main.c`
- Check Wi-Fi credentials are correct
- Ensure device is within Wi-Fi range
- Check network connection logs

### Response Not Displayed

1. **Check Logs**: Look for error messages in device logs
2. **Check Response Format**: Server should return valid HTTP response
3. **Check Buffer Size**: Increase `server_response` buffer if response is truncated
4. **Check UI**: Verify UI is initialized correctly

## Advanced Usage

### Custom Server Implementation

You can create your own server. The server should:
1. Accept POST requests on the configured path
2. Return HTTP response with body content
3. The client displays raw response data directly

Example server response:
```json
{
  "message": "Your response data here"
}
```

Or plain text:
```
Your response data here
```

### Code Separation

The project separates UI and network code:
- **`src/tuya_main.c`**: Network initialization, HTTP POST requests, response handling
- **`src/ui.c`**: LVGL UI implementation, button creation, display updates
- **`include/ui.h`**: UI interface definitions

This separation makes the code more maintainable and easier to understand.

## Technical Support

You can get support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com
- Developer Center: https://developer.tuya.com
- Help Center: https://support.tuya.com/help
- Technical Support Ticket Center: https://service.console.tuya.com
