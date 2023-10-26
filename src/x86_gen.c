#include <cc/x86_gen.h>
#include <cc/ir.h>

const x86conv X86_CONV_WIN64_FASTCALL =
{
    {
        1, 1, 1, 0, // a, c, d, b
        0, 0, 0, 0, // sp, bp, si, di

        1, 1, 1, 1, // r8, r9, r10, r11
        0, 0, 0, 0, // r12, r13, r14, r15

        1, 1, 1, 1, // xmm0, xmm1, xmm2, xmm3
        1, 1, 0, 0, // xmm4, xmm5, xmm6, xmm7

        0, 0, 0, 0, // xmm8, xmm9, xmm10, xmm11
        0, 0, 0, 0, // xmm12, xmm13, xmm14, xmm15
    },
    32,    // stack_preargs
    0,     // stack_postargs
    false, // noreturn
};
const x86conv X86_CONV_SYSV64_CDECL =
{
    {
        1, 1, 1, 0, // a, c, d, b
        0, 0, 1, 1, // sp, bp, si, di

        1, 1, 1, 1, // r8, r9, r10, r11
        0, 0, 0, 0, // r12, r13, r14, r15

        0, 0, 0, 0, // xmm0, xmm1, xmm2, xmm3
        0, 0, 0, 0, // xmm4, xmm5, xmm6, xmm7

        0, 0, 0, 0, // xmm8, xmm9, xmm10, xmm11
        0, 0, 0, 0, // xmm12, xmm13, xmm14, xmm15
    },
    0,     // stack_preargs
    0,     // stack_postargs
    false, // noreturn
};

void x86gen_create(x86gen* gen, const x86conv* conv, const cc_ir_func* irfunc)
{
    memset(gen, 0, sizeof(*gen));
    gen->conv = conv;
    gen->irfunc = irfunc;
}

void x86gen_destroy(x86gen* gen) {}
