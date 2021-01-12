#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

int main()
{
    for (int i = 0; i < 10; i++) {
        if (fork() == 0) {
            while (1) {
                test_proc();
            }
        }
    }
    while (1) {
        test_proc();
    }

    exit(0);
}
