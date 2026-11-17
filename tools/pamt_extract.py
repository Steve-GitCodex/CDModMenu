import sys
from pathlib import Path

from pamt_list import list_chunk


def lz4_block(src, usize):
    dst = bytearray()
    i = 0
    while i < len(src):
        token = src[i]; i += 1
        lit = token >> 4
        if lit == 15:
            while True:
                b = src[i]; i += 1
                lit += b
                if b != 255:
                    break
        dst += src[i:i + lit]; i += lit
        if i >= len(src) or len(dst) >= usize:
            break
        off = src[i] | (src[i + 1] << 8); i += 2
        ml = token & 15
        if ml == 15:
            while True:
                b = src[i]; i += 1
                ml += b
                if b != 255:
                    break
        ml += 4
        start = len(dst) - off
        for k in range(ml):
            dst.append(dst[start + k])
    return bytes(dst)


def main():
    root, chunk, wanted, out_dir = Path(sys.argv[1]), sys.argv[2], sys.argv[3], Path(sys.argv[4])
    out_dir.mkdir(parents=True, exist_ok=True)
    for path, name, off, csize, usize, paz, comp, flag in list_chunk(root / chunk):
        full = f"{path}/{name}"
        if wanted not in full:
            continue
        with open(root / chunk / f"{paz}.paz", "rb") as f:
            f.seek(off)
            raw = f.read(csize)
        method = comp & 0x0F
        data = lz4_block(raw, usize) if method == 2 else raw
        dest = out_dir / full.replace("/", "__")
        dest.write_bytes(data)
        print(f"{full} comp={comp} -> {dest} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
