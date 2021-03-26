################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Dave/Generated/GLOBAL_ADC/global_adc.c \
../Dave/Generated/GLOBAL_ADC/global_adc_conf.c 

OBJS += \
./Dave/Generated/GLOBAL_ADC/global_adc.o \
./Dave/Generated/GLOBAL_ADC/global_adc_conf.o 

C_DEPS += \
./Dave/Generated/GLOBAL_ADC/global_adc.d \
./Dave/Generated/GLOBAL_ADC/global_adc_conf.d 


# Each subdirectory must supply rules for building sources it contributes
Dave/Generated/GLOBAL_ADC/%.o: ../Dave/Generated/GLOBAL_ADC/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM-GCC C Compiler'
	"$(TOOLCHAIN_ROOT)/bin/arm-none-eabi-gcc" -MMD -MT "$@" -DXMC4700_F144x2048 -I"$(PROJECT_LOC)/Libraries/XMCLib/inc" -I"$(PROJECT_LOC)/Libraries/CMSIS/Include" -I"$(PROJECT_LOC)/Libraries/CMSIS/Infineon/XMC4700_series/Include" -I"$(PROJECT_LOC)" -I"$(PROJECT_LOC)/Dave/Generated" -I"$(PROJECT_LOC)/Libraries" -Os -ffunction-sections -fdata-sections -Wall -std=gnu99 -mfloat-abi=softfp -Wa,-adhlns="$@.lst" -pipe -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $@" -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -o "$@" "$<" 
	@echo 'Finished building: $<'
	@echo.

