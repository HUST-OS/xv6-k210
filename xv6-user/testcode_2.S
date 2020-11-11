# Test process 2
# This code run in user space

.globl test_2
test_2:
        li a0, 66
        li a1, 0
        li a2, 0
        li a3, 0
        li a7, 1
        ecall 
loop_2:
        jal loop_2