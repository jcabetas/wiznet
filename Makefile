##############################################################################
# Build global options
# NOTE: Can be overridden externally.
#


$(shell echo "#include \"version.h\"\n\nchar const *const GIT_COMMIT = \"$$(git describe --always --dirty --match 'NOT A TAG')\";\nchar const *const GIT_TAG = \"$$(git tag)\";" > version.cpp.tmp; if diff -q version.cpp.tmp version.cpp >/dev/null 2>&1; then rm version.cpp.tmp; else mv version.cpp.tmp version.cpp; fi)


# Compiler options here.
ifeq ($(USE_OPT),)
  USE_OPT = -O0 -ggdb -fomit-frame-pointer -falign-functions=16 -lm
endif

# C specific options here (added to USE_OPT).
ifeq ($(USE_COPT),)
  USE_COPT = 
endif

# C++ specific options here (added to USE_OPT).
ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -std=gnu++11 -fno-exceptions -fno-rtti
endif

# Enable this if you want the linker to remove unused code and data.
ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

# Linker extra options here.
ifeq ($(USE_LDOPT),)
  USE_LDOPT = 
endif

# Enable this if you want link time optimizations (LTO).
ifeq ($(USE_LTO),)
  USE_LTO = yes
endif

# Enable this if you want to see the full log while compiling.
ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

# If enabled, this option makes the build process faster by not compiling
# modules not used in the current configuration.
ifeq ($(USE_SMART_BUILD),)
  USE_SMART_BUILD = yes
endif

#
# Build global options
##############################################################################

##############################################################################
# Architecture or project specific options
#

# Stack size to be allocated to the Cortex-M process stack. This stack is
# the stack used by the main() thread.
ifeq ($(USE_PROCESS_STACKSIZE),)
  USE_PROCESS_STACKSIZE = 0x800
endif

# Stack size to the allocated to the Cortex-M main/exceptions stack. This
# stack is used for processing interrupts and exceptions.
ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  USE_EXCEPTIONS_STACKSIZE = 0x400
endif

# Enables the use of FPU (no, softfp, hard).
ifeq ($(USE_FPU),)
  USE_FPU = hard
endif

# FPU-related options.
ifeq ($(USE_FPU_OPT),)
  USE_FPU_OPT = -mfloat-abi=$(USE_FPU) -mfpu=fpv4-sp-d16
endif

#
# Architecture or project specific options
##############################################################################

##############################################################################
# Project, target, sources and paths
#

# Define project name here
PROJECT = miniPozo

# Target settings.
MCU  = cortex-m4

# Imported source files and paths.
ifeq ($(OS),Windows_NT)
  CHIBIOS = C:\ChibiStudio\ChibiOS203
else
  CHIBIOS  := /home/joaquin/ChibiStudio/chibios_stable-21.11.x
endif

CONFDIR  := ./cfg
BUILDDIR := ./build
DEPDIR   := ./.dep

# Licensing files.
include $(CHIBIOS)/os/license/license.mk
# Startup files.
include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f4xx.mk
# HAL-OSAL files (optional).
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F4xx/platform.mk
include WEACT_F411RE/board.mk
include $(CHIBIOS)/os/hal/osal/rt-nil/osal.mk
# RTOS files (optional).
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk
#include $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC/mk/port_v7m.mk
# Auto-build files in ./source recursively.
include $(CHIBIOS)/tools/mk/autobuild.mk
# Other files (optional).
include $(CHIBIOS)/os/hal/lib/streams/streams.mk
include $(CHIBIOS)/os/various/cpp_wrappers/chcpp.mk

# Define linker script file here
LDSCRIPT= $(STARTUPLD)/STM32F411xE.ld

# C sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CSRC = $(ALLCSRC) \
       $(CHIBIOS)/os/various/evtimer.c \
       main.c 
       #  MPU6050/i2cdev_chibi.c 
       # $(CHIBIOS)MPU6050/i2cdev_chibi.c $(CHIBIOS)MPU6050/mpu6050.c \
       #       $(CHIBIOS)/os/various/syscalls.c \
       #       $(CONFDIR)/portab.c usbcfg.c \

# C++ sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CPPSRC = $(ALLCPPSRC) \
         w25q16/w25q16.cpp variables/varsGestion.cpp variables/volcarFlash.cpp variables/variables.cpp version.cpp \
         calendar/calendar.cpp calendar/rtcV2.cpp \
         serial.cpp tty/gets.cpp heap.cpp lcd/lcd.cpp lcd/threadDisplay.cpp \
         rf95/rf95.cpp rf95/RH_RF95.cpp \
         radio/pozoComun.cpp radio/tratamientoMsgRF95.cpp radio/llamador.cpp  radio/radio.cpp\
         colas/colas.cpp colas/colasMensajesRx.cpp colas/colasMensajesTx.cpp colas/colasMensajesLcd.cpp
         

#         usbSource/serialUSB.cpp w25q16/varsFlash.cpp

# List ASM source files here.
ASMSRC = $(ALLASMSRC)

# List ASM with preprocessor source files here.
ASMXSRC = $(ALLXASMSRC)

# Inclusion directories.
INCDIR = $(CONFDIR) $(ALLINC) 

# Define C warning options here.
CWARN = -Wall -Wextra -Wundef -Wstrict-prototypes

# Define C++ warning options here.
CPPWARN = -Wall -Wextra -Wundef

#
# Project, target, sources and paths
##############################################################################

##############################################################################
# Start of user section
#

# List all user C define here, like -D_DEBUG=1
UDEFS = -DCHPRINTF_USE_FLOAT=TRUE

# Define ASM defines here
UADEFS =

# List all user directories here
UINCDIR = $(CHIBIOS)/os/hal/lib/streams usbSource cfg w25q16 variables tty calendar lcd colas ssd1306 rf95 pozo

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
ULIBS =

#
# End of user section
##############################################################################

##############################################################################
# Common rules
#

RULESPATH = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk
include $(RULESPATH)/arm-none-eabi.mk
include $(RULESPATH)/rules.mk

#
# Common rules
##############################################################################

##############################################################################
# Custom rules
#

#
# Custom rules
##############################################################################
