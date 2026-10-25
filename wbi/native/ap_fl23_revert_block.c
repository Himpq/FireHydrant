/* ap_fl23_revert_block.c -- PATCH_APPLY == 215
 *
 * Revert the AP ELF header from the extended test size back to the FL23
 * baseline values (0x4488EED).  This is a 4-byte content change in two u64
 * program-header fields (0x60 and 0x68); each differs only in its low 16 bits.
 *
 * Sequence: remount /usr rw -> open appFw.so O_RDWR -> pread 16 B at 0x60 ->
 * verify current value is the extended one -> pwrite the FL23 pair -> fsync ->
 * pread verify -> remount /usr ro.
 */
#if PATCH_APPLY == 215
    {
        u8 a[16];
        i64 fd, rc, n, w;
        int i, ok = 1;
        ps("ap_fl23_revert\n");
        rc = sc6(40, 0, (i64)"/usr", 0, 32, 0, 0);
        if (rc < 0) rc = sc6(40, (i64)"none", (i64)"/usr", 0, 32, (i64)"rw", 0);
        ps("remount_rw="); dec(rc); ps("\n");
        if (rc < 0) { ps("abort=REMOUNT_RW\n"); goto emit; }
        fd = opn("/usr/lib/appFw.so", 2);
        ps("open="); dec(fd); ps("\n");
        if (fd < 0) { ps("abort=OPEN\n"); sc6(40, 0, (i64)"/usr", 0, 33, 0, 0); goto emit; }
        n = sc6(67, fd, (i64)a, 16, 0x60, 0, 0);
        ps("pre="); dec(n); ps(" cur=");
        for (i = 0; i < 8; i++) hxbyte(a[i]);
        ps("\n");
        if (n != 16) { ps("abort=PREREAD\n"); cl(fd); goto emit; }
        /* expect extended 0x4489230 twice */
        for (i = 0; i < 8; i++) if (a[i] != a[i + 8]) ok = 0;
        if (!(a[0] == 0x30 && a[1] == 0x92 && a[2] == 0x48 && a[3] == 0x04 && ok)) {
            ps("abort=NOT_EXTENDED_HEADER\n"); cl(fd); goto emit;
        }
        /* FL23 baseline: 0x4488EED -> ed 8e 48 04 00 00 00 00 */
        a[0] = 0xed; a[1] = 0x8e; a[2] = 0x48; a[3] = 0x04;
        a[4] = 0; a[5] = 0; a[6] = 0; a[7] = 0;
        for (i = 8; i < 16; i++) a[i] = a[i - 8];
        w = sc6(68, fd, (i64)a, 16, 0x60, 0, 0);
        ps("write="); dec(w); ps("\n");
        if (w != 16) { ps("abort=WRITE\n"); cl(fd); goto emit; }
        ps("fsync="); dec(sc6(82, fd, 0, 0, 0, 0, 0)); ps("\n");
        n = sc6(67, fd, (i64)a, 16, 0x60, 0, 0);
        ps("read2="); dec(n); ps(" now=");
        for (i = 0; i < 8; i++) hxbyte(a[i]);
        ps("\n");
        ok = (n == 16 && a[0] == 0xed && a[1] == 0x8e && a[2] == 0x48 && a[3] == 0x04);
        cl(fd);
        rc = sc6(40, 0, (i64)"/usr", 0, 33, 0, 0);
        if (rc < 0) rc = sc6(40, (i64)"none", (i64)"/usr", 0, 33, 0, 0);
        ps("remount_ro="); dec(rc); ps("\n");
        ps(ok ? "RESULT=OK\nnote=AP header reverted to FL23 0x4488EED\n"
              : "RESULT=VERIFY_FAILED\nnote=header not reverted\n");
        goto emit;
    }
#endif
