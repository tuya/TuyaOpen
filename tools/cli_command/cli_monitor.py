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
import time
import click
import serial
from serial.tools.miniterm import Miniterm

from tools.cli_command.util import (
    get_logger, get_global_params, check_proj_dir,
    parse_config_file,
)
from tools.cli_command.cli_config import init_using_config
from tools.cli_command.cli_flash import (
    get_configure_baudrate
)

_DEFAULT_BAUDRATE = 115200

# Per-chip monitor baudrate defaults, matching old tyutool FlashInterface
_CHIP_MONITOR_BAUDRATE = {
    "T5": 460800,
    "T5AI": 460800,
}

_SERIAL_IO_EXCEPTIONS = (serial.SerialException, OSError)


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


class _FriendlyMiniterm(Miniterm):
    """Miniterm variant that exits cleanly when the serial device disappears."""

    def __init__(self, serial_instance, port, **kwargs):
        super().__init__(serial_instance, **kwargs)
        self.port = port
        self._disconnect_reported = False

    def _report_disconnect(self, exc):
        if self._disconnect_reported:
            return
        self._disconnect_reported = True

        detail = str(exc).strip()
        sys.stderr.write(
            f"\r\n--- Monitor stopped: serial port {self.port} "
            "disconnected or unavailable. ---\r\n"
        )
        if detail:
            sys.stderr.write(f"--- Detail: {detail} ---\r\n")
        sys.stderr.flush()

    def _cancel_console_quietly(self):
        try:
            self.console.cancel()
        except OSError:
            pass

    def reader(self):
        """Loop and copy serial->console."""
        try:
            while self.alive and self._reader_alive:
                data = self.serial.read(self.serial.in_waiting or 1)
                if data:
                    if self.raw:
                        self.console.write_bytes(data)
                    else:
                        text = self.rx_decoder.decode(data)
                        for transformation in self.rx_transformations:
                            text = transformation.rx(text)
                        self.console.write(text)
        except _SERIAL_IO_EXCEPTIONS as exc:
            self.alive = False
            self._report_disconnect(exc)
            self._cancel_console_quietly()

    def writer(self):
        """Loop and copy console->serial until the exit character is found."""
        menu_active = False
        try:
            while self.alive:
                try:
                    c = self.console.getkey()
                except KeyboardInterrupt:
                    c = '\x03'
                if not self.alive:
                    break
                if menu_active:
                    self.handle_menu_key(c)
                    menu_active = False
                elif c == self.menu_character:
                    menu_active = True
                elif c == self.exit_character:
                    self.stop()
                    break
                else:
                    text = c
                    for transformation in self.tx_transformations:
                        text = transformation.tx(text)
                    self.serial.write(self.tx_encoder.encode(text))
                    if self.echo:
                        echo_text = c
                        for transformation in self.tx_transformations:
                            echo_text = transformation.echo(echo_text)
                        self.console.write(echo_text)
        except _SERIAL_IO_EXCEPTIONS as exc:
            self.alive = False
            self._report_disconnect(exc)
            self._cancel_console_quietly()


def _close_miniterm(miniterm):
    miniterm.alive = False
    if hasattr(miniterm.serial, 'cancel_read'):
        try:
            miniterm.serial.cancel_read()
        except _SERIAL_IO_EXCEPTIONS:
            pass

    if miniterm.receiver_thread and miniterm.receiver_thread.is_alive():
        miniterm.receiver_thread.join(timeout=1)
    if miniterm.transmitter_thread and miniterm.transmitter_thread.is_alive():
        miniterm._cancel_console_quietly()
        miniterm.transmitter_thread.join(timeout=1)

    try:
        miniterm.console.cleanup()
    except OSError:
        pass
    try:
        miniterm.close()
    except _SERIAL_IO_EXCEPTIONS:
        pass


def _wait_miniterm(miniterm):
    try:
        while miniterm.alive:
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        _close_miniterm(miniterm)


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
    logger.info("Monitor: Press Ctrl+] to quit.")
    check_proj_dir()

    # Derive using.config from app_default.config (same as `tos.py build`)
    # so the correct board's baudrate is known even if the project has
    # never been built yet.
    init_using_config(force=False)
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
        miniterm = _FriendlyMiniterm(serial_obj, port, filters=('direct',))
        miniterm.set_rx_encoding('utf-8', 'replace')
        miniterm.set_tx_encoding('utf-8', 'replace')
        miniterm.exit_character = chr(0x1d)  # Ctrl+]
        miniterm.menu_character = chr(0x14)  # Ctrl+T
        miniterm.start()
        sys.stderr.write(f'--- Monitor {port}  {baudrate} baud --- Quit: Ctrl+] ---\r\n')
        _wait_miniterm(miniterm)
    finally:
        if logfile:
            logfile.close()
    sys.exit(0)
