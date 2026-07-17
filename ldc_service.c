#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

// --- LDC1614 Register Definitions ---
#define LDC1614_ADDR             0x2B
#define LDC1614_DATA0_MSB        0x00
#define LDC1614_DATA0_LSB        0x01
#define LDC1614_RCOUNT0          0x08
#define LDC1614_SETTLECOUNT0     0x10
#define LDC1614_CLOCK_DIVIDERS0  0x14
#define LDC1614_ERROR_CONFIG     0x19
#define LDC1614_MUX_CONFIG       0x1B
#define LDC1614_CONFIG           0x1A
#define LDC1614_DRIVE_CURRENT0   0x1E

// --- Error Config Bit Masks ---
#define LDC_DRDY_2INT            (1 << 0)
#define LDC1614_AL_ERR2OUT       (1 << 11)
#define LDC1614_AH_ERR2OUT       (1 << 12)
#define LDC1614_WD_ERR2OUT       (1 << 13)
#define LDC1614_OR_ERR2OUT       (1 << 14)
#define LDC1614_UR_ERR2OUT       (1 << 15)

#define ERROR_CONFIG_VAL (LDC_DRDY_2INT | LDC1614_AH_ERR2OUT | LDC1614_AL_ERR2OUT | LDC1614_UR_ERR2OUT | LDC1614_OR_ERR2OUT)

// --- Polling Configuration ---
#define POLL_INTERVAL_NS         1000000 // 1 ms in nanoseconds (1 kHz polling rate)

// --- Global Shared State ---
pthread_mutex_t value_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t current_frequency_value = 0;
volatile sig_atomic_t stop_event = 0;
int logging = 0; // default logging disabled
char logfile[50] = "./testing/ldc1614_log.csv"; // default logfile name
int port = 5432; // default UDP port


// Signal handler to gracefully shut down the service
void handle_sigint(int sig) {
    stop_event = 1;
}

// Read 16-bit register (SMBus reads little-endian, LDC1614 requires big-endian swap)
int32_t i2c_read_reg16(int fd, uint8_t reg) {
    int32_t word = i2c_smbus_read_word_data(fd, reg);
    if (word < 0) return word;
    return (int32_t)ntohs((uint16_t)word);
}

// Write 16-bit register (LDC1614 requires big-endian format)
int32_t i2c_write_reg16(int fd, uint8_t reg, uint16_t value) {
    uint16_t swapped = htons(value);
    return i2c_smbus_write_word_data(fd, reg, swapped);
}

// Initializes the LDC1614 with specific configuration values
void init_device(int fd) {
    i2c_write_reg16(fd, LDC1614_RCOUNT0, 0xFFFF);
    i2c_write_reg16(fd, LDC1614_SETTLECOUNT0, 0x000A);
    i2c_write_reg16(fd, LDC1614_CLOCK_DIVIDERS0, 0x1001);
    i2c_write_reg16(fd, LDC1614_ERROR_CONFIG, ERROR_CONFIG_VAL);
    i2c_write_reg16(fd, LDC1614_MUX_CONFIG, 0x020C);
    i2c_write_reg16(fd, LDC1614_DRIVE_CURRENT0, 0xB000);
    i2c_write_reg16(fd, LDC1614_CONFIG, 0x1601);
}

// polling thread
void* polling_worker(void* arg) {
    int fd = *(int*)arg;
    struct timespec next_time;
    
    printf("Starting LDC1614 hardware polling thread...\n");
    clock_gettime(CLOCK_MONOTONIC, &next_time);

    while (!stop_event) {
        int32_t msb = i2c_read_reg16(fd, LDC1614_DATA0_MSB);
        int32_t lsb = i2c_read_reg16(fd, LDC1614_DATA0_LSB);

        if (msb >= 0 && lsb >= 0) {
            // Mask out error flags (top 4 bits of MSB) and combine to 28-bit
            uint32_t val = (((uint32_t)msb & 0x0FFF) << 16) | (uint32_t)lsb;
            
            pthread_mutex_lock(&value_lock);
            current_frequency_value = val;
            pthread_mutex_unlock(&value_lock);
        }

        // Calculate next wake-up time to prevent drift
        next_time.tv_nsec += POLL_INTERVAL_NS;
        if (next_time.tv_nsec >= 1000000000L) {
            next_time.tv_nsec -= 1000000000L;
            next_time.tv_sec += 1;
        }

        // Sleep until the next 1ms interval
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    int opt = 0; // option for command line argument parsing

    printf("Initializing LDC1614 Sensor Service...\n");

    while((opt = getopt(argc, argv, "hp:lf:")) != -1) {
        switch(opt) {
            case 'h':
                printf("Usage: %s [-h] [-p port] [-l] [-f logfile]\n", argv[0]);
                printf("  -h : Show this help message\n");
                return 0;
            case 'p':
                port = atoi(optarg);
                printf("UDP port set to: %d\n", port);
                break;
            case 'l':
                logging = 1;
                printf("Logging enabled\n");
                break;
            case 'f':
                strncpy(logfile, optarg, sizeof(logfile) - 1);
                logfile[sizeof(logfile) - 1] = '\0'; // Ensure null termination
                logging = 1; // Enable logging if logfile is specified
                printf("Log file set to: %s\n", logfile);
                break;
            default:
                printf("Usage: %s [-h] [-p port] [-l] [-f logfile]\n", argv[0]);
                return -1; // Exit on invalid option
        }
    }

    // Trap SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);

    // --- I2C Setup ---
    int i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) {
        perror("Failed to open I2C bus");
        return 1;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, LDC1614_ADDR) < 0) {
        perror("Failed to acquire bus access and/or talk to slave");
        close(i2c_fd);
        return 1;
    }

    init_device(i2c_fd);

    // --- Start Polling Thread ---
    pthread_t poll_thread;
    if (pthread_create(&poll_thread, NULL, polling_worker, &i2c_fd) != 0) {
        perror("Failed to create polling thread");
        close(i2c_fd);
        return 1;
    }

    // --- UDP Server Setup ---
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("Failed to create UDP socket");
        stop_event = 1;
        pthread_join(poll_thread, NULL);
        close(i2c_fd);
        return 1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(udp_sock, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("UDP Bind failed");
        stop_event = 1;
        close(udp_sock);
        pthread_join(poll_thread, NULL);
        close(i2c_fd);
        return 1;
    }

    // Set 0.5s receive timeout on the UDP socket to periodically check stop_event
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    setsockopt(udp_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    printf("UDP Server listening on port %d...\n", port);

    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    char recv_buffer[1024];

    // --- Main UDP Server Loop ---
    while (!stop_event) {
        int n = recvfrom(udp_sock, recv_buffer, sizeof(recv_buffer), 0, 
                         (struct sockaddr*)&cliaddr, &len);
        
        if (n > 0) {
            // Retrieve thread-safe value
            pthread_mutex_lock(&value_lock);
            uint32_t val = current_frequency_value;
            pthread_mutex_unlock(&value_lock);

            // Pack the 28-bit integer into a 4-byte Big-Endian network integer
            uint32_t net_val = htonl(val);
            sendto(udp_sock, &net_val, sizeof(net_val), 0, 
                   (struct sockaddr*)&cliaddr, len);
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("UDP receive error");
        }
    }

    // --- Cleanup ---
    printf("\nShutting down service...\n");
    pthread_join(poll_thread, NULL);
    close(udp_sock);
    close(i2c_fd);

    return 0;
}