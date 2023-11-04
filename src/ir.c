#include <cc/ir.h>
#include <cc/lib.h>

const cc_ir_ins_format cc_ir_ins_formats[CC_IR_OPCODE__COUNT] =
{
//   mnemonic,   write-only,            read-only,            read-only            
    {"ldc",     {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_U32,    0}},
    {"ldla",    {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_LOCAL,  0}},
    {"ldls",    {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_LOCAL,  0}},
    {"mov",     {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_LOCAL,  0}},
    {"add",     {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_LOCAL,  CC_IR_OPERAND_LOCAL}},
    {"sub",     {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_LOCAL,  CC_IR_OPERAND_LOCAL}},
    {"jmp",     {0,                     CC_IR_OPERAND_LOCAL,  0}},
    {"jnz",     {0,                     CC_IR_OPERAND_LOCAL,  CC_IR_OPERAND_LOCAL}},
    {"ret",     {0,                     0,                    0}},
    {"retl",    {0,                     CC_IR_OPERAND_LOCAL,  0}},
    {"phi",     {CC_IR_OPERAND_LOCAL,   CC_IR_OPERAND_LOCAL,  CC_IR_OPERAND_LOCAL}},
};

/// @brief Append a new local to the function's locals array
static cc_ir_localid cc_ir_func_local(cc_ir_func* func, const char* name, uint32_t var_size, uint16_t localtypeid)
{
    ++func->num_locals;
    func->locals = (cc_ir_local*)realloc(func->locals, func->num_locals * sizeof(func->locals[0]));

    char* name_copy = NULL;
    if (name && name[0])
    {
        size_t len = strlen(name);
        name_copy = (char*)malloc(len + 1);
        memcpy(name_copy, name, len + 1);
    }
    
    cc_ir_local* local = &func->locals[func->num_locals - 1];
    local->localid = func->_next_localid++;
    local->name = name_copy;
    local->var_size = var_size;
    local->localtypeid = localtypeid;
    return local->localid;
}

void cc_ir_func_create(cc_ir_func* func, const char* name)
{
    memset(func, 0, sizeof(*func));

    // Create a local to represent the function. This should be localid 0
    cc_ir_func_local(func, name, 0, CC_IR_LOCALTYPEID_FUNC);

    // Create the entry block.
    // Don't forget to give it a localid (cc_ir_func_insert normally does this for us)
    func->entry_block = (cc_ir_block*)malloc(sizeof(*func->entry_block));
    memset(func->entry_block, 0, sizeof(*func->entry_block));
    func->entry_block->localid = cc_ir_func_local(func, NULL, 0, CC_IR_LOCALTYPEID_BLOCK);
}

void cc_ir_func_clone(const cc_ir_func* func, cc_ir_func* clone)
{
    memset(clone, 0, sizeof(*clone));
    clone->num_blocks = func->num_blocks;
    clone->num_locals = func->num_locals;
    clone->_next_localid = func->_next_localid;

    // Clone the locals array and the names of all locals
    clone->locals = (cc_ir_local*)malloc(clone->num_locals * sizeof(clone->locals[0]));
    memcpy(clone->locals, func->locals, clone->num_locals * sizeof(clone->locals[0]));
    for (size_t i = 0; i < clone->num_locals; ++i)
    {
        if (clone->locals[i].name)
        {
            size_t size = strlen(func->locals[i].name) + 1;
            clone->locals[i].name = (char*)malloc(size);
            memcpy(clone->locals[i].name, func->locals[i].name, size);
        }
    }

    // Clone the linked list of blocks
    cc_ir_block** link = &clone->entry_block;
    for (const cc_ir_block* block = func->entry_block; block; block = block->next_block)
    {
        cc_ir_block* clone_block = (cc_ir_block*)malloc(sizeof(*block));
        memcpy(clone_block, block, sizeof(*clone_block));
        *link = clone_block;
        link = &clone_block->next_block;
        
        if (clone_block->ins)
        {
            clone_block->ins = (cc_ir_ins*)malloc(clone_block->num_ins * sizeof(clone_block->ins[0]));
            memcpy(clone_block->ins, block->ins, clone_block->num_ins * sizeof(clone_block->ins[0]));
        }
    }
}

void cc_ir_func_destroy(cc_ir_func* func)
{
    // Free each block's instruction array, and then the block itself
    cc_ir_block* block = func->entry_block;
    while (block)
    {
        cc_ir_block* next_block = block->next_block;
        free(block->ins);
        free(block);
        block = next_block;
    }
    
    for (size_t i = 0; i < func->num_locals; ++i)
        free(func->locals[i].name);
    free(func->locals);
    memset(func, 0, sizeof(*func));
}

cc_ir_local* cc_ir_func_getlocal(const cc_ir_func* func, cc_ir_localid localid)
{
    assert(localid < func->num_locals && "localid out of bounds");
    return &func->locals[localid];
}
cc_ir_block* cc_ir_func_getblock(const cc_ir_func* func, cc_ir_localid localid)
{
    for (cc_ir_block* block = func->entry_block; block; block = block->next_block)
    {
        if (block->localid == localid)
            return block;
    }
    return NULL;
}

cc_ir_block* cc_ir_func_insert(cc_ir_func* func, cc_ir_block* prev, const char* name)
{
    cc_ir_localid localid = cc_ir_func_local(func, name, 0, CC_IR_LOCALTYPEID_BLOCK);
    ++func->num_blocks;
    
    cc_ir_block* next = prev->next_block;
    cc_ir_block* current = (cc_ir_block*)malloc(sizeof(cc_ir_block));
    memset(current, 0, sizeof(*current));
    current->localid = localid;
    current->next_block = next;

    prev->next_block = current;
    return current;
}

cc_ir_localid cc_ir_func_int(cc_ir_func* func, uint32_t size_bytes, const char* name) {
    return cc_ir_func_local(func, name, size_bytes, CC_IR_LOCALTYPEID_INT);
}
cc_ir_localid cc_ir_func_ptr(cc_ir_func* func, const char* name) {
    return cc_ir_func_local(func, name, 0, CC_IR_LOCALTYPEID_PTR);
}
cc_ir_localid cc_ir_func_clonelocal(cc_ir_func* func, cc_ir_localid localid, const char* name)
{
    const cc_ir_local* info = cc_ir_func_getlocal(func, localid);
    return cc_ir_func_local(func, name, info->var_size, info->localtypeid);
}

void cc_ir_block_insert(cc_ir_block* block, size_t index, cc_ir_ins ins)
{
    assert(index <= block->num_ins && "Instruction index out of bounds");

    size_t new_num = block->num_ins + 1;
    block->ins = (cc_ir_ins*)realloc(block->ins, new_num * sizeof(block->ins[0]));
    if (index != new_num - 1)
        memmove(block->ins + index + 1, block->ins + index, (block->num_ins - index) * sizeof(block->ins[0]));
    block->num_ins = new_num;
    block->ins[index] = ins;
}

/// @brief Append an instruction to the block
/// @return Instruction pointer (do not save it)
static cc_ir_ins* cc_ir_block_ins(cc_ir_block* block, uint8_t opcode)
{
    cc_ir_ins ins = {0};
    ins.opcode = opcode;
    cc_ir_block_insert(block, block->num_ins, ins);
    return &block->ins[block->num_ins - 1];
}

/// @brief Shortcut to create a binary operation instruction
static void cc_ir_block_binary(cc_ir_block* block, uint8_t opcode, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs)
{
    cc_ir_ins* ins = cc_ir_block_ins(block, opcode);
    ins->write = dst;
    ins->read.local[0] = lhs;
    ins->read.local[1] = rhs;
}

void cc_ir_block_ldc(cc_ir_block* block, cc_ir_localid dst, uint32_t value)
{
    cc_ir_ins* ins = cc_ir_block_ins(block, CC_IR_OPCODE_LDC);
    ins->write = dst;
    ins->read.u32 = value;
}
void cc_ir_block_ldla(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid src)
{
    cc_ir_ins* ins = cc_ir_block_ins(block, CC_IR_OPCODE_LDLA);
    ins->write = dst;
    ins->read.local[0] = src;
}
void cc_ir_block_ldls(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid src)
{
    cc_ir_ins* ins = cc_ir_block_ins(block, CC_IR_OPCODE_LDLS);
    ins->write = dst;
    ins->read.local[0] = src;
}
void cc_ir_block_add(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs) {
    cc_ir_block_binary(block, CC_IR_OPCODE_ADD, dst, lhs, rhs);
}
void cc_ir_block_sub(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs) {
    cc_ir_block_binary(block, CC_IR_OPCODE_SUB, dst, lhs, rhs);
}
void cc_ir_block_jmp(cc_ir_block* block, const cc_ir_block* dst)
{
    cc_ir_ins* ins = cc_ir_block_ins(block, CC_IR_OPCODE_JMP);
    ins->read.local[0] = dst->localid;
}
void cc_ir_block_jnz(cc_ir_block* block, const cc_ir_block* dst, cc_ir_localid value)
{
    cc_ir_ins* ins = cc_ir_block_ins(block, CC_IR_OPCODE_JNZ);
    ins->read.local[0] = dst->localid;
    ins->read.local[1] = value;
}
void cc_ir_block_ret(cc_ir_block* block) {
    cc_ir_block_ins(block, CC_IR_OPCODE_RET);
}
void cc_ir_block_retl(cc_ir_block* block, cc_ir_localid value)
{
    cc_ir_ins* ins = cc_ir_block_ins(block, CC_IR_OPCODE_RETL);
    ins->read.local[0] = value;
}
void cc_ir_block_phi(cc_ir_block* block, cc_ir_localid dst, cc_ir_localid lhs, cc_ir_localid rhs) {
    cc_ir_block_binary(block, CC_IR_OPCODE_PHI, dst, lhs, rhs);
}

cc_ir_ins cc_ir_ins_mov(cc_ir_localid dst, cc_ir_localid src)
{
    cc_ir_ins ins;
    memset(&ins, 0, sizeof(ins));
    ins.opcode = CC_IR_OPCODE_MOV;
    ins.write = dst;
    ins.read.local[0] = src;
    return ins;
}