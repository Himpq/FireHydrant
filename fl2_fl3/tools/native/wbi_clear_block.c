/* wbi_clear_block.c -- PATCH_APPLY == 213
 *
 * Backup + clear of the WBI header sector:
 *   1. read 512 B at /dev/nflasha5 offset 0, require magic "WBI2"
 *   2. save the sector to /tmp/jiritsu/wbihdr_backup.bin
 *   3. write 512 zero bytes at offset 0, fsync
 *   4. read back and verify all zero
 *
 * Prints the original 512 B as hex so the host keeps an independent copy.
 */
#if PATCH_APPLY == 213
    {
        unsigned char a[512], o[512], z[512];
        i64 fd, n, w, s, m;
        int i, allzero = 1;
        ps("wbi_clear\n");
        for (i = 0; i < 512; i++) { a[i] = 0; o[i] = 0; z[i] = 0; }
        fd = opn("/dev/nflasha5", 2);
        ps("open="); dec(fd); ps("\n");
        if (fd < 0) { ps("abort=OPEN\n"); goto emit; }
        n = sc6(67, fd, (i64)a, 512, 0, 0, 0);
        ps("pre="); dec(n); ps(" magic=");
        for (i = 0; i < 4; i++) hxbyte(a[i]);
        ps("\n");
        if (n != 512) { ps("abort=PREREAD\n"); cl(fd); goto emit; }
        if (!(a[0] == 'W' && a[1] == 'B' && a[2] == 'I' && a[3] == '2')) {
            ps("abort=NOT_WBI2\n"); cl(fd); goto emit;
        }
        for (i = 0; i < 512; i++) o[i] = a[i];
        {
            i64 bf = opn("/tmp/jiritsu/wbihdr_backup.bin", 0x241);
            if (bf < 0) { ps("abort=BACKUP_OPEN\n"); cl(fd); goto emit; }
            if (wrt(bf, o, 512) != 512) { ps("abort=BACKUP_WRITE\n"); cl(bf); cl(fd); goto emit; }
            sc6(82, bf, 0, 0, 0, 0, 0);
            cl(bf);
            ps("backup=ok\n");
        }
        w = sc6(68, fd, (i64)z, 512, 0, 0, 0);
        ps("write="); dec(w); ps("\n");
        if (w != 512) { ps("abort=WRITE\n"); cl(fd); goto emit; }
        s = sc6(82, fd, 0, 0, 0, 0, 0);
        ps("fsync="); dec(s); ps("\n");
        m = sc6(67, fd, (i64)a, 512, 0, 0, 0);
        ps("read2="); dec(m); ps("\n");
        cl(fd);
        if (m != 512) { ps("abort=REREAD\n"); goto emit; }
        for (i = 0; i < 512; i++) if (a[i]) allzero = 0;
        ps("zero="); dec(allzero); ps("\n");
        ps("orig512=");
        for (i = 0; i < 512; i++) hxbyte(o[i]);
        ps("\n");
        ps(allzero ? "RESULT=OK\nnote=header sector cleared; backup on camera and in this log\n"
                   : "RESULT=VERIFY_FAILED\nnote=readback not zero\n");
        goto emit;
    }
#endif
