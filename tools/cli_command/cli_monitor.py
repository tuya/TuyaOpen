#!/usr/bin/env python3
# coding=utf-8
#
# Usage examples:
#   tos.py monitor                              # auto-detect port, auto baud
#   tos.py monitor -p /dev/ttyUSB0             # specify port
#   tos.py monitor -p /dev/ttyUSB0 -b 115200   # specify port and baud rate
#   tos.py monitor -l device.log               # save log to file (append)
#   tos.py monitor -p /dev/ttyUSB0 -l out.log  # specify port and log file
#   Quit: Ctrl+]

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


class _LoggingSerial:
    """Proxy that forwards all serial calls and tees read data to a file."""

    def __init__(self, ser, logfile):
        self._ser = ser
        self._logfile = logfile

    def __getattr__(self, name):
        return getattr(self._ser, name)

    def read(self, size=1):
        data = self._ser.read(size)
        if data:
            self._logfile.write(data)
            self._logfile.flush()
        return data


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
@click.option('-l', '--log',
              type=click.Path(dir_okay=False, writable=True), default=None,
              help="Save received log to file.")
def cli(port, baud, log):
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
    if log:
        logger.info(f"Log file: {log}")

    try:
        ser = serial.Serial(port, baudrate, timeout=1)
    except serial.SerialException as e:
        logger.error(f"Open port failed: {e}")
        sys.exit(1)

    ser.reset_input_buffer()

    logfile = open(log, 'ab') if log else None
    try:
        serial_obj = _LoggingSerial(ser, logfile) if logfile else ser
        miniterm = Miniterm(serial_obj, filters=('direct',))
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
    finally:
        if logfile:
            logfile.close()
    sys.exit(0)
