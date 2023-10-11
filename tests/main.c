#include <stdio.h>
#include "test.h"

static void run_test(const char* name, int(*test)(void)) {
    printf("Testing %s ...\n", name);
    if (test())
        printf("%s succeeded\n", name);
    else
        printf("%s failed\n", name);
}

int main(int argc, char** argv)
{
    run_test("test_expr", &test_expr);
    run_test("test_stmt", &test_stmt);
    run_test("test_function", &test_function);
    run_test("test_block", &test_block);
    run_test("test_x86", &test_x86);

    printf("Program completed with no crashes");
    return 0;
}