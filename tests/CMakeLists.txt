add_executable(tests
    ${CC_SOURCE_LIST}
    main.c
    helper.c
    test_function.c
    test_stmt.c
)
target_include_directories(tests PRIVATE ${CC_INCLUDE_DIR})