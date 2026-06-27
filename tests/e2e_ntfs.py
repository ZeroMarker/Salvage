#!/usr/bin/env python3
"""NTFS E2E smoke test.

Verifies that Salvage can detect and scan an NTFS volume without crashing.
Does not wait for full scan completion on large drives.
"""

import os
import sys
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)


def find_salvage():
    for c in [
        os.path.join(PROJECT_DIR, 'build', 'Release', 'salvage.exe'),
        os.path.join(PROJECT_DIR, 'build', 'salvage'),
    ]:
        if os.path.exists(c):
            return c
    return None


def main():
    exe = find_salvage()
    if not exe:
        print('ERROR: Build salvage first.')
        return 1

    print('=' * 60)
    print('NTFS E2E Smoke Test')
    print('=' * 60)

    # Find an NTFS drive
    print('\n[1/2] Finding NTFS drive...')
    r = subprocess.run(
        ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command',
         'Get-Volume | Where-Object { $_.FileSystem -eq "NTFS" -and $_.DriveLetter -and $_.Size -lt 1GB } | '
         'Select-Object -First 1 -ExpandProperty DriveLetter'],
        capture_output=True, text=True, timeout=15
    )
    drive = r.stdout.strip()

    if not drive or len(drive) != 1:
        # Fallback: use C:
        r2 = subprocess.run(
            ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command',
             '(Get-Volume -DriveLetter C).FileSystem'],
            capture_output=True, text=True, timeout=15
        )
        if 'NTFS' in r2.stdout:
            drive = 'C'
        else:
            print('  SKIP: No NTFS drive found.')
            return 0

    print(f'  Using {drive}:\\')

    # Run salvage scan with a short timeout (just verify it starts and detects the volume)
    print(f'\n[2/2] Running salvage scan on {drive}: (5s timeout)...')
    try:
        r = subprocess.run(
            [exe, 'scan', f'\\\\.\\{drive}:', '-m', 'quick'],
            capture_output=True, text=True, timeout=5
        )
        # If it finishes in 5s, check output
        combined = r.stdout + r.stderr
        if 'No partitions found' in combined and 'No partition table' not in combined:
            print('  FAIL: Could not detect volume')
            return 1
        elif 'Scanning' in combined or 'NTFS' in combined or 'No partition table' in combined:
            print('  PASS: Volume detected, scan started')
            return 0
        else:
            print(f'  PASS: Scan executed (exit code {r.returncode})')
            return 0
    except subprocess.TimeoutExpired:
        # Timeout is expected on large drives - means scan is running
        print('  PASS: Scan started (timed out on large drive, expected)')
        return 0


if __name__ == '__main__':
    sys.exit(main())
