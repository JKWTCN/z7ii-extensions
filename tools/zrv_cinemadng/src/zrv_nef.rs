use std::io;

const LENGTHS: [u8; 13] = [5, 5, 4, 3, 3, 2, 3, 3, 4, 5, 6, 7, 7];
const CODES: [u8; 13] = [30, 28, 12, 4, 2, 0, 3, 5, 13, 29, 62, 126, 127];

const fn prefixes() -> [(u8, u8); 128] {
    let mut table = [(0, 0); 128];
    let mut i = 0;
    while i < CODES.len() {
        let shift = 7 - LENGTHS[i];
        let begin = (CODES[i] as usize) << shift;
        let mut prefix = begin;
        while prefix < begin + (1 << shift) {
            table[prefix] = (LENGTHS[i], i as u8);
            prefix += 1;
        }
        i += 1;
    }
    table
}

const PREFIXES: [(u8, u8); 128] = prefixes();

pub fn unpack(encoded: &[u8], packed: &mut [u8], info: super::ClipInfo) -> io::Result<()> {
    let invalid = || super::invalid("ZRV 原厂 NEF 无损码流无效");
    if packed.len() != info.packed_bytes() {
        return Err(invalid());
    }
    let mut bytes = encoded.iter().copied();
    let (mut window, mut available) = (0u32, 0u8);
    let mut vertical = [[512i32; 2]; 2];
    for (y, row) in packed.chunks_exact_mut(info.stride as usize).enumerate() {
        let (samples, padding) = row.split_at_mut(info.width as usize * 3 / 2);
        let mut horizontal = vertical[y & 1];
        for (x, pair) in samples.chunks_exact_mut(3).enumerate() {
            for predictor in &mut horizontal {
                while available < 19 {
                    let Some(byte) = bytes.next() else { break };
                    window = (window << 8) | u32::from(byte);
                    available += 8;
                }
                let lookahead = if available >= 7 {
                    window >> (available - 7)
                } else {
                    window << (7 - available)
                };
                let (length, category) = PREFIXES[(lookahead & 127) as usize];
                if length == 0 || length + category > available {
                    return Err(invalid());
                }
                available -= length + category;
                let mask = (1 << category) - 1;
                let value = ((window >> available) as i32) & mask;
                *predictor += if category != 0 && value < 1 << (category - 1) {
                    value - mask
                } else {
                    value
                };
                if !(0..=4095).contains(predictor) {
                    return Err(invalid());
                }
            }
            if x == 0 {
                vertical[y & 1] = horizontal;
            }
            let [a, b] = horizontal;
            pair.copy_from_slice(&[a as u8, ((a >> 8) | (b << 4)) as u8, (b >> 4) as u8]);
        }
        padding.fill(0);
    }
    if available >= 8 || bytes.next().is_some() {
        return Err(invalid());
    }
    Ok(())
}
