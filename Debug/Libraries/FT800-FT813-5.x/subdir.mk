################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Libraries/FT800-FT813-5.x/EVE_commands.c \
../Libraries/FT800-FT813-5.x/EVE_target.c \
../Libraries/FT800-FT813-5.x/tft.c \
../Libraries/FT800-FT813-5.x/tft_data.c 

OBJS += \
./Libraries/FT800-FT813-5.x/EVE_commands.o \
./Libraries/FT800-FT813-5.x/EVE_target.o \
./Libraries/FT800-FT813-5.x/tft.o \
./Libraries/FT800-FT813-5.x/tft_data.o 

C_DEPS += \
./Libraries/FT800-FT813-5.x/EVE_commands.d \
./Libraries/FT800-FT813-5.x/EVE_target.d \
./Libraries/FT800-FT813-5.x/tft.d \
./Libraries/FT800-FT813-5.x/tft_data.d 


# Each subdirectory must supply rules for building sources it contributes
Libraries/FT800-FT813-5.x/%.o: ../Libraries/FT800-FT813-5.x/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM-GCC C Compiler'
	"$(TOOLCHAIN_ROOT)/bin/arm-none-eabi-gcc" -MMD -MT "$@" -DXMC4700_F144x2048 -I"$(PROJECT_LOC)/Libraries/XMCLib/inc" -I"$(PROJECT_LOC)/Libraries/CMSIS/Include" -I"$(PROJECT_LOC)/Libraries/CMSIS/Infineon/XMC4700_series/Include" -I"$(PROJECT_LOC)" -I"$(PROJECT_LOC)/Dave/Generated" -I"$(PROJECT_LOC)/Libraries" -O0 -ffunction-sections -fdata-sections -Wall -std=gnu99 -mfloat-abi=softfp -Wa,-adhlns="$@.lst" -pipe -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $@" -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -g -gdwarf-2 -o "$@" "$<" 
	@echo 'Finished building: $<'
	@echo.

