#include "ldc1614.h"
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int devID = 0x3055; // Device ID for LDC1614
int fd = 0; // File descriptor for LDC1614 I2C bus
int channel = 0; // Default channel to use
uint32_t value = 0; // Variable to hold measurement value
int16_t status = 0; // Variable to hold status register value
int num_samples = 100; // Number of samples to read
int ret = 0; // Return value for function calls

int main(int argc, char *argv[]) {
    fd = wiringPiI2CSetup(LDC1614_ADDR);
    int16_t ID = 0;

    if (fd == -1) {
        fprintf(stderr, "Failed to initialize I2C communication: %s\n", strerror(errno));
        return -1;
    }
    printf("I2C device initialized.\n");

    // Initialize the LDC1614 for the specified channel 
    if (ldc1614_init(fd, channel) != 0) {
        fprintf(stderr, "Failed to initialize LDC1614 on channel %d: %s\n", channel, strerror(errno));
        return -1;
    }
    printf("LDC1614 initialized on channel %d.\n", channel);

    // Read the device ID to verify endianness of communication
    ret = ldc1614_read_reg(fd, LDC1614_DEVICE_ID, &ID);
    if (ret == -1) {
        fprintf(stderr, "Failed to read device ID: %s\n", strerror(errno));
        return -1;
    } else if(ID != devID) {
        fprintf(stderr, "Unexpected Device ID: 0x%04X, expected: 0x%04X\n", ID, devID);
        return -1; // Error if device ID does not match 
    }
        else {
        printf("LDC1614 Device ID: 0x%04X verified\n", ID);
    }


    for(int i=0; i<num_samples; i++) {
        status = 0;
        while(status == 0) {
            ldc1614_read_reg(fd, LDC1614_STATUS, &status);
            status = status & 0x08; // Check if data is ready
        }

        ret = ldc1614_read_ch0(fd, &value);
        if (ret == -1) {
            fprintf(stderr, "Failed to read channel 0 value: %s\n", strerror(errno));
            return -1;
        } else {
            printf("Channel 0 value: %i\n", value);
        }
    }

    return 0; // Exit successfully

}
