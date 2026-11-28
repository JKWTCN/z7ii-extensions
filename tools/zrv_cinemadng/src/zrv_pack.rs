use std::io;

pub(super) const HEIGHT: usize = 1166;
pub(super) const STRIDE: usize = 3120;
const ACTIVE: usize = 3105;
const GROUPS: usize = (2070 - 2) / 16;
const TAIL: usize = 6;
const MIN_ROW: usize = 3 + GROUPS + TAIL;
const BYTE_MIN_ROW: usize = 3 + 2070 / 2 - 1;

pub(super) fn valid_size(bytes: u64, byte_deltas: bool) -> bool {
    let min_row = if byte_deltas { BYTE_MIN_ROW } else { MIN_ROW };
    (HEIGHT as u64 * (min_row + 2) as u64..=HEIGHT as u64 * (ACTIVE + 2) as u64).contains(&bytes)
}

fn invalid() -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, "ZRV 无损 RAW 行无效")
}

fn pair(data: &[u8]) -> (i32, i32) {
    (
        i32::from(data[0]) | (i32::from(data[1] & 15) << 8),
        i32::from(data[1] >> 4) | (i32::from(data[2]) << 4),
    )
}

fn unpack_byte_row(mut row: &[u8], output: &mut [u8]) -> io::Result<()> {
    output[..3].copy_from_slice(&row[..3]);
    let mut previous = u32::from_le_bytes([row[0], row[1], row[2], 0]);
    row = &row[3..];
    for pixel in output[3..ACTIVE].chunks_exact_mut(3) {
        let (&code, rest) = row.split_first().ok_or_else(invalid)?;
        row = rest;
        if code == 255 {
            let (literal, rest) = row.split_at_checked(3).ok_or_else(invalid)?;
            previous = u32::from_le_bytes([literal[0], literal[1], literal[2], 0]);
            row = rest;
        } else {
            let delta = u32::from(code & 15) + (u32::from(code & 240) << 8);
            previous = (previous + delta).wrapping_sub(0x8008) & 0xff_ffff;
        }
        pixel.copy_from_slice(&previous.to_le_bytes()[..3]);
    }
    if !row.is_empty() {
        return Err(invalid());
    }
    Ok(())
}

pub(super) fn unpack(mut encoded: &[u8], output: &mut [u8], byte_deltas: bool) -> io::Result<()> {
    if output.len() != STRIDE * HEIGHT {
        return Err(invalid());
    }
    let min_row = if byte_deltas { BYTE_MIN_ROW } else { MIN_ROW };
    for out in output.chunks_exact_mut(STRIDE) {
        let (length, rest) = encoded.split_at_checked(2).ok_or_else(invalid)?;
        let length = u16::from_le_bytes([length[0], length[1]]) as usize;
        if !(min_row..=ACTIVE).contains(&length) {
            return Err(invalid());
        }
        let (mut row, rest) = rest.split_at_checked(length).ok_or_else(invalid)?;
        encoded = rest;
        out[ACTIVE..].fill(0);
        if length == ACTIVE {
            out[..ACTIVE].copy_from_slice(row);
            continue;
        }
        if byte_deltas {
            unpack_byte_row(row, out)?;
            continue;
        }
        out[..3].copy_from_slice(&row[..3]);
        let (mut left, mut right) = pair(&row[..3]);
        row = &row[3..];
        for dest in out[3..ACTIVE - TAIL].chunks_exact_mut(24) {
            let (&bits, rest) = row.split_first().ok_or_else(invalid)?;
            if bits > 12 {
                return Err(invalid());
            }
            let (group, next) = rest
                .split_at_checked(2 * usize::from(bits))
                .ok_or_else(invalid)?;
            row = next;
            if bits == 12 {
                dest.copy_from_slice(group);
                (left, right) = pair(&group[21..]);
                continue;
            }
            let (mut word, mut used, mut cursor) = (0u32, 0u8, 0usize);
            let mask = (1 << bits) - 1;
            for pixel in dest.chunks_exact_mut(3) {
                while used < 2 * bits {
                    word |= u32::from(group[cursor]) << used;
                    cursor += 1;
                    used += 8;
                }
                let a = (word & mask) as i32;
                let b = ((word >> bits) & mask) as i32;
                word >>= 2 * bits;
                used -= 2 * bits;
                left += (a >> 1) ^ -(a & 1);
                right += (b >> 1) ^ -(b & 1);
                if !(0..=4095).contains(&left) || !(0..=4095).contains(&right) {
                    return Err(invalid());
                }
                pixel[0] = left as u8;
                pixel[1] = ((left >> 8) | ((right & 15) << 4)) as u8;
                pixel[2] = (right >> 4) as u8;
            }
        }
        if row.len() != TAIL {
            return Err(invalid());
        }
        out[ACTIVE - TAIL..ACTIVE].copy_from_slice(row);
    }
    if !encoded.is_empty() {
        return Err(invalid());
    }
    Ok(())
}
