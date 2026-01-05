#pragma once

/*
 * Project-local shim.
 *
 * Some modules may include "lfs_config.h" from upstream LittleFS.
 * On T5AI we use platform LittleFS and do not want per-module ABI overrides.
 */
