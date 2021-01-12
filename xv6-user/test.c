#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

int main()
{
    while (1) {
        test_proc();
    }
    exit(0);
}
