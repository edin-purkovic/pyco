#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "pyco_compiler.h"

// MARK: MISC

enum PYCO_TOKEN_FLAG
{
    PYCO_TOKEN_FLAG_SUCCESSIVE = (1 << 3),
};

enum PYCO_TOKEN_TYPE
{
    PYCO_TOKEN_TYPE_IDENTIFIER = (1 << 5),

    PYCO_TOKEN_TYPE_NUMBER = (1 << 8),
    PYCO_TOKEN_TYPE_INTEGER = (1 << 9),
    PYCO_TOKEN_TYPE_FLOAT = (1 << 10),
    PYCO_TOKEN_TYPE_DOUBLE = (1 << 11),

    PYCO_TOKEN_TYPE_INDENT = (1 << 17),
    PYCO_TOKEN_TYPE_INDENT_SPACE = (1 << 18),
    PYCO_TOKEN_TYPE_INDENT_TAB = (1 << 19),

    PYCO_TOKEN_TYPE_STRING = (1 << 20),
    PYCO_TOKEN_TYPE_STRING_TEMPLATE_LITERAL = (1 << 21),

    PYCO_TOKEN_TYPE_SPECIAL = (1 << 22),

    PYCO_TOKEN_TYPE_ERROR = (1 << 25),
    PYCO_TOKEN_TYPE_ERROR_INCOMPLETE = (1 << 26),
    PYCO_TOKEN_TYPE_ERROR_MALFORMED = (1 << 26),
};

enum PYCO_AST_NODE_TYPE
{
    PYCO_AST_NODE_TYPE_NONE = 0,
    PYCO_AST_NODE_TYPE_UNKNOWN,
    PYCO_AST_NODE_TYPE_ROOT,
    PYCO_AST_NODE_TYPE_LITERAL,
    PYCO_AST_NODE_TYPE_STRUCT,
    PYCO_AST_NODE_TYPE_STRUCT_FIELD,
    PYCO_AST_NODE_TYPE_FUNCTION,
    PYCO_AST_NODE_TYPE_ARGUMENTS,
    PYCO_AST_NODE_TYPE_STATEMENT,
    PYCO_AST_NODE_TYPE_EXPRESSION,
    PYCO_AST_NODE_TYPE_CALL,
    PYCO_AST_NODE_TYPE_IF,
    PYCO_AST_NODE_TYPE_FOR,
    PYCO_AST_NODE_TYPE_FOR_IN,
    PYCO_AST_NODE_TYPE_WHILE,
    PYCO_AST_NODE_TYPE_DO_WHILE,
    PYCO_AST_NODE_TYPE_CONTINUE,
    PYCO_AST_NODE_TYPE_BREAK,
    PYCO_AST_NODE_TYPE_SCOPE,
};

const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_NONE = "NONE";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_UNKNOWN = "UNKNOWN";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_ROOT = "ROOT";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_LITERAL = "LITERAL";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_STRUCT = "STRUCT";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_STRUCT_FIELD = "STRUCT_FIELD";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_FUNCTION = "FUNCTION";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_ARGUMENTS = "ARGUMENTS";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_STATEMENT = "STATEMENT";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_EXPRESSION = "EXPRESSION";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_CALL = "CALL";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_IF = "IF";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_FOR = "FOR";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_FOR_IN = "FOR_IN";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_WHILE = "WHILE";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_DO_WHILE = "DO_WHILE";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_CONTINUE = "CONTINUE";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_BREAK = "BREAK";
const pyco_uint8 *PYCO_AST_NODE_TYPE_NAME_SCOPE = "SCOPE";

// MARK: BUFFER READER
typedef struct pyco_buffer
{
    pyco_uint8 *data;
    pyco_uint64 size;
} pyco_buffer;

typedef struct pyco_buffer_reader
{
    const pyco_buffer *buffer;
    pyco_uint8 *data_pointer;
    pyco_uint64 offset;
} pyco_buffer_reader;

pyco_buffer_reader create_buffer_reader(const pyco_buffer *buffer)
{
    return (pyco_buffer_reader){
        .buffer = buffer,
        .offset = 0,
        .data_pointer = buffer->data,
    };
}

inline bool is_buffer_reader_valid(pyco_buffer_reader *reader)
{
    return reader->buffer && reader->data_pointer && reader->buffer->size;
}

inline char buffer_reader_current_char(pyco_buffer_reader *reader)
{
    return reader->data_pointer[reader->offset];
}

inline char buffer_reader_peek_next_char(pyco_buffer_reader *reader)
{
    if (reader->offset >= reader->buffer->size)
    {
        return '\0';
    }

    return reader->data_pointer[reader->offset + 1];
}

inline char buffer_reader_next_char(pyco_buffer_reader *reader)
{
    if (reader->offset >= reader->buffer->size)
    {
        return '\0';
    }

    return reader->data_pointer[reader->offset++];
}

inline bool buffer_reader_next_char_valid(pyco_buffer_reader *reader)
{
    return (reader->offset + 1) < reader->buffer->size;
}

pyco_uint64 buffer_reader_get_position(pyco_buffer_reader *reader)
{
    return reader->offset;
}

const pyco_uint8 *buffer_reader_get_data_pointer(pyco_buffer_reader *reader, pyco_uint64 offset)
{
    if (offset >= reader->buffer->size)
    {
        return 0;
    }

    return reader->data_pointer + offset;
}

// MARK: TOKENIZER

typedef struct token_location
{
    pyco_uint64 line;
    pyco_uint64 column;
    pyco_uint64 offset;
} token_location;

typedef struct pyco_token
{
    pyco_flags flags;
    pyco_uint64 length;
    char *value;

    token_location start;
    token_location end;

    struct pyco_token *next;
} pyco_token;

typedef struct pyco_lexer_options
{
    pyco_uint64 token_block_initial_size;
    pyco_uint64 token_block_increment_size;
    pyco_uint64 token_buffer_block_initial_size;
    pyco_uint64 token_buffer_block_increment_size;
    pyco_allocators allocators;
} pyco_lexer_options;

typedef struct pyco_lexer
{
    pyco_lexer_options options;
    pyco_uint64 token_buffer_allocated;
    pyco_uint64 token_buffer_offset;
    pyco_uint8 *token_buffer_data;
    pyco_token *tokens;
    pyco_token *current_token;
    pyco_uint64 tokens_allocated;
    pyco_uint64 tokens_count;
    pyco_uint64 current_line;
    pyco_uint64 last_newline;
    pyco_uint8 track_indents;
} pyco_lexer;

void _lexer_add_token(pyco_lexer *lexer, const pyco_uint8 *source_token, token_location start, token_location end, pyco_uint64 length, pyco_flags type)
{
    const pyco_uint64 null_terminator_length = 1;

    if (lexer->token_buffer_offset + length + null_terminator_length > lexer->token_buffer_allocated)
    {
        lexer->token_buffer_allocated = lexer->token_buffer_allocated + lexer->options.token_buffer_block_increment_size;
        lexer->token_buffer_data = lexer->options.allocators.realloc(lexer->token_buffer_data, lexer->token_buffer_allocated);
    }

    pyco_uint8 *token_value = &lexer->token_buffer_data[lexer->token_buffer_offset];
    for (pyco_uint64 byte_index = length; byte_index--;)
    {
        token_value[byte_index] = source_token[byte_index];
    }
    token_value[length] = '\0';
    lexer->token_buffer_offset += length + null_terminator_length;

    if (lexer->tokens_count + length > lexer->tokens_allocated)
    {
        lexer->tokens_count = lexer->tokens_allocated + lexer->options.token_block_increment_size;
        lexer->tokens = lexer->options.allocators.realloc(lexer->tokens, lexer->tokens_count);
    }

    pyco_token *token = &lexer->tokens[lexer->tokens_count];
    token->flags = type;
    token->length = length;
    token->value = token_value;
    token->start = start;
    token->end = end;
    token->next = PYCO_NULL;
    // token->column = start.offset - lexer->last_newline + 1;

    if (lexer->tokens_count)
    {
        pyco_token *previous_token = &lexer->tokens[lexer->tokens_count - 1];

        previous_token->next = token;

        if ((~previous_token->flags & PYCO_TOKEN_TYPE_INDENT) && (token->start.offset == previous_token->end.offset + 1))
        {
            token->flags |= PYCO_TOKEN_FLAG_SUCCESSIVE;
        }
    }

    lexer->tokens_count++;
}

inline bool is_number(char ch, bool include_dot /*= false*/, const bool include_hex /*= false*/)
{
    return (ch >= '0' && ch <= '9') || (include_dot && ch == '.') || (include_hex && ch == 'x');
}

inline bool is_special(char ch, char exclude /*= '\0'*/)
{
    return (exclude == ch) || (ch == '_') ? false : (ch >= '!' && ch <= '/') || (ch >= ':' && ch <= '@') || (ch >= '[' && ch <= '`') || (ch >= '{' && ch <= '~');
}

inline bool is_newline(char ch)
{
    return ch == '\n' || ch == '\r';
}

inline bool is_whitespace(char ch)
{
    return ch == ' ' || ch == '\t';
}

void _lexer_handle_newlines(pyco_lexer *lexer, pyco_buffer_reader *reader)
{
    if (buffer_reader_current_char(reader) == '\r')
    {
        if (buffer_reader_peek_next_char(reader) == '\n')
        {
            buffer_reader_next_char(reader);
        }
    }

    lexer->last_newline = buffer_reader_get_position(reader) + 1;
    lexer->current_line++;
    lexer->track_indents = 1;
}

void _lexer_handle_indents(pyco_lexer *lexer, pyco_buffer_reader *reader)
{
    const char current_char = buffer_reader_current_char(reader);

    token_location token_start = {
        .offset = buffer_reader_get_position(reader),
        .line = lexer->current_line,
        .column = 1,
    };

    const pyco_uint8 *token_pointer = buffer_reader_get_data_pointer(reader, token_start.offset);

    pyco_flags token_type = PYCO_TOKEN_TYPE_INDENT | PYCO_TOKEN_TYPE_INDENT_SPACE;
    pyco_uint32 token_length = 1;

    if (current_char == '\t')
    {
        token_type = PYCO_TOKEN_TYPE_INDENT | PYCO_TOKEN_TYPE_INDENT_TAB;
    }

    while (buffer_reader_peek_next_char(reader) == current_char)
    {
        token_length++;
        buffer_reader_next_char(reader);
    }

    if (is_newline(buffer_reader_current_char(reader)))
    {
        return;
    }

    token_location token_end = {
        .line = lexer->current_line,
        .column = buffer_reader_get_position(reader) - lexer->last_newline + 1,
        .offset = buffer_reader_get_position(reader),
    };

    _lexer_add_token(lexer, token_pointer, token_start, token_end, token_length, token_type);
}

void _lexer_handle_whitespaces(pyco_lexer *lexer, pyco_buffer_reader *reader)
{
    if (lexer->track_indents)
    {
        _lexer_handle_indents(lexer, reader);
    }
}

inline token_location _initialize_token_location(pyco_lexer *lexer, pyco_buffer_reader *reader)
{
    return (token_location){
        .line = lexer->current_line,
        .column = buffer_reader_get_position(reader) - lexer->last_newline + 1,
        .offset = buffer_reader_get_position(reader),
    };
}

void _lexer_handle_numbers(pyco_lexer *lexer, pyco_buffer_reader *reader)
{
    token_location token_start = _initialize_token_location(lexer, reader);

    const pyco_uint8 *token_pointer = buffer_reader_get_data_pointer(reader, token_start.offset);

    pyco_flags token_type = PYCO_TOKEN_TYPE_NUMBER | PYCO_TOKEN_TYPE_INTEGER;
    pyco_uint32 token_length = 1;

    while (buffer_reader_next_char_valid(reader))
    {
        char next_char = buffer_reader_peek_next_char(reader);

        if (!is_number(next_char, false, false) && next_char != '.')
        {
            token_type |= PYCO_TOKEN_TYPE_ERROR_MALFORMED;
        }

        if (is_whitespace(next_char) ||
            is_newline(next_char) ||
            is_special(next_char, '.'))
        {
            break;
        }

        if (next_char == '.')
        {
            if (token_type & PYCO_TOKEN_TYPE_DOUBLE)
            {
                token_type |= PYCO_TOKEN_TYPE_ERROR_MALFORMED;
            }

            token_type = PYCO_TOKEN_TYPE_NUMBER | PYCO_TOKEN_TYPE_DOUBLE;
        }

        token_length++;

        buffer_reader_next_char(reader);
    }

    if (buffer_reader_peek_next_char(reader) == 'f' && token_type & PYCO_TOKEN_TYPE_DOUBLE)
    {
        token_type = PYCO_TOKEN_TYPE_NUMBER | PYCO_TOKEN_TYPE_FLOAT;
        token_length++;

        buffer_reader_next_char(reader);
    }

    lexer->track_indents = 0;

    token_location token_end = _initialize_token_location(lexer, reader);

    _lexer_add_token(lexer, token_pointer, token_start, token_end, token_length, token_type);
}

void _lexer_handle_identifiers(pyco_lexer *lexer, pyco_buffer_reader *reader)
{
    token_location token_start = _initialize_token_location(lexer, reader);

    const pyco_uint8 *token_pointer = buffer_reader_get_data_pointer(reader, token_start.offset);

    pyco_uint32 token_length = 1;

    while (buffer_reader_next_char_valid(reader))
    {
        char next_char = buffer_reader_peek_next_char(reader);

        if (is_whitespace(next_char) || is_newline(next_char) || (next_char != '_' && is_special(next_char, '\0')))
        {
            break;
        }

        token_length++;

        buffer_reader_next_char(reader);
    }

    lexer->track_indents = 0;

    token_location token_end = _initialize_token_location(lexer, reader);

    _lexer_add_token(lexer, token_pointer, token_start, token_end, token_length, PYCO_TOKEN_TYPE_IDENTIFIER);
}

void _lexer_handle_strings(pyco_lexer *lexer, pyco_buffer_reader *reader)
{
    token_location token_start = _initialize_token_location(lexer, reader);

    const pyco_uint8 *token_pointer = buffer_reader_get_data_pointer(reader, token_start.offset);

    char bracket_type = buffer_reader_current_char(reader);

    pyco_flags token_type = PYCO_TOKEN_TYPE_STRING;
    pyco_uint32 token_length = 0;

    if (bracket_type == '`')
    {
        token_type |= PYCO_TOKEN_TYPE_STRING_TEMPLATE_LITERAL;
    }

    while (buffer_reader_next_char_valid(reader))
    {
        char next_char = buffer_reader_peek_next_char(reader);

        if (is_newline(next_char))
        {
            if ((token_type & PYCO_TOKEN_TYPE_STRING_TEMPLATE_LITERAL) == 0)
            {
                token_type |= PYCO_TOKEN_TYPE_ERROR;
                token_type |= PYCO_TOKEN_TYPE_ERROR_INCOMPLETE;
                break;
            }

            lexer->current_line++;
        }

        if (next_char == bracket_type)
        {
            buffer_reader_next_char(reader);
            break;
        }

        token_length++;

        buffer_reader_next_char(reader);
    }

    lexer->track_indents = 0;

    token_location token_end = _initialize_token_location(lexer, reader);

    _lexer_add_token(lexer, token_pointer, token_start, token_end, token_length, token_type);
}

void _lexer_handle_specials(pyco_lexer *lexer, pyco_buffer_reader *reader)
{
    token_location token_start = _initialize_token_location(lexer, reader);

    const pyco_uint8 *token_pointer = buffer_reader_get_data_pointer(reader, token_start.offset);

    pyco_uint32 token_length = 1;

    switch (buffer_reader_current_char(reader))
    {
    case '"':
    case '`':
        _lexer_handle_strings(lexer, reader);
        return;
    }

    lexer->track_indents = 0;

    token_location token_end = _initialize_token_location(lexer, reader);

    _lexer_add_token(lexer, token_pointer, token_start, token_end, token_length, PYCO_TOKEN_TYPE_SPECIAL);
}

pyco_lexer_options lexer_initialize_options()
{
    return (pyco_lexer_options){
        .token_block_initial_size = 1000,
        .token_block_increment_size = 1000,
        .token_buffer_block_initial_size = 2000,
        .token_buffer_block_increment_size = 2000,
        .allocators = {
            .malloc = PYCO_NULL,
            .realloc = PYCO_NULL,
            .free = PYCO_NULL,
        }};
}

pyco_lexer *lexer_create(pyco_lexer_options options)
{
    if (!options.allocators.malloc || !options.allocators.realloc || !options.allocators.free)
    {
        return PYCO_NULL;
    }

    if (!options.token_block_initial_size && !options.token_block_increment_size)
    {
        return PYCO_NULL;
    }

    pyco_lexer *lexer = options.allocators.malloc(sizeof(pyco_lexer));
    lexer->options = options;
    lexer->current_line = 1;
    lexer->last_newline = 0;
    lexer->track_indents = 1;
    lexer->tokens = 0;

    lexer->tokens_count = 0;
    lexer->tokens_allocated = options.token_block_initial_size;
    if (lexer->tokens_allocated)
    {
        lexer->tokens = options.allocators.malloc(sizeof(pyco_token) * lexer->tokens_allocated);
    }

    lexer->token_buffer_offset = 0;
    lexer->token_buffer_allocated = options.token_buffer_block_initial_size;
    if (lexer->token_buffer_allocated)
    {
        lexer->token_buffer_data = options.allocators.malloc(lexer->token_buffer_allocated);
    }

    lexer->current_token = lexer->tokens;

    return lexer;
}

bool lexer_free(pyco_lexer *lexer)
{
    if (lexer && lexer->options.allocators.free)
    {
        if (lexer->token_buffer_data)
        {
            lexer->options.allocators.free(lexer->token_buffer_data);
        }

        if (lexer->tokens)
        {
            lexer->options.allocators.free(lexer->tokens);
        }

        lexer->options.allocators.free(lexer);

        return true;
    }

    return false;
}

bool lexer_process_buffer(pyco_lexer *lexer, const pyco_buffer *buffer)
{
    pyco_buffer_reader reader = create_buffer_reader(buffer);

    if (!is_buffer_reader_valid(&reader))
    {
        return false;
    }

    do
    {
        char ch = buffer_reader_current_char(&reader);

        if (is_newline(ch))
        {
            _lexer_handle_newlines(lexer, &reader);
            continue;
        }

        if (is_whitespace(ch))
        {
            _lexer_handle_whitespaces(lexer, &reader);
            continue;
        }

        if (is_number(ch, false, false))
        {
            _lexer_handle_numbers(lexer, &reader);
            continue;
        }

        if (is_special(ch, '\0'))
        {
            _lexer_handle_specials(lexer, &reader);
            continue;
        }

        _lexer_handle_identifiers(lexer, &reader);
    } while (buffer_reader_next_char_valid(&reader) && buffer_reader_next_char(&reader));

    return true;
}

const pyco_token *lexer_get_current_token(pyco_lexer *lexer)
{
    return lexer->current_token;
}

const pyco_token *lexer_get_next_token(pyco_lexer *lexer)
{
    if (lexer->current_token)
    {
        return lexer->current_token = lexer->current_token->next;
    }

    return PYCO_NULL;
}

const pyco_token *lexer_peek_next_token(pyco_lexer *lexer)
{
    if (lexer->current_token)
    {
        return lexer->current_token->next;
    }

    return PYCO_NULL;
}

// MARK: AST TREE BUILDER

typedef struct pyco_ast_node
{
    struct pyco_ast_node *parent;
    struct pyco_ast_node *child_first;
    struct pyco_ast_node *child_last;
    struct pyco_ast_node *next;
    const char *name;
    pyco_uint32 type;
    pyco_uint32 flags;
    void *data;
} pyco_ast_node;

typedef struct pyco_ast_options
{
    pyco_allocators allocators;
    pyco_uint64 buffer_initial_size;
    pyco_uint64 buffer_increment_size;
} pyco_ast_options;

typedef struct pyco_ast
{
    pyco_ast_options options;
    pyco_ast_node *root_node;
    pyco_uint64 buffer_allocated;
    pyco_uint64 buffer_offset;
    pyco_uint8 *buffer_data;
} pyco_ast;

pyco_ast_node *pyco_ast_node_create(pyco_ast *ast, const char *name, pyco_uint32 type, pyco_uint32 flags, pyco_uint64 data_size);

pyco_ast initialize_tree(pyco_ast_options options)
{
    pyco_ast tree = {
        .options = options,
    };

    tree.buffer_data = options.allocators.malloc(options.buffer_initial_size);
    tree.buffer_allocated = options.buffer_initial_size;
    tree.buffer_offset = 0;

    tree.root_node = pyco_ast_node_create(&tree, PYCO_NULL, PYCO_AST_NODE_TYPE_ROOT, PYCO_NULL, 0);

    return tree;
}

pyco_ast_node *pyco_ast_node_create(pyco_ast *ast, const char *name, pyco_uint32 type, pyco_uint32 flags, pyco_uint64 data_size)
{
    pyco_uint64 node_size = sizeof(pyco_ast_node) + data_size;

    if ((ast->buffer_offset + node_size) > ast->buffer_allocated)
    {
        pyco_uint64 new_buffer_size = ast->buffer_allocated + ast->options.buffer_increment_size;
        ast->buffer_data = ast->options.allocators.realloc(ast->buffer_data, new_buffer_size);
        ast->buffer_allocated = new_buffer_size;
    }

    pyco_uint8 *offset = ast->buffer_data + ast->buffer_offset;

    pyco_ast_node *node = (pyco_ast_node *)offset;
    node->data = offset + sizeof(pyco_ast_node);
    node->name = name;
    node->type = type;
    node->flags = flags;
    node->parent = PYCO_NULL;
    node->child_first = PYCO_NULL;
    node->child_last = PYCO_NULL;
    node->next = PYCO_NULL;

    ast->buffer_offset += node_size;

    return node;
}

inline pyco_ast_node *pyco_ast_node_append(pyco_ast_node *root_node, pyco_ast_node *node_to_append)
{
    if (node_to_append == PYCO_NULL)
    {
        return root_node;
    }

    if (root_node->child_first == PYCO_NULL)
    {
        root_node->child_first = node_to_append;
        root_node->child_last = node_to_append;
    }
    else
    {
        root_node->child_last->next = node_to_append;
        root_node->child_last = node_to_append;
    }

    node_to_append->parent = root_node;

    return node_to_append;
}

pyco_ast_node *pyco_ast_node_add(pyco_ast *ast, pyco_ast_node *root_node, const char *name, pyco_uint32 type, pyco_uint32 flags, pyco_uint64 data_size)
{
    return pyco_ast_node_append(root_node, pyco_ast_node_create(ast, name, type, flags, data_size));
}

void pyco_ast_free(pyco_ast *ast, pyco_ast_node *root_node)
{
    ast->options.allocators.free(ast->buffer_data);
    ast->buffer_allocated = 0;
    ast->buffer_offset = 0;
    ast->buffer_data = PYCO_NULL;
}

// MARK: NODE DATA STRUCTS

enum PYCO_VAR_TYPE
{
    PYCO_VAR_TYPE_INT8,
    PYCO_VAR_TYPE_INT16,
    PYCO_VAR_TYPE_INT32,
    PYCO_VAR_TYPE_INT64,
    PYCO_VAR_TYPE_UINT8,
    PYCO_VAR_TYPE_UINT16,
    PYCO_VAR_TYPE_UINT32,
    PYCO_VAR_TYPE_UINT64,
    PYCO_VAR_TYPE_F32,
    PYCO_VAR_TYPE_F64,
    PYCO_VAR_TYPE_BYTE,
    PYCO_VAR_TYPE_RUNE,
    PYCO_VAR_TYPE_STRING,
    PYCO_VAR_TYPE_ARRAY,
    PYCO_VAR_TYPE_MAP,
};

typedef struct ast_data_struct
{
    pyco_uint32 flags;
} ast_data_struct;

typedef struct ast_data_struct_field
{
    pyco_uint32 type;
    pyco_uint32 flags;
} ast_data_struct_field;

// MARK: AST TREE PRINTER

inline void _pyco_ast_node_print_json_indent(FILE *file, pyco_uint32 indent)
{
    for (pyco_uint32 i = 0; i < indent; i++)
    {
        fprintf(file, "    ");
    }
}

const char *_pyco_ast_node_get_node_type(pyco_uint32 type)
{
    switch (type)
    {
    case PYCO_AST_NODE_TYPE_NONE:
        return PYCO_AST_NODE_TYPE_NAME_NONE;
    case PYCO_AST_NODE_TYPE_ROOT:
        return PYCO_AST_NODE_TYPE_NAME_ROOT;
    case PYCO_AST_NODE_TYPE_LITERAL:
        return PYCO_AST_NODE_TYPE_NAME_LITERAL;
    case PYCO_AST_NODE_TYPE_STRUCT:
        return PYCO_AST_NODE_TYPE_NAME_STRUCT;
    case PYCO_AST_NODE_TYPE_STRUCT_FIELD:
        return PYCO_AST_NODE_TYPE_NAME_STRUCT_FIELD;
    case PYCO_AST_NODE_TYPE_FUNCTION:
        return PYCO_AST_NODE_TYPE_NAME_FUNCTION;
    case PYCO_AST_NODE_TYPE_ARGUMENTS:
        return PYCO_AST_NODE_TYPE_NAME_ARGUMENTS;
    case PYCO_AST_NODE_TYPE_STATEMENT:
        return PYCO_AST_NODE_TYPE_NAME_STATEMENT;
    case PYCO_AST_NODE_TYPE_EXPRESSION:
        return PYCO_AST_NODE_TYPE_NAME_EXPRESSION;
    case PYCO_AST_NODE_TYPE_CALL:
        return PYCO_AST_NODE_TYPE_NAME_CALL;
    case PYCO_AST_NODE_TYPE_IF:
        return PYCO_AST_NODE_TYPE_NAME_IF;
    case PYCO_AST_NODE_TYPE_FOR:
        return PYCO_AST_NODE_TYPE_NAME_FOR;
    case PYCO_AST_NODE_TYPE_WHILE:
        return PYCO_AST_NODE_TYPE_NAME_WHILE;
    case PYCO_AST_NODE_TYPE_DO_WHILE:
        return PYCO_AST_NODE_TYPE_NAME_DO_WHILE;
    case PYCO_AST_NODE_TYPE_CONTINUE:
        return PYCO_AST_NODE_TYPE_NAME_CONTINUE;
    case PYCO_AST_NODE_TYPE_BREAK:
        return PYCO_AST_NODE_TYPE_NAME_BREAK;
    case PYCO_AST_NODE_TYPE_SCOPE:
        return PYCO_AST_NODE_TYPE_NAME_SCOPE;
    }

    return PYCO_AST_NODE_TYPE_NAME_UNKNOWN;
}

void pyco_ast_node_print_json(FILE *file, pyco_ast_node *node, pyco_uint32 indent)
{
    _pyco_ast_node_print_json_indent(file, indent);
    fprintf(file, "{\n");

    _pyco_ast_node_print_json_indent(file, indent + 1);
    fprintf(file, "type: \"%s\",\n", _pyco_ast_node_get_node_type(node->type));

    if (node->name)
    {
        _pyco_ast_node_print_json_indent(file, indent + 1);
        fprintf(file, "name: \"%s\",\n", node->name);
    }

    if (node->child_first)
    {
        _pyco_ast_node_print_json_indent(file, indent + 1);
        fprintf(file, "children: [\n");

        pyco_ast_node_print_json(file, node->child_first, indent + 2);

        _pyco_ast_node_print_json_indent(file, indent + 1);
        fprintf(file, "]\n");
    }

    _pyco_ast_node_print_json_indent(file, indent);

    if (node->next)
    {
        fprintf(file, "},\n");
        pyco_ast_node_print_json(file, node->next, indent);
    }
    else
    {
        fprintf(file, "}\n");
    }
}

bool pyco_ast_node_to_json_file(const char *filename, pyco_ast_node *root_node, const char *extra)
{
    if (root_node == PYCO_NULL)
    {
        return false;
    }

    FILE *file = fopen(filename, "w");

    if (file == NULL)
    {
        return false;
    }

    if (extra)
    {
        fprintf(file, "/*\n%s\n*/\n\n", extra);
    }

    fprintf(file, "export const AST = ");

    pyco_ast_node_print_json(file, root_node, 0);

    fclose(file);

    return true;
}

// MARK: PARSER

enum PYCO_OPERATOR
{
    PYCO_OPERATOR_NONE = PYCO_NULL,
    PYCO_OPERATOR_COMPOSITE = (1 << 0),
    PYCO_OPERATOR_ADD = (1 << 1),
    PYCO_OPERATOR_SUBTRACT = (1 << 2),
    PYCO_OPERATOR_MULTIPLY = (1 << 3),
    PYCO_OPERATOR_DIVIDE = (1 << 4),
    PYCO_OPERATOR_INCREMENT = (1 << 5),
    PYCO_OPERATOR_DECREMENT = (1 << 6),
    PYCO_OPERATOR_ASSIGN = (1 << 7),
    PYCO_OPERATOR_ASSIGN_TYPE = (1 << 8),
    PYCO_OPERATOR_ASSIGN_CONST = (1 << 9),
    PYCO_OPERATOR_EQUAL = (1 << 10),
    PYCO_OPERATOR_GREATER = (1 << 11),
    PYCO_OPERATOR_LESS = (1 << 12),
    PYCO_OPERATOR_AND = (1 << 13),
    PYCO_OPERATOR_OR = (1 << 14),
    PYCO_OPERATOR_NOT = (1 << 15),
    PYCO_OPERATOR_XOR = (1 << 16),
    PYCO_OPERATOR_BITWISE = (1 << 17),
    PYCO_OPERATOR_LEFT_SHIFT = (1 << 18),
    PYCO_OPERATOR_RIGHT_SHIFT = (1 << 19),
    PYCO_OPERATOR_TERNARY = (1 << 20),
    PYCO_OPERATOR_GROUPING = (1 << 21),
    PYCO_OPERATOR_FUNCTION_CALL = (1 << 22),
    PYCO_OPERATOR_ARRAY_INDEX = (1 << 25),
    PYCO_OPERATOR_MEMBER_ACCESS = (1 << 26),
    PYCO_OPERATOR_INVALID = (1 << 30),
};

inline pyco_uint8 _is_successive(const pyco_token *current_token, char operator)
{
    return (
        operator != '\0' &&
        current_token->next &&
        current_token->next->flags & PYCO_TOKEN_TYPE_SPECIAL &&
        current_token->next->flags & PYCO_TOKEN_FLAG_SUCCESSIVE &&
        current_token->next->value[0] == operator);
}

pyco_uint32 _token_to_operator(const pyco_token *token)
{
    if (token == PYCO_NULL || ~token->flags & PYCO_TOKEN_TYPE_SPECIAL)
    {
        return PYCO_OPERATOR_NONE;
    }

    if (token->value[0] == ')' || token->value[0] == ']' || token->value[0] == '{' || token->value[0] == '}' || token->value[0] == ';')
    {
        return PYCO_OPERATOR_NONE;
    }

    pyco_uint32 operator = PYCO_OPERATOR_NONE;
    pyco_uint32 assign_composite = PYCO_OPERATOR_ASSIGN | PYCO_OPERATOR_COMPOSITE;

    switch (token->value[0])
    {
    case ':':
        return _is_successive(token, '=') ? PYCO_OPERATOR_ASSIGN_TYPE | assign_composite : _is_successive(token, ':') ? PYCO_OPERATOR_ASSIGN_TYPE | PYCO_OPERATOR_ASSIGN_CONST | PYCO_OPERATOR_COMPOSITE
                                                                                                                      : PYCO_OPERATOR_ASSIGN_TYPE;
    case '+':
        return _is_successive(token, '=') ? PYCO_OPERATOR_ADD | assign_composite : _is_successive(token, '+') ? PYCO_OPERATOR_INCREMENT | PYCO_OPERATOR_COMPOSITE
                                                                                                              : PYCO_OPERATOR_ADD;
    case '-':
        return _is_successive(token, '=') ? PYCO_OPERATOR_SUBTRACT | assign_composite : _is_successive(token, '-') ? PYCO_OPERATOR_DECREMENT | PYCO_OPERATOR_COMPOSITE
                                                                                                                   : PYCO_OPERATOR_SUBTRACT;
    case '*':
        return _is_successive(token, '=') ? PYCO_OPERATOR_MULTIPLY | assign_composite : PYCO_OPERATOR_MULTIPLY;
    case '/':
        return _is_successive(token, '=') ? PYCO_OPERATOR_DIVIDE | assign_composite : _is_successive(token, '/') ? PYCO_OPERATOR_NONE | PYCO_OPERATOR_COMPOSITE
                                                                                                                 : PYCO_OPERATOR_DIVIDE;
    case '=':
        return _is_successive(token, '=') ? PYCO_OPERATOR_EQUAL | PYCO_OPERATOR_COMPOSITE : PYCO_OPERATOR_ASSIGN;
    case '<':
        return _is_successive(token, '=') ? PYCO_OPERATOR_LESS | PYCO_OPERATOR_EQUAL | PYCO_OPERATOR_COMPOSITE : _is_successive(token, '<') ? PYCO_OPERATOR_LEFT_SHIFT | PYCO_OPERATOR_BITWISE | PYCO_OPERATOR_COMPOSITE
                                                                                                                                            : PYCO_OPERATOR_LESS;
    case '>':
        return _is_successive(token, '=') ? PYCO_OPERATOR_GREATER | PYCO_OPERATOR_EQUAL | PYCO_OPERATOR_COMPOSITE : _is_successive(token, '>') ? PYCO_OPERATOR_RIGHT_SHIFT | PYCO_OPERATOR_BITWISE | PYCO_OPERATOR_COMPOSITE
                                                                                                                                               : PYCO_OPERATOR_GREATER;
    case '!':
        return PYCO_OPERATOR_NOT;
    case '~':
        return PYCO_OPERATOR_NOT | PYCO_OPERATOR_BITWISE | PYCO_OPERATOR_COMPOSITE;
    case '?':
        return PYCO_OPERATOR_TERNARY;
    case '(':
        return PYCO_OPERATOR_GROUPING;
    case '[':
        return PYCO_OPERATOR_ARRAY_INDEX;
    case '.':
        return PYCO_OPERATOR_MEMBER_ACCESS;
    }

    return PYCO_OPERATOR_INVALID;
}

inline pyco_uint16 _encode_powers(pyco_uint8 left, pyco_uint8 right)
{
    return (left << 8) | right;
}

inline pyco_uint16 _parser_get_prefix_binding_power(pyco_uint32 operator)
{
    switch (operator)
    {
    case PYCO_OPERATOR_NOT:
        return 9;
    }

    return 0;
}

pyco_uint32 clearBit(pyco_uint32 N, pyco_uint32 K)
{
    return (N & (~(1 << (K - 1))));
}

inline pyco_uint16 _parser_get_infix_binding_power(pyco_uint32 operator)
{
    pyco_uint32 new_operator = clearBit(operator, PYCO_OPERATOR_COMPOSITE);

    switch (new_operator)
    {
    case PYCO_OPERATOR_TERNARY:
        return _encode_powers(4, 3);
    case PYCO_OPERATOR_ADD:
    case PYCO_OPERATOR_SUBTRACT:
        return _encode_powers(5, 6);
    case PYCO_OPERATOR_MULTIPLY:
    case PYCO_OPERATOR_DIVIDE:
    case PYCO_OPERATOR_EQUAL:
    case PYCO_OPERATOR_LESS:
    case PYCO_OPERATOR_GREATER:
    case PYCO_OPERATOR_LEFT_SHIFT:
    case PYCO_OPERATOR_RIGHT_SHIFT:
        return _encode_powers(7, 8);
    case PYCO_OPERATOR_MEMBER_ACCESS:
        return _encode_powers(14, 13);
    }

    return 0;
}

inline pyco_uint16 _parser_get_postfix_binding_power(pyco_uint32 operator)
{
    pyco_uint32 new_operator = clearBit(operator, PYCO_OPERATOR_COMPOSITE);

    switch (new_operator)
    {
    case PYCO_OPERATOR_ARRAY_INDEX:
    case PYCO_OPERATOR_INCREMENT:
    case PYCO_OPERATOR_DECREMENT:
        return 11;
    }

    return PYCO_OPERATOR_NONE;
}

inline bool _parser_is_infix_operator(pyco_uint32 operator)
{
    switch (operator)
    {
    case PYCO_OPERATOR_ADD:
    case PYCO_OPERATOR_SUBTRACT:
    case PYCO_OPERATOR_MULTIPLY:
    case PYCO_OPERATOR_DIVIDE:
    case PYCO_OPERATOR_LESS:
    case PYCO_OPERATOR_GREATER:
    case PYCO_OPERATOR_EQUAL:
    case PYCO_OPERATOR_LEFT_SHIFT:
    case PYCO_OPERATOR_RIGHT_SHIFT:
        return true;
    }

    return false;
}

inline const bool _parser_is_prefix_operator(pyco_uint32 operator)
{
    pyco_uint32 new_operator = clearBit(operator, PYCO_OPERATOR_COMPOSITE);

    switch (new_operator)
    {
    case PYCO_OPERATOR_NOT:
        return true;
    }

    return false;
}

inline const bool _parser_is_postfix_operator(pyco_uint32 operator)
{
    pyco_uint32 new_operator = clearBit(operator, PYCO_OPERATOR_COMPOSITE);

    switch (new_operator)
    {
    case PYCO_OPERATOR_ARRAY_INDEX:
    case PYCO_OPERATOR_INCREMENT:
    case PYCO_OPERATOR_DECREMENT:
        return true;
    }

    return false;
}

pyco_ast_node *_parse_scope(pyco_ast *ast, pyco_lexer *lexer);
pyco_ast_node *_parse_expression(pyco_ast *ast, pyco_lexer *lexer, pyco_uint32 flags, pyco_uint8 minimum_binding_power);

bool _parser_handle_comments(pyco_ast *ast, pyco_lexer *lexer)
{
    const pyco_token *start_token = lexer_get_current_token(lexer);

    do
    {
        const pyco_token *current_token = lexer_get_current_token(lexer);

        if (current_token->start.line != start_token->start.line)
        {
            return true;
        }
    } while (lexer_get_next_token(lexer));

    return true;
}

pyco_ast_node *_parser_handle_function_arguments(pyco_ast *ast, pyco_lexer *lexer)
{
    pyco_ast_node *arguments_node = pyco_ast_node_create(ast, PYCO_NULL, PYCO_AST_NODE_TYPE_ARGUMENTS, PYCO_NULL, 0);

    const pyco_token *arguments_start_token = lexer_get_next_token(lexer);

    if ((~arguments_start_token->flags & PYCO_TOKEN_TYPE_SPECIAL) || arguments_start_token->value[0] != '(')
    {
        return false;
    }

    const pyco_token *argument_name = PYCO_NULL;
    const pyco_token *argument_type = PYCO_NULL;

    while (lexer_get_next_token(lexer))
    {
        const pyco_token *current_token = lexer_get_current_token(lexer);

        if (current_token->flags & PYCO_TOKEN_TYPE_SPECIAL && current_token->value[0] == ')')
        {
            lexer_get_next_token(lexer);
            break;
        }

        if (argument_name == PYCO_NULL)
        {
            argument_name = current_token;
        }
        else if (argument_type == PYCO_NULL)
        {
            argument_type = current_token;
        }

        if (argument_name && argument_type)
        {
            pyco_ast_node_add(ast, arguments_node, argument_name->value, PYCO_AST_NODE_TYPE_FUNCTION, PYCO_NULL, 0);
            argument_name = PYCO_NULL;
            argument_type = PYCO_NULL;
        }
    }

    return arguments_node;
}

pyco_ast_node *_parser_handle_function_body(pyco_ast *ast, pyco_lexer *lexer)
{
    return _parse_scope(ast, lexer);
}

pyco_ast_node *_parser_handle_function_declaration(pyco_ast *ast, pyco_lexer *lexer, const pyco_token *identifier_token)
{
    pyco_ast_node *function_node = pyco_ast_node_create(ast, identifier_token->value, PYCO_AST_NODE_TYPE_FUNCTION, PYCO_NULL, 0);

    pyco_ast_node *function_arguments_node = _parser_handle_function_arguments(ast, lexer);
    pyco_ast_node *function_body_node = _parser_handle_function_body(ast, lexer);

    if (function_arguments_node)
    {
        pyco_ast_node_append(function_node, function_arguments_node);
    }

    if (function_body_node)
    {
        pyco_ast_node_append(function_node, function_body_node);
    }

    return function_node;
}

// MARK: parse struct
pyco_ast_node *_parser_handle_struct_declaration(pyco_ast *ast, pyco_lexer *lexer, const pyco_token *identifier_token)
{
    pyco_ast_node *struct_node = pyco_ast_node_create(ast, identifier_token->value, PYCO_AST_NODE_TYPE_STRUCT, PYCO_NULL, 0);

    const pyco_token *definition_start = lexer_get_next_token(lexer);

    if (!definition_start || ~definition_start->flags & PYCO_TOKEN_TYPE_SPECIAL || definition_start->value[0] != '{')
    {
        return false;
    }

    const pyco_token *field_name = PYCO_NULL;
    const pyco_token *field_type = PYCO_NULL;
    bool invalid = false;
    bool ready_to_parse = true;

    while (lexer_get_next_token(lexer))
    {
        const pyco_token *current_token = lexer_get_current_token(lexer);

        bool is_field_completed = field_name && field_type;

        if (!current_token)
        {
            invalid = true;
            break;
        }

        if (current_token->flags & PYCO_TOKEN_TYPE_INDENT)
        {
            ready_to_parse = true;
            continue;
        }

        if (current_token->flags & PYCO_TOKEN_TYPE_SPECIAL)
        {
            if (current_token->value[0] == ';')
            {
                if (field_name && !field_type)
                {
                    break;
                }

                ready_to_parse = true;
                continue;
            }

            if (current_token->value[0] == '}')
            {
                lexer_get_next_token(lexer);
                break;
            }

            break;
        }

        if (ready_to_parse)
        {
            if (field_name == PYCO_NULL)
            {
                field_name = current_token;
            }
            else if (field_type == PYCO_NULL)
            {
                field_type = current_token;
            }
            else
            {
                invalid = true;
                break;
            }

            if (field_name && field_type)
            {
                if (field_name->start.line != field_type->start.line)
                {
                    invalid = true;
                    break;
                }

                pyco_ast_node *field_node = pyco_ast_node_add(ast, struct_node, field_name->value, PYCO_AST_NODE_TYPE_STRUCT_FIELD, PYCO_NULL, 0);

                field_name = PYCO_NULL;
                field_type = PYCO_NULL;
                ready_to_parse = false;
            }
        }
    }

    return struct_node;
}

// MARK: parse var declaration
pyco_ast_node *_parser_handle_variable_declaration(pyco_ast *ast, pyco_lexer *lexer, const pyco_token *identifier_token)
{
    pyco_ast_node *declaration_node = pyco_ast_node_create(ast, identifier_token->value, PYCO_AST_NODE_TYPE_STATEMENT, PYCO_NULL, 0);
    pyco_ast_node *expression_node = _parse_expression(ast, lexer, PYCO_NULL, 0);

    pyco_ast_node_append(declaration_node, expression_node);

    return declaration_node;
}

// MARK: parse declaration
pyco_ast_node *_parser_handle_declaration(pyco_ast *ast, pyco_lexer *lexer, const pyco_token *identifier_token)
{
    const pyco_token *declaration_type_token = lexer_get_current_token(lexer);
    const pyco_token *declaration_modifier_token = lexer_get_next_token(lexer);

    const pyco_token *next_token = lexer_get_next_token(lexer);

    if (!next_token || next_token->flags & PYCO_TOKEN_TYPE_INDENT)
    {
        return false;
    }

    if (declaration_modifier_token->flags & PYCO_TOKEN_TYPE_SPECIAL && declaration_modifier_token->value[0] == ':' && next_token->flags & PYCO_TOKEN_TYPE_IDENTIFIER)
    {
        if (strcmp(next_token->value, "function") == 0)
        {
            return _parser_handle_function_declaration(ast, lexer, identifier_token);
        }

        if (strcmp(next_token->value, "struct") == 0)
        {
            return _parser_handle_struct_declaration(ast, lexer, identifier_token);
        }
    }

    return _parser_handle_variable_declaration(ast, lexer, identifier_token);
}

pyco_uint32 get_control_flow_type(const pyco_token *token)
{
    if (strcmp(token->value, "if") == 0)
    {
        return PYCO_AST_NODE_TYPE_IF;
    }

    if (strcmp(token->value, "for") == 0)
    {
        return PYCO_AST_NODE_TYPE_FOR;
    }

    if (strcmp(token->value, "do") == 0)
    {
        return PYCO_AST_NODE_TYPE_DO_WHILE;
    }

    if (strcmp(token->value, "while") == 0)
    {
        return PYCO_AST_NODE_TYPE_WHILE;
    }

    if (strcmp(token->value, "continue") == 0)
    {
        return PYCO_AST_NODE_TYPE_CONTINUE;
    }

    if (strcmp(token->value, "break") == 0)
    {
        return PYCO_AST_NODE_TYPE_BREAK;
    }

    return PYCO_AST_NODE_TYPE_NONE;
}

// MARK: parse control flow
pyco_ast_node *_parse_control_flow(pyco_ast *ast, pyco_lexer *lexer)
{
    const pyco_token *token = lexer_get_current_token(lexer);

    pyco_uint32 control_flow_type = get_control_flow_type(token);

    if (!lexer_get_next_token(lexer))
    {
        return PYCO_NULL;
    }

    pyco_ast_node *control_flow_node = pyco_ast_node_create(ast, PYCO_NULL, control_flow_type, PYCO_NULL, 0);

    if (control_flow_type == PYCO_AST_NODE_TYPE_IF)
    {
        pyco_ast_node *true_path_node = pyco_ast_node_create(ast, "IF_TRUE", control_flow_type, PYCO_NULL, 0);
        pyco_ast_node *condition_node = pyco_ast_node_create(ast, "CONDITION", control_flow_type, PYCO_NULL, 0);
        pyco_ast_node *expression_node = _parse_expression(ast, lexer, 0, 0);
        pyco_ast_node *body_node = _parse_scope(ast, lexer);

        pyco_ast_node_append(condition_node, expression_node);
        pyco_ast_node_append(true_path_node, condition_node);
        pyco_ast_node_append(true_path_node, body_node);
        pyco_ast_node_append(control_flow_node, true_path_node);

        const pyco_token *current_token = lexer_get_current_token(lexer);

        if (current_token && current_token->length == 4 && strcmp(current_token->value, "else") == 0)
        {
            if (!current_token->next)
            {
                return PYCO_NULL;
            }

            lexer_get_next_token(lexer);

            pyco_ast_node *else_path_node = pyco_ast_node_create(ast, "ELSE", control_flow_type, PYCO_NULL, 0);

            if (current_token->next->flags & PYCO_TOKEN_TYPE_SPECIAL && current_token->next->value[0] == '{')
            {
                pyco_ast_node *else_body_node = _parse_scope(ast, lexer);
                pyco_ast_node_append(else_path_node, else_body_node);
            }

            if (current_token->next->length == 2 && current_token->next->value[0] == 'i' && current_token->next->value[1] == 'f')
            {
                pyco_ast_node *else_body_node = _parse_control_flow(ast, lexer);
                pyco_ast_node_append(else_path_node, else_body_node);
            }

            pyco_ast_node_append(control_flow_node, else_path_node);
        }
    }

    if (control_flow_type == PYCO_AST_NODE_TYPE_WHILE)
    {
        pyco_ast_node *condition_node = pyco_ast_node_create(ast, "CONDITION", control_flow_type, PYCO_NULL, 0);
        pyco_ast_node *expression_node = _parse_expression(ast, lexer, 0, 0);
        pyco_ast_node *body_node = _parse_scope(ast, lexer);

        pyco_ast_node_append(control_flow_node, condition_node);
        pyco_ast_node_append(condition_node, expression_node);
        pyco_ast_node_append(control_flow_node, body_node);
    }

    if (control_flow_type == PYCO_AST_NODE_TYPE_DO_WHILE)
    {
        const pyco_token *current_token = lexer_get_current_token(lexer);

        if (!(current_token->flags & PYCO_TOKEN_TYPE_SPECIAL) || current_token->value[0] != '{')
        {
            return PYCO_NULL;
        }

        pyco_ast_node *body_node = _parse_scope(ast, lexer);

        current_token = lexer_get_current_token(lexer);

        if (!current_token || strcmp(current_token->value, "while"))
        {
            return PYCO_NULL;
        }

        pyco_ast_node *condition_node = pyco_ast_node_create(ast, "CONDITION", control_flow_type, PYCO_NULL, 0);
        pyco_ast_node *expression_node = _parse_expression(ast, lexer, 0, 0);

        pyco_ast_node_append(control_flow_node, condition_node);
        pyco_ast_node_append(condition_node, expression_node);
        pyco_ast_node_append(control_flow_node, body_node);
    }

    if (control_flow_type == PYCO_AST_NODE_TYPE_FOR)
    {
        const pyco_token *current_token = lexer_get_current_token(lexer);

        if (!(current_token->flags & PYCO_TOKEN_TYPE_SPECIAL) || current_token->value[0] != '{')
        {
            pyco_ast_node *arguments_node = pyco_ast_node_create(ast, "ARGUMENTS", control_flow_type, PYCO_NULL, 0);
            pyco_ast_node *expression_node = PYCO_NULL;
            do
            {
                current_token = lexer_get_current_token(lexer);

                if (current_token->flags & PYCO_TOKEN_TYPE_SPECIAL && current_token->value[0] == ';')
                {
                    if (expression_node == PYCO_NULL)
                    {
                        pyco_ast_node *empty_argument_node = pyco_ast_node_create(ast, "ARGUMENT_PART_EMPTY", control_flow_type, PYCO_NULL, 0);
                        pyco_ast_node_append(arguments_node, empty_argument_node);
                    }
                    continue;
                }

                if (expression_node = _parse_expression(ast, lexer, 0, 0))
                {
                    pyco_ast_node *argument_expression_node = pyco_ast_node_create(ast, "ARGUMENT_EXPRESSION", control_flow_type, PYCO_NULL, 0);
                    pyco_ast_node_append(argument_expression_node, expression_node);
                    pyco_ast_node_append(arguments_node, argument_expression_node);
                }

                current_token = lexer_get_current_token(lexer);

                if (current_token->flags & PYCO_TOKEN_TYPE_SPECIAL && current_token->value[0] == '{')
                {
                    break;
                }
            } while (lexer_get_next_token(lexer));

            pyco_ast_node_append(control_flow_node, arguments_node);
        }

        pyco_ast_node *body_node = _parse_scope(ast, lexer);

        pyco_ast_node_append(control_flow_node, body_node);
    }

    return control_flow_node;
}

// MARK: parse scope
pyco_ast_node *_parse_scope(pyco_ast *ast, pyco_lexer *lexer)
{
    if (!lexer_get_next_token(lexer))
    {
        return PYCO_NULL;
    }

    pyco_ast_node *scope_node = pyco_ast_node_create(ast, PYCO_NULL, PYCO_AST_NODE_TYPE_SCOPE, PYCO_NULL, 0);

    do
    {
        const pyco_token *token = lexer_get_current_token(lexer);

        if (token->flags & PYCO_TOKEN_TYPE_INDENT)
        {
            continue;
        }

        if (token->value[0] == '{')
        {
            pyco_ast_node_append(scope_node, _parse_scope(ast, lexer));
            continue;
        }

        if (get_control_flow_type(token))
        {
            pyco_ast_node_append(scope_node, _parse_control_flow(ast, lexer));
            continue;
        }

        pyco_ast_node *expression_node = _parse_expression(ast, lexer, 0, 0);

        if (expression_node)
        {
            pyco_ast_node_append(scope_node, expression_node);
        }

        token = lexer_get_current_token(lexer);

        if (token && token->flags & PYCO_TOKEN_TYPE_SPECIAL && token->value[0] == '}')
        {
            lexer_get_next_token(lexer);
            break;
        }
    } while (lexer_get_next_token(lexer));

    return scope_node;
}

// MARK: parse expression
pyco_ast_node *_parse_expression(pyco_ast *ast, pyco_lexer *lexer, pyco_uint32 flags, pyco_uint8 minimum_binding_power)
{
    const pyco_token *left_hand_token = lexer_get_current_token(lexer);
    pyco_ast_node *left_hand_side = PYCO_NULL;

    pyco_uint32 possible_operator = _token_to_operator(left_hand_token);

    if (left_hand_token && (left_hand_token->value[0] == '{' || left_hand_token->value[0] == '}'))
    {
        return left_hand_side;
    }

    if (possible_operator && (possible_operator & PYCO_OPERATOR_COMPOSITE) && left_hand_token->value[0] == '/' && left_hand_token->next->value[0] == '/')
    {
        _parser_handle_comments(ast, lexer);
        return left_hand_side;
    }

    lexer_get_next_token(lexer);

    if (_parser_is_prefix_operator(possible_operator))
    {
        pyco_ast_node *right_hand_side = _parse_expression(ast, lexer, flags, _parser_get_prefix_binding_power(possible_operator));
        left_hand_side = pyco_ast_node_create(ast, left_hand_token->value, PYCO_AST_NODE_TYPE_EXPRESSION, 0, 0);
        pyco_ast_node_append(left_hand_side, right_hand_side);
    }
    else if (possible_operator == PYCO_OPERATOR_GROUPING)
    {
        pyco_ast_node *expression = _parse_expression(ast, lexer, flags, 0);
        left_hand_side = pyco_ast_node_create(ast, left_hand_token->value, PYCO_AST_NODE_TYPE_EXPRESSION, 0, 0);
        pyco_ast_node_append(left_hand_side, expression);
        lexer_get_next_token(lexer);
    }
    else
    {
        left_hand_side = pyco_ast_node_create(ast, left_hand_token->value, PYCO_AST_NODE_TYPE_LITERAL, 0, 0);
    }

    do
    {
        const pyco_token *operator_token = lexer_get_current_token(lexer);
        const pyco_uint32 operator = _token_to_operator(operator_token);

        if (!operator)
        {
            break;
        }

        if (operator & PYCO_OPERATOR_COMPOSITE && (operator_token->value[0] == '=' || operator_token->value[0] == '+'))
        {
            lexer_get_next_token(lexer);
        }

        if ((operator & PYCO_OPERATOR_COMPOSITE) && operator_token->value[0] == '/' && operator_token->next->value[0] == '/')
        {
            _parser_handle_comments(ast, lexer);
            continue;
        }

        if (_parser_is_postfix_operator(operator))
        {
            const pyco_uint16 postfix_binding_power = _parser_get_postfix_binding_power(operator);

            if (postfix_binding_power < minimum_binding_power)
            {
                break;
            }

            lexer_get_next_token(lexer);

            if (operator == PYCO_OPERATOR_ARRAY_INDEX)
            {
                pyco_ast_node *right_hand_side = _parse_expression(ast, lexer, flags, 0);
                pyco_ast_node *new_left_hand_side = pyco_ast_node_create(ast, "INDEX_OPERATOR", PYCO_AST_NODE_TYPE_EXPRESSION, 0, 0);

                pyco_ast_node_append(new_left_hand_side, left_hand_side);
                pyco_ast_node_append(new_left_hand_side, right_hand_side);

                left_hand_side = new_left_hand_side;

                lexer_get_next_token(lexer);
            }
            else
            {
                pyco_ast_node *new_left_hand_side = pyco_ast_node_create(ast, operator_token->value, PYCO_AST_NODE_TYPE_EXPRESSION, 0, 0);
                pyco_ast_node_append(new_left_hand_side, left_hand_side);
                left_hand_side = new_left_hand_side;
            }

            continue;
        }

        if (flags & PYCO_OPERATOR_FUNCTION_CALL && operator_token->flags & PYCO_TOKEN_TYPE_SPECIAL && operator_token->value[0] == ',')
        {
            break;
        }

        if (operator == PYCO_OPERATOR_INVALID)
        {
            // throw error: invalid operator found "operation"
            return PYCO_NULL;
        }

        if (operator & PYCO_OPERATOR_ASSIGN_TYPE && (operator & PYCO_OPERATOR_ASSIGN || operator & PYCO_OPERATOR_ASSIGN_CONST))
        {
            left_hand_side = _parser_handle_declaration(ast, lexer, left_hand_token);
            break;
        }

        if (left_hand_token->flags & PYCO_TOKEN_TYPE_IDENTIFIER && operator == PYCO_OPERATOR_GROUPING)
        {
            pyco_ast_node *new_left_hand_side = pyco_ast_node_create(ast, left_hand_token->value, PYCO_AST_NODE_TYPE_CALL, 0, 0);

            lexer_get_next_token(lexer);

            do
            {
                const pyco_token *current_token = lexer_get_current_token(lexer);

                if (current_token == PYCO_NULL || (current_token->flags & PYCO_TOKEN_TYPE_SPECIAL && current_token->value[0] == ')'))
                {
                    break;
                }

                if (current_token->flags & PYCO_TOKEN_TYPE_SPECIAL && current_token->value[0] == ',')
                {
                    lexer_get_next_token(lexer);
                    continue;
                }

                pyco_ast_node *expression = _parse_expression(ast, lexer, flags | PYCO_OPERATOR_FUNCTION_CALL, 0);
                pyco_ast_node_append(new_left_hand_side, expression);
            } while (true);

            lexer_get_next_token(lexer);
            pyco_ast_node_append(left_hand_side, new_left_hand_side);
            left_hand_side = new_left_hand_side;

            continue;
        }

        pyco_uint16 binding_powers = _parser_get_infix_binding_power(operator);

        if (binding_powers)
        {
            pyco_uint8 left_binding_power = (binding_powers >> 8) & 0xFF;
            pyco_uint8 right_binding_power = binding_powers & 0xFF;

            if (left_binding_power < minimum_binding_power)
            {
                break;
            }

            lexer_get_next_token(lexer);

            possible_operator = _token_to_operator(lexer_get_current_token(lexer));

            if (possible_operator == PYCO_OPERATOR_TERNARY)
            {
                pyco_ast_node *middle_hand_side = _parse_expression(ast, lexer, flags, 0);

                lexer_get_next_token(lexer);

                pyco_ast_node *right_hand_side = _parse_expression(ast, lexer, flags, right_binding_power);
                pyco_ast_node *new_left_hand_side = pyco_ast_node_create(ast, operator_token->value, PYCO_AST_NODE_TYPE_EXPRESSION, 0, 0);

                pyco_ast_node_append(new_left_hand_side, left_hand_side);
                pyco_ast_node_append(new_left_hand_side, middle_hand_side);
                pyco_ast_node_append(new_left_hand_side, right_hand_side);

                left_hand_side = new_left_hand_side;
            }
            else
            {
                pyco_ast_node *right_hand_side = _parse_expression(ast, lexer, flags, right_binding_power);
                pyco_ast_node *new_left_hand_side = pyco_ast_node_create(ast, operator_token->value, PYCO_AST_NODE_TYPE_EXPRESSION, 0, 0);

                pyco_ast_node_append(new_left_hand_side, left_hand_side);
                pyco_ast_node_append(new_left_hand_side, right_hand_side);

                left_hand_side = new_left_hand_side;
            }

            continue;
        }

        break;
    } while (true);

    return left_hand_side;
}

bool _parser_handle_file(pyco_ast *ast, pyco_lexer *lexer)
{
    if (!(ast->root_node = _parse_scope(ast, lexer)))
    {
        return false;
    }

    return true;
}

typedef struct build_ast_options
{
    pyco_allocators allocators;
    bool indent_based;
} build_ast_options;

pyco_ast parser_build_ast(pyco_lexer *lexer, build_ast_options options)
{
    pyco_ast_options tree_options = {
        .allocators = options.allocators,
        .buffer_initial_size = 2000,
        .buffer_increment_size = 2000,
    };

    pyco_ast ast = initialize_tree(tree_options);

    _parser_handle_file(&ast, lexer);

    return ast;
}

pyco_compile_options pyco_initialize_compile_options()
{
    pyco_compile_options options;

    options.allocators.malloc = PYCO_NULL;
    options.allocators.realloc = PYCO_NULL;
    options.allocators.free = PYCO_NULL;

    options.copy_buffer = 0;
    options.indent_based = 0;

    return options;
}

// MARK: compilation
pyco_compiled_program pyco_compile(const pyco_uint8 *data, pyco_uint64 size, pyco_compile_options options)
{
    pyco_compiled_program program;

    program.data = PYCO_NULL;
    program.size = 0;
    program.valid = 0;
    program.errors = 0;
    program.compile_options = options;

    pyco_lexer_options lexer_options = lexer_initialize_options();
    lexer_options.allocators = options.allocators;

    pyco_lexer *lexer = lexer_create(lexer_options);

    pyco_buffer buffer = {
        .data = (pyco_uint8 *)data,
        .size = size,
    };

    lexer_process_buffer(lexer, &buffer);
    printf("testing lexer - token count: %lld\n\n", lexer->tokens_count);

    build_ast_options ast_options = {
        .indent_based = !!options.indent_based,
        .allocators = options.allocators,
    };

    pyco_ast ast = parser_build_ast(lexer, ast_options);
    pyco_ast_node_to_json_file("tree_output.js", ast.root_node, data);
    pyco_ast_free(&ast, ast.root_node);

    lexer_free(lexer);

    return program;
}

void pyco_free_compiled_program(pyco_compiled_program *program)
{
    if (program == PYCO_NULL)
    {
        return;
    }

    if (program->compile_options.copy_buffer)
    {
        program->compile_options.allocators.free(program->data);
    }

    program->data = PYCO_NULL;
    program->size = 0;
    program->valid = 0;
    program->errors = 0;
    program->compile_options = pyco_initialize_compile_options();
}
