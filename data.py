import serial
import time
from websocket import create_connection, WebSocketConnectionClosedException
import socket


SERIAL_PORT = 'COM4'
BAUD_RATE = 9600

def connect_ws():
    while True:
        try:
            ws = create_connection("ws://localhost:8000/ws")
            print("WebSocket connected.")
            return ws
        except Exception as e:
            print(f"WebSocket connection failed: {e}, retrying in 3s...")
            time.sleep(3)

def q_pH(value):
    try:
        value = float(value)
        if 6.5 <= value <= 8.5:
            return 100
        elif value < 6.5:
            return max(0, 100 - (6.5 - value) * 50)
        else:
            return max(0, 100 - (value - 8.5) * 50)
    except:
        return 0  # Return 0 if invalid

def q_turbidity(value):
    try:
        value = float(value)
        if value <= 5:
            return 100
        elif value <= 10:
            return 80
        elif value <= 20:
            return 60
        else:
            return 30
    except:
        return 0

def q_conductivity(value):
    try:
        value = float(value)
        if value <= 500:
            return 100
        elif value <= 1000:
            return 80
        elif value <= 2000:
            return 50
        else:
            return 20
    except:
        return 0


def compute_wqi(ph, turbidity, conductivity):
    weights = {'pH': 0.3, 'T': 0.4, 'E': 0.3}
    q_ph = q_pH(ph)
    q_turb = q_turbidity(turbidity)
    q_cond = q_conductivity(conductivity)

    return round(q_ph * weights['pH'] + q_turb * weights['T'] + q_cond * weights['E'], 2)

ws = connect_ws()
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
turbidity = None
conductivity = None
ph = None
time.sleep(2)  # Wait for Arduino reset

try:
    while True:
        line = ser.readline().decode('utf-8').strip()
        if line:
            print(f"Sending: {line}")
            if line.startswith(("T:", "E:", "pH:")):
                key, value = line.split(":")
                if key == "T":
                    turbidity = value
                elif key == "E":
                    value = float(value.strip())
                    conductivity = value
                elif key == "pH":
                    value = float(value.strip())
                    ph = value
                else:
                    continue
            
            try:
                ws.send(line)
                if ph and turbidity and conductivity:
                    wqi = compute_wqi(ph, turbidity, conductivity)
                    line = f"WQI: {wqi:.2f}"
                    ph = turbidity = conductivity = None
                    ws.send(line)
                    print(f"Sent WQI: {line}")
            except (WebSocketConnectionClosedException, ConnectionAbortedError, socket.error) as e:
                print(f"WebSocket error: {e}. Reconnecting...")
                try:
                    ws.close()
                except:
                    pass
                ws = connect_ws()
except KeyboardInterrupt:
    print("Interrupted by user. Exiting...")
finally:
    try:
        ws.close()
    except:
        pass
    ser.close()
