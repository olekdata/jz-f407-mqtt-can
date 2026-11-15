"""Read JZ-F407 stack headroom, LwIP heap statistics and CAN registers."""

import argparse
from pathlib import Path
import re
import subprocess


RAM_START = 0x20000000
RAM_END = 0x20020000
CAN1_BASE = 0x40006400
FIELDS = ("err", "avail", "used", "max", "illegal")
PROJECT = Path(__file__).resolve().parent.parent


def elf_metadata(gdb, elf):
    queries = {
        "heap_start": "(unsigned int)&_end",
        "heap_end_pointer": "(unsigned int)&__sbrk_heap_end",
        "stack_offset": "(unsigned int)&((TCB_t *)0)->pxStack",
        "name_offset": "(unsigned int)&((TCB_t *)0)->pcTaskName",
    }
    for field in FIELDS:
        queries[field + "_address"] = f"(unsigned int)&lwip_stats.mem.{field}"
        queries[field + "_size"] = f"sizeof(lwip_stats.mem.{field})"
    command = [gdb, "-batch", str(elf)]
    for expression in queries.values():
        command.extend(["-ex", "p /x " + expression])
    result = subprocess.run(
        command, capture_output=True, text=True, check=True, timeout=30
    )
    values = re.findall(r"^\$\d+ = (0x[0-9a-fA-F]+)$", result.stdout, re.M)
    if len(values) != len(queries):
        raise RuntimeError(f"Cannot resolve ELF metadata:\n{result.stdout}\n{result.stderr}")
    return dict(zip(queries, (int(value, 16) for value in values)))


def read_memory(programmer, serial, address, size):
    if size <= 0 or not (
        RAM_START <= address < address + size <= RAM_END
        or (address == CAN1_BASE and size == 32)
    ):
        raise RuntimeError(f"Read outside allowed JZ-F407 memory: {address:#x}, {size}")
    result = subprocess.run(
        [
            programmer, "-c", "port=SWD", "sn=" + serial, "mode=HOTPLUG",
            "-r8", hex(address), str(size),
        ],
        capture_output=True, text=True, check=True, timeout=30,
    )
    data = bytearray()
    for line in result.stdout.splitlines():
        match = re.fullmatch(
            r"(0x[0-9a-fA-F]{8})\s*:\s*((?:[0-9a-fA-F]{2}\s*)+)", line.strip()
        )
        if match:
            if int(match[1], 16) != address + len(data):
                raise RuntimeError("Non-contiguous programmer memory output")
            data.extend(bytes.fromhex(match[2]))
    if len(data) != size:
        raise RuntimeError(
            f"Read {len(data)} of {size} bytes:\n{result.stdout}\n{result.stderr}"
        )
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True, help="Target ST-LINK serial number")
    parser.add_argument(
        "--elf", type=Path,
        default=PROJECT / "build" / "debug" / "JZ-F407-MQTT-CAN.elf",
    )
    parser.add_argument(
        "--gdb",
        default=r"D:\ST\STM32CubeCLT_1.22.0\GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe",
    )
    parser.add_argument(
        "--programmer",
        default=r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
    )
    args = parser.parse_args()
    if not args.elf.is_file():
        parser.error(f"ELF not found: {args.elf}")
    metadata = elf_metadata(args.gdb, args.elf)

    def read(address, size):
        return read_memory(args.programmer, args.serial, address, size)

    start = metadata["heap_start"]
    end = int.from_bytes(read(metadata["heap_end_pointer"], 4), "little")
    heap = read(start, end - start)
    name = b"tcpip_thread\x00"
    if heap.count(name) != 1:
        raise RuntimeError("Expected exactly one TCP/IP TCB in the heap_3 region")
    pointer_offset = heap.index(name) - metadata["name_offset"] + metadata["stack_offset"]
    if not 0 <= pointer_offset <= len(heap) - 4:
        raise RuntimeError("Invalid TCP/IP TCB location")
    stack = int.from_bytes(heap[pointer_offset : pointer_offset + 4], "little")
    if not start <= stack < end or stack % 4:
        raise RuntimeError(f"Invalid TCP/IP stack pointer: {stack:#x}")
    stack_bytes = heap[stack - start :]
    free = next((i for i, value in enumerate(stack_bytes) if value != 0xA5), None)
    if free is None:
        raise RuntimeError("Cannot locate the used part of the TCP/IP stack")
    print(f"TCP/IP minimum remaining stack (A5 pattern): {free} bytes")
    if free < 512:
        raise RuntimeError("TCP/IP stack headroom below 512 bytes")

    stats_start = min(metadata[field + "_address"] for field in FIELDS)
    stats_end = max(
        metadata[field + "_address"] + metadata[field + "_size"] for field in FIELDS
    )
    stats_bytes = read(stats_start, stats_end - stats_start)
    stats = {}
    for field in FIELDS:
        offset = metadata[field + "_address"] - stats_start
        size = metadata[field + "_size"]
        if size not in (1, 2, 4):
            raise RuntimeError(f"Unsupported LwIP statistics field size: {field}, {size}")
        stats[field] = int.from_bytes(stats_bytes[offset : offset + size], "little")
    print("LwIP heap:", stats)
    if stats["err"] or stats["illegal"]:
        raise RuntimeError("LwIP allocation errors or illegal frees detected")

    registers = read(CAN1_BASE, 32)
    tsr = int.from_bytes(registers[8:12], "little")
    esr = int.from_bytes(registers[24:28], "little")
    btr = int.from_bytes(registers[28:32], "little")
    print(f"CAN TSR={tsr:#010x}, ESR={esr:#010x}, BTR={btr:#010x}")
    if tsr & 0x1C000000 != 0x1C000000 or esr & 0x77:
        raise RuntimeError("CAN TX mailbox occupied or CAN error detected")
    print("PASS: TCP/IP stack, LwIP allocator and idle CAN TX checks")


if __name__ == "__main__":
    main()
