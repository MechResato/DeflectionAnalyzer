################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Dave/Generated/SDMMC_BLOCK/sdmmc_block.c \
../Dave/Generated/SDMMC_BLOCK/sdmmc_block_conf.c \
../Dave/Generated/SDMMC_BLOCK/sdmmc_block_private_sd.c 

OBJS += \
./Dave/Generated/SDMMC_BLOCK/sdmmc_block.o \
./Dave/Generated/SDMMC_BLOCK/sdmmc_block_conf.o \
./Dave/Generated/SDMMC_BLOCK/sdmmc_block_private_sd.o 

C_DEPS += \
./Dave/Generated/SDMMC_BLOCK/sdmmc_block.d \
./Dave/Generated/SDMMC_BLOCK/sdmmc_block_conf.d \
./Dave/Generated/SDMMC_BLOCK/sdmmc_block_private_sd.d 


# Each subdirectory must supply rules for building sources it contributes
Dave/Generated/SDMMC_BLOCK/%.o: ../Dave/Generated/SDMMC_BLOCK/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM-GCC C Compiler'
	"$(TOOLCHAIN_ROOT)/bin/arm-none-eabi-gcc" -MMD -MT "$@" -DXMC4700_F144x2048 -I"$(PROJECT_LOC)/Libraries/XMCLib/inc" -I"$(PROJECT_LOC)/Libraries/CMSIS/Include" -I"$(PROJECT_LOC)/Libraries/CMSIS/Infineon/XMC4700_series/Include" -I"$(PROJECT_LOC)" -I"$(PROJECT_LOC)/Dave/Generated" -I"$(PROJECT_LOC)/Libraries" -O0 -ffunction-sections -fdata-sections -Wall -std=gnu99 -mfloat-abi=softfp -Wa,-adhlns="$@.lst" -pipe -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $@" -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -g -gdwarf-2 -o "$@" "$<" 
	@echo 'Finished building: $<'
	@echo.

