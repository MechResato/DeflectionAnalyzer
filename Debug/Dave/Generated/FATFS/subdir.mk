################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Dave/Generated/FATFS/fatfs.c \
../Dave/Generated/FATFS/fatfs_conf.c \
../Dave/Generated/FATFS/sltha.c 

OBJS += \
./Dave/Generated/FATFS/fatfs.o \
./Dave/Generated/FATFS/fatfs_conf.o \
./Dave/Generated/FATFS/sltha.o 

C_DEPS += \
./Dave/Generated/FATFS/fatfs.d \
./Dave/Generated/FATFS/fatfs_conf.d \
./Dave/Generated/FATFS/sltha.d 


# Each subdirectory must supply rules for building sources it contributes
Dave/Generated/FATFS/%.o: ../Dave/Generated/FATFS/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM-GCC C Compiler'
	"$(TOOLCHAIN_ROOT)/bin/arm-none-eabi-gcc" -MMD -MT "$@" -DXMC4700_F144x2048 -I"$(PROJECT_LOC)/Libraries/XMCLib/inc" -I"$(PROJECT_LOC)/Libraries/CMSIS/Include" -I"$(PROJECT_LOC)/Libraries/CMSIS/Infineon/XMC4700_series/Include" -I"$(PROJECT_LOC)" -I"$(PROJECT_LOC)/Dave/Generated" -I"$(PROJECT_LOC)/Libraries" -O0 -ffunction-sections -fdata-sections -Wall -std=gnu99 -mfloat-abi=softfp -Wa,-adhlns="$@.lst" -pipe -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $@" -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -g -gdwarf-2 -o "$@" "$<" 
	@echo 'Finished building: $<'
	@echo.

