#include "test.h"
#include <cc/x86_gen.h>
#include <stdio.h>

int test_x86gen(void)
{
    x86varmap map;
    x86varmap_create(&map);
    
    cc_ir_localid dst = 0, lhs = 1, rhs = 2;
    x86operand dst_operand = x86_index(X86_REG_SP, X86_REG_SP, 0, 0);
    x86operand lhs_operand = x86_index(X86_REG_SP, X86_REG_SP, 0, -4);
    x86operand rhs_operand = x86_index(X86_REG_SP, X86_REG_SP, 0, -8);

    x86varmap_setperm(&map, dst, dst_operand);
    x86varmap_setperm(&map, lhs, lhs_operand);
    x86varmap_setperm(&map, rhs, rhs_operand);

    test_assert("Expected equal operands", !x86operand_cmp(dst_operand, x86varmap_getperm(&map, dst)));
    test_assert("Expected equal operands", !x86operand_cmp(lhs_operand, x86varmap_getperm(&map, lhs)));
    test_assert("Expected equal operands", !x86operand_cmp(rhs_operand, x86varmap_getperm(&map, rhs)));
    
    // Test setting and clearing the temp value
    test_assert("Expected equal operands", !x86operand_cmp(dst_operand, x86varmap_gettemp(&map, dst)));
    x86varmap_settemp(&map, dst, x86_reg(X86_REG_A));
    test_assert("Expected equal operands", !x86operand_cmp(x86_reg(X86_REG_A), x86varmap_gettemp(&map, dst)));
    x86varmap_update(&map, dst);
    test_assert("Expected equal operands", !x86operand_cmp(dst_operand, x86varmap_gettemp(&map, dst)));

    // Test overwriting an operand, with no reference to the local
    test_assert("Expected equal operands", !x86operand_cmp(dst_operand, x86varmap_gettemp(&map, dst)));
    x86varmap_settemp(&map, dst, x86_reg(X86_REG_A));
    x86varmap_settemp(&map, lhs, x86_reg(X86_REG_B));
    test_assert("Expected equal operands", !x86operand_cmp(x86_reg(X86_REG_A), x86varmap_gettemp(&map, dst)));
    test_assert("Expected equal operands", !x86operand_cmp(x86_reg(X86_REG_B), x86varmap_gettemp(&map, lhs)));
    x86varmap_overwrite(&map, x86_reg(X86_REG_A));
    test_assert("Expected equal operands", !x86operand_cmp(dst_operand, x86varmap_gettemp(&map, dst)));
    test_assert("Expected equal operands", !x86operand_cmp(x86_reg(X86_REG_B), x86varmap_gettemp(&map, lhs)));

    return 1;
}