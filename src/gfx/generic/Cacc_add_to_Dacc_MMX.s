
.data

.extern Cacc
.extern Dacc
.extern Dlength

.globl Cacc_add_to_Dacc_MMX

.text

.align 8
Cacc_add_to_Dacc_MMX: 
	pushal

	movl	Dacc, %edi
        movl    Dlength, %ecx
        
        movq    Cacc, %mm0

.align 8
.CONVERT: 
	movq	(%edi), %mm1
        
        paddw   %mm0, %mm1
	
        movq    %mm1, (%edi)
	
	addl	$8, %edi
	
        dec	%ecx
        jnz     .CONVERT

	emms

	popal
	ret

