
.data

.extern color
.extern Sacc
.extern Xacc
.extern Dlength

zeros:  .long   0,              0
ones:   .long   0x00010001,     0x00010001

.globl Xacc_blend_srcalpha_MMX

.text

.align 8
Xacc_blend_srcalpha_MMX:
        pushal

        movl    Sacc, %esi
        movl    Xacc, %edi
        movl    Dlength, %ecx

        movq    ones, %mm7

        cmpl    $0, %esi
        jnz     .blend_from_Sacc


        movd    color, %mm0

        punpcklbw zeros, %mm0           # mm0 = 00aa 00rr 00gg 00bb
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

        popal
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

        popal
        ret

