# Sect: 1 - Data struct Def'n

.section .data 										# Kainda feels like writing a raw application 
.align 8											# So this tells the CPU to align memory blocks clanely by 8 bytes
#													# NGL: basically 64 bits

.global DynStrg
DynStrg:
	.quad 0											# [Offset: 0]  :- Base memory buffer address pointer (char*)
	.quad 0											# [Offset: 8]  :- Current tracking length of text characters
	.quad 0											# [Offset: 16] :- Max capacity threshold limit allocation

# Sect: 2 - Main Code

.section .text

#----------------------------------------------------------------------------------------
# FUN: ctCX_Append -- Appends raw C-Style text to the C++ string wrapper
#
# Inputs from c++:
# 	%rdi = Addr where 'DynStrg' structure lives
# 	%rsi = Addr of the incoming raw text block we want to add
# ---------------------------------------------------------------------------------------

.global  ctCX_Append
.type ctCX_Append, @function
ctCX_Append:
	# --- Step: 1 - Safeguard callerl's reg's --- #
	pushq %rbx										# Backup: original %rbx --> stack
	pushq %r12										# Backup: original %r12 --> stack
	pushq %r13										# Backup: original %r13 --> stack

	# --- Step: 2 - Lock Input Arg's into Safe Reg's 	--- #
	movq %rdi, %rbx									# Move structure location pointer into %rbx
	movq %rsi, %r12									# Mov incoming text location pointer into %r12

	# --- Step: 3 - Find the .len() if incomming text 	--- #
	# --- RAW: custom Inline Strlen FUN 				--- #
	xorq %r13, %r13 								# This set's length counter to 0

.rCX_Strg_LenLoop:
	# Load a single byte from the addr: %12 -: Apprently : :- incomming text base : + %13 :- counter index :
	# %r8b is :] smallest 8 bit pocket --->^ %r8 Reg, :] perfect for holding a single character . .. letter
	movb (%r12, %r13), %r8b 

	testb %r8b, %r8b								# Test: GrabbedChar != 0 : nullTerm -:		
	jz .sCX_rfCX_Strg_LenSCC						# sCX_rfCX_SCC:: 0 == true :- end of thex $= break -:

	incq %r13										# Test: !0 Lencount++ :
	jmp .rCX_Strg_LenLoop							# Loop: Look :: NEXT CHAR POS

.sCX_rfCX_Strg_LenSCC:
	jmp .rCX_rdStrg_Len								# Fix: bridge gap naturally to length evaluation block below

.rCX_rdStrg_Len:
	testq %r13, %r13								# Calc: is === 0 :?
	jz .ctCX_C_EE_C_0 								# Check: == 0, case true: Skip && quit

	# --- Step: 4 - Calculate req:? space 	--- #
	movq 8(%rbx), %rax 								# Read: CurrentLen (Offset: 8) --> structure to %rax
	addq %r13, %rax 								# Add: incoming text length -: r13 --: into:: %rax
	incq %rax 										# Add: 1 extra byte -:>  the text's ending null stop-sign :/ CKA: '\0'

	# --- Step: 5 - Check if Buffer got Room ---#
	movq 16(%rbx), %rcx 							# Read: Max Cap (Offset: 16) 
	cmpq %rcx, %rax 								# Compare: %rcx :- MaxCap :-] :- %rax :Total needed space
	jbe .rCX_oCP_TXT 								# Test: MaxCap >= , *eh~ Mem alloc				

	# --- Step 6: Compute New Cap cap? --- #
	testq %rcx, %rcx 								# Check: MaxCap = 0?
	jnz .dCX_DubCap 								# Check: !0 :: Jmp: down - DubSize
	movq $256, %rcx 								# Check: = empty/null :) Default: initial size -* 256
	jmp .oCX_Alloc 									# Skip: DUbLoop | ProceedL Fwd

.dCX_DubCap:
	shlq $1, %rcx 									# Shift: left binary bit --> %rcx -> 1 :] (Fast multiply capacity by 2)... IDK bruh

.oCX_Alloc:
	# --- Step: 7 - Bound check the new Cap --- #
	cmpq %rcx, %rax 								# Compare: %rcx :- DubCap : :-] %rcx :- Needed space :
	jbe .oCX_AllocMem								# Test: If doubled fits everything fine -> skip adjustments
	movq %rax, %rcx									# Force: copy required space layout directly over if doubling fell short

.oCX_AllocMem:
	# --- Step: 8 - Request: 2 cups of Fresh Memory via C++ please --- #
	pushq %rax 										# Put: total needspace - calc ---> stack --> protect .
	pushq %rcx 										# Put: target Cap calculation ---> stack --> protect .

	movq %rcx, %rdi 								# Setup: First arg for C++ arr -] allocator :| required size
	call _Znam										# Call: C++ operator new[](size_t) to slice raw heap space

	popq %rcx										# Restore: target capacity threshold metric
	popq %rdx										# Restore: absolute required total bounds tracking calculations

	movq (%rbx), %rdi								# Read: old text buffer memory address location out of structure
	testq %rdi, %rdi								# Calc: check if old string memory was actually null/0?
	jz .oCX_SK_O_CP									# Check: == 0 case true -> Skip historical transfer phase

	pushq %rax										# Put: fresh buffer pointer tracking location -> protect
	pushq %rcx										# Put: newly defined capacity tracker boundary -> protect
	pushq %rdi										# Fix: Cache old buffer location pointer securely on stack top!

	# --- RAW INLINE MIGRATION COPY LOOP (Bypassed memcpy) ---
	movq %rax, %r8									# Move: destination workspace address tracking pointer to %r8
	movq %rdi, %r9									# Move: old baseline memory address location over to source %r9
	xorq %r10, %r10									# Reset: index pointer location tracker register to 0

.raw_migrate_loop:
	cmpq 8(%rbx), %r10								# Compare: index vs historical current text length configurations
	jge .raw_migrate_done							# Test: index >= old_len case true -> finish data processing

	movb (%r9, %r10), %r11b							# Move: pull character item byte securely into tiny temp pocket
	movb %r11b, (%r8, %r10)							# Move: deposit extracted byte block back into target index
	incq %r10										# Step: progress tracking indicator index forward
	jmp .raw_migrate_loop							# Loop: jump back to execute shift configurations on next character

.raw_migrate_done:
	popq %rdi										# Fix: Cleanly pull original cached old buffer address into %rdi
	call _ZdaPv										# Call: C++ operator delete[](void*) to destroy the old buffer tracking array

	popq %rcx										# Restore: current capacity settings back into tracking registers
	popq %rax										# Restore: newly allocated memory buffer base address pointer

.oCX_SK_O_CP:
	movq %rax, (%rbx)								# Map: secure new buffer master address down at structural Offset 0
	movq %rcx, 16(%rbx)								# Map: store freshly expanded capacity metrics down at Offset 16

.rCX_oCP_TXT:
	# --- RAW INLINE APPEND LOOP (Bypassed memcpy) ---
	movq (%rbx), %r8								# Move: master structure base tracking buffer location across to %r8
	addq 8(%rbx), %r8								# Add: offset cursor index directly forward by current length bytes
	xorq %r10, %r10									# Reset: insertion point offset indicator index down to 0

.raw_append_loop:
	cmpq %r13, %r10									# Compare: offset loop index vs total length of incoming payload (%r13)
	jge .raw_append_done							# Test: index >= incoming_len case true -> seal tail limits

	movb (%r12, %r10), %r11b						# Move: grab 1 incoming parameter character byte from source %r12
	movb %r11b, (%r8, %r10)							# Move: insert single payload letter directly onto target tracking index
	incq %r10										# Step: shift character parsing indicators forward
	jmp .raw_append_loop							# Loop: loop back to append next letter configuration

.raw_append_done:
	addq %r13, 8(%rbx)								# Add: combine newly added length directly into structure metric limits
	movq (%rbx), %rax								# Move: isolate text buffer coordinate pointer arrays
	addq 8(%rbx), %rax								# Add: progress focus directly up to final boundary marker index
	movb $0, (%rax)									# Move: drop ending string null stop-sign '\0' to protect tail parameters

.ctCX_C_EE_C_0:
	popq %r13										# Restore: recover pristine environment tracking register status
	popq %r12										# Restore: recover pristine environment tracking register status
	popq %rbx										# Restore: recover pristine environment tracking register status
	ret												# Return: jump safely back into master C++ execution loops

#----------------------------------------------------------------------------------------
# FUN: ctCX_DynCstInt -- Conversions mapping for numbers into ASCII text arrays
# ---------------------------------------------------------------------------------------

.global ctCX_DynCstInt
.type ctCX_DynCstInt, @function
ctCX_DynCstInt:
	pushq %rbx										# Backup: protect original baseline %rbx variables
	pushq %r12										# Backup: %r12 for sign flag
	movq %rdi, %rbx									# Move: save core structure instantiation target inside %rbx

	subq $33, %rsp									# Secure: 33 bytes for digits + potential '-' sign
	leaq 32(%rsp), %rcx								# Point to end of 32-byte digit workspace
	movb $0, (%rcx)									# Inject trailing null terminator

	movq %rsi, %rax									# Move raw incoming number into %rax
	xorq %r12, %r12									# r12 = 0 sign flag: 0 = positive, 1 = negative

	testq %rax, %rax								# Check sign of input number
	jns .oCX_cst_Convert							# If positive, skip negation

	movq $1, %r12									# Mark as negative
	negq %rax										# Compute absolute value (works for LLONG_MIN too)

.oCX_cst_Convert:
	movq $10, %r8									# Set numerical base constant to 10

.oCX_cst_Loop:
	xorq %rdx, %rdx									# Clear upper dividend register
	divq %r8										# Divide rdx:rax by 10. Quotient -> %rax, Remainder -> %rdx
	addb $48, %dl									# Convert remainder digit to ASCII
	decq %rcx										# Move cursor backward in workspace
	movb %dl, (%rcx)								# Write digit character into workspace slot
	testq %rax, %rax								# Check if quotient reached zero
	jnz .oCX_cst_Loop								# Loop if more digits remain

	testq %r12, %r12								# Check sign flag
	jz .oCX_cst_Append								# If positive, skip '-' insertion
	decq %rcx										# Move cursor back one more slot
	movb $45, (%rcx)								# Insert ASCII '-' prefix

.oCX_cst_Append:
	movq %rbx, %rdi									# Pass struct pointer as arg 1
	movq %rcx, %rsi									# Pass digit string pointer as arg 2
	call ctCX_Append								# Append the converted number string

	addq $33, %rsp									# Collapse stack workspace
	popq %r12										# Restore %r12
	popq %rbx										# Restore %rbx
	ret												# Return to C++ caller

#----------------------------------------------------------------------------------------
# FUN: ctCX_Free -- Cleans up allocated heap buffers to prevent memory leaks
# ---------------------------------------------------------------------------------------

.global ctCX_Free
.type ctCX_Free, @function
ctCX_Free:
	movq (%rdi), %rsi								# Read: extract heap block buffer pointer from structural Offset 0
	testq %rsi, %rsi								# Test: verify if buffer allocation is currently sitting at 0/NULL
	jz .ctFree_C_DN									# Check: == 0 case true -> skip destruction cycles and go to end
	
	pushq %rdi										# Backup: hold structure destination coordinates safe on stack
	movq %rsi, %rdi									# Move: pass heap target memory buffer block address to input register
	call _ZdaPv									    # Call: C++ operator delete[](void*) to drop memory configurations
	popq %rdi										# Restore: recover structure memory target pointer back into register

	movq $0, (%rdi)									# FIX: Use movq to completely wipe the full 64-bit pointer slot (0)
	movq $0, 8(%rdi)								# Reset: length indicators safely to 0
	movq $0, 16(%rdi)								# Reset: capacity ceiling metrics safely to 0

.ctFree_C_DN:
	ret												# Return: jump cleanly out back to master engine thread

#----------------------------------------------------------------------------------------
# FUN: ctCX_GetCStr -- Returns the raw char* pointer from the structure
# C++ Signature: extern "C" const char* ctCX_GetCStr(void* struct_ptr);
# Inputs:  %rdi = Address where our 'DynStrg' structure lives in memory
# Outputs: %rax = The raw char* memory address pointing to the text
# ---------------------------------------------------------------------------------------
.global ctCX_GetCStr
.type ctCX_GetCStr, @function
ctCX_GetCStr:
	# Read the first 8 bytes (Offset 0) of the structure.
	# This is where our base buffer pointer lives. 
	# Move it to %rax, because functions always return answers in %rax!
	movq (%rdi), %rax  
	ret

#----------------------------------------------------------------------------------------
# FUN: asm_rawPrintStr -- Pure zero-dependency string writer 
# ---------------------------------------------------------------------------------------
.global asm_rawPrintStr
.type asm_rawPrintStr, @function
asm_rawPrintStr:
	testq %rsi, %rsi
	jz .printStr_done
	xorq %rdx, %rdx
.printStr_len_loop:
	movb (%rsi, %rdx), %r8b
	testb %r8b, %r8b
	jz .printStr_exec
	incq %rdx
	jmp .printStr_len_loop
.printStr_exec:
	testq %rdx, %rdx
	jz .printStr_done
	movq $1, %rax									
	syscall											
.printStr_done:
	ret

#----------------------------------------------------------------------------------------
# FUN: asm_rawPrintDynStr -- Routes your dynamic DynStrg structure values straight to the terminal
# ---------------------------------------------------------------------------------------
.global asm_rawPrintDynStr
.type asm_rawPrintDynStr, @function
asm_rawPrintDynStr:
	movq (%rsi), %r8								
	testq %r8, %r8									
	jz .printDyn_done
	movq 8(%rsi), %rdx								
	testq %rdx, %rdx
	jz .printDyn_done
	movq %r8, %rsi									
	movq $1, %rax									
	syscall
.printDyn_done:
	ret

#----------------------------------------------------------------------------------------
# FUN: asm_rawPrintInt -- Casts numbers on the fly and drops them straight into the out pipe
# ---------------------------------------------------------------------------------------
.global asm_rawPrintInt
.type asm_rawPrintInt, @function
asm_rawPrintInt:
	pushq %rbx
	movq %rdi, %rbx									
	subq $32, %rsp									
	movq %rsp, %rdi									
	movq $0, (%rdi)									
	movq $0, 8(%rdi)								
	movq $0, 16(%rdi)								
	call ctCX_DynCstInt
	movq (%rsp), %rsi								
	testq %rsi, %rsi
	jz .printInt_cleanup
	movq %rbx, %rdi									
	movq 8(%rsp), %rdx								
	movq $1, %rax									
	syscall
	movq (%rsp), %rdi
	call _ZdaPv										
.printInt_cleanup:
	addq $32, %rsp									
	popq %rbx
	ret


#----------------------------------------------------------------------------------------
# FUN: ctCX_GetWritable -- Returns raw writable char* from the struct
# Needed when external C APIs (like mkostemp) need to modify the buffer in-place
# Inputs:  %rdi = struct ptr
# Outputs: %rax = char* (writable, direct pointer into heap buffer)
#----------------------------------------------------------------------------------------
.global ctCX_GetWritable
.type ctCX_GetWritable, @function
ctCX_GetWritable:
    movq (%rdi), %rax       # Offset 0: base buffer pointer
    ret

#----------------------------------------------------------------------------------------
# FUN: ctCX_RecalcLen -- Resyncs length field after external in-place modification
# Call this after mkostemp or any C API that writes into the buffer directly
# Inputs:  %rdi = struct ptr
# Outputs: void (updates Offset 8 in struct with new strlen)
#----------------------------------------------------------------------------------------
.global ctCX_RecalcLen
.type ctCX_RecalcLen, @function
ctCX_RecalcLen:
    movq (%rdi), %rsi       # %rsi = base buffer pointer
    testq %rsi, %rsi
    jz .oCX_rCL_NullBuf

    xorq %rcx, %rcx
.rCX_rCL_WalkLoop:
    movb (%rsi, %rcx), %al
    testb %al, %al
    jz .oCX_rCL_WalkDone
    incq %rcx
    jmp .rCX_rCL_WalkLoop
.oCX_rCL_WalkDone:
    movq %rcx, 8(%rdi)      # write new length into Offset 8
    ret
.oCX_rCL_NullBuf:
    movq $0, 8(%rdi)
    ret
