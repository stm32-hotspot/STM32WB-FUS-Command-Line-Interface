/**
  @page FUS_CLI Application

  @verbatim
  ******************************************************************************
  * @file    BLE/FUS_CLI/readme.txt 
  * @author  MCD Application Team
  * @brief   Description of the FUS_CLI application
  ******************************************************************************
  *
  * Copyright (c) 2019-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  @endverbatim

@par Example Description

This application shows how to interact with FUS using FUS commands in order to perform a wireless stack or FUS upgrade.

@note Care must be taken when using HAL_Delay(), this function provides accurate delay (in milliseconds)
      based on variable incremented in SysTick ISR. This implies that if HAL_Delay() is called from
      a peripheral ISR process, then the SysTick interrupt must have higher priority (numerically lower)
      than the peripheral interrupt. Otherwise the caller ISR process will be blocked.
      To change the SysTick interrupt priority you have to use HAL_NVIC_SetPriority() function.
      
@note The application needs to ensure that the SysTick time base is always set to 1 millisecond
      to have correct HAL operation.

@par Keywords

FUS, CLI, Wireless FW, UART, IPCC

@par Directory contents 

  - BLE/FUS_CLI/Core/Inc/app_common.h            	Header for all modules with common definition
  - BLE/FUS_CLI/Core/Inc/app_conf.h              	Parameters configuration file of the application
  - BLE/FUS_CLI/Core/Inc/app_debug.h              	Header for app_debug.c module
  - BLE/FUS_CLI/Core/Inc/app_entry.h            	Parameters configuration file of the application
  - BLE/FUS_CLI/Core/Inc/hw_conf.h           		Configuration file of the HW
  - BLE/FUS_CLI/Core/Inc/hw_if.h                     	HW interface
  - BLE/FUS_CLI/Core/Inc/main.h                  	Header for main.c module
  - BLE/FUS_CLI/Core/Inc/stm_logging.h                  Header for stm_logging.c module
  - BLE/FUS_CLI/Core/Inc/stm32_lpm_if.h              	Header for stm32_lpm_if.c module (LP management)
  - BLE/FUS_CLI/Core/Inc/stm32wbxx_hal_conf.h        	HAL configuration file
  - BLE/FUS_CLI/Core/Inc/stm32wbxx_it.h              	Interrupt handlers header file
  - BLE/FUS_CLI/Core/Inc/utilities_conf.h            	Configuration file of the utilities
  - BLE/FUS_CLI/STM32_WPAN/App/tl_dbg_conf.h         	Debug configuration file for stm32wpan transport layer interface 
  - BLE/FUS_CLI/Core/Src/app_debug.c                 	Debug capabilities source file for STM32WPAN Middleware
  - BLE/FUS_CLI/Core/Src/app_entry.c                 	Initialization of the application
  - BLE/FUS_CLI/Core/Src/hw_timerserver.c            	Timer Server based on RTC 
  - BLE/FUS_CLI/Core/Inc/hw_uart.c                   	HW uart functions 
  - BLE/FUS_CLI/Core/Src/main.c                      	Main program
  - BLE/FUS_CLI/Core/Src/stm32_lpm_if.c              	Low Power Manager Interface
  - BLE/FUS_CLI/Core/Src/stm32wbxx_hal_msp.c         	MSP Initialization and de-Initialization code
  - BLE/FUS_CLI/Core/Src/stm32wbxx_it.c              	Interrupt handlers
  - BLE/FUS_CLI/Core/Src/system_stm32wbxx.c          	stm32wbxx system source file
  - BLE/FUS_CLI/STM32_WPAN/Target/hw_ipcc.c          	IPCC Driver
  
@par Hardware and Software environment

  - This example runs on STM32WB55xx devices.
  
  - This example has been tested with an STMicroelectronics STM32WB55VG-Nucleo
    board and can be easily tailored to any other supported device 
    and development board.

@par How to use it ? 

This application is compatible with all wireless coprocessor binary.
All available binaries are located under /Projects/STM32_Copro_Wireless_Binaries directory.

In order to make the program work, you must do the following:
 - Open your toolchain 
 - Rebuild all files and flash the board with the executable file
 - Or use the FUS_CLI_reference.bin from Binary directory
 - Run the application

On the Nucleo board side:
 - Power on the Nucleo board with the FUS_CLI application

On the PC side:
 - Open a terminal window with the following settings:
   baud rate of 115200
   Byte size of 8
   Parity None
   Stop bits 1
   Data Flow Control None
   
Once the board is flashed, the application displays wireless FW versions and the possible actions on hyperterminal like this:
 - Press SW1 to delete the installed Wireless Stack
 - Press SW2 to install the CPU2 update (either FUS or Wireless Stack)
 - Short press SW3 to switch between FUS and Wireless Stack
 - Long press SW3 to delete all sectors between 0x08008000 and SFSA
 
There is also LED indications:
 - When the wireless stack is running, the green led flashes slowly
 - When the FUS is running, the green led flashes quickly
 - When an upgrade is on going, deleting the wireless stack or deleting the user flash memory, the blue led is on 
 - When there is an error, the red led is on 

In order to perform a FUS or a Wireless Stack Firmware update:
 - Download the wireless stack or the FUS image from www.st.com or from the STM32CubeMX repository
 - Long press on SW3 until blue LED is ON to delete all flash sectors between 0x08008000 and SFSA
 - Wait few seconds until blue LED is OFF
 - Write the FW image in the user Flash memory at an address between 0x08008000 (end of application address) and the secure part (see SFSA option byte) minus the image size
   using erase and programming tab of STM32CubeProgrammer. Image start address must be aligned to sector start address (this is a multiple of 4-kbytes).
 - Reset the board thanks to reset button SW4   
 - Press SW2 to install the CPU2 update
   - For wireless stack update:
      - If there is no wireless stack installed, the new FW will be installed at the download address.
      - If there is already a wireless stack, the new FW will be installed at the optimal address
 - Wait few seconds until blue LED is OFF, firwmare is updated

For more details refer to the Application Note: 
  AN5185 - ST firmware upgrade services for STM32WB Series

Available Wiki pages:
  - https://wiki.st.com/stm32mcu/wiki/Connectivity:BLE_overview
  - https://wiki.st.com/stm32mcu/wiki/Connectivity:STM32WB_FUS
  - https://wiki.st.com/stm32mcu/wiki/Connectivity:STM32WB_BLE_Wireless_Stack
  
 * <h3><center>&copy; COPYRIGHT STMicroelectronics</center></h3>
 */

