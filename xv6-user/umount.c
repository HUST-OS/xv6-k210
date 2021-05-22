#include "user.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(2, "Usage: umount MOUNT-POINT\n");
        exit(1);
    }

    if (umount(argv[1], 0) < 0) {
        fprintf(2, "umount: fail to umount %s\n", argv[1]);
    }

    exit(0);
}