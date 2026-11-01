// SPDX-License-Identifier: AGPL-3.0-or-later
// Included inside the plugin's anonymous namespace, after resolve().
#if defined(CPU_REMATCH_HOST_TEST)
extern "C" void cpu_rematch_debug_write(const char*, size_t);
#endif

static void debug_output_raw(const char* message, size_t size) {
#if defined(CPU_REMATCH_HOST_TEST)
    cpu_rematch_debug_write(message, size);
#else
    register U64 x0 asm("x0") = reinterpret_cast<U64>(message);
    register U64 x1 asm("x1") = size;
    asm volatile("svc #0x27" : "+r"(x0), "+r"(x1) : : "memory", "cc");
#endif
}

struct LogHandle { void* handle; };
struct LogWriteOption { int flags; };

static void log(const char* message, bool truncate = false) {
    // Bounded diagnostic output: this is a test build, not a frame-by-frame trace.
    static unsigned lines = 0;
    if (truncate) lines = 0;
    if (++lines > 512) return;
    char line[512];
    const char prefix[] = "[CPU-REMATCH-DIAG-v05] ";
    size_t n = 0;
    for (size_t i = 0; prefix[i]; ++i) line[n++] = prefix[i];
    for (size_t i = 0; message[i] && n + 1 < sizeof(line); ++i) line[n++] = message[i];
    line[n++] = '\n';
    debug_output_raw(line, n); // Independent of libc and the network logger.

    using Open = U32 (*)(LogHandle*, const char*, int);
    using Create = U32 (*)(const char*, int64_t);
    using Resize = U32 (*)(LogHandle, int64_t);
    using Size = U32 (*)(int64_t*, LogHandle);
    using Write = U32 (*)(LogHandle, int64_t, const void*, U64, const LogWriteOption*);
    using Close = void (*)(LogHandle);
    Open open = nullptr; Create create = nullptr; Resize resize = nullptr;
    Size get_size = nullptr; Write write = nullptr; Close close = nullptr;
    // Use the native Switch filesystem API, as Skyline does, not fopen.
    if (!resolve(open, "_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci") ||
        !resolve(create, "_ZN2nn2fs10CreateFileEPKcl") ||
        !resolve(resize, "_ZN2nn2fs11SetFileSizeENS0_10FileHandleEl") ||
        !resolve(get_size, "_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE") ||
        !resolve(write, "_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE") ||
        !resolve(close, "_ZN2nn2fs9CloseFileENS0_10FileHandleE")) return;
    const char* path = "sd:/cpu_rematch.log";
    LogHandle file = {};
    U32 result = open(&file, path, 6); // Write + allow extension
    if (result) {
        create(path, 0);
        result = open(&file, path, 6);
    }
    if (result) return;
    int64_t offset = 0;
    result = truncate ? resize(file, 0) : get_size(&offset, file);
    if (!result) {
        LogWriteOption option{1}; // Flush this small record immediately.
        write(file, offset, line, n, &option);
    }
    close(file);
}

static void log_value(const char* label, U64 value) {
    char out[256];
    size_t n = 0;
    while (*label && n < sizeof(out) - 20) out[n++] = *label++;
    out[n++] = '='; out[n++] = '0'; out[n++] = 'x';
    const char hex[] = "0123456789abcdef";
    for (int i = 60; i >= 0; i -= 4) out[n++] = hex[(value >> i) & 15];
    out[n] = '\0';
    log(out);
}

template<class T> static bool required(T& out, const char* name) {
    if (resolve(out, name)) return true;
    log("Missing required symbol:");
    log(name);
    return false;
}
