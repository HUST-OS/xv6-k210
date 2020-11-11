# Test process 3
# This code run in user space

.globl test_3
test_3:
        li a0, 67
        li a1, 0
        li a2, 0
        li a3, 0
        li a7, 1
        ecall 
loop_3:
        jal loop_3