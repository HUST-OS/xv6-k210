# Test process 4
# This code run in user space

.globl test_4
test_4:
        li a0, 68
        li a1, 0
        li a2, 0
        li a3, 0
        li a7, 1
        ecall 
loop_4:
        jal loop_4