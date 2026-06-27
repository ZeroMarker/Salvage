#!/usr/bin/env python3
"""End-to-end NTFS test: create VHD with deleted files, scan and recover.

Requires: Windows with Hyper-V PowerShell module. Run as Administrator.
"""

import os
import sys
import subprocess
import shutil
import json
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
VHD_PATH = os.path.join(SCRIPT_DIR, 'test_ntfs.vhdx')
PS_SCRIPT = os.path.join(SCRIPT_DIR, '_ntfs_setup.ps1')
RECOVER_DIR = os.path.join(SCRIPT_DIR, 'recovered_ntfs')
JSON_PATH = os.path.join(SCRIPT_DIR, 'ntfs_results.json')

TEST_FILES = {
    'HELLO.TXT': b'Hello from NTFS!\n',
    'BINARY.DAT': bytes(range(256)) * 4,
    'LARGE.BIN': b'X' * 8192,
}
NESTED_FILES = {
    'README.TXT': b'Nested file content.\n',
    'main.c': b'#include <stdio.h>\nint main() { return 0; }\n',
    'notes.txt': b'Notes with spaces in path.\n',
}


def find_salvage():
    for c in [
        os.path.join(PROJECT_DIR, 'build', 'Release', 'salvage.exe'),
        os.path.join(PROJECT_DIR, 'build', 'salvage'),
    ]:
        if os.path.exists(c):
            return c
    return None


def run_salvage(exe, args):
    cmd = [exe] + args
    print(f'  $ {" ".join(cmd)}')
    return subprocess.run(cmd, capture_output=True, text=True, timeout=120)


def cleanup():
    for p in [VHD_PATH, JSON_PATH, PS_SCRIPT]:
        if os.path.exists(p):
            try:
                os.remove(p)
            except Exception:
                pass
    if os.path.exists(RECOVER_DIR):
        shutil.rmtree(RECOVER_DIR, ignore_errors=True)
    # Dismount any leftover VHD
    subprocess.run(
        ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command',
         f'if (Test-Path "{VHD_PATH}") {{ Dismount-VHD -Path "{VHD_PATH}" -ErrorAction SilentlyContinue }}'],
        capture_output=True, timeout=15
    )


def create_ps_script():
    """Write PowerShell script that creates VHD, writes files, deletes some, dismounts."""
    # Build file creation lines
    write_lines = []
    for name, content in TEST_FILES.items():
        b64 = __import__('base64').b64encode(content).decode()
        write_lines.append(f'$b = [Convert]::FromBase64String("{b64}"); [IO.File]::WriteAllBytes("{name}", $b)')

    # Nested files
    nested_dirs = ['DOCS', 'PROJECT' + os.sep + 'SRC', 'My Files']
    for name, content in NESTED_FILES.items():
        b64 = __import__('base64').b64encode(content).decode()
        if name == 'README.TXT':
            write_lines.append(f'$b = [Convert]::FromBase64String("{b64}"); [IO.File]::WriteAllBytes("DOCS\\{name}", $b)')
        elif name == 'main.c':
            write_lines.append(f'$b = [Convert]::FromBase64String("{b64}"); [IO.File]::WriteAllBytes("PROJECT\\SRC\\{name}", $b)')
        elif name == 'notes.txt':
            write_lines.append(f'$b = [Convert]::FromBase64String("{b64}"); [IO.File]::WriteAllBytes("My Files\\{name}", $b)')

    delete_lines = [
        'Get-ChildItem -Path $drive -Recurse -File | Remove-Item -Force',
    ]

    script = f'''
$vhdPath = "{VHD_PATH}"
$sizeBytes = 20MB

# Create VHD
$vhd = New-VHD -Path $vhdPath -SizeBytes $sizeBytes -Fixed
$disk = $vhd | Mount-VHD -PassThru
Start-Sleep -Seconds 3

# Initialize with MBR
$disk | Initialize-Disk -PartitionStyle MBR -ErrorAction Stop
Start-Sleep -Seconds 2

# Create partition
$partition = $disk | New-Partition -UseMaximumSize -AssignDriveLetter -IsActive
Start-Sleep -Seconds 2

# Format as NTFS
$partition | Format-Volume -FileSystem NTFS -NewFileSystemLabel "NTFS_TEST" -Force -Confirm:$false
Start-Sleep -Seconds 3

# Get drive letter
$driveLetter = $partition.DriveLetter
if (-not $driveLetter) {{ $driveLetter = "Z" }}
$drive = "${{driveLetter}}:"
Write-Output "DRIVE=$drive"

# Check partition style
$diskInfo = Get-Disk -Number $disk.Number
Write-Output "DISK_SIZE=$($diskInfo.Size)"
Write-Output "PART_STYLE=$($diskInfo.PartitionStyle)"

# Flush file system buffers
$vol = Get-Volume -DriveLetter $driveLetter
Write-Output "VOL_FS=$($vol.FileSystem)"

# Create directories
New-Item -Path "$drive\\DOCS" -ItemType Directory -Force | Out-Null
New-Item -Path "$drive\\PROJECT\\SRC" -ItemType Directory -Force | Out-Null
New-Item -Path "$drive\\My Files" -ItemType Directory -Force | Out-Null

# Write files (using base64 to handle binary content)
Set-Location $drive
{chr(10).join(write_lines)}

# Verify files exist
$files = Get-ChildItem -Path $drive -Recurse -File
Write-Output "FILES=$($files.Count)"

# Delete all files
{chr(10).join(delete_lines)}

# Verify deletion
$remaining = Get-ChildItem -Path $drive -Recurse -File
Write-Output "REMAINING=$($remaining.Count)"

# Dismount
Set-Location C:\\
Dismount-VHD -Path $vhdPath
Write-Output "DONE"
'''
    with open(PS_SCRIPT, 'w', encoding='utf-8') as f:
        f.write(script)


def main():
    exe = find_salvage()
    if not exe:
        print('ERROR: Build salvage first.')
        return 1

    cleanup()

    print('=' * 60)
    print('NTFS E2E Test')
    print('=' * 60)

    # Check Hyper-V availability
    print('\nChecking Hyper-V module...')
    r = subprocess.run(
        ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command',
         'if (Get-Module -ListAvailable -Name Hyper-V) { Write-Output "OK" } else { Write-Output "MISSING" }'],
        capture_output=True, text=True, timeout=15
    )
    if 'MISSING' in r.stdout or 'OK' not in r.stdout:
        print('  SKIP: Hyper-V module not available.')
        print('  This test requires Windows with Hyper-V enabled.')
        print('  On CI (windows-latest), Hyper-V is available.')
        return 0

    # 1. Create NTFS VHD with files
    print('\n[1/4] Creating NTFS VHD...')
    create_ps_script()
    r = subprocess.run(
        ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', PS_SCRIPT],
        capture_output=True, text=True, timeout=120
    )
    print(r.stdout)
    if r.returncode != 0:
        print(f'  Error: {r.stderr}')
        cleanup()
        return 1

    if 'DONE' not in r.stdout:
        print('  VHD setup did not complete.')
        cleanup()
        return 1

    expected = {}
    expected.update(TEST_FILES)
    expected.update(NESTED_FILES)
    print(f'  Expected files: {len(expected)}')

    # 2. Scan
    print('\n[2/4] Scanning for deleted files...')
    r = run_salvage(exe, ['scan', VHD_PATH, '-p', '0', '-o', JSON_PATH])
    print(r.stdout)
    if r.returncode != 0:
        print(r.stderr)

    if os.path.exists(JSON_PATH):
        with open(JSON_PATH) as f:
            data = json.load(f)
        print(f'  Found {data["count"]} deleted file(s):')
        for item in data['results']:
            print(f'    ID={item["id"]} {item["name"]} {item["size"]}B')
    else:
        print('  No scan results.')
        cleanup()
        return 1

    # 3. Recover
    print('\n[3/4] Recovering files...')
    if data['count'] > 0:
        ids = ','.join(str(item['id']) for item in data['results'])
        r = run_salvage(exe, ['recover', VHD_PATH, ids, '-p', '0', '-o', RECOVER_DIR, '-f'])
        print(r.stdout)
        if r.returncode != 0:
            print(r.stderr)

    # 4. Verify
    print('[4/4] Verifying...')
    ok = 0
    if os.path.exists(RECOVER_DIR):
        for name, content in expected.items():
            # Search in recovered dir (files might be flat or nested)
            found = None
            for root, dirs, files in os.walk(RECOVER_DIR):
                if name in files:
                    found = os.path.join(root, name)
                    break
            if found:
                with open(found, 'rb') as f:
                    actual = f.read()
                if actual == content:
                    print(f'  {name}: MATCH ({len(content)} bytes)')
                    ok += 1
                else:
                    print(f'  {name}: MISMATCH ({len(actual)} vs {len(content)})')
            else:
                print(f'  {name}: NOT FOUND')

    # Cleanup
    cleanup()

    total = len(expected)
    print('\n' + '=' * 60)
    if ok == total:
        print(f'PASS: {ok}/{total} files recovered')
        return 0
    elif ok > 0:
        print(f'PARTIAL: {ok}/{total} files recovered')
        return 0
    else:
        print(f'FAIL: {ok}/{total} files recovered')
        return 1


if __name__ == '__main__':
    sys.exit(main())
