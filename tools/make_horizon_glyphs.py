"""Convert only 仪/儀 from a local TrueType font to native HIF simple outlines.

Requires fontTools. Pass a font with 2048 units/em, as in the C 1.70 CJK face.
The native decoder accepts little-endian endpoint indices, on-curve bits and
signed x/y delta runs; no complete font is embedded or redistributed.
"""
import argparse
import struct

from fontTools.ttLib import TTFont


def outline(font,character):
    if font['head'].unitsPerEm!=2048:
        raise ValueError('Expected 2048 font units per em')
    name=font.getBestCmap()[ord(character)]
    glyph=font['glyf'][name]
    coordinates,ends,flags=glyph.getCoordinates(font['glyf'])
    points=[(round(x),round(y)) for x,y in coordinates]
    xmin,ymin=min(x for x,y in points),min(y for x,y in points)
    xmax,ymax=max(x for x,y in points),max(y for x,y in points)
    result=bytearray(struct.pack('<HH6hH',0x12,len(ends),xmax-xmin,ymax-ymin,
                                 xmin,ymax,xmin-1024,1760-ymax,len(points)))
    for end in ends[:-1]:
        result.extend(struct.pack('<B' if len(points)<=255 else '<H',end))
    for start in range(0,len(points),8):
        result.append(sum((flags[i]&1)<<(i-start) for i in range(start,min(start+8,len(points)))))
    runs=bytearray([0x00])  # Initial absolute x/y are both signed 16-bit values.
    remaining=len(points)-1
    while remaining:
        count=min(remaining,64)
        runs.append(count-1)  # No high bits: both x/y deltas use signed 16-bit values.
        remaining-=count
    result.extend(struct.pack('<H',len(runs)))
    result.extend(runs)
    previous=(0,0)
    for x,y in points:
        result.extend(struct.pack('<hh',x-previous[0],y-previous[1]))
        previous=x,y
    return bytes(result)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font',required=True)
    parser.add_argument('--font-number',type=int,default=0)
    args=parser.parse_args()
    font=TTFont(args.font,fontNumber=args.font_number)
    for character,name in (('仪','instrument_simplified'),('儀','instrument_traditional')):
        data=outline(font,character)
        print(f'    /* {character}: {len(data)} bytes, converted from a local TrueType outline. */')
        print(f'    static const unsigned char {name}[]={{')
        for offset in range(0,len(data),16):
            print('        '+','.join(f'0x{byte:02x}' for byte in data[offset:offset+16])+',')
        print('    };')
