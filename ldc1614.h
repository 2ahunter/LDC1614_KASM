/*
 * ldc1614.h
 *
 *  Created on: May 29, 2025
 *      Author: aahunter
 */

#ifndef INC_LDC1614_H_
#define INC_LDC1614_H_

#include <stdint.h>

#define NSAMPS 100
#define SAMP_DELAY 10

#define leftshift(value,nbits) (((uint32_t)(value)) << (nbits))

// Device I2C Address
#define LDC1614_ADDR 0x2b //if ADDR pin pulled low, 0x2A, but it is pulled high so 0x2B
// Register Addresses
#define LDC1614_DATA0_MSB        0x00  // Channel 0 MSB Conversion Result and Error Status
#define LDC1614_DATA0_LSB        0x01  // Channel 0 LSB Conversion Result. Must be read after Register address 0x00.
#define LDC1614_DATA1_MSB        0x02  // Channel 1 MSB Conversion Result and Error Status
#define LDC1614_DATA1_LSB        0x03  // Channel 1 LSB Conversion Result. Must be read after Register address 0x02.
#define LDC1614_DATA2_MSB        0x04  // Channel 2 MSB Conversion Result and Error Status. (LDC1614 only)
#define LDC1614_DATA2_LSB        0x05  // Channel 2 LSB Conversion Result. Must be read after Register address 0x04. (LDC1614 only)
#define LDC1614_DATA3_MSB        0x06  // Channel 3 MSB Conversion Result and Error Status. (LDC1614 only)
#define LDC1614_DATA3_LSB        0x07  // Channel 3 LSB Conversion Result. Must be read after Register address 0x06. (LDC1614 only)
#define LDC1614_RCOUNT0          0x08  // Reference Count setting for Channel 0
#define LDC1614_RCOUNT1          0x09  // Reference Count setting for Channel 1
#define LDC1614_RCOUNT2          0x0A  // Reference Count setting for Channel 2. (LDC1614 only)
#define LDC1614_RCOUNT3          0x0B  // Reference Count setting for Channel 3. (LDC1614 only)
#define LDC1614_OFFSET0          0x0C  // Offset value for Channel 0
#define LDC1614_OFFSET1          0x0D  // Offset value for Channel 1
#define LDC1614_OFFSET2          0x0E  // Offset value for Channel 2. (LDC1614 only)
#define LDC1614_OFFSET3          0x0F  // Offset value for Channel 3. (LDC1614 only)
#define LDC1614_SETTLECOUNT0     0x10  // Channel 0 Settling Reference Count
#define LDC1614_SETTLECOUNT1     0x11  // Channel 1 Settling Reference Count
#define LDC1614_SETTLECOUNT2     0x12  // Channel 2 Settling Reference Count. (LDC1614 only)
#define LDC1614_SETTLECOUNT3     0x13  // Channel 3 Settling Reference Count. (LDC1614 only)
#define LDC1614_CLOCK_DIVIDERS0  0x14  // Reference and Sensor Divider settings for Channel 0
#define LDC1614_CLOCK_DIVIDERS1  0x15  // Reference and Sensor Divider settings for Channel 1
#define LDC1614_CLOCK_DIVIDERS2  0x16  // Reference and Sensor Divider settings for Channel 2. (LDC1614 only)
#define LDC1614_CLOCK_DIVIDERS3  0x17  // Reference and Sensor Divider settings for Channel 3. (LDC1614 only)
#define LDC1614_STATUS           0x18  // Device Status Report
#define LDC1614_ERROR_CONFIG     0x19  // Error Reporting Configuration
#define LDC1614_CONFIG           0x1A  // Conversion Configuration
#define LDC1614_MUX_CONFIG       0x1B  // Channel Multiplexing Configuration
#define LDC1614_RESET_DEV        0x1C  // Reset Device
#define LDC1614_DRIVE_CURRENT0   0x1E  // Channel 0 sensor current drive configuration
#define LDC1614_DRIVE_CURRENT1   0x1F  // Channel 1 sensor current drive configuration
#define LDC1614_DRIVE_CURRENT2   0x20  // Channel 2 sensor current drive configuration. (LDC1614 only)
#define LDC1614_DRIVE_CURRENT3   0x21  // Channel 3 sensor current drive configuration. (LDC1614 only)
#define LDC1614_MANUFACTURER_ID  0x7E  // Manufacturer ID
#define LDC1614_DEVICE_ID        0x7F  // Device ID

// error configs
#define LDC_DRDY_2INT  1<<0    // Data ready interrupt
#define LDC1614_AL_ERR2OUT  1<<11   // report amplitude low error
#define LDC1614_AH_ERR2OUT  1<<12   // report amplitude high error
#define LDC1614_WD_ERR2OUT  1<<13   // report watchdog error
#define LDC1614_OR_ERR2OUT  1<<14   // report over-range error
#define LDC1614_UR_ERR2OUT  1<<15   // report under-range error

// error codes
#define LDC1614_ERR_UR0  1<<15   // under-range error
#define LDC1614_ERR_OR0  1<<14   // over-range error
#define LDC1614_ERR_WD0  1<<13   // watchdog error
#define LDC1614_ERR_AE0  1<<12   // amplitude error


// device status
#define LDC1614_DATA_READY (1<<6) // Data is available 

uint16_t byteswap(uint16_t value);
int ldc1614_init(int fd, int channel);
int ldc1614_read_reg(int fd, uint8_t reg, uint16_t *value);
int ldc1614_write_reg(int fd, uint8_t reg, uint16_t value);
int ldc1614_read_ch0(int fd, uint32_t *value);

#endif /* INC_LDC1614_H_ */
