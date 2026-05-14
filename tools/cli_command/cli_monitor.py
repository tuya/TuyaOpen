#!/usr/bin/env python3
# coding=utf-8

import sys
import click
import serial
from serial.tools.miniterm import Miniterm

from tools.cli_command.util import (
    get_logger, get_global_params, check_proj_dir,
    parse_config_file,
)
from tools.cli_command.cli_flash import (
    get_configure_baudrate
)

_DEFAULT_BAUDRATE = 115200

# Per-chip monitor baudrate defaults, matching old tyutool FlashInterface
_CHIP_MONITOR_BAUDRATE = {
    "T5": 460800,
    "T5AI": 460800,
}


def _choose_port() -> str:
    from serial.tools import list_ports
    ports = [p.device for p in list_ports.comports()
             if not p.device.startswith("/dev/ttyS")]
    if not ports:
        return ""
    ports.sort()
    if len(ports) == 1:
        return ports[0]
    print("--------------------")
    for i, p in enumerate(ports):
        print(f"{i+1}. {p}")
    print("--------------------")
    while True:
        try:
            num = int(input("Select serial port: "))
            if 1 <= num <= len(ports):
                return ports[num - 1]
        except ValueError:
            continue
        except KeyboardInterrupt:
            sys.exit(0)


##
# @brief tos.py monitor
#
@click.command(help="Display the device log.")
@click.option('-p', '--port',
              type=str, default="",
              help="Target port.")
@click.option('-b', '--baud',
              type=int, default=0,
              help="Uart baud rate.")
def cli(port, baud):
    logger = get_logger()
    check_proj_dir()

    params = get_global_params()
    using_config = params["using_config"]
    using_data = parse_config_file(using_config)

    baudrate = get_configure_baudrate(
        using_data, "CONFIG_MONITOR_BAUDRATE", baud)
    if not baudrate:
        platform = using_data.get("CONFIG_PLATFORM_CHOICE", "")
        chip = using_data.get("CONFIG_CHIP_CHOICE", "")
        device = (chip or platform).upper()
        baudrate = _CHIP_MONITOR_BAUDRATE.get(device, _DEFAULT_BAUDRATE)

    if not port:
        port = _choose_port()
        if not port:
            logger.error("No serial port found. Use -p to specify a port.")
            sys.exit(1)

    logger.info(f"Monitor: port={port}, baudrate={baudrate}")

    try:
        ser = serial.Serial(port, baudrate, timeout=1)
    except serial.SerialException as e:
        logger.error(f"Open port failed: {e}")
        sys.exit(1)

    ser.reset_input_buffer()

    miniterm = Miniterm(ser, filters=('direct',))
    miniterm.set_rx_encoding('utf-8', 'replace')
    miniterm.set_tx_encoding('utf-8', 'replace')
    miniterm.exit_character = chr(0x1d)  # Ctrl+]
    miniterm.menu_character = chr(0x14)  # Ctrl+T
    miniterm.start()
    sys.stderr.write(f'--- Monitor {port}  {baudrate} baud --- Quit: Ctrl+] ---\r\n')
    try:
        miniterm.join(True)
    except KeyboardInterrupt:
        pass
    finally:
        miniterm.join()
        miniterm.close()
    sys.exit(0)
