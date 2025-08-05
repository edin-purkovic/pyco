#include <stdio.h>
#include <stdlib.h>
#include "pyco_compiler.h"

int main()
{
    // debugging the work done so far
    printf("testing lexer - starting\n");

    char *input1 = "\
        a := 55\n\
        b := 22\n\
        c := a + b * 2 * (1 + 3)\n\
        d := 5 + test(6 + 1, 2) * 1\n\
        increment :: function(a) => a + 1 \n\
        point :: struct {\n\
            x int32\n\
            y int32\n\
        }\n\
        circle :: struct { p point; r f32 }\n\
        \n";

    char *input2 = "e := 0 + test(1 + 2, func(3), 1) + x(_y) * 5";
    char *input3 = "\n\
        \n\
        circle :: struct { p point; r f32 }\n\
        increment :: function(a x) {a + 1} \n\
        x2 := 2\n\
        // test(1+2)\n\
        ";

    char *input4 = "circle :: struct { p point; r f32 }";

    char *input5 = "\n\
        \n\
        increment :: function() { \n\
            circle :: struct { p point; r f32 }\n\
            x2 := 2\n\
            test(1+2)\n\
        }\n\
        y2 := 2\n\
        ";

    char *input6 = "\n\
        \n\
        a :: function() { \n\
            b :: function() {\n\
                x2 := 2\n\
            }\n\
            y2 := b(1 + 2 + 3)\n\
        }\n\
        ";

    char *input7 = "\n\
        \n\
        if a > b {\n\
            x := a\n\
        }\n\
        z := 1\n\
        ";

    char *input8 = "\n\
        \n\
        do {\n\
            x := a\n\
        } while 2 > 3\n\
        z := 1\n\
        ";

    char *input9 = "\n\
        \n\
        for i := 0; i < 10; i++ {\n\
            for j := 0; j < 10; j++ {\n\
                grid[i][j] = 0\n\
            }\n\
        }\n\
        z := 1\n\
        ";

    pyco_compile_options compile_options = pyco_initialize_compile_options();

    compile_options.allocators.malloc = malloc;
    compile_options.allocators.realloc = realloc;
    compile_options.allocators.free = free;

    pyco_compile(input9, strlen(input9), compile_options);

    return 0;
}