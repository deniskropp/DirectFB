
.data

.extern Sacc
.extern Aop
.extern Dlength

preload: .long  0xFF00FF00,     0x0000FF00
postload:.long  0x00FF00FF,     0x000000FF
pm:      .long  0x01000001,     0x00000001

.globl Sacc_to_Aop_rgb32_MMX

.text

.align 8
Sacc_to_Aop_rgb32_MMX: 
	pushal

	movl	Sacc, %esi
	movl	Aop, %edi
        movl    Dlength, %ecx
        
        movq    preload, %mm1
        movq    postload, %mm2
        movq    pm, %mm3

.align 8
.CONVERT:
        testw   $0xF000, 6(%esi)
        jnz     .SKIP

	movq	(%esi), %mm0
        
        paddusw %mm1, %mm0
        pand    %mm2, %mm0
        
        pmaddwd %mm3, %mm0
        
        movq    %mm0, %mm4
        
        psrlq   $16, %mm0
        por     %mm0, %mm4
        
        movd    %mm4, (%edi)

.align 8
.SKIP:
        addl	$8, %esi
	addl	$4, %edi
	
        dec	%ecx
        jnz     .CONVERT

	emms

	popal
	ret

