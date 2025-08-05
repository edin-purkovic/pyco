#ifndef PYCO_COMPILER_H
#define PYCO_COMPILER_H

#define PYCO_NULL 0

typedef unsigned char pyco_uint8;
typedef unsigned short pyco_uint16;
typedef unsigned int pyco_uint32;
typedef unsigned long long pyco_uint64;
typedef unsigned long long pyco_flags;

typedef void *(*PYCO_FUNC_MALLOC)(size_t);
typedef void *(*PYCO_FUNC_REALLOC)(void *, size_t);
typedef void (*PYCO_FUNC_FREE)(void *);

typedef struct pyco_allocators
{
    PYCO_FUNC_MALLOC malloc;
    PYCO_FUNC_REALLOC realloc;
    PYCO_FUNC_FREE free;
} pyco_allocators;

typedef struct pyco_compile_options
{
    pyco_allocators allocators;
    pyco_uint32 copy_buffer;
    pyco_uint32 indent_based;
} pyco_compile_options;

typedef struct pyco_compiled_program
{
    pyco_uint8 *data;
    pyco_uint64 size;
    pyco_uint32 valid;
    pyco_uint32 errors;

    pyco_compile_options compile_options;
} pyco_compiled_program;

pyco_compile_options pyco_initialize_compile_options();

pyco_compiled_program pyco_compile(const pyco_uint8 *data, pyco_uint64 size, pyco_compile_options options);

void pyco_free_compiled_program(pyco_compiled_program *program);

#endif