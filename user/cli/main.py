#!/usr/bin/env python3
# Simple Python program to communicate with the nxp_simtemp driver
# Uses poll() to wait for data and can also configure parameters via sysfs

import os
import time
import struct
import select
from datetime import datetime, UTC  # add UTC to imports above
import argparse

# Structure used by the driver
# unsigned long long (Q) -> timestamp
# int (i) -> temperature
# unsigned int (I) -> flags
SAMPLE_STRUCT = "Q i I"
SAMPLE_SIZE = struct.calcsize(SAMPLE_STRUCT)

# Paths to files
DEV_PATH = "/dev/simtemp0"
SYSFS_PATH = "/sys/class/simtemp/simtemp0"

def leer_sysfs(nombre):
    try:
        with open(os.path.join(SYSFS_PATH, nombre), "r") as f:
            return f.read().strip()
    except Exception as e:
        print("Error reading sysfs:", e)
        return None

def escribir_sysfs(nombre, valor):
    try:
        with open(os.path.join(SYSFS_PATH, nombre), "w") as f:
            f.write(str(valor))
        print(f"{nombre} = {valor}")
    except Exception as e:
        print("Error writing sysfs:", e)

def mostrar_tiempo(ns):
    # Convert nanoseconds to seconds and create a UTC datetime object
    dt = datetime.fromtimestamp(ns / 1e9, tz=UTC)
    # Strip the last 3 digits of microseconds to keep milliseconds only and append 'Z' to indicate UTC time
    return dt.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"


def main():
    parser = argparse.ArgumentParser(description="CLI for the nxp_simtemp driver (simple version)")
    parser.add_argument("--timeout", type=int, default=2000, help="Wait time (ms) for poll()")
    parser.add_argument("--sampling", type=int, help="Sampling period in ms")
    parser.add_argument("--threshold", type=int, help="Threshold in milliCelsius")
    parser.add_argument("--mode", type=str, help="Mode: normal, noisy, or ramp")
    parser.add_argument("--test", action="store_true", help="Automatic alert test")
    args = parser.parse_args()

    # Configure if requested by the user
    print("Initial configuration:\n")
    if args.sampling:
        escribir_sysfs("sampling_ms", args.sampling)
    if args.threshold:
        escribir_sysfs("threshold_mc", args.threshold)
    if args.mode:
        escribir_sysfs("mode", args.mode)

    # Check if the user specified a timeout different from the default
    if args.timeout != 2000:
        print(f"Wait time (poll timeout) modified: {args.timeout} ms\n")
    else:
        print(f"Default wait time (poll timeout): {args.timeout} ms\n")
    
    # Show stats if they exist
    stats = leer_sysfs("stats")
    if stats:
        print("Current stats:\n", stats, "\n")

    # Open the device file
    try:
        fd = os.open(DEV_PATH, os.O_RDONLY | os.O_NONBLOCK)
    except Exception as e:
        print("Could not open device:", e)
        return

    poller = select.poll()
    poller.register(fd, select.POLLIN | select.POLLPRI)
    

    print("Waiting for temperature samples...\n(CTRL+C to exit)")
    inicio = time.time()
    alerta = False

    try:
        while True:
            eventos = poller.poll(args.timeout) # wait seconds
            if not eventos:
                print("No data (timeout).")
                continue

            for fd_event, flag in eventos:
                if flag & select.POLLPRI:
                    print(">>> ALERT: Threshold exceeded <<<")
                    alerta = True

                if flag & select.POLLIN:
                    data = os.read(fd, SAMPLE_SIZE)
                    if len(data) == SAMPLE_SIZE:
                        ts_ns, temp_mC, flags = struct.unpack(SAMPLE_STRUCT, data)
                        tiempo = mostrar_tiempo(ts_ns)
                        print(f"{tiempo} temp={temp_mC/1000:.1f}C alert={(flags & 0x2) >> 1}")
                        
                    else:
                        print("Incomplete data received")

            # Test mode (checks if there is an alert in 2 periods)
            if args.test:
                periodo = args.sampling if args.sampling else 5000
                if time.time() - inicio > (2 * periodo / 1000):
                    if alerta:
                        print("TEST: PASS (alert detected)")
                        return
                    else:
                        print("TEST: FAIL (no alert detected)")
                        return

    except KeyboardInterrupt:
        print("\nExiting program.")
    finally:
        os.close(fd)

if __name__ == "__main__":
    main()
