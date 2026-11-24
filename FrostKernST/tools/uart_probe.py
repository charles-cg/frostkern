#!/usr/bin/env python3
"""FrostKern UART v1: encode/decode without dependencies; optional pyserial monitor."""
import argparse
import binascii
import json
import struct
import time

SOF, MAX_PAYLOAD = 0xA5, 64
NAMES = {1: "HEARTBEAT", 2: "PING", 3: "PONG", 0x10: "TELEMETRY_TEMP",
         0x11: "DEVICE_JOINED", 0x20: "PERMIT_JOIN", 0x21: "SET_SETPOINT",
         0x7E: "ACK", 0x7F: "NACK"}


def encode(kind, sequence, payload=b""):
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload exceeds 64 bytes")
    body = bytes((len(payload), kind, sequence)) + payload
    return bytes((SOF,)) + body + struct.pack("<H", binascii.crc_hqx(body, 0xFFFF))


def extract(buffer):
    while buffer:
        if buffer[0] != SOF:
            del buffer[0]
            continue
        if len(buffer) < 2:
            return
        size = buffer[1]
        if size > MAX_PAYLOAD:
            del buffer[0]
            continue
        total = size + 6
        if len(buffer) < total:
            return
        crc = struct.unpack_from("<H", buffer, size + 4)[0]
        if crc != binascii.crc_hqx(buffer[1:size + 4], 0xFFFF):
            del buffer[0]
            continue
        kind, sequence = buffer[2:4]
        payload = bytes(buffer[4:size + 4])
        del buffer[:total]
        yield kind, sequence, payload


def describe(frame):
    kind, sequence, p = frame
    result = {"type": NAMES.get(kind, hex(kind)), "seq": sequence,
              "payload_hex": p.hex(" ")}
    if kind in (1, 3) and len(p) in (6, 22):
        result.update(version=p[0], network_started=bool(p[1]),
                      uptime_ms=struct.unpack_from("<I", p, 2)[0])
        if len(p) == 22:
            result.update(zip(("rx_dropped", "uart_errors", "crc_errors", "tx_errors"),
                              struct.unpack_from("<IIII", p, 6)))
    elif kind == 0x11 and len(p) == 10:
        result.update(version=p[0], node=p[1], ieee=p[2:10].hex().upper())
    elif kind == 0x10 and len(p) == 21:
        flags = p[10]
        raw = struct.unpack_from("<h", p, 11)[0]
        result.update(version=p[0], node=p[1], ieee=p[2:10].hex().upper(),
                      temperature_valid=bool(flags & 1), stale=bool(flags & 2),
                      report_seen=bool(flags & 4),
                      temperature_c=raw / 100 if flags & 1 else None,
                      master_update_ms=struct.unpack_from("<I", p, 13)[0],
                      age_ms=struct.unpack_from("<I", p, 17)[0])
    elif kind in (0x7E, 0x7F) and len(p) == 2:
        result.update(request_type=NAMES.get(p[0], hex(p[0])), result_code=p[1])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    ping = commands.add_parser("ping", help="print a PING frame in hexadecimal")
    ping.add_argument("--seq", type=int, default=1)
    permit = commands.add_parser("permit-join", help="print a PERMIT_JOIN frame")
    permit.add_argument("--seconds", type=int, default=180)
    permit.add_argument("--seq", type=int, default=2)
    decode = commands.add_parser("decode", help="decode space-separated hexadecimal bytes")
    decode.add_argument("hex_bytes")
    monitor = commands.add_parser("monitor", help="requires pyserial and a RAW UART serial port")
    monitor.add_argument("--port", required=True)
    monitor.add_argument("--seconds", type=float, default=20)
    args = parser.parse_args()
    if hasattr(args, "seq") and not 0 <= args.seq <= 255:
        parser.error("--seq must be 0..255")
    if args.action == "ping":
        print(encode(2, args.seq).hex(" ").upper())
    elif args.action == "permit-join":
        if not 0 <= args.seconds <= 254:
            parser.error("--seconds must be 0..254")
        print(encode(0x20, args.seq, bytes((args.seconds,))).hex(" ").upper())
    elif args.action == "decode":
        frames = list(extract(bytearray.fromhex(args.hex_bytes)))
        if not frames:
            parser.error("no complete CRC-valid frame found")
        for frame in frames:
            print(json.dumps(describe(frame), ensure_ascii=False))
    else:
        try:
            import serial
        except ImportError:
            parser.error("install pyserial first: python -m pip install pyserial")
        buffer = bytearray()
        started, last_ping, last_rx, sequence = time.monotonic(), -10.0, time.monotonic(), 0
        with serial.Serial(args.port, 115200, timeout=0.02) as port:
            while time.monotonic() - started < args.seconds:
                now = time.monotonic()
                if now - last_ping >= 2:
                    port.write(encode(2, sequence))
                    sequence = (sequence + 1) & 255
                    last_ping = now
                data = port.read(256)
                if data:
                    if time.monotonic() - last_rx >= 0.1:
                        buffer.clear()
                    last_rx = time.monotonic()
                    buffer.extend(data)
                    for frame in extract(buffer):
                        print(json.dumps(describe(frame), ensure_ascii=False), flush=True)
                if len(buffer) > 70:
                    del buffer[:-70]


if __name__ == "__main__":
    main()
