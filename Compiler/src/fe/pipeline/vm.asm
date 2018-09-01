PUBLIC vm_interpret, vm_init
.data
registers QWORD 64 dup (0) ; 64 registers of 64 bits each, initialized to 0
handlers  QWORD 2 dup (?)  ; dispatch target locations, initialized in vm_init
.code



vm_interpret PROC

nop_ LABEL NEAR PTR WORD
	
mov rbx, [rax]
ret
vm_interpret ENDP



vm_init PROC
lea rax, handlers
lea rbx, nop_
mov [rax], rbx

ret
vm_init ENDP

END
