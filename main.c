#include "ldc1614.h"
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include<unistd.h>
#include <fcntl.h>



int devID = 0x3055; // Device ID for LDC1614
int i2c_fd = 0; // File descriptor for LDC1614 I2C bus
int channel = 0; // Default channel to use
uint32_t value = 0; // Variable to hold measurement value
uint16_t status = 0; // Variable to hold status register value
int ret = 0; // Return value for function calls

int main(int argc, char *argv[]) {

    int opt = 0; // option for command line argument parsing
    char logfile[50] = "ldc1614_log.txt"; // default logfile name
    int log_fd = -1; // File descriptor for log file
    int num_samples = 100; // default number of samples to read

    while ((opt = getopt(argc, argv, "n:l:")) != -1) {
        switch(opt) {
            case 'l':
                // printf("Log file: %s\n", optarg);
                strncpy(logfile, optarg, sizeof(logfile) - 1); // Set logfile name
                logfile[sizeof(logfile) - 1] = '\0'; // Ensure null termination
                printf("Log file set to: %s\n", logfile);
                break;
            case 'n':
                num_samples = atoi(optarg); // Set number of samples to read
                printf("Number of samples: %d\n", num_samples);
                if (num_samples <= 0 || num_samples > 1000) { // Validate number of samples
                    fprintf(stderr, "Number of samples must be greater than 0 and less than 1000.\n");
                    return -1; // Exit if invalid number of samples
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-l logfile] [-n num_samples]\n", argv[0]);
                return -1; // Exit on invalid option
        }
    }

    // Initialize wiringPi library and get the file descriptor for I2C communication
    i2c_fd = wiringPiI2CSetup(LDC1614_ADDR);
    uint16_t ID = 0;

    if (i2c_fd == -1) {
        fprintf(stderr, "Failed to initialize I2C communication: %s\n", strerror(errno));
        return -1;
    }
    printf("I2C device initialized.\n");

    // Initialize the LDC1614 for the specified channel 
    if (ldc1614_init(i2c_fd, channel) != 0) {
        fprintf(stderr, "Failed to initialize LDC1614 on channel %d: %s\n", channel, strerror(errno));
        return -1;
    }
    printf("LDC1614 initialized on channel %d.\n", channel);

    // Read the device ID to confirm communication
    ret = ldc1614_read_reg(i2c_fd, LDC1614_DEVICE_ID, &ID);
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

    // Open the log file for writing only, create it if it doesn't exist, and truncate it to 0 if it does
    log_fd = open(logfile, O_WRONLY | O_CREAT | O_TRUNC, 0666); // 
    if (log_fd == -1 ) {
        fprintf(stderr, "Failed to open log file %s: %s\n", logfile, strerror(errno));
        return -1; // Exit if log file cannot be opened
    }
    char log_header[] = "Channel, Value\n"; // Header for log file
    if (write(log_fd, log_header, sizeof(log_header) - 1) == -1) {
        fprintf(stderr, "Failed to write header to log file: %s\n", strerror(errno));
        close(log_fd);
        return -1; // Exit if writing header fails
    }

    for(int i=0; i<num_samples; i++) {
        status = 0;
        while(status == 0) {
            ldc1614_read_reg(i2c_fd, LDC1614_STATUS, &status);
            status = status & 0x08; // Check if data is ready
        }

        ret = ldc1614_read_ch0(i2c_fd, &value);
        if (ret == -1) {
            fprintf(stderr, "Failed to read channel 0 value: %s\n", strerror(errno));
            return -1;
        } else {
            printf("Channel 0 value: %i\n", value);
        }
    }

    close(log_fd); // Close the log file after writing all samples
    printf("Data collection complete. Log file closed.\n");
    return 0; // Exit successfully

}
