/* wbi_rewrite_block.c -- PATCH_APPLY == 212
 *
 * Write-channel self-test: re-write the CURRENT 512-byte header back to
 * /dev/nflasha5 offset 0, then read it back and compare.
 *
 * This does NOT change any content: the buffer is filled by reading the same
 * sector first.  It only proves that /dev/nflasha5 is writable from this
 * process and that pwrite/fsync/readback work, so the real clear-header step
 * has a verified recovery/write path.
 */
#if PATCH_APPLY == 212
    {
        unsigned char a[512], b[512];
        i64 fd, n, w, s, m;
        int i, same = 1, diff = 0;
        ps("wbi_rewrite\n");
        for (i = 0; i < 512; i++) { a[i] = 0; b[i] = 0; }
        fd = opn("/dev/nflasha5", 2);          /* O_RDWR */
        ps("open="); dec(fd); ps("\n");
        if (fd < 0) { ps("abort=OPEN\n"); goto emit; }
        n = sc6(67, fd, (i64)a, 512, 0, 0, 0);  /* pread64 */
        ps("pre="); dec(n); ps(" magic=");
        for (i = 0; i < 4; i++) hxbyte(a[i]);
        ps("\n");
        if (n != 512) { ps("abort=PREREAD\n"); cl(fd); goto emit; }
        if (!(a[0] == 'W' && a[1] == 'B' && a[2] == 'I' && a[3] == '2')) {
            ps("abort=NOT_WBI2\n"); cl(fd); goto emit;
        }
        w = sc6(68, fd, (i64)a, 512, 0, 0, 0); /* pwrite64 */
        ps("write="); dec(w); ps("\n");
        if (w != 512) { ps("abort=WRITE\n"); cl(fd); goto emit; }
        s = sc6(82, fd, 0, 0, 0, 0, 0);         /* fsync */
        ps("fsync="); dec(s); ps("\n");
        m = sc6(67, fd, (i64)b, 512, 0, 0, 0);  /* pread64 again */
        ps("read2="); dec(m); ps("\n");
        cl(fd);
        if (m != 512) { ps("abort=REREAD\n"); goto emit; }
        for (i = 0; i < 512; i++) if (a[i] != b[i]) { same = 0; diff++; }
        ps("match="); dec(same); ps(" diff="); dec(diff); ps("\n");
        ps("hdr=");
        for (i = 0; i < 16; i++) hxbyte(b[i]);
        ps("\n");
        ps(same ? "RESULT=OK\nnote=same content written back and verified\n"
                : "RESULT=VERIFY_FAILED\nnote=readback differs\n");
        goto emit;
    }
#endif
