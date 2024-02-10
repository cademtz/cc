#include <cc/ir.h>
#include <cc/lib.h>

const cc_ir_ins_format cc_ir_ins_formats[CC_IR_OPCODE__COUNT] =
{
//   mnemonic,  operands          
    {"la",      {CC_IR_OPERAND_LOCAL}},
    {"ls",      {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_LOCAL}},
    {"lld",     {CC_IR_OPERAND_LOCAL}},

    {"iconst",  {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_U32}},
    {"uconst",  {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_U32}},
    {"ld",      {CC_IR_OPERAND_DATASIZE}},
    {"sto",     {CC_IR_OPERAND_DATASIZE}},
    {"dup",     {CC_IR_OPERAND_DATASIZE}},

    {"add",     {CC_IR_OPERAND_DATASIZE}},
    {"sub",     {CC_IR_OPERAND_DATASIZE}},
    {"mul",     {CC_IR_OPERAND_DATASIZE}},
    {"umul",    {CC_IR_OPERAND_DATASIZE}},
    {"div",     {CC_IR_OPERAND_DATASIZE}},
    {"udiv",    {CC_IR_OPERAND_DATASIZE}},
    {"mod",     {CC_IR_OPERAND_DATASIZE}},
    {"umod",    {CC_IR_OPERAND_DATASIZE}},
    {"neg",     {CC_IR_OPERAND_DATASIZE}},

    {"not",     {CC_IR_OPERAND_DATASIZE}},
    {"and",     {CC_IR_OPERAND_DATASIZE}},
    {"or",      {CC_IR_OPERAND_DATASIZE}},
    {"xor",     {CC_IR_OPERAND_DATASIZE}},
    {"lsh",     {CC_IR_OPERAND_DATASIZE}},
    {"rsh",     {CC_IR_OPERAND_DATASIZE}},

    {"zext",    {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_EXTEND_DATASIZE}},
    {"sext",    {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_EXTEND_DATASIZE}},

    {"call",    {0}},
    {"jmp",     {0}},
    {"jnz",     {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_LOCAL}},
    {"ret",     {0}},
};

/// @brief Append a new local to the function's locals array
static cc_ir_localid cc_ir_func_local(cc_ir_func* func, const char* name, uint32_t data_size, uint16_t localtypeid)
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
    local->data_size = data_size;
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
    return cc_ir_func_local(func, name, info->data_size, info->localtypeid);
}

void cc_ir_block_insert(cc_ir_block* block, size_t index, const cc_ir_ins* ins)
{
    assert(index <= block->num_ins && "Instruction index out of bounds");

    size_t new_num = block->num_ins + 1;
    block->ins = (cc_ir_ins*)realloc(block->ins, new_num * sizeof(block->ins[0]));
    if (index != new_num - 1)
        memmove(block->ins + index + 1, block->ins + index, (block->num_ins - index) * sizeof(block->ins[0]));
    block->num_ins = new_num;
    block->ins[index] = *ins;
}

static size_t cc__ir_block_append_noop(cc_ir_block* block, uint8_t opcode)
{
    cc_ir_ins ins = {0};
    ins.opcode = opcode;
    return cc_ir_block_append(block, &ins);
}
static size_t cc__ir_block_append_localop(cc_ir_block* block, uint8_t opcode, cc_ir_localid localid)
{
    cc_ir_ins ins = {0};
    ins.opcode = opcode;
    ins.operand.local = localid;
    return cc_ir_block_append(block, &ins);
}
static size_t cc__ir_block_append_u32op(cc_ir_block* block, uint8_t opcode, uint32_t u32)
{
    cc_ir_ins ins = {0};
    ins.opcode = opcode;
    ins.operand.u32 = u32;
    return cc_ir_block_append(block, &ins);
}
static size_t cc__ir_block_append_sizeop(cc_ir_block* block, uint8_t opcode, cc_ir_datasize data_size)
{
    cc_ir_ins ins = {0};
    ins.opcode = opcode;
    ins.data_size = data_size;
    return cc_ir_block_append(block, &ins);
}

void cc_ir_block_la(cc_ir_block* block, cc_ir_localid localid) { cc__ir_block_append_localop(block, CC_IR_OPCODE_LA, localid); }
void cc_ir_block_ls(cc_ir_block* block, cc_ir_datasize data_size, cc_ir_localid localid)
{
    size_t index = cc__ir_block_append_localop(block, CC_IR_OPCODE_LS, localid);
    block->ins[index].data_size = data_size;
}
void cc_ir_block_lld(cc_ir_block* block, cc_ir_localid localid) { cc__ir_block_append_localop(block, CC_IR_OPCODE_LLD, localid); }
void cc_ir_block_iconst(cc_ir_block* block, cc_ir_datasize data_size, int32_t value)
{
    size_t index = cc__ir_block_append_u32op(block, CC_IR_OPCODE_ICONST, (uint32_t)value);
    block->ins[index].data_size = data_size;
}
void cc_ir_block_uconst(cc_ir_block* block, cc_ir_datasize data_size, uint32_t value)
{
    size_t index = cc__ir_block_append_u32op(block, CC_IR_OPCODE_UCONST, value);
    block->ins[index].data_size = data_size;
}
void cc_ir_block_ld(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_LD, data_size); }
void cc_ir_block_sto(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_STO, data_size); }
void cc_ir_block_dup(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_DUP, data_size); }
void cc_ir_block_add(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_ADD, data_size); }
void cc_ir_block_sub(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_SUB, data_size); }
void cc_ir_block_mul(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_MUL, data_size); }
void cc_ir_block_umul(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_UMUL, data_size); }
void cc_ir_block_div(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_DIV, data_size); }
void cc_ir_block_udiv(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_UDIV, data_size); }
void cc_ir_block_mod(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_MOD, data_size); }
void cc_ir_block_umod(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_UMOD, data_size); }
void cc_ir_block_neg(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_NEG, data_size); }
void cc_ir_block_not(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_NOT, data_size); }
void cc_ir_block_and(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_AND, data_size); }
void cc_ir_block_or(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_OR, data_size); }
void cc_ir_block_xor(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_XOR, data_size); }
void cc_ir_block_lsh(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_LSH, data_size); }
void cc_ir_block_rsh(cc_ir_block* block, cc_ir_datasize data_size) { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_RSH, data_size); }
void cc_ir_block_zext(cc_ir_block* block, cc_ir_datasize data_size, cc_ir_datasize extend_data_size)
{
    size_t index = cc__ir_block_append_sizeop(block, CC_IR_OPCODE_ZEXT, data_size);
    block->ins[index].operand.extend_data_size = extend_data_size;
}
void cc_ir_block_sext(cc_ir_block* block, cc_ir_datasize data_size, cc_ir_datasize extend_data_size)
{
    size_t index = cc__ir_block_append_sizeop(block, CC_IR_OPCODE_SEXT, data_size);
    block->ins[index].operand.extend_data_size = extend_data_size;
}
void cc_ir_block_call(cc_ir_block* block) { cc__ir_block_append_noop(block, CC_IR_OPCODE_CALL); }
void cc_ir_block_jmp(cc_ir_block* block) { cc__ir_block_append_noop(block, CC_IR_OPCODE_JMP); }
void cc_ir_block_jnz(cc_ir_block* block, cc_ir_datasize data_size, const cc_ir_block* dst)
{
    size_t index = cc__ir_block_append_sizeop(block, CC_IR_OPCODE_JNZ, data_size);
    block->ins[index].operand.local = dst->localid;
}
void cc_ir_block_ret(cc_ir_block* block) { cc__ir_block_append_noop(block, CC_IR_OPCODE_RET); }