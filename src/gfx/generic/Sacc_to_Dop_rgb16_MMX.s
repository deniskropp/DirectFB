
.data

.extern Sacc
.extern Aop
.extern Dlength

preload: .long  0xFF00FF00,     0x0000FF00

mask:    .long  0x00FC00F8,     0x000000F8
pm:      .long  0x01000004,     0x00000004

.globl Sacc_to_Aop_rgb16_MMX

.text

.Lget_pic:
        movl	(%esp), %ebx
        ret
 
.align 8
Sacc_to_Aop_rgb16_MMX: 
    	pushl   %esi
        pushl   %edi
        pushl   %ebx

  	call    .Lget_pic   
1:	addl    $_GLOBAL_OFFSET_TABLE_, %ebx
        
        movl    Sacc@GOT(%ebx), %eax
        movl	(%eax), %esi
        movl    Aop@GOT(%ebx), %eax
        movl	(%eax), %edi
        movl    Dlength@GOT(%ebx), %eax
        movl    (%eax), %ecx

	movq	preload@GOTOFF(%ebx), %mm7
	movq	mask@GOTOFF(%ebx), %mm5
        movq	pm@GOTOFF(%ebx), %mm4
        
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

        popl	%ebx
        popl    %edi
        popl    %esi
	ret

