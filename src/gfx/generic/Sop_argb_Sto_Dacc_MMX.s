
.data

.extern Sop
.extern Dacc
.extern Dlength
.extern SperD

zeros:  .long   0,              0

.globl Sop_argb_Sto_Dacc_MMX

.text

.align 8
Sop_argb_Sto_Dacc_MMX:
        pushal

        movl    Sop, %esi
        movl    Dacc, %edi
        movl    Dlength, %ecx
        movl    SperD, %eax
        xor     %edx, %edx

        movq    zeros, %mm0

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

        popal
        ret

