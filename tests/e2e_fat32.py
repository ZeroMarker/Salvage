#!/usr/bin/env python3
"""End-to-end FAT32 test: subdirectories, fragmented files, scan and recover."""

import struct
import os
import sys
import subprocess
import shutil
import json

SECTOR_SIZE = 512
CLUSTER_SIZE = 4096
SECTORS_PER_CLUSTER = CLUSTER_SIZE // SECTOR_SIZE
IMAGE_SIZE = 2 * 1024 * 1024  # 2 MB - smaller, enough for test
TOTAL_SECTORS = IMAGE_SIZE // SECTOR_SIZE
RESERVED_SECTORS = 32
NUM_FATS = 2
FAT_SIZE_SECTORS = 8
DATA_START_SECTOR = RESERVED_SECTORS + NUM_FATS * FAT_SIZE_SECTORS
ROOT_CLUSTER = 2
PARTITION_START_SECTOR = 2048


def le16(v): return struct.pack('<H', v)
def le32(v): return struct.pack('<I', v)
def ple16(buf, off, v): struct.pack_into('<H', buf, off, v)
def ple32(buf, off, v): struct.pack_into('<I', buf, off, v)


def make_boot_sector():
    b = bytearray(SECTOR_SIZE)
    b[0:3] = b'\xEB\x58\x90'
    b[3:11] = b'MSWIN4.1'
    ple16(b, 0x0B, SECTOR_SIZE)
    b[0x0D] = SECTORS_PER_CLUSTER
    ple16(b, 0x0E, RESERVED_SECTORS)
    b[0x10] = NUM_FATS
    b[0x15] = 0xF8
    ple32(b, 0x1C, PARTITION_START_SECTOR)
    ple32(b, 0x20, TOTAL_SECTORS - PARTITION_START_SECTOR)
    ple32(b, 0x24, FAT_SIZE_SECTORS)
    ple32(b, 0x2C, ROOT_CLUSTER)
    ple16(b, 0x30, 1)
    b[0x42] = 0x29
    ple32(b, 0x43, 0xAABBCCDD)
    b[0x47:0x52] = b'TEST       '
    b[0x52:0x5A] = b'FAT32   '
    ple16(b, 0x1FE, 0x55AA)
    return b


def make_mbr():
    mbr = bytearray(SECTOR_SIZE)
    pe = 446
    mbr[pe + 4] = 0x0B
    ple32(mbr, pe + 8, PARTITION_START_SECTOR)
    ple32(mbr, pe + 12, TOTAL_SECTORS - PARTITION_START_SECTOR)
    ple16(mbr, 510, 0x55AA)
    return mbr


def make_fs_info():
    i = bytearray(SECTOR_SIZE)
    ple32(i, 0, 0x41615252)
    ple32(i, 484, 0x61417272)
    ple32(i, 488, 0xFFFFFFFF)
    ple32(i, 492, 5)
    ple16(i, 510, 0x55AA)
    return i


class FatImage:
    """Helper to build a FAT32 image with proper cluster allocation."""

    def __init__(self):
        self.image = bytearray(IMAGE_SIZE)
        self.next_free = 3  # clusters 0,1 reserved, 2=root
        self.fat1_off = (PARTITION_START_SECTOR + RESERVED_SECTORS) * SECTOR_SIZE
        self.fat2_off = (PARTITION_START_SECTOR + RESERVED_SECTORS + FAT_SIZE_SECTORS) * SECTOR_SIZE
        self.data_off = (PARTITION_START_SECTOR + DATA_START_SECTOR) * SECTOR_SIZE

    def cluster_offset(self, c):
        return self.data_off + (c - 2) * CLUSTER_SIZE

    def alloc_cluster(self):
        c = self.next_free
        self.next_free += 1
        return c

    def alloc_specific(self, c):
        """Reserve a specific cluster number (for fragmentation)."""
        while self.next_free <= c:
            self.next_free = c + 1
        return c

    def fat_set(self, cluster, value):
        ple32(self.image, self.fat1_off + cluster * 4, value)
        ple32(self.image, self.fat2_off + cluster * 4, value)

    def write_cluster(self, cluster, data):
        off = self.cluster_offset(cluster)
        end = min(off + len(data), len(self.image))
        self.image[off:end] = data[:end - off]

    def write_file_clusters(self, cluster_chain, data):
        """Write data across a chain of clusters."""
        pos = 0
        for i, c in enumerate(cluster_chain):
            chunk = data[pos:pos + CLUSTER_SIZE]
            self.write_cluster(c, chunk)
            pos += CLUSTER_SIZE
            if i < len(cluster_chain) - 1:
                self.fat_set(c, cluster_chain[i + 1])
            else:
                self.fat_set(c, 0x0FFFFFFF)

    def build_mbr_and_boot(self):
        self.image[0:SECTOR_SIZE] = make_mbr()
        boot = make_boot_sector()
        off = PARTITION_START_SECTOR * SECTOR_SIZE
        self.image[off:off + SECTOR_SIZE] = boot
        info = make_fs_info()
        self.image[off + SECTOR_SIZE:off + 2 * SECTOR_SIZE] = info
        self.image[off + 6 * SECTOR_SIZE:off + 7 * SECTOR_SIZE] = boot
        # FAT reserved entries
        self.fat_set(0, 0x0FFFFFF8)
        self.fat_set(1, 0x0FFFFFFF)
        self.fat_set(ROOT_CLUSTER, 0x0FFFFFFF)

    def build_lfn_entries(self, name, short_name, cluster, size, is_dir=False):
        """Build LFN + short dir entry bytes."""
        entries = bytearray()
        cksum = lfn_checksum(short_name)
        if isinstance(name, bytes):
            chars = [c for c in name] + [0]
        else:
            chars = [ord(c) for c in name] + [0]
        num_lfn = (len(chars) + 12) // 13
        for i in range(num_lfn - 1, -1, -1):
            order = i + 1
            if i == num_lfn - 1:
                order |= 0x40
            chunk = chars[i * 13:(i + 1) * 13]
            while len(chunk) < 13:
                chunk.append(0)
            entries += make_lfn_entry(order, chunk, cksum)
        # Short entry
        entry = bytearray(32)
        entry[0:11] = short_name
        entry[0x0B] = 0x10 if is_dir else 0x20
        ple16(entry, 0x1A, cluster & 0xFFFF)
        ple16(entry, 0x14, (cluster >> 16) & 0xFFFF)
        ple32(entry, 0x1C, size)
        ple16(entry, 0x10, 0x5621)
        ple16(entry, 0x0E, 0x8000)
        ple16(entry, 0x18, 0x5621)
        ple16(entry, 0x16, 0x8000)
        entries += entry
        return entries

    def write_dir(self, cluster, entries_bytes):
        off = self.cluster_offset(cluster)
        self.image[off:off + len(entries_bytes)] = entries_bytes

    def mark_deleted(self, dir_cluster, name_prefix=b""):
        """Mark all non-LFN, non-label, non-dot entries in a dir as deleted."""
        off = self.cluster_offset(dir_cluster)
        pos = off
        while pos < off + CLUSTER_SIZE:
            b = self.image[pos]
            if b == 0x00:
                break
            if b == 0xE5:
                pos += 32
                continue
            attr = self.image[pos + 0x0B]
            if attr in (0x08, 0x0F):
                pos += 32
                continue
            # Skip . and .. entries
            if self.image[pos] == 0x2E and (self.image[pos+1] == 0x20 or self.image[pos+1] == 0x2E):
                pos += 32
                continue
            self.image[pos] = 0xE5
            pos += 32

    def save(self, path):
        with open(path, 'wb') as f:
            f.write(self.image)


def lfn_checksum(short_name):
    s = 0
    for b in short_name:
        s = ((s >> 1) + ((s & 1) << 7) + b) & 0xFF
    return s


def make_short_name(name):
    parts = name.split(b'.')
    base = parts[0][:8].ljust(8, b' ')
    ext = parts[1][:3].ljust(3, b' ') if len(parts) > 1 else b'   '
    return base + ext


def make_lfn_entry(order, chars, cksum):
    e = bytearray(32)
    e[0] = order
    e[0x0B] = 0x0F
    e[0x0D] = cksum
    offsets = [1, 14, 28]
    lengths = [5, 6, 2]
    idx = 0
    for p in range(3):
        base = offsets[p]
        for i in range(lengths[p]):
            c = chars[idx] if idx < len(chars) else 0xFFFF
            if c == 0 and idx >= len(chars):
                c = 0xFFFF
            ple16(e, base + i * 2, c)
            idx += 1
    return e


def run_salvage(exe, args):
    cmd = [exe] + args
    print(f"  $ {' '.join(cmd)}")
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    return r


def check(result, recover_dir, expected_files):
    """Verify recovered files match expectations."""
    ok = 0
    for fname, expected_content in expected_files.items():
        fpath = os.path.join(recover_dir, fname)
        if not os.path.exists(fpath):
            print(f"    {fname}: NOT FOUND")
            continue
        with open(fpath, 'rb') as f:
            actual = f.read()
        if actual == expected_content:
            print(f"    {fname}: MATCH ({len(expected_content)} bytes)")
            ok += 1
        else:
            print(f"    {fname}: MISMATCH (expected {len(expected_content)}, got {len(actual)})")
    return ok


def build_scenario(image_path):
    """Build image with: root files + subdir files + fragmented file."""
    img = FatImage()
    img.build_mbr_and_boot()

    root_dir = bytearray()
    # Volume label
    label = bytearray(32)
    label[0:11] = b'TEST       '
    label[0x0B] = 0x08
    root_dir += label

    expected = {}

    # --- Scenario 1: Simple file in root (cluster 3) ---
    content1 = b"Hello from root!\n"
    c1 = img.alloc_cluster()
    img.write_file_clusters([c1], content1)
    root_dir += img.build_lfn_entries(b'HELLO.TXT', make_short_name(b'HELLO.TXT'), c1, len(content1))
    expected['HELLO.TXT'] = content1

    # --- Scenario 2: Subdirectory with file ---
    # Directory cluster for DOCS
    dir_docs = img.alloc_cluster()
    root_dir += img.build_lfn_entries(b'DOCS', make_short_name(b'DOCS'), dir_docs, 0, is_dir=True)

    # File inside DOCS (cluster 5)
    content2 = b"Document inside subdirectory.\nMore lines here.\n"
    c2 = img.alloc_cluster()
    img.write_file_clusters([c2], content2)

    docs_dir = bytearray()
    # . and .. entries
    dot = bytearray(32)
    dot[0:11] = b'.          '
    dot[0x0B] = 0x10
    ple16(dot, 0x1A, dir_docs & 0xFFFF)
    docs_dir += dot
    dotdot = bytearray(32)
    dotdot[0:11] = b'..         '
    dotdot[0x0B] = 0x10
    ple16(dotdot, 0x1A, ROOT_CLUSTER & 0xFFFF)
    docs_dir += dotdot
    docs_dir += img.build_lfn_entries(b'README.TXT', make_short_name(b'README.TXT'), c2, len(content2))
    img.write_dir(dir_docs, docs_dir)
    img.fat_set(dir_docs, 0x0FFFFFFF)
    expected['README.TXT'] = content2

    # --- Scenario 3: Fragmented file (clusters 6, 9, 12 - non-contiguous) ---
    # First allocate some filler clusters to force fragmentation
    filler_a = img.alloc_cluster()  # 6
    filler_b = img.alloc_cluster()  # 7
    filler_c = img.alloc_cluster()  # 8

    frag_c1 = img.alloc_specific(9)   # 9
    # filler
    filler_d = img.alloc_specific(10)  # 10
    filler_e = img.alloc_specific(11)  # 11

    frag_c2 = img.alloc_specific(12)  # 12
    filler_f = img.alloc_specific(13)  # 13

    frag_c3 = img.alloc_specific(14)  # 14

    # Write fillers with dummy data
    for fc in [filler_a, filler_b, filler_c, filler_d, filler_e, filler_f]:
        img.write_file_clusters([fc], b'\x00' * CLUSTER_SIZE)

    # Fragmented content: 3 clusters, non-contiguous
    frag_content = bytes(range(256)) * 48  # 12288 bytes = 3 clusters
    frag_chain = [frag_c1, frag_c2, frag_c3]
    img.write_file_clusters(frag_chain, frag_content)
    root_dir += img.build_lfn_entries(b'FRAG.DAT', make_short_name(b'FRAG.DAT'), frag_c1, len(frag_content))
    expected['FRAG.DAT'] = frag_content

    # --- Scenario 4: Multi-cluster file in subdirectory ---
    dir_data = img.alloc_cluster()
    root_dir += img.build_lfn_entries(b'DATA', make_short_name(b'DATA'), dir_data, 0, is_dir=True)

    content4 = b"X" * 8192 + b"Y" * 4096  # 12288 bytes, 3 clusters
    mc_chain = [img.alloc_cluster() for _ in range(3)]
    img.write_file_clusters(mc_chain, content4)

    data_dir = bytearray()
    ddot = bytearray(32)
    ddot[0:11] = b'.          '
    ddot[0x0B] = 0x10
    ple16(ddot, 0x1A, dir_data & 0xFFFF)
    data_dir += ddot
    ddot2 = bytearray(32)
    ddot2[0:11] = b'..         '
    ddot2[0x0B] = 0x10
    ple16(ddot2, 0x1A, ROOT_CLUSTER & 0xFFFF)
    data_dir += ddot2
    data_dir += img.build_lfn_entries(b'BIG.BIN', make_short_name(b'BIG.BIN'), mc_chain[0], len(content4))
    img.write_dir(dir_data, data_dir)
    img.fat_set(dir_data, 0x0FFFFFFF)
    expected['BIG.BIN'] = content4

    # --- Scenario 5: Deep nesting (3 levels) ---
    # ROOT -> PROJECT -> SRC -> DEEP.TXT
    dir_l1 = img.alloc_cluster()
    root_dir += img.build_lfn_entries(b'PROJECT', make_short_name(b'PROJECT'), dir_l1, 0, is_dir=True)

    dir_l2 = img.alloc_cluster()
    l1_dir = bytearray()
    l1_dot = bytearray(32); l1_dot[0:11] = b'.          '; l1_dot[0x0B] = 0x10; ple16(l1_dot, 0x1A, dir_l1 & 0xFFFF)
    l1_ddot = bytearray(32); l1_ddot[0:11] = b'..         '; l1_ddot[0x0B] = 0x10; ple16(l1_ddot, 0x1A, ROOT_CLUSTER & 0xFFFF)
    l1_dir += l1_dot + l1_ddot
    l1_dir += img.build_lfn_entries(b'SRC', make_short_name(b'SRC'), dir_l2, 0, is_dir=True)
    img.write_dir(dir_l1, l1_dir)
    img.fat_set(dir_l1, 0x0FFFFFFF)

    dir_l3 = img.alloc_cluster()
    l2_dir = bytearray()
    l2_dot = bytearray(32); l2_dot[0:11] = b'.          '; l2_dot[0x0B] = 0x10; ple16(l2_dot, 0x1A, dir_l2 & 0xFFFF)
    l2_ddot = bytearray(32); l2_ddot[0:11] = b'..         '; l2_ddot[0x0B] = 0x10; ple16(l2_ddot, 0x1A, dir_l1 & 0xFFFF)
    l2_dir += l2_dot + l2_ddot
    l2_dir += img.build_lfn_entries(b'INCLUDE', make_short_name(b'INCLUDE'), dir_l3, 0, is_dir=True)
    img.write_dir(dir_l2, l2_dir)
    img.fat_set(dir_l2, 0x0FFFFFFF)

    content5 = b"Deep nested file content at level 3.\n"
    c5 = img.alloc_cluster()
    img.write_file_clusters([c5], content5)

    l3_dir = bytearray()
    l3_dot = bytearray(32); l3_dot[0:11] = b'.          '; l3_dot[0x0B] = 0x10; ple16(l3_dot, 0x1A, dir_l3 & 0xFFFF)
    l3_ddot = bytearray(32); l3_ddot[0:11] = b'..         '; l3_ddot[0x0B] = 0x10; ple16(l3_ddot, 0x1A, dir_l2 & 0xFFFF)
    l3_dir += l3_dot + l3_ddot
    l3_dir += img.build_lfn_entries(b'DEEP.TXT', make_short_name(b'DEEP.TXT'), c5, len(content5))
    img.write_dir(dir_l3, l3_dir)
    img.fat_set(dir_l3, 0x0FFFFFFF)
    expected['DEEP.TXT'] = content5

    # --- Scenario 6: Directory with spaces and special chars ---
    # "My Files" directory with "notes & ideas.txt" inside
    dir_spaces = img.alloc_cluster()
    root_dir += img.build_lfn_entries(b'My Files', make_short_name(b'MYFILES'), dir_spaces, 0, is_dir=True)

    content6 = b"Notes with spaces and special chars: !@#$%^&*()\n"
    c6 = img.alloc_cluster()
    img.write_file_clusters([c6], content6)

    sp_dir = bytearray()
    sp_dot = bytearray(32); sp_dot[0:11] = b'.          '; sp_dot[0x0B] = 0x10; ple16(sp_dot, 0x1A, dir_spaces & 0xFFFF)
    sp_ddot = bytearray(32); sp_ddot[0:11] = b'..         '; sp_ddot[0x0B] = 0x10; ple16(sp_ddot, 0x1A, ROOT_CLUSTER & 0xFFFF)
    sp_dir += sp_dot + sp_ddot
    sp_dir += img.build_lfn_entries(b'notes & ideas.txt', make_short_name(b'NOTES&I.TXT'), c6, len(content6))
    img.write_dir(dir_spaces, sp_dir)
    img.fat_set(dir_spaces, 0x0FFFFFFF)
    expected['notes & ideas.txt'] = content6

    # --- Scenario 7: File with Unicode-like long name in nested dir ---
    # "Backup (2024)" dir with "résumé.doc" file
    dir_paren = img.alloc_cluster()
    root_dir += img.build_lfn_entries(b'Backup (2024)', make_short_name(b'BACKUP~1'), dir_paren, 0, is_dir=True)

    content7 = b"Resume content with special naming.\n"
    c7 = img.alloc_cluster()
    img.write_file_clusters([c7], content7)

    bp_dir = bytearray()
    bp_dot = bytearray(32); bp_dot[0:11] = b'.          '; bp_dot[0x0B] = 0x10; ple16(bp_dot, 0x1A, dir_paren & 0xFFFF)
    bp_ddot = bytearray(32); bp_ddot[0:11] = b'..         '; bp_ddot[0x0B] = 0x10; ple16(bp_ddot, 0x1A, ROOT_CLUSTER & 0xFFFF)
    bp_dir += bp_dot + bp_ddot
    bp_dir += img.build_lfn_entries(b'resume.doc', make_short_name(b'RESUME.DOC'), c7, len(content7))
    img.write_dir(dir_paren, bp_dir)
    img.fat_set(dir_paren, 0x0FFFFFFF)
    expected['resume.doc'] = content7

    # --- Scenario 8: Deep nesting with spaces: "My Documents/Work Files/report final.txt" ---
    dir_md = img.alloc_cluster()
    root_dir += img.build_lfn_entries(b'My Documents', make_short_name(b'MYDOCUM~1'), dir_md, 0, is_dir=True)

    dir_wf = img.alloc_cluster()
    md_dir = bytearray()
    md_dot = bytearray(32); md_dot[0:11] = b'.          '; md_dot[0x0B] = 0x10; ple16(md_dot, 0x1A, dir_md & 0xFFFF)
    md_ddot = bytearray(32); md_ddot[0:11] = b'..         '; md_ddot[0x0B] = 0x10; ple16(md_ddot, 0x1A, ROOT_CLUSTER & 0xFFFF)
    md_dir += md_dot + md_ddot
    md_dir += img.build_lfn_entries(b'Work Files', make_short_name(b'WORKFI~1'), dir_wf, 0, is_dir=True)
    img.write_dir(dir_md, md_dir)
    img.fat_set(dir_md, 0x0FFFFFFF)

    content8 = b"Final report with spaces in path.\n"
    c8 = img.alloc_cluster()
    img.write_file_clusters([c8], content8)

    wf_dir = bytearray()
    wf_dot = bytearray(32); wf_dot[0:11] = b'.          '; wf_dot[0x0B] = 0x10; ple16(wf_dot, 0x1A, dir_wf & 0xFFFF)
    wf_ddot = bytearray(32); wf_ddot[0:11] = b'..         '; wf_ddot[0x0B] = 0x10; ple16(wf_ddot, 0x1A, dir_md & 0xFFFF)
    wf_dir += wf_dot + wf_ddot
    wf_dir += img.build_lfn_entries(b'report final.txt', make_short_name(b'REPORT~1.TXT'), c8, len(content8))
    img.write_dir(dir_wf, wf_dir)
    img.fat_set(dir_wf, 0x0FFFFFFF)
    expected['report final.txt'] = content8

    # Write root directory
    img.write_dir(ROOT_CLUSTER, root_dir)

    # --- Delete files ---
    # Delete root files
    img.mark_deleted(ROOT_CLUSTER)
    # Delete files in subdirs (mark subdir entries as deleted)
    img.mark_deleted(dir_docs)
    img.mark_deleted(dir_data)
    # Delete deep nested files
    img.mark_deleted(dir_l3)
    # Delete special char dirs
    img.mark_deleted(dir_spaces)
    img.mark_deleted(dir_paren)
    img.mark_deleted(dir_wf)

    img.save(image_path)
    return expected


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)

    # Find executable (Windows: build/Release/salvage.exe, Linux: build/salvage)
    candidates = [
        os.path.join(project_dir, 'build', 'Release', 'salvage.exe'),
        os.path.join(project_dir, 'build', 'salvage'),
        os.path.join(project_dir, 'build', 'salvage.exe'),
    ]
    exe = None
    for c in candidates:
        if os.path.exists(c):
            exe = c
            break
    if not exe:
        print("ERROR: Build salvage first.")
        return 1

    image_path = os.path.join(script_dir, 'test_fat32.img')
    recover_dir = os.path.join(script_dir, 'recovered')
    json_path = os.path.join(script_dir, 'scan_results.json')

    print("=" * 60)
    print("FAT32 E2E Test: Subdirectories + Fragmented Files")
    print("=" * 60)

    # Clean up
    for p in [image_path, json_path]:
        if os.path.exists(p):
            os.remove(p)
    if os.path.exists(recover_dir):
        shutil.rmtree(recover_dir)

    # 1. Build image
    print("\n[1/4] Building test image...")
    expected = build_scenario(image_path)
    print(f"  Image: {IMAGE_SIZE // 1024} KB, {len(expected)} files to recover")
    for name in expected:
        print(f"    {name} ({len(expected[name])} bytes)")

    # 2. Scan
    print("\n[2/4] Scanning for deleted files...")
    r = run_salvage(exe, ['scan', image_path, '-p', '0', '-o', json_path])
    print(r.stdout)
    if r.returncode != 0:
        print(r.stderr)
        print("  SCAN FAILED")
        return 1

    with open(json_path) as f:
        data = json.load(f)
    found = {r['name']: r for r in data['results']}
    print(f"  Found {data['count']} deleted file(s):")
    for r in data['results']:
        print(f"    ID={r['id']} {r['name']} {r['size']}B conf={r['confidence']}")

    # 3. Recover all
    print("\n[3/4] Recovering all files...")
    ids = ','.join(str(r['id']) for r in data['results'])
    r = run_salvage(exe, ['recover', image_path, ids, '-p', '0', '-o', recover_dir, '-f'])
    print(r.stdout)
    if r.returncode != 0:
        print(r.stderr)

    # 4. Verify
    print("[4/4] Verifying recovered files...")
    ok = check(None, recover_dir, expected)

    # Cleanup
    for p in [image_path, json_path]:
        if os.path.exists(p):
            os.remove(p)
    if os.path.exists(recover_dir):
        shutil.rmtree(recover_dir)

    print("\n" + "=" * 60)
    total = len(expected)
    if ok == total:
        print(f"PASS: {ok}/{total} files recovered and verified")
        print("=" * 60)
        return 0
    else:
        print(f"FAIL: {ok}/{total} files verified")
        print("=" * 60)
        return 1


if __name__ == '__main__':
    sys.exit(main())
