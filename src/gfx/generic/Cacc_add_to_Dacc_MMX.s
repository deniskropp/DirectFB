
.data

.extern Cacc
.extern Dacc
.extern Dlength

.globl Cacc_add_to_Dacc_MMX

.text

.Lget_pic:
    	movl	(%esp), %ebx
	ret
 
.align 8
Cacc_add_to_Dacc_MMX: 
    	pushl   %edi
        pushl   %ebx

  	call	.Lget_pic
1:	addl    $_GLOBAL_OFFSET_TABLE_, %ebx

        movl	Dacc@GOT(%ebx),%eax
        movl    (%eax), %edi
        movl	Dlength@GOT(%ebx),%eax
        movl    (%eax), %ecx
        
	movl	Cacc@GOT(%ebx),%eax
	movq	(%eax), %mm0

.align 8
.CONVERT: 
	movq	(%edi), %mm1
        
        paddw   %mm0, %mm1
	
        movq    %mm1, (%edi)
	
	addl	$8, %edi
	
        dec	%ecx
        jnz     .CONVERT

	emms

	popl	%ebx
	popl	%edi
	ret

