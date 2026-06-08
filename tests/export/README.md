# Export script tests

Tests for `export.sh`, `export.ps1`, and the IDE progress protocol.

## Run all

```bash
bash tests/export/run_all.sh
```

On Windows (Git Bash / WSL), the same command works. PowerShell progress tests run when `pwsh` or `powershell` is available.

## Layout

| File | Purpose |
|------|---------|
| `test_export.sh` | Integration and unit tests (migrated from `tools/test_export.sh`) |
| `test_progress.sh` | POSIX progress-line parser fixtures |
| `test_progress.ps1` | Windows progress-line parser fixtures |
| `fixtures/` | Sample uv/python/sync output lines |
