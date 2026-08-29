param(
    [string]$SignatureHex = "93 F8 9B 0C 97 04 B0 F7 3B FB AC FF 27 01 5B FD 24 09 00 00",
    [uint32]$EmulatedSignatureAddress = 0x00138A20,
    [uint32]$EmulatedDumpAddress = 0x00138488,
    [uint32]$DumpLength = 3004,
    [string]$OutputPath = ".local/verification/create_player/runtime/nopsx-records.bin"
)

$ErrorActionPreference = "Stop"

if (-not ("NoPsxReadOnlyMemory" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;

public static class NoPsxReadOnlyMemory
{
    const uint PROCESS_VM_READ = 0x0010;
    const uint PROCESS_QUERY_INFORMATION = 0x0400;
    const uint MEM_COMMIT = 0x1000;
    const uint PAGE_GUARD = 0x100;
    const uint PAGE_NOACCESS = 0x01;

    [StructLayout(LayoutKind.Sequential)]
    struct MEMORY_BASIC_INFORMATION
    {
        public IntPtr BaseAddress;
        public IntPtr AllocationBase;
        public uint AllocationProtect;
        public UIntPtr RegionSize;
        public uint State;
        public uint Protect;
        public uint Type;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr OpenProcess(uint access, bool inheritHandle, int processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool ReadProcessMemory(IntPtr process, IntPtr address,
        [Out] byte[] buffer, UIntPtr size, out UIntPtr bytesRead);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern UIntPtr VirtualQueryEx(IntPtr process, IntPtr address,
        out MEMORY_BASIC_INFORMATION info, UIntPtr length);

    [DllImport("kernel32.dll")]
    static extern bool CloseHandle(IntPtr handle);

    static int Find(byte[] data, int length, byte[] pattern)
    {
        int last = length - pattern.Length;
        for (int i = 0; i <= last; ++i) {
            int j = 0;
            while (j < pattern.Length && data[i + j] == pattern[j]) ++j;
            if (j == pattern.Length) return i;
        }
        return -1;
    }

    public static long[] FindPatterns(int processId, byte[] pattern)
    {
        if (pattern == null || pattern.Length == 0)
            throw new ArgumentException("signature cannot be empty");
        IntPtr process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                     false, processId);
        if (process == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error());
        try {
            List<long> matches = new List<long>();
            long address = 0;
            int mbiSize = Marshal.SizeOf(typeof(MEMORY_BASIC_INFORMATION));
            while (address < 0x80000000L) {
                MEMORY_BASIC_INFORMATION info;
                UIntPtr queried = VirtualQueryEx(process, new IntPtr(address),
                    out info, new UIntPtr((uint)mbiSize));
                if (queried == UIntPtr.Zero) break;
                long baseAddress = info.BaseAddress.ToInt64();
                long regionSize = unchecked((long)info.RegionSize.ToUInt64());
                if (regionSize <= 0) break;
                bool readable = info.State == MEM_COMMIT &&
                    (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
                if (readable) {
                    const int chunkSize = 4 * 1024 * 1024;
                    long consumed = 0;
                    byte[] tail = new byte[Math.Max(0, pattern.Length - 1)];
                    int tailLength = 0;
                    while (consumed < regionSize) {
                        int requested = (int)Math.Min(chunkSize, regionSize - consumed);
                        byte[] chunk = new byte[tailLength + requested];
                        if (tailLength != 0) Buffer.BlockCopy(tail, 0, chunk, 0, tailLength);
                        UIntPtr got;
                        bool ok = ReadProcessMemory(process,
                            new IntPtr(baseAddress + consumed), chunk,
                            new UIntPtr((uint)requested), out got);
                        int read = ok ? checked((int)got.ToUInt64()) : 0;
                        int available = tailLength + read;
                        int searchStart = 0;
                        while (searchStart <= available - pattern.Length) {
                            byte[] remaining = new byte[available - searchStart];
                            Buffer.BlockCopy(chunk, searchStart, remaining, 0,
                                             remaining.Length);
                            int found = Find(remaining, remaining.Length, pattern);
                            if (found < 0) break;
                            long match = baseAddress + consumed - tailLength +
                                         searchStart + found;
                            if (matches.Count == 0 || matches[matches.Count - 1] != match)
                                matches.Add(match);
                            searchStart += found + 1;
                        }
                        tailLength = Math.Min(tail.Length, available);
                        if (tailLength != 0)
                            Buffer.BlockCopy(chunk, available - tailLength, tail, 0, tailLength);
                        consumed += requested;
                    }
                }
                long next = baseAddress + regionSize;
                if (next <= address) break;
                address = next;
            }
            return matches.ToArray();
        }
        finally { CloseHandle(process); }
    }

    public static byte[] Read(int processId, long address, int length)
    {
        IntPtr process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                     false, processId);
        if (process == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error());
        try {
            byte[] result = new byte[length];
            UIntPtr got;
            if (!ReadProcessMemory(process, new IntPtr(address), result,
                                   new UIntPtr((uint)length), out got) ||
                got.ToUInt64() != (ulong)length)
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "could not read complete no$psx runtime block");
            return result;
        }
        finally { CloseHandle(process); }
    }
}
'@
}

$signature = [byte[]](($SignatureHex -split "\s+") | ForEach-Object {
    [Convert]::ToByte($_, 16)
})
$processes = Get-Process | Where-Object {
    $_.Path -eq "F:\Games\PS1\no`$psx\NO`$PSX.EXE"
}
if (-not $processes) {
    throw "no no`$psx process is running"
}

$matches = @()
foreach ($process in $processes) {
    $processMatches = [NoPsxReadOnlyMemory]::FindPatterns($process.Id, $signature)
    if ($processMatches.Count -eq 0) {
        Write-Host "[SNAPSHOT] pid=$($process.Id) synchronized signature absent"
        continue
    }
    Write-Host "[SNAPSHOT] pid=$($process.Id) signature-candidates=$($processMatches.Count)"
    foreach ($match in $processMatches) {
        $ramBase = $match - [int64]$EmulatedSignatureAddress
        try {
            # Reject GTE/cache copies of the signature. A true candidate must
            # expose the complete contiguous 2 MiB PS1 main-RAM image.
            $ram = [NoPsxReadOnlyMemory]::Read($process.Id, $ramBase, 0x200000)
        }
        catch {
            continue
        }
        $signatureMatches = $true
        for ($i = 0; $i -lt $signature.Count; ++$i) {
            if ($ram[$EmulatedSignatureAddress + $i] -ne $signature[$i]) {
                $signatureMatches = $false
                break
            }
        }
        if (-not $signatureMatches -or
            $EmulatedDumpAddress + $DumpLength -gt $ram.Count) {
            continue
        }
        $bytes = [byte[]]::new($DumpLength)
        [Array]::Copy($ram, [int]$EmulatedDumpAddress, $bytes, 0, [int]$DumpLength)
        $matches += [pscustomobject]@{
            Process = $process
            Match = $match
            RamBase = $ramBase
            Bytes = $bytes
        }
    }
}

if ($matches.Count -ne 1) {
    throw "expected one synchronized no`$psx session, found $($matches.Count)"
}

$resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
$parent = Split-Path -Parent $resolvedOutput
[IO.Directory]::CreateDirectory($parent) | Out-Null
[IO.File]::WriteAllBytes($resolvedOutput, $matches[0].Bytes)
Write-Host ("[SNAPSHOT] pid={0} signature-host=0x{1:X} ram-host=0x{2:X}" -f `
    $matches[0].Process.Id, $matches[0].Match, $matches[0].RamBase)
Write-Host ("[SNAPSHOT] emulated=0x{0:X8} bytes={1} output={2}" -f `
    (0x80000000L + $EmulatedDumpAddress), $DumpLength, $resolvedOutput)
