# AI Pocket Pet - Modular Refactor

This directory contains the refactored AI Pocket Pet application, organized into modular components for better maintainability and development.

## Architecture

The application has been refactored from a monolithic `lv_demo_ai_pocket_pet.c` file into modular components:

### Core Application
- `ai_pocket_pet_app.c` - Main application entry point that integrates all components
- `lv_demo_ai_pocket_pet.h` - Original public API header (unchanged)

### Modular Components
- `keyboard.c/.h` - Custom keyboard widget (refactored from `lv_keyboard_widget.c`)
- `status_bar.c/.h` - Status bar with WiFi, cellular, and battery icons
- `pet_area.c/.h` - Pet display area with animations
- `menu_system.c/.h` - Menu navigation and pet statistics management
- `toast.c/.h` - Toast message notifications

### Assets
- `data/` - Icons and pet animations (unchanged)

## Public API

The original public API from `lv_demo_ai_pocket_pet.h` is maintained through delegation:

```c
// Main functions
void lv_demo_ai_pocket_pet(void);
void lv_demo_ai_pocket_pet_handle_input(uint32_t key);

// Toast messages
void lv_demo_ai_pocket_pet_show_toast(const char *message, uint32_t delay_ms);
void lv_demo_ai_pocket_pet_hide_toast(void);

// Network status
void lv_demo_ai_pocket_pet_set_wifi_strength(uint8_t strength);
void lv_demo_ai_pocket_pet_set_cellular_status(uint8_t strength, bool connected);
uint8_t lv_demo_ai_pocket_pet_get_wifi_strength(void);
uint8_t lv_demo_ai_pocket_pet_get_cellular_strength(void);
bool lv_demo_ai_pocket_pet_get_cellular_connected(void);

// Battery status
void lv_demo_ai_pocket_pet_set_battery_status(uint8_t level, bool charging);
uint8_t lv_demo_ai_pocket_pet_get_battery_level(void);
bool lv_demo_ai_pocket_pet_get_battery_charging(void);
```

## Build Configuration

The CMakeLists.txt has been updated to build all modular components:

```cmake
add_executable(main
    ${PROJECT_SOURCE_DIR}/main/src/main.c
    ${PROJECT_SOURCE_DIR}/main/src/mouse_cursor_icon.c
    ${PROJECT_SOURCE_DIR}/ai-pocket-pet/ai_pocket_pet_app.c
    ${PROJECT_SOURCE_DIR}/ai-pocket-pet/keyboard.c
    ${PROJECT_SOURCE_DIR}/ai-pocket-pet/status_bar.c
    ${PROJECT_SOURCE_DIR}/ai-pocket-pet/pet_area.c
    ${PROJECT_SOURCE_DIR}/ai-pocket-pet/menu_system.c
    ${PROJECT_SOURCE_DIR}/ai-pocket-pet/toast.c
)
```

## Features Preserved

All original functionality has been preserved:
- Pet animations and movement
- Status bar with network and battery icons
- Menu system with navigation
- Keyboard input for pet naming
- Toast message notifications
- All keyboard shortcuts and testing functions

## Benefits of Refactoring

1. **Modularity** - Each component has a clear responsibility
2. **Maintainability** - Easier to modify individual components
3. **Testability** - Components can be tested in isolation
4. **Reusability** - Components can be reused in other projects
5. **Code Organization** - Better separation of concerns
6. **Development** - Multiple developers can work on different components

The refactoring maintains 100% backward compatibility while providing a cleaner, more maintainable codebase.
