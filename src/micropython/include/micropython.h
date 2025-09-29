/*
 * MicroPython Component Public Interface
 */

#ifndef MICROPYTHON_H
#define MICROPYTHON_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize MicroPython component
 * @return 0 on success, negative on error
 */
int micropython_init(void);

/**
 * @brief Deinitialize MicroPython component
 */
void micropython_deinit(void);

/**
 * @brief Execute Python code string
 * @param code Python code to execute
 * @return 0 on success, negative on error
 */
int micropython_exec(const char *code);

/**
 * @brief Execute Python script from file
 * @param filename Path to Python script
 * @return 0 on success, negative on error
 */
int micropython_exec_file(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* MICROPYTHON_H */