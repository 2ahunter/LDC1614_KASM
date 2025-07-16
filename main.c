#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <time.h>
#include "ldc1614.h"
#include "UDP_client.h"


char ip[]="127.0.0.0";
char port[] = "2345";


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
 * @brief send command values to actuater.
 * @param cmd_val
 * @return status: 0 on success, -1 on failure
 * @note This function sends command values to the actuater via UDP.
 * It prepares the command data and sends it using the UDP_send function.
 */
int send_command(int16_t cmd_val) {
    union CMD_DATA cmd_data, buf_data;

    // Prepare the command data
    for(int i = 0; i < CMD_SIZE/2; i++) {
        cmd_data.values[i] = cmd_val; // Set command value
        buf_data.values[i] = htons(cmd_val); // Convert to network byte order
    }

    // Send the command buffer values
    size_t bytes_sent = UDP_send(buf_data);
    if (bytes_sent <= 0) {
        fprintf(stderr, "Failed to send command data: %s\n", strerror(errno));
        return -1; // Return error if sending fails
    }
    printf("Sent %zu bytes\n", bytes_sent);
    return 0; // Return success

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
    int num_samples = 500; // default number of samples to read
    int num_steps = 1; // Number of steps for command value increment
    int16_t cmd_inc = 1000; // Increment value for command
    int16_t start_value = 100; // Starting value for command
    struct timespec start_time; // t0
    struct timespec current_time; // t 
    struct timespec elapsed_time; // Timestamp for datalogging (t - t0)
    int16_t cmd_val = 0;
    int16_t max_cmd = 24000; // Maximum command value

    // Initialize the timer and logger 
    clock_gettime(CLOCK_MONOTONIC, &start_time); // Start time measurement
    openlog(NULL, LOG_PERROR, LOG_LOCAL6); // Open syslog for logging
    syslog(LOG_INFO, "Starting LDC1614 data collection program.\n");

    // Parse command line arguments for logfile, and number of samples
    while ((opt = getopt(argc, argv, "hn:l:v:s:")) != -1) {
        switch(opt) {
            case 'l':
                strncpy(logfile, optarg, sizeof(logfile) - 1); // Set logfile name
                logfile[sizeof(logfile) - 1] = '\0'; // Ensure null termination
                syslog(LOG_INFO,"Datalog file set to: %s\n", logfile);
                break;
            case 'n':
                num_samples = atoi(optarg); // Set number of samples to read
                syslog(LOG_INFO, "Number of samples: %d\n", num_samples);
                if (num_samples <= 0 || num_samples > 1000) { // Validate number of samples
                    syslog(LOG_ERR, "Number of samples must be greater than 0 and less than 1000.\n");
                    return -1; // Exit if invalid number of samples
                }
                break;
            case 'v':
                cmd_inc = atoi(optarg);
                syslog(LOG_INFO, "Command value increment set to %d", cmd_inc);
                break;
            case 's':
                num_steps = atoi(optarg);
                if(num_steps <= 0){
                    syslog(LOG_ERR, "Number of steps must be greater than 0.\n");
                    return -1; // Exit if invalid number of steps
                }
                syslog(LOG_INFO, "Number of steps set to %d", num_steps);
                break;
            default:
                fprintf(stderr, "Usage: %s [-l logfile] [-n num_samples] [-v command] [-s number of steps]\n", argv[0]);
                return -1; // Exit on invalid option
        }
    }

        // Initialize the UDP communication to the KASM PCB
    int fd=0;
    fd = UDP_init(ip, port);

    if(fd<0){
        syslog(LOG_ERR, "Failed to get socket descriptor");
        exit(EXIT_FAILURE);
    } else{
        syslog(LOG_INFO, "UDP client initialized");
    }

    /* Get baseline data */
    send_command(start_value); // Send initial command value to actuater
    usleep(100000); // Sleep for 100ms to allow actuater to settle

    // Initialize wiringPi library and get the file descriptor for I2C communication
    i2c_fd = wiringPiI2CSetup(LDC1614_ADDR);
    uint16_t ID = 0;

    if (i2c_fd == -1) {
        syslog(LOG_ERR,"Failed to initialize I2C peripheral: %s\n", strerror(errno));
        return -1;
    }
    syslog(LOG_INFO, "I2C peripheral initialized.\n");

    // Initialize the LDC1614 for the specified channel 
    if (ldc1614_init(i2c_fd, channel) != 0) {
        syslog(LOG_ERR, "Failed to initialize LDC1614 on channel %d: %s\n", channel, strerror(errno));
        return -1;
    }
    syslog(LOG_DEBUG, "LDC1614 initialized on channel %d.\n", channel);

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
        syslog(LOG_INFO, "LDC1614 Device ID: 0x%04X verified\n", ID);
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

 
    // Get the data from the LDC1614 and log to a file
    for(int step = 0; step < num_steps; step++) {
        for(int i=0; i < num_samples; i++) {
            status = 0;
            while(status == 0) {
                ldc1614_read_reg(i2c_fd, LDC1614_STATUS, &status);
                status = status & 0x08; // Check if data is ready
            }
            ret = ldc1614_read_ch0(i2c_fd, &value);
            if (ret == -1) {
                syslog(LOG_ERR, "Failed to read value: %s\n", strerror(errno));
                // return -1;
            } else {
                clock_gettime(CLOCK_MONOTONIC, &current_time); // Get current time for timestamp
                elapsed_time = get_elapsed_time(start_time, current_time); // Calculate elapsed time
                char data_line[80]; 
                int line_length = 0; 
                line_length =  sprintf( data_line, "%d, %ld.%09ld, %d\n", channel,elapsed_time.tv_sec,elapsed_time.tv_nsec, value); // format data into a string
                if (write(log_fd, data_line, line_length) == -1) {
                    syslog(LOG_ERR, "Failed to write data to log file: %s", strerror(errno));
                    fprintf(stderr, "Failed to write data to log file: %s\n", strerror(errno));
                    close(log_fd);
                    return -1; // Exit with error if data write fails
                }
            }
        }
        cmd_val += cmd_inc;
        if(abs(cmd_val) > max_cmd) {
            syslog(LOG_ERR, "Command value exceeded maximum limit of %d. Stopping data collection.", max_cmd);
            break; 
        }

        if (send_command(cmd_val) == -1) {
            syslog(LOG_ERR, "Failed to send command value %d: %s\n", cmd_val, strerror(errno));
            break;
        }
    }

    close(log_fd); 
    syslog(LOG_INFO, "Data collection complete.\n");
    closelog();
    return 0;

}

