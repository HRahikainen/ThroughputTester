/***********************************************************************************************/ /**
 * \file   app.h
 * \brief  Application header file
 **************************************************************************************************/

#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************
 * Type Definitions
 **************************************************************************************************/
// Test parameters to be given from the command line.
typedef struct {
    uint16_t connection_interval;
    uint8_t phy;
    uint16_t mtu_size;
    uint8_t client_conf_flag;
    uint8_t mode;
    uint32_t fixed_time;
    uint32_t fixed_amount;
} TestParameters_t;

// Discovering services/characteristics and subscribing raises procedure_complete events
// Actions are used to indicate which procedure was completed.
typedef enum {
    act_none = 0,
    act_discover_service,
    act_discover_characteristics,
    act_enable_notification,
    act_enable_indication,
    act_subscribe_result
} Action_t;

// App main states
typedef enum {
    State_SCANNING = 0, 
    State_SET_PARAMETERS,
    State_DISCOVER,
    State_TRANSMISSION
} State_t;
/***************************************************************************************************
 * Function Declarations
 **************************************************************************************************/
int app_handle_events(struct gecko_cmd_packet *evt, TestParameters_t *params);


#ifdef __cplusplus
};
#endif

#endif /* APP_H */