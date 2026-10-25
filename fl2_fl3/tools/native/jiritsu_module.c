/* jiritsu_module.c -- minimal freestanding AArch64 module for the Jiritsu channel.
 *
 * Build one variant per tool, e.g.
 *     clang --target=aarch64-linux-gnu -O2 -fPIC -ffreestanding -fno-builtin \
 *           -fno-stack-protector -fvisibility=hidden -nostdlib -shared \
 *           -Wl,-init,_init -Wl,-z,defs -Wl,--no-undefined -Wl,--build-id=none \
 *           -DPATCH_APPLY=213 -DMODE_NAME='"wbiclear"' \
 *           jiritsu_module.c -o rampatch_wbiclear.so
 *
 * There is no libc: every call goes straight through `svc #0`.  The module
 * never returns a value -- it appends printable text to a buffer and writes the
 * whole buffer to /tmp/jiritsu/rampatch.result at the end, which the host reads
 * back over the same file channel.
 *
 * The .so is loaded by the firmware when it is written to /tmp/jiritsu/libJit.so;
 * the entry point is therefore `_init` (DT_INIT), not a constructor attribute.
 *
 * Blocks use `goto emit;`, so the `emit:` label below must stay in _init.
 */
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

#ifndef PATCH_APPLY
#define PATCH_APPLY 0
#endif

#ifndef MODE_NAME
#define MODE_NAME "unknown"
#endif

#define OUTCAP (64u * 1024u)
static char out[OUTCAP];
static u32 olen;

/* ---- raw syscalls -------------------------------------------------------- */

static i64 sc6(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e, i64 f) {
    register i64 x8 __asm__("x8") = n;
    register i64 x0 __asm__("x0") = a;
    register i64 x1 __asm__("x1") = b;
    register i64 x2 __asm__("x2") = c;
    register i64 x3 __asm__("x3") = d;
    register i64 x4 __asm__("x4") = e;
    register i64 x5 __asm__("x5") = f;
    __asm__ volatile("svc #0" : "+r"(x0)
                     : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
                     : "memory");
    return x0;
}

static i64 opn(const char *p, i64 fl) { return sc6(56, -100, (i64)p, fl, 0, 0, 0); }
static i64 rd(i64 fd, void *b, u64 n) { return sc6(63, fd, (i64)b, (i64)n, 0, 0, 0); }
static i64 cl(i64 fd) { return sc6(57, fd, 0, 0, 0, 0, 0); }
static i64 wrt(i64 fd, const void *b, u64 n) { return sc6(64, fd, (i64)b, (i64)n, 0, 0, 0); }

/* ---- output helpers ------------------------------------------------------ */

static void put(u8 c) { if (olen < OUTCAP - 1) out[olen++] = (char)c; }

static void ps(const char *s) {
    if (!s) { put('?'); return; }
    u32 n = 0;
    while (s[n]) n++;
    for (u32 i = 0; i < n; i++) put((u8)s[i]);
}

static void dec(i64 v) {
    char b[24]; int i = 23; b[i--] = 0;
    if (v < 0) { ps("-"); v = -v; }
    if (!v) b[i--] = '0';
    while (v) { b[i--] = (char)('0' + (int)(v % 10)); v /= 10; }
    ps(&b[i + 1]);
}

static void hx(u64 v) {
    static const char H[] = "0123456789abcdef";
    char b[18]; int i = 17; b[i--] = 0;
    if (!v) b[i--] = '0';
    while (v) { b[i--] = H[(int)(v & 15)]; v >>= 4; }
    ps("0x"); ps(&b[i + 1]);
}

static void hxbyte(u8 b) {
    static const char H[] = "0123456789abcdef";
    put((u8)H[b >> 4]);
    put((u8)H[b & 15]);
}

/* ---- entry --------------------------------------------------------------- */

__attribute__((visibility("default"))) void _init(void) {
    static const char H[] = "A6700RAMPATCH\n";
    for (u32 i = 0; i < 14; i++) put((u8)H[i]);
    ps("mode="); ps(MODE_NAME); ps("\n");

#if PATCH_APPLY == 211
#include "wbi_hdr_block.c"
#elif PATCH_APPLY == 212
#include "wbi_rewrite_block.c"
#elif PATCH_APPLY == 213
#include "wbi_clear_block.c"
#elif PATCH_APPLY == 214
#include "wbi_restore_block.c"
#elif PATCH_APPLY == 215
#include "ap_fl23_revert_block.c"
#else
    ps("RESULT=NO_BLOCK\nnote=no block compiled in\n");
    goto emit;
#endif

emit:
    {
        i64 fd = opn("/tmp/jiritsu/rampatch.result", 0x241);
        if (fd >= 0) { wrt(fd, out, olen); cl(fd); }
    }
}
