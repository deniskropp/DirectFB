
.data

.extern Sop
.extern Dacc
.extern Dlength

zeros:	.long	0,		0

.globl Sop_argb_to_Dacc_MMX

.text

.align 8
Sop_argb_to_Dacc_MMX: 
	pushal

	movl	Sop, %esi
	movl	Dacc, %edi
        movl    Dlength, %ecx
        
        movq    zeros, %mm0

.align 8
.CONVERT:
        movd	(%esi), %mm1
	punpcklbw %mm0, %mm1
	
        movq    %mm1, (%edi)
	
	addl	$4, %esi
	addl	$8, %edi
	
        dec	%ecx
        jnz     .CONVERT

	emms

	popal
	ret

