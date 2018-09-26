PUBLIC vm_interpret, vm_init
.data
registers QWORD 64 dup (0) ; 64 registers of 64 bits each, initialized to 0
handlers  QWORD 57 dup (?) ; dispatch target locations, initialized in vm_init
stack_    BYTE 16384 dup (0)
.code


; interprets the given bytecode
; rcx : pointer to first bytecode instruction

vm_interpret PROC

; save nonvolatile registers rbx, r12, r13
push rbx
push r12
push r13
push r14
push r15

; move register pointer into r13, used for register operations
mov r13, OFFSET registers

; move stack pointer into r14, used for memory loads and stores
mov r14, OFFSET stack_
; move stack offset into r15
mov r15, 0

; r8 contains the pointer to data
; r9 contains the operands
; r13 contains register base
; r14 contains stack base
; r15 contains stack pointer

; mov argument into r8
mov r8, rcx

; initial dispatch
jmp dispatch

; BEGIN nop
lbl_ui32 LABEL NEAR PTR WORD
nop_ LABEL NEAR PTR WORD
	add r8, 1
	jmp dispatch
; END nop

; BEGIN add reg reg reg
add_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 24
	shr rax, 16
	shr rbx, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh

	mov rax, [r13 + rax*8]
	add rax, [r13 + r9*8]
	mov [r13 + rbx*8], rax

	add r8, 4
	jmp dispatch
; END add reg reg reg


; BEGIN add reg reg ui8
add_reg_reg_ui8 LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; r9b gets literal
	; al gets second source
	; bl gets target
	shr r9, 24
	shr rax, 16
	shr rbx, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh

	add r9, [r13 + rax*8]
	mov [r13 + rbx*8], r9

	add r8, 4
	jmp dispatch
; END add reg reg ui8

; BEGIN sub reg reg reg
sub_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 24
	shr rax, 16
	shr rbx, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh

	mov rax, [r13 + rax*8]
	sub rax, [r13 + r9*8]
	mov [r13 + rbx*8], rax

	add r8, 4
	jmp dispatch
; END sub reg reg reg

; BEGIN sub reg reg ui8
sub_reg_reg_ui8 LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; r9b gets literal
	; al gets source
	; bl gets target
	shr r9, 24
	shr rax, 16
	shr rbx, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh

	sub r9, [r13 + rax*8]
	mov [r13 + rbx*8], r9

	add r8, 4
	jmp dispatch
; END sub reg reg ui8

; BEGIN mul reg reg reg
mul_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr rbx, 8
	shr rax, 16
	shr r9, 24
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh

	mov rax, [r13 + rax*8]
	imul QWORD PTR [r13 + r9*8]
	mov [r13 + rbx*8], rax

	add r8, 4
	jmp dispatch
; END mul reg reg reg

; BEGIN div reg reg reg
div_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; this is slightly different from regular since we want the numerator in rax

	; al gets first source
	; r9b gets second source
	; bl gets target
	shr rbx, 8
	shr rax, 16
	shr r9, 24
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh

	mov rax, [r13 + rax*8]
	idiv QWORD PTR [r13 + r9*8]
	mov [r13 + rbx*8], rax

	add r8, 4
	jmp dispatch
; END div reg reg reg

; BEGIN mod reg reg reg
mod_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; this is slightly different from regular (but the same as div) since we want the numerator in rax

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr rbx, 8
	shr rax, 16
	shr r9, 24
	and rbx, 0ffh
	and rax, 0ffh
	and r9, 0ffh

	mov rax, [r13 + rax*8]
	idiv QWORD PTR [r13 + r9*8]
	mov [r13 + rbx*8], rdx

	add r8, 4
	jmp dispatch
; END mod reg reg reg

; BEGIN gt reg reg reg
gt_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; bl gets target
	; al gets first source
	; r9b gets second source
	shr rbx, 8
	shr rax, 16
	shr r9, 24
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

	add r8, 4
	jmp dispatch
; END gt reg reg reg

; BEGIN gte reg reg reg
gte_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; bl gets target
	; al gets first source
	; r9b gets second source
	shr rbx, 8
	shr rax, 16
	shr r9, 24
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

	add r8, 4
	jmp dispatch
; END gte reg reg reg

; BEGIN lt reg reg reg
lt_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; bl gets target
	; al gets first source
	; r9b gets second source
	shr rbx, 8
	shr rax, 16
	shr r9, 24
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

	add r8, 4
	jmp dispatch
; END lt reg reg reg

; BEGIN lte reg reg reg
lte_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; bl gets target
	; al gets first source
	; r9b gets second source
	shr rbx, 8
	shr rax, 16
	shr r9, 24
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

	add r8, 4
	jmp dispatch
; END lte reg reg reg


; BEGIN eq reg reg reg
eq_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 24
	shr rax, 16
	shr rbx, 8
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

	add r8, 4
	jmp dispatch
; END eq reg reg reg

; BEGIN neq reg reg reg
neq_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 24
	shr rax, 16
	shr rbx, 8
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

	add r8, 4
	jmp dispatch
; END neq reg reg reg 

; BEGIN and reg reg reg
and_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 24
	shr rax, 16
	shr rbx, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh

	mov r9, [r13 + r9*8]
	and r9, [r13 + rax*8]
	mov [r13 + rbx*8], r9

	add r8, 4
	jmp dispatch
; END and reg reg reg 

; BEGIN and reg reg ui8
and_reg_reg_ui8 LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; r9b gets literal
	; al gets source
	; bl gets target
	shr r9, 24
	shr rax, 16
	shr rbx, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh

	mov rax, [r13 + rax*8]
	and r9, rax
	mov [r13 + rbx*8], r9

	add r8, 4
	jmp dispatch
; END and reg reg ui8 

; BEGIN or reg reg reg
or_reg_reg_reg LABEL NEAR PTR WORD
	mov rax, r9
	mov rbx, r9

	; r9b gets second source
	; al gets first source
	; bl gets target
	shr r9, 24
	shr rax, 16
	shr rbx, 8
	and r9, 0ffh
	and rax, 0ffh
	and rbx, 0ffh

	mov r9, [r13 + r9*8]
	or r9, [r13 + rax*8]
	mov [r13 + rbx*8], r9

	add r8, 4
	jmp dispatch
; END or reg reg reg 

; BEGIN mv reg sp
mv_reg_sp LABEL NEAR PTR WORD
	shr r9, 8
	and r9, 0ffh
	mov rax, [r13 + 62*8]
	mov [r13 + r9*8], rax

	add r8, 2
	jmp dispatch
; END mv reg sp

; BEGIN mv reg ip
mv_reg_ip LABEL NEAR PTR WORD
	shr r9, 8
	and r9, 0ffh
	mov rax, [r13 + 63*8]
	mov [r13 + r9*8], rax

	add r8, 2
	jmp dispatch
; END mv reg ip

; BEGIN mv reg ui8 | mv reg i8
mv_reg_i8 LABEL NEAR PTR WORD
mv_reg_ui8 LABEL NEAR PTR WORD
	shr r9, 8
	xor rax, rax
	; al contains reg
	mov al, r9b 
	shr r9, 8
	and r9, 0ffh
	; r9 contains literal
	mov [r13 + rax*8], r9

	add r8, 3
	jmp dispatch
; END mv reg ui8 | mv reg i8

; BEGIN mv reg ui16 | mv reg i16
mv_reg_i16 LABEL NEAR PTR WORD
mv_reg_ui16 LABEL NEAR PTR WORD
	shr r9, 8
	xor rax, rax
	; al contains reg
	mov al, r9b 
	shr r9, 8
	and r9, 0ffffh
	; r9 contains literal
	mov [r13 + rax*8], r9

	add r8, 4
	jmp dispatch
; END mv reg ui16 | mv reg i16

; BEGIN mv reg ui32 | mv reg ui32
mv_reg_i32 LABEL NEAR PTR WORD
mv_reg_ui32 LABEL NEAR PTR WORD
	shr r9, 8
	xor rax, rax
	; al contains reg
	mov al, r9b 
	shr r9, 8
	and r9, 0ffffffffh
	; r9 contains literal
	mov [r13 + rax*8], r9

	add r8, 6
	jmp dispatch
; END mv reg ui32 | mv reg i32

; BEGIN mv reg ui64 | mv reg i64
mv_reg_i64 LABEL NEAR PTR WORD
mv_reg_ui64 LABEL NEAR PTR WORD
	mov rax, QWORD PTR [r8 + 2]
	shr r9, 8
	and r9, 0ffh
	mov [r13 + r9*8], rax

	add r8, 10
	jmp dispatch
; END mv reg ui64 | mv reg i64

; BEGIN mv8 reg reg
mv8_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 8
	xor rax, rax
	mov al, r9b
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	mov rcx, QWORD PTR [r13 + rbx*8]
	and rcx, 0ffh
	mov QWORD PTR [r13 + rax*8], rcx

	add r8, 3
	jmp dispatch
; END mv8 reg reg

; BEGIN mv16 reg reg
mv16_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 8
	xor rax, rax
	mov al, r9b
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	mov rcx, QWORD PTR [r13 + rbx*8]
	and rcx, 0ffffh
	mov QWORD PTR [r13 + rax*8], rcx

	add r8, 3
	jmp dispatch
; END mv16 reg reg

; BEGIN mv32 reg reg
mv32_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 8
	xor rax, rax
	mov al, r9b
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	mov rcx, QWORD PTR [r13 + rbx*8]
	and rcx, 0ffffffffh
	mov QWORD PTR [r13 + rax*8], rcx

	add r8, 3
	jmp dispatch
; END mv32 reg reg

; BEGIN mv64 reg reg
mv64_reg_reg LABEL NEAR PTR WORD
	; al contains target
	shr r9, 8
	xor rax, rax
	mov al, r9b
	; bl contains source
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	mov rcx, QWORD PTR [r13 + rbx*8]
	mov QWORD PTR [r13 + rax*8], rcx

	add r8, 3
	jmp dispatch
; END mv64 reg reg

; BEGIN mv8 loc reg
mv8_loc_reg LABEL NEAR PTR WORD
	; al contains loc
	shr r9, 8
	xor rax, rax
	mov al, r9b
	; bl contains reg
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	mov rax, QWORD PTR [r13 + rax*8]
	mov bl, BYTE PTR [r13 + rbx*8]
	mov BYTE PTR [r14 + rax], bl

	add r8, 3
	jmp dispatch
; END mv8 loc reg

; BEGIN mv16 loc reg
mv16_loc_reg LABEL NEAR PTR WORD
	; al contains loc
	shr r9, 8
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
	mov WORD PTR [r14 + rax], bx

	add r8, 3
	jmp dispatch
; END mv16 loc reg

; BEGIN mv32 loc reg
mv32_loc_reg LABEL NEAR PTR WORD
	; al contains loc
	shr r9, 8
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
	mov DWORD PTR [r14 + rax], ebx

	add r8, 3
	jmp dispatch
; END mv32 loc reg

; BEGIN mv64 loc reg
mv64_loc_reg LABEL NEAR PTR WORD
	; al contains loc
	shr r9, 8
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
	mov QWORD PTR [r14 + rax], rbx

	add r8, 3
	jmp dispatch
; END mv64 loc reg

; BEGIN mv8 reg loc
mv8_reg_loc LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	; bl contains loc
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rbx, QWORD PTR [r13 + rbx*8]
	; get value at location
	mov bl, BYTE PTR [r14 + rbx]
	; put value in register
	mov BYTE PTR [r13 + rax*8], bl

	add r8, 3
	jmp dispatch
; END mv8 reg loc

; BEGIN mv16 reg loc
mv16_reg_loc LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	; bl contains loc
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rbx, QWORD PTR [r13 + rbx*8]
	; get value at location
	mov bx, WORD PTR [r14 + rbx]
	; put value in register
	mov WORD PTR [r13 + rax*8], bx

	add r8, 3
	jmp dispatch
; END mv16 reg loc

; BEGIN mv32 reg loc
mv32_reg_loc LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	; bl contains loc
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rbx, QWORD PTR [r13 + rbx*8]
	; get value at location
	mov ebx, DWORD PTR [r14 + rbx]
	; put value in register
	mov DWORD PTR [r13 + rax*8], ebx

	add r8, 3
	jmp dispatch
; END mv32 reg loc

; BEGIN mv64 reg loc
mv64_reg_loc LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	; bl contains loc
	shr r9, 8
	xor rbx, rbx
	mov bl, r9b

	; get location from register
	mov rbx, QWORD PTR [r13 + rbx*8]
	; get value at location
	mov rbx, QWORD PTR [r14 + rbx]
	; put value in register
	mov QWORD PTR [r13 + rax*8], rbx

	add r8, 3
	jmp dispatch
; END mv64 reg loc

; BEGIN push8 reg
push8_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	
	mov al, BYTE PTR [r13 + rax*8]
	mov BYTE PTR [r14 + r15], al
	inc r15

	add r8, 2
	jmp dispatch
; END push8 reg

; BEGIN push16 reg
push16_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	
	mov ax, WORD PTR [r13 + rax*8]
	mov WORD PTR [r14 + r15], ax
	add r15, 2

	add r8, 2
	jmp dispatch
; END push16 reg

; BEGIN push32 reg
push32_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	
	mov eax, DWORD PTR [r13 + rax*8]
	mov DWORD PTR [r14 + r15], eax
	add r15, 4

	add r8, 2
	jmp dispatch
; END push32 reg

; BEGIN push64 reg
push64_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	
	mov rax, QWORD PTR [r13 + rax*8]
	mov QWORD PTR [r14 + r15], rax
	add r15, 8

	add r8, 2
	jmp dispatch
; END push64 reg

; BEGIN pop8 reg
pop8_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	
	dec r15
	mov bl, BYTE PTR [r14 + r15]
	mov BYTE PTR [r13 + rax*8], bl

	add r8, 2
	jmp dispatch
; END pop8 reg

; BEGIN pop16 reg
pop16_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	
	sub r15, 2
	mov bx, WORD PTR [r14 + r15]
	mov WORD PTR [r13 + rax*8], bx

	add r8, 2
	jmp dispatch
; END pop16 reg

; BEGIN pop32 reg
pop32_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	
	sub r15, 4
	mov ebx, DWORD PTR [r14 + r15]
	mov DWORD PTR [r13 + rax*8], ebx

	add r8, 2
	jmp dispatch
; END pop32 reg

; BEGIN pop64 reg
pop64_reg LABEL NEAR PTR WORD
	; al contains reg
	shr r9, 8
	xor rax, rax
	mov al, r9b
	
	sub r15, 8
	mov rbx, QWORD PTR [r14 + r15]
	mov QWORD PTR [r13 + rax*8], rbx

	add r8, 2
	jmp dispatch
; END pop64 reg

; BEGIN jmpr i32
jmpr_i32 LABEL NEAR PTR WORD
	; r9d contains offset
	shr r9, 8
	movsxd r9, r9d
	add r8, r9
	jmp dispatch
; END jmpr i32

; BEGIN jrnz reg i32
jrnz_reg_i32 LABEL NEAR PTR WORD
	shr r9, 8
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
	jmp dispatch
_skip_jrnz:
	add r8, 6
	jmp dispatch
; END jrnz reg i32

; BEGIN jrz reg i32
jrz_reg_i32 LABEL NEAR PTR WORD
	shr r9, 8
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
	jmp dispatch
_skip_jrz:
	add r8, 6
	jmp dispatch
; END jrz reg i32

; BEGIN call ui64
call_ui64 LABEL NEAR PTR WORD
	; rax contains offset
	mov rax, QWORD PTR [r8 + 1]

	; simulate call

	; save frame pointer
	mov rbx, QWORD PTR [r13 + 61*8]
	mov QWORD PTR [r14 + r15], rbx
	add r15, 8
	; push the next op so we return and continue execution after this op
	mov rbx, r8
	add rbx, 9
	mov QWORD PTR [r14 + r15], rbx
	add r15, 8

	; set new frame pointer
	; load sp
	mov rbx, QWORD PTR [r13 + 62*8]
	mov QWORD PTR [r13 + 61*8], rbx
	
	; update ip to function entry location
	add r8, rax

	jmp dispatch
; END call ui64

; BEGIN ret ui8
ret_ui8 LABEL NEAR PTR WORD
	; r9b contains param size
	shr r9, 8
	and r9, 0ffh

	sub r15, 8
	mov r8, QWORD PTR [r14 + r15]
	sub r15, 8
	mov rbx, QWORD PTR [r14 + r15]
	mov QWORD PTR [r13 + 62*8], r15
	mov QWORD PTR [r13 + 61*8], rbx
	sub r15, r9

	jmp dispatch
; END ret ui8

; BEGIN call native ui64
call_native_ui64 LABEL NEAR PTR WORD
	; TODO skip for now
	add r8, 9
	jmp dispatch
; END call native ui64


; BEGIN salloc reg ui8
salloc_reg_ui8 LABEL NEAR PTR WORD
	shr r9, 8
	xor rax, rax
	mov r9b, al
	shr r9, 8
	and r9, 0ffh

	mov [r13 + rax], r15

	add r15, r9

	add r8, 3
	jmp dispatch
; END salloc reg ui8

; BEGIN sdealloc ui8
sdealloc_ui8 LABEL NEAR PTR WORD
	shr r9, 8
	sub r15, r9

	add r8, 2
	jmp dispatch
; END sdealloc ui8

; BEGIN exit
exit LABEL NEAR PTR WORD
	mov rax, QWORD PTR [r13 + 60*8]

	; restore nonvolatile registers rbx, r12, r13, r14, r15
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	ret
; END exit

dispatch:
	; put operands in r9
	mov r9, QWORD PTR [r8] 
	lea rdx, OFFSET handlers
	xor ecx, ecx
	mov cl, r9b
	lea rcx, [rcx*8 + rdx]
	mov rcx, [rcx]
	jmp rcx

vm_interpret ENDP

; initializes each handler pointer

vm_init PROC
lea rax, handlers

lea rbx, nop_
mov [rax + 0*8], rbx

lea rbx, add_reg_reg_reg
mov [rax + 1*8], rbx

lea rbx, add_reg_reg_ui8
mov [rax + 2*8], rbx

lea rbx, sub_reg_reg_reg
mov [rax + 3*8], rbx

lea rbx, sub_reg_reg_ui8
mov [rax + 4*8], rbx

lea rbx, mul_reg_reg_reg
mov [rax + 5*8], rbx

lea rbx, div_reg_reg_reg
mov [rax + 6*8], rbx

lea rbx, mod_reg_reg_reg
mov [rax + 7*8], rbx

lea rbx, gt_reg_reg_reg
mov [rax + 8*8], rbx

lea rbx, gte_reg_reg_reg
mov [rax + 9*8], rbx

lea rbx, lt_reg_reg_reg
mov [rax + 10*8], rbx

lea rbx, lte_reg_reg_reg
mov [rax + 11*8], rbx

lea rbx, eq_reg_reg_reg
mov [rax + 12*8], rbx

lea rbx, neq_reg_reg_reg
mov [rax + 13*8], rbx

lea rbx, and_reg_reg_reg
mov [rax + 14*8], rbx

lea rbx, and_reg_reg_ui8
mov [rax + 15*8], rbx

lea rbx, or_reg_reg_reg
mov [rax + 16*8], rbx

lea rbx, mv_reg_sp
mov [rax + 17*8], rbx

lea rbx, mv_reg_ip
mov [rax + 18*8], rbx

lea rbx, mv_reg_ui8
mov [rax + 19*8], rbx

lea rbx, mv_reg_ui16
mov [rax + 20*8], rbx

lea rbx, mv_reg_ui32
mov [rax + 21*8], rbx

lea rbx, mv_reg_ui64
mov [rax + 22*8], rbx

lea rbx, mv_reg_i8
mov [rax + 23*8], rbx

lea rbx, mv_reg_i16
mov [rax + 24*8], rbx

lea rbx, mv_reg_i32
mov [rax + 25*8], rbx

lea rbx, mv_reg_i64
mov [rax + 26*8], rbx

lea rbx, mv8_reg_reg
mov [rax + 27*8], rbx

lea rbx, mv16_reg_reg
mov [rax + 28*8], rbx

lea rbx, mv32_reg_reg
mov [rax + 29*8], rbx

lea rbx, mv64_reg_reg
mov [rax + 30*8], rbx

lea rbx, mv8_loc_reg
mov [rax + 31*8], rbx

lea rbx, mv16_loc_reg
mov [rax + 32*8], rbx

lea rbx, mv32_loc_reg
mov [rax + 33*8], rbx

lea rbx, mv64_loc_reg
mov [rax + 34*8], rbx

lea rbx, mv8_reg_loc
mov [rax + 35*8], rbx

lea rbx, mv16_reg_loc
mov [rax + 36*8], rbx

lea rbx, mv32_reg_loc
mov [rax + 37*8], rbx

lea rbx, mv64_reg_loc
mov [rax + 38*8], rbx

lea rbx, push8_reg
mov [rax + 39*8], rbx

lea rbx, push16_reg
mov [rax + 40*8], rbx

lea rbx, push32_reg
mov [rax + 41*8], rbx

lea rbx, push64_reg
mov [rax + 42*8], rbx

lea rbx, pop8_reg
mov [rax + 43*8], rbx

lea rbx, pop16_reg
mov [rax + 44*8], rbx

lea rbx, pop32_reg
mov [rax + 45*8], rbx

lea rbx, pop64_reg
mov [rax + 46*8], rbx

lea rbx, jmpr_i32
mov [rax + 47*8], rbx

lea rbx, jrnz_reg_i32
mov [rax + 48*8], rbx

lea rbx, jrz_reg_i32
mov [rax + 49*8], rbx

lea rbx, call_ui64
mov [rax + 50*8], rbx

lea rbx, call_native_ui64
mov [rax + 51*8], rbx

lea rbx, ret_ui8
mov [rax + 52*8], rbx

lea rbx, lbl_ui32
mov [rax + 53*8], rbx

lea rbx, salloc_reg_ui8
mov [rax + 54*8], rbx

lea rbx, sdealloc_ui8
mov [rax + 55*8], rbx

lea rbx, exit
mov [rax + 56*8], rbx

ret
vm_init ENDP

END
