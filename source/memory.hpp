// SPDX-License-Identifier: AGPL-3.0-or-later
// Never dereference an unvalidated retry-chain pointer.
#if defined(CPU_REMATCH_HOST_TEST)
extern "C" U32 cpu_rematch_query_memory(Addr, Addr*, Addr*, U32*);
#else
static U32 cpu_rematch_query_memory(Addr address, Addr* base, Addr* size, U32* permissions) {
    struct MemoryInfo {
        U64 base, size;
        U32 type, attributes, permission, ipc_references, device_references, padding;
    } info = {};
    static_assert(sizeof(MemoryInfo) == 40, "QueryMemory ABI");
    register U64 x0 asm("x0") = reinterpret_cast<U64>(&info);
    register U64 x1 asm("x1");
    register U64 x2 asm("x2") = address;
    asm volatile("svc #0x6" : "+r"(x0), "=r"(x1), "+r"(x2) : : "memory", "cc");
    if (U32(x0)) return U32(x0);
    *base = info.base; *size = info.size; *permissions = info.permission;
    return 0;
}
#endif

static bool mapped(Addr address, size_t length, U32 needed = 1) {
    if (!address || !length) return false;
    Addr base = 0, size = 0;
    U32 permissions = 0;
    if (cpu_rematch_query_memory(address, &base, &size, &permissions)) return false;
    if ((permissions & needed) != needed || address < base || length > size) return false;
    return address - base <= size - length;
}
