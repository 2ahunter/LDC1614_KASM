import socket
import struct
import collections
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# --- Configuration ---
print("Starting LDC1614 Real-Time Plotting Client...")
SERVER_IP = "127.0.0.1"  # Replace with target IP if running remotely
PORT = 5432
WINDOW_SIZE = 200        # Number of data points to show on screen at once

# Deque handles rolling window array structures efficiently
x_data = collections.deque(maxlen=WINDOW_SIZE)
y_data = collections.deque(maxlen=WINDOW_SIZE)

# Initialize deque values
for i in range(WINDOW_SIZE):
    x_data.append(i)
    y_data.append(0)

# Set up socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(0.2)

# Set up Matplotlib Figure
fig, ax = plt.subplots()
line, = ax.plot(x_data, y_data, label="Ch 0 Frequency", color='teal')
ax.set_ylim(0, 2**28 - 1)  # Max scale of a 28-bit converter
ax.set_title("LDC1614 Real-Time Streaming Data")
ax.set_ylabel("Raw Conversion Value")
ax.set_xlabel("Samples Window")
ax.grid(True)

def fetch_data():
    try:
        # Send an empty ping payload to prompt response
        sock.sendto(b'\x00', (SERVER_IP, PORT))
        data, _ = sock.recvfrom(1024)
        # Unpack the 4-byte Big Endian Integer
        val = struct.unpack("!I", data)[0]
        return val
    except Exception:
        # Fall back to returning the last item if data drops frame
        return y_data[-1]

def update_plot(frame):
    new_val = fetch_data()
    y_data.append(new_val)
    
    line.set_ydata(y_data)
    
    # Dynamically scale view bounds based on current local window max/min
    current_min, current_max = min(y_data), max(y_data)
    padding = max(100, int((current_max - current_min) * 0.1))
    ax.set_ylim(current_min - padding, current_max + padding)
    
    return line,

# Animate at ~50Hz refresh rate (every 20ms)
ani = animation.FuncAnimation(fig, update_plot, blit=False, interval=20, cache_frame_data=False)
plt.legend(loc="upper right")
plt.show()
sock.close()