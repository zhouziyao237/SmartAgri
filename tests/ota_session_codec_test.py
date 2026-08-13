OTA_MAGIC=0xA55A
VERSION=1

def crc(version,dev_lo,dev_hi,group,tid_lo,tid_hi):
    c=0x6D39
    for w in (version,dev_lo,dev_hi,group,tid_lo,tid_hi):
        c ^= w
        c=((c<<5)|(c>>11)) & 0xFFFF
        c=(c+0x9E37)&0xFFFF
    return c

def encode(device,group,transfer):
    words=[0]*8
    words[1]=VERSION
    words[2]=device&0xFFFF
    words[3]=(device>>16)&0xFFFF
    words[4]=group&0xFFFF
    words[5]=transfer&0xFFFF
    words[6]=(transfer>>16)&0xFFFF
    words[7]=crc(*words[1:7])
    words[0]=OTA_MAGIC
    return words

def decode(words):
    assert words[0]==OTA_MAGIC and words[1]==VERSION
    assert words[7]==crc(*words[1:7])
    dev=words[2]|(words[3]<<16)
    tid=words[5]|(words[6]<<16)
    return dev,words[4],tid

for dev in (1,1001,1002,0x12345678):
    for group in (1,2,77):
        for tid in (1,0x12345678,0xFFFFFFFF):
            w=encode(dev,group,tid)
            assert decode(w)==(dev,group,tid)
print('OTA SESSION CODEC PASS')
