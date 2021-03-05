#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

int main()
{
    int n = 5;
    while (n--) {
        test_proc(n);
    }

    exit(0);
}
