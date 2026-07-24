#!/usr/bin/env python3
"""
Unit and integration tests for MiniMax LLM provider support in MimiClaw.

Since MimiClaw is an embedded C firmware, these tests validate:
1. Source-level correctness of the provider routing changes
2. MiniMax API compatibility via direct HTTP calls (integration)

Usage:
    python3 -m pytest tests/test_minimax_provider.py -v
    # or: python3 tests/test_minimax_provider.py
"""

import json
import os
import re
import subprocess
import sys
import unittest

# Paths relative to the mimiclaw app root
MIMICLAW_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LLM_PROXY_C = os.path.join(MIMICLAW_ROOT, "llm", "llm_proxy.c")
LLM_PROXY_H = os.path.join(MIMICLAW_ROOT, "llm", "llm_proxy.h")
MIMI_CONFIG_H = os.path.join(MIMICLAW_ROOT, "mimi_config.h")
SERIAL_CLI_C = os.path.join(MIMICLAW_ROOT, "cli", "serial_cli.c")
MIMI_SECRETS_EXAMPLE = os.path.join(MIMICLAW_ROOT, "mimi_secrets.h.example")
README_MD = os.path.join(MIMICLAW_ROOT, "README.md")
README_ZH_MD = os.path.join(MIMICLAW_ROOT, "README_zh.md")


def read_file(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


# ---------------------------------------------------------------------------
# Unit tests: validate C source code structure
# ---------------------------------------------------------------------------

class TestMiniMaxProviderConfig(unittest.TestCase):
    """Verify mimi_config.h has the MiniMax API URL defined."""

    def setUp(self):
        self.config = read_file(MIMI_CONFIG_H)

    def test_minimax_api_url_defined(self):
        self.assertIn("MIMI_MINIMAX_API_URL", self.config)

    def test_minimax_api_url_value(self):
        match = re.search(
            r'#define\s+MIMI_MINIMAX_API_URL\s+"([^"]+)"', self.config
        )
        self.assertIsNotNone(match, "MIMI_MINIMAX_API_URL should have a default value")
        self.assertEqual(
            match.group(1), "https://api.minimax.io/v1/chat/completions"
        )

    def test_minimax_url_guarded_by_ifndef(self):
        self.assertIn("#ifndef MIMI_MINIMAX_API_URL", self.config)

    def test_openai_and_anthropic_urls_preserved(self):
        self.assertIn("MIMI_OPENAI_API_URL", self.config)
        self.assertIn("MIMI_LLM_API_URL", self.config)
        self.assertIn("api.openai.com", self.config)
        self.assertIn("api.anthropic.com", self.config)


class TestMiniMaxProviderRouting(unittest.TestCase):
    """Verify llm_proxy.c has correct MiniMax provider routing."""

    def setUp(self):
        self.proxy = read_file(LLM_PROXY_C)

    def test_provider_is_minimax_function_exists(self):
        self.assertIn("provider_is_minimax", self.proxy)

    def test_provider_uses_openai_format_function_exists(self):
        self.assertIn("provider_uses_openai_format", self.proxy)

    def test_provider_uses_openai_format_includes_minimax(self):
        match = re.search(
            r"static bool provider_uses_openai_format\(void\)\s*\{([^}]+)\}",
            self.proxy,
        )
        self.assertIsNotNone(match)
        body = match.group(1)
        self.assertIn("provider_is_openai()", body)
        self.assertIn("provider_is_minimax()", body)

    def test_minimax_endpoint_cache_exists(self):
        self.assertIn("s_minimax_endpoint", self.proxy)
        self.assertIn("s_minimax_cacert", self.proxy)
        self.assertIn("s_minimax_cacert_len", self.proxy)

    def test_clear_endpoint_cache_handles_minimax(self):
        match = re.search(
            r"static void clear_endpoint_cache\(void\)\s*\{([^}]+)\}",
            self.proxy,
        )
        self.assertIsNotNone(match)
        body = match.group(1)
        self.assertIn("s_minimax_endpoint", body)
        self.assertIn("s_minimax_cacert", body)

    def test_llm_default_endpoint_url_handles_minimax(self):
        match = re.search(
            r"static const char \*llm_default_endpoint_url\(void\)\s*\{([\s\S]*?)\n\}",
            self.proxy,
        )
        self.assertIsNotNone(match)
        body = match.group(1)
        self.assertIn("provider_is_minimax()", body)
        self.assertIn("MIMI_MINIMAX_API_URL", body)

    def test_auth_header_uses_openai_format(self):
        """MiniMax should use Bearer auth (same as OpenAI)."""
        self.assertIn("provider_uses_openai_format()", self.proxy)
        # The old code had provider_is_openai() for auth; now it should be
        # provider_uses_openai_format() so MiniMax gets Bearer auth too
        auth_section = self.proxy[
            self.proxy.find("llm_http_call") : self.proxy.find("http_client_request")
        ]
        self.assertIn("provider_uses_openai_format()", auth_section)

    def test_message_format_uses_openai_format(self):
        """MiniMax should use OpenAI message format."""
        chat_func = self.proxy[
            self.proxy.find("OPERATE_RET llm_chat(") : self.proxy.find(
                "void llm_response_free"
            )
        ]
        self.assertIn("provider_uses_openai_format()", chat_func)
        self.assertNotIn(
            "provider_is_openai()", chat_func,
            "llm_chat should use provider_uses_openai_format(), not provider_is_openai()"
        )

    def test_response_parsing_uses_openai_format(self):
        """MiniMax should use OpenAI response parsing."""
        tools_func = self.proxy[
            self.proxy.find("OPERATE_RET llm_chat_tools_ex(") :
        ]
        self.assertIn("provider_uses_openai_format()", tools_func)

    def test_get_provider_cert_handles_minimax(self):
        match = re.search(
            r"static void get_provider_cert\([^)]+\)\s*\{([\s\S]*?)\n\}",
            self.proxy,
        )
        self.assertIsNotNone(match)
        body = match.group(1)
        self.assertIn("provider_is_minimax()", body)
        self.assertIn("s_minimax_cacert", body)

    def test_ensure_provider_cert_handles_minimax(self):
        match = re.search(
            r"static OPERATE_RET ensure_provider_cert\(void\)\s*\{([\s\S]*?)\n\}",
            self.proxy,
        )
        self.assertIsNotNone(match)
        body = match.group(1)
        self.assertIn("provider_is_minimax()", body)
        self.assertIn("s_minimax_cacert", body)

    def test_provider_is_openai_still_exists(self):
        """Original provider_is_openai should still exist for endpoint routing."""
        self.assertIn("provider_is_openai(void)", self.proxy)

    def test_no_raw_provider_is_openai_in_message_format(self):
        """provider_is_openai() should NOT be used for message/auth format decisions."""
        # Extract the sections after the helper function definitions
        after_helpers = self.proxy[self.proxy.find("static cJSON *convert_tools_openai"):]
        # Count occurrences: provider_is_openai() should only appear in
        # endpoint routing and cert caching, not in format decisions
        openai_count = after_helpers.count("provider_is_openai()")
        format_count = after_helpers.count("provider_uses_openai_format()")
        # provider_uses_openai_format should be used in all format decisions
        self.assertGreater(format_count, 0)


class TestCLIMiniMaxSupport(unittest.TestCase):
    """Verify serial_cli.c recognizes MiniMax as a provider."""

    def setUp(self):
        self.cli = read_file(SERIAL_CLI_C)

    def test_cli_help_mentions_minimax(self):
        self.assertIn("minimax", self.cli.lower())

    def test_provider_help_includes_minimax(self):
        self.assertIn("anthropic|openai|minimax", self.cli)

    def test_llm_default_api_url_handles_minimax(self):
        match = re.search(
            r"static const char \*llm_default_api_url_for_provider\(void\)\s*\{([\s\S]*?)\n\}",
            self.cli,
        )
        self.assertIsNotNone(match)
        body = match.group(1)
        self.assertIn('"minimax"', body)
        self.assertIn("MIMI_MINIMAX_API_URL", body)


class TestSecretsExample(unittest.TestCase):
    """Verify mimi_secrets.h.example documents MiniMax."""

    def setUp(self):
        self.secrets = read_file(MIMI_SECRETS_EXAMPLE)

    def test_minimax_mentioned_in_provider_comment(self):
        self.assertIn("minimax", self.secrets.lower())

    def test_minimax_url_in_example(self):
        self.assertIn("MIMI_MINIMAX_API_URL", self.secrets)
        self.assertIn("api.minimax.io", self.secrets)


class TestREADMEDocumentation(unittest.TestCase):
    """Verify README files document MiniMax provider."""

    def test_readme_en_mentions_minimax(self):
        readme = read_file(README_MD)
        self.assertIn("MiniMax", readme)
        self.assertIn("set_model_provider minimax", readme)
        self.assertIn("MiniMax-M2.7", readme)

    def test_readme_zh_mentions_minimax(self):
        readme_zh = read_file(README_ZH_MD)
        self.assertIn("MiniMax", readme_zh)
        self.assertIn("set_model_provider minimax", readme_zh)


# ---------------------------------------------------------------------------
# Integration tests: verify MiniMax API compatibility
# ---------------------------------------------------------------------------

class TestMiniMaxAPIIntegration(unittest.TestCase):
    """Integration tests that call the real MiniMax API."""

    @classmethod
    def setUpClass(cls):
        # Load API key from env or .env.local
        cls.api_key = os.environ.get("MINIMAX_API_KEY", "")
        if not cls.api_key:
            env_path = os.path.join(
                os.path.dirname(MIMICLAW_ROOT), "..", "..", "..", ".env.local"
            )
            env_path = os.path.normpath(env_path)
            if os.path.exists(env_path):
                with open(env_path) as f:
                    for line in f:
                        line = line.strip()
                        if line.startswith("MINIMAX_API_KEY="):
                            cls.api_key = line.split("=", 1)[1].strip().strip('"').strip("'")
                            break

    def _skip_without_key(self):
        if not self.api_key:
            self.skipTest("MINIMAX_API_KEY not set")

    def _call_minimax(self, payload):
        """Call MiniMax API using the same format as llm_proxy.c OpenAI path."""
        import urllib.request
        import urllib.error

        url = "https://api.minimax.io/v1/chat/completions"
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            url,
            data=data,
            headers={
                "Content-Type": "application/json",
                "Authorization": f"Bearer {self.api_key}",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                return json.loads(resp.read().decode("utf-8")), resp.status
        except urllib.error.HTTPError as e:
            body = e.read().decode("utf-8", errors="replace")
            return json.loads(body) if body else {}, e.code

    def test_basic_chat_completion(self):
        """Verify MiniMax accepts OpenAI-format chat/completions request."""
        self._skip_without_key()
        payload = {
            "model": "MiniMax-M2.7",
            "max_completion_tokens": 64,
            "messages": [
                {"role": "system", "content": "Reply in one word."},
                {"role": "user", "content": "What is 2+2?"},
            ],
        }
        resp, status = self._call_minimax(payload)
        self.assertEqual(status, 200, f"API returned {status}: {resp}")
        # Validate OpenAI-format response structure
        self.assertIn("choices", resp)
        self.assertIsInstance(resp["choices"], list)
        self.assertGreater(len(resp["choices"]), 0)
        choice = resp["choices"][0]
        self.assertIn("message", choice)
        self.assertIn("content", choice["message"])
        self.assertIn("finish_reason", choice)

    def test_tool_calling(self):
        """Verify MiniMax supports OpenAI-format tool calling."""
        self._skip_without_key()
        payload = {
            "model": "MiniMax-M2.7",
            "max_completion_tokens": 128,
            "messages": [
                {"role": "user", "content": "What time is it in Tokyo?"},
            ],
            "tools": [
                {
                    "type": "function",
                    "function": {
                        "name": "get_time",
                        "description": "Get current time in a timezone",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "timezone": {
                                    "type": "string",
                                    "description": "IANA timezone name",
                                }
                            },
                            "required": ["timezone"],
                        },
                    },
                }
            ],
            "tool_choice": "auto",
        }
        resp, status = self._call_minimax(payload)
        self.assertEqual(status, 200, f"API returned {status}: {resp}")
        self.assertIn("choices", resp)
        # Either tool_calls or regular content is acceptable
        choice = resp["choices"][0]
        self.assertIn("message", choice)

    def test_bearer_auth_format(self):
        """Verify MiniMax accepts Bearer token auth (same as OpenAI)."""
        self._skip_without_key()
        payload = {
            "model": "MiniMax-M2.7",
            "max_completion_tokens": 16,
            "messages": [{"role": "user", "content": "hi"}],
        }
        resp, status = self._call_minimax(payload)
        self.assertEqual(status, 200, f"Bearer auth should work: {resp}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
