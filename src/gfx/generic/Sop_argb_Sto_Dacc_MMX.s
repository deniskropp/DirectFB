
.data

.extern Sop
.extern Dacc
.extern Dlength
.extern SperD

zeros:  .long   0,              0

.globl Sop_argb_Sto_Dacc_MMX

.text

.Lget_pic:
	movl	(%esp), %ebx
	ret

.align 8
Sop_argb_Sto_Dacc_MMX:
        pushl	%esi
	pushl	%edi
	pushl	%ebx

	call	.Lget_pic
1:	addl	$_GLOBAL_OFFSET_TABLE_, %ebx

        movl    Sop@GOT(%ebx), %eax
	movl	(%eax), %esi
        movl    Dacc@GOT(%ebx), %eax
	movl	(%eax), %edi
        movl    Dlength@GOT(%ebx), %eax
	movl	(%eax), %ecx
        movl    SperD@GOT(%ebx), %eax
	movl	(%eax), %eax

        xor     %edx, %edx

	movq	zeros@GOTOFF(%ebx), %mm0

.align 8
.LOAD:
        movd    (%esi), %mm1
        punpcklbw %mm0, %mm1

.align 8
.WRITE:
        movq    %mm1, (%edi)

        dec     %ecx
        jz      .FINISH

        addl    $8, %edi
        addl    %eax, %edx

        testl   $0xFFFF0000, %edx
        jz      .WRITE

        movl    %edx, %ebx
        and     $0xFFFF0000, %ebx
        shrl    $14, %ebx
        addl    %ebx, %esi

        and     $0xFFFF, %edx

        jmp     .LOAD

.FINISH:
        emms

	popl	%ebx
	popl	%edi
	popl	%esi
        ret

