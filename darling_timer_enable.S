/* Audited replacement of the enable branch in Darling's kqueue copyin.
 * Existing frame slots: source=-0x18, knote=-0x20, filter=-0x28, result=-0x2c.
 * Filter callbacks: modify=0x28, enable=0x38. EVFILT_TIMER=-7, EV_ADD=1.
 * Original branch length=0x72; its final jump reaches the existing join.
 * Complete libraries, frame layout and instruction regions are pinned in
 * launcher/macoblox/darling_patches.py. Other builds are left unchanged.
 */
.text
_macoblox_timer_enable_repair:
    movq -0x18(%rbp), %rdx
    testw $1, 0xa(%rdx)
    jz Lenable_only
    cmpw $-7, 8(%rdx)
    jne Lenable_only
    movq -0x20(%rbp), %rsi
    movq 0x18(%rdx), %rax
    movq %rax, 0x20(%rsi)
    movq -0x28(%rbp), %rdi
    callq *0x28(%rdi)
    jmp Lresult
Lenable_only:
    movq -0x20(%rbp), %rsi
    andw $-9, 0xa(%rsi)
    movq -0x28(%rbp), %rdi
    callq *0x38(%rdi)
Lresult:
    movl %eax, -0x2c(%rbp)
    .fill 0x6d-(.-_macoblox_timer_enable_repair), 1, 0x90
    .byte 0xe9,0xb5,0,0,0
