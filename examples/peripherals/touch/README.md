# Touch Driver Example

## Overview

This example will introduce how to use the `touch` peripheral implementation principles, configuration methods, and development techniques. The touch driver is based on capacitive touch sensing technology, which identifies touch events by detecting changes in capacitance when the human body makes contact.

* Touch Sensor Introduction

  The basic principle of capacitive touch detection is to use the human body as a conductor. When a finger approaches or touches the touch electrode, it changes the capacitance value between the electrode and ground. The driver continuously monitors these capacitance changes and, combined with complex signal processing algorithms, accurately identifies different user operations such as touch, release, and long press.

## Features

### Core Functions
The touch driver provides a complete touch detection solution, supporting up to 16 independent touch channels. The driver has built-in detection for three main types of touch events:

**Touch Press Event**: Triggered immediately when a valid touch is detected, with response times typically within tens of milliseconds. This is the most basic and important touch event, used to respond to users' basic interactive operations. The system notifies the application layer through the `TUYA_TOUCH_EVENT_PRESSED` event type.

**Touch Release Event**: Triggered when the finger leaves the touch area, ensuring that each touch operation has a complete start and end. Release detection uses a debouncing algorithm to avoid false positives caused by slight finger movements. It is passed to the application through the `TUYA_TOUCH_EVENT_RELEASED` event type.

**Long Press Event**: Triggered when touch continues beyond a preset time (default 2 seconds), commonly used to implement special functions or enter setup mode. The long press time can be adjusted according to application requirements. The system uses the `TUYA_TOUCH_EVENT_LONG_PRESS` event type to identify long press operations. (**Touch state will be cleared after touch time exceeds 10s, requiring release and re-trigger**)

![tkl_touch_event_process_en](https://images.tuyacn.com/fe-static/docs/img/d1e3d32e-345a-4a58-a06d-945e59d0af1f.png)

## Driver Architecture

### Signal Processing Flow
The signal processing for touch detection is a complex multi-stage processing flow, with each stage having its specific function:

![tkl_touch_single_process_en](https://images.tuyacn.com/fe-static/docs/img/01e0eeba-a52a-4fcf-8925-c11727c59c12.png)

**Raw Signal Collection**: The driver first obtains the raw capacitance values for each touch channel through hardware circuits. This process requires precise timing control and analog circuit design to ensure measurement accuracy and repeatability. Collection frequency is typically between tens to hundreds of hertz to balance response speed and power consumption.

**Preprocessing Filter**: Raw signals typically contain various noise and interference, requiring primary filtering to remove high-frequency noise and random interference. This step is crucial for the accuracy of subsequent processing. The preprocessing stage removes obvious outliers and pulse interference.

**Adaptive Baseline Tracking**: The baseline refers to the capacitance value when there is no touch, which slowly changes with environmental conditions. The driver uses intelligent algorithms to continuously track and update the baseline, ensuring touch detection accuracy is not affected by environmental changes. The baseline update speed automatically adjusts based on signal stability.

**Touch Signal Detection**: By comparing the difference between the current signal and baseline, combined with preset threshold parameters, it determines whether a valid touch event has occurred. This process uses the concept of a hysteresis comparator to avoid false positives when the signal fluctuates near the threshold.

**Event State Management**: Converts detected signal changes into specific touch events, including recognition of press, release, and long press. The state machine ensures logical correctness and timing accuracy of events.

### Filtering Algorithm Details
The driver uses a combination of multiple filtering algorithms to handle different types of interference and noise:

![tkl_touch_filter_process](https://images.tuyacn.com/fe-static/docs/img/c8dcf2c4-a5c7-491c-bd4f-551494951d4b.png)

**IIR Digital Filter**: Infinite Impulse Response filters are used to smooth signal changes and remove high-frequency noise. The driver automatically adjusts filter parameters based on signal change speed, finding the optimal balance between fast response and stability. When fast changes are detected, the filter reduces delay to improve response speed.

**Median Filter**: Effectively removes pulse-type interference and random noise. By sorting signals within a certain time window and selecting the median as output, it maintains good signal edge characteristics. This filtering method is particularly effective for removing occasional strong interference.

**Square Wave Filter Algorithm**: This is a filtering algorithm specifically designed for touch signals, capable of quickly responding to the start and end of touches while maintaining good anti-noise performance in stable states. The algorithm automatically selects appropriate filtering strength based on signal change characteristics.

**Adaptive Filter**: Dynamically adjusts filtering parameters based on signal statistical characteristics and environmental conditions. Uses stronger filtering when signals are stable to improve anti-noise capability, and reduces filtering when signals change to improve response speed.

## Key Configuration Parameters

```c
typedef struct {
    TUYA_TOUCH_SENSITIVITY_LEVEL_E sensitivity_level;  // Sensitivity level
    TUYA_TOUCH_DETECT_THRESHOLD_E  detect_threshold;   // Detection threshold
    TUYA_TOUCH_DETECT_RANGE_E      detect_range;       // Detection range
    TUYA_TOUCH_THRESHOLD_T         threshold;          // threshold parameters
} TUYA_TOUCH_CONFIG_T;
```

The first three parameters are used for factory initialization and should generally be kept as default according to the example.

  - `sensitivity_level`: Touch sensitivity level, used to control touch sensitivity.
  - `detect_threshold`: Touch detection threshold, used to determine if touch occurs.
  - `detect_range`: Touch detection range, used to specify the response range of touch detection.

```c
typedef struct {
    float touch_static_noise_threshold;
    float touch_filter_update_threshold;
    float touch_detect_threshold;
    float touch_variance_threshold;
} TUYA_TOUCH_THRESHOLD_T;
```

The `threshold` parameter is used for custom touch parameters and should be set according to actual application requirements.
 - `touch_static_noise_threshold` Static noise threshold, refers to the fluctuation range of sensor values, generally set to slightly larger than the **maximum value** of sensor values in static state
 - `touch_filter_update_threshold` Filter parameter update threshold, refers to the update range of filter parameters, generally set to the maximum value of sensor values in static state
 - `touch_detect_threshold` Touch detection threshold, refers to the threshold for touch detection, generally set to a value slightly smaller than `touch_filter_update_threshold`
 - `touch_variance_threshold` Variance value of touch raw data, generally keep default as in the example

:tipping_hand_man: Developers can obtain the `touch_filter_update_threshold` by estimating the difference using the return parameters from the `tkl_touch_get_filter_update_threshold` function and the `tkl_touch_get_single_median_filter_value` function, and adjust other parameters accordingly.
:tipping_hand_woman: During threshold debugging, you can enable the `DEBUG_ENABLE` macro in `tkl_touch.c` and use the **Serial Debug Assistant** from Microsoft Store to view waveforms for debugging assistance.

## Running Results

```c
ap0:W(306):baseline[1]: 7.521484
ap0:W(307):Touch init success, channel_mask=0x2
ap0:W(308):[01-01 00:00:00 ty D][example_touch.c:101] touch channel [1] cap value: 0.000000
ap0:W(1808):[01-01 00:00:01 ty D][example_touch.c:101] touch channel [1] cap value: 7.658960
ap0:W(2070):[01-01 00:00:02 ty N][example_touch.c:44] *** TOUCH EVENT PRESSD DOWN *** Channel 1
ap0:W(2308):[01-01 00:00:02 ty D][example_touch.c:101] touch channel [1] cap value: 8.372679
ap0:W(4808):[01-01 00:00:04 ty D][example_touch.c:101] touch channel [1] cap value: 8.856133
ap0:W(5043):[01-01 00:00:05 ty N][example_touch.c:52] *** TOUCH EVENT LONG PRESSED *** Channel 1
ap0:W(5308):[01-01 00:00:05 ty D][example_touch.c:101] touch channel [1] cap value: 8.928961
ap0:W(5808):[01-01 00:00:05 ty D][example_touch.c:101] touch channel [1] cap value: 8.825094
[01-01 00:00:06 ty N][example_touch.c:48] *** TOUCH EVENT RELEASED UP *** Channel 1
```
