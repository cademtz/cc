#include "test.h"
#include <cc/x86_gen.h>
#include <stdio.h>

#define equal_code(func, offset, expected) equal__code(func, offset, expected, sizeof(expected) - 1)

/// @brief Check if an x86 function contains the exact sequence of bytes in `expected`
/// @param offset Offset of code to compare
/// @param expected The expected x86 code
/// @param expected_len Length of expected code
/// @return `true` if the expected code matches
static int equal__code(const x86func* func, size_t offset, const char* expected, size_t expected_len)
{   
    uint8_t* code = func->code + offset;
    uint8_t* end = func->code + func->size_code;

    size_t min_len = end - code;
    int result = min_len <= expected_len && memcmp(code, expected, min_len) == 0;
    if (!result)
    {
        printf("Expected x86:  ");
        for(size_t i = 0; i < expected_len; ++i)
            printf(" %.2X", (uint8_t)expected[i]);
        printf("\n");
        printf("Incorrect x86: ");
        for (size_t i = 0; i < min_len; ++i)
            printf(" %.2X", code[i]);
        printf("\n");
    }
    return result;
}

int test_x86(void)
{
    x86func func;

    { // Test return
        x86func_create(&func, X86_MODE_LONG);
        x86func_ret(&func);
        test_assert("Expected opcode 0xC3", equal_code(&func, 0, "\xC3"));
        x86func_destroy(&func);
    }
    { // Test various operands in long mode with default operand size
        x86func_create(&func, X86_MODE_LONG);

        size_t offset = func.size_code;
        x86func_add(&func, 0, x86_reg(X86_REG_C), x86_reg(X86_REG_B));
        test_assert("Expected `add ecx, ebx`", equal_code(&func, offset, "\x01\xD9"));

        offset = func.size_code;
        x86func_add(&func, 0, x86_reg(X86_REG_R15), x86_reg(X86_REG_B));
        test_assert("Expected `add r15d, ebx`", equal_code(&func, offset, "\x41\x01\xDF"));

        offset = func.size_code;
        x86func_add(&func, 0, x86_reg(X86_REG_B), x86_deref(X86_REG_R15));
        test_assert("Expected `add ebx, DWORD PTR [r15]`", equal_code(&func, offset, "\x41\x03\x1F"));
        
        offset = func.size_code;
        x86func_add(&func, 0, x86_deref(X86_REG_SP), x86_reg(X86_REG_C));
        test_assert("Expected `add DWORD PTR [rsp], ecx`", equal_code(&func, offset, "\x01\x0C\x24"));
        
        offset = func.size_code;
        x86func_add(&func, 0, x86_deref(X86_REG_BP), x86_reg(X86_REG_C));
        test_assert("Expected `add DWORD PTR [rbp+0x0], ecx`", equal_code(&func, offset, "\x01\x4D\x00"));
        
        offset = func.size_code;
        x86func_add(&func, 0, x86_index(X86_REG_BP, X86_REG_A, X86_SIB_SCALE_4, -0x20), x86_reg(X86_REG_C));
        test_assert("Expected `add DWORD PTR [rbp+rax*4-0x20], ecx`", equal_code(&func, offset, "\x01\x4C\x85\xE0"));
        
        offset = func.size_code;
        x86func_add(&func, 0, x86_index(X86_REG_BP, X86_REG_A, X86_SIB_SCALE_4, -0x400), x86_reg(X86_REG_C));
        test_assert("Expected `add DWORD PTR [rbp+rax*4-0x400], ecx`", equal_code(&func, offset, "\x01\x8C\x85\x00\xFC\xFF\xFF"));
        
        offset = func.size_code;
        x86func_add(&func, 0, x86_index(X86_REG_A, X86_REG_C, X86_SIB_SCALE_1, 0), x86_const(0x20));
        test_assert("Expected `add DWORD PTR [eax+ecx], 0x20`", equal_code(&func, offset, "\x83\x04\x08\x20"));
        
        offset = func.size_code;
        x86func_add(&func, 0, x86_offset(0x44444444), x86_const(0x33333333));
        test_assert("Expected `add DWORD PTR [rip+0x44444444], 0x33333333`", equal_code(&func, offset, "\x81\x05\x44\x44\x44\x44\x33\x33\x33\x33"));

        x86func_destroy(&func);
    }
    { // Test reg, mem, and const operands in long mode with various operand sizes
        x86func_create(&func, X86_MODE_LONG);

        // byte size
        
        size_t offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_BYTE, x86_reg(X86_REG_C), x86_const(0x11));
        test_assert("Expected `add cl, 0x11`", equal_code(&func, offset, "\x80\xC1\x11"));
        
        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_BYTE, x86_reg(X86_REG_C), x86_reg(X86_REG_A));
        test_assert("Expected `add cl, al`", equal_code(&func, offset, "\x00\xC1"));

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_BYTE, x86_deref(X86_REG_C), x86_const(0x11));
        test_assert("Expected `add BYTE PTR [rcx], 0x11`", equal_code(&func, offset, "\x80\x01\x11"));

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_BYTE, x86_deref(X86_REG_C), x86_reg(X86_REG_A));
        test_assert("Expected `add BYTE PTR [rcx], al`", equal_code(&func, offset, "\x00\x01"));

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_BYTE, x86_reg(X86_REG_C), x86_deref(X86_REG_A));
        test_assert("Expected `add cl, BYTE PTR [rax]`", equal_code(&func, offset, "\x02\x08"));

        // word size

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_WORD, x86_reg(X86_REG_C), x86_const(0x1122));
        test_assert("Expected `add cx, 0x1122`", equal_code(&func, offset, "\x66\x81\xC1\x22\x11"));

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_WORD, x86_deref(X86_REG_C), x86_const(0x11));
        test_assert("Expected `add WORD PTR [rcx], 0x11`", equal_code(&func, offset, "\x66\x83\x01\x11"));

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_WORD, x86_deref(X86_REG_C), x86_reg(X86_REG_A));
        test_assert("Expected `add WORD PTR [rcx], ax`", equal_code(&func, offset, "\x66\x01\x01"));

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_WORD, x86_reg(X86_REG_C), x86_deref(X86_REG_A));
        test_assert("Expected `add cx, WORD PTR [rax]`", equal_code(&func, offset, "\x66\x03\x08"));
        
        // qword size

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_QWORD, x86_reg(X86_REG_C), x86_const(0x11));
        test_assert("Expected `add rcx, 0x11`", equal_code(&func, offset, "\x48\x83\xC1\x11"));

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_QWORD, x86_deref(X86_REG_C), x86_const(0x11));
        test_assert("Expected `add QWORD PTR [rcx], 0x11`", equal_code(&func, offset, "\x48\x83\x01\x11"));

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_QWORD, x86_deref(X86_REG_C), x86_reg(X86_REG_A));
        test_assert("Expected `add QWORD PTR [rcx], rax`", equal_code(&func, offset, "\x48\x01\x01"));

        offset = func.size_code;
        x86func_add(&func, X86_OPSIZE_QWORD, x86_reg(X86_REG_C), x86_deref(X86_REG_A));
        test_assert("Expected `add rcx, QWORD PTR [rax]`", equal_code(&func, offset, "\x48\x03\x08"));

        x86func_destroy(&func);
    }
    { // Test mov instruction with various operands and operand sizes
        x86func_create(&func, X86_MODE_LONG);

        // dword size (default)

        size_t offset = func.size_code;
        x86func_mov(&func, 0, x86_deref(X86_REG_C), x86_reg(X86_REG_B));
        test_assert("Expected `mov DWORD PTR [rcx], ebx", equal_code(&func, offset, "\x89\x19"));

        offset = func.size_code;
        x86func_mov(&func, 0, x86_reg(X86_REG_C), x86_deref(X86_REG_B));
        test_assert("Expected `mov ecx, DWORD PTR [rbx]", equal_code(&func, offset, "\x8B\x0B"));

        offset = func.size_code;
        x86func_mov(&func, 0, x86_deref(X86_REG_C), x86_const(0x11));
        test_assert("Expected `mov DWORD PTR [rcx], 0x11", equal_code(&func, offset, "\xC7\x01\x11\x00\x00\x00"));

        // byte size

        offset = func.size_code;
        x86func_mov(&func, X86_OPSIZE_BYTE, x86_deref(X86_REG_C), x86_reg(X86_REG_B));
        test_assert("Expected `mov BYTE PTR [rcx], bl", equal_code(&func, offset, "\x88\x19"));

        offset = func.size_code;
        x86func_mov(&func, X86_OPSIZE_BYTE, x86_reg(X86_REG_C), x86_deref(X86_REG_B));
        test_assert("Expected `mov cl, BYTE PTR [rbx]", equal_code(&func, offset, "\x8A\x0B"));

        offset = func.size_code;
        x86func_mov(&func, X86_OPSIZE_BYTE, x86_deref(X86_REG_C), x86_const(0x11));
        test_assert("Expected `mov BYTE PTR [rcx], 0x11", equal_code(&func, offset, "\xC6\x01\x11"));

        // word size

        offset = func.size_code;
        x86func_mov(&func, X86_OPSIZE_WORD, x86_deref(X86_REG_C), x86_const(0x11));
        test_assert("Expected `mov WORD PTR [rcx], 0x11", equal_code(&func, offset, "\x66\xC7\x01\x11\x00"));

        // qword size

        offset = func.size_code;
        x86func_mov(&func, X86_OPSIZE_QWORD, x86_deref(X86_REG_C), x86_const(0x11));
        test_assert("Expected `mov QWORD PTR [rcx], 0x11", equal_code(&func, offset, "\x48\xC7\x01\x11\x00\x00\x00"));

        x86func_destroy(&func);
    }
    { // Test lhs_imm and rhs_imm's values
        x86func_create(&func, X86_MODE_LONG);
        x86label label = x86func_newlabel(&func);

        x86func_jz(&func, label);
        test_assert("Expected 4-byte immediate", func.lhs_imm.size == 4 && func.lhs_imm.offset == func.size_code - 4);

        x86func_add(&func, X86_OPSIZE_DWORD, x86_offset(0xFF), x86_const(0xFF));
        test_assert("Expected 4-byte lhs immediate", func.lhs_imm.size == 4 && func.lhs_imm.offset < func.size_code - 4);
        test_assert("Expected 4-byte rhs immediate", func.rhs_imm.size == 4 && func.rhs_imm.offset == func.size_code - 4);

        x86func_destroy(&func);
    }
    { // Test imul
        x86func_create(&func, X86_MODE_LONG);

        x86func_imul2(&func, 0, X86_REG_A, x86_reg(X86_REG_C));
        test_assert("Expected `imul eax, ecx`", equal_code(&func, 0, "\x0F\xAF\xC1"));

        size_t offset = func.size_code;
        x86func_imul2(&func, X86_OPSIZE_QWORD, X86_REG_A, x86_reg(X86_REG_C));
        test_assert("Expected `imul eax, ecx`", equal_code(&func, offset, "\x48\x0F\xAF\xC1"));

        offset = func.size_code;
        x86func_imul2(&func, 0, X86_REG_A, x86_const(0x11223344));
        test_assert("Expected `imul eax, eax, 0x11223344`", equal_code(&func, offset, "\x69\xC0\x44\x33\x22\x11"));

        offset = func.size_code;
        x86func_imul(&func, 0, x86_reg(X86_REG_R10));
        test_assert("Expected `imul r10d`", equal_code(&func, offset, "\x41\xF7\xEA"));

        x86func_destroy(&func);
    }
    { // Test some labels and jumps
        x86func_create(&func, X86_MODE_PROTECTED);
        x86label loop = x86func_newlabel(&func);
        x86label exit = x86func_newlabel(&func);
        x86_regmem eax = x86_reg(X86_REG_A);
        x86_regmem ecx = x86_reg(X86_REG_C);

        x86func_mov(&func, 0, eax, x86_const(0));       // eax = 0
        x86func_mov(&func, 0, ecx, x86_offset(0x1000)); // ecx = *(uint32_t*)(0x1000)
        x86func_cmp(&func, 0, ecx, x86_const(0));       // if (ecx == 0) goto exit
        x86func_je(&func, exit);
        
        x86func_label(&func, loop);               // loop:
        x86func_add(&func, 0, eax, ecx);          // eax += ecx
        x86func_cmp(&func, 0, eax, x86_const(0)); // if (eax < 0) goto loop
        x86func_jl(&func, loop);

        x86func_label(&func, exit);
        x86func_ret(&func);

        test_assert("Expected matching code sample", equal_code(&func, 0,
            "\xC7\xC0\x00\x00\x00\x00\x8B\x05\x00\x10\x00\x00\x83\xF9\x00\x0F\x84\x07\x00\x00\x00\x01\xC8\x83\xF8\x00\x7C\xF9\xC3"
        ));
        
        printf("x86:\n");
        for (size_t i = 0; i < func.size_code; ++i)
            printf("%.2X ", func.code[i]);
        printf("\n");
        
        x86func_destroy(&func);
    }

    return 1;
}