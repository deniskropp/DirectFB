
.data

.extern Sop
.extern Dacc
.extern Dlength

zeros:	.long	0,		0

.globl Sop_argb_to_Dacc_MMX

.text

.Lget_pic:
        movl	(%esp), %ebx
        ret
 
.align 8
Sop_argb_to_Dacc_MMX: 
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
        
	movq	zeros@GOTOFF(%ebx), %mm0

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

        popl	%ebx
        popl    %edi
        popl    %esi
	ret

