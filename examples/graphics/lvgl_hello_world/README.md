# LVGL Hello World Example

## Overview

This example demonstrates a simple LVGL (Light and Versatile Graphics Library) application that displays a button on the screen. When the button is clicked, a message box appears showing "hello world".

## Features

- **Button UI**: A centered button labeled "Button" on a white background
- **Message Box**: A modal message box that appears when the button is clicked
- **Clean Architecture**: UI code is separated from main application code

## Project Structure

```
lvgl_hello_world/
├── src/
│   ├── tuya_main.c          # Main application entry point
│   └── ui_hello_world.c     # UI implementation (button and message box)
├── include/
│   └── ui_hello_world.h     # UI interface header
├── CMakeLists.txt           # Build configuration
├── app_default.config       # Application configuration
└── README.md                # This file
```

## Configuration

The example requires the following configurations in `app_default.config`:

- `CONFIG_ENABLE_LIBLVGL=y` - Enable LVGL library
- `CONFIG_LVGL_ENABLE_TP=y` - Enable touch panel support
- `CONFIG_LV_USE_BUTTON=y` - Enable button widget
- `CONFIG_LV_USE_MSGBOX=y` - Enable message box widget

## Usage

1. **Build the project**:
   ```bash
   # Configure and build
   ./build.sh lvgl_hello_world
   ```

2. **Flash and run**:
   - Flash the firmware to your device
   - The device will display a button labeled "Click Me"

3. **Interact**:
   - Click the button on the screen
   - A message box will appear showing "hello world"
   - Click the close button (X) to dismiss the message box

## Code Structure

### UI Module (`ui_hello_world.c`)

The UI module handles all visual components:

- **`ui_hello_world_init()`**: Initializes the UI, creates the button
- **`button_click_event_cb()`**: Handles button click events
- **`show_hello_world_msgbox()`**: Creates and displays the message box
- **`msgbox_close_event_cb()`**: Handles message box close button events

### Main Module (`tuya_main.c`)

The main module handles application initialization:

- Initializes logging
- Registers hardware
- Initializes LVGL vendor
- Calls UI initialization
- Starts LVGL task

## Customization

You can customize the example by modifying:

- **Button text**: Change `"Click Me"` in `ui_hello_world_init()`
- **Message box title**: Change `"Hello"` in `show_hello_world_msgbox()`
- **Message box text**: Change `"hello world"` in `show_hello_world_msgbox()`
- **Button size**: Modify `lv_obj_set_size()` parameters
- **Colors**: Add `lv_obj_set_style_bg_color()` calls

## Requirements

- TuyaOpen SDK
- LVGL library (v9)
- Display hardware with touch support
- Board configuration with LCD module

## Notes

- The message box is modal (blocks interaction with other UI elements)
- Only one message box can be displayed at a time
- The example uses LVGL v9 API (`lv_button_create`, `lv_msgbox_create`, etc.)

