# Building and linking

`src/CMakeLists.txt` provides two variables:
- `${CC_SOURCE_LIST}`
- `${CC_INCLUDE_DIR}`

You may add them to your executable like so:
```cmake
add_subdirectory(path/to/src)
target_sources(my-executable PRIVATE ${CC_SOURCE_LIST})
target_include_directories(my-executable PRIVATE ${CC_INCLUDE_DIR})
```

Alternatively, add all files in `src/*` and include `src/include`.

# Quickstart

First, turn some source code into an array of tokens:

```c
    static const char* source_code =
    "int example(const char* text, int index) {"
    "   return *(text + index) != 0;"
    "}";

    cc_token* tokens;
    size_t num_tokens;

    if (!cc_lexer_readall(source_code, NULL, &tokens, &num_tokens)) {
        printf("Unrecognized token in source_code\n");
        free(tokens);
        return;
    }
```

Next, create an AST with those tokens:

```c
    cc_parser parser;
    cc_ast_decl decl; // AST node
    int parse_result;

    cc_parser_create(&parser, tokens, tokens + num_token);
    parse_result = cc_parser_parse_decl(&parser, &decl);

    if (parse_result) {
        // Do something with `decl`...
    }
```

Finally, free all the data after it has been used:

```c
    cc_parser_destroy(&parser); // Every 'create' function has a 'destroy'
    free(tokens); // Free after use, according to function doc
```

For the complete example, see [examples/function_decl.c](examples/function_decl.c)