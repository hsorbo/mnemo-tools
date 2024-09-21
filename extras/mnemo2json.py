#!/usr/bin/env python3
import sys
import struct
import datetime
import json
from enum import Enum

HEADER_LEN=10
SHOT_LEN=16
DIRECTION=["IN","OUT"]
class ShotType(Enum):
    CSA = 0
    CSB = 1
    STD = 2
    EOC = 3

def to_unsigned(x): return x if x > -1 else 256 + x

def to_binary(semicolon_separated):
    string_bytes = [l for l in semicolon_separated.split(';') if l.strip()]
    return bytearray(map(to_unsigned,map(int,string_bytes)))

def parse_survey_header(data):
    (hdr,year,month,day,hh,mm,name,direction) = struct.unpack("bbbbbb3sb",data)
    if hdr != 2: raise Exception("Couldn't find magic number")
    if year not in range(16,25): raise Exception("Year is out of range")
    return {
        "date": datetime.datetime(2000+year, month, day, hh, mm).isoformat(),
        "name": name.decode("utf-8"),
        "direction":direction}

def parse_survey_shot(data):
    s = struct.unpack(">bhhhhhhhb",data)
    return {
        "type": s[0],
        "head_in": s[1]/10,
        "head_out": s[2]/10,
        "length": s[3]/100,
        "depth_in": s[4]/100,
        "depth_out": s[5]/100,
        "pitch_in": s[6]/10,
        "pitch_out": s[7]/10,
        "marker": s[8] }


def decode(data):
    x = 0
    surveys = []
    while True:
        hdr_data = data[x:x+HEADER_LEN]
        if len(hdr_data) < HEADER_LEN: break
        try:
            survey=parse_survey_header(hdr_data)
        except Exception as err:
            sys.stderr.write(err)
        except:
            sys.stderr.write("Failed to parse header at offset %d, ignoring\n" % x)
            break
        x+=HEADER_LEN
        shots = []
        for i in range(x,10000,SHOT_LEN):
            shot_arr = data[i:i+SHOT_LEN]
            if len(shot_arr) < SHOT_LEN: break
            shot = parse_survey_shot(shot_arr)
            shots.append(shot)
            x += SHOT_LEN
            if shot["type"] == ShotType.EOC.value: break
        survey["shots"] = shots
        surveys.append(survey)
    return surveys

if __name__ == "__main__":
    filename = sys.argv[1]
    data = decode(to_binary(open(filename).read()))
    print(json.dumps(data, sort_keys=True, indent=2))
