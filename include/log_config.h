#ifndef LOG_CONFIG_H
#define LOG_CONFIG_H

/*
 * LOG_LEVEL — compile-time maximum.
 * Calls at or below this level are compiled in; everything above is stripped.
 *
 *   0  OFF     nothing compiled in
 *   1  ERROR
 *   2  WARN
 *   3  INFO    (default)
 *   4  DEBUG
 *
 * Override per-target in your build system:
 *   -DLOG_LEVEL=4          (CMake: target_compile_definitions)
 */
#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

/* TX ring buffer size for log output (bytes). */
#ifndef LOG_TX_BUF_SIZE
#define LOG_TX_BUF_SIZE 256
#endif

#endif /* LOG_CONFIG_H */
