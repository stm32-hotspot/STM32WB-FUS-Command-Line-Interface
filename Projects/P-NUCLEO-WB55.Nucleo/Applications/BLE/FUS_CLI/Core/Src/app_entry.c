/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_entry.c
  * @author  MCD Application Team
  * @brief   Entry point of the Application
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_common.h"
#include "main.h"
#include "app_entry.h"
#include "tl.h"
#include "stm32_seq.h"
#include "shci_tl.h"
#include "stm32_lpm.h"
#include "app_debug.h"
#include "stm_logging.h"
#include "dbg_trace.h"
#include "shci.h"
#include "otp.h"

/* Private includes -----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
extern RTC_HandleTypeDef hrtc;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/
#define POOL_SIZE               550	/* Shall be large enough to receive at least the ready event SHCI_SUB_EVT_CODE_READY */
#define FLASH_SECTOR_SIZE       4096	/* 4Kb */
#define LONG_PRESS_THRESHOLD    2000U   /* 2s  */
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t EvtPool[POOL_SIZE];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static TL_CmdPacket_t SystemCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t SystemSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255U];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t BleSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255];

/* USER CODE BEGIN PV */
/* Recover the end of the application from linker file */
extern const uint32_t __END_OF_APPLICATION_ADDRESS__;

/* USER CODE END PV */

/* Private functions prototypes-----------------------------------------------*/
static void Config_HSE(void);
static void Reset_Device(void);
#if (CFG_HW_RESET_BY_FW == 1)
static void Reset_IPCC(void);
static void Reset_BackupDomain(void);
#endif /* CFG_HW_RESET_BY_FW == 1*/
static void System_Init(void);
static void SystemPower_Config(void);
static void appe_Tl_Init(void);
static void APPE_SysStatusNot(SHCI_TL_CmdStatus_t status);
static void APPE_SysUserEvtRx(void * pPayload);
static void APPE_SysEvtReadyProcessing(void * pPayload);
static void Init_Rtc(void);
static void Delete_Sectors(void);
static void APP_Entry_Key_Button1_Action(void);
static void APP_Entry_Key_Button2_Action(void);
static void APP_Entry_Key_Button3_Action(void);
/* USER CODE BEGIN PFP */
static void Led_Init( void );
static void Button_Init( void );
/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void MX_APPE_Config(void)
{
  /**
   * The OPTVERR flag is wrongly set at power on
   * It shall be cleared before using any HAL_FLASH_xxx() api
   */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

  /**
   * Reset some configurations so that the system behave in the same way
   * when either out of nReset or Power On
   */
  Reset_Device();

  /* Configure HSE Tuning */
  Config_HSE();

  return;
}

void MX_APPE_Init(void)
{
  System_Init();       /**< System initialization */

  SystemPower_Config(); /**< Configure the system Power Mode */

  HW_TS_Init(hw_ts_InitMode_Full, &hrtc); /**< Initialize the TimerServer */

/* USER CODE BEGIN APPE_Init_1 */
  APPD_Init();
  
  /**
   * The Standby mode should not be entered before the initialization is over
   * The default state of the Low Power Manager is to allow the Standby Mode so an request is needed here
   */
  UTIL_LPM_SetOffMode(1 << CFG_LPM_APP, UTIL_LPM_DISABLE);

  Led_Init();

  Button_Init();
/* USER CODE END APPE_Init_1 */
  appe_Tl_Init();	/* Initialize all transport layers */

  /**
   * From now, the application is waiting for the ready event (VS_HCI_C2_Ready)
   * received on the system channel before starting the Stack
   * This system event is received with APPE_SysUserEvtRx()
   */
/* USER CODE BEGIN APPE_Init_2 */

/* USER CODE END APPE_Init_2 */
   return;
}

void Init_Smps(void)
{
#if (CFG_USE_SMPS != 0)
  /**
   *  Configure and enable SMPS
   *
   *  The SMPS configuration is not yet supported by CubeMx
   *  when SMPS output voltage is set to 1.4V, the RF output power is limited to 3.7dBm
   *  the SMPS output voltage shall be increased for higher RF output power
   */
  LL_PWR_SMPS_SetStartupCurrent(LL_PWR_SMPS_STARTUP_CURRENT_80MA);
  LL_PWR_SMPS_SetOutputVoltageLevel(LL_PWR_SMPS_OUTPUT_VOLTAGE_1V40);
  LL_PWR_SMPS_Enable();
#endif /* CFG_USE_SMPS != 0 */

  return;
}

void Init_Exti(void)
{
  /* Enable IPCC(36), HSEM(38) wakeup interrupts on CPU1 */
  LL_EXTI_EnableIT_32_63(LL_EXTI_LINE_36 | LL_EXTI_LINE_38);

  return;
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
static void Reset_Device(void)
{
#if (CFG_HW_RESET_BY_FW == 1)
  Reset_BackupDomain();

  Reset_IPCC();
#endif /* CFG_HW_RESET_BY_FW == 1 */

  return;
}

#if (CFG_HW_RESET_BY_FW == 1)
static void Reset_BackupDomain(void)
{
  if ((LL_RCC_IsActiveFlag_PINRST() != FALSE) && (LL_RCC_IsActiveFlag_SFTRST() == FALSE))
  {
    HAL_PWR_EnableBkUpAccess(); /**< Enable access to the RTC registers */

    /**
     *  Write twice the value to flush the APB-AHB bridge
     *  This bit shall be written in the register before writing the next one
     */
    HAL_PWR_EnableBkUpAccess();

    __HAL_RCC_BACKUPRESET_FORCE();
    __HAL_RCC_BACKUPRESET_RELEASE();
  }

  return;
}

static void Reset_IPCC(void)
{
  LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_IPCC);

  LL_C1_IPCC_ClearFlag_CHx(
      IPCC,
      LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
      | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  LL_C2_IPCC_ClearFlag_CHx(
      IPCC,
      LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
      | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  LL_C1_IPCC_DisableTransmitChannel(
      IPCC,
      LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
      | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  LL_C2_IPCC_DisableTransmitChannel(
      IPCC,
      LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
      | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  LL_C1_IPCC_DisableReceiveChannel(
      IPCC,
      LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
      | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  LL_C2_IPCC_DisableReceiveChannel(
      IPCC,
      LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
      | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  return;
}
#endif /* CFG_HW_RESET_BY_FW == 1 */

static void Config_HSE(void)
{
    OTP_ID0_t * p_otp;

  /**
   * Read HSE_Tuning from OTP
   */
  p_otp = (OTP_ID0_t *) OTP_Read(0);
  if (p_otp)
  {
    LL_RCC_HSE_SetCapacitorTuning(p_otp->hse_tuning);
  }

  return;
}

static void System_Init(void)
{
  Init_Smps();

  Init_Exti();

  Init_Rtc();

  return;
}

static void Init_Rtc(void)
{
  /* Disable RTC registers write protection */
  LL_RTC_DisableWriteProtection(RTC);

  LL_RTC_WAKEUP_SetClock(RTC, CFG_RTC_WUCKSEL_DIVIDER);

  /* Enable RTC registers write protection */
  LL_RTC_EnableWriteProtection(RTC);

  return;
}

/**
 * @brief  Configure the system for power optimization
 *
 * @note  This API configures the system to be ready for low power mode
 *
 * @param  None
 * @retval None
 */
static void SystemPower_Config(void)
{
  /**
   * Select HSI as system clock source after Wake Up from Stop mode
   */
  LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI);

  /* Initialize low power manager */
  UTIL_LPM_Init();
  /* Initialize the CPU2 reset value before starting CPU2 with C2BOOT */
  LL_C2_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);

#if (CFG_USB_INTERFACE_ENABLE != 0)
  /**
   *  Enable USB power
   */
  HAL_PWREx_EnableVddUSB();
#endif /* CFG_USB_INTERFACE_ENABLE != 0 */

  return;
}

static void appe_Tl_Init(void)
{
  TL_MM_Config_t tl_mm_config;
  SHCI_TL_HciInitConf_t SHci_Tl_Init_Conf;
  /**< Reference table initialization */
  TL_Init();

  /**< System channel initialization */
  UTIL_SEQ_RegTask(1<< CFG_TASK_SYSTEM_HCI_ASYNCH_EVT_ID, UTIL_SEQ_RFU, shci_user_evt_proc);
  SHci_Tl_Init_Conf.p_cmdbuffer = (uint8_t*)&SystemCmdBuffer;
  SHci_Tl_Init_Conf.StatusNotCallBack = APPE_SysStatusNot;
  shci_init(APPE_SysUserEvtRx, (void*) &SHci_Tl_Init_Conf);

  /**< Memory Manager channel initialization */
  tl_mm_config.p_BleSpareEvtBuffer = BleSpareEvtBuffer;
  tl_mm_config.p_SystemSpareEvtBuffer = SystemSpareEvtBuffer;
  tl_mm_config.p_AsynchEvtPool = EvtPool;
  tl_mm_config.AsynchEvtPoolSize = POOL_SIZE;
  TL_MM_Init(&tl_mm_config);

  TL_Enable();

  return;
}

static void APPE_SysStatusNot(SHCI_TL_CmdStatus_t status)
{
  UNUSED(status);
  return;
}

/**
 * The type of the payload for a system user event is tSHCI_UserEvtRxParam
 * When the system event is both :
 *    - a ready event (subevtcode = SHCI_SUB_EVT_CODE_READY)
 *    - reported by the FUS (sysevt_ready_rsp == FUS_FW_RUNNING)
 * The buffer shall not be released
 * (eg ((tSHCI_UserEvtRxParam*)pPayload)->status shall be set to SHCI_TL_UserEventFlow_Disable)
 * When the status is not filled, the buffer is released by default
 */
static void APPE_SysUserEvtRx(void * pPayload)
{
  TL_AsynchEvt_t *p_sys_event;
  WirelessFwInfo_t WirelessInfo;

  p_sys_event = (TL_AsynchEvt_t*)(((tSHCI_UserEvtRxParam*)pPayload)->pckt->evtserial.evt.payload);

  /* Read the firmware version of both the wireless firmware and the FUS */
  SHCI_GetWirelessFwInfo(&WirelessInfo);
  APP_DBG_MSG("Wireless Firmware version %d.%d.%d\n", WirelessInfo.VersionMajor, WirelessInfo.VersionMinor, WirelessInfo.VersionSub);
  APP_DBG_MSG("Wireless Firmware build %d\n", WirelessInfo.VersionReleaseType);
  APP_DBG_MSG("FUS version %d.%d.%d\n", WirelessInfo.FusVersionMajor, WirelessInfo.FusVersionMinor, WirelessInfo.FusVersionSub);
  
  switch(WirelessInfo.StackType)
  {
  case INFO_STACK_TYPE_BLE_FULL:
    APP_DBG_MSG("Wireless Firmware Type : Ble Full\n");
    break;

  case INFO_STACK_TYPE_BLE_FULL_EXT_ADV:
    APP_DBG_MSG("Wireless Firmware Type : Ble Full Extended\n");
    break;

  case INFO_STACK_TYPE_BLE_BASIC:
    APP_DBG_MSG("Wireless Firmware Type : Ble Basic\n");
    break;

  case INFO_STACK_TYPE_BLE_HCI:
    APP_DBG_MSG("Wireless Firmware Type : Ble HCI\n");
    break;

  case INFO_STACK_TYPE_BLE_LIGHT:
    APP_DBG_MSG("Wireless Firmware Type : Ble Light\n");
    break;

  case INFO_STACK_TYPE_BLE_BEACON:
    APP_DBG_MSG("Wireless Firmware Type : Ble Beacon\n");
    break;

  case INFO_STACK_TYPE_THREAD_FTD:
    APP_DBG_MSG("Wireless Firmware Type : Thread FTD\n");
    break;

  case INFO_STACK_TYPE_THREAD_MTD:
    APP_DBG_MSG("Wireless Firmware Type : Thread MTD\n");
    break;

  case INFO_STACK_TYPE_ZIGBEE_FFD:
    APP_DBG_MSG("Wireless Firmware Type : Zigbee FFD\n");
    break;

  case INFO_STACK_TYPE_ZIGBEE_RFD:
    APP_DBG_MSG("Wireless Firmware Type : Zigbee RFD\n");
    break;

  case INFO_STACK_TYPE_MAC:
    APP_DBG_MSG("Wireless Firmware Type : Mac\n");
    break;

  case INFO_STACK_TYPE_BLE_THREAD_FTD_STATIC:
    APP_DBG_MSG("Wireless Firmware Type : Ble Thread FTD Static\n");
    break;

  case INFO_STACK_TYPE_BLE_THREAD_FTD_DYAMIC:
    APP_DBG_MSG("Wireless Firmware Type : Ble Thread FTD Dynamic\n");
    break;
   
  case INFO_STACK_TYPE_802154_LLD_TESTS:
    APP_DBG_MSG("Wireless Firmware Type : 802.15.4 LLD Tests\n");
    break;

  case INFO_STACK_TYPE_802154_PHY_VALID:
    APP_DBG_MSG("Wireless Firmware Type : 802.15.4 Phy Valid\n");
    break;

  case INFO_STACK_TYPE_BLE_PHY_VALID:
    APP_DBG_MSG("Wireless Firmware Type : Ble Phy Valid\n");
    break;
          
  case INFO_STACK_TYPE_BLE_LLD_TESTS:
    APP_DBG_MSG("Wireless Firmware Type : Ble LLD Tests\n");
    break;

  case INFO_STACK_TYPE_BLE_RLV:
    APP_DBG_MSG("Wireless Firmware Type : Ble RLV\n");
    break;

  case INFO_STACK_TYPE_802154_RLV:
    APP_DBG_MSG("Wireless Firmware Type : 802.15.4 RLV\n");
    break;

  case INFO_STACK_TYPE_BLE_ZIGBEE_FFD_STATIC:
    APP_DBG_MSG("Wireless Firmware Type : Ble Zigbee FFD Static\n");
    break;
          
  case INFO_STACK_TYPE_BLE_ZIGBEE_RFD_STATIC:
    APP_DBG_MSG("Wireless Firmware Type : Ble Zigbee RFD Static\n");
    break;
          
  case INFO_STACK_TYPE_BLE_ZIGBEE_FFD_DYNAMIC:
    APP_DBG_MSG("Wireless Firmware Type : Ble Zigbee FFD Dynamic\n");
    break;

  case INFO_STACK_TYPE_BLE_ZIGBEE_RFD_DYNAMIC:
    APP_DBG_MSG("Wireless Firmware Type : Ble Zigbee RFD Dynamic\n");
    break;

  case INFO_STACK_TYPE_RLV:
    APP_DBG_MSG("Wireless Firmware Type : RLV\n");
    break;

  default:
    APP_DBG_MSG("Wireless Firmware Type : Unknown\n");
    break;
  }
  APP_DBG_MSG("\n");
  
  switch(p_sys_event->subevtcode)
  {
  case SHCI_SUB_EVT_CODE_READY:
    APPE_SysEvtReadyProcessing(pPayload);
    break;

  default:
    break;
  }

  return;
}

static void APPE_SysEvtReadyProcessing(void * pPayload)
{
  uint8_t fus_state_value;
  uint32_t timing_count_second;
  uint32_t timing_count_minute;
  
  TL_AsynchEvt_t *p_sys_event;
  SHCI_C2_Ready_Evt_t *p_sys_ready_event;
  p_sys_event = (TL_AsynchEvt_t*)(((tSHCI_UserEvtRxParam*)pPayload)->pckt->evtserial.evt.payload);
  p_sys_ready_event = (SHCI_C2_Ready_Evt_t*) p_sys_event->payload;
  
  /* Traces channel initialization */
  TL_TRACES_Init();

#if ( CFG_LED_SUPPORTED != 0)
  BSP_LED_Off(LED_BLUE);
  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_RED);
#endif

  if(p_sys_ready_event->sysevt_ready_rsp == WIRELESS_FW_RUNNING)
  {
    APP_DBG("WIRELESS FW IS RUNNING");

    switch(CFG_REBOOT_VAL_MSG)
    {
    case CFG_WAITING_BUTTON_REQ:
      /**
       * Wait for button pressed
       */
      APP_DBG("CFG_WAITING_BUTTON_REQ");
      APP_DBG("Press SW1 to delete the installed Wireless Stack");
      APP_DBG("Press SW2 to install the CPU2 update (either FUS or Wireless Stack)");
      APP_DBG("Short press SW3 to switch between FUS and Wireless Stack");
      APP_DBG("Long press SW3 to delete all sectors between 0x08008000 and SFSA");
      while(1)
      {
#if ( CFG_LED_SUPPORTED != 0)
        BSP_LED_On(LED_GREEN);
        HAL_Delay(200);
        BSP_LED_Off(LED_GREEN);
        HAL_Delay(200);
#endif
      }
      break;

    case CFG_SW3_FUS_CPU2_SWITCH:
      CFG_REBOOT_VAL_MSG = CFG_WAITING_BUTTON_REQ;
    case CFG_SW1_FUS_FW_DELETE:
    case CFG_SW2_FUS_FW_UPGRADE:
      APP_DBG("Call SHCI_C2_FUS_GetState twice to reboot on FUS");
      /**
       * The wireless firmware does not support any FUS command except CKS and SHCI_C2_FUS_GetState() to reboot on FUS
       * Request CPU2 to reboot on FUS by sending two FUS command
       */
      (void)SHCI_C2_FUS_GetState( NULL );
      (void)SHCI_C2_FUS_GetState( NULL );
      while(1);
      break;

    default:
      APP_DBG("Default case set CFG_REBOOT_VAL_MSG = CFG_WAITING_BUTTON_REQ");
      CFG_REBOOT_VAL_MSG = CFG_WAITING_BUTTON_REQ;
#if ( CFG_LED_SUPPORTED != 0)
      BSP_LED_On(LED_RED);
#endif
      while(1);
      break;
    }

  }
  else
  {
    APP_DBG("FUS IS RUNNING");

    /**
     * FUS is running on CPU2
     */

    /**
     * The CPU2 firmware update procedure is starting from now
     * There may be several device reset during CPU2 firmware upgrade
     * The key word at the beginning of SRAM1 shall be changed CFG_REBOOT_ON_CPU2_UPGRADE
     *
     * Wireless Firmware upgrade:
     * Once the upgrade is over, the CPU2 will run the wireless stack
     * When the wireless stack is running, the SRAM1 is checked and when equal to CFG_REBOOT_ON_CPU2_UPGRADE,
     * it means we may restart on the firmware application.
     *
     * FUS Firmware Upgrade:
     * Once the upgrade is over, the CPU2 will run FUS and the FUS return the Idle state
     * The SRAM1 is checked and when equal to CFG_REBOOT_ON_CPU2_UPGRADE,
     * it means we may restart on the firmware application.
     */
    fus_state_value = SHCI_C2_FUS_GetState( NULL );

    APP_DBG("fus_state_value = %d", fus_state_value);
    APP_DBG("CFG_REBOOT_VAL_MSG = %d", CFG_REBOOT_VAL_MSG);

    if( fus_state_value == FUS_STATE_VALUE_ERROR)
    {
      APP_DBG("fus_state_value == FUS_STATE_VALUE_ERROR call NVIC_SystemReset()");
      /**
       * This is the first time in the life of the product the FUS is involved. After this command, it will be properly initialized
       * Request the device to reboot to install the wireless firmware
       */
      NVIC_SystemReset(); /* it waits until reset */
    }

    if(fus_state_value != FUS_STATE_VALUE_IDLE)
    {
      APP_DBG("fus_state_value != FUS_STATE_VALUE_IDLE : An upgrade is on going, Wait to reboot on the wireless stack");
      /**
       * An upgrade is on going
       * Wait to reboot on the wireless stack
       */
#if ( CFG_LED_SUPPORTED != 0)
      BSP_LED_On(LED_BLUE);
#endif
      timing_count_second = 0;
      timing_count_minute = 0;
      while(1)
      {
        HAL_Delay(10000);   /* Wait 10s */
        timing_count_second += 10;
        if(timing_count_second == 60)
        {
          timing_count_second = 0;
          timing_count_minute++;
        }
        APP_DBG("Upgrade ongoing : %dmn%ds",  timing_count_minute, timing_count_second);
        fus_state_value = SHCI_C2_FUS_GetState( NULL );
        if((fus_state_value < FUS_STATE_VALUE_FW_UPGRD_ONGOING) || (fus_state_value > FUS_STATE_VALUE_FUS_UPGRD_ONGOING_END) )
        {
          APP_DBG("fus_state_value == %d call NVIC_SystemReset()", fus_state_value);
          HAL_Delay(100);   /* Wait 100us to output traces */
          NVIC_SystemReset(); /* it waits until reset */
        }
      }
    }

    /**
     * FUS is idle
     * Request an upgrade and wait to reboot on the wireless stack
     * The first two parameters are currently not supported by the FUS
     */

    switch(CFG_REBOOT_VAL_MSG)
    {
    case CFG_WAITING_BUTTON_REQ:
      APP_DBG("CFG_WAITING_BUTTON_REQ");
      APP_DBG("Press SW1 to delete the installed Wireless Stack");
      APP_DBG("Press SW2 to install the CPU2 update (either FUS or Wireless Stack)");
      APP_DBG("Short press SW3 to switch between FUS and Wireless Stack");
      APP_DBG("Long press SW3 to delete all sectors between 0x08008000 and SFSA");
      /**
       * Wait for button pressed
       */
      while(1)
      {
#if ( CFG_LED_SUPPORTED != 0)
        BSP_LED_On(LED_GREEN);
        HAL_Delay(50);
        BSP_LED_Off(LED_GREEN);
        HAL_Delay(50);
#endif
      }
      break;

    case CFG_SW1_FUS_FW_DELETE:
      APP_DBG("case CFG_SW1_FUS_FW_DELETE : executing SHCI_C2_FUS_FwDelete()");
      CFG_REBOOT_VAL_MSG = CFG_WAITING_BUTTON_REQ;
      SHCI_C2_FUS_FwDelete();
#if ( CFG_LED_SUPPORTED != 0)
      BSP_LED_On(LED_BLUE);
#endif
      /**
       * Wait for a system reboot
       */
      while(1);
      break;

    case CFG_SW2_FUS_FW_UPGRADE:
      APP_DBG("case CFG_SW2_FUS_FW_UPGRADE");
      APP_DBG("Request FUS to upgrade the CPU2 firmware");
      CFG_REBOOT_VAL_MSG = CFG_WAITING_BUTTON_REQ;
      SHCI_C2_FUS_FwUpgrade(0,0);
#if ( CFG_LED_SUPPORTED != 0)
      BSP_LED_On(LED_BLUE);
#endif
      /**
       * Wait for a system reboot
       */
      timing_count_second = 0;
      timing_count_minute = 0;
      while(1)
      {
        HAL_Delay(10000);   /* Wait 10s */
        timing_count_second += 10;
        if(timing_count_second == 60)
        {
          timing_count_second = 0;
          timing_count_minute++;
        }
        APP_DBG("Upgrade ongoing : %dmn%ds",  timing_count_minute, timing_count_second);
        fus_state_value = SHCI_C2_FUS_GetState( NULL );
        if( (fus_state_value < FUS_STATE_VALUE_FW_UPGRD_ONGOING) || (fus_state_value > FUS_STATE_VALUE_FUS_UPGRD_ONGOING_END) )
        {
          APP_DBG("fus_state_value == %d call NVIC_SystemReset()", fus_state_value);
          HAL_Delay(100);   /* Wait 100us to output traces */
          NVIC_SystemReset(); /* it waits until reset */
        }
      }
      break;

    case CFG_SW3_FUS_CPU2_SWITCH:
      APP_DBG("case CFG_SW3_FUS_CPU2_SWITCH");
      CFG_REBOOT_VAL_MSG = CFG_WAITING_BUTTON_REQ;
      /**
       * Request CPU2 to reboot on wireless firmware
       */
      SHCI_C2_FUS_StartWs();
      /**
       * Wait for a system reboot
       */
      while(1);
      break;

    default:
      APP_DBG("default case");
      CFG_REBOOT_VAL_MSG = CFG_WAITING_BUTTON_REQ;
#if ( CFG_LED_SUPPORTED != 0)
      BSP_LED_On(LED_RED);
#endif
      while(1);
      break;
    }
  }
}

/**
  * Erase all flash sectors between the end of the app and the secure part (SFSA)
  * The end of the app is defined in the scatter file, see __ICFEDIT_region_ROM_end__ for IAR and _endapp for STM32CubeIDE
  * The limit can be read from the SFSA option byte which provides the first secured sector address.
  */
static void Delete_Sectors(void)
{
  uint32_t first_secure_sector_idx, nbr_of_sector_to_be_erased, end_app_address, end_app_sector_idx,page_error;
  HAL_StatusTypeDef status;
  FLASH_EraseInitTypeDef p_erase_init;

  /* Read SFSA option byte */ 
  first_secure_sector_idx = (READ_BIT(FLASH->SFR, FLASH_SFR_SFSA) >> FLASH_SFR_SFSA_Pos);
  
  /* Recover the end of application address from linker file*/ 
  end_app_address = (uint32_t) (&__END_OF_APPLICATION_ADDRESS__);
  
  /* End of application sector index */ 
  end_app_sector_idx = (end_app_address - FLASH_BASE)/FLASH_SECTOR_SIZE;
  
  /* Number of sector to be erased */ 
  nbr_of_sector_to_be_erased = first_secure_sector_idx - end_app_sector_idx;
  
  p_erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  p_erase_init.NbPages = nbr_of_sector_to_be_erased;
  p_erase_init.Page = end_app_sector_idx;

  /* Erase sectors */ 
  HAL_FLASH_Unlock();
  status = HAL_FLASHEx_Erase(&p_erase_init, &page_error);
  HAL_FLASH_Lock();

  if(status != HAL_OK)
  {
#if ( CFG_LED_SUPPORTED != 0)
    BSP_LED_On(LED_RED);
#endif
    while(1);
  }
 
#if ( CFG_LED_SUPPORTED != 0)
  BSP_LED_Off(LED_BLUE);
#endif  
  
  return;
}

static void APP_Entry_Key_Button1_Action(void)
{
  CFG_REBOOT_VAL_MSG = CFG_SW1_FUS_FW_DELETE;
}

static void APP_Entry_Key_Button2_Action(void)
{
  CFG_REBOOT_VAL_MSG = CFG_SW2_FUS_FW_UPGRADE;
}

static void APP_Entry_Key_Button3_Action(void)
{
  uint32_t t0 = 0,t1 = 0;

  t0 = HAL_GetTick(); /* SW3 press timing */
  
  while(BSP_PB_GetState(BUTTON_SW3) == GPIO_PIN_RESET)
  {
    t1 = HAL_GetTick();
    if((t1 - t0) > LONG_PRESS_THRESHOLD)
    {
#if ( CFG_LED_SUPPORTED != 0)
      BSP_LED_On(LED_BLUE);
#endif
    }
  }
 
  t1 = HAL_GetTick(); /* SW3 release timing */
  
  if((t1 - t0) > LONG_PRESS_THRESHOLD)
  {
    /* Button 3 long press action */
    Delete_Sectors();
  }
  else 
  {
    /* Button 3 short press action */
    CFG_REBOOT_VAL_MSG = CFG_SW3_FUS_CPU2_SWITCH;
  }
}


/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */
static void Led_Init( void )
{
#if (CFG_LED_SUPPORTED == 1)
  /**
   * Leds Initialization
   */

  BSP_LED_Init(LED_BLUE);
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);

#endif

  return;
}

static void Button_Init( void )
{
#if (CFG_BUTTON_SUPPORTED == 1)
  /**
   * Button Initialization
   */

  BSP_PB_Init(BUTTON_SW1, BUTTON_MODE_EXTI);
  BSP_PB_Init(BUTTON_SW2, BUTTON_MODE_EXTI);
  BSP_PB_Init(BUTTON_SW3, BUTTON_MODE_EXTI);
#endif

  return;
}
/* USER CODE END FD_LOCAL_FUNCTIONS */

/*************************************************************
 *
 * WRAP FUNCTIONS
 *
 *************************************************************/
void HAL_Delay(uint32_t Delay)
{
  uint32_t tickstart = HAL_GetTick();
  uint32_t wait = Delay;

  /* Add a freq to guarantee minimum wait */
  if (wait < HAL_MAX_DELAY)
  {
    wait += HAL_GetTickFreq();
  }

  while ((HAL_GetTick() - tickstart) < wait)
  {
    /************************************************************************************
     * ENTER SLEEP MODE
     ***********************************************************************************/
    LL_LPM_EnableSleep(); /**< Clear SLEEPDEEP bit of Cortex System Control Register */

    /**
     * This option is used to ensure that store operations are completed
     */
  #if defined (__CC_ARM)
    __force_stores();
  #endif /* __CC_ARM */

    __WFI();
  }
}

void MX_APPE_Process(void)
{
  /* USER CODE BEGIN MX_APPE_Process_1 */

  /* USER CODE END MX_APPE_Process_1 */
  UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);
  /* USER CODE BEGIN MX_APPE_Process_2 */

  /* USER CODE END MX_APPE_Process_2 */
}

void UTIL_SEQ_Idle(void)
{
#if (CFG_LPM_SUPPORTED == 1)
  UTIL_LPM_EnterLowPower();
#endif /* CFG_LPM_SUPPORTED == 1 */
  return;
}

/**
  * @brief  This function is called by the scheduler each time an event
  *         is pending.
  *
  * @param  evt_waited_bm : Event pending.
  * @retval None
  */
void UTIL_SEQ_EvtIdle(UTIL_SEQ_bm_t task_id_bm, UTIL_SEQ_bm_t evt_waited_bm)
{
  UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);

  return;
}

void shci_notify_asynch_evt(void* pdata)
{
  UTIL_SEQ_SetTask(1<<CFG_TASK_SYSTEM_HCI_ASYNCH_EVT_ID, CFG_SCH_PRIO_0);
  return;
}

void shci_cmd_resp_release(uint32_t flag)
{
  UTIL_SEQ_SetEvt(1<< CFG_IDLEEVT_SYSTEM_HCI_CMD_EVT_RSP_ID);
  return;
}

void shci_cmd_resp_wait(uint32_t timeout)
{
  UTIL_SEQ_WaitEvt(1<< CFG_IDLEEVT_SYSTEM_HCI_CMD_EVT_RSP_ID);
  return;
}

/* USER CODE BEGIN FD_WRAP_FUNCTIONS */
void HAL_GPIO_EXTI_Callback( uint16_t GPIO_Pin )
{
  switch (GPIO_Pin)
  {
    case BUTTON_SW1_PIN:
      APP_Entry_Key_Button1_Action();
      break; 

    case BUTTON_SW2_PIN:
      APP_Entry_Key_Button2_Action();
      break; 

    case BUTTON_SW3_PIN:
      APP_Entry_Key_Button3_Action();
      break;

    default:
      break;
  }

  APP_DBG("NVIC_SystemReset");
  NVIC_SystemReset(); /* it waits until reset */
}
/* USER CODE END FD_WRAP_FUNCTIONS */
