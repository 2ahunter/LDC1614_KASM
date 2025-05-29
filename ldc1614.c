// Source file for ldc1614 driver.
#include "ldc1614.h"
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

uint16_t byteswap(uint16_t value){
    // reverse byte order for 16 bit value
    uint16_t lsb = value >> 8; // Get LSB
    uint64_t msb = value << 8; // Get MSB
    return (msb | lsb); // Combine MSB and LSB
}

int ldc1614_init(int fd, int channel) {
    int result = 0;
    // Set up the LDC1614 with default values (see p 51 of the datasheet)
    result = ldc1614_write_reg(fd, LDC1614_RCOUNT0, 0xFFFF); // Max conversion interval
    if (result == -1) {
        fprintf(stderr, "Failed to write RCOUNT0: %s\n", strerror(errno));
        return -1; // Error
    }
    result = ldc1614_write_reg(fd, LDC1614_SETTLECOUNT0, 0x000A); // Settle time
    if (result == -1) {
        fprintf(stderr, "Failed to write SETTLECOUNT0: %s\n", strerror(errno));
        return -1; // Error
    }
    result = ldc1614_write_reg(fd, LDC1614_CLOCK_DIVIDERS0, 0x1002); // No clock division
    if (result == -1) {
        fprintf(stderr, "Failed to write CLOCK_DIVIDERS0: %s\n", strerror(errno));
        return -1; // Error
    }
    result = ldc1614_write_reg(fd, LDC1614_ERROR_CONFIG, 0x0000); // No error reporting
    if (result == -1) {
        fprintf(stderr, "Failed to write ERROR_CONFIG: %s\n", strerror(errno));
        return -1; // Error
    }
    // Enable Channel 0 in continuous mode, set Input deglitch bandwidth to 3.3MHz
    result = ldc1614_write_reg(fd, LDC1614_MUX_CONFIG, 0x020C); 
    if (result == -1) {
        fprintf(stderr, "Failed to write MUX_CONFIG: %s\n", strerror(errno));
        return -1; // Error
    }
    //Manually set sensor drive current on channel 0
	result = ldc1614_write_reg(fd, LDC1614_DRIVE_CURRENT0, 0x9000);
    if (result == -1) {
        fprintf(stderr, "Failed to write DRIVE_CURRENT0: %s\n", strerror(errno));
        return -1; // Error
    }
    // Select active channel = ch 0, disable auto-amplitude correction and autocalibration, 
    // enable full current drive during sensor activation, select
	// internal clock source, wake up device to start conversion. This register
	// write must occur last because device configuration is not permitted while
	// the LDC is in active mode.
	result = ldc1614_write_reg(fd, LDC1614_CONFIG, 0x1201);
    if (result == -1) {
        fprintf(stderr, "Failed to write CONFIG: %s\n", strerror(errno));
        return -1; // Error
    }

    return 0; // Success
}

int ldc1614_read_reg(int fd, uint8_t reg, uint16_t *value){
    int16_t data = 0;
    data = wiringPiI2CReadReg16(fd, reg);
    if (data == -1) {
        fprintf(stderr, "Failed to read register 0x%02X: %s\n", reg, strerror(errno));
        return -1; // Error
    } 
    *value = byteswap((uint16_t)data);
    return 0; // Success
}

int ldc1614_write_reg(int fd, uint8_t reg, uint16_t value) {
    value = byteswap(value); // Ensure value is in correct byte order
    int result = wiringPiI2CWriteReg16(fd, reg, value);
    if (result == -1) {
        fprintf(stderr, "Failed to write register 0x%02X: %s\n", reg, strerror(errno));
        return -1; // Error
    }
    return 0; // Success
}

int ldc1614_read_ch0(int fd, uint32_t * data){
    uint16_t msb = 0;
    int ret = 0;
    // Read the MSB and LSB of DATA0
    ret = ldc1614_read_reg(fd, LDC1614_DATA0_MSB, &msb);
    if (ret == -1) {
        fprintf(stderr, "Failed to read DATA0_MSB: %s\n", strerror(errno));
        return -1; // Error
    }
    // printf("DATA0_MSB: 0x%04X\n", msb);

    uint16_t lsb = 0;
    ret = ldc1614_read_reg(fd, LDC1614_DATA0_LSB, &lsb);
    if (ret == -1) {
        fprintf(stderr, "Failed to read DATA0_LSB: %s\n", strerror(errno));
        return -1; // Error
    }
    // printf("DATA0_LSB: 0x%04X\n", lsb);
    
    // Combine MSB and LSB into a single 32-bit value
    int16_t errors = (msb >> 12) & 0x0F; // Extract error bits from MSB
    if (errors != 0) {
        fprintf(stderr, "Error bits detected: 0x%02X\n", errors);
    }
    uint32_t sensor_value = ((msb & 0x0FFF) << 16) | lsb;
    *data = sensor_value; // Store the sensor reading in the provided data container
    
    return 0; // Return success
}