
.data

.extern Cacc
.extern Dacc
.extern Dlength

.globl Dacc_modulate_argb_MMX

.text

.Lget_pic:
    	movl	(%esp), %ebx
	ret
 
.align 8
Dacc_modulate_argb_MMX:
        pushl   %esi
    	pushl   %edi
        pushl   %ebx

  	call	.Lget_pic
1:	addl    $_GLOBAL_OFFSET_TABLE_, %ebx

        movl	Dacc@GOT(%ebx), %eax
        movl    (%eax), %edi
        movl	Dlength@GOT(%ebx), %eax
        movl    (%eax), %ecx

	movl	Cacc@GOT(%ebx), %eax
        movq    (%eax), %mm0

.align 8
.MODULATE_ARGB:
        testw   $0xF000, (%edi)
        jnz     .SKIP

        movq    (%edi), %mm1

        pmullw  %mm0, %mm1
        psrlw   $8, %mm1

        movq    %mm1, (%edi)

.SKIP:
        addl    $8, %edi

        dec     %ecx
        jnz     .MODULATE_ARGB

        emms

        popl	%ebx
        popl    %edi
        popl    %esi
        ret

