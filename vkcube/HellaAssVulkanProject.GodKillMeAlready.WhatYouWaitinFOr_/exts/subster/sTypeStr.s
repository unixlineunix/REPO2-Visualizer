; Sect: 1 - Data struct Def'n

.section .data 										; Kainda feels like writing a raw application 
.align 8											; So this tells the CPU to align memory blocks clanely by 8 bytes
;													; NGL: basically 64 bits

.global DynmStrg:
DynStrg:
	.quad 0
	.quad 0
	.quad 0

; Sect: 2 - Main Code

.section .text

;----------------------------------------------------------------------------------------
; FUN: ctCX_Append -- Appends raw C-Style text to the C++ string wrapper
;
; Inputs from c++:
; 	%rdi = Addr where 'DynStrg' structure lives
; 	%rsi = Addr of the incoming raw text block we want to add
; ---------------------------------------------------------------------------------------

.global  ctCX_Append
.type ctCX_Append, @function
ctCX_Append:
	; --- Step: 1 - Safeguard callerl's reg's --- ;
	pushq %rbx										; Backup: original %rbx --> stack
	pushq %r12										; Backup: original %r12 --> stack
	pushq %r13										; Backup: original %r13 --> stack

	; --- Step: 2 - Lock Input Arg's into Safe Reg's 	--- ;
	movq %rdi, %rbx									; Move structure location pointer into %rbx
	movq %rsi, %r12									; Mov incoming text location pointer into %r12

	; --- Step: 3 - Find the .len() if incomming text 	--- ;
	; --- RAW: custom Inline Strlen FUN 				--- ;
	xorq %r13, %r13 				; This set's length counter to 0

.rCX_Strg_LenLoop:
	; Load a single byte from the addr: %12 -: Apprently : :- incomming text base : + %13 :- counter index :
	; %r8b is :] smallest 8 bit pocket --->^ %r8 Reg, :] perfect for holding a single character . .. letter
	movb (%r12, %r13), %r8b 

	testb %r8b, %r8b								; Test: GrabbedChar != 0 : nullTerm -:		
	jz .sCX_rfCX_Strg_LenSCC						; sCX_rfCX_SCC:: 0 == true :- end of thex $= break -:

	incq %r13										; Test: !0 Lencount++ :
	jmp .rCX_Strg_LenLoop							; Loop: Look :: NEXT CHAR POS

.rCX_rdStrg_Len:
	testq %r13, %r13								; Calc: is === 0 :?
	jz .ctCX_C_EE_C_0 								; Check: == 0, case true: Skip && quit

	; --- Step: 4 - Calculate req:? space 	--- ;
	movq 8(%rbx), %rax 							; Read: CurrentLen (Offset: 8) --> structure to %rax
	addq %r13, %rax 								; Add: incoming text length -: r13 --: into:: %rax
	incq %rax 										; Add: 1 extra byte -:>  the text's ending null stop-sign :/ CKA: '\0'

	; --- Step: 5 - Check if Buffer got Room ---;
	movq 16(%rbx), %rcx 							; Read: Max Cap (Offset: 16) 
	cmpq %rcx, %rax 								; Compare: %rcx :- MaxCap :-] :- %rax :Total needed space
	jbe .rCX_oCP_TXT 								; Test: MaxCap >= , *eh~ Mem alloc				

	; --- Step 6: Compute New Cap cap? --- ;
	testq %rcx, %rcx 								; Check: MaxCap = 0?
	jnz .dCX_DubCap 								; Check: !0 :: Jmp: down - DubSize
	movq $256, %rcx 								; Check: = empty/null :) Default: initial size -* 256
	jmp .oCX_Alloc 									; Skip: DUbLoop | ProceedL Fwd

.dCX_DubCap:
	shlq $1, %rcx 									; Shift: left binary bit --> %rcx -> 1 :] (Fast multiply capacity by 2)... IDK bruh

.oCX_Alloc:
	; --- Step: 7 - Bound check the new Cap --- ;
	cmpq %rcx, %rax 								; Compare: %rcx :- DubCap : :-] %rcx :- Needed space :

.oCX_AllocMem:
	; --- Step: 8 - Request: 2 cups of Fresh Memory via C++ please --- ;
	pushq %rax 										; Put: total needspace - calc ---> stack --> protect .
	pushq %rcx 										; Put: target Cap calculation ---> stack --> protect .

	mov %rcx, %rdi 									; Setup: First arg for C++ arr -] allocator :| required size
	call _Znam

	popq %rcx
	popq %rdx

	movq (%rbx), %rdi
	jz .oCX_SK_O_CP

	pushq %rax
	pushq %rcx

	movq %rax, %rdi
	movq (%rbx), %rsi
	movq 8(%rbx), %rdx
	call memcpy

	movq 16(%rsp), %rdi
	call _ZdaPv

	popq %rcx
	popq %rax

.oCX_SK_O_CP:
	movq %rax, (%rbx)
	movq %rcx, 16(%rbx)

.rCX_oCP_TXT:
	movq (%rbx), %rdi
	addq 8(%rbx), %rdi
	movq %r12, %rsi
	movq %r13, %rdx
	call memcpy

	addq %r13, 8(%rbx)
	movq (%rbx), %rax
	addq 8(%rbx), %rax
	movb $0, (%rax)

.ctCX_C_EE_C_0:
	popq %r13
	popq %r12
	popq %rbx
	ret

;

.global ctCX_DynCstInt
.type ctCX_DynCstInt, @function

ctCX_DynCstInt:
	pushq %rbx
	movq %rdi, %rbx

	subq $32, %rsp
	leaq 32(%rsp), %rcx
	movb $0, (%rcx)

	movq %rsi, %rax
	movq $10, %r8

.oCX_cst_Loop:
	xorq %rdx, %rdx
	divq %r8
	addb $48, %dl
	decq %rcx
	movb %dl, (%rcx)
	testq %rax, %rax
	jnz .oCX_cst_Loop

	movq %rbx, %rdi
	movq %rcx, %rsi
	call ctCX_Append

	addq $32, %rsp
	popq %rbx
	ret

.global ctCX_Free
.type ctCX_Free, @function
ctCX_Free:
	movq (%rdi), %rsi
	testq %rsi, %rsi
	jz .ctFree_C_DN
	pushq %rdi
	movq %rsi, %rdi
	call _ZdaPv
	popq %rdi

	mov $0, (%rdi)
	movq $0, 8(%rdi)
	movq $0, 16(%rdi)

.ctFree_C_DN:
	ret

