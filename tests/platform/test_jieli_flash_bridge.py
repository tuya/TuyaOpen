import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "platform" / "JIELI"))

import platform_flash_bridge


class _Logger:
    def __init__(self):
        self.messages = []

    def info(self, message):
        self.messages.append(message)


class JieliFlashBridgeTest(unittest.TestCase):
    def test_windows_command_keeps_backslashes_and_quotes(self):
        with patch.object(platform_flash_bridge.os, "name", "nt"):
            command = platform_flash_bridge._split_command(
                r'"C:\Program Files\Jieli\isd_download.exe" --file "D:\build dir\app.bin"'
            )
        self.assertEqual(
            command,
            [
                r"C:\Program Files\Jieli\isd_download.exe",
                "--file",
                r"D:\build dir\app.bin",
            ],
        )

    def test_missing_uploader_is_reported(self):
        logger = _Logger()
        result = platform_flash_bridge.platform_flash(
            using_data={},
            binfile="/missing/jieli.bin",
            port="/dev/ttyUSB0",
            baud=115200,
            boards_root="/boards",
            logger=logger,
        )
        self.assertFalse(result["success"])
        self.assertIn("firmware image not found", result["message"])

    def test_default_windows_downloader_is_constructed(self):
        with patch.object(platform_flash_bridge.os, "name", "nt"):
            command = platform_flash_bridge._default_flash_command(pathlib.Path("D:/build/app.bin"))
        self.assertIsNotNone(command)
        args, tools_dir = command
        self.assertEqual(args[0], str(tools_dir / "isd_download.exe"))
        self.assertIn("-dev", args)
        self.assertIn("wl82", args)
        self.assertIn("-res", args)
        self.assertIn("cfg", args)
        self.assertIn(str(tools_dir / "uboot.boot"), args)
        self.assertIn("D:\\build\\app.bin", args)

    def test_platform_flash_runs_sdk_downloader_by_default(self):
        logger = _Logger()
        with tempfile.TemporaryDirectory() as temp:
            image = pathlib.Path(temp) / "app.bin"
            image.write_bytes(b"firmware")
            completed = type("Completed", (), {"returncode": 0})()
            with patch.object(platform_flash_bridge.os, "name", "nt"), \
                    patch.dict(platform_flash_bridge.os.environ, {"JIELI_FLASH_CMD": ""}), \
                    patch.object(platform_flash_bridge.subprocess, "run", return_value=completed) as run:
                result = platform_flash_bridge.platform_flash(
                    using_data={"CONFIG_BOARD_CHOICE": "AC7916A"},
                    binfile=str(image),
                    port="",
                    baud=0,
                    boards_root="/boards",
                    logger=logger,
                )

        self.assertTrue(result["success"])
        command = run.call_args.args[0]
        self.assertTrue(command[0].endswith("isd_download.exe"))
        self.assertIn("-app", command)
        self.assertIn(str(image), command)
        self.assertEqual(run.call_args.kwargs["cwd"].name, "tools")


if __name__ == "__main__":
    unittest.main()
