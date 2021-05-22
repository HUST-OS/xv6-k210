#include "user.h"

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: mount IMAGE-FILE MOUNT-POINT\n");
        exit(1);
    }

    if (mount(argv[1], argv[2], "vfat", 0, NULL) < 0) {
        fprintf(2, "mount: fail to mount %s at %s\n", argv[1], argv[2]);
    }

    exit(0);
}