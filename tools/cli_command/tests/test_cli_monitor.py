#!/usr/bin/env python3
# coding=utf-8
import io
import unittest
from unittest.mock import patch

import serial


class _FailingReadSerial:
    in_waiting = 1

    def read(self, size=1):
        raise serial.SerialException(
            "device reports readiness to read but returned no data"
        )


class _OSErrorReadSerial:
    @property
    def in_waiting(self):
        raise OSError(5, "Input/output error")

    def read(self, size=1):
        return b""


class _FailingWriteSerial:
    def write(self, data):
        raise serial.SerialException("write failed")


class _FailingCancelConsole:
    def cancel(self):
        raise OSError(5, "Input/output error")


class _OneKeyConsole(_FailingCancelConsole):
    def getkey(self):
        return "x"


class _TextEncoder:
    def encode(self, text):
        return text.encode("utf-8")


class TestFriendlyMiniterm(unittest.TestCase):
    def _make_reader_term(self):
        import tools.cli_command.cli_monitor as m

        term = object.__new__(m._FriendlyMiniterm)
        term.alive = True
        term._reader_alive = True
        term._disconnect_reported = False
        term.port = "/dev/ttyUSB0"
        term.serial = _FailingReadSerial()
        term.console = _FailingCancelConsole()
        term.raw = False
        return term

    def _make_os_error_reader_term(self):
        term = self._make_reader_term()
        term.serial = _OSErrorReadSerial()
        return term

    def _make_writer_term(self):
        import tools.cli_command.cli_monitor as m

        term = object.__new__(m._FriendlyMiniterm)
        term.alive = True
        term._disconnect_reported = False
        term.port = "/dev/ttyUSB0"
        term.serial = _FailingWriteSerial()
        term.console = _OneKeyConsole()
        term.menu_character = "\x14"
        term.exit_character = "\x1d"
        term.tx_transformations = []
        term.tx_encoder = _TextEncoder()
        term.echo = False
        return term

    def test_reader_reports_disconnect_without_traceback(self):
        term = self._make_reader_term()
        stderr = io.StringIO()

        with patch("tools.cli_command.cli_monitor.sys.stderr", stderr):
            term.reader()

        output = stderr.getvalue()
        self.assertFalse(term.alive)
        self.assertIn("Monitor stopped", output)
        self.assertIn("/dev/ttyUSB0", output)
        self.assertNotIn("Traceback", output)

    def test_writer_reports_disconnect_without_traceback(self):
        term = self._make_writer_term()
        stderr = io.StringIO()

        with patch("tools.cli_command.cli_monitor.sys.stderr", stderr):
            term.writer()

        output = stderr.getvalue()
        self.assertFalse(term.alive)
        self.assertIn("Monitor stopped", output)
        self.assertIn("/dev/ttyUSB0", output)
        self.assertNotIn("Traceback", output)

    def test_reader_handles_os_error_without_traceback(self):
        term = self._make_os_error_reader_term()
        stderr = io.StringIO()

        with patch("tools.cli_command.cli_monitor.sys.stderr", stderr):
            term.reader()

        output = stderr.getvalue()
        self.assertFalse(term.alive)
        self.assertIn("Monitor stopped", output)
        self.assertNotIn("Traceback", output)


if __name__ == "__main__":
    unittest.main()
