
.data

.extern Sop
.extern Dacc
.extern Dlength

alpha:	.long	0,      	0x00FF0000
zeros:	.long	0,		0

.globl Sop_rgb32_to_Dacc_MMX

.text

.Lget_pic:
        movl	(%esp), %ebx
        ret
 
.align 8
Sop_rgb32_to_Dacc_MMX: 
    	pushl   %esi
        pushl   %edi
        pushl   %ebx
	
  	call    .Lget_pic   
1:	addl    $_GLOBAL_OFFSET_TABLE_, %ebx

        movl    Sop@GOT(%ebx), %eax
        movl	(%eax), %esi
        movl    Dacc@GOT(%ebx), %eax
        movl	(%eax), %edi
        movl    Dlength@GOT(%ebx), %eax
        movl    (%eax), %ecx
        
	movq	alpha@GOTOFF(%ebx), %mm7
	movq	zeros@GOTOFF(%ebx), %mm6

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

        popl	%ebx
        popl    %edi
        popl    %esi
	ret

