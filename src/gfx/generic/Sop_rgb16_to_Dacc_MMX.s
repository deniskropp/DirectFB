
.data

.extern Sop
.extern Dacc
.extern Dlength

mask:	.long	0x07E0001f,	0x0000F800
smul:	.long	0x00200800,	0x00000001
alpha:	.long	0,      	0x00FF0000

.globl Sop_rgb16_to_Dacc_MMX

.text

.Lget_pic:
	movl	(%esp), %ebx
	ret
 
.align 8
Sop_rgb16_to_Dacc_MMX: 
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	call	.Lget_pic
1:	addl	$_GLOBAL_OFFSET_TABLE_, %ebx

	movl	Sop@GOT(%ebx), %eax  
	movl	(%eax), %esi
	movl	Dacc@GOT(%ebx), %eax
	movl	(%eax), %edi
	movl	Dlength@GOT(%ebx), %eax
        movl    (%eax), %ecx
        
	movq	mask@GOTOFF(%ebx), %mm4
	movq	smul@GOTOFF(%ebx), %mm5
	movq	alpha@GOTOFF(%ebx), %mm7

.align 8
.CONVERT4:
        movq    (%esi), %mm0
      
# 1.
  # Konvertierung nach 24 bit interleaved
	movq	%mm0, %mm3		# pixel in mm3 kopieren
        
        punpcklwd %mm3, %mm3
        punpckldq %mm3, %mm3
        
        pand    %mm4, %mm3
        
        pmullw  %mm5, %mm3
        
        psrlw   $8, %mm3
  # mm3 enthaelt jetzt: 0000 00rr 00gg 00bb des alten pixels
        
        por     %mm7, %mm3
        movq    %mm3, (%edi)

        dec     %ecx
        jz      .FINISH
        
        psrlq   $16, %mm0
	addl	$8, %edi
	
# 2.        
  # Konvertierung nach 24 bit interleaved
	movq	%mm0, %mm3		# pixel in mm3 kopieren
        
        punpcklwd %mm3, %mm3
        punpckldq %mm3, %mm3
        
        pand    %mm4, %mm3
        
        pmullw  %mm5, %mm3
        
        psrlw   $8, %mm3
  # mm3 enthaelt jetzt: 0000 00rr 00gg 00bb des alten pixels
        
        por     %mm7, %mm3
        movq    %mm3, (%edi)
        
        dec     %ecx
        jz      .FINISH
        
        psrlq   $16, %mm0
	addl	$8, %edi
	
# 3.        
  # Konvertierung nach 24 bit interleaved
	movq	%mm0, %mm3		# pixel in mm3 kopieren
        
        punpcklwd %mm3, %mm3
        punpckldq %mm3, %mm3
        
        pand    %mm4, %mm3
        
        pmullw  %mm5, %mm3
        
        psrlw   $8, %mm3
  # mm3 enthaelt jetzt: 0000 00rr 00gg 00bb des alten pixels
        
        por     %mm7, %mm3
        movq    %mm3, (%edi)
	
        dec     %ecx
        jz      .FINISH
        
        psrlq   $16, %mm0
	addl	$8, %edi

# 4.        
  # Konvertierung nach 24 bit interleaved
	movq	%mm0, %mm3		# pixel in mm3 kopieren
        
        punpcklwd %mm3, %mm3
        punpckldq %mm3, %mm3
        
        pand    %mm4, %mm3
        
        pmullw  %mm5, %mm3
        
        psrlw   $8, %mm3
  # mm3 enthaelt jetzt: 0000 00rr 00gg 00bb des alten pixels
        
        por     %mm7, %mm3
        movq    %mm3, (%edi)
	
        dec     %ecx
        jz      .FINISH
        
	addl	$8, %edi
        
        addl    $8, %esi
        jmp     .CONVERT4

.FINISH:
	emms

	popl	%ebx
	popl	%edi
	popl	%esi
	ret

