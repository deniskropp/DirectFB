
.data

.extern Sacc
.extern Dacc
.extern Dlength

.globl Sacc_add_to_Dacc_MMX

.text

.Lget_pic:
        movl	(%esp), %ebx
        ret
 
.align 8
Sacc_add_to_Dacc_MMX: 
    	pushl   %esi
        pushl   %edi
        pushl   %ebx

  	call    .Lget_pic   
1:	addl    $_GLOBAL_OFFSET_TABLE_, %ebx
        
        movl    Sacc@GOT(%ebx), %eax
        movl	(%eax), %esi
        movl    Dacc@GOT(%ebx), %eax
        movl	(%eax), %edi
        movl    Dlength@GOT(%ebx), %eax
        movl    (%eax), %ecx

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

        popl	%ebx
        popl    %edi
        popl    %esi
	ret

