
.data

.extern Sop
.extern Dacc
.extern Dlength

alpha:	.long	0,      	0x00FF0000
zeros:	.long	0,		0

.globl Sop_rgb32_to_Dacc_MMX

.text

.align 8
Sop_rgb32_to_Dacc_MMX: 
	pushal

	movl	Sop, %esi
	movl	Dacc, %edi
        movl    Dlength, %ecx
        
        movq    alpha, %mm7
        movq    zeros, %mm6

.align 8
.CONVERT: 
	movd	(%esi), %mm0
	punpcklbw %mm6, %mm0
        por     %mm7, %mm0
	
        movq    %mm0, (%edi)
	
	addl	$4, %esi
	addl	$8, %edi
	
        dec	%ecx
        jnz     .CONVERT

	emms

	popal
	ret

