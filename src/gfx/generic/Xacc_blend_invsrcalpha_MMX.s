
.data

.extern color
.extern Sacc
.extern Xacc
.extern Dlength

zeros:	.long	0,		0
einser:	.long	0x01000100,     0x01000100

.globl Xacc_blend_invsrcalpha_MMX

.text

.align 8
Xacc_blend_invsrcalpha_MMX: 
	pushal

	movl	Sacc, %esi
	movl	Xacc, %edi
        movl    Dlength, %ecx

        cmpl    $0, %esi
        jnz     .blend_from_Sacc
        
	movd	color, %mm0
        
	punpcklbw zeros, %mm0		# mm0 = 00aa 00rr 00gg 00bb
	punpcklwd %mm0, %mm0		# mm0 = 00aa 00aa xxxx xxxx
        movq    einser, %mm1
	punpckldq %mm0, %mm0		# mm0 = 00aa 00aa 00aa 00aa
        
        psubw   %mm0, %mm1
        
.align 8
.blend_from_color:
        testw   $0xF000, 6(%edi)
        jnz     .SKIP1
	
        movq	(%edi), %mm0
        
        pmullw  %mm1, %mm0
        psrlw   $8, %mm0
	
        movq    %mm0, (%edi)
	
.SKIP1:
	addl	$8, %edi
	
        dec	%ecx
        jnz     .blend_from_color

	emms

	popal
	ret



.align 8
.blend_from_Sacc:
        testw   $0xF000, 6(%edi)
        jnz     .SKIP2
	
        movq	(%esi), %mm2
        movq	(%edi), %mm0
        
	punpckhwd %mm2, %mm2		# mm2 = 00aa 00aa xxxx xxxx
        punpckhdq %mm2, %mm2		# mm2 = 00aa 00aa 00aa 00aa
        
        pmullw  %mm0, %mm2
        psrlw   $8, %mm2
        
        psubw   %mm2, %mm0
	
        movq    %mm0, (%edi)
	
.SKIP2:
	addl	$8, %esi
	addl	$8, %edi
	
        dec	%ecx
        jnz     .blend_from_Sacc

	emms

	popal
	ret

