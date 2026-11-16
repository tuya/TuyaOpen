/**
 * @file config_wl83_user.h
 * @brief mbedtls user config for the JIELI wl83 bundled build.
 *
 * The wl83 newlib stubs implement neither a usable dirent API nor the
 * Unix/Windows timing module, so parts of the default mbedtls 3.1.0 config
 * cannot compile as-is. Certificates are linked into the image and this port
 * does not use DTLS, so both are switched off here:
 *
 *  - MBEDTLS_TIMING_C gates the whole of timing.c, so undefining it empties
 *    that translation unit instead of tripping its platform #error.
 *  - MBEDTLS_NO_DIRENT is a local opt-out (x509_crt.c) that keeps
 *    MBEDTLS_FS_IO enabled - PSA's file-backed ITS needs the flag - while
 *    excluding the directory-walking x509 parsers. Nothing in TuyaOpen
 *    calls mbedtls_x509_crt_parse_path/_file; certificates are embedded.
 *
 * This header deliberately has no include guard.  build_info.h pulls it in
 * twice: once from the Tuya port config (tuya_tls_config.h) and again after
 * the default mbedtls_config.h, which unconditionally re-enables these
 * modules.  Dropping the guard makes the second inclusion take effect.
 */
#undef MBEDTLS_TIMING_C
#define MBEDTLS_NO_DIRENT
