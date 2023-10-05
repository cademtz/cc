#include <cc/ir.h>
#include <cc/lib.h>

const cc_ir_ins_format cc_ir_ins_formats[CC_IR_OPCODE__COUNT] =
{
//   mnemonic,   write-only,            read-only,            read-only            
    {"ldc",     {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_U32,    0}},
    {"ldla",    {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_LOCAL,  0}},
    {"ldls",    {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_LOCAL,  0}},
    {"add",     {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_LOCAL,  CC_IR_OPERAND_LOCAL}},
    {"sub",     {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_LOCAL,  CC_IR_OPERAND_LOCAL}},
    {"jnz",     {0,                     CC_IR_OPERAND_LOCAL,  0}},
    {"ret",     {0,                     0,                    0}},
};

void cc_ir_func_create(cc_ir_func* func) {
    memset(func, 0, sizeof(*func));
}
void cc_ir_func_destroy(cc_ir_func* func)
{
    // Free each block's instruction array
    for (size_t i = 0; i < func->num_blocks; ++i)
        free(func->blocks[i].ins);
    
    free(func->blocks);
    free(func->locals);
    memset(func, 0, sizeof(*func));
}

static cc_ir_local* cc_ir_func_local(cc_ir_func* func, const char* name, uint32_t var_size, uint16_t localtypeid)
{
    ++func->num_locals;
    func->locals = (cc_ir_local*)realloc(func->locals, func->num_locals * sizeof(func->locals[0]));
    
    cc_ir_local* local = &func->locals[func->num_locals - 1];
    local->localid = func->_next_localid++;
    local->name = name;
    local->var_size = var_size;
    local->localtypeid = localtypeid;
    return local;
}

cc_ir_block* cc_ir_func_block(cc_ir_func* func, const char* name)
{
    cc_ir_local* local = cc_ir_func_local(func, name, 0, CC_IR_LOCALTYPEID_BLOCK);
    ++func->num_blocks;
    func->blocks = (cc_ir_block*)realloc(func->blocks, func->num_blocks * sizeof(func->blocks[0]));

    cc_ir_block* block = &func->blocks[func->num_blocks - 1];
    memset(block, 0, sizeof(*block));
    return block;
}

cc_ir_localid cc_ir_func_int(cc_ir_func* func, uint32_t size_bytes, const char* name) {
    return cc_ir_func_local(func, name, size_bytes, CC_IR_LOCALTYPEID_INT)->localid;
}

/// @brief Append an instruction to the block
/// @return Instruction index
static size_t cc_ir_block_ins(cc_ir_block* block)
{
    ++block->num_ins;
    block->ins = (cc_ir_ins*)realloc(block->ins, block->num_ins * sizeof(block->ins[0]));
    size_t index = block->num_ins - 1;
    memset(&block->ins[index], 0, sizeof(block->ins[index]));
    return index;
}

/// @brief Shortcut to create a binary operation instruction
/// @return Instruction index
static size_t cc_ir_block_binary(cc_ir_block* block, uint8_t opcode, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs)
{
    size_t i = cc_ir_block_ins(block);
    block->ins[i].opcode = opcode;
    block->ins[i].write = dst;
    block->ins[i].read.local[0] = lhs;
    block->ins[i].read.local[1] = rhs;
    return i;
}

size_t cc_ir_block_ldc(cc_ir_block* block, cc_ir_localid dst, uint32_t value)
{
    size_t i = cc_ir_block_ins(block);
    block->ins[i].opcode = CC_IR_OPCODE_LDC;
    block->ins[i].write = dst;
    block->ins[i].read.u32 = value;
    return i;
}

size_t cc_ir_block_add(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs) {
    return cc_ir_block_binary(block, CC_IR_OPCODE_ADD, dst, lhs, rhs);
}
size_t cc_ir_block_sub(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs) {
    return cc_ir_block_binary(block, CC_IR_OPCODE_SUB, dst, lhs, rhs);
}
