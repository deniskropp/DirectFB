
.data

.extern Sacc
.extern Dacc
.extern Dlength

.globl Sacc_add_to_Dacc_MMX

.text

.align 8
Sacc_add_to_Dacc_MMX: 
	pushal

	movl	Sacc, %esi
	movl	Dacc, %edi
        movl    Dlength, %ecx

.align 8
.CONVERT: 
        movq	(%esi), %mm0
	movq	(%edi), %mm1
        
        paddw   %mm1, %mm0
	
        movq    %mm0, (%edi)
	
	addl	$8, %esi
	addl	$8, %edi
	
        dec	%ecx
        jnz     .CONVERT

	emms

	popal
	ret

