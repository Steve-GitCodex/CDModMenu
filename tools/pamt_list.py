import struct
import sys
from pathlib import Path


def name_from_blob(blob, off):
    parts = []
    while off != 0xFFFFFFFF and len(parts) < 64:
        if off + 5 > len(blob):
            break
        parent, ln = struct.unpack_from("<IB", blob, off)
        parts.append(blob[off + 5:off + 5 + ln].decode("latin-1"))
        off = parent
    return "".join(reversed(parts))


def list_chunk(chunk_dir):
    d = (chunk_dir / "0.pamt").read_bytes()
    paz_count = struct.unpack_from("<I", d, 4)[0]
    o = 12 + paz_count * 12
    dir_sz = struct.unpack_from("<I", d, o)[0]; o += 4
    dir_blob = d[o:o + dir_sz]; o += dir_sz
    file_sz = struct.unpack_from("<I", d, o)[0]; o += 4
    file_blob = d[o:o + file_sz]; o += file_sz
    n_dirs = struct.unpack_from("<I", d, o)[0]; o += 4
    dirs = [struct.unpack_from("<IIII", d, o + i * 16) for i in range(n_dirs)]; o += n_dirs * 16
    n_files = struct.unpack_from("<I", d, o)[0]; o += 4
    files = [struct.unpack_from("<IIIIHBB", d, o + i * 20) for i in range(n_files)]
    for _, name_off, first, count in dirs:
        path = name_from_blob(dir_blob, name_off)
        for i in range(first, min(first + count, n_files)):
            f = files[i]
            yield path, name_from_blob(file_blob, f[0]), f[1], f[2], f[3], f[4], f[5], f[6]


def main():
    root = Path(sys.argv[1])
    needle = sys.argv[2].lower() if len(sys.argv) > 2 else ""
    for chunk_dir in sorted(p for p in root.iterdir() if p.is_dir() and p.name.isdigit()):
        if not (chunk_dir / "0.pamt").exists():
            continue
        for path, name, off, csize, usize, paz, comp, flag in list_chunk(chunk_dir):
            full = f"{path}/{name}"
            if needle in full.lower():
                print(f"{chunk_dir.name}\t{full}\tpaz={paz} off={off} csize={csize} usize={usize} comp={comp} flag={flag}")


if __name__ == "__main__":
    main()
