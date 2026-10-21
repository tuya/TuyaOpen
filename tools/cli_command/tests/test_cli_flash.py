#!/usr/bin/env python3
# coding=utf-8
import io
import sys
import unittest
from unittest.mock import MagicMock, patch


class TestRenderBar(unittest.TestCase):
    def setUp(self):
        import tools.cli_command.cli_flash as m
        self.render = m._render_bar

    def test_starts_with_carriage_return(self):
        self.assertTrue(self.render(0).startswith('\r'))

    def test_zero_percent_all_dashes(self):
        result = self.render(0, width=10)
        self.assertIn('-' * 10, result)
        self.assertIn('  0%', result)

    def test_hundred_percent_all_hashes(self):
        result = self.render(100, width=10)
        self.assertIn('#' * 10, result)
        self.assertIn('100%', result)

    def test_fifty_percent_half_filled(self):
        result = self.render(50, width=10)
        self.assertIn('#####', result)
        self.assertIn('-----', result)

    def test_format_has_brackets(self):
        result = self.render(30)
        self.assertIn('[', result)
        self.assertIn(']', result)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_mock_proc(lines, returncode=0):
    proc = MagicMock()
    proc.stdout = iter(lines)
    proc.wait.return_value = None
    proc.returncode = returncode
    return proc


def _run_pipe(lines, returncode=0):
    """Run _do_flash_pipe with fake subprocess output, return (ret, stdout)."""
    import tools.cli_command.cli_flash as m
    mock_proc = _make_mock_proc(lines, returncode)
    buf = io.StringIO()
    with patch('tools.cli_command.cli_flash.subprocess.Popen',
               return_value=mock_proc), \
         patch('tools.cli_command.cli_flash.get_logger',
               return_value=MagicMock()), \
         patch('tools.cli_command.cli_flash.sys.stdout', buf):
        ret = m._do_flash_pipe("fake cmd")
    return ret, buf.getvalue()


# ---------------------------------------------------------------------------
# _do_flash_pipe: output parsing
# ---------------------------------------------------------------------------

class TestDoFlashPipeOutputParsing(unittest.TestCase):

    def test_phase_line_is_printed(self):
        _, out = _run_pipe(['[phase] Handshake\n'])
        self.assertIn('[phase] Handshake', out)

    def test_two_phases_both_printed(self):
        _, out = _run_pipe(['[phase] Handshake\n', '[phase] Flash ID\n'])
        self.assertIn('[phase] Handshake', out)
        self.assertIn('[phase] Flash ID', out)

    def test_progress_line_not_printed_as_raw_text(self):
        # [progress] lines should be rendered as bar, never appear as-is
        _, out = _run_pipe(['[phase] Erase\n', '[progress] 50%\n'])
        self.assertNotIn('[progress]', out)

    def test_progress_line_renders_percentage(self):
        _, out = _run_pipe(['[phase] Erase\n', '[progress] 50%\n'])
        self.assertIn('50%', out)

    def test_plain_tyutool_output_passes_through(self):
        # lines that match neither regex should be printed verbatim
        _, out = _run_pipe(['Handshake         OK\n', 'Flash OK  12.3s\n'])
        self.assertIn('Handshake', out)
        self.assertIn('Flash OK', out)

    def test_returns_zero_on_success(self):
        ret, _ = _run_pipe(['[phase] Handshake\n'])
        self.assertEqual(ret, 0)

    def test_returns_nonzero_on_popen_failure(self):
        import tools.cli_command.cli_flash as m
        with patch('tools.cli_command.cli_flash.subprocess.Popen',
                   side_effect=OSError("no binary")), \
             patch('tools.cli_command.cli_flash.get_logger',
                   return_value=MagicMock()):
            ret = m._do_flash_pipe("bad cmd")
        self.assertNotEqual(ret, 0)

    def test_empty_progress_lines_ignored(self):
        # duplicate percentages should not add extra output
        _, out1 = _run_pipe(['[phase] Write\n', '[progress] 50%\n'])
        _, out2 = _run_pipe(['[phase] Write\n', '[progress] 50%\n',
                             '[progress] 50%\n'])
        self.assertEqual(out1, out2)


# ---------------------------------------------------------------------------
# do_flash_subprocess: dispatch logic
# ---------------------------------------------------------------------------

class TestDoFlashSubprocessDispatch(unittest.TestCase):

    def test_empty_cmd_returns_zero_without_subprocess(self):
        import tools.cli_command.cli_flash as m
        with patch('tools.cli_command.cli_flash.subprocess.Popen') as mock_popen, \
             patch('tools.cli_command.cli_flash.get_logger',
                   return_value=MagicMock()):
            ret = m.do_flash_subprocess("")
        mock_popen.assert_not_called()
        self.assertEqual(ret, 0)

    def test_on_windows_calls_pipe_not_pty(self):
        import tools.cli_command.cli_flash as m
        with patch('tools.cli_command.cli_flash.sys.platform', 'win32'), \
             patch('tools.cli_command.cli_flash._do_flash_pipe',
                   return_value=0) as mock_pipe, \
             patch('tools.cli_command.cli_flash._do_flash_pty') as mock_pty, \
             patch('tools.cli_command.cli_flash.get_logger',
                   return_value=MagicMock()):
            m.do_flash_subprocess("some cmd")
        mock_pipe.assert_called_once_with("some cmd")
        mock_pty.assert_not_called()

    def test_on_linux_calls_pty_first(self):
        import tools.cli_command.cli_flash as m
        with patch('tools.cli_command.cli_flash.sys.platform', 'linux'), \
             patch('tools.cli_command.cli_flash._do_flash_pty',
                   return_value=0) as mock_pty, \
             patch('tools.cli_command.cli_flash._do_flash_pipe',
                   return_value=0) as mock_pipe, \
             patch('tools.cli_command.cli_flash.get_logger',
                   return_value=MagicMock()):
            m.do_flash_subprocess("some cmd")
        mock_pty.assert_called_once_with("some cmd")
        mock_pipe.assert_not_called()

    def test_on_linux_pty_failure_falls_back_to_pipe(self):
        import tools.cli_command.cli_flash as m
        with patch('tools.cli_command.cli_flash.sys.platform', 'linux'), \
             patch('tools.cli_command.cli_flash._do_flash_pty',
                   side_effect=Exception("no pty")), \
             patch('tools.cli_command.cli_flash._do_flash_pipe',
                   return_value=0) as mock_pipe, \
             patch('tools.cli_command.cli_flash.get_logger',
                   return_value=MagicMock()):
            ret = m.do_flash_subprocess("some cmd")
        mock_pipe.assert_called_once_with("some cmd")
        self.assertEqual(ret, 0)


# ---------------------------------------------------------------------------
# Stream decoding & safe print (Windows CP936 / UTF-8 robustness)
# ---------------------------------------------------------------------------

class TestStreamDecodingAndSafePrint(unittest.TestCase):

    def test_decode_utf8_chinese(self):
        import tools.cli_command.cli_flash as m
        raw = '这是一个测试中文字符串'.encode('utf-8')
        decoded = m._decode_stream_line(raw)
        self.assertEqual(decoded, '这是一个测试中文字符串')

    def test_decode_gbk_chinese(self):
        import tools.cli_command.cli_flash as m
        with patch('tools.cli_command.cli_flash.sys.platform', 'win32'), \
             patch('tools.cli_command.cli_flash.locale.getpreferredencoding',
                   return_value='cp936'):
            raw = '这是一个测试中文字符串'.encode('gbk')
            decoded = m._decode_stream_line(raw)
            self.assertEqual(decoded, '这是一个测试中文字符串')

    def test_decode_malformed_lone_byte(self):
        import tools.cli_command.cli_flash as m
        with patch('tools.cli_command.cli_flash.sys.platform', 'win32'), \
             patch('tools.cli_command.cli_flash.locale.getpreferredencoding',
                   return_value='cp936'):
            raw = b'err: \xd2 test\n'
            decoded = m._decode_stream_line(raw)
            self.assertIn('\ufffd', decoded)
            self.assertTrue(decoded.startswith('err:'))

    def test_safe_print_does_not_crash_on_strict_gbk_stream(self):
        import tools.cli_command.cli_flash as m

        class FakeStrictGbkStdout(io.TextIOWrapper):
            def __init__(self):
                super().__init__(io.BytesIO(), encoding='gbk', errors='strict')

        mock_stdout = FakeStrictGbkStdout()
        with patch('tools.cli_command.cli_flash.sys.stdout', mock_stdout):
            # Print string containing \ufffd that cannot be represented in GBK
            m._safe_print('err: \ufffd test')
            # Print Chinese string that can be represented in GBK
            m._safe_print('中文测试')
            mock_stdout.flush()

        output_bytes = mock_stdout.buffer.getvalue()
        self.assertIn(b'err: ? test\n', output_bytes)
        self.assertIn('中文测试'.encode('gbk'), output_bytes)

    def test_pipe_runs_with_byte_streams_without_error(self):
        # End-to-end pipe test with byte lines containing both valid UTF-8 and lone 0xd2
        lines = [
            b'[phase] Handshake\r\n',
            b'[progress] 50%\r\n',
            b'err: \xd2 test\r\n',
            '这是一个UTF-8测试\n'.encode('utf-8'),
        ]
        ret, out = _run_pipe(lines, returncode=0)
        self.assertEqual(ret, 0)
        self.assertIn('[phase] Handshake', out)
        self.assertIn('50%', out)
        self.assertIn('这是一个UTF-8测试', out)


if __name__ == '__main__':
    unittest.main()
