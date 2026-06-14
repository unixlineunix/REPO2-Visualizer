# POSIX syscall replacements — no unistd.h

.section .text

#----------------------------------------------------------------------------------------
# FUN: asm_close -- close(fd)
# Input:  %rdi = fd (int)
# Output: %rax = 0 on success, -1 on error
#----------------------------------------------------------------------------------------
.global asm_close
.type asm_close, @function
asm_close:
    movq $3, %rax       # SYS_close
    syscall
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_ftruncate -- ftruncate(fd, length)
# Input:  %rdi = fd, %rsi = length (off_t)
# Output: %rax = 0 on success, -1 on error
#----------------------------------------------------------------------------------------
.global asm_ftruncate
.type asm_ftruncate, @function
asm_ftruncate:
    movq $77, %rax      # SYS_ftruncate
    syscall
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_unlink -- unlink(path)
# Input:  %rdi = const char* path
# Output: %rax = 0 on success, -1 on error
#----------------------------------------------------------------------------------------
.global asm_unlink
.type asm_unlink, @function
asm_unlink:
    movq $87, %rax      # SYS_unlink
    syscall
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_write -- write(fd, buf, count)
# Input:  %rdi = fd, %rsi = buf, %rdx = count
# Output: %rax = bytes written or -1
#----------------------------------------------------------------------------------------
.global asm_write
.type asm_write, @function
asm_write:
    movq $1, %rax       # SYS_write
    syscall
    ret

#----------------------------------------------------------------------------------------
# FUN: asm_read -- read(fd, buf, count)
# Input:  %rdi = fd, %rsi = buf, %rdx = count
# Output: %rax = bytes read or -1
#----------------------------------------------------------------------------------------
.global asm_read
.type asm_read, @function
asm_read:
    movq $0, %rax       # SYS_read
    syscall
    ret
