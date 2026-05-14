#!/usr/bin/env python3
# coding=utf-8
import os
import sys
import time
import tempfile
import unittest
import requests
from unittest.mock import patch


class TestGetPlatformKey(unittest.TestCase):
    def _call(self, system, machine):
        import tools.cli_command.util_tyutool as m
        with patch('tools.cli_command.util_tyutool.platform.system', return_value=system), \
             patch('tools.cli_command.util_tyutool.platform.machine', return_value=machine):
            return m.get_platform_key()

    def test_linux_x86_64(self):
        self.assertEqual(self._call('Linux', 'x86_64'), 'linux-x86_64')

    def test_linux_amd64(self):
        self.assertEqual(self._call('Linux', 'AMD64'), 'linux-x86_64')

    def test_linux_aarch64(self):
        self.assertEqual(self._call('Linux', 'aarch64'), 'linux-aarch64')

    def test_macos_arm64(self):  # Mac M-series
        self.assertEqual(self._call('Darwin', 'arm64'), 'darwin-aarch64')

    def test_macos_x86_64(self):
        self.assertEqual(self._call('Darwin', 'x86_64'), 'darwin-x86_64')

    def test_windows_any_arch(self):
        self.assertEqual(self._call('Windows', 'AMD64'), 'windows-x86_64')

    def test_unsupported_arch_raises(self):
        with self.assertRaises(RuntimeError):
            self._call('Linux', 'mips')

    def test_unsupported_os_raises(self):
        with self.assertRaises(RuntimeError):
            self._call('FreeBSD', 'x86_64')


class TestCompareVersions(unittest.TestCase):
    def setUp(self):
        from tools.cli_command import util_tyutool as m
        self.compare = m.compare_versions

    def test_equal(self):
        self.assertEqual(self.compare('3.0.7', '3.0.7'), 0)

    def test_newer_patch(self):
        self.assertEqual(self.compare('3.0.8', '3.0.7'), 1)

    def test_older_patch(self):
        self.assertEqual(self.compare('3.0.6', '3.0.7'), -1)

    def test_double_digit_patch(self):
        # String compare would wrongly say '3.0.9' > '3.0.10'
        self.assertEqual(self.compare('3.0.10', '3.0.9'), 1)

    def test_v_prefix_stripped(self):
        self.assertEqual(self.compare('v3.0.7', '3.0.7'), 0)

    def test_minor_bump(self):
        self.assertEqual(self.compare('3.1.0', '3.0.9'), 1)

    def test_major_bump(self):
        self.assertEqual(self.compare('4.0.0', '3.9.9'), 1)

    def test_malformed_returns_zero_tuple(self):
        self.assertEqual(self.compare('bad', 'bad'), 0)


class TestShouldCheckUpdate(unittest.TestCase):
    def setUp(self):
        import tools.cli_command.util_tyutool as m
        self.fn = m.should_check_update

    def test_never_checked_returns_true(self):
        with patch('tools.cli_command.util_tyutool.env_read', return_value=0):
            self.assertTrue(self.fn())

    def test_just_checked_returns_false(self):
        with patch('tools.cli_command.util_tyutool.env_read', return_value=time.time()):
            self.assertFalse(self.fn())

    def test_checked_25h_ago_returns_true(self):
        old_time = time.time() - (25 * 3600)
        with patch('tools.cli_command.util_tyutool.env_read', return_value=old_time):
            self.assertTrue(self.fn())

    def test_checked_23h_ago_returns_false(self):
        recent_time = time.time() - (23 * 3600)
        with patch('tools.cli_command.util_tyutool.env_read', return_value=recent_time):
            self.assertFalse(self.fn())


class TestFetchLatestJson(unittest.TestCase):
    def setUp(self):
        import tools.cli_command.util_tyutool as m
        self.fn = m.fetch_latest_json

    def test_success_returns_dict(self):
        fake_json = {"version": "3.0.7", "cli": {}}
        mock_resp = unittest.mock.MagicMock()
        mock_resp.json.return_value = fake_json
        mock_resp.raise_for_status.return_value = None
        with patch('tools.cli_command.util_tyutool.requests.get',
                   return_value=mock_resp):
            result = self.fn()
        self.assertEqual(result, fake_json)

    def test_network_error_returns_none(self):
        with patch('tools.cli_command.util_tyutool.requests.get',
                   side_effect=requests.exceptions.ConnectionError("no network")):
            result = self.fn()
        self.assertIsNone(result)

    def test_timeout_returns_none(self):
        with patch('tools.cli_command.util_tyutool.requests.get',
                   side_effect=requests.exceptions.Timeout()):
            result = self.fn()
        self.assertIsNone(result)

    def test_http_error_returns_none(self):
        mock_resp = unittest.mock.MagicMock()
        mock_resp.raise_for_status.side_effect = requests.exceptions.HTTPError("429")
        with patch('tools.cli_command.util_tyutool.requests.get',
                   return_value=mock_resp):
            result = self.fn()
        self.assertIsNone(result)


class TestGetLocalVersion(unittest.TestCase):
    def setUp(self):
        import tools.cli_command.util_tyutool as m
        self.fn = m.get_local_version

    def test_returns_stored_version(self):
        with patch('tools.cli_command.util_tyutool.env_read',
                   return_value='3.0.7'):
            self.assertEqual(self.fn(), '3.0.7')

    def test_returns_none_when_not_installed(self):
        with patch('tools.cli_command.util_tyutool.env_read',
                   return_value=None):
            self.assertIsNone(self.fn())


class TestDownloadTyutoolBin(unittest.TestCase):
    FAKE_LATEST = {
        "version": "3.0.7",
        "cli": {
            "linux-x86_64": {
                "url": "https://github.com/tuya/tyutool/releases/download/v3.0.7/tyutool-cli_linux_x86_64_3.0.7.tar.gz",
                "sha256": "FAKE_SHA256",
            }
        }
    }

    def _make_params(self, tmp_path):
        return {
            "tyutool_bin_dir": str(tmp_path),
            "tyutool_bin": os.path.join(str(tmp_path), "tyutool_cli"),
        }

    def test_unsupported_platform_returns_false(self):
        import tools.cli_command.util_tyutool as m
        latest = {"version": "3.0.7", "cli": {}}
        with patch('tools.cli_command.util_tyutool.get_global_params',
                   return_value={"tyutool_bin_dir": "/tmp/t", "tyutool_bin": "/tmp/t/tyutool_cli"}), \
             patch('tools.cli_command.util_tyutool.get_platform_key',
                   return_value="linux-x86_64"), \
             patch('tools.cli_command.util_tyutool.get_country_code',
                   return_value="US"):
            result = m.download_tyutool_bin(latest)
        self.assertFalse(result)

    def test_sha256_mismatch_returns_false(self):
        import tools.cli_command.util_tyutool as m
        with tempfile.TemporaryDirectory() as tmp:
            params = self._make_params(tmp)
            def fake_download(url, dest):
                with open(dest, 'wb') as f:
                    f.write(b"fake content")
                return True
            with patch('tools.cli_command.util_tyutool.get_global_params',
                       return_value=params), \
                 patch('tools.cli_command.util_tyutool.get_platform_key',
                       return_value="linux-x86_64"), \
                 patch('tools.cli_command.util_tyutool.get_country_code',
                       return_value="US"), \
                 patch('tools.cli_command.util_tyutool.env_write'), \
                 patch('tools.cli_command.util_tyutool._download_file',
                       side_effect=fake_download):
                result = m.download_tyutool_bin(self.FAKE_LATEST)
        self.assertFalse(result)

    def test_all_sources_fail_returns_false(self):
        import tools.cli_command.util_tyutool as m
        with tempfile.TemporaryDirectory() as tmp:
            params = self._make_params(tmp)
            with patch('tools.cli_command.util_tyutool.get_global_params',
                       return_value=params), \
                 patch('tools.cli_command.util_tyutool.get_platform_key',
                       return_value="linux-x86_64"), \
                 patch('tools.cli_command.util_tyutool.get_country_code',
                       return_value="US"), \
                 patch('tools.cli_command.util_tyutool._download_file',
                       return_value=False):
                result = m.download_tyutool_bin(self.FAKE_LATEST)
        self.assertFalse(result)

    def test_gitee_prioritized_for_china(self):
        import tools.cli_command.util_tyutool as m
        with tempfile.TemporaryDirectory() as tmp:
            params = self._make_params(tmp)
            tried_urls = []
            def fake_download(url, dest):
                tried_urls.append(url)
                return False
            with patch('tools.cli_command.util_tyutool.get_global_params',
                       return_value=params), \
                 patch('tools.cli_command.util_tyutool.get_platform_key',
                       return_value="linux-x86_64"), \
                 patch('tools.cli_command.util_tyutool.get_country_code',
                       return_value="China"), \
                 patch('tools.cli_command.util_tyutool._download_file',
                       side_effect=fake_download):
                m.download_tyutool_bin(self.FAKE_LATEST)
        self.assertIn("gitee.com", tried_urls[0])
        self.assertIn("github.com", tried_urls[1])

    def test_successful_install(self):
        import io, hashlib, tarfile as _tarfile
        import tools.cli_command.util_tyutool as m

        with tempfile.TemporaryDirectory() as tmp:
            params = self._make_params(tmp)
            # Create a fake tar.gz containing 'tyutool_cli'
            buf = io.BytesIO()
            with _tarfile.open(fileobj=buf, mode='w:gz') as tf:
                content = b"#!/bin/sh\necho hello"
                info = _tarfile.TarInfo(name="tyutool_cli")
                info.size = len(content)
                tf.addfile(info, io.BytesIO(content))
            archive_bytes = buf.getvalue()
            expected_sha = hashlib.sha256(archive_bytes).hexdigest()

            latest = {
                "version": "3.0.7",
                "cli": {
                    "linux-x86_64": {
                        "url": "https://github.com/tuya/tyutool/releases/download/v3.0.7/tyutool-cli_linux_x86_64_3.0.7.tar.gz",
                        "sha256": expected_sha,
                    }
                }
            }

            def fake_download(url, dest):
                with open(dest, 'wb') as f:
                    f.write(archive_bytes)
                return True

            written = {}
            def fake_env_write(key, val):
                written[key] = val

            with patch('tools.cli_command.util_tyutool.get_global_params',
                       return_value=params), \
                 patch('tools.cli_command.util_tyutool.get_platform_key',
                       return_value="linux-x86_64"), \
                 patch('tools.cli_command.util_tyutool.get_country_code',
                       return_value="US"), \
                 patch('tools.cli_command.util_tyutool._download_file',
                       side_effect=fake_download), \
                 patch('tools.cli_command.util_tyutool.env_write',
                       side_effect=fake_env_write):
                result = m.download_tyutool_bin(latest)

            self.assertTrue(result)
            self.assertTrue(os.path.isfile(params["tyutool_bin"]))
            self.assertEqual(written.get("tyutool_version"), "3.0.7")


if __name__ == '__main__':
    unittest.main()
