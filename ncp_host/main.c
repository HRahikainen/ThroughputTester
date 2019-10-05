/* Throughput tester NCP example application*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "infrastructure.h"

/* BG stack headers */
#include "gecko_bglib.h"

/* hardware specific headers */
#include "uart.h"

/* application specific files */
#include "app.h"

/***************************************************************************************************
 * Local Macros and Definitions
 **************************************************************************************************/
BGLIB_DEFINE();

// The default baud rate to use.
const uint32_t DEFAULT_BAUD_RATE = 115200;
// The serial port to use for BGAPI communication.
static char *uartPort = NULL;
// The baud rate to use.
static uint32_t baudRate = 0;
// Enable flow control by default.
static uint32_t flowControl = 1;
static volatile int userKeyboardInterrupt = 0;

// Test parameters structure default values.
// 50 ms interval, 1M PHY, 250B MTU, Notifications, Free Mode.
TestParameters_t params = {
  .connection_interval = 40,
  .phy = 1,
  .mtu_size = 250,
  .client_conf_flag = 1,
  .mode = 3,
  .fixed_time = 0,
  .fixed_amount = 0
};

/***************************************************************************************************
 * Static Function Declarations
 **************************************************************************************************/

static int init_serialport(int argc, char *argv[], int32_t timeout);
static void on_message_send(uint32_t msg_len, uint8_t *msg_data);
// Get CTRL+C interrupt signal and toggle flag.
static void sighandler(int sig) { userKeyboardInterrupt = 1; }
static void usage(void);
static void help(void);
static void handle_user_input(void);
static void parse_commands(int argc, char *argv[]);

/***************************************************************************************************
 * Public Function Definitions
 **************************************************************************************************/

/***********************************************************************************************/ /**
 *  \brief  The main program.
 *  \param[in] argc Argument count.
 *  \param[in] argv Buffer contaning Serial Port data.
 *  \return  0 on success, -1 on failure.
 **************************************************************************************************/
int main(int argc, char *argv[])
{
  struct gecko_cmd_packet *evt;
  signal(SIGINT, sighandler); // Setup interrupt handler.
  /* Initialize BGLIB with our output function for sending messages. */
  BGLIB_INITIALIZE_NONBLOCK(on_message_send, uartRx, uartRxPeek);

  /* Initialise serial communication as non-blocking. */
  if (init_serialport(argc, argv, 100) < 0) {
    printf("Non-blocking serial port init failure\n");
    exit(EXIT_FAILURE);
  }

  fflush(stdout);

  printf("\n\nStarting up...\nResetting NCP target...\n");

  /* Reset NCP to ensure it gets into a defined state.
   * Once the chip successfully boots, gecko_evt_system_boot_id event should be received. */
  gecko_cmd_system_reset(0);

  while (1) {
    if (userKeyboardInterrupt) {
      if (params.mode == 3) { // CTRL+C quits free mode straight away.
        gecko_cmd_system_reset(0);
        uartClose();
        printf("Exiting program from free mode...\n\n");
        exit(0);
      } else {
        handle_user_input();
      }
    }
    // Check for stack event.
    evt = gecko_peek_event();

    // Run application and event handler.
    // Return value is 1 if user input is needed after one-shot test run, default 0.
    if (app_handle_events(evt, &params) == 1) {
      handle_user_input();
    }
  }

  return -1;
}


/***************************************************************************************************
 * Static Function Definitions
 **************************************************************************************************/

/***********************************************************************************************/ /**
 *  \brief  Serial Port initialisation routine.
 *  \param[in] argc Argument count.
 *  \param[in] argv Buffer contaning Serial Port data.
 *  \return  0 on success, -1 on failure.
 **************************************************************************************************/
static int init_serialport(int argc, char *argv[], int32_t timeout)
{
  // Handle the command-line arguments.
  baudRate = DEFAULT_BAUD_RATE;
  parse_commands(argc, argv);

  if (!uartPort || !baudRate || (flowControl > 1)) {
    usage();
    exit(EXIT_FAILURE);
  }

  /* Initialise the serial port with RTS/CTS enabled. */
  return uartOpen((int8_t *)uartPort, baudRate, flowControl, timeout);
}
/***********************************************************************************************/ /**
 *  \brief  Function called when a message needs to be written to the serial port.
 *  \param[in] msg_len Length of the message.
 *  \param[in] msg_data Message data, including the header.
 **************************************************************************************************/
static void on_message_send(uint32_t msg_len, uint8_t *msg_data)
{
  // Variable for storing function return values.
  int32_t ret;

  ret = uartTx(msg_len, msg_data);
  if (ret < 0) {
    printf("Failed to write to serial port %s, ret: %d, errno: %d\n", uartPort, ret, errno);
    exit(EXIT_FAILURE);
  }
}

// Command line interface help message utility functions
static void usage(void)
{
  printf("Examples of usage:\n");
  printf("  throughput.exe -p COM11\n");                                                  // Default, free mode, 1M phy
  printf("  throughput.exe -p COM11 -m 1 5\n");                                           // Fixed time 5 seconds
  printf("  throughput.exe -p COM11 -m 2 50000\n");                                       // Fixed amount 50 kB
  printf("  throughput.exe -p COM11 --params 2 25 240 1\n");                              // Free mode with 2M phy and different interval and MTU
  printf("  throughput.exe -p COM11 -b 2000000 -f 1 -m 1 5 --params 1 50 250 1\n");
  printf("  throughput.exe -p COM11 -b 2000000 -f 1 -m 2 100000 --params 2 25 250 1\n");  // Different modes and PHYs with full verbosity
  printf("  throughput.exe -p COM11 -b 2000000 -f 1 -m 3 --params 4 200 250 2\n");
  printf("  throughput.exe -h \n\n");
}

static void help(void)
{
  printf("\nHelp:\n");
  printf("-p <port>       - COM port e.g. COM11 \n");
  printf("-b <baudRate>   - Baud rate.\n");
  printf("                  Default %u b/s.\n", DEFAULT_BAUD_RATE);
  printf("-f <1/0>        - Enable/Disable flow control. Enabled by default (1).\n");
  printf("-m <1/2/3>      - Transmission mode.\n");
  printf("1=fixed time in seconds, 2=fixed data amount in bytes, 3=free mode using buttons on slave.\n");
  printf("--params        - Connection parameters <phy 1=1M/2=2M/4=LE Coded (S8) > <connection interval [ms]> <mtu size [B]> <1=notify/2=indicate>\n");
  printf("                  Defaults: 1, 50 ms, 250B, 1=notifications/2=indications\n");
  printf("-h              - Help\n\n");
  usage();
  exit(EXIT_SUCCESS);
}

// Prompt user to exit or boot and start a new scan.
static void handle_user_input(void)
{
  char command[64];
  printf("\n\nRun the test again? (run/exit)>");

  fgets(command, sizeof(command) - 2, stdin);
  fflush(stdin);

  if (strncmp(command, "exit\n", 5) == 0) {
    gecko_cmd_system_reset(0);
    uartClose();
    exit(0);
  } else if (strncmp(command, "run\n", 4) == 0) {
    gecko_cmd_le_gap_end_procedure();
    gecko_cmd_system_reset(0); // Go to regular boot and start scanning again.
  } else {
    printf("Invalid command: %s\n", command);
    handle_user_input();
  }

  if (signal(SIGINT, &sighandler) == SIG_ERR) {
    printf("\nCan't catch SIGINT\n");
  }
  userKeyboardInterrupt = 0;
}

/***********************************************************************************************/ /**
 *  \brief  Command line parser for additional parameters
 *  \param[in] argc Argument count.
 *  \param[in] argv Buffer contaning .
 **************************************************************************************************/
static void parse_commands(int argc, char *argv[]) 
{
  if (argc == 1) {
    help();
  }

  for (uint8_t i = 1; i < argc; i++)
  { // 0th argument is name of executable so start at 1st.
    if (argv[i][0] == '-') { // -flag.
      // Port
      if (argv[i][1] == 'p') {
        if (argv[i + 1]) {
          uartPort = argv[i + 1];
        }
        // Baud rate
      } else if (argv[i][1] == 'b') {
        if (argv[i + 1]) {
          baudRate = atoi(argv[i + 1]);
        }
        // Flow control
      } else if (argv[i][1] == 'f') {
        if (argv[i + 1]) {
          flowControl = atoi(argv[i + 1]);
        }
        // Transimission Mode
      } else if (argv[i][1] == 'm') {
        // Assign mode
        if (argv[i + 1]) {
          if ((atoi(argv[i + 1]) == 1) || (atoi(argv[i + 1]) == 2) || (atoi(argv[i + 1]) == 3)) {
            params.mode = atoi(argv[i + 1]);
            if (params.mode == 1) { // Fixed modes take the time or amount as argument so check for that.
              if (argv[i + 2]) {
                if ((atoi(argv[i + 2]) >= 1) && (atoi(argv[i + 2]) < 600)) { // Between 1s and 10min
                  params.fixed_time = atoi(argv[i + 2]);
                } else {
                  printf("Fixed time has invalid type or exceeds interval 1s - 10 min.\n");
                  exit(EXIT_FAILURE);
                }
              } else {
                printf("Please input a valid time parameter.\n");
                exit(EXIT_FAILURE);
              }
            } else if (params.mode == 2) {
              if (argv[i + 2]) {
                if ((atoi(argv[i + 2]) >= 1000) && (atoi(argv[i + 2]) < 10000000)) { // Between 1k and 10M.
                  params.fixed_amount = atoi(argv[i + 2]);
                } else {
                  printf("Fixed amount has invalid type or exceeds interval 1k - 10M.\n");
                  exit(EXIT_FAILURE);
                }
              } else {
                printf("Please input a valid data amount parameter.\n");
                exit(EXIT_FAILURE);
              }
            }
          } else {
            printf("Mode must be one of these: 1 = fixed transmit time , 2 = fixed transmit data, 3 = free mode with buttons.\n");
            exit(EXIT_FAILURE);
          }
        }
      } else if (argv[i][1] == '-') {
        // --flags
        if (strncmp(&argv[i][2], "params", 6) == 0) {
          // Assign test parameters.
          if (argv[i + 1]) {
            if ((atoi(argv[i + 1]) == 1) || (atoi(argv[i + 1]) == 2) || (atoi(argv[i + 1]) == 4)) {
              params.phy = atoi(argv[i + 1]);
              if (argv[i + 2]) {
                if (atoi(argv[i + 2]) >= 20) {
                  // User input is in ms, but value is passed as (ms / 1.25)
                  params.connection_interval = (uint16_t)(((float)atoi(argv[i + 2])) / 1.25);
                  if (argv[i + 3]) {
                    if ((atoi(argv[i + 3]) >= 23) && (atoi(argv[i + 3]) <= 250)) {
                      params.mtu_size = atoi(argv[i + 3]);
                      if (argv[i + 4]) {
                        if ((atoi(argv[i + 4]) == 1) || (atoi(argv[i + 4]) == 2)) {
                          params.client_conf_flag = atoi(argv[i + 4]);
                        } else {
                          printf("Wrong Client Characteristic Configuration argument. Must be 1 for notification or 2 for indication\n");
                          exit(EXIT_FAILURE);
                        }
                      }
                    } else {
                      printf("MTU size must be between 23 and 250.\n");
                      exit(EXIT_FAILURE);
                    }
                  }
                } else {
                  printf("Connection interval should be above 20 (20 * 1.25 = 25 ms).\n");
                  exit(EXIT_FAILURE);
                }
              }
            } else {
              printf("PHY must be one of these: 1 = 1M, 2 = 2M, 4 = 125k\n");
              exit(EXIT_FAILURE);
            }
          }
        }
        // Show help
      } else if (argv[i][1] == 'h') {
        help();
        exit(EXIT_SUCCESS);
      } else {
        usage();
        exit(EXIT_FAILURE);
      }
    }
  }
}
