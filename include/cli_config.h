#ifndef CLI_CONFIG_H
#define CLI_CONFIG_H

/* Maximum length of a single input line (including null terminator) */
#ifndef CLI_MAX_LINE_LEN
#define CLI_MAX_LINE_LEN 80
#endif

/* Maximum number of arguments (including the command name) */
#ifndef CLI_MAX_ARGS
#define CLI_MAX_ARGS 8
#endif

/* RX ring buffer size — bytes received from UART before cli_process() drains them */
#ifndef CLI_RX_BUF_SIZE
#define CLI_RX_BUF_SIZE 64
#endif

/* TX ring buffer size — bytes queued by CLI output before UART drains them */
#ifndef CLI_TX_BUF_SIZE
#define CLI_TX_BUF_SIZE 128
#endif

/* Set to 1 to echo received characters back to the terminal */
#ifndef CLI_ECHO
#define CLI_ECHO 0
#endif

#endif /* CLI_CONFIG_H */
