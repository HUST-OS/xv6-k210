# Test process 1
# This code run in user space

.globl test_1
test_1:
        li a0, 65
        li a1, 0
        li a2, 0
        li a3, 0
        li a7, 1
        ecall 
loop_1:
        jal loop_1