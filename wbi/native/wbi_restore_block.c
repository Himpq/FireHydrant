/* wbi_restore_block.c -- PATCH_APPLY == 214
 *
 * Restore the WBI header sector from /tmp/jiritsu/wbihdr_backup.bin.
 * The file is 512 bytes captured by mode 213 before clearing (or staged by
 * the host).  Verifies magic WBI2 before writing, then pwrite + fsync and
 * read-back compare.
 */
#if PATCH_APPLY == 214
    {
        unsigned char a[512], b[512];
        i64 fd, bf, n, w, s, m;
        int i, same = 1, diff = 0;
        ps("wbi_restore\n");
        for (i = 0; i < 512; i++) { a[i] = 0; b[i] = 0; }
        bf = opn("/tmp/jiritsu/wbihdr_backup.bin", 0);
        ps("backup_open="); dec(bf); ps("\n");
        if (bf < 0) { ps("abort=BACKUP_OPEN\n"); goto emit; }
        n = rd(bf, a, 512);
        cl(bf);
        ps("backup_read="); dec(n); ps(" magic=");
        for (i = 0; i < 4; i++) hxbyte(a[i]);
        ps("\n");
        if (n != 512) { ps("abort=BACKUP_READ\n"); goto emit; }
        if (!(a[0] == 'W' && a[1] == 'B' && a[2] == 'I' && a[3] == '2')) {
            ps("abort=NOT_WBI2\n"); goto emit;
        }
        fd = opn("/dev/nflasha5", 2);
        ps("open="); dec(fd); ps("\n");
        if (fd < 0) { ps("abort=OPEN\n"); goto emit; }
        w = sc6(68, fd, (i64)a, 512, 0, 0, 0);
        ps("write="); dec(w); ps("\n");
        if (w != 512) { ps("abort=WRITE\n"); cl(fd); goto emit; }
        s = sc6(82, fd, 0, 0, 0, 0, 0);
        ps("fsync="); dec(s); ps("\n");
        m = sc6(67, fd, (i64)b, 512, 0, 0, 0);
        ps("read2="); dec(m); ps("\n");
        cl(fd);
        if (m != 512) { ps("abort=REREAD\n"); goto emit; }
        for (i = 0; i < 512; i++) if (a[i] != b[i]) { same = 0; diff++; }
        ps("match="); dec(same); ps(" diff="); dec(diff); ps("\n");
        ps(same ? "RESULT=OK\nnote=header restored and verified\n"
                : "RESULT=VERIFY_FAILED\nnote=restore mismatch\n");
        goto emit;
    }
#endif
