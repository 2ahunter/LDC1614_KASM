import socket
import threading
import time
import struct
import sys
from smbus2 import SMBus

# --- LDC1614 Register Definitions ---
LDC1614_ADDR = 0x2B
LDC1614_DATA0_MSB = 0x00
LDC1614_DATA0_LSB = 0x01
LDC1614_RCOUNT0 = 0x08
LDC1614_SETTLECOUNT0 = 0x10
LDC1614_CLOCK_DIVIDERS0 = 0x14
LDC1614_ERROR_CONFIG = 0x19
LDC1614_MUX_CONFIG = 0x1B
LDC1614_DRIVE_CURRENT0 = 0x1E
LDC1614_CONFIG = 0x1A

# --- Error Config Bit Masks ---
LDC_DRDY_2INT = 1<<0    # Data ready interrupt
LDC1614_AL_ERR2OUT = 1<<11   # report amplitude low error
LDC1614_AH_ERR2OUT = 1<<12   # report amplitude high error
LDC1614_WD_ERR2OUT = 1<<13   # report watchdog error
LDC1614_OR_ERR2OUT = 1<<14   # report over-range error
LDC1614_UR_ERR2OUT = 1<<15   # report under-range error

# --- Error Flags in Data MSB ---
LDC1614_ERR_UR0 = 1<<15   # under-range error
LDC1614_ERR_OR0 = 1<<14   # over-range error
LDC1614_ERR_WD0 = 1<<13   # watchdog error
LDC1614_ERR_AE0 = 1<<12   # amplitude error

error_config = LDC_DRDY_2INT| LDC1614_AH_ERR2OUT | LDC1614_AL_ERR2OUT | LDC1614_UR_ERR2OUT | LDC1614_OR_ERR2OUT



POLL_INTERVAL_SEC = 0.0005  # 500 microseconds
BUFFER_DURATION_SEC = 1.0
BUFFER_SIZE = int(BUFFER_DURATION_SEC / POLL_INTERVAL_SEC)  # 2000 samples

class FreqValue:
    def __init__(self):
        self.current_value = 0
        self.lock = threading.Lock()

    def update(self, value):
        with self.lock:
            self.current_value = value

    def get_value(self):
        with self.lock:
            return self.current_value


class LDC1614Driver:
    def __init__(self, bus_number=1):
        self.bus_number = bus_number
        self.bus = None

    # __enter__ and __exit__ allow the use of 'with' statement for automatic 
    # resource management
    def __enter__(self):
        self.bus = SMBus(self.bus_number)
        self.init_device()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.bus:
            self.bus.close()

    def _read_reg_16(self, reg):
        # read_word_data returns little-endian; LDC1614 is big-endian.
        # This performs the equivalent of the C byteswap logic.
        word = self.bus.read_word_data(LDC1614_ADDR, reg)
        return socket.ntohs(word)  # Network to host byte order (big-endian to host)

    def _write_reg_16(self, reg, value):
        # Convert to big-endian format for the LDC1614
        value = socket.htons(value)  # Host to network byte order (big-endian)
        self.bus.write_word_data(LDC1614_ADDR, reg, value)

    def read_channel_0(self):
        try:
            msb = self._read_reg_16(LDC1614_DATA0_MSB)
            lsb = self._read_reg_16(LDC1614_DATA0_LSB)
            
            # Mask out error flags (top 4 bits of MSB) and combine to 28-bit
            value = ((msb & 0x0FFF) << 16) | lsb
            return value
        except Exception as e:
            # Avoid crashing the polling thread on transient I2C hiccups
            return None

    def init_device(self):
        self._write_reg_16(LDC1614_RCOUNT0, 0xFFFF)
        self._write_reg_16(LDC1614_SETTLECOUNT0, 0x000A)
        self._write_reg_16(LDC1614_CLOCK_DIVIDERS0, 0x1001)
        self._write_reg_16(LDC1614_ERROR_CONFIG, error_config)
        self._write_reg_16(LDC1614_MUX_CONFIG, 0x020C)
        self._write_reg_16(LDC1614_DRIVE_CURRENT0, 0xb000)
        self._write_reg_16(LDC1614_CONFIG, 0x1601)

def polling_worker(driver, value, stop_event):
    print("Starting LDC1614 hardware polling thread...")
    next_time = time.time()
    
    while not stop_event.is_set():
        val = driver.read_channel_0()
        if val is not None:
            value.update(val)
        
        # High-precision timing loop adjustment
        next_time += POLL_INTERVAL_SEC
        sleep_time = next_time - time.time()
        if sleep_time > 0:
            time.sleep(sleep_time)


def main():
    print("Initializing LDC1614 Sensor Service...")
    value_container = FreqValue()
    stop_event = threading.Event()

    # Start UDP Server Setup
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.bind(("0.0.0.0", 5432))
    udp_sock.settimeout(0.5)
    print("UDP Server listening on port 5432...")

    try:
        with LDC1614Driver(bus_number=1) as driver:
            # Fire up the polling thread
            poll_thread = threading.Thread(
                target=polling_worker, 
                args=(driver, value_container, stop_event), 
                daemon=True
            )
            poll_thread.start()

            # Main thread runs the UDP server loop
            while True:
                try:
                    data, addr = udp_sock.recvfrom(1024)
                    # Respond back with the 28-bit integer packed as a 4-byte standard Big-Endian network integer
                    current_val = value_container.get_value()
                    response = struct.pack("!I", current_val)
                    udp_sock.sendto(response, addr)
                except socket.timeout:
                    continue  # Keep checking for keyboard interrupts
    except KeyboardInterrupt:
        print("\nShutting down service...")
    finally:
        stop_event.set()
        udp_sock.close()

if __name__ == "__main__":
    main()