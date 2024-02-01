add_executable(tests
    ${CC_SOURCE_LIST}
    main.c
    helper.c
    test_expr.c
    test_stmt.c
    test_function.c
    test_block.c
    test_x86asm.c
    test_x86gen.c
    test_hmap.c
    test_vm.c
)
target_include_directories(tests PRIVATE ${CC_INCLUDE_DIR})