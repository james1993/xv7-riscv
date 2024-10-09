        # qemu -kernel loads the kernel at 0x80000000 and causes each CPU to jump there
        # kernel.ld causes the following code to be placed at 0x80000000
.section .text
.global _entry
_entry:
        # start.c declares stack0 as a 4096-byte stack per CPU.
        # set the stack pointers to stack0 + ((hartid + 1) * 4096)
        # +1 is because the stack grows downward
        la sp, stack0
        li a0, 4096
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        # stacks are set up, call start() in start.c
        call start
spin:
        j spin
