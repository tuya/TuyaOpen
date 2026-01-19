/**
 * @file spectrum_meter.c
 * @brief Microphone input to FFT spectrum meter on 32x32 LED pixel display
 *
 * This demo reads 16kHz mono audio input, performs FFT analysis, and displays
 * the spectrum on a 32x32 LED matrix. The display shows 8 frequency bands,
 * each 4 pixels wide, with height representing magnitude.
 */

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_memory.h"
#include "tkl_mutex.h"
#include "board_com_api.h"
#include "board_pixel_api.h"
#include "tdl_audio_manage.h"
#include "tuya_ringbuf.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#define LED_PIXELS_TOTAL_NUM 1027
#define COLOR_RESOLUTION     1000u
#define BRIGHTNESS           0.02f // 10% brightness
#define MATRIX_WIDTH         32
#define MATRIX_HEIGHT        32

// Audio configuration
#define SAMPLE_RATE        16000
#define CHANNELS           1
#define BYTES_PER_SAMPLE   2 // 16-bit PCM
#define FRAME_SIZE_MS      10
#define FRAME_SIZE_BYTES   (SAMPLE_RATE * CHANNELS * BYTES_PER_SAMPLE * FRAME_SIZE_MS / 1000) // 320 bytes
#define AUDIO_RINGBUF_SIZE (FRAME_SIZE_BYTES * 32) // Buffer for 32 frames (~320ms)

#ifndef AUDIO_CODEC_NAME
#define AUDIO_CODEC_NAME "audio" // Audio codec device name (can be overridden in config)
#endif

// FFT configuration
#define FFT_SIZE   128 // Number of samples for FFT (8ms at 16kHz) - reduced for performance
#define NUM_BANDS  8
#define BAND_WIDTH 4 // Pixels per band

// Frequency band definitions (Hz)
// Band 0: 0-500, Band 1: 500-1000, Band 2: 1000-2000, Band 3: 2000-3000
// Band 4: 3000-4000, Band 5: 4000-5000, Band 6: 5000-6000, Band 7: 6000-8000
static const float g_freq_band_start[NUM_BANDS] = {0.0f, 500.0f, 1000.0f, 2000.0f, 3000.0f, 4000.0f, 5000.0f, 6000.0f};
static const float g_freq_band_end[NUM_BANDS] = {500.0f, 1000.0f, 2000.0f, 3000.0f, 4000.0f, 5000.0f, 6000.0f, 8000.0f};

/***********************************************************
***********************variable define**********************
***********************************************************/

static PIXEL_HANDLE_T g_pixels_handle = NULL;

static TDL_AUDIO_HANDLE_T g_audio_handle = NULL;
static TUYA_RINGBUFF_T g_audio_ringbuf = NULL;
static MUTEX_HANDLE g_audio_rb_mutex = NULL;

static int16_t g_audio_buffer[FFT_SIZE];
static float g_fft_real[FFT_SIZE];
static float g_fft_imag[FFT_SIZE];
static float g_band_magnitude[NUM_BANDS];
static float g_band_peak[NUM_BANDS]; // Peak hold for visual effect

// Audio statistics for logging
static uint32_t g_audio_frames_received = 0;
static uint32_t g_audio_bytes_received = 0;
static uint32_t g_audio_frames_processed = 0;
static uint32_t g_last_log_time = 0;

/***********************************************************
********************function declaration********************
***********************************************************/

static void spectrum_display(void);

static void audio_frame_callback(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len);
static void process_audio_fft(uint8_t *audio_data, uint32_t data_len);
static void compute_fft(void);
static void calculate_band_magnitudes(void);
static float hann_window(int n, int N);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Display spectrum on LED matrix
 */
static void spectrum_display(void)
{
    if (g_pixels_handle == NULL) {
        return;
    }

    // Clear all pixels
    PIXEL_COLOR_T off_color = {0};
    tdl_pixel_set_single_color(g_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, &off_color);

    // Update peak decay (every frame, decay peaks by 0.8 pixels for very fast drop)
    for (int i = 0; i < NUM_BANDS; i++) {
        if (g_band_peak[i] > 0.0f) {
            g_band_peak[i] -= 0.8f; // Very fast decay - drops 0.8 pixels per frame
            if (g_band_peak[i] < 0.0f) {
                g_band_peak[i] = 0.0f;
            }
        }
    }

    // Display each frequency band
    for (int band = 0; band < NUM_BANDS; band++) {
        // Calculate bar height (0-32 pixels)
        float magnitude = g_band_magnitude[band];
        int bar_height = (int)(magnitude * MATRIX_HEIGHT);
        if (bar_height > MATRIX_HEIGHT) {
            bar_height = MATRIX_HEIGHT;
        }
        if (bar_height < 0) {
            bar_height = 0;
        }

        // Update peak hold
        if (bar_height > g_band_peak[band]) {
            g_band_peak[band] = (float)bar_height;
        }

        // Calculate column range for this band (4 pixels wide)
        int col_start = band * BAND_WIDTH;
        int col_end = col_start + BAND_WIDTH;
        // Ensure we don't exceed matrix width
        if (col_end > MATRIX_WIDTH) {
            col_end = MATRIX_WIDTH;
        }

        // Color based on frequency band (hue from red to blue)
        // Pure colors: red, orange, yellow, green, cyan, blue, purple, magenta
        float base_hue = (float)band / (float)NUM_BANDS * 360.0f; // 0-360 degrees
        float saturation = 1.0f;                                  // Full saturation for vibrant colors
        float value = 1.0f;                                       // Full brightness

        // Draw the bar from bottom (y=31) upward with gradient
        for (int col = col_start; col < col_end && col < MATRIX_WIDTH; col++) {
            // Draw magnitude bar with hue shift gradient
            for (int row = MATRIX_HEIGHT - 1; row >= MATRIX_HEIGHT - bar_height && row >= 0; row--) {
                // Calculate gradient: 0.0 at bottom, 1.0 at top
                int row_in_bar = MATRIX_HEIGHT - 1 - row;
                float gradient = 0.0f;
                if (bar_height > 1) {
                    gradient = (float)row_in_bar / (float)(bar_height - 1);
                }
                // Hue shift: shift by -10 degrees at bottom to +10 degrees at top
                // This creates a subtle color transition while staying close to the base color
                float hue_shift = -10.0f + gradient * 20.0f;
                float shifted_hue = base_hue + hue_shift;

                // Convert HSV to RGB with hue shift but constant saturation and value
                uint32_t r, g, b;
                board_pixel_hsv_to_rgb(shifted_hue, saturation, value, &r, &g, &b);

                // Apply brightness scaling
                PIXEL_COLOR_T gradient_color = {.red = (uint32_t)(r * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                                .green = (uint32_t)(g * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                                .blue = (uint32_t)(b * COLOR_RESOLUTION * BRIGHTNESS / 255),
                                                .warm = 0,
                                                .cold = 0};

                uint32_t led_index = board_pixel_matrix_coord_to_led_index((uint32_t)col, (uint32_t)row);
                if (led_index < LED_PIXELS_TOTAL_NUM) {
                    tdl_pixel_set_single_color(g_pixels_handle, led_index, 1, &gradient_color);
                }
            }

            // Draw peak indicator (white dot at peak position)
            int peak_pos = MATRIX_HEIGHT - 1 - (int)g_band_peak[band];
            if (peak_pos >= 0 && peak_pos < MATRIX_HEIGHT) {
                PIXEL_COLOR_T peak_color = {.red = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS),
                                            .green = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS),
                                            .blue = (uint32_t)(COLOR_RESOLUTION * BRIGHTNESS),
                                            .warm = 0,
                                            .cold = 0};
                uint32_t peak_led_index = board_pixel_matrix_coord_to_led_index((uint32_t)col, (uint32_t)peak_pos);
                if (peak_led_index < LED_PIXELS_TOTAL_NUM) {
                    tdl_pixel_set_single_color(g_pixels_handle, peak_led_index, 1, &peak_color);
                }
            }
        }
    }

    tdl_pixel_dev_refresh(g_pixels_handle);
}

/**
 * @brief Hann window function
 */
static float hann_window(int n, int N)
{
    return 0.5f * (1.0f - cosf(2.0f * M_PI * (float)n / (float)(N - 1)));
}

/**
 * @brief Compute FFT using optimized DFT - only compute bins we need
 */
static void compute_fft(void)
{
    // Pre-compute window function values
    static float window[FFT_SIZE];
    static bool window_computed = false;
    if (!window_computed) {
        for (int n = 0; n < FFT_SIZE; n++) {
            window[n] = hann_window(n, FFT_SIZE);
        }
        window_computed = true;
    }

    // Apply window and compute only needed frequency bins
    // We only need bins up to ~8kHz (bin 64 for 16kHz sample rate with 128-point FFT)
    int max_bin = FFT_SIZE / 2; // Nyquist frequency

    for (int k = 0; k < max_bin; k++) {
        float real_sum = 0.0f;
        float imag_sum = 0.0f;
        float k_angle_scale = -2.0f * M_PI * (float)k / (float)FFT_SIZE;

        for (int n = 0; n < FFT_SIZE; n++) {
            float windowed = (float)g_audio_buffer[n] * window[n];
            float angle = k_angle_scale * (float)n;
            real_sum += windowed * cosf(angle);
            imag_sum += windowed * sinf(angle);
        }

        g_fft_real[k] = real_sum;
        g_fft_imag[k] = imag_sum;
    }

    // Clear unused bins
    for (int k = max_bin; k < FFT_SIZE; k++) {
        g_fft_real[k] = 0.0f;
        g_fft_imag[k] = 0.0f;
    }
}

/**
 * @brief Calculate magnitude for each frequency band
 */
static void calculate_band_magnitudes(void)
{
    float freq_resolution = (float)SAMPLE_RATE / (float)FFT_SIZE; // ~62.5 Hz per bin

    for (int band = 0; band < NUM_BANDS; band++) {
        float band_start_hz = g_freq_band_start[band];
        float band_end_hz = g_freq_band_end[band];

        int bin_start = (int)(band_start_hz / freq_resolution);
        int bin_end = (int)(band_end_hz / freq_resolution);

        if (bin_start >= FFT_SIZE)
            bin_start = FFT_SIZE - 1;
        if (bin_end >= FFT_SIZE)
            bin_end = FFT_SIZE - 1;
        if (bin_start < 0)
            bin_start = 0;
        if (bin_end < bin_start)
            bin_end = bin_start;

        // Calculate average magnitude in this band
        float magnitude_sum = 0.0f;
        int bin_count = 0;

        for (int bin = bin_start; bin <= bin_end; bin++) {
            float magnitude = sqrtf(g_fft_real[bin] * g_fft_real[bin] + g_fft_imag[bin] * g_fft_imag[bin]);
            magnitude_sum += magnitude;
            bin_count++;
        }

        float avg_magnitude = (bin_count > 0) ? (magnitude_sum / (float)bin_count) : 0.0f;

        // Normalize and apply logarithmic scaling for better visualization
        // Scale to 0-1 range with some compression
        float normalized = avg_magnitude / 10000.0f; // Adjust this divisor based on your audio levels
        if (normalized > 1.0f)
            normalized = 1.0f;
        if (normalized < 0.0f)
            normalized = 0.0f;

        // Apply logarithmic scaling for better visual response
        normalized = log10f(1.0f + normalized * 9.0f); // Maps 0-1 to 0-1 with log curve

        g_band_magnitude[band] = normalized;
    }
}

/**
 * @brief Process audio data and perform FFT
 */
static void process_audio_fft(uint8_t *audio_data, uint32_t data_len)
{
    // Convert bytes to samples (16-bit PCM, mono)
    uint32_t num_samples = data_len / BYTES_PER_SAMPLE;
    if (num_samples > FFT_SIZE) {
        num_samples = FFT_SIZE;
    }

    // Convert PCM bytes to int16 samples
    int16_t *pcm_samples = (int16_t *)audio_data;

    // Shift buffer and add new samples
    // Keep last FFT_SIZE samples in buffer
    if (num_samples >= FFT_SIZE) {
        // Replace entire buffer
        memcpy(g_audio_buffer, pcm_samples, FFT_SIZE * sizeof(int16_t));
    } else {
        // Shift existing samples left
        memmove(g_audio_buffer, &g_audio_buffer[num_samples], (FFT_SIZE - num_samples) * sizeof(int16_t));
        // Add new samples at the end
        memcpy(&g_audio_buffer[FFT_SIZE - num_samples], pcm_samples, num_samples * sizeof(int16_t));
    }

    // Perform FFT
    compute_fft();

    // Calculate band magnitudes
    calculate_band_magnitudes();

    // Update display
    spectrum_display();
}

/**
 * @brief Audio frame callback from tdl_audio - NON-BLOCKING
 * Only collects data into ring buffer, processing happens in separate task
 */
static void audio_frame_callback(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len)
{
    // Only process PCM audio frames
    if (type != TDL_AUDIO_FRAME_FORMAT_PCM) {
        PR_DEBUG("Audio frame: type=%d (not PCM), len=%d", type, len);
        return;
    }

    // Log first few frames and periodically
    g_audio_frames_received++;
    g_audio_bytes_received += len;

    if (g_audio_frames_received <= 5 || (g_audio_frames_received % 100 == 0)) {
        // Calculate sample statistics for first few samples
        int16_t sample_val = 0;
        int16_t max_val = 0;
        int16_t min_val = 0;
        if (len >= 2) {
            sample_val = (int16_t)(data[0] | (data[1] << 8));
            max_val = min_val = sample_val;
            for (uint32_t i = 0; i < len && i < 20; i += 2) {
                int16_t val = (int16_t)(data[i] | (data[i + 1] << 8));
                if (val > max_val)
                    max_val = val;
                if (val < min_val)
                    min_val = val;
            }
        }

        PR_NOTICE("Audio frame[%d]: status=%d, len=%d, sample_range=[%d, %d], first_sample=%d", g_audio_frames_received,
                  status, len, min_val, max_val, sample_val);
    }

    // NON-BLOCKING: Just write to ring buffer, no processing here
    // Processing happens in separate task to avoid blocking audio callback
    if (g_audio_ringbuf != NULL && g_audio_rb_mutex != NULL) {
        tal_mutex_lock(g_audio_rb_mutex);
        uint32_t before_size = tuya_ring_buff_used_size_get(g_audio_ringbuf);
        // Discard old data if buffer is getting full (keep it small for real-time)
        if (before_size > AUDIO_RINGBUF_SIZE / 2) {
            tuya_ring_buff_discard(g_audio_ringbuf, before_size - AUDIO_RINGBUF_SIZE / 4);
        }
        tuya_ring_buff_write(g_audio_ringbuf, data, len);
        tal_mutex_unlock(g_audio_rb_mutex);
    }
}

/**
 * @brief Audio processing task - Does single FFT and display update
 * Reads from ring buffer and processes in separate task (non-blocking callback)
 */
static void audio_processing_task(void *args)
{
    uint8_t *audio_buffer = NULL;

    audio_buffer = (uint8_t *)tal_malloc(FRAME_SIZE_BYTES);
    if (audio_buffer == NULL) {
        PR_ERR("Failed to allocate audio buffer");
        return;
    }

    PR_NOTICE("Audio processing task started");

    while (1) {
        uint32_t available = 0;

        // Check available data in ring buffer
        if (g_audio_ringbuf != NULL && g_audio_rb_mutex != NULL) {
            tal_mutex_lock(g_audio_rb_mutex);
            available = tuya_ring_buff_used_size_get(g_audio_ringbuf);
            tal_mutex_unlock(g_audio_rb_mutex);
        }

        if (available >= FRAME_SIZE_BYTES) {
            // Read one frame from ring buffer
            uint32_t read_len = 0;
            if (g_audio_ringbuf != NULL && g_audio_rb_mutex != NULL) {
                tal_mutex_lock(g_audio_rb_mutex);
                read_len = tuya_ring_buff_read(g_audio_ringbuf, audio_buffer, FRAME_SIZE_BYTES);
                tal_mutex_unlock(g_audio_rb_mutex);
            }

            if (read_len == FRAME_SIZE_BYTES) {
                g_audio_frames_processed++;

                // Log processing statistics periodically
                if (g_audio_frames_processed <= 5 || (g_audio_frames_processed % 50 == 0)) {
                    // Calculate audio level statistics
                    int16_t max_sample = 0;
                    int16_t min_sample = 0;
                    int32_t sum_samples = 0;
                    for (uint32_t i = 0; i < read_len; i += 2) {
                        int16_t sample = (int16_t)(audio_buffer[i] | (audio_buffer[i + 1] << 8));
                        if (i == 0) {
                            max_sample = min_sample = sample;
                        } else {
                            if (sample > max_sample)
                                max_sample = sample;
                            if (sample < min_sample)
                                min_sample = sample;
                        }
                        sum_samples += abs(sample);
                    }
                    int16_t avg_level = (int16_t)(sum_samples / (read_len / 2));

                    PR_DEBUG("Processing frame[%d]: available=%d, read=%d, level=[%d, %d, avg=%d]",
                             g_audio_frames_processed, available, read_len, min_sample, max_sample, avg_level);
                }

                // Single FFT and display update
                process_audio_fft(audio_buffer, read_len);
            } else {
                PR_WARN("Failed to read full frame: expected=%d, got=%d", FRAME_SIZE_BYTES, read_len);
            }
        } else {
            // Not enough data yet, wait a bit
            tal_system_sleep(5);
        }

        // Periodic statistics log (every ~5 seconds at 10ms frames = 500 frames)
        if (g_audio_frames_processed > 0 && (g_audio_frames_processed % 500 == 0)) {
            uint32_t current_time = tal_time_get_posix();
            if (g_last_log_time > 0) {
                uint32_t elapsed = current_time - g_last_log_time;
                if (elapsed > 0) {
                    float fps = 500.0f / elapsed;
                    PR_NOTICE("Audio stats: frames_received=%d, frames_processed=%d, bytes_received=%d, fps=%.2f",
                              g_audio_frames_received, g_audio_frames_processed, g_audio_bytes_received, fps);
                }
            }
            g_last_log_time = current_time;
        }
    }

    if (audio_buffer) {
        tal_free(audio_buffer);
    }
}

/**
 * @brief Main user function
 */
static void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("==========================================");
    PR_NOTICE("Tuya T5AI Pixel Spectrum Meter");
    PR_NOTICE("==========================================");
    PR_NOTICE("16kHz mono audio input to FFT spectrum");
    PR_NOTICE("32x32 LED display with 8 frequency bands");
    PR_NOTICE("==========================================");

    // Initialize hardware
    rt = board_register_hardware();
    if (OPRT_OK != rt) {
        PR_ERR("board_register_hardware failed: %d", rt);
        return;
    }
    PR_NOTICE("Hardware initialized");

    tal_system_sleep(100);
    rt = board_pixel_get_handle(&g_pixels_handle);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to get pixel device handle: %d", rt);
        return;
    }
    PR_NOTICE("Pixel LED initialized: %d pixels", LED_PIXELS_TOTAL_NUM);

    // Initialize audio ring buffer
    rt = tuya_ring_buff_create(AUDIO_RINGBUF_SIZE, OVERFLOW_PSRAM_STOP_TYPE, &g_audio_ringbuf);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create audio ring buffer: %d", rt);
        return;
    }
    PR_NOTICE("Audio ring buffer created");

    // Create mutex for ring buffer
    rt = tal_mutex_create_init(&g_audio_rb_mutex);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create audio ring buffer mutex: %d", rt);
        return;
    }
    PR_NOTICE("Audio ring buffer mutex created");

    // Wait a bit to ensure audio driver registration is complete
    tal_system_sleep(200);

    // Find audio device
    rt = tdl_audio_find(AUDIO_CODEC_NAME, &g_audio_handle);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to find audio device '%s': %d", AUDIO_CODEC_NAME, rt);
        return;
    }
    PR_NOTICE("Audio device found");

    // Open audio device with callback
    rt = tdl_audio_open(g_audio_handle, audio_frame_callback);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to open audio device: %d", rt);
        return;
    }
    PR_NOTICE("Audio device opened and started");

    // Initialize band magnitude arrays
    memset(g_band_magnitude, 0, sizeof(g_band_magnitude));
    memset(g_band_peak, 0, sizeof(g_band_peak));
    memset(g_audio_buffer, 0, sizeof(g_audio_buffer));

    // Start audio processing task
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_2;
    thrd_param.thrdname = "audio_proc";

    THREAD_HANDLE audio_thrd = NULL;
    rt = tal_thread_create_and_start(&audio_thrd, NULL, NULL, audio_processing_task, NULL, &thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to start audio processing thread: %d", rt);
        return;
    }
    PR_NOTICE("Audio processing thread started");

    PR_NOTICE("==========================================");
    PR_NOTICE("Spectrum Meter Ready!");
    PR_NOTICE("==========================================");

    // Main loop
    int cnt = 0;
    while (1) {
        // Print status every 10 seconds
        if (cnt % 100 == 0) {
            PR_DEBUG("Spectrum meter running... (count: %d)", cnt);
        }
        tal_system_sleep(100);
        cnt++;
    }
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
