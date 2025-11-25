"""
Keyboard Event Detector
Detects and handles keyboard key down and key up events.
Sends serial data to the configured serial port at 115200 baud:
- 0xA0 0x01 for key down events
- 0xA0 0x00 for key up events
Press Ctrl+C to exit the program.
"""

from pynput import keyboard
import serial
import sys
import threading

# ---------------------------
# Serial Port Configuration Guide
# ---------------------------
# To enable communication between this script and the Tuya T5, you must specify
# the correct serial port device name for your operating system.
#
# - Windows:
#     Use port names such as 'COM1', 'COM2', 'COM3', etc.
#     For example: 'COM14'
#
# - Linux:
#     Common options include '/dev/ttyUSB0', '/dev/ttyACM0', '/dev/ttyS0', etc.
#
# - MacOS:
#     Possible device names include:
#         '/dev/cu.usbserial-*', '/dev/cu.usbmodem*', '/dev/tty.usbserial-*', etc.
#
# ---- Tuya T5 Device Info ----
# The Tuya T5 board presents two serial ports:
#   1. One port is for flashing firmware
#   2. The other is for debugging output (logs)
# These typically appear as two different COM or device ports on your system
# (e.g. Serial Port A and B).
#
# For this script's demo, use the *flashing* serial port.
#
# IMPORTANT:
#   - While flashing the Tuya T5, this script must be stopped/closed first,
#     as it keeps the serial port open and will block other tools from accessing it.
# ---------------------------



SERIAL_PORT = 'COM14'  # Change this to match your system's serial port
SERIAL_BAUD = 115200

# Global variable to track Ctrl key state
ctrl_pressed = False

def on_key_press(key, ser, exit_event):
    """Callback function for key down events."""
    global ctrl_pressed
    
    try:
        # Check for Ctrl key
        if key == keyboard.Key.ctrl_l or key == keyboard.Key.ctrl_r:
            ctrl_pressed = True
            return
        
        # Check for Ctrl+C combination
        if ctrl_pressed and hasattr(key, 'char') and key.char == 'c':
            print("\n\nCtrl+C detected. Exiting program...")
            exit_event.set()
            return
        
        # Send 0xA0 0x01 for key down event
        ser.write(bytes([0xA0, 0x01]))
        
        # Try to get the character representation
        if hasattr(key, 'char') and key.char is not None:
            print(f"Key DOWN: '{key.char}' -> Sent: 0xA0 0x01")
        else:
            # Special keys (like Ctrl, Alt, etc.)
            print(f"Key DOWN: {key.name if hasattr(key, 'name') else key} -> Sent: 0xA0 0x01")
    except AttributeError:
        # Some keys don't have a char attribute
        print(f"Key DOWN: {key} -> Sent: 0xA0 0x01")
    except Exception as e:
        print(f"Error sending key down: {e}")


def on_key_release(key, ser, exit_event):
    """Callback function for key up events."""
    global ctrl_pressed
    
    try:
        # Check for Ctrl key release
        if key == keyboard.Key.ctrl_l or key == keyboard.Key.ctrl_r:
            ctrl_pressed = False
        
        # Send 0xA0 0x00 for key up event
        ser.write(bytes([0xA0, 0x00]))
        
        # Try to get the character representation
        if hasattr(key, 'char') and key.char is not None:
            print(f"Key UP: '{key.char}' -> Sent: 0xA0 0x00")
        else:
            # Special keys (like Ctrl, Alt, etc.)
            print(f"Key UP: {key.name if hasattr(key, 'name') else key} -> Sent: 0xA0 0x00")
    except AttributeError:
        # Some keys don't have a char attribute
        print(f"Key UP: {key} -> Sent: 0xA0 0x00")
    except Exception as e:
        print(f"Error sending key up: {e}")


def main():
    """Main function to start keyboard event monitoring."""
    print("Keyboard Event Detector Started")
    print(f"Opening serial port {SERIAL_PORT} at {SERIAL_BAUD} baud...")
    
    try:
        # Open serial port at specified baud rate
        ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=1)
        print("Serial port opened successfully!")
        print("Press any key to see key down/up events")
        print("Press Ctrl+C to exit\n")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        print(f"Make sure {SERIAL_PORT} is available and not in use by another program.")
        sys.exit(1)
    
    # Event to signal exit
    exit_event = threading.Event()
    
    # Create wrapper functions that include serial port and exit event
    def on_press_wrapper(key):
        on_key_press(key, ser, exit_event)
    
    def on_release_wrapper(key):
        on_key_release(key, ser, exit_event)
    
    # Create keyboard listeners
    listener = keyboard.Listener(
        on_press=on_press_wrapper,
        on_release=on_release_wrapper,
        suppress=False  # Don't suppress system keys so Ctrl+C works
    )
    
    try:
        # Start the listener
        listener.start()
        
        # Keep the program running, waiting for exit event or KeyboardInterrupt
        try:
            # Wait for exit event with timeout to allow KeyboardInterrupt
            while not exit_event.wait(timeout=0.1):
                pass
        except KeyboardInterrupt:
            print("\n\nKeyboardInterrupt received. Exiting program...")
        
        # Exit requested
        print("\nExiting program...")
        listener.stop()
        ser.close()
        print("Serial port closed.")
        sys.exit(0)
    except Exception as e:
        print(f"\nError: {e}")
        listener.stop()
        ser.close()
        sys.exit(1)


if __name__ == "__main__":
    main()

