#!/usr/bin/env python3

def bit12(FPAIRED, FPROPER_PAIR, FUNMAP, FMUNMAP):
    return FPAIRED & (~FUNMAP) & FPROPER_PAIR

def bit13(FPAIRED, FPROPER_PAIR, FUNMAP, FMUNMAP):
    return FPAIRED & (~FUNMAP) & FMUNMAP

def bit14(FPAIRED, FPROPER_PAIR, FUNMAP, FMUNMAP):
    return FPAIRED & (~FUNMAP) & (~FMUNMAP)

print(" # | MNUNMAP | FUNMAP | FPROPER_PAIR | FPAIRED || bit #12 | bit #13 | bit #14 | pshufb word")
print("---+---------+--------+--------------+---------++---------+---------+---------+-------------")

pshufb_values = []
for k in range(16):
    FPAIRED      = int(k & 0x01 != 0)
    FPROPER_PAIR = int(k & 0x02 != 0)
    FUNMAP       = int(k & 0x04 != 0)
    FMUNMAP      = int(k & 0x08 != 0)

    b12 = bit12(FPAIRED, FPROPER_PAIR, FUNMAP, FMUNMAP) & 0x01
    b13 = bit13(FPAIRED, FPROPER_PAIR, FUNMAP, FMUNMAP) & 0x01
    b14 = bit14(FPAIRED, FPROPER_PAIR, FUNMAP, FMUNMAP) & 0x01
    
    pshufb = (b12 << 4) | (b13 << 5) | (b14 << 6)
    pshufb_values.append(pshufb)

    print(f"{k:^3x}|{FMUNMAP:^9}|{FUNMAP:^8}|{FPROPER_PAIR:^14}|{FPAIRED:^9}||{b12:^9}|{b13:^9}|{b14:^9}| 0x{pshufb:02x}")

print("pshufb: %s" % (', '.join(f'0x{x:02x}' for x in pshufb_values)),)
