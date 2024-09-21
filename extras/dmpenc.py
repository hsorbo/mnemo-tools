#!/usr/bin/env python3
import argparse

def to_unsigned(x): return x if x > -1  else 256 + x
def to_signed(x):   return x if x < 128 else -256+x

def decode(semicolon_separated):
    string_bytes = [l for l in semicolon_separated.decode("ascii").split(';') if l.strip()]
    return bytearray(map(to_unsigned,map(int,string_bytes)))

def encode(raw_data):
    dmp_str = ';'.join(str(to_signed(e)) for e in raw_data) + ";\r\n"
    return bytes(dmp_str, "ascii")

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--encode", "-e", help="encode to ariane compatible dmp-format", action="store_true")
    group.add_argument("--decode", "-d", help="decode from ariane compatible format to binary", action="store_true")
    parser.add_argument("--input", "-i", help="input file", type=argparse.FileType('rb'))
    parser.add_argument("--output", "-o", help="output file", type=argparse.FileType('wb'))
    args = parser.parse_args()
    codec = encode if args.encode else decode
    args.output.write(codec(args.input.read()))
