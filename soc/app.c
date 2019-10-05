/***************************************************************************//**
 * @file
 * @brief app.c
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

/* Bluetooth stack headers */
#include "bg_types.h"
#include "native_gecko.h"
#include "gatt_db.h"
#include "gpiointerrupt.h"
#include "graphics.h"

#include "init_mcu.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_rtcc.h"

#include "app.h"
#include "app_utils.h"

/* Main application */
void appMain(gecko_configuration_t *pconfig)
{
#if DISABLE_SLEEP > 0
  pconfig->sleep.flags = 0;
#endif

  /* Initialize debug prints. Note: debug prints are off by default. See DEBUG_LEVEL in app.h */
  initLog();

  pconfig->bluetooth.max_advertisers = 4; // Change to allow more than one connectable advertisement set

  /* Initialize stack */
  gecko_init(pconfig);
  // Adaptive frequency hopping enabled if transmit power set to over 10 dBm
#if (TX_POWER > 100)
  gecko_init_afh();
#endif

#if defined(_SILICON_LABS_32B_SERIES_1_CONFIG_3) //|| defined(_SILICON_LABS_32B_SERIES_2_CONFIG_1)
  // RTCC init
 void initMcu_RTCC(void) {
    /* Enable clock to LE modules */
    CMU_ClockEnable(cmuClock_CORELE, true);
    /* Enable clock for RTCC */
    CMU_ClockEnable(cmuClock_RTCC, true);
    RTCC_Init_TypeDef rtccInit = RTCC_INIT_DEFAULT;

    rtccInit.enable                = true;
    rtccInit.debugRun              = false;
    rtccInit.precntWrapOnCCV0      = false;
    rtccInit.cntWrapOnCCV1         = false;
    rtccInit.prescMode             = rtccCntTickPresc;
    rtccInit.presc                 = rtccCntPresc_1;
    rtccInit.enaOSCFailDetect      = false;
    rtccInit.cntMode               = rtccCntModeNormal;

    RTCC_Init(&rtccInit);
  }
 initMcu_RTCC();
#endif

  setup_pins_interrupts();
   // Set mode to master if PB0 pressed at boot time.
  if (GPIO_PinInGet(BSP_BUTTON0_PORT, BSP_BUTTON0_PIN)) {
    roleIsSlave = true;
    roleString = (char *)ROLE_ADVERT_STRING;
  } else {
    roleIsSlave = false;
    roleString = (char *)ROLE_SCANNER_STRING;
  }

  // Initialize display
  GRAPHICS_Init();

  if (roleIsSlave) {
    slave_main();
  } else {
    master_main();
  }
}
