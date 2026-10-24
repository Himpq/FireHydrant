/* wbi_hdr_block.c -- PATCH_APPLY == 211
 *
 * Read-only: dump the first 512 bytes of the warm-boot-image partition header
 * and show struct wb_header fields (include/linux/warmboot.h, 88 bytes):
 *   fVersion[4], pad, sections, flag, resume_vector, dVersion, sector_size,
 *   r_data_size, kern_start, kern_size, kern_cksum, o_data_size
 *
 * Also samples the first 4 bytes of /dev/nflasha1..15 to see which partition
 * carries a WBI magic.  No writes, no ioctls, no device changes.
 */
#if PATCH_APPLY == 211
    {
        unsigned char buf[512];
        i64 fd, n;
        int i, k;
        static const char *DEVS[15] = {
            "/dev/nflasha1", "/dev/nflasha2", "/dev/nflasha3", "/dev/nflasha4",
            "/dev/nflasha5", "/dev/nflasha6", "/dev/nflasha7", "/dev/nflasha8",
            "/dev/nflasha9", "/dev/nflasha10", "/dev/nflasha11", "/dev/nflasha12",
            "/dev/nflasha13", "/dev/nflasha14", "/dev/nflasha15"
        };
        ps("wbi_hdr\n");
        for (i = 0; i < 512; i++) buf[i] = 0;
        fd = opn("/dev/nflasha5", 0);
        ps("open5="); dec(fd); ps("\n");
        if (fd >= 0) {
            n = rd(fd, buf, 512);
            ps("read5="); dec(n); ps("\n");
            cl(fd);
            if (n == 512) {
                ps("magic=");
                for (k = 0; k < 4; k++) hxbyte(buf[k]);
                ps("\n");
                ps("sections="); hx(*(u64 *)(buf + 8));
                ps(" flag="); hx(*(u64 *)(buf + 16));
                ps(" resume="); hx(*(u64 *)(buf + 24));
                ps("\n");
                ps("dver="); hx(*(u64 *)(buf + 32));
                ps(" sector="); hx(*(u64 *)(buf + 40));
                ps(" rdata="); hx(*(u64 *)(buf + 48));
                ps("\n");
                ps("kstart="); hx(*(u64 *)(buf + 56));
                ps(" ksize="); hx(*(u64 *)(buf + 64));
                ps(" kcksum="); hx(*(u64 *)(buf + 72));
                ps(" odata="); hx(*(u64 *)(buf + 80));
                ps("\n");
                ps("hdr88=");
                for (i = 0; i < 88; i++) hxbyte(buf[i]);
                ps("\n");
                k = 0;
                for (i = 88; i < 512; i++) if (buf[i]) k++;
                ps("nonzero_after88="); dec(k); ps("\n");
            }
        }
        ps("scan:\n");
        for (i = 0; i < 15; i++) {
            unsigned char m[4];
            i64 f2 = opn(DEVS[i], 0);
            if (f2 < 0) continue;
            if (rd(f2, m, 4) == 4) {
                ps("  "); ps(DEVS[i]); ps(" ");
                for (k = 0; k < 4; k++) hxbyte(m[k]);
                ps("\n");
            }
            cl(f2);
        }
        ps("RESULT=OK\nnote=read only; no writes, no ioctls\n");
        goto emit;
    }
#endif
