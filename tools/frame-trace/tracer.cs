// frame-trace: a minimal Win32 debugger that measures how often chosen functions
// of GEX3D.EXE (EU, 32-bit) run, without patching a single byte of the target.
//
// Why hardware breakpoints: an INT3 (0xCC) plus single-step round trip re-enters
// the debugger in the middle of the game's timer callbacks and the process dies
// with 0xC000041D / 0xC0000005 before the first gameplay frame. The x86 debug
// registers Dr0-Dr3 fire on execution of an address and leave the code intact,
// so the game keeps running normally. The cost is four addresses per session.
//
// Build (x64 host, WOW64 target):
//   %WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe /nologo /platform:x64 ^
//     /optimize+ /out:tracer.exe tracer.cs
//
// Usage:
//   tracer.exe <exe> <workdir> <log> <timeoutMs> hw:<addr,...> <keyScript|none>
//
//   addr      image-relative address in hex (e.g. 43e1ba), rebased automatically
//   keyScript comma separated virtual-key codes in hex, sent 8 s apart (0D Enter,
//             1B Escape, 28 Down, 20 Space)
//
// Output (TSV, one line per hit):
//   BP  seq  ms  tid  addr  esp  ret  a0  a1  a2
// where `ret` is the return address pushed by the call and a0..a2 are the first
// three stack arguments. tools/frame-trace/summarize.mjs reads this format.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public static class Tracer
{
    const uint DEBUG_ONLY_THIS_PROCESS = 0x00000002;
    const uint DBG_CONTINUE = 0x00010002;
    const uint DBG_EXCEPTION_NOT_HANDLED = 0x80010001;
    const uint EXCEPTION_BREAKPOINT = 0x80000003;
    const uint EXCEPTION_SINGLE_STEP = 0x80000004;
    const uint THREAD_ALL_ACCESS = 0x1F03FF;
    const uint CONTEXT_i386 = 0x00010000;
    const uint CONTEXT_FLAGS = CONTEXT_i386 | 0x00000007 | 0x00000010;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct STARTUPINFO
    {
        public int cb; public string lpReserved; public string lpDesktop; public string lpTitle;
        public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
        public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public uint dwProcessId, dwThreadId; }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    struct WOW64_FLOATING_SAVE_AREA
    {
        public uint ControlWord, StatusWord, TagWord, ErrorOffset, ErrorSelector, DataOffset, DataSelector;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 80)] public byte[] RegisterArea;
        public uint Cr0NpxState;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    struct WOW64_CONTEXT
    {
        public uint ContextFlags;
        public uint Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
        public WOW64_FLOATING_SAVE_AREA FloatSave;
        public uint SegGs, SegFs, SegEs, SegDs;
        public uint Edi, Esi, Ebx, Edx, Ecx, Eax;
        public uint Ebp, Eip, SegCs, EFlags, Esp, SegSs;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)] public byte[] ExtendedRegisters;
    }

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool CreateProcessW(string app, string cmd, IntPtr pa, IntPtr ta, bool inherit,
        uint flags, IntPtr env, string cwd, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
    [DllImport("kernel32", SetLastError = true)]
    static extern bool WaitForDebugEvent(IntPtr de, uint ms);
    [DllImport("kernel32", SetLastError = true)]
    static extern bool ContinueDebugEvent(uint pid, uint tid, uint status);
    [DllImport("kernel32", SetLastError = true)]
    static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int size, out IntPtr read);
    [DllImport("kernel32", SetLastError = true)]
    static extern bool Wow64GetThreadContext(IntPtr hThread, ref WOW64_CONTEXT ctx);
    [DllImport("kernel32", SetLastError = true)]
    static extern bool Wow64SetThreadContext(IntPtr hThread, ref WOW64_CONTEXT ctx);
    [DllImport("kernel32", SetLastError = true)]
    static extern IntPtr OpenThread(uint access, bool inherit, uint tid);
    [DllImport("kernel32", SetLastError = true)]
    static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32", SetLastError = true)]
    static extern bool TerminateProcess(IntPtr h, uint code);
    [DllImport("kernel32")]
    static extern void DebugActiveProcessStop(uint pid);
    [DllImport("user32", CharSet = CharSet.Unicode)]
    static extern bool EnumWindows(EnumProc cb, IntPtr lp);
    [DllImport("user32")]
    static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32")]
    static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32", CharSet = CharSet.Unicode)]
    static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32")]
    static extern bool PostMessageW(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
    delegate bool EnumProc(IntPtr h, IntPtr lp);

    static StreamWriter log;
    static IntPtr hProc;
    static uint pid;
    static uint delta;
    static uint[] hwAddrs = new uint[0];
    static readonly Dictionary<uint, int> hitCount = new Dictionary<uint, int>();
    static long seq = 0;
    static readonly Stopwatch clock = new Stopwatch();
    static string[] keyScript = new string[0];
    static int nextKey = 0;
    static long[] keyAt = new long[0];
    static IntPtr gameWindow = IntPtr.Zero;

    static void Log(string s) { log.WriteLine(s); log.Flush(); }

    static uint U32(IntPtr addr)
    {
        var b = new byte[4];
        IntPtr got;
        if (!ReadProcessMemory(hProc, addr, b, 4, out got)) return 0xFFFFFFFF;
        return BitConverter.ToUInt32(b, 0);
    }

    static void ApplyHw(IntPtr hThread)
    {
        if (hwAddrs.Length == 0) return;
        var ctx = new WOW64_CONTEXT();
        ctx.ContextFlags = CONTEXT_FLAGS;
        ctx.ExtendedRegisters = new byte[512];
        if (!Wow64GetThreadContext(hThread, ref ctx)) return;
        ctx.Dr0 = hwAddrs.Length > 0 ? hwAddrs[0] : 0;
        ctx.Dr1 = hwAddrs.Length > 1 ? hwAddrs[1] : 0;
        ctx.Dr2 = hwAddrs.Length > 2 ? hwAddrs[2] : 0;
        ctx.Dr3 = hwAddrs.Length > 3 ? hwAddrs[3] : 0;
        uint dr7 = 0;
        for (int i = 0; i < Math.Min(4, hwAddrs.Length); i++) dr7 |= 1u << (2 * i); // local enable, execute, 1 byte
        ctx.Dr7 = dr7;
        ctx.Dr6 = 0;
        Wow64SetThreadContext(hThread, ref ctx);
    }

    static IntPtr FindWindow(uint targetPid)
    {
        IntPtr found = IntPtr.Zero;
        EnumWindows((h, lp) =>
        {
            uint p;
            GetWindowThreadProcessId(h, out p);
            if (p == targetPid && IsWindowVisible(h))
            {
                // nGlide creates a D3DProxyWindow; the game window is the other one.
                var cn = new StringBuilder(128);
                GetClassNameW(h, cn, 128);
                if (cn.ToString() != "D3DProxyWindow") { found = h; return false; }
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    static void SendKey(uint vk)
    {
        if (gameWindow == IntPtr.Zero) gameWindow = FindWindow(pid);
        Log(string.Format("# key attempt vk=0x{0:X2} hwnd=0x{1:X}", vk, gameWindow.ToInt64()));
        if (gameWindow == IntPtr.Zero) return;
        PostMessageW(gameWindow, 0x0100, (IntPtr)vk, (IntPtr)1);
        Thread.Sleep(60);
        PostMessageW(gameWindow, 0x0101, (IntPtr)vk, (IntPtr)0xC0000001);
        Log(string.Format("# key vk=0x{0:X2}", vk));
    }

    public static int Main(string[] args)
    {
        if (args.Length < 5)
        {
            Console.Error.WriteLine("usage: tracer.exe <exe> <workdir> <log> <timeoutMs> hw:<addr,...> <keys|none>");
            return 1;
        }
        string exe = args[0], workdir = args[1], logPath = args[2];
        int timeout = int.Parse(args[3]);
        string spec = args[4];
        string list = spec.StartsWith("hw:") ? spec.Substring(3) : spec;
        var addrList = list == "none" ? new string[0] : list.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
        keyScript = (args.Length > 5 && args[5] != "none") ? args[5].Split(',') : new string[0];

        log = new StreamWriter(logPath, false, new UTF8Encoding(false));
        Log("# frame-trace (hardware breakpoints, no code patching)");
        Log("# exe=" + exe);
        Log("# workdir=" + workdir);
        Log("# started=" + DateTime.UtcNow.ToString("O"));

        var si = new STARTUPINFO(); si.cb = Marshal.SizeOf(typeof(STARTUPINFO));
        PROCESS_INFORMATION pi;
        if (!CreateProcessW(exe, null, IntPtr.Zero, IntPtr.Zero, false,
                DEBUG_ONLY_THIS_PROCESS, IntPtr.Zero, workdir, ref si, out pi))
        {
            Log("# CreateProcess failed: " + Marshal.GetLastWin32Error());
            return 2;
        }
        hProc = pi.hProcess; pid = pi.dwProcessId;
        Log("# pid=" + pid);
        clock.Start();

        keyAt = new long[keyScript.Length];
        for (int i = 0; i < keyScript.Length; i++) keyAt[i] = 8000 + i * 2000;

        IntPtr evBuf = Marshal.AllocHGlobal(512);
        long deadline = timeout;
        while (true)
        {
            long now = clock.ElapsedMilliseconds;
            if (nextKey < keyScript.Length && now >= keyAt[nextKey]) { SendKey(Convert.ToUInt32(keyScript[nextKey], 16)); nextKey++; }
            if (!WaitForDebugEvent(evBuf, 100))
            {
                if (clock.ElapsedMilliseconds > deadline) { Log("# timeout"); break; }
                continue;
            }

            uint evCode = (uint)Marshal.ReadInt32(evBuf, 0);
            uint evPid = (uint)Marshal.ReadInt32(evBuf, 4);
            uint evTid = (uint)Marshal.ReadInt32(evBuf, 8);
            const int u = 16;
            uint status = DBG_CONTINUE;
            bool stop = false;

            if (evCode == 3) // CREATE_PROCESS
            {
                IntPtr baseOfImage = Marshal.ReadIntPtr(evBuf, u + 24);
                IntPtr hThread = Marshal.ReadIntPtr(evBuf, u + 16);
                uint baseImage = (uint)baseOfImage.ToInt64();
                delta = baseImage - 0x00400000u;
                Log(string.Format("# image base=0x{0:X8}", baseImage));
                var listAddrs = new List<uint>();
                foreach (var s in addrList) listAddrs.Add(Convert.ToUInt32(s, 16) + delta);
                hwAddrs = listAddrs.ToArray();
                foreach (var a in hwAddrs) { hitCount[a] = 0; Log(string.Format("# hw bp 0x{0:X8}", a)); }
                ApplyHw(hThread);
            }
            else if (evCode == 2) // CREATE_THREAD
            {
                IntPtr hThread = Marshal.ReadIntPtr(evBuf, u);
                ApplyHw(hThread);
                Log("# thread " + evTid);
            }
            else if (evCode == 1) // EXCEPTION
            {
                uint code = (uint)Marshal.ReadInt32(evBuf, u);
                uint addr = (uint)Marshal.ReadIntPtr(evBuf, u + 16).ToInt64();
                uint firstChance = (uint)Marshal.ReadInt32(evBuf, u + 152);
                if (code == EXCEPTION_BREAKPOINT || code == 0x4000001F)
                {
                    status = DBG_CONTINUE; // loader / ntdll breakpoints: just continue
                }
                else if (code == EXCEPTION_SINGLE_STEP || code == 0x4000001E)
                {
                    IntPtr hThread = OpenThread(THREAD_ALL_ACCESS, false, evTid);
                    var ctx = new WOW64_CONTEXT();
                    ctx.ContextFlags = CONTEXT_FLAGS;
                    ctx.ExtendedRegisters = new byte[512];
                    if (Wow64GetThreadContext(hThread, ref ctx))
                    {
                        uint at = ctx.Eip;
                        if (hitCount.ContainsKey(at))
                        {
                            hitCount[at]++;
                            LogHit(at, ctx.Esp, evTid);
                            // RF: run the instruction once before the breakpoint can fire again
                            ctx.EFlags |= 0x10000u;
                            Wow64SetThreadContext(hThread, ref ctx);
                        }
                    }
                    CloseHandle(hThread);
                }
                else
                {
                    if (firstChance == 0)
                        Log(string.Format("# 2nd-chance 0x{0:X8} at 0x{1:X8}", code, addr));
                    status = DBG_EXCEPTION_NOT_HANDLED;
                }
            }
            else if (evCode == 5) // EXIT_PROCESS
            {
                uint exitCode = (uint)Marshal.ReadInt32(evBuf, u);
                Log("# exit code=" + exitCode);
                stop = true;
            }

            ContinueDebugEvent(evPid, evTid, status);
            if (stop) break;
            if (clock.ElapsedMilliseconds > timeout + 5000) break;
        }

        Log(string.Format("# hits={0}", seq));
        try { TerminateProcess(hProc, 0); } catch { }
        try { DebugActiveProcessStop(pid); } catch { }
        log.Close();
        return 0;
    }

    static void LogHit(uint at, uint esp, uint tid)
    {
        uint ret = U32((IntPtr)esp);
        uint a0 = U32((IntPtr)(esp + 4));
        uint a1 = U32((IntPtr)(esp + 8));
        uint a2 = U32((IntPtr)(esp + 12));
        seq++;
        Log(string.Format("BP\t{0}\t{1}\t{2}\t0x{3:X8}\t0x{4:X8}\t0x{5:X8}\t0x{6:X8}\t0x{7:X8}\t0x{8:X8}",
            seq, clock.ElapsedMilliseconds, tid, at, esp, ret, a0, a1, a2));
    }
}
