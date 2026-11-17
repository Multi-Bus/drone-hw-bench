"""Send one newline-terminated runner command on a 115200/7O1 UART."""
import argparse
import time
import serial

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--port', required=True)
p.add_argument('command', help='s=pause initiator; r=run/rearm responder; a16368=set delay')
a = p.parse_args()
with serial.Serial(a.port, 115200, bytesize=serial.SEVENBITS,
                   parity=serial.PARITY_ODD, stopbits=serial.STOPBITS_ONE, timeout=.2) as port:
    port.reset_input_buffer()
    port.write((a.command + '\n').encode('ascii'))
    deadline = time.monotonic() + 2
    received = False
    while time.monotonic() < deadline:
        line = port.readline().decode('ascii', errors='replace').strip()
        if line.startswith('#'):
            print(line)
            if ('PAUSE' in line and a.command == 's') or ('RUN' in line and a.command == 'r') or ('ANTENNA_DELAY=' in line and a.command.startswith('a')):
                received = True
    if not received:
        raise SystemExit('No matching command acknowledgement; do not assume it was applied')
