SET(CC_SOURCE_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ast.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ir.c
    ${CMAKE_CURRENT_SOURCE_DIR}/x86_asm.c
    CACHE INTERNAL "cc library source files"
)

SET(CC_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    CACHE INTERNAL "cc library include directory"
)