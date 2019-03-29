PUBLIC vm_interpret, vm_init
.data
handlers  WORD 128 dup (?) ; dispatch target locations, initialized in vm_init
native_functions QWORD (0) ; pointer to array of native functions
.code


DISPATCH MACRO 
	; put operands in r9
	mov r9, QWORD PTR [r8] 
	movzx rax, r9w
	add rax, r12
	jmp rax
ENDM

; interprets the given bytecode
; rcx : pointer to first bytecode instruction

vm_interpret PROC

; save nonvolatile registers rbx, r12, r13
push rbx
push r12
push r13
push r14
push r15

; move begin_interp offset into r12, used for dispatch
mov r12, OFFSET begin_interp

; move register pointer into r13, used for register operations
mov r13, rsp

; r8 contains the pointer to data
; r9 contains the operands
; r13 contains register base
; rsp contains stack pointer

; mov argument into r8
mov r8, rcx

; initial dispatch
DISPATCH

begin_interp LABEL NEAR PTR WORD

; BEGIN nop
lbl_ui32 LABEL NEAR PTR WORD
nop_ LABEL NEAR PTR WORD
	inc r8
	DISPATCH
; END nop

; BEGIN add reg reg reg
add_r64_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov rax, [r13 + rax]
	add rax, [r13 + r9]
	mov [r13 + rbx], rax

	add r8, 5
	DISPATCH
; END add reg reg reg


; BEGIN add reg reg ui8
add_r64_r64_ui8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets literal
	; rax gets second source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	add r9, [r13 + rax]
	mov [r13 + rbx], r9

	add r8, 5
	DISPATCH
; END add reg reg ui8

; BEGIN sub reg reg reg
sub_r64_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov rax, [r13 + rax]
	sub rax, [r13 + r9]
	mov [r13 + rbx], rax

	add r8, 5
	DISPATCH
; END sub reg reg reg

; BEGIN sub reg reg ui8
sub_r64_r64_ui8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets literal
	; rax gets source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov rcx, [r13 + rax]
	sub rcx, r9
	mov [r13 + rbx], rcx

	add r8, 5
	DISPATCH
; END sub reg reg ui8

; BEGIN mul reg reg reg
mul_r64_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov rax, [r13 + rax]
	imul QWORD PTR [r13 + r9]
	mov [r13 + rbx], rax

	add r8, 5
	DISPATCH
; END mul reg reg reg

; BEGIN div reg reg reg
div_r64_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; this is slightly different from regular since we want the numerator in rax

	; rax gets first source
	; r9  gets second source
	; rbx gets target

	shr rax, 8
	and rax, 0ffh
	neg rax

	shr r9, 16
	and r9, 0ffh
	neg r9

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov rax, [r13 + rax]
	; div uses rax:rdx so make sure rdx is clears
	xor rdx, rdx
	div QWORD PTR [r13 + r9]
	mov [r13 + rbx], rax

	add r8, 5
	DISPATCH
; END div reg reg reg

; BEGIN mod reg reg reg
mod_r64_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; this is slightly different from regular (but the same as div) since we want the numerator in rax

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr rax, 8
	and rax, 0ffh
	neg rax

	shr r9, 16
	and r9, 0ffh
	neg r9

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov rax, [r13 + rax]
	; div uses rax:rdx so make sure rdx is clears
	xor rdx, rdx
	div QWORD PTR [r13 + r9]
	mov [r13 + rbx], rdx

	add r8, 5
	DISPATCH
; END mod reg reg reg

; BEGIN gt reg reg reg
gt_r8_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; rbx gets target
	; rax gets first source
	; r9  gets second source

	shr rax, 8
	and rax, 0ffh
	neg rax

	shr r9, 16
	and r9, 0ffh
	neg r9

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovg rdx, rcx

	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END gt reg reg reg


; BEGIN gt r8 r8 r8
gt_r8_r8_r8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; rbx gets target
	; rax gets first source
	; r9  gets second source

	shr rax, 8
	and rax, 0ffh
	neg rax

	shr r9, 16
	and r9, 0ffh
	neg r9

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9b, BYTE PTR [r13 + r9]
	mov al, BYTE PTR [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp al, r9b
	cmovg rdx, rcx

	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END gt reg reg reg

; BEGIN gte reg reg reg
gte_r8_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; rbx gets target
	; rax gets first source
	; r9  gets second source

	shr rax, 8
	and rax, 0ffh
	neg rax

	shr r9, 16
	and r9, 0ffh
	neg r9

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovge rdx, rcx
	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END gte reg reg reg


; BEGIN gte reg reg reg
gte_r8_r8_r8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; rbx gets target
	; rax gets first source
	; r9  gets second source

	shr rax, 8
	and rax, 0ffh
	neg rax

	shr r9, 16
	and r9, 0ffh
	neg r9

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9b, BYTE PTR [r13 + r9]
	mov al, BYTE PTR [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovge rdx, rcx
	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END gte reg reg reg

; BEGIN lt reg reg reg
lt_r8_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; rbx gets target
	; rax gets first source
	; r9  gets second source

	shr rax, 8
	and rax, 0ffh
	neg rax

	shr r9, 16
	and r9, 0ffh
	neg r9

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovl rdx, rcx
	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END lt reg reg reg

; BEGIN lt reg reg reg
lt_r8_r8_r8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; rbx gets target
	; rax gets first source
	; r9  gets second source

	shr rax, 8
	and rax, 0ffh
	neg rax

	shr r9, 16
	and r9, 0ffh
	neg r9

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9b, BYTE PTR [r13 + r9]
	mov al, BYTE PTR [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovl rdx, rcx
	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END lt reg reg reg

; BEGIN lte reg reg reg
lte_r8_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; rbx gets target
	; rax gets first source
	; r9  gets second source

	shr rax, 8
	and rax, 0ffh
	neg rax

	shr r9, 16
	and r9, 0ffh
	neg r9

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovle rdx, rcx
	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END lte reg reg reg


; BEGIN lte reg reg reg
lte_r8_r8_r8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; rbx gets target
	; rax gets first source
	; r9  gets second source

	shr rax, 8
	and rax, 0ffh
	neg rax

	shr r9, 16
	and r9, 0ffh
	neg r9

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9b, BYTE PTR [r13 + r9]
	mov al, BYTE PTR [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovle rdx, rcx
	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END lte reg reg reg

; BEGIN eq reg reg reg
eq_r8_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rdx
	mov rdx, 0
	; conditional move 1 into rdx
	mov rcx, 1
	cmp rax, r9
	cmove rdx, rcx
	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END eq reg reg reg

; BEGIN eq reg reg reg
eq_r8_r8_r8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9b, BYTE PTR [r13 + r9]
	mov al, BYTE PTR [r13 + rax]
	; default move 0 into rdx
	mov rdx, 0
	; conditional move 1 into rdx
	mov rcx, 1
	cmp rax, r9
	cmove rdx, rcx
	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END eq reg reg reg

; BEGIN neq reg reg reg
neq_r8_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovne rdx, rcx
	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END neq reg reg reg 

; BEGIN neq reg reg reg
neq_r8_r8_r8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9b, BYTE PTR [r13 + r9]
	mov al, BYTE PTR [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovne rdx, rcx
	mov BYTE PTR [r13 + rbx], dl

	add r8, 5
	DISPATCH
; END neq reg reg reg 

; BEGIN and reg reg reg
and_r64_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9, [r13 + r9]
	and r9, [r13 + rax]
	mov [r13 + rbx], r9

	add r8, 5
	DISPATCH
; END and reg reg reg 

; BEGIN and reg reg reg
and_r8_r8_r8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9b, BYTE PTR [r13 + r9]
	and r9b, BYTE PTR [r13 + rax]
	mov BYTE PTR [r13 + rbx], r9b

	add r8, 5
	DISPATCH
; END and reg reg reg 

; BEGIN and reg reg ui8
and_r8_r8_ui8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets literal 
	; rax gets source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov al, BYTE PTR [r13 + rax]
	and r9b, al
	mov BYTE PTR [r13 + rbx], r9b

	add r8, 5
	DISPATCH
; END and reg reg ui8 

; BEGIN or reg reg reg
or_r64_r64_r64 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9, [r13 + r9]
	or r9, [r13 + rax]
	mov [r13 + rbx], r9

	add r8, 5
	DISPATCH
; END or reg reg reg 

; BEGIN or reg reg reg
or_r8_r8_r8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets second source
	; rax gets first source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh
	neg r9

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov r9b, BYTE PTR [r13 + r9]
	or r9b, BYTE PTR [r13 + rax]
	mov BYTE PTR [r13 + rbx], r9b

	add r8, 5
	DISPATCH
; END or reg reg reg 

; BEGIN xor reg reg ui8
xor_r8_r8_ui8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9  gets literal 
	; rax gets source
	; rbx gets target

	shr r9, 16
	and r9, 0ffh

	shr rax, 8
	and rax, 0ffh
	neg rax

	and rbx, 0ffh
	neg rbx

	; perform ops

	mov cl, BYTE PTR [r13 + rbx]
	xor cl, r9b
	mov BYTE PTR [r13 + rax], cl

	add r8, 5
	DISPATCH
; END xor reg reg ui8

; BEGIN mv reg ip
mv_reg_ip LABEL NEAR PTR WORD
	shr r9, 16
	and r9, 0ffh
	mov rax, [r13 + 63*8]
	mov [r13 + r9*8], rax

	; trigger fault because this is outdated
	mov r8, 0

	add r8, 3
	DISPATCH
; END mv reg ip

; BEGIN mv reg ui8 | mv reg i8
mv_reg_i8 LABEL NEAR PTR WORD
mv_reg_ui8 LABEL NEAR PTR WORD
	shr r9, 16

	; rax gets target
	; r9  gets literal
	xor rax, rax
	mov al, r9b 
	neg rax

	shr r9, 8
	and r9, 0ffh

	mov BYTE PTR [r13 + rax], r9b

	add r8, 4
	DISPATCH
; END mv reg ui8 | mv reg i8

; BEGIN mv reg ui16 | mv reg i16
mv_reg_i16 LABEL NEAR PTR WORD
mv_reg_ui16 LABEL NEAR PTR WORD
	shr r9, 16

	; rax gets target
	; r9  gets literal
	xor rax, rax
	mov al, r9b 
	neg rax

	shr r9, 8
	and r9, 0ffffh

	mov [r13 + rax], r9

	add r8, 5
	DISPATCH
; END mv reg ui16 | mv reg i16

; BEGIN mv reg ui32 | mv reg ui32
mv_reg_i32 LABEL NEAR PTR WORD
mv_reg_ui32 LABEL NEAR PTR WORD
	shr r9, 16

	; rax gets reg
	; r9  gets literal
	xor rax, rax
	mov al, r9b 
	neg rax

	shr r9, 8
	and r9, 0ffffffffh

	mov [r13 + rax], r9

	add r8, 7
	DISPATCH
; END mv reg ui32 | mv reg i32

; BEGIN mv reg ui64 | mv reg i64
mv_reg_i64 LABEL NEAR PTR WORD
mv_reg_ui64 LABEL NEAR PTR WORD
	mov rax, QWORD PTR [r8 + 3]

	; r9 gets target
	shr r9, 16
	and r9, 0ffh
	neg r9

	mov [r13 + r9], rax

	add r8, 11
	DISPATCH
; END mv reg ui64 | mv reg i64

; BEGIN mv count reg reg
mv_rn_rn LABEL NEAR PTR WORD
	shr r9, 16

	; rax gets count
	xor rax, rax
	mov al, r9b

	; rbx gets dest
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b
	neg rbx

	; r9 gets source
	shr r9, 8
	and r9, 0ffh
	neg r9

	; move as many 8 bytes as posible
	_mv_8_mv_rn_rn:
	cmp rax, 8
	jl _mv_4_mv_rn_rn

	mov rcx, QWORD PTR [r13 + r9]
	mov QWORD PTR [r13 + rbx], rcx
	sub rax, 8
	add r9, 8
	add rbx, 8

	jmp _mv_8_mv_rn_rn

	; move as many 4 bytes as posible
	_mv_4_mv_rn_rn:
	cmp rax, 4
	jl _mv_2_mv_rn_rn

	mov ecx, DWORD PTR [r13 + r9]
	mov DWORD PTR [r13 + rbx], ecx
	sub rax, 4
	add r9, 4
	add rbx, 4

	jmp _mv_4_mv_rn_rn

	; move as many 2 bytes as posible
	_mv_2_mv_rn_rn:
	cmp rax, 2
	jl _mv_1_mv_rn_rn

	mov cx, WORD PTR [r13 + r9]
	mov WORD PTR [r13 + rbx], cx
	sub rax, 2
	add r9, 2
	add rbx, 2

	jmp _mv_2_mv_rn_rn

	; move last byte if possible
	_mv_1_mv_rn_rn:
	cmp rax, 1
	jl _mv_0_mv_rn_rn

	mov cl, BYTE PTR [r13 + r9]
	mov BYTE PTR [r13 + rbx], cl

	_mv_0_mv_rn_rn:

	add r8, 5
	DISPATCH
; END mv count reg reg

; BEGIN mv count rn ln
mv_rn_ln LABEL NEAR PTR WORD
	shr r9, 16

	; rax gets count
	xor rax, rax
	mov al, r9b

	; rbx gets dest
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b
	neg rbx

	; r9 gets source
	shr r9, 8
	and r9, 0ffh
	neg r9
	mov r9, QWORD PTR [r13 + r9]
	neg r9

	; move as many 8 bytes as posible
	_mv_8_mv_rn_ln:
	cmp rax, 8
	jl _mv_4_mv_rn_ln

	mov rcx, QWORD PTR [r13 + r9]
	mov QWORD PTR [r13 + rbx], rcx
	sub rax, 8
	add r9, 8
	add rbx, 8

	jmp _mv_8_mv_rn_ln

	; move as many 4 bytes as posible
	_mv_4_mv_rn_ln:
	cmp rax, 4
	jl _mv_2_mv_rn_ln

	mov ecx, DWORD PTR [r13 + r9]
	mov DWORD PTR [r13 + rbx], ecx
	sub rax, 4
	add r9, 4
	add rbx, 4

	jmp _mv_4_mv_rn_ln

	; move as many 2 bytes as posible
	_mv_2_mv_rn_ln:
	cmp rax, 2
	jl _mv_1_mv_rn_ln

	mov cx, WORD PTR [r13 + r9]
	mov WORD PTR [r13 + rbx], cx
	sub rax, 2
	add r9, 2
	add rbx, 2

	jmp _mv_2_mv_rn_ln

	; move last byte if possible
	_mv_1_mv_rn_ln:
	cmp rax, 1
	jl _mv_0_mv_rn_ln

	mov cl, BYTE PTR [r13 + r9]
	mov BYTE PTR [r13 + rbx], cl

	_mv_0_mv_rn_ln:

	add r8, 5
	DISPATCH
; END mv count rn ln

; BEGIN jmpr i32
jmpr_i32 LABEL NEAR PTR WORD
	; r9 gets offset
	shr r9, 16
	movsxd r9, r9d
	add r8, r9
	DISPATCH
; END jmpr i32

; BEGIN jrnz reg i32
jrnz_reg_i32 LABEL NEAR PTR WORD
	shr r9, 16

	; rax gets test
	xor rax, rax
	mov al, r9b
	neg rax

	; r9 gets offset
	shr r9, 8
	movsxd r9, r9d

	mov al, BYTE PTR [r13 + rax]

	cmp al, 0
	je _skip_jrnz
	add r8, r9
	DISPATCH
_skip_jrnz:
	add r8, 7
	DISPATCH
; END jrnz reg i32

; BEGIN jrz reg i32
jrz_reg_i32 LABEL NEAR PTR WORD
	shr r9, 16

	; rax gets test
	xor rax, rax
	mov al, r9b
	neg rax

	; r9 gets offset
	shr r9, 8
	movsxd r9, r9d

	mov al, BYTE PTR [r13 + rax]

	cmp al, 0
	jne _skip_jrz
	add r8, r9
	DISPATCH
_skip_jrz:
	add r8, 7
	DISPATCH
; END jrz reg i32

; BEGIN call ui64 ui8 ui8 ui8
call_ui64_ui8_ui8_ui8 LABEL NEAR PTR WORD
	; rax gets offset
	mov rax, QWORD PTR [r8 + 2]

	; r14 gets first arg register
	mov r14b, BYTE PTR [r8 + 10]
	and r14, 0ffh
	neg r14

	; r9 gets register count
	mov r9b, BYTE PTR [r8 + 11]
	and r9, 0ffh

	; rdx contains first return reg
	mov dl, BYTE PTR [r8 + 12]
	and rdx, 0ffh

	; simulate call

	; push the next op so we return and continue execution after this op
	mov rcx, r8
	add rcx, 13
	push rcx

	; update ip to function entry location
	add r8, rax

	; push the current register base
	push r13

	; push the first return reg
	push rdx

	; load location of source registers
	lea rdx, [r13 + r14]
	
	; set new register base
	lea r13, [rsp - 1]

	; allocate frame size
	sub rsp, r9

	mov rax, 0

	; move args
before_args:
	cmp r9, 0
	je skip_args

	dec r9

	mov cl, BYTE PTR [rdx + r9]
	mov BYTE PTR [r13 + rax], cl

	dec rax

	jmp before_args
skip_args:

	; put operands in r9
	mov r9, QWORD PTR [r8] 
	movzx rax, r9w
	add rax, r12
	jmp rax
	DISPATCH
; END call ui64 ui8 ui8 ui8

; BEGIN alloc ui8
alloc_ui8 LABEL NEAR PTR WORD
	; r9 gets alloc size
	shr r9, 16
	and r9, 0ffh
	sub rsp, r9
	add r8, 3
	DISPATCH
; END alloc ui8

; BEGIN ret ui8 ui8 ui8 ui8
ret_ui8_ui8_ui8_ui8 LABEL NEAR PTR WORD
	; rax gets param size 
	shr r9, 16
	mov rax, r9
	and rax, 0ffh

	; rbx gets rest-of-frame size
	shr r9, 8
	mov rbx, r9
	and rbx, 0ffh

	; rcx gets callee return register
	shr r9, 8
	mov rcx, r9
	and rcx, 0ffh
	neg rcx

	; r9l contains return count
	shr r9, 8
	and r9, 0ffh
	neg r9

	add rsp, rbx
	add rsp, rax

	; pop first caller return register
	pop rdx
	neg rdx
	
	; move current register base into rax
	mov rax, r13

	; restore previous register base
	pop r13

	; pointer to first return register
	lea rax, [rax + rcx]
	; pointer to first result register
	lea rcx, [r13 + rdx]

	neg r9

	; move return values to prev frame
begin_return_vals:
	cmp r9, 0
	je skip_return_vals

	dec r9

	; load return reg
	mov bl, BYTE PTR [rax + r9]
	; store result reg
	mov BYTE PTR [rcx + r9], bl

	jmp begin_return_vals

skip_return_vals:
	; restore ip
	pop r8

	DISPATCH
; END ret ui8 ui8 ui8 ui8

; BEGIN call native ui64 ui8 ui8
call_native_ui64_ui8_ui8 LABEL NEAR PTR WORD
	mov rax, QWORD PTR [r8 + 2]
	mov rbx, [native_functions]
	mov rbx, [rbx + rax*8]
	mov r9, QWORD PTR [r8 + 10]

	; rcx gets first register 
	xor rcx, rcx
	mov cl, r9b
	neg rcx

	; rdx gets register count
	shr r9, 8
	xor rdx, rdx
	mov dl, r9b

	; save r8 in non-volatile and restore after
	mov r14, r8

	; push right to left, stack pointer then register pointer
	lea rcx, [r13 + rcx]
	sub rsp, 32
	call rbx
	add rsp, 32
	mov r8, r14

	add r8, 12
	DISPATCH
; END call native ui64 ui8 ui8

; BEGIN exit
exit LABEL NEAR PTR WORD
	; restore nonvolatile registers rbx, r12, r13, r14, r15
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	ret
; END exit

vm_interpret ENDP

; initializes each handler pointer

vm_init PROC
; store the pointer to native function pointers
mov native_functions, rcx

lea rax, handlers
 
lea rbx, begin_interp

mov rdx, 0

lea rcx, nop_
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, exit 
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx ; skip err opcode

inc rdx
lea rcx, lbl_ui32
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, add_r64_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, add_r64_r64_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, sub_r64_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, sub_r64_r64_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mul_r64_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, div_r64_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mod_r64_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, gt_r8_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, gt_r8_r8_r8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, gte_r8_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, gte_r8_r8_r8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, lt_r8_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, lt_r8_r8_r8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, lte_r8_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, lte_r8_r8_r8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, eq_r8_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, eq_r8_r8_r8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, neq_r8_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, neq_r8_r8_r8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, and_r64_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, and_r8_r8_r8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, and_r8_r8_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, or_r64_r64_r64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, or_r8_r8_r8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, xor_r8_r8_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv_reg_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv_reg_ui16
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv_reg_ui32
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv_reg_ui64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv_reg_i8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv_reg_i16
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv_reg_i32
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv_reg_i64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv_rn_rn
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv_rn_ln
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, jmpr_i32
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, jrnz_reg_i32
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, jrz_reg_i32
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, call_ui64_ui8_ui8_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, call_native_ui64_ui8_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, alloc_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, ret_ui8_ui8_ui8_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

ret
vm_init ENDP

END
