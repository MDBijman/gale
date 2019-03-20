PUBLIC vm_interpret, vm_init
.data
handlers  WORD 64 dup (?) ; dispatch target locations, initialized in vm_init
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
	add r8, 1
	DISPATCH
; END nop

; BEGIN add reg reg reg
add_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 16
	shr rax, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov rax, [r13 + rax]
	add rax, [r13 + r9]
	mov [r13 + rbx], rax

	add r8, 5
	DISPATCH
; END add reg reg reg


; BEGIN add reg reg ui8
add_reg_reg_ui8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9b gets literal
	; al gets second source
	; bl gets target
	shr r9, 16
	shr rax, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh
	imul rax, -8
	imul rbx, -8

	add r9, [r13 + rax]
	mov [r13 + rbx], r9

	add r8, 5
	DISPATCH
; END add reg reg ui8

; BEGIN sub reg reg reg
sub_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 16
	shr rax, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov rax, [r13 + rax]
	sub rax, [r13 + r9]
	mov [r13 + rbx], rax

	add r8, 5
	DISPATCH
; END sub reg reg reg

; BEGIN sub reg reg ui8
sub_reg_reg_ui8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9b gets literal
	; al gets source
	; bl gets target
	shr r9, 16
	shr rax, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh
	imul rax, -8
	imul rbx, -8

	mov rcx, [r13 + rax]
	sub rcx, r9
	mov [r13 + rbx], rcx

	add r8, 5
	DISPATCH
; END sub reg reg ui8

; BEGIN mul reg reg reg
mul_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr rax, 8
	shr r9, 16
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov rax, [r13 + rax]
	imul QWORD PTR [r13 + r9]
	mov [r13 + rbx], rax

	add r8, 5
	DISPATCH
; END mul reg reg reg

; BEGIN div reg reg reg
div_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; this is slightly different from regular since we want the numerator in rax

	; al gets first source
	; r9b gets second source
	; bl gets target
	shr rax, 8
	shr r9, 16
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov rax, [r13 + rax]
	; div uses rax:rdx so make sure rdx is clears
	xor rdx, rdx
	div QWORD PTR [r13 + r9]
	mov [r13 + rbx], rax

	add r8, 5
	DISPATCH
; END div reg reg reg

; BEGIN mod reg reg reg
mod_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; this is slightly different from regular (but the same as div) since we want the numerator in rax

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr rax, 8
	shr r9, 16
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov rax, [r13 + rax]
	; div uses rax:rdx so make sure rdx is clears
	xor rdx, rdx
	div QWORD PTR [r13 + r9]
	mov [r13 + rbx], rdx

	add r8, 5
	DISPATCH
; END mod reg reg reg

; BEGIN gt reg reg reg
gt_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; bl gets target
	; al gets first source
	; r9b gets second source
	shr rax, 8
	shr r9, 16
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovg rdx, rcx

	mov [r13 + rbx], rdx

	add r8, 5
	DISPATCH
; END gt reg reg reg

; BEGIN gte reg reg reg
gte_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; bl gets target
	; al gets first source
	; r9b gets second source
	shr rax, 8
	shr r9, 16
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovge rdx, rcx
	mov [r13 + rbx], rdx

	add r8, 5
	DISPATCH
; END gte reg reg reg

; BEGIN lt reg reg reg
lt_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; bl gets target
	; al gets first source
	; r9b gets second source
	shr rax, 8
	shr r9, 16
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovl rdx, rcx
	mov [r13 + rbx], rdx

	add r8, 5
	DISPATCH
; END lt reg reg reg

; BEGIN lte reg reg reg
lte_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; bl gets target
	; al gets first source
	; r9b gets second source
	shr rax, 8
	shr r9, 16
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovle rdx, rcx
	mov [r13 + rbx], rdx

	add r8, 5
	DISPATCH
; END lte reg reg reg

; BEGIN lte reg reg ui8
lte_reg_reg_ui8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; bl gets target
	; al gets first source
	; r9b gets literal
	shr rax, 8
	shr r9, 16
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh
	imul rax, -8
	imul rbx, -8

	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovle rdx, rcx
	mov [r13 + rbx], rdx

	add r8, 5
	DISPATCH
; END lte reg reg ui8

; BEGIN eq reg reg reg
eq_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 16
	shr rax, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rdx
	mov rdx, 0
	; conditional move 1 into rdx
	mov rcx, 1
	cmp rax, r9
	cmove rdx, rcx
	mov [r13 + rbx], rdx

	add r8, 5
	DISPATCH
; END eq reg reg reg

; BEGIN neq reg reg reg
neq_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 16
	shr rax, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov r9, [r13 + r9]
	mov rax, [r13 + rax]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovne rdx, rcx
	mov [r13 + rbx], rdx

	add r8, 5
	DISPATCH
; END neq reg reg reg 

; BEGIN and reg reg reg
and_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 16
	shr rax, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov r9, [r13 + r9]
	and r9, [r13 + rax]
	mov [r13 + rbx], r9

	add r8, 5
	DISPATCH
; END and reg reg reg 

; BEGIN and reg reg ui8
and_reg_reg_ui8 LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9b gets literal
	; al gets source
	; bl gets target
	shr r9, 16
	shr rax, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh
	imul rax, -8
	imul rbx, -8

	mov rax, [r13 + rax]
	and r9, rax
	mov [r13 + rbx], r9

	add r8, 5
	DISPATCH
; END and reg reg ui8 

; BEGIN or reg reg reg
or_reg_reg_reg LABEL NEAR PTR WORD
	shr r9, 16
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 16
	shr rax, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh
	imul rax, -8
	imul r9, -8
	imul rbx, -8

	mov r9, [r13 + r9]
	or r9, [r13 + rax]
	mov [r13 + rbx], r9

	add r8, 5
	DISPATCH
; END or reg reg reg 

; BEGIN xor reg reg ui8
xor_reg_reg_ui8 LABEL NEAR PTR WORD
	; al contains target
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b
	; r9 contains literal
	shr r9, 8
	and r9, 0ffh
	imul rax, -8
	imul rbx, -8

	mov rcx, QWORD PTR [r13 + rbx]
	xor rcx, r9
	mov QWORD PTR [r13 + rax], rcx

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
	xor rax, rax
	; al contains reg
	mov al, r9b 
	shr r9, 8
	and r9, 0ffh
	imul rax, -8
	; r9 contains literal
	mov [r13 + rax], r9

	add r8, 4
	DISPATCH
; END mv reg ui8 | mv reg i8

; BEGIN mv reg ui16 | mv reg i16
mv_reg_i16 LABEL NEAR PTR WORD
mv_reg_ui16 LABEL NEAR PTR WORD
	shr r9, 16
	xor rax, rax
	; al contains reg
	mov al, r9b 
	shr r9, 8
	and r9, 0ffffh
	imul rax, -8
	; r9 contains literal
	mov [r13 + rax], r9

	add r8, 5
	DISPATCH
; END mv reg ui16 | mv reg i16

; BEGIN mv reg ui32 | mv reg ui32
mv_reg_i32 LABEL NEAR PTR WORD
mv_reg_ui32 LABEL NEAR PTR WORD
	shr r9, 16
	xor rax, rax
	; al contains reg
	mov al, r9b 
	imul rax, -8
	shr r9, 8
	and r9, 0ffffffffh
	; r9 contains literal
	mov [r13 + rax], r9

	add r8, 7
	DISPATCH
; END mv reg ui32 | mv reg i32

; BEGIN mv reg ui64 | mv reg i64
mv_reg_i64 LABEL NEAR PTR WORD
mv_reg_ui64 LABEL NEAR PTR WORD
	mov rax, QWORD PTR [r8 + 3]
	shr r9, 16
	and r9, 0ffh
	imul r9, -8
	mov [r13 + r9], rax

	add r8, 11
	DISPATCH
; END mv reg ui64 | mv reg i64

; BEGIN mv8 reg reg
mv8_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 16
	xor rax, rax
	mov al, r9b
	imul rax, -8
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b
	imul rbx, -8

	mov rcx, QWORD PTR [r13 + rbx]
	and rcx, 0ffh
	mov QWORD PTR [r13 + rax], rcx

	add r8, 4
	DISPATCH
; END mv8 reg reg

; BEGIN mv16 reg reg
mv16_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 16
	xor rax, rax
	mov al, r9b
	imul rax, -8
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b
	imul rbx, -8

	mov rcx, QWORD PTR [r13 + rbx]
	and rcx, 0ffffh
	mov QWORD PTR [r13 + rax], rcx

	add r8, 4
	DISPATCH
; END mv16 reg reg

; BEGIN mv32 reg reg
mv32_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 16
	xor rax, rax
	mov al, r9b
	imul rax, -8
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b
	imul rbx, -8

	mov rcx, QWORD PTR [r13 + rbx]
	and rcx, 0ffffffffh
	mov QWORD PTR [r13 + rax], rcx

	add r8, 4
	DISPATCH
; END mv32 reg reg

; BEGIN mv64 reg reg
mv64_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 16
	xor rax, rax
	mov al, r9b
	imul rax, -8
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b
	imul rbx, -8

	mov rcx, QWORD PTR [r13 + rbx]
	mov QWORD PTR [r13 + rax], rcx

	add r8, 4
	DISPATCH
; END mv64 reg reg

; BEGIN jmpr i32
jmpr_i32 LABEL NEAR PTR WORD
	; r9d contains offset
	shr r9, 16
	movsxd r9, r9d
	add r8, r9
	DISPATCH
; END jmpr i32

; BEGIN jrnz reg i32
jrnz_reg_i32 LABEL NEAR PTR WORD
	shr r9, 16
	; al contains reg
	xor rax, rax
	mov al, r9b
	imul rax, -8
	; r9d contains offset
	shr r9, 8
	movsxd r9, r9d

	mov rax, QWORD PTR [r13 + rax]

	cmp rax, 0
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
	; al contains reg
	xor rax, rax
	mov al, r9b
	imul rax, -8
	; r9d contains offset
	shr r9, 8
	movsxd r9, r9d

	mov rax, QWORD PTR [r13 + rax]

	cmp rax, 0
	jne _skip_jrz
	add r8, r9
	DISPATCH
_skip_jrz:
	add r8, 7
	DISPATCH
; END jrz reg i32

; BEGIN call ui64 ui8 ui8 ui8
call_ui64_ui8_ui8_ui8 LABEL NEAR PTR WORD
	; rax contains offset
	mov rax, QWORD PTR [r8 + 2]
	; r14 contains first arg register
	mov r14b, BYTE PTR [r8 + 10]
	and r14, 0ffh
	imul r14, -8
	; r9b contains register count * 8
	mov r9b, BYTE PTR [r8 + 11]
	and r9, 0ffh
	imul r9, -8
	; dl contains first return reg
	mov dl, BYTE PTR [r8 + 12]
	and rdx, 0ffh

	; simulate call

	; push the next op so we return and continue execution after this op
	mov rcx, r8
	add rcx, 13
	push rcx

	; push the current register base
	push r13

	; push the first return reg
	push rdx

	; load location of source registers
	lea rdx, [r13 + r14]
	
	; set new register base
	lea r13, [rsp - 8]

	; allocate frame size
	add rsp, r9

	; move args
before_args:
	cmp r9, 0
	je skip_args

	add r9, 8

	mov rcx, [rdx + r9]
	mov [r13 + r9], rcx
	jmp before_args
skip_args:

	; update ip to function entry location
	add r8, rax

	DISPATCH
; END call ui64 ui8 ui8 ui8

; BEGIN alloc ui8
alloc_ui8 LABEL NEAR PTR WORD
	; al contains alloc size
	shr r9, 16
	and r9, 0ffh
	imul r9, 8
	sub rsp, r9
	add r8, 3
	DISPATCH
; END alloc ui8

; BEGIN ret ui8 ui8 ui8 ui8
ret_ui8_ui8_ui8_ui8 LABEL NEAR PTR WORD
	; al contains param size 
	shr r9, 16
	mov rax, r9
	and rax, 0ffh
	imul rax, 8

	; bl contains rest-of-frame size
	shr r9, 8
	mov rbx, r9
	and rbx, 0ffh
	imul rbx, 8

	; cl contains callee return register
	shr r9, 8
	mov rcx, r9
	and rcx, 0ffh
	imul rcx, -8

	; r9l contains return count
	shr r9, 8
	and r9, 0ffh
	imul r9, -8

	add rsp, rbx
	add rsp, rax

	; pop first caller return register
	pop rdx
	imul rdx, -8
	
	; move current register base into rax
	mov rax, r13

	; restore previous register base
	pop r13

	; pointer to first return register
	lea rax, [rax + rcx]
	; pointer to first result register
	lea rcx, [r13 + rdx]

	; move return values to prev frame
begin_return_vals:
	cmp r9, 0
	je skip_return_vals

	add r9, 8

	; load return reg
	mov rbx, [rax + r9]
	; store result reg
	mov [rcx + r9], rbx

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
	; cl gets first register 
	xor rcx, rcx
	mov cl, r9b
	imul rcx, -8
	; dl gets register count
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
lea rcx, add_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, add_reg_reg_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, sub_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, sub_reg_reg_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mul_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, div_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mod_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, gt_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, gte_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, lt_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, lte_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, lte_reg_reg_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, eq_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, neq_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, and_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, and_reg_reg_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, or_reg_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, xor_reg_reg_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

; #todo remove? #security
inc rdx
lea rcx, mv_reg_ip
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
lea rcx, mv8_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv16_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv32_reg_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv64_reg_reg
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
