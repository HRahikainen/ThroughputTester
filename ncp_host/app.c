/***********************************************************************************************/ /**
 * \file   app.c
 * \brief  Event handling and application code for NCP Host application
 **************************************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/***************************************************************************************************
 * Platform specific timing functions for calculating transmission time.
 **************************************************************************************************/
#if ((_WIN32 == 1) || (__CYGWIN__ == 1))
#include <windows.h>
LARGE_INTEGER StartingTime;
LARGE_INTEGER EndingTime;
LARGE_INTEGER ElapsedMicroseconds;
LARGE_INTEGER Frequency;

static void timer_start()
{
    QueryPerformanceFrequency(&Frequency); 
    QueryPerformanceCounter(&StartingTime);
}

static double timer_end()
{
    QueryPerformanceCounter(&EndingTime);
    ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
    ElapsedMicroseconds.QuadPart *= 1000000;
    
    volatile double time = (double)(ElapsedMicroseconds.QuadPart / Frequency.QuadPart) / 1e6;
    return time;
}
#else
#include <unistd.h>
#include <time.h>
volatile struct timespec startingTime;
volatile struct timespec endingTime;

static void timer_start()
{
    clock_gettime(CLOCK_REALTIME, &startingTime);
}

static double timer_end()
{
    clock_gettime(CLOCK_REALTIME, &endingTime);
    volatile double time = (double)((endingTime.tv_sec - startingTime.tv_sec) + (endingTime.tv_nsec - startingTime.tv_nsec) * 1e-9);
    return time;
}
#endif

/* BG stack headers */
#include "bg_types.h"
#include "gecko_bglib.h"

/* Own header */
#include "app.h"

// --------------------------------
// Local variables and constants
const uint16_t SCAN_INTERVAL = 16;                      // 16 * 0.625 = 10ms
const uint16_t SCAN_WINDOW = 16;                        // 16 * 0.625 = 10ms
const uint16_t HW_TICKS_PER_SECOND = 32768;             // Hardware clock ticks that equal one second
const uint8_t SOFT_TIMER_FIXED_TRANSFER_TIME_HANDLE = 0;
const uint8_t TX_POWER = 100;                           // 10 dBm is the max allowed without Adaptive Frequency Hopping. 

const char *DEVICE_NAME = "Throughput Tester"; // Device name to match against scan results.
// bbb99e70-fff7-46cf-abc7-2d32c71820f2
const uint8_t SERVICE_UUID[] = {0xf2, 0x20, 0x18, 0xc7, 0x32, 0x2d, 0xc7, 0xab, 0xcf, 0x46, 0xf7, 0xff, 0x70, 0x9e, 0xb9, 0xbb};
// 6109b631-a643-4a51-83d2-2059700ad49f
const uint8_t INDICATIONS_CHARACTERISTIC_UUID[] = {0x9f, 0xd4, 0x0a, 0x70, 0x59, 0x20, 0xd2, 0x83, 0x51, 0x4a, 0x43, 0xa6, 0x31, 0xb6, 0x09, 0x61};
// 47b73dd6-dee3-4da1-9be0-f5c539a9a4be
const uint8_t NOTIFICATIONS_CHARACTERISTIC_UUID[] = {0xbe, 0xa4, 0xa9, 0x39, 0xc5, 0xf5, 0xe0, 0x9b, 0xa1, 0x4d, 0xe3, 0xde, 0xd6, 0x3d, 0xb7, 0x47};
// be6b6be1-cd8a-4106-9181-5ffe2bc67718
const uint8_t TRANSMISSION_CHARACTERISTIC_UUID[] = {0x18, 0x77, 0xc6, 0x2b, 0xfe, 0x5f, 0x81, 0x91, 0x06, 0x41, 0x8a, 0xcd, 0xe1, 0x6b, 0x6b, 0xbe};
//adf32227-b00f-400c-9eeb-b903a6cc291b
const uint8_t RESULT_CHARACTERISTIC_UUID[] = {0x1b, 0x29, 0xcc, 0xa6, 0x03, 0xb9, 0xeb, 0x9e, 0x0c, 0x40, 0x0f, 0xb0, 0x27, 0x22, 0xf3, 0xad};

static bool appBooted = false;
static uint8_t askForInput = 0;

static Action_t action = act_none;
static State_t state = State_SCANNING;

static uint8_t connection = 0xFF;
static uint32_t serviceHandle = 0xFFFFFFFF;
static uint16_t notificationsHandle = 0xFFFF;
static uint16_t indicationsHandle = 0xFFFF;
static uint16_t transmissionHandle = 0xFFFF;
static uint16_t resultHandle = 0xFFFF;
static uint8_t numCharacteristicsDiscovered = 0;
static uint8_t initPhy = 1;
static uint8_t phyInUse = 1;

static uint16_t interval = 0;
static uint16_t mtuSize = 0;
static uint16_t pduSize = 0;
static uint16_t supervisionTimeout = 0;
static uint16_t slaveLatency = 0;

static bool isFirstPacket = true;
static uint64_t bitsSent = 0;
static uint64_t throughput = 0;
static uint32_t operationCount = 0;
static uint32_t result = 0;

const uint8_t TRANSMISSION_ON = 1;
const uint8_t TRANSMISSION_OFF = 0;

/***************************************************************************************************
 * Static Function Declarations
 **************************************************************************************************/
// Helper functions
static void set_action(Action_t act) { action = act; }
static void waiting_indication(void);
static void reset_variables(void);
// Data transmission functions
static void start_data_transmission(TestParameters_t *params);
static void end_data_transmission(TestParameters_t *params);
// Scan and discovery result processing
static void process_procedure_complete_event(struct gecko_cmd_packet *evt, TestParameters_t *params);
static bool process_scan_response(struct gecko_msg_le_gap_scan_response_evt_t *pResp);
static void check_characteristic_uuid(struct gecko_cmd_packet *evt);

/***************************************************************************************************
 * Public Function Definitions
 **************************************************************************************************/

/***********************************************************************************************/ /**
 *  \brief  Event handler function.
 *  \param[in] evt Event pointer.
 **************************************************************************************************/
int app_handle_events(struct gecko_cmd_packet *evt, TestParameters_t *params)
{
    askForInput = 0;
    if (NULL == evt) {
        return 0;
    }

    // Do not handle any events until system is booted up properly.
    if ((BGLIB_MSG_ID(evt->header) != gecko_evt_system_boot_id) && !appBooted) {
#if defined(DEBUG)
        printf("Event: 0x%04x\n", BGLIB_MSG_ID(evt->header));
#endif

#if ((_WIN32 == 1) || (__CYGWIN__ == 1))
        Sleep(50);
#else
        usleep(50000);  
#endif
        return 0;
    }

    // Switch main state, check only events relevant to those states.
    switch (state) {
        case State_SCANNING:
            switch(BGLIB_MSG_ID(evt->header) ) {
                case gecko_evt_system_boot_id:
                    appBooted = true;
                    reset_variables();
                    gecko_cmd_gatt_set_max_mtu(params->mtu_size);
                    gecko_cmd_system_set_tx_power(TX_POWER);
                    // 2M isn't allowed as initiating PHY by stack.
                    if (params->phy == 2) {
                        initPhy = 1;
                    } else {
                        initPhy = params->phy;
                    }
                    printf("\nSystem booted. Starting scanning... \n\n");
                    printf("Mode: %s\n\n", (params->mode == 3) ? "Free mode" : ((params->mode == 2) ? "Fixed data" : "Fixed time"));
                    gecko_cmd_le_gap_set_discovery_type(5, 0);
                    gecko_cmd_le_gap_set_discovery_timing(5, SCAN_INTERVAL, SCAN_WINDOW);
                    gecko_cmd_le_gap_start_discovery(initPhy, le_gap_discover_observation);
                    break;

                case gecko_evt_le_gap_scan_response_id:
                    if (process_scan_response(&(evt->data.evt_le_gap_scan_response))) {
                        gecko_cmd_le_gap_end_procedure(); // Stop scanning in the background.
                        gecko_cmd_le_gap_connect(evt->data.evt_le_gap_scan_response.address, evt->data.evt_le_gap_scan_response.address_type, initPhy);
                    } else {
                        waiting_indication();
                    }

                    break;

                case gecko_evt_le_connection_opened_id:
                    connection = evt->data.evt_le_connection_opened.connection;
                    printf("Connection opened!\n\n");
                    // Change PHY from initial if needed (2M).
                    if (initPhy != params->phy) {
                        while (gecko_cmd_le_connection_set_phy(evt->data.evt_le_connection_opened.connection, params->phy)->result != 0);
                    }
                    // Set connection parameters to those that were given as input.
                    gecko_cmd_le_connection_set_timing_parameters(evt->data.evt_le_connection_opened.connection, params->connection_interval, params->connection_interval, 0, 100, 0, 0xFFFF);
                    state = State_SET_PARAMETERS;
                    break;
                default:
                    break;
            }
            break;

        case State_SET_PARAMETERS:
            // Wait for parameters to update.
            switch(BGLIB_MSG_ID(evt->header) ) {
                    
                case gecko_evt_le_connection_parameters_id:
                    interval = evt->data.evt_le_connection_parameters.interval;
                    pduSize = evt->data.evt_le_connection_parameters.txsize;
                    slaveLatency = evt->data.evt_le_connection_parameters.latency;
                    supervisionTimeout = evt->data.evt_le_connection_parameters.timeout;

                    if ((interval == params->connection_interval) && (mtuSize == params->mtu_size)) {
                        if (phyInUse == params->phy) {
                            state = State_DISCOVER;
                            gecko_cmd_gatt_discover_primary_services_by_uuid(connection, 16, SERVICE_UUID);
                        }
                    }
                    break;

                default: break;
            }
            break;

        case State_DISCOVER:
            switch(BGLIB_MSG_ID(evt->header) ) {
                case gecko_evt_gatt_procedure_completed_id:
                    process_procedure_complete_event(evt, params);
                    break;

                case gecko_evt_gatt_characteristic_id:
                    check_characteristic_uuid(evt);
                    break;

                case gecko_evt_gatt_service_id:

                    if (evt->data.evt_gatt_service.uuid.len == 16) {
                        if (memcmp(SERVICE_UUID, evt->data.evt_gatt_service.uuid.data, 16) == 0) {
                            serviceHandle = evt->data.evt_gatt_service.service;
                            set_action(act_discover_service);
                            printf("-------------------------------\n");
                            printf("Service found!\n\n");
                        }
                    }
                    break;

                default:
                    break;
            }
            break;

        case State_TRANSMISSION:
            switch(BGLIB_MSG_ID(evt->header) ) {
                case gecko_evt_gatt_characteristic_value_id:
                    if (evt->data.evt_gatt_characteristic_value.characteristic == resultHandle) {
                        if (evt->data.evt_gatt_characteristic_value.att_opcode == gatt_handle_value_indication) {
                            gecko_cmd_gatt_send_characteristic_confirmation(evt->data.evt_gatt_characteristic_value.connection);
                            // Slave sends indication about result after each test. Data is uint8array LSB first.
                            memcpy(&result, evt->data.evt_gatt_characteristic_value.value.data, 4);  
                        }

                        if (params->mode == 3) {
                            end_data_transmission(params);
                            askForInput = 0;
                        }

                        printf("Throughput result reported by slave: %lu bps\n\n", result);

                        if ((params->mode == 1) || (params->mode == 2)) {   
                            // If in one-shot modes, ask if user wants to re-run test.
                            state = State_SCANNING;
                            askForInput = 1;
                        }
                        break;
                    }
                    // Data received
                    if (evt->data.evt_gatt_characteristic_value.characteristic == indicationsHandle) {
                        if (evt->data.evt_gatt_characteristic_value.att_opcode == gatt_handle_value_indication) {
                            gecko_cmd_gatt_send_characteristic_confirmation(evt->data.evt_gatt_characteristic_value.connection);
                        }
                    }
                    bitsSent += (evt->data.evt_gatt_characteristic_value.value.len * 8);
                    operationCount++;

                    // Fixed data mode
                    if (params->mode == 2) { 
                        if (bitsSent >= (params->fixed_amount * 8)) {
                            end_data_transmission(params);
                        }
                    }

                    // Button has been pressed on slave, first packet of transmission.
                    if (isFirstPacket && (params->mode == 3)) { 
                        start_data_transmission(params);
                    }
                    isFirstPacket = false;
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }

    // Handle universal events regardless of state.
    switch (BGLIB_MSG_ID(evt->header)) {
        
        case gecko_evt_gatt_mtu_exchanged_id:
            mtuSize = evt->data.evt_gatt_mtu_exchanged.mtu;
            printf("MTU exchanged: %u\n\n", mtuSize);
            break;

        case gecko_evt_le_connection_phy_status_id:
            phyInUse = evt->data.evt_le_connection_phy_status.phy;
            printf("PHY status: %u\n\n", phyInUse);
            break;

        case gecko_evt_le_connection_parameters_id:
            interval = evt->data.evt_le_connection_parameters.interval;
            pduSize = evt->data.evt_le_connection_parameters.txsize;
            slaveLatency = evt->data.evt_le_connection_parameters.latency;
            supervisionTimeout = evt->data.evt_le_connection_parameters.timeout;
            break;

        case gecko_evt_hardware_soft_timer_id:
            if (evt->data.evt_hardware_soft_timer.handle == SOFT_TIMER_FIXED_TRANSFER_TIME_HANDLE) {
                end_data_transmission(params);
            }
            break;

        case gecko_evt_le_connection_closed_id:
            printf("Connection closed.\n\n");
            reset_variables();
            gecko_cmd_le_gap_set_discovery_type(5, 0);
            gecko_cmd_le_gap_set_discovery_timing(5, SCAN_INTERVAL, SCAN_WINDOW);
            gecko_cmd_le_gap_start_discovery(initPhy, le_gap_discover_observation);
            state = State_SCANNING;
            break;
        
        default:
            break;
    }
    return askForInput;
}


/***************************************************************************************************
 * Static Function Definitions
 **************************************************************************************************/

// Make a loading animation while waiting for scan response match.
static void waiting_indication(void)
{
    static uint32_t cnt = 0;
    const char *fan[] = {"-", "\\", "|", "/"};
    printf("(%s)", fan[((cnt++) % 4)]);
    printf("\b\b\b");   // Move cursor back with backspace character '\b'.
    fflush(stdout);
}

// Reset handles, flags and calculation variables to initial state.
static void reset_variables(void)
{
    connection = 0xFF;
    serviceHandle = 0xFFFFFFFF;
    notificationsHandle = 0xFFFF;
    indicationsHandle = 0xFFFF;
    transmissionHandle = 0xFFFF;
    numCharacteristicsDiscovered = 0;
    throughput = 0;
    bitsSent = 0;
    operationCount = 0;
    interval = 0;
    mtuSize = 0;
    pduSize = 0;
    supervisionTimeout = 0;
    slaveLatency = 0;
    isFirstPacket = true;
    state = State_SCANNING;
}

static void start_data_transmission(TestParameters_t *params)
{
    throughput = 0;
    timer_start();

    // Turn OFF Display refresh on slave side
    if ((params->mode == 1) || (params->mode == 2)) {
        // This triggers the data transmission if we're on fixed data amount or fixed time modes.
        while (gecko_cmd_gatt_write_characteristic_value_without_response(connection, transmissionHandle, 1, &TRANSMISSION_ON)->result != 0);
    }

    if (params->mode == 1) {
        // Start fixed time one-shot soft timer.
        gecko_cmd_hardware_set_soft_timer(((HW_TICKS_PER_SECOND)*params->fixed_time), SOFT_TIMER_FIXED_TRANSFER_TIME_HANDLE, 1);
    }
}

// When transmission is done, print out the summary of the transmission.
static void end_data_transmission(TestParameters_t *params)
{
    volatile double endTime = timer_end();

    // Turn ON display again
    if ((params->mode == 1) || (params->mode == 2)) {
        // This triggers the data transmission end if we're on fixed data amount or fixed time modes.
        while (gecko_cmd_gatt_write_characteristic_value_without_response(connection, transmissionHandle, 1, &TRANSMISSION_OFF)->result != 0);
    }

    throughput = (uint64_t)((double)bitsSent / endTime);

    printf("-------------------------------\n");
    printf("RESULTS:\n\n");
    printf("Bits sent: %lu\n", bitsSent);
    printf("Time elapsed: %.3f sec\n", endTime);
    printf("Host calculated throughput: %lu bps\n", throughput);
    printf("Operation count: %lu\n", operationCount);
    printf("-------------------------------\n\n");

    isFirstPacket = true;
    bitsSent = 0;
    throughput = 0;
    operationCount = 0;
}

// Helper function to make the discovery and subscribing flow correct.
// Action enum values indicate which procedure was completed.
static void process_procedure_complete_event(struct gecko_cmd_packet *evt, TestParameters_t *params)
{
    uint16_t result = evt->data.evt_gatt_procedure_completed.result;

    switch (action) {
        case act_discover_service:
            set_action(act_none);
            if (!result) {
                printf("Starting characteristic discovery...\n");
                // Discover successful, start characteristic discovery.
                gecko_cmd_gatt_discover_characteristics(connection, serviceHandle);
                set_action(act_discover_characteristics);
            }
        break;

        case act_discover_characteristics:
            set_action(act_none);
            if (!result) {
                if (numCharacteristicsDiscovered == 4) {
                    printf("All necessary characteristics discovered.\n");
                    if (params->mode == 3) {
                        // In free mode subscribe to notifications first, then indications
                        printf("Subscribing to notifications.\n");
                        gecko_cmd_gatt_set_characteristic_notification(connection, notificationsHandle, gatt_notification);
                        set_action(act_enable_notification);
                    } else {
                        if (params->client_conf_flag == gatt_indication) {
                            printf("Subscribing to indications.\n");
                            gecko_cmd_gatt_set_characteristic_notification(connection, indicationsHandle, gatt_indication);
                            set_action(act_enable_indication);
                        } else if (params->client_conf_flag == gatt_notification) {
                            printf("Subscribing to notifications.\n");
                            gecko_cmd_gatt_set_characteristic_notification(connection, notificationsHandle, gatt_notification);
                            set_action(act_enable_notification);
                        }
                    }
                    
                }
            }
        break;

        case act_enable_notification:
            set_action(act_none);
            if (!result) {
                // Notifications turned on.
                printf("Subscribed to notifications.\n");
                
                if (params->mode == 3) {
                    printf("Subscribing to indications.\n");
                    gecko_cmd_gatt_set_characteristic_notification(connection, indicationsHandle, gatt_indication);
                    set_action(act_enable_indication);
                } else {
                    // Subscribe to slave result.
                    gecko_cmd_gatt_set_characteristic_notification(connection, resultHandle, gatt_indication);
                    set_action(act_subscribe_result);
                }
            }
        break;

        case act_enable_indication:
            set_action(act_none);
            if (!result) {
                // Indications turned on.
                printf("Subscribed to indications.\n");
                // Subscribe to slave result.
                gecko_cmd_gatt_set_characteristic_notification(connection, resultHandle, gatt_indication);
                set_action(act_subscribe_result);
            }
        break;

        case act_subscribe_result:
            set_action(act_none);
            if (!result) {
                printf("Subscribed to throughput result.\n");
                printf("\nDISCOVERY DONE.\n");
                printf("-----------------------------------------------------------------------------\n");
                printf("\nParameters to be used:\n");
                printf("-------------------------------\n");
                printf("Interval: %u\n", (unsigned int)((float)interval * 1.25));
                printf("Latency: %u\n", slaveLatency);
                printf("Timeout: %u\n", supervisionTimeout);
                printf("PDU size: %u\n", pduSize);
                printf("-----------------------------------------------------------------------------\n\n");
                printf("\nSTARTING TEST\n\n");
                state = State_TRANSMISSION;
                // In free mode, button press on slave triggers the transmission,
                // but in fixed modes, transmission is initiated here with the following call.
                if ((params->mode == 1) || (params->mode == 2)) {
                    start_data_transmission(params);
                }
            }
            break;

        case act_none:
            break;

        default:
            break;
    }
}

// Cycle through advertisement contents and look for matching device name.
static bool process_scan_response(struct gecko_msg_le_gap_scan_response_evt_t *pResp)
{
    int i = 0;
    bool adMatchFound = false;
    uint8_t adLen;
    uint8_t adType;

    while (i < (pResp->data.len - 1)) {
        adLen = pResp->data.data[i];
        adType = pResp->data.data[i + 1];

        /* Type 0x09 = Complete Local Name, 0x08 Shortened Name */
        if (adType == 0x09) {
            /* Check if device name is Throughput Tester */
            if (memcmp(pResp->data.data + i + 2, DEVICE_NAME, 17) == 0) {
                adMatchFound = true;
                break;
            }
        }
        /* Jump to next AD record */
        i = i + adLen + 1;
    }

    return (adMatchFound);
}

// Check if found characteristic matches the UUIDs that we are searching for.
static void check_characteristic_uuid(struct gecko_cmd_packet *evt) 
{
    if (evt->data.evt_gatt_characteristic.uuid.len == 16) {
        if (memcmp(NOTIFICATIONS_CHARACTERISTIC_UUID, evt->data.evt_gatt_characteristic.uuid.data, 16) == 0) {
            notificationsHandle = evt->data.evt_gatt_characteristic.characteristic;
            printf("Found notifications characteristic.\n");
            numCharacteristicsDiscovered++;
        } else if (memcmp(INDICATIONS_CHARACTERISTIC_UUID, evt->data.evt_gatt_characteristic.uuid.data, 16) == 0) {
            indicationsHandle = evt->data.evt_gatt_characteristic.characteristic;
            printf("Found indications characteristic.\n");
            numCharacteristicsDiscovered++;
        } else if (memcmp(TRANSMISSION_CHARACTERISTIC_UUID, evt->data.evt_gatt_characteristic.uuid.data, 16) == 0) {
            printf("Found transmission characteristic.\n");
            transmissionHandle = evt->data.evt_gatt_characteristic.characteristic;
            numCharacteristicsDiscovered++;
        } else if (memcmp(RESULT_CHARACTERISTIC_UUID, evt->data.evt_gatt_characteristic.uuid.data, 16) == 0) {
            printf("Found throughput result characteristic.\n");
            resultHandle = evt->data.evt_gatt_characteristic.characteristic;
            numCharacteristicsDiscovered++;
        }
    }
}