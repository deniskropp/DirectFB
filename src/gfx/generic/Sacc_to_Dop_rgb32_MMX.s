
.data

.extern Sacc
.extern Aop
.extern Dlength

preload: .long  0xFF00FF00,     0x0000FF00
postload:.long  0x00FF00FF,     0x000000FF
pm:      .long  0x01000001,     0x00000001

.globl Sacc_to_Aop_rgb32_MMX

.text

.Lget_pic:
        movl	(%esp), %ebx
        ret
 
.align 8
Sacc_to_Aop_rgb32_MMX: 
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

	movq	preload@GOTOFF(%ebx), %mm1
	movq	postload@GOTOFF(%ebx), %mm2
	movq    pm@GOTOFF(%ebx), %mm3

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

        popl	%ebx
        popl    %edi
        popl    %esi
	ret

