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
//              [dump:<arg>:<off>,...] [dumpabs:<base>:<off>,...]
//              [dumpind:<arg>:<ptrOff>:<off>,...] [dumpraw:<arg>:<off>:<len>]
//              [dumpabsraw:<base>:<len>] [regs]
//
// Opciones de volcado (solo lectura, nunca se escribe en el proceso):
//   dump:<arg>:<off>,...      dwords en (argumento <arg> + <off>); <arg> 0..2
//                             son a0, a1 y a2 (las tres primeras palabras de
//                             la pila en el punto de interrupcion)
//   dumpabs:<base>:<off>,...  dwords en la direccion absoluta <base>; se puede
//                             repetir separando bloques con ';'
//   dumpind:<arg>:<ptrOff>:<off>,...
//                             dwords en la direccion contenida en
//                             (argumento <arg> + <ptrOff>), mas <off>
//   dumpraw:<arg>:<off>:<len>  bytes contiguos en (argumento <arg> + <off>)
//   dumpabsraw:<base>:<len>    bytes contiguos en la direccion <base>
//   dumpabsind:<ptrAddr>:<len> bytes contiguos en la direccion guardada en
//                             <ptrAddr> (puntero global)
//   dumpstack:<off>,...        dwords en (esp + <off>) del punto de interrupcion
//   dumpindraw:<arg>:<ptrOff>:<len>
//                             bytes contiguos en la direccion contenida en
//                             (argumento <arg> + <ptrOff>)
//   dumpstr:<arg>:<off>:<max>  cadena terminada en NUL en (argumento <arg> + <off>)
//   regs                      vuelca los registros del hilo en el punto
//                             de interrupcion (eax..ebp, esp)
//   dumplimit:N               numero maximo de bloques volcados por impacto
//                             (4 por defecto)
//
// Salida: una linea BP por impacto (como antes) y, si se piden, lineas
// MEM / ABS / IND / RAW / RAWABS / REG. summarize.mjs sigue leyendo BP.
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

    // --- extensiones de volcado (solo lectura) ---
    static int dumpArg = -1;                        // 0..2 -> a0, a1, a2
    static readonly List<uint> dumpOffs = new List<uint>();
    static readonly List<int> dumpArgs = new List<int>();      // un bloque por opcion dump:
    static readonly List<uint[]> dumpOffSets = new List<uint[]>();
    static readonly List<uint> dumpAbsBases = new List<uint>();
    static readonly List<uint[]> dumpAbsOffSets = new List<uint[]>();
    static int dumpIndArg = -1;
    static readonly List<uint> dumpIndPtrOffs = new List<uint>();
    static readonly List<uint[]> dumpIndOffSets = new List<uint[]>();
    static int dumpRawArg = -1;                      // arg de dumpraw:
    static uint dumpRawBase = 0;                   // desplazamiento de dumpraw:
    static uint dumpRawLen = 0;                     // longitud de dumpraw:
    static uint dumpAbsRawBase = 0;                // base de dumpabsraw:
    static uint dumpAbsRawLen = 0;                 // longitud de dumpabsraw:
    static uint dumpAbsIndPtrAddr = 0;             // puntero en direccion absoluta
    static uint dumpAbsIndLen = 0;                 // longitud de dumpabsind:
    static bool wantRegs = false;
    static readonly List<uint> dumpStackOffs = new List<uint>();  // dwords en (esp+off)
    static int dumpIndRawArg = -1;                              // lectura contigua por indireccion
    static uint dumpIndRawPtrOff = 0;
    static uint dumpIndRawLen = 0;
    static int dumpStrArg = -1;                                 // cadena terminada en NUL
    static uint dumpStrOff = 0;
    static uint dumpStrMax = 0;
    static int dumpLimit = 4;                      // volcados por impacto
    static WOW64_CONTEXT lastCtx;                  // contexto del impacto

    static void Log(string s) { log.WriteLine(s); log.Flush(); }

    static uint U32(IntPtr addr)
    {
        var b = new byte[4];
        IntPtr got;
        if (!ReadProcessMemory(hProc, addr, b, 4, out got)) return 0xFFFFFFFF;
        return BitConverter.ToUInt32(b, 0);
    }

    // Lee un bloque contiguo de bytes (solo lectura). Devuelve null si la
    // memoria no es legible.
    static byte[] Block(uint addr, int len)
    {
        var b = new byte[len];
        IntPtr got;
        if (!ReadProcessMemory(hProc, (IntPtr)addr, b, len, out got)) return null;
        return b;
    }

    static string HexBlock(byte[] b)
    {
        if (b == null) return "<ilegible>";
        var sb = new StringBuilder();
        for (int i = 0; i < b.Length; i++)
        {
            if (i > 0) sb.Append(' ');
            sb.Append(b[i].ToString("X2"));
        }
        return sb.ToString();
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

        for (int i = 6; i < args.Length; i++)
        {
            string opt = args[i];
            if (opt == "regs") { wantRegs = true; continue; }
            if (opt.StartsWith("dumplimit:"))
            {
                dumpLimit = int.Parse(opt.Substring(10));
                continue;
            }
            if (opt.StartsWith("dump:"))
            {
                string[] parts = opt.Substring(5).Split(':');
                dumpArg = int.Parse(parts[0]);
                dumpArgs.Add(dumpArg);
                var offs = new List<uint>();
                foreach (var s in parts[1].Split(',')) offs.Add(Convert.ToUInt32(s, 16));
                dumpOffs.AddRange(offs);
                dumpOffSets.Add(offs.ToArray());
            }
            else if (opt.StartsWith("dumpabs:"))
            {
                foreach (var blk in opt.Substring(8).Split(';'))
                {
                    string[] parts = blk.Split(':');
                    dumpAbsBases.Add(Convert.ToUInt32(parts[0], 16));
                    var offs = new List<uint>();
                    foreach (var s in parts[1].Split(',')) offs.Add(Convert.ToUInt32(s, 16));
                    dumpAbsOffSets.Add(offs.ToArray());
                }
            }
            else if (opt.StartsWith("dumpind:"))
            {
                string[] parts = opt.Substring(8).Split(':');
                dumpIndArg = int.Parse(parts[0]);
                dumpIndPtrOffs.Add(Convert.ToUInt32(parts[1], 16));
                var offs = new List<uint>();
                foreach (var s in parts[2].Split(',')) offs.Add(Convert.ToUInt32(s, 16));
                dumpIndOffSets.Add(offs.ToArray());
            }
            else if (opt.StartsWith("dumpraw:"))
            {
                string[] parts = opt.Substring(8).Split(':');
                dumpRawArg = int.Parse(parts[0]);
                dumpRawBase = Convert.ToUInt32(parts[1], 16);
                dumpRawLen = Convert.ToUInt32(parts[2], 16);
            }
            else if (opt.StartsWith("dumpstack:"))
            {
                foreach (var s in opt.Substring(10).Split(',')) dumpStackOffs.Add(Convert.ToUInt32(s, 16));
            }
            else if (opt.StartsWith("dumpindraw:"))
            {
                string[] parts = opt.Substring(11).Split(':');
                dumpIndRawArg = int.Parse(parts[0]);
                dumpIndRawPtrOff = Convert.ToUInt32(parts[1], 16);
                dumpIndRawLen = Convert.ToUInt32(parts[2], 16);
            }
            else if (opt.StartsWith("dumpstr:"))
            {
                string[] parts = opt.Substring(8).Split(':');
                dumpStrArg = int.Parse(parts[0]);
                dumpStrOff = Convert.ToUInt32(parts[1], 16);
                dumpStrMax = Convert.ToUInt32(parts[2], 16);
            }
            else if (opt.StartsWith("dumpabsind:"))
            {
                string[] parts = opt.Substring(11).Split(':');
                dumpAbsIndPtrAddr = Convert.ToUInt32(parts[0], 16);
                dumpAbsIndLen = Convert.ToUInt32(parts[1], 16);
            }
            else if (opt.StartsWith("dumpabsraw:"))
            {
                string[] parts = opt.Substring(11).Split(':');
                dumpAbsRawBase = Convert.ToUInt32(parts[0], 16);
                dumpAbsRawLen = Convert.ToUInt32(parts[1], 16);
            }
            else
            {
                Console.Error.WriteLine("argumento desconocido: " + opt);
                return 1;
            }
        }

        log = new StreamWriter(logPath, false, new UTF8Encoding(false));
        Log("# frame-trace (hardware breakpoints, no code patching)");
        Log("# exe=" + exe);
        Log("# workdir=" + workdir);
        Log("# started=" + DateTime.UtcNow.ToString("O"));
        if (dumpArg >= 0) Log("# dump arg=" + dumpArg + " offs=" + dumpOffs.Count);
        if (dumpAbsBases.Count > 0) Log("# dumpabs bases=" + dumpAbsBases.Count);
        if (dumpIndArg >= 0) Log("# dumpind arg=" + dumpIndArg + " blocks=" + dumpIndOffSets.Count);
        if (dumpRawLen > 0) Log("# dumpraw off=0x" + dumpRawBase.ToString("X8") + " len=" + dumpRawLen);
        if (dumpAbsRawLen > 0) Log("# dumpabsraw base=0x" + dumpAbsRawBase.ToString("X8") + " len=" + dumpAbsRawLen);
        if (dumpAbsIndLen > 0) Log("# dumpabsind ptraddr=0x" + dumpAbsIndPtrAddr.ToString("X8") + " len=" + dumpAbsIndLen);
        if (wantRegs) Log("# regs");
        if (dumpStackOffs.Count > 0) Log("# dumpstack offs=" + dumpStackOffs.Count);
        if (dumpIndRawArg >= 0) Log("# dumpindraw arg=" + dumpIndRawArg + " len=" + dumpIndRawLen);
        if (dumpStrArg >= 0) Log("# dumpstr arg=" + dumpStrArg + " max=" + dumpStrMax);

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
                            lastCtx = ctx;
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

        if (wantRegs)
        {
            Log(string.Format("REG\t{0}\t{1}\teax=0x{2:X8}\tebx=0x{3:X8}\tecx=0x{4:X8}\tedx=0x{5:X8}\tesi=0x{6:X8}\tedi=0x{7:X8}\tebp=0x{8:X8}\tesp=0x{9:X8}",
                seq, clock.ElapsedMilliseconds, lastCtx.Eax, lastCtx.Ebx, lastCtx.Ecx,
                lastCtx.Edx, lastCtx.Esi, lastCtx.Edi, lastCtx.Ebp, lastCtx.Esp));
        }

        if (dumpStackOffs.Count > 0)
        {
            var sb = new StringBuilder();
            sb.Append(string.Format("STK\t{0}\t{1}\tesp=0x{2:X8}", seq, clock.ElapsedMilliseconds, esp));
            foreach (uint off in dumpStackOffs)
                sb.Append(string.Format("\t+0x{0:X}=0x{1:X8}", off, U32((IntPtr)(esp + off))));
            Log(sb.ToString());
        }

        if (dumpStrArg >= 0)
        {
            uint argp = dumpStrArg == 0 ? a0 : (dumpStrArg == 1 ? a1 : a2);
            if (argp != 0 && argp != 0xFFFFFFFF)
            {
                uint addr = argp + dumpStrOff;
                byte[] b = Block(addr, (int)dumpStrMax);
                if (b != null)
                {
                    int n = 0;
                    while (n < b.Length && b[n] != 0) n++;
                    string s = Encoding.ASCII.GetString(b, 0, n);
                    Log(string.Format("STR\t{0}\t{1}\taddr=0x{2:X8}\t{3}", seq, clock.ElapsedMilliseconds, addr, s));
                }
            }
        }

        if (dumpIndRawArg >= 0)
        {
            uint argp = dumpIndRawArg == 0 ? a0 : (dumpIndRawArg == 1 ? a1 : a2);
            uint ptr = U32((IntPtr)(argp + dumpIndRawPtrOff));
            if (ptr != 0 && ptr != 0xFFFFFFFF && dumpIndRawLen > 0)
            {
                dumpLimit--;
                Log(string.Format("INDRAW\t{0}\t{1}\tptr=0x{2:X8}\tlen={3}\t{4}",
                    seq, clock.ElapsedMilliseconds, ptr, dumpIndRawLen,
                    HexBlock(Block(ptr, (int)dumpIndRawLen))));
            }
        }

        if (dumpLimit <= 0) return;

        for (int bi = 0; bi < dumpArgs.Count; bi++)
        {
            if (dumpLimit <= 0) break;
            uint ptr = dumpArgs[bi] == 0 ? a0 : (dumpArgs[bi] == 1 ? a1 : a2);
            if (ptr == 0 || ptr == 0xFFFFFFFF) continue;
            dumpLimit--;
            var sb = new StringBuilder();
            sb.Append(string.Format("MEM\t{0}\t{1}\tptr=0x{2:X8}", seq, clock.ElapsedMilliseconds, ptr));
            foreach (uint off in dumpOffSets[bi])
                sb.Append(string.Format("\t+0x{0:X}=0x{1:X8}", off, U32((IntPtr)(ptr + off))));
            Log(sb.ToString());
        }

        for (int bi = 0; bi < dumpAbsBases.Count; bi++)
        {
            if (dumpLimit <= 0) break;
            dumpLimit--;
            uint bse = dumpAbsBases[bi];
            uint[] boffs = dumpAbsOffSets[bi];
            var sb = new StringBuilder();
            sb.Append(string.Format("ABS\t{0}\t{1}\tbase=0x{2:X8}", seq, clock.ElapsedMilliseconds, bse));
            foreach (uint off in boffs)
                sb.Append(string.Format("\t+0x{0:X}=0x{1:X8}", off, U32((IntPtr)(bse + off))));
            Log(sb.ToString());
        }

        if (dumpIndArg >= 0)
        {
            uint argp = dumpIndArg == 0 ? a0 : (dumpIndArg == 1 ? a1 : a2);
            for (int bi = 0; bi < dumpIndPtrOffs.Count; bi++)
            {
                if (dumpLimit <= 0) break;
                uint ptr = U32((IntPtr)(argp + dumpIndPtrOffs[bi]));
                if (ptr == 0 || ptr == 0xFFFFFFFF) continue;
                dumpLimit--;
                var sb = new StringBuilder();
                sb.Append(string.Format("IND\t{0}\t{1}\tptr=0x{2:X8}", seq, clock.ElapsedMilliseconds, ptr));
                foreach (uint off in dumpIndOffSets[bi])
                    sb.Append(string.Format("\t+0x{0:X}=0x{1:X8}", off, U32((IntPtr)(ptr + off))));
                Log(sb.ToString());
            }
        }

        if (dumpRawLen > 0)
        {
            uint argp = dumpRawArg == 0 ? a0 : (dumpRawArg == 1 ? a1 : a2);
            if (argp != 0 && argp != 0xFFFFFFFF)
            {
                dumpLimit--;
                Log(string.Format("RAW\t{0}\t{1}\tbase=0x{2:X8}\tlen={3}\t{4}",
                    seq, clock.ElapsedMilliseconds, argp + dumpRawBase, dumpRawLen,
                    HexBlock(Block(argp + dumpRawBase, (int)dumpRawLen))));
            }
        }

        if (dumpAbsIndLen > 0 && dumpAbsIndPtrAddr != 0)
        {
            uint ptr = U32((IntPtr)dumpAbsIndPtrAddr);
            if (ptr != 0 && ptr != 0xFFFFFFFF)
            {
                dumpLimit--;
                Log(string.Format("RAWABSIND\t{0}\t{1}\tptr=0x{2:X8}\tlen={3}\t{4}",
                    seq, clock.ElapsedMilliseconds, ptr, dumpAbsIndLen,
                    HexBlock(Block(ptr, (int)dumpAbsIndLen))));
            }
        }

        if (dumpAbsRawLen > 0)
        {
            dumpLimit--;
            Log(string.Format("RAWABS\t{0}\t{1}\tbase=0x{2:X8}\tlen={3}\t{4}",
                seq, clock.ElapsedMilliseconds, dumpAbsRawBase, dumpAbsRawLen,
                HexBlock(Block(dumpAbsRawBase, (int)dumpAbsRawLen))));
        }
    }
}
