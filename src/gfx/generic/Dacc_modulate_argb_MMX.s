
.data

.extern Cacc
.extern Dacc
.extern Dlength

.globl Dacc_modulate_argb_MMX

.text

.align 8
Dacc_modulate_argb_MMX:
        pushal

        movl    Dacc, %edi
        movl    Dlength, %ecx

        movq    Cacc, %mm0

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

        popal
        ret

