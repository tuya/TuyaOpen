# LVGL Button Demo

A simple LVGL demonstration that creates a button on screen. Clicking the button toggles the screen background color between blue and white.

## Features

- **LVGL Display**: Shows a button widget on screen
- **Interactive Button**: Click button to toggle background color
- **Color Toggle**: Switches between blue (`0x0000FF`) and white (`0xFFFFFF`) backgrounds
- **Simple UI**: Clean interface with centered button

## Prerequisites

1. **Display Board**: Board with LCD screen support (e.g., T5AI with LCD module)
2. **Touch Support** (Optional): Touch screen for button interaction

## Configuration

### Configure Display

Edit `app_default.config`:

```config
CONFIG_BOARD_CHOICE_T5AI=y
CONFIG_TUYA_T5AI_BOARD_EX_MODULE_35565LCD=y
CONFIG_ENABLE_LIBLVGL=y
CONFIG_LVGL_ENABLE_TP=y
CONFIG_ENABLE_LVGL_DEMO=y
```

### Configure Display Driver

Run `tos menuconfig` to configure:
- Display driver (SPI/RGB/8080)
- Touch driver (if touch screen is available)
- GPIO pins for display and touch

## Build and Run

```bash
# Configure project
tos config_choice  # Select your board

# Configure display (if needed)
tos menuconfig    # Configure display driver and pins

# Build
tos build

# Flash to device
```

## Usage

1. **Power on** the device
2. **Button appears** on screen (centered, 120x50 pixels)
3. **Click the button** to toggle background color:
   - First click: Background changes to blue
   - Second click: Background changes to white
   - Continues toggling on each click

## How It Works

1. **Display Initialization**: LVGL initializes and creates a button widget on screen
2. **Button Creation**: A button (120x50 pixels) is created and centered on screen
3. **Event Handling**: When button is clicked:
   - Event callback `button_event_cb` is triggered
   - Background color toggles between blue and white
   - State is tracked using a static boolean variable

## Code Structure

```
lvgl_button_demo/
├── CMakeLists.txt          # Build configuration
├── app_default.config      # App configuration
└── src/
    └── example_lvgl.c      # Main application (button creation and event handling)
```

## Key Code Sections

### Button Creation

```c
void lvgl_demo_button()
{
    // Create a button
    lv_obj_t * btn = lv_btn_create(lv_screen_active());
    lv_obj_set_size(btn, 120, 50);
    lv_obj_center(btn);
    
    // Add label to button
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Button");
    lv_obj_center(label);

    // Register click event callback
    lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, NULL);
}
```

### Button Click Event Handler

```c
void button_event_cb(lv_event_t * event)
{
    static bool is_blue = false;
    lv_obj_t * screen = lv_screen_active();
    
    if (is_blue) {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0);  // White
        is_blue = false;
    } else {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x0000FF), 0);  // Blue
        is_blue = true;
    }
}
```

## Supported Platforms

- **T5AI**: RGB/8080/SPI interfaces
- **T3**: SPI interface

## Supported Drivers

### Screen
- SPI: ST7789, ILI9341, GC9A01
- RGB: ILI9488

### Touch (Optional)
- I2C: GT911, CST816, GT1511

## Troubleshooting

### Button Not Displaying

- Check display configuration in `app_default.config`
- Verify display driver is configured in `tos menuconfig`
- Check LVGL initialization logs
- Ensure `CONFIG_ENABLE_LIBLVGL=y` is set

### Button Not Responding to Clicks

- Verify touch screen is configured if using touch input
- Check touch driver configuration in `tos menuconfig`
- Ensure `CONFIG_LVGL_ENABLE_TP=y` is set if using touch
- Verify GPIO pins for touch are correctly configured

### Display Issues

- Check display interface configuration (SPI/RGB/8080)
- Verify display driver matches your hardware
- Check GPIO pin configuration
- Review display initialization logs

## Technical Support

You can get support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com
- Developer Center: https://developer.tuya.com
- Help Center: https://support.tuya.com/help
- Technical Support Ticket Center: https://service.console.tuya.com
