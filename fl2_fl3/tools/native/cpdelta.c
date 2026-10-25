/* cpdelta.c -- apply/reverse the 33-window CP delta in place.
 *
 * Reads /tmp/jiritsu/cp.delta (9536 B), /tmp/jiritsu/cpdelta.mode
 * ("apply" or "restore"), opens /cp/cpapp-b.bin O_RDWR and modifies only the
 * delta windows. No 17.9 MB copy, no extra /cp space.
 *
 * apply  : first pass verifies every window equals the delta's "before";
 *          only then writes "after" and reads each window back.
 * restore: allows each window to be before or after (partial apply), writes
 *          "before" back.
 *
 * Result: /tmp/jiritsu/cpdelta.result
 * Syscalls: 56 openat, 57 close, 63 read, 64 write, 67 pread64, 68 pwrite64, 82 fsync.
 */
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

#define AT_FDCWD (-100)
#define O_RDONLY 0
#define O_RDWR 2
#define O_WRONLY 1
#define O_CREAT 0x40
#define O_TRUNC 0x200

#define DELTA_LEN 9536
#define MAX_WINDOWS 64
#define MAX_WIN 8192

static u8 delta[DELTA_LEN];
static u8 win[MAX_WIN];
static char out[2048];
static u32 olen;

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

static i64 opn(const char *p, i64 fl) { return sc6(56, AT_FDCWD, (i64)p, fl, 0644, 0, 0); }
static i64 rd(i64 fd, void *b, u64 n) { return sc6(63, fd, (i64)b, (i64)n, 0, 0, 0); }
static i64 wr(i64 fd, const void *b, u64 n) { return sc6(64, fd, (i64)b, (i64)n, 0, 0, 0); }
static i64 cl(i64 fd) { return sc6(57, fd, 0, 0, 0, 0, 0); }
static i64 pread64_(i64 fd, void *b, u64 n, u64 off) { return sc6(67, fd, (i64)b, (i64)n, (i64)off, 0, 0); }
static i64 pwrite64_(i64 fd, const void *b, u64 n, u64 off) { return sc6(68, fd, (i64)b, (i64)n, (i64)off, 0, 0); }
static i64 fsyn(i64 fd) { return sc6(82, fd, 0, 0, 0, 0, 0); }

static u32 rd32(const u8 *p, int o) {
    return (u32)p[o] | ((u32)p[o + 1] << 8) | ((u32)p[o + 2] << 16) | ((u32)p[o + 3] << 24);
}
static u64 rd64(const u8 *p, int o) {
    u64 v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | p[o + i];
    return v;
}

static void put(u8 c) { if (olen < sizeof(out) - 1) out[olen++] = (char)c; }
static void ps(const char *s) { u32 n = 0; while (s[n]) n++; for (u32 i = 0; i < n; i++) put((u8)s[i]); }
static void dec(i64 v) {
    char b[24]; int i = 23; b[i--] = 0;
    if (v < 0) { ps("-"); v = -v; }
    if (!v) b[i--] = '0';
    while (v) { b[i--] = (char)('0' + (int)(v % 10)); v /= 10; }
    ps(&b[i + 1]);
}
static void udec(u64 v) {
    char b[24]; int i = 23; b[i--] = 0;
    if (!v) b[i--] = '0';
    while (v) { b[i--] = (char)('0' + (int)(v % 10)); v /= 10; }
    ps(&b[i + 1]);
}

static void write_result(void) {
    i64 fd = opn("/tmp/jiritsu/cpdelta.result", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        i64 off = 0;
        while (off < (i64)olen) {
            i64 n = wr(fd, out + off, (u64)(olen - (u32)off));
            if (n <= 0) break;
            off += n;
        }
        cl(fd);
    }
}

static void fail(const char *why, int idx) {
    olen = 0;
    ps("A6700CPDELTA\n");
    ps("RESULT=FAIL\nreason="); ps(why); ps("\n");
    if (idx >= 0) { ps("window="); dec(idx); ps("\n"); }
    write_result();
}

static int read_full(i64 fd, u8 *p, int n) {
    int got = 0;
    while (got < n) {
        i64 k = rd(fd, p + got, (u64)(n - got));
        if (k <= 0) break;
        got += (int)k;
    }
    return got;
}

__attribute__((visibility("default"))) void _init(void) {
    /* 1. mode */
    u8 mb[8];
    i64 mfd = opn("/tmp/jiritsu/cpdelta.mode", O_RDONLY);
    if (mfd < 0) { fail("no_mode_file", -1); return; }
    i64 mn = rd(mfd, mb, sizeof(mb));
    cl(mfd);
    int apply = 1;
    if (mn <= 0) { fail("empty_mode", -1); return; }
    if (mb[0] == 'r' || mb[0] == 'R') apply = 0;

    /* 2. delta file */
    i64 dfd = opn("/tmp/jiritsu/cp.delta", O_RDONLY);
    if (dfd < 0) { fail("no_delta_file", -1); return; }
    int dn = read_full(dfd, delta, DELTA_LEN);
    cl(dfd);
    if (dn != DELTA_LEN) { fail("short_delta", -1); return; }

    /* 3. header */
    const u8 magic[8] = { 'A','6','7','0','0','D','0','1' };
    for (int i = 0; i < 8; i++) if (delta[i] != magic[i]) { fail("bad_magic", -1); return; }
    u32 ver = rd32(delta, 8);
    u32 n = rd32(delta, 12);
    u64 orig = rd64(delta, 16);
    u64 pay = rd64(delta, 24);
    if (ver != 1) { fail("bad_version", -1); return; }
    if (n == 0 || n > MAX_WINDOWS) { fail("bad_window_count", -1); return; }
    if (orig != 17892024ULL) { fail("bad_original_length", -1); return; }
    if (pay > (u64)(DELTA_LEN - 96)) { fail("bad_payload_length", -1); return; }
    u64 pbase = 96 + (u64)n * 16;
    if (pbase + pay != (u64)DELTA_LEN) { fail("bad_layout", -1); return; }

    /* 4. descriptor bounds + non-overlap */
    u64 prev_end = 0;
    for (u32 i = 0; i < n; i++) {
        int o = 96 + (int)i * 16;
        u64 off = rd64(delta, o);
        u32 len = rd32(delta, o + 8);
        u32 rel = rd32(delta, o + 12);
        if (len == 0 || len > MAX_WIN) { fail("bad_window_len", (int)i); return; }
        if (off > orig || (u64)len > orig - off) { fail("window_oob", (int)i); return; }
        if ((u64)rel + 2ULL * (u64)len > pay) { fail("payload_oob", (int)i); return; }
        if (i && off < prev_end) { fail("window_overlap", (int)i); return; }
        prev_end = off + len;
    }

    /* 5. open the live file */
    i64 fd = opn("/cp/cpapp-b.bin", O_RDWR);
    if (fd < 0) { fail("open_cp_fail", -1); return; }

    /* 6. first pass: verify the expected side of every window, no writes */
    u64 written = 0;
    for (u32 i = 0; i < n; i++) {
        int o = 96 + (int)i * 16;
        u64 off = rd64(delta, o);
        u32 len = rd32(delta, o + 8);
        u32 rel = rd32(delta, o + 12);
        const u8 *before = delta + pbase + rel;
        const u8 *after = before + len;
        i64 got = pread64_(fd, win, len, off);
        if (got != (i64)len) { cl(fd); fail("pread_fail", (int)i); return; }
        int ok = 1;
        for (u32 k = 0; k < len; k++) if (win[k] != before[k]) { ok = 0; break; }
        if (!ok) {
            if (apply) { cl(fd); fail("before_mismatch", (int)i); return; }
            /* restore mode: allow already-patched windows */
            ok = 1;
            for (u32 k = 0; k < len; k++) if (win[k] != after[k]) { ok = 0; break; }
            if (!ok) { cl(fd); fail("neither_before_nor_after", (int)i); return; }
        }
    }

    /* 7. second pass: write the desired side, read back */
    for (u32 i = 0; i < n; i++) {
        int o = 96 + (int)i * 16;
        u64 off = rd64(delta, o);
        u32 len = rd32(delta, o + 8);
        u32 rel = rd32(delta, o + 12);
        const u8 *before = delta + pbase + rel;
        const u8 *after = before + len;
        const u8 *want = apply ? after : before;
        i64 w = pwrite64_(fd, want, len, off);
        if (w != (i64)len) { cl(fd); fail("pwrite_fail", (int)i); return; }
        i64 got = pread64_(fd, win, len, off);
        if (got != (i64)len) { cl(fd); fail("readback_fail", (int)i); return; }
        for (u32 k = 0; k < len; k++) {
            if (win[k] != want[k]) { cl(fd); fail("readback_mismatch", (int)i); return; }
        }
        written += len;
    }

    i64 fr = fsyn(fd);
    cl(fd);

    olen = 0;
    ps("A6700CPDELTA\n");
    ps("mode="); ps(apply ? "apply" : "restore"); ps("\n");
    ps("windows="); udec(n); ps("\n");
    ps("written="); udec(written); ps("\n");
    ps("fsync="); dec(fr); ps("\n");
    ps("RESULT="); ps(fr == 0 ? "OK\n" : "FAIL\n");
    write_result();
}
