
.data

.extern color
.extern Sacc
.extern Xacc
.extern Dlength

zeros: .long   0,              0
ones:   .long   0x00010001,     0x00010001

.globl Xacc_blend_srcalpha_MMX

.text

.Lget_pic:
        movl	(%esp), %ebx
        ret
 
.align 8
Xacc_blend_srcalpha_MMX:
	pushl   %esi
	pushl   %edi
	pushl   %ebx

	call	.Lget_pic
1:	addl	$_GLOBAL_OFFSET_TABLE_, %ebx

	movl	Sacc@GOT(%ebx), %eax
	movl	(%eax), %esi
	movl	Xacc@GOT(%ebx), %eax
	movl	(%eax), %edi
	movl	Dlength@GOT(%ebx), %eax
	movl	(%eax), %ecx

	movq	ones@GOTOFF(%ebx), %mm7

        cmpl    $0, %esi
        jnz     .blend_from_Sacc

	movq	zeros@GOTOFF(%ebx), %mm6

	movl	color@GOT(%ebx), %eax
	movd	(%eax), %mm0

        punpcklbw %mm6, %mm0           # mm0 = 00aa 00rr 00gg 00bb
        punpcklwd %mm0, %mm0            # mm0 = 00aa 00aa xxxx xxxx
        punpckldq %mm0, %mm0            # mm0 = 00aa 00aa 00aa 00aa

        paddw   %mm7, %mm0

.align 8
.blend_from_color:
        testw   $0xF000, 6(%edi)
        jnz     .SKIP1

        movq    (%edi), %mm1

        pmullw  %mm0, %mm1
        psrlw   $8, %mm1

        movq    %mm1, (%edi)

.SKIP1:
        addl    $8, %edi

        dec     %ecx
        jnz     .blend_from_color

        emms

	popl    %ebx
	popl	%edi
	popl	%esi
        ret



.align 8
.blend_from_Sacc:
        testw   $0xF000, 6(%edi)
        jnz     .SKIP2

        movq    (%esi), %mm0
        movq    (%edi), %mm1

        punpckhwd %mm0, %mm0            # mm0 = 00aa 00aa xxxx xxxx
        punpckhdq %mm0, %mm0            # mm0 = 00aa 00aa 00aa 00aa

        paddw   %mm7, %mm0

        pmullw  %mm0, %mm1
        psrlw   $8, %mm1

        movq    %mm1, (%edi)

.SKIP2:
        addl    $8, %esi
        addl    $8, %edi

        dec     %ecx
        jnz     .blend_from_Sacc

        emms

	popl    %ebx
	popl	%edi
	popl	%esi
        ret

