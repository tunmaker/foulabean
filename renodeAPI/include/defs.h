#pragma once
#include <stdint.h>
#include <string>

#define SERVER_START_COMMAND "emulation CreateExternalControlServer \"NAME\" PORT"

// --------------------------------------------------------------------
//  Hand‑shake constants (adjust if your server uses different IDs)
// --------------------------------------------------------------------
static const uint8_t HANDSHAKE_CMD_ID = 0x00;          // command ID for version exchange
static const uint8_t SUCCESS_HANDSHAKE = 0xAA ;        // expected reply byte

typedef enum {
    ANY_COMMAND = 0,
    RUN_FOR = 1,
    GET_TIME,
    GET_MACHINE,
    ADC,
    GPIO,
    SYSTEM_BUS,
    EVENT = -1,
} api_commands;


/* Simple error enum – mirrors the original renode_error_t API */
enum class RenodeError {
    Ok = 0,
    ConnectionFailed,
    Fatal
};

enum return_code_t {
  COMMAND_FAILED,       // code, command, data
  FATAL_ERROR,          // code, data
  INVALID_COMMAND,      // code, command
  SUCCESS_WITH_DATA,    // code, command, data
  SUCCESS_WITHOUT_DATA, // code, command
  OK_HANDSHAKE,    // code
  ASYNC_EVENT,          // code, command, callback id, data
} ;

enum renode_error_code {
    ERR_CONNECTION_FAILED,
    ERR_FATAL,
    ERR_NOT_CONNECTED,
    ERR_PERIPHERAL_INIT_FAILED,
    ERR_TIMEOUT,
    ERR_COMMAND_FAILED,
    ERR_NO_ERROR = -1,
} ;

struct renode_error{
    renode_error_code code;
    int flags;
    std::string message;
    void *data;
} ;

enum renode_time_unit{
    TU_MICROSECONDS =       1,
    TU_MILLISECONDS =    1000,
    TU_SECONDS      = 1000000,
} ;

struct renode_gpio_event_data {
    uint64_t timestamp_us;
    bool state;
} ;


enum renode_access_width {
    AW_MULTI_BYTE  = 0,
    AW_BYTE        = 1,
    AW_WORD        = 2,
    AW_DOUBLE_WORD = 4,
    AW_QUAD_WORD   = 8,
};