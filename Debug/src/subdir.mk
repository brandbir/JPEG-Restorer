################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/carver.c \
../src/decoding.c \
../src/filesystem.c \
../src/globals.c \
../src/helper.c \
../src/jpegtools.c \
../src/performance.c \
../src/picojpeg.c \
../src/runner.c 

OBJS += \
./src/carver.o \
./src/decoding.o \
./src/filesystem.o \
./src/globals.o \
./src/helper.o \
./src/jpegtools.o \
./src/performance.o \
./src/picojpeg.o \
./src/runner.o 

C_DEPS += \
./src/carver.d \
./src/decoding.d \
./src/filesystem.d \
./src/globals.d \
./src/helper.d \
./src/jpegtools.d \
./src/performance.d \
./src/picojpeg.d \
./src/runner.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc `pkg-config --cflags opencv --libs opencv` -g `pkg-config --cflags --libs glib-2.0` -I/usr/include/glib-2.0 -I/usr/include/opencv -I/usr/include/tsk3 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


