
.data

.extern Sacc
.extern Aop
.extern Dlength

preload: .long  0xFF00FF00,     0x0000FF00

mask:    .long  0x00FC00F8,     0x000000F8
pm:      .long  0x01000004,     0x00000004

.globl Sacc_to_Aop_rgb16_MMX

.text

.align 8
Sacc_to_Aop_rgb16_MMX: 
	pushal

	movl	Sacc, %esi
	movl	Aop, %edi
        movl    Dlength, %ecx
        
        movq    preload, %mm7
        movq    mask, %mm5
        movq    pm, %mm4
        
.align 8
.CONVERT:
        testw   $0xF000, 6(%esi)
        jnz     .SKIP

	movq	(%esi), %mm0
        
        paddusw %mm7, %mm0
        
        pand    %mm5, %mm0
        pmaddwd %mm4, %mm0
        
        psrlq   $5, %mm0
        movq    %mm0, %mm1
        
        psrlq   $21, %mm0
        por     %mm1, %mm0
        
        movd    %mm0, %eax
        movw    %ax, (%edi)

.align 8
.SKIP:
        addl	$8, %esi
	addl	$2, %edi
	
        dec	%ecx
        jnz     .CONVERT

	emms

	popal
	ret

