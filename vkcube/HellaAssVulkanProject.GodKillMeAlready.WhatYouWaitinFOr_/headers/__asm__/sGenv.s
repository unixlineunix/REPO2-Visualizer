# Sect: 1 - Data Section (Not needed for this stateless tool)
.section .data

# Sect: 2 - Main Environment Parsing Code
.section .text

#----------------------------------------------------------------------------------------
# FUN: ctCX_GetEnv -- Searches the Linux application stack for an environment variable
# Inputs from C++: 
#   %rdi = Pointer to the null-terminated variable name to find (e.g., "USER")
# Outputs to C++: 
#   %rax = Pointer to the value string, or 0 (NULL) if not found
# ---------------------------------------------------------------------------------------
.global ctCX_GetEnv
.type ctCX_GetEnv, @function
ctCX_GetEnv:
	pushq %rbp
	movq %rsp, %rbp

	# Guard: exit early if input key pointer is completely NULL
	testq %rdi, %rdi
	jz .getenv_not_found

	# Read the location of 'environ' from the Global Offset Table (GOT)
	# The Linux runtime linker sets up this array pointing to the raw system stack base.
	movq environ@GOTPCREL(%rip), %rcx
	movq (%rcx), %rcx                  # %rcx = address of the first array entry pointer
	testq %rcx, %rcx
	jz .getenv_not_found

.getenv_loop:
	movq (%rcx), %rsi                  # %rsi = raw address pointer to string block (e.g. "USER=uki")
	testq %rsi, %rsi
	jz .getenv_not_found               # If the current string entry pointer is 0 (NULL), array ended!

	# Match your requested input key (%rdi) character-by-character against system target (%rsi)
	xorq %r8, %r8                      # %r8 = string offset index counter set to 0

.getenv_match_key:
	movb (%rdi, %r8), %r9b             # Load 1 byte from your requested input variable name
	movb (%rsi, %r8), %r10b            # Load 1 byte from the current system environment string entry

	# Check if the system entry hit its '=' delimiter sign
	cmpb $'=', %r10b
	je .getenv_check_key_end

	# If characters don't match, this entry is dead. Jump to advance to the next index string.
	cmpb %r9b, %r10b
	jne .getenv_next_entry

	incq %r8                           # Advance character match tracking offset index (%r8++)
	jmp .getenv_match_key

.getenv_check_key_end:
	# If the system entry is at '=' and your string is at '\0', we found a perfect structural match!
	cmpb $0, %r9b
	je .getenv_match_found

.getenv_next_entry:
	addq $8, %rcx                      # Move forward 8 bytes (64-bits) to inspect the next pointer entry slot
	jmp .getenv_loop

.getenv_match_found:
	# The environment value block lives immediately behind the '=' symbol characters.
	# %rsi contains string base address. %r8 contains exact string length of your key variable name.
	leaq 1(%rsi, %r8), %rax            # Address calculation: Base (%rsi) + Key len (%r8) + 1 (skips the '=')
	popq %rbp
	ret

.getenv_not_found:
	xorq %rax, %rax                    # Return 0 (NULL) to C++ if the entry lookup falls flat
	popq %rbp
	ret
