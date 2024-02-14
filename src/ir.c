#include <cc/ir.h>
#include <cc/lib.h>

const cc_ir_ins_format cc_ir_ins_formats[CC_IR_OPCODE__COUNT] =
{
//   mnemonic,  operands          
    {"argp",    {0}},
    {"addrl",   {CC_IR_OPERAND_LOCAL}},
    {"sizel",   {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_LOCAL}},
    {"loadl",   {CC_IR_OPERAND_LOCAL}},
    {"addrg",   {CC_IR_OPERAND_SYMBOLID}},

    {"sizep",   {CC_IR_OPERAND_DATASIZE}},

    {"iconst",  {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_U32}},
    {"uconst",  {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_U32}},
    {"load",    {CC_IR_OPERAND_DATASIZE}},
    {"store",   {CC_IR_OPERAND_DATASIZE}},
    {"dupe",    {CC_IR_OPERAND_DATASIZE}},

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
    {"jz",      {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_BLOCKID}},
    {"jnz",     {CC_IR_OPERAND_DATASIZE, CC_IR_OPERAND_BLOCKID}},
    {"ret",     {0}},
    {"int",     {CC_IR_OPERAND_U32}},
    {"frame",   {CC_IR_OPERAND_U32}},
};

void cc_ir_object_create(cc_ir_object* obj) {
    memset(obj, 0, sizeof(*obj));
}
void cc_ir_object_destroy(cc_ir_object* obj)
{
    for (size_t i = 0; i < obj->num_symbols; ++i)
        cc_ir_symbol_destroy(&obj->symbols[i]);
    free(obj->symbols);
}
cc_ir_symbol* cc_ir_object_get_symbolid(const cc_ir_object* obj, cc_ir_symbolid symbolid)
{
    for (size_t i = 0; i < obj->num_symbols; ++i)
    {
        if (obj->symbols[i].symbolid == symbolid)
            return &obj->symbols[i];
    }
    return NULL;
}
cc_ir_symbol* cc_ir_object_get_symbolname(const cc_ir_object* obj, const char* name, size_t name_len)
{
    if (name_len == (size_t)-1)
        name_len = strlen(name);
    if (!name_len)
        return NULL;

    for (size_t i = 0; i < obj->num_symbols; ++i)
    {
        cc_ir_symbol* symbol = &obj->symbols[i];
        if (symbol->name_len == name_len && !memcmp(symbol->name, name, name_len))
            return symbol;
    }
    return NULL;
}
cc_ir_symbolid cc_ir_object_add_symbol(cc_ir_object* obj, const char* name, size_t name_len, cc_ir_symbol** out_symbolptr)
{
    ++obj->num_symbols;
    cc_vec_resize(obj->symbols, obj->num_symbols);
    cc_ir_symbol* symbol = &obj->symbols[obj->num_symbols - 1];
    cc_ir_symbolid symbolid = obj->_next_symbolid++;
    cc_ir_symbol_create(symbol, symbolid, name, name_len);
    if (out_symbolptr)
        *out_symbolptr = symbol;
    return symbolid;
}
cc_ir_symbolid cc_ir_object_import(cc_ir_object* obj, bool is_runtime, const char* name, size_t name_len)
{
    cc_ir_symbol* symbol;
    cc_ir_symbolid symbolid = cc_ir_object_add_symbol(obj, name, name_len, &symbol);

    symbol->symbol_flags |= CC_IR_SYMBOLFLAG_EXTERNAL;
    if (is_runtime)
        symbol->symbol_flags |= CC_IR_SYMBOLFLAG_RUNTIME;
    return symbolid;
}
cc_ir_func* cc_ir_object_add_func(cc_ir_object* obj, const char* name, size_t name_len)
{
    cc_ir_symbol* symbol;
    cc_ir_symbolid symbolid = cc_ir_object_add_symbol(obj, name, name_len, &symbol);
    cc_ir_func* func = cc_ir_func_create(symbolid);
    symbol->ptr.func = func;
    return func;
}

void cc_ir_symbol_create(cc_ir_symbol* symbol, cc_ir_symbolid symbolid, const char* name, size_t name_len)
{
    memset(symbol, 0, sizeof(*symbol));
    symbol->symbolid = symbolid;
    symbol->name = cc_strclone_char(name, name_len, &symbol->name_len);
}
void cc_ir_symbol_destroy(cc_ir_symbol* symbol)
{
    if (!(symbol->symbol_flags & CC_IR_SYMBOLFLAG_EXTERNAL))
        cc_ir_func_destroy(symbol->ptr.func);
    free(symbol->name);
}

/// @brief Append a new local to the function's locals array
static cc_ir_localid cc_ir_func_local(cc_ir_func* func, const char* name, uint32_t data_size, uint16_t typeid)
{
    ++func->num_locals;
    cc_vec_resize(func->locals, func->num_locals);
    
    cc_ir_local* local = &func->locals[func->num_locals - 1];
    local->localid = func->_next_localid++;
    local->name = cc_strclone_char(name, (size_t)-1, NULL);
    local->data_size = data_size;
    local->typeid = typeid;
    return local->localid;
}

cc_ir_func* cc_ir_func_create(cc_ir_symbolid symbolid)
{
    cc_ir_func* func = (cc_ir_func*)calloc(1, sizeof(*func));
    memset(func, 0, sizeof(*func));

    func->symbolid = symbolid;
    // Create a local to represent the function. This should be localid 0
    cc_ir_func_local(func, NULL, 0, CC_IR_TYPEID_FUNC);
    // Create the entry block
    cc_ir_func_insert(func, NULL, NULL, 0);

    return func;
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
    // Free all blocks
    cc_ir_block* block = func->entry_block;
    while (block)
    {
        cc_ir_block* next_block = block->next_block;
        cc_ir_block_destroy(block);
        block = next_block;
    }
    
    // Free all locals
    for (size_t i = 0; i < func->num_locals; ++i)
        free(func->locals[i].name);
    free(func->locals);
    free(func);
}

cc_ir_local* cc_ir_func_getlocal(const cc_ir_func* func, cc_ir_localid localid)
{
    assert(localid < func->num_locals && "localid out of bounds");
    return &func->locals[localid];
}
cc_ir_block* cc_ir_func_getblock(const cc_ir_func* func, cc_ir_blockid blockid)
{
    for (cc_ir_block* block = func->entry_block; block; block = block->next_block)
    {
        if (block->blockid == blockid)
            return block;
    }
    return NULL;
}

cc_ir_block* cc_ir_func_insert(cc_ir_func* func, cc_ir_block* prev, const char* name, size_t name_len)
{
    cc_ir_blockid blockid = func->_next_blockid;
    ++func->num_blocks;
    ++func->_next_blockid;
    
    cc_ir_block* current = cc_ir_block_create(blockid, name, name_len);
    
    if (prev)
    {
        cc_ir_block* next = prev->next_block;
        current->next_block = next;
        prev->next_block = current;
    }
    else
    {
        current->next_block = func->entry_block;
        func->entry_block = current;
    }
    
    return current;
}

cc_ir_localid cc_ir_func_int(cc_ir_func* func, uint32_t size_bytes, const char* name) {
    return cc_ir_func_local(func, name, size_bytes, CC_IR_TYPEID_INT);
}
cc_ir_localid cc_ir_func_ptr(cc_ir_func* func, const char* name) {
    return cc_ir_func_local(func, name, 0, CC_IR_TYPEID_PTR);
}
cc_ir_localid cc_ir_func_clonelocal(cc_ir_func* func, cc_ir_localid localid, const char* name)
{
    const cc_ir_local* info = cc_ir_func_getlocal(func, localid);
    return cc_ir_func_local(func, name, info->data_size, info->typeid);
}

cc_ir_block* cc_ir_block_create(cc_ir_blockid blockid, const char* name, size_t name_len)
{
    cc_ir_block* block = (cc_ir_block*)calloc(1, sizeof(*block));
    memset(block, 0, sizeof(*block));
    block->blockid = blockid;
    block->name = cc_strclone_char(name, name_len, NULL);
    return block;
}
void cc_ir_block_destroy(cc_ir_block* block)
{
    free(block->name);
    free(block->ins);
    free(block);
}

void cc_ir_block_insert(cc_ir_block* block, size_t index, const cc_ir_ins* ins)
{
    assert(index <= block->num_ins && "Instruction index out of bounds");

    size_t new_num = block->num_ins + 1;
    cc_vec_resize(block->ins, new_num);
    if (index != new_num - 1)
        memmove(block->ins + index + 1, block->ins + index, (block->num_ins - index) * sizeof(block->ins[0]));
    block->num_ins = new_num;
    block->ins[index] = *ins;
}

static cc_ir_ins* cc__ir_block_append_noop(cc_ir_block* block, uint8_t opcode)
{
    cc_ir_ins ins = {0};
    ins.opcode = opcode;
    return &block->ins[cc_ir_block_append(block, &ins)];
}
static cc_ir_ins* cc__ir_block_append_localop(cc_ir_block* block, uint8_t opcode, cc_ir_localid localid)
{
    cc_ir_ins ins = {0};
    ins.opcode = opcode;
    ins.operand.local = localid;
    return &block->ins[cc_ir_block_append(block, &ins)];
}
static cc_ir_ins* cc__ir_block_append_u32op(cc_ir_block* block, uint8_t opcode, uint32_t u32)
{
    cc_ir_ins ins = {0};
    ins.opcode = opcode;
    ins.operand.u32 = u32;
    return &block->ins[cc_ir_block_append(block, &ins)];
}
static cc_ir_ins* cc__ir_block_append_sizeop(cc_ir_block* block, uint8_t opcode, cc_ir_datasize data_size)
{
    cc_ir_ins ins = {0};
    ins.opcode = opcode;
    ins.data_size = data_size;
    return &block->ins[cc_ir_block_append(block, &ins)];
}

void cc_ir_block_argp(cc_ir_block* block)                           { cc__ir_block_append_noop(block, CC_IR_OPCODE_ARGP); }
void cc_ir_block_addrl(cc_ir_block* block, cc_ir_localid localid)   { cc__ir_block_append_localop(block, CC_IR_OPCODE_ADDRL, localid); }
void cc_ir_block_sizel(cc_ir_block* block, cc_ir_datasize data_size, cc_ir_localid localid) {
    cc__ir_block_append_localop(block, CC_IR_OPCODE_SIZEL, localid)->data_size = data_size;
}
void cc_ir_block_loadl(cc_ir_block* block, cc_ir_localid localid) { cc__ir_block_append_localop(block, CC_IR_OPCODE_LOADL, localid); }
void cc_ir_block_addrg(cc_ir_block* block, cc_ir_localid localid) { cc__ir_block_append_localop(block, CC_IR_OPCODE_ADDRG, localid); }
void cc_ir_block_iconst(cc_ir_block* block, cc_ir_datasize data_size, int32_t value) {
    cc__ir_block_append_u32op(block, CC_IR_OPCODE_ICONST, (uint32_t)value)->data_size = data_size;
}
void cc_ir_block_uconst(cc_ir_block* block, cc_ir_datasize data_size, uint32_t value) {
    cc__ir_block_append_u32op(block, CC_IR_OPCODE_UCONST, value)->data_size = data_size;
}
void cc_ir_block_load(cc_ir_block* block, cc_ir_datasize data_size)     { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_LOAD, data_size); }
void cc_ir_block_store(cc_ir_block* block, cc_ir_datasize data_size)    { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_STORE, data_size); }
void cc_ir_block_dupe(cc_ir_block* block, cc_ir_datasize data_size)     { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_DUPE, data_size); }
void cc_ir_block_add(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_ADD, data_size); }
void cc_ir_block_sub(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_SUB, data_size); }
void cc_ir_block_mul(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_MUL, data_size); }
void cc_ir_block_umul(cc_ir_block* block, cc_ir_datasize data_size)     { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_UMUL, data_size); }
void cc_ir_block_div(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_DIV, data_size); }
void cc_ir_block_udiv(cc_ir_block* block, cc_ir_datasize data_size)     { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_UDIV, data_size); }
void cc_ir_block_mod(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_MOD, data_size); }
void cc_ir_block_umod(cc_ir_block* block, cc_ir_datasize data_size)     { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_UMOD, data_size); }
void cc_ir_block_neg(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_NEG, data_size); }
void cc_ir_block_not(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_NOT, data_size); }
void cc_ir_block_and(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_AND, data_size); }
void cc_ir_block_or(cc_ir_block* block, cc_ir_datasize data_size)       { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_OR, data_size); }
void cc_ir_block_xor(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_XOR, data_size); }
void cc_ir_block_lsh(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_LSH, data_size); }
void cc_ir_block_rsh(cc_ir_block* block, cc_ir_datasize data_size)      { cc__ir_block_append_sizeop(block, CC_IR_OPCODE_RSH, data_size); }
void cc_ir_block_zext(cc_ir_block* block, cc_ir_datasize data_size, cc_ir_datasize extend_data_size) {
    cc__ir_block_append_sizeop(block, CC_IR_OPCODE_ZEXT, data_size)->operand.extend_data_size = extend_data_size;
}
void cc_ir_block_sext(cc_ir_block* block, cc_ir_datasize data_size, cc_ir_datasize extend_data_size) {
    cc__ir_block_append_sizeop(block, CC_IR_OPCODE_SEXT, data_size)->operand.extend_data_size = extend_data_size;
}
void cc_ir_block_call(cc_ir_block* block)   { cc__ir_block_append_noop(block, CC_IR_OPCODE_CALL); }
void cc_ir_block_jmp(cc_ir_block* block)    { cc__ir_block_append_noop(block, CC_IR_OPCODE_JMP); }
void cc_ir_block_jnz(cc_ir_block* block, cc_ir_datasize data_size, const cc_ir_block* dst) {
    cc__ir_block_append_sizeop(block, CC_IR_OPCODE_JNZ, data_size)->operand.blockid = dst->blockid;
}
void cc_ir_block_ret(cc_ir_block* block) { cc__ir_block_append_noop(block, CC_IR_OPCODE_RET); }
void cc_ir_block_int(cc_ir_block* block, uint32_t interrupt_code) { cc__ir_block_append_u32op(block, CC_IR_OPCODE_INT, interrupt_code); }
