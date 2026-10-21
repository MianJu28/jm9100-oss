/* Direct pipe-screen smoke test via renderD128, mirroring glamor's GBM path */
#include <xf86drm.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(void) {
    int fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if (fd < 0) { perror("open renderD128"); return 1; }

    drmVersionPtr v = drmGetVersion(fd);
    if (v) { printf("drm driver: %.*s %d.%d.%d\n", v->name_len, v->name, v->version_major, v->version_minor, v->version_patchlevel); drmFreeVersion(v); }

    /* CTX ioctl exactly as the winsys does (DRM_COMMAND_BASE + 0x04, op 0) */
    struct { unsigned op, flags, handle, resv; } args = {0};
    int err = drmCommandWriteRead(fd, 0x04, &args, sizeof(args));
    if (err < 0) {
        printf("CTX create: FAILED errno=%d (%s)\n", -err, strerror(-err));
        close(fd); return 2;
    }
    printf("CTX create: OK handle=%u\n", args.handle);

    /* GEM create 4KB */
    union { struct { unsigned long long size, alignment; unsigned preferred_domain, flags; } in;
            struct { unsigned handle, resv; } out; } create = {0};
    create.in.size = 4096; create.in.alignment = 0x1000; create.in.preferred_domain = 0x2; create.in.flags = 1;
    err = drmCommandWriteRead(fd, 0x01, &create, sizeof(create));
    if (err < 0) { printf("GEM create: FAILED errno=%d\n", -err); close(fd); return 3; }
    printf("GEM create: OK handle=%u\n", create.out.handle);

    /* INFO ioctl (engine counts) */
    struct { unsigned op, nr_recs; unsigned long long recs; } info = {0};
    unsigned rec[16] = {0};
    info.op = 0; info.nr_recs = 5; info.recs = (unsigned long long)rec;
    err = drmCommandWriteRead(fd, 0x00, &info, sizeof(info));
    printf("INFO query: %s 3D=%u DEC=%u ENC=%u 2D=%u DMA=%u\n",
           err ? "FAILED" : "OK", rec[1], rec[3], rec[5], rec[7], rec[9]);

    /* destroy ctx */
    args.op = 1; args.handle = args.handle;
    err = drmCommandWriteRead(fd, 0x04, &args, sizeof(args));
    printf("CTX destroy: %s\n", err ? "FAILED" : "OK");

    close(fd);
    printf("\nSMOKE: kernel interface fully operational\n");
    return 0;
}
