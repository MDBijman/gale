PUBLIC vm_interpret, vm_init
.data
registers QWORD 64 dup (0) ; 64 registers of 64 bits each, initialized to 0
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
mov r13, OFFSET registers

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

	mov rax, [r13 + rax*8]
	add rax, [r13 + r9*8]
	mov [r13 + rbx*8], rax

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

	add r9, [r13 + rax*8]
	mov [r13 + rbx*8], r9

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

	mov rax, [r13 + rax*8]
	sub rax, [r13 + r9*8]
	mov [r13 + rbx*8], rax

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

	mov rcx, [r13 + rax*8]
	sub rcx, r9
	mov [r13 + rbx*8], rcx

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

	mov rax, [r13 + rax*8]
	imul QWORD PTR [r13 + r9*8]
	mov [r13 + rbx*8], rax

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

	mov rax, [r13 + rax*8]
	idiv QWORD PTR [r13 + r9*8]
	mov [r13 + rbx*8], rax

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

	mov rax, [r13 + rax*8]
	idiv QWORD PTR [r13 + r9*8]
	mov [r13 + rbx*8], rdx

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

	mov r9, [r13 + r9*8]
	mov rax, [r13 + rax*8]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovg rdx, rcx

	mov [r13 + rbx*8], rdx

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

	mov r9, [r13 + r9*8]
	mov rax, [r13 + rax*8]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovge rdx, rcx
	mov [r13 + rbx*8], rdx

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

	mov r9, [r13 + r9*8]
	mov rax, [r13 + rax*8]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovl rdx, rcx
	mov [r13 + rbx*8], rdx

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

	mov r9, [r13 + r9*8]
	mov rax, [r13 + rax*8]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovle rdx, rcx
	mov [r13 + rbx*8], rdx

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

	mov rax, [r13 + rax*8]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovle rdx, rcx
	mov [r13 + rbx*8], rdx

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

	mov r9, [r13 + r9*8]
	mov rax, [r13 + rax*8]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmove rdx, rcx
	mov [r13 + rbx*8], rdx

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

	mov r9, [r13 + r9*8]
	mov rax, [r13 + rax*8]
	; default move 0 into rbx
	mov rdx, 0
	; conditional move 1 into rbx
	mov rcx, 1
	cmp rax, r9
	cmovne rdx, rcx
	mov [r13 + rbx*8], rdx

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

	mov r9, [r13 + r9*8]
	and r9, [r13 + rax*8]
	mov [r13 + rbx*8], r9

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

	mov rax, [r13 + rax*8]
	and r9, rax
	mov [r13 + rbx*8], r9

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

	mov r9, [r13 + r9*8]
	or r9, [r13 + rax*8]
	mov [r13 + rbx*8], r9

	add r8, 5
	DISPATCH
; END or reg reg reg 

; BEGIN mv reg sp
mv_reg_sp LABEL NEAR PTR WORD
	shr r9, 16
	and r9, 0ffh
	mov [r13 + r9*8], rsp

	add r8, 3
	DISPATCH
; END mv reg sp

; BEGIN mv reg ip
mv_reg_ip LABEL NEAR PTR WORD
	shr r9, 16
	and r9, 0ffh
	mov rax, [r13 + 63*8]
	mov [r13 + r9*8], rax

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
	; r9 contains literal
	mov [r13 + rax*8], r9

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
	; r9 contains literal
	mov [r13 + rax*8], r9

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
	shr r9, 8
	and r9, 0ffffffffh
	; r9 contains literal
	mov [r13 + rax*8], r9

	add r8, 7
	DISPATCH
; END mv reg ui32 | mv reg i32

; BEGIN mv reg ui64 | mv reg i64
mv_reg_i64 LABEL NEAR PTR WORD
mv_reg_ui64 LABEL NEAR PTR WORD
	mov rax, QWORD PTR [r8 + 3]
	shr r9, 16
	and r9, 0ffh
	mov [r13 + r9*8], rax

	add r8, 11
	DISPATCH
; END mv reg ui64 | mv reg i64

; BEGIN mv8 reg reg
mv8_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	mov rcx, QWORD PTR [r13 + rbx*8]
	and rcx, 0ffh
	mov QWORD PTR [r13 + rax*8], rcx

	add r8, 4
	DISPATCH
; END mv8 reg reg

; BEGIN mv16 reg reg
mv16_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	mov rcx, QWORD PTR [r13 + rbx*8]
	and rcx, 0ffffh
	mov QWORD PTR [r13 + rax*8], rcx

	add r8, 4
	DISPATCH
; END mv16 reg reg

; BEGIN mv32 reg reg
mv32_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	mov rcx, QWORD PTR [r13 + rbx*8]
	and rcx, 0ffffffffh
	mov QWORD PTR [r13 + rax*8], rcx

	add r8, 4
	DISPATCH
; END mv32 reg reg

; BEGIN mv64 reg reg
mv64_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	mov rcx, QWORD PTR [r13 + rbx*8]
	mov QWORD PTR [r13 + rax*8], rcx

	add r8, 4
	DISPATCH
; END mv64 reg reg

; BEGIN mv8 loc reg
mv8_loc_reg LABEL NEAR PTR WORD
	; al contains loc
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains reg
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	mov rax, QWORD PTR [r13 + rax*8]
	mov bl, BYTE PTR [r13 + rbx*8]
	mov BYTE PTR [rax], bl

	add r8, 4
	DISPATCH
; END mv8 loc reg

; BEGIN mv16 loc reg
mv16_loc_reg LABEL NEAR PTR WORD
	; al contains loc
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains reg
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rax, QWORD PTR [r13 + rax*8]
	; get value from register
	mov bx, WORD PTR [r13 + rbx*8]
	mov WORD PTR [rax], bx

	add r8, 4
	DISPATCH
; END mv16 loc reg

; BEGIN mv32 loc reg
mv32_loc_reg LABEL NEAR PTR WORD
	; al contains loc
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains reg
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rax, QWORD PTR [r13 + rax*8]
	; get value from register
	mov ebx, DWORD PTR [r13 + rbx*8]
	mov DWORD PTR [rax], ebx

	add r8, 4
	DISPATCH
; END mv32 loc reg

; BEGIN mv64 loc reg
mv64_loc_reg LABEL NEAR PTR WORD
	; al contains loc
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains reg
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rax, QWORD PTR [r13 + rax*8]
	; get value from register
	mov rbx, QWORD PTR [r13 + rbx*8]
	mov QWORD PTR [rax], rbx

	add r8, 4
	DISPATCH
; END mv64 loc reg

; BEGIN mv8 reg loc
mv8_reg_loc LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains loc
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rbx, QWORD PTR [r13 + rbx*8]
	; get value at location
	mov bl, BYTE PTR [rbx]
	; put value in register
	mov BYTE PTR [r13 + rax*8], bl

	add r8, 4
	DISPATCH
; END mv8 reg loc

; BEGIN mv16 reg loc
mv16_reg_loc LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains loc
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rbx, QWORD PTR [r13 + rbx*8]
	; get value at location
	mov bx, WORD PTR [rbx]
	; put value in register
	mov WORD PTR [r13 + rax*8], bx

	add r8, 4
	DISPATCH
; END mv16 reg loc

; BEGIN mv32 reg loc
mv32_reg_loc LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains loc
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rbx, QWORD PTR [r13 + rbx*8]
	; get value at location
	mov ebx, DWORD PTR [rbx]
	; put value in register
	mov DWORD PTR [r13 + rax*8], ebx

	add r8, 4
	DISPATCH
; END mv32 reg loc

; BEGIN mv64 reg loc
mv64_reg_loc LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	; bl contains loc
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rbx, QWORD PTR [r13 + rbx*8]
	; get value at location
	mov rbx, QWORD PTR [rbx]
	; put value in register
	mov QWORD PTR [r13 + rax*8], rbx

	add r8, 4
	DISPATCH
; END mv64 reg loc

; BEGIN push8 reg
push8_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b

	dec rsp
	mov al, BYTE PTR [r13 + rax*8] 
	mov BYTE PTR [rsp], al
	
	add r8, 3
	DISPATCH
; END push8 reg

; BEGIN push16 reg
push16_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	
	mov ax, WORD PTR [r13 + rax*8]
	push ax

	add r8, 3
	DISPATCH
; END push16 reg

; BEGIN push32 reg
push32_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	
	sub rsp, 4
	mov eax, DWORD PTR [r13 + rax*8]
	mov [rsp], eax

	add r8, 3
	DISPATCH
; END push32 reg

; BEGIN push64 reg
push64_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	
	mov rax, QWORD PTR [r13 + rax*8]
	push rax

	add r8, 3
	DISPATCH
; END push64 reg

; BEGIN pop8 reg
pop8_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	
	mov bl, [rsp]
	inc rsp
	mov BYTE PTR [r13 + rax*8], bl

	add r8, 3
	DISPATCH
; END pop8 reg

; BEGIN pop16 reg
pop16_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	
	pop bx
	mov WORD PTR [r13 + rax*8], bx

	add r8, 3
	DISPATCH
; END pop16 reg

; BEGIN pop32 reg
pop32_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	
	mov ebx, [rsp]
	add rsp, 4
	mov DWORD PTR [r13 + rax*8], ebx

	add r8, 3
	DISPATCH
; END pop32 reg

; BEGIN pop64 reg
pop64_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 16
	xor rax, rax
	mov al, r9b
	
	pop rbx
	mov QWORD PTR [r13 + rax*8], rbx

	add r8, 3
	DISPATCH
; END pop64 reg

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
	; r9d contains offset
	shr r9, 8
	movsxd r9, r9d

	mov rax, QWORD PTR [r13 + rax*8]

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
	; r9d contains offset
	shr r9, 8
	movsxd r9, r9d

	mov rax, QWORD PTR [r13 + rax*8]

	cmp rax, 0
	jne _skip_jrz
	add r8, r9
	DISPATCH
_skip_jrz:
	add r8, 7
	DISPATCH
; END jrz reg i32

; BEGIN call ui64
call_ui64 LABEL NEAR PTR WORD
	; rax contains offset
	mov rax, QWORD PTR [r8 + 2]

	; simulate call

	; save frame pointer
	mov rbx, QWORD PTR [r13 + 61*8]
	push rbx
	; push the next op so we return and continue execution after this op
	mov rbx, r8
	add rbx, 10
	push rbx

	; set new frame pointer
	mov QWORD PTR [r13 + 61*8], rsp
	
	; update ip to function entry location
	add r8, rax

	DISPATCH
; END call ui64

; BEGIN ret ui8
ret_ui8 LABEL NEAR PTR WORD
	; r9b contains param size
	shr r9, 16
	and r9, 0ffh

	pop r8
	pop rbx
	mov QWORD PTR [r13 + 61*8], rbx
	add rsp, r9

	DISPATCH
; END ret ui8

; BEGIN call native ui64
call_native_ui64 LABEL NEAR PTR WORD
	mov rax, QWORD PTR [r8 + 2]
	mov rbx, [native_functions]
	mov rax, [rbx + rax*8]

	mov rcx, OFFSET registers
	mov rdx, rsp

	push r8
	push r9
	sub rsp, 16
	call rax
	add rsp, 16
	pop r9
	pop r8

	cmp rax, 0
	je _after

	cmp rax, 1
	inc rsp
	je _after

	cmp rax, 2
	add rsp, 1
	je _after

	cmp rax, 4
	add rsp, 2
	je _after

	cmp rax, 8
	add rsp, 4

	_after:
	add r8, 10
	DISPATCH
; END call native ui64


; BEGIN salloc reg ui8
salloc_reg_ui8 LABEL NEAR PTR WORD
	shr r9, 16
	xor rax, rax
	mov r9b, al
	shr r9, 8
	and r9, 0ffh

	sub rsp, r9
	mov [r13 + rax*8], rsp

	add r8, 4
	DISPATCH
; END salloc reg ui8

; BEGIN sdealloc ui8
sdealloc_ui8 LABEL NEAR PTR
	shr r9, 16
	and r9, 0ffh
	add rsp, r9

	add r8, 3
	DISPATCH
; END sdealloc ui8

; BEGIN exit
exit LABEL NEAR PTR WORD
	lea rax, OFFSET registers 

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
mov [native_functions], rax

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

; todo remove
inc rdx
lea rcx, mv_reg_sp
sub rcx, rbx
mov [rax + rdx*2], cx

; todo remove
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
lea rcx, mv8_loc_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv16_loc_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv32_loc_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv64_loc_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv8_reg_loc
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv16_reg_loc
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv32_reg_loc
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, mv64_reg_loc
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, push8_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, push16_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, push32_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, push64_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, pop8_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, pop16_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, pop32_reg
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, pop64_reg
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
lea rcx, call_ui64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, call_native_ui64
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, ret_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, salloc_reg_ui8
sub rcx, rbx
mov [rax + rdx*2], cx

inc rdx
lea rcx, sdealloc_ui8 
sub rcx, rbx
mov [rax + rdx*2], cx

ret
vm_init ENDP

END
