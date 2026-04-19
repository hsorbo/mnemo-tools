#!/usr/bin/env python3
"""Read MCP2221A serial values via USB HID Read Flash Data (0xB0)."""

from __future__ import annotations

import argparse
import sys

try:
    import hid  # type: ignore
except ImportError:
    print("Missing dependency: hid/hidapi")
    print("Install with: python3 -m pip install hidapi")
    sys.exit(2)


DEFAULT_VID = 0x04D8
DEFAULT_PID = 0x00DD
READ_FLASH_DATA_CMD = 0xB0
USB_SERIAL_DESCRIPTOR_SUBCMD = 0x04
FACTORY_SERIAL_SUBCMD = 0x05
REPORT_SIZE = 64


def parse_hex_int(value: str) -> int:
    return int(value, 0)


def enumerate_matching_devices(vid: int, pid: int) -> list[dict]:
    return [
        d
        for d in hid.enumerate()
        if d.get("vendor_id") == vid and d.get("product_id") == pid
    ]


def open_device(dev_info: dict) -> "hid.device":
    dev = hid.device()
    dev.open_path(dev_info["path"])
    dev.set_nonblocking(0)
    return dev


def build_report(cmd: int, subcmd: int) -> list[int]:
    payload = [0] * (REPORT_SIZE + 1)
    payload[1] = cmd
    payload[2] = subcmd
    return payload


def read_flash_data(dev: "hid.device", cmd: int, subcmd: int, timeout_ms: int) -> bytes:
    report = build_report(cmd, subcmd)
    wrote = dev.write(report)
    if wrote <= 0:
        raise RuntimeError("Write failed")

    resp_list = dev.read(REPORT_SIZE, timeout_ms=timeout_ms)
    if not resp_list:
        raise RuntimeError("No response (timeout)")

    return bytes(resp_list)


def validate_response_header(resp: bytes, cmd: int) -> str | None:
    if len(resp) < 4:
        return f"response too short: {len(resp)} bytes"
    if resp[0] != cmd:
        return f"unexpected command echo: 0x{resp[0]:02X} (expected 0x{cmd:02X})"
    if resp[1] != 0x00:
        return f"device returned error status: 0x{resp[1]:02X}"
    return None


def decode_usb_serial_descriptor(resp: bytes) -> str:
    length = resp[2]
    descriptor_type = resp[3]

    if descriptor_type != 0x03:
        raise ValueError(f"unexpected descriptor type: 0x{descriptor_type:02X}")
    if length < 2:
        raise ValueError(f"invalid descriptor length: {length}")

    data_len = min(length - 2, len(resp) - 4)
    if data_len < 0:
        raise ValueError("invalid descriptor payload length")

    payload = resp[4 : 4 + data_len]
    if len(payload) % 2 != 0:
        payload = payload[:-1]

    text = payload.decode("utf-16le", errors="strict") if payload else ""
    return text.rstrip("\x00")


def decode_factory_serial(resp: bytes) -> str:
    length = resp[2]
    if length == 0:
        return ""

    data_len = min(length, len(resp) - 4)
    payload = resp[4 : 4 + data_len]
    return payload.decode("ascii", errors="replace").rstrip("\x00")


def print_raw_response(subcmd: int, resp: bytes) -> None:
    print(f"Raw response for subcmd 0x{subcmd:02X}:")
    print("  " + " ".join(f"{b:02X}" for b in resp))


def read_and_print_serial(
    dev: "hid.device", cmd: int, subcmd: int, timeout_ms: int, show_raw: bool
) -> int:
    resp = read_flash_data(dev, cmd=cmd, subcmd=subcmd, timeout_ms=timeout_ms)

    if show_raw:
        print_raw_response(subcmd, resp)

    header_error = validate_response_header(resp, cmd)
    if header_error:
        print(f"subcmd 0x{subcmd:02X}: {header_error}")
        return 1

    if subcmd == USB_SERIAL_DESCRIPTOR_SUBCMD:
        try:
            value = decode_usb_serial_descriptor(resp)
        except ValueError as ex:
            print(f"USB Serial Descriptor (0x04): decode error: {ex}")
            return 1
        print(f"USB Serial Descriptor (0x04): {value!r}")
        return 0

    if subcmd == FACTORY_SERIAL_SUBCMD:
        value = decode_factory_serial(resp)
        print(f"Chip Factory Serial (0x05): {value!r}")
        return 0

    print(f"Subcmd 0x{subcmd:02X}: unsupported decode mode")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Read MCP2221A serial data over USB HID Read Flash Data."
    )
    parser.add_argument(
        "--vid",
        type=parse_hex_int,
        default=DEFAULT_VID,
        help="USB vendor id (default: 0x04D8)",
    )
    parser.add_argument(
        "--pid",
        type=parse_hex_int,
        default=DEFAULT_PID,
        help="USB product id (default: 0x00DD)",
    )
    parser.add_argument(
        "--index", type=int, default=0, help="matching HID device index (default: 0)"
    )
    parser.add_argument(
        "--cmd",
        type=parse_hex_int,
        default=READ_FLASH_DATA_CMD,
        help="Read Flash Data command byte (default: 0xB0)",
    )
    parser.add_argument(
        "--mode",
        choices=("descriptor", "factory", "both"),
        default="descriptor",
        help="which serial to read (default: descriptor)",
    )
    parser.add_argument(
        "--subcmd",
        type=parse_hex_int,
        default=None,
        help="override and read exactly this subcommand (e.g. 0x04 or 0x05)",
    )
    parser.add_argument(
        "--raw",
        action="store_true",
        help="print raw 64-byte HID response",
    )
    parser.add_argument(
        "--timeout-ms",
        type=int,
        default=1500,
        help="read timeout in ms (default: 1500)",
    )
    args = parser.parse_args()

    matches = enumerate_matching_devices(args.vid, args.pid)
    if not matches:
        print(f"No HID devices found for VID:PID {args.vid:#06x}:{args.pid:#06x}")
        return 1

    if args.index < 0 or args.index >= len(matches):
        print(f"Invalid --index {args.index}; found {len(matches)} matching device(s)")
        for i, dev in enumerate(matches):
            print(
                f"  [{i}] path={dev.get('path')} interface={dev.get('interface_number')} product={dev.get('product_string')}"
            )
        return 1

    target = matches[args.index]
    print(f"Using HID path: {target.get('path')}")
    print(
        f"Interface: {target.get('interface_number')} Product: {target.get('product_string')}"
    )

    dev = open_device(target)
    try:
        failures = 0

        if args.subcmd is not None:
            failures += read_and_print_serial(
                dev,
                cmd=args.cmd,
                subcmd=args.subcmd,
                timeout_ms=args.timeout_ms,
                show_raw=args.raw,
            )
        elif args.mode == "descriptor":
            failures += read_and_print_serial(
                dev,
                cmd=args.cmd,
                subcmd=USB_SERIAL_DESCRIPTOR_SUBCMD,
                timeout_ms=args.timeout_ms,
                show_raw=args.raw,
            )
        elif args.mode == "factory":
            failures += read_and_print_serial(
                dev,
                cmd=args.cmd,
                subcmd=FACTORY_SERIAL_SUBCMD,
                timeout_ms=args.timeout_ms,
                show_raw=args.raw,
            )
        else:
            failures += read_and_print_serial(
                dev,
                cmd=args.cmd,
                subcmd=USB_SERIAL_DESCRIPTOR_SUBCMD,
                timeout_ms=args.timeout_ms,
                show_raw=args.raw,
            )
            failures += read_and_print_serial(
                dev,
                cmd=args.cmd,
                subcmd=FACTORY_SERIAL_SUBCMD,
                timeout_ms=args.timeout_ms,
                show_raw=args.raw,
            )

        if failures:
            return 1

    finally:
        dev.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
