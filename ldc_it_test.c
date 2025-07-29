
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include "ldc1614.h"

#define INTB 0// interrupt pin number (BCM GPIO 17 is wiringpi 0)

int data_ready = 0;

/**
 * @brief Calculate the elapsed time between two timespec structures.
 * @param start The starting time.
 * @param end The ending time.
 * @return A timespec structure representing the elapsed time.
 */
struct timespec get_elapsed_time(struct timespec start, struct timespec end) {
    struct timespec elapsed;
    if ((end.tv_nsec - start.tv_nsec) < 0) { // account for nanosecond overflow
        elapsed.tv_sec = end.tv_sec - start.tv_sec - 1;
        elapsed.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
    else {
        elapsed.tv_sec = end.tv_sec - start.tv_sec;
        elapsed.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return elapsed;
}

/**
 * @brief Interrupt Service Routine (ISR) for handling data ready signal from LDC1614.
 * This function is called when the INTB pin goes low, indicating that data is ready to be read.
 */
void LDC_ISR(void) {
    data_ready = 1; // Set the data ready flag to indicate that data is available
}

int main(int argc, char *argv[]) {

    // private variables 
    int opt = 0; // option for command line argument parsing
    int devID = 0x3055; // Device ID for LDC1614
    int i2c_fd = 0; // File descriptor for LDC1614 I2C bus
    int channel = 0; // Default channel to use
    uint32_t value = 0; // Variable to hold measurement value
    uint16_t status = 0; // Variable to hold status register value
    int ret = 0; // Return value for function calls
    char logfile[50] = "./testing/ldc1614_log.csv"; // default logfile name
    int log_fd = -1; // File descriptor for log file
    int num_samples = 10; // default number of samples to read
    int num_steps = 1; // Number of steps for command value increment
    struct timespec start_time; // t0
    struct timespec current_time; // t 
    struct timespec elapsed_time; // Timestamp for datalogging (t - t0)


    // Initialize the timer and logger 
    clock_gettime(CLOCK_MONOTONIC, &start_time); // Start time measurement

     // Parse command line arguments for logfile, and number of samples
    while ((opt = getopt(argc, argv, "hn:l:v:s:")) != -1) {
        switch(opt) {
            case 'l':
                strncpy(logfile, optarg, sizeof(logfile) - 1); // Set logfile name
                logfile[sizeof(logfile) - 1] = '\0'; // Ensure null termination
                break;
            case 'n':
                num_samples = atoi(optarg); // Set number of samples to read
                printf("Number of samples: %d\n", num_samples);
                if (num_samples <= 0 || num_samples > 1000) { // Validate number of samples
                    printf("Number of samples must be greater than 0 and less than 1000.\n");
                    return -1; // Exit if invalid number of samples
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-l logfile] [-n num_samples] [-v command] [-s number of steps]\n", argv[0]);
                return -1; // Exit on invalid option
        }
    }

    // initialize wiringPi library
    wiringPiSetupGpio();
    pinMode (INTB, INPUT);
    pullUpDnControl(INTB,PUD_UP); // 
    // Set up interrupt on the INTB pin
    wiringPiISR(INTB, INT_EDGE_BOTH, &LDC_ISR);

    // Initialize wiringPi library and get the file descriptor for I2C communication
    i2c_fd = wiringPiI2CSetup(LDC1614_ADDR);

    // Initialize the LDC1614 for the specified channel 
    if (ldc1614_init(i2c_fd, channel) != 0) {
        printf("Failed to initialize LDC1614 on channel %d: %s\n", channel, strerror(errno));
        return -1;
    } else {
        printf("LDC1614 initialized successfully on channel %d.\n", channel);
    }

    uint16_t ID = 0;
    // Read the device ID to confirm communication
    ret = ldc1614_read_reg(i2c_fd, LDC1614_DEVICE_ID, &ID);
    if (ret == -1) {
        printf("Failed to read device ID: %s\n", strerror(errno));
        return -1;
    } else if(ID != devID) {
        printf("Unexpected Device ID: 0x%04X, expected: 0x%04X\n", ID, devID);
        return -1; // Error if device ID does not match 
    }
        else {
        printf("LDC1614 Device ID: 0x%04X verified\n",ID);
    }

    // Open the log file for writing only, create it if non-existent, and overwrite it if it exists
    log_fd = open(logfile, O_WRONLY | O_CREAT | O_TRUNC, 0666); // 
    if (log_fd == -1 ) {
        fprintf(stderr, "Failed to open log file %s: %s\n", logfile, strerror(errno));
        return -1; // Exit if log file cannot be opened
    }
    char log_header[] = "Channel, Timestamp, Value\n"; // Header for log file
    if (write(log_fd, log_header, sizeof(log_header) - 1) == -1) {
        fprintf(stderr, "Failed to write header to log file: %s\n", strerror(errno));
        close(log_fd);
        return -1; // Exit if writing header fails
    }
    for(int i=0; i < num_samples;) {
        
        if(data_ready == 1){
            i++; // Increment sample count
            data_ready = 0; // Reset data ready flag
            // Read the value from channel 0
            if(ret = ldc1614_read_ch0(i2c_fd, &value)==-1){
                printf("Failed to read value: %s\n", strerror(errno));
                // return -1;
            } else {
                clock_gettime(CLOCK_MONOTONIC, &current_time); // Get current time for timestamp
                elapsed_time = get_elapsed_time(start_time, current_time); // Calculate elapsed time
                char data_line[80]; 
                int line_length = 0; 
                line_length =  sprintf( data_line, "%d, %ld.%09ld, %d\n", channel,elapsed_time.tv_sec,elapsed_time.tv_nsec, value); // format data into a string
                if (write(log_fd, data_line, line_length) == -1) {
                    fprintf(stderr, "Failed to write data to log file: %s\n", strerror(errno));
                    close(log_fd);
                    return -1; // Exit with error if data write fails
                }
            }
        }

    }

}