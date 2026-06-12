import socket
import csv
import time
import struct
import sys
import argparse

# --- Server Defaults (Override via script if needed) ---
SERVER_IP = "127.0.0.1"
SERVER_PORT = 5432

def parse_arguments():
    """Configures and parses command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Remote logging client for the LDC1614 frequency sensor service."
    )
    
     parser.add_argument(
        '-i', '--IP',
        type=str,
        default=SERVER_IP,
        help="Server IP address. Default is '127.0.0.1'."
    )

    parser.add_argument(
        '-p', '--period',
        type=float,
        default=50.0,
        help="Logging period in milliseconds. Default is 50 ms (20 Hz)."
    )
    
    parser.add_argument(
        '-f', '--filename',
        type=str,
        default="ldc_log.csv",
        help="Filename for the destination CSV log. Default is 'ldc_log.csv'."
    )
    
    parser.add_argument(
        '-n', '--samples',
        type=int,
        default=1000,
        help="Total number of data points to collect. Default is 1000."
    )
    
    return parser.parse_args()

def main():
    args = parse_arguments()
    
    # Map arguments to local variables
    poll_interval_sec = args.period / 1000.0  # Convert ms input to seconds
    filename = args.output
    total_samples = args.samples
    
    print("Initializing LDC1614 Remote Logger...")
    print(f"Configuration -> Interval: {args.period}ms ({1/poll_interval_sec:.1f}Hz) | File: {filename} | Target: {total_samples} samples")
    
    # Setup UDP Socket
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.settimeout(1.0) 
    server_addr = (args.IP, SERVER_PORT)
    
    samples_collected = 0
    
    try:
        print(f"Opening log file '{filename}'...")
        with open(filename, mode='w', newline='') as csv_file:
            csv_writer = csv.writer(csv_file)
            
            # Write the header line
            csv_writer.writerow(['Timestamp', 'Freq'])
            
            print("Logging started...")
            next_time = time.time()
            
            while samples_collected < total_samples:
                try:
                    # Request data from service
                    udp_sock.sendto(b'\x00', server_addr)
                    
                    # Receive data from service
                    data, _ = udp_sock.recvfrom(1024)
                    timestamp = time.time() 
                    
                    if len(data) == 4:
                        freq_val = struct.unpack("!I", data)[0]
                        
                        # Log to CSV file
                        csv_writer.writerow([timestamp, freq_val])
                        samples_collected += 1
                    else:
                        print(f"Warning: Received payload of unexpected size ({len(data)} bytes)", file=sys.stderr)
                        
                except socket.timeout:
                    print("Warning: Connection timed out. Is the LDC service running?", file=sys.stderr)
                except Exception as e:
                    print(f"Error while fetching data: {e}", file=sys.stderr)
                
                # High-precision interval loop tracking
                next_time += poll_interval_sec
                sleep_time = next_time - time.time()
                if sleep_time > 0:
                    time.sleep(sleep_time)
                else:
                    # Reset baseline if logging pipeline lags
                    next_time = time.time()

        print(f"\nSuccessfully logged {samples_collected} data points.")

    except KeyboardInterrupt:
        print(f"\nLogging execution interrupted by user. Saved {samples_collected} points.")
    finally:
        udp_sock.close()
        print("UDP socket closed. Safe to exit.")

if __name__ == "__main__":
    main()