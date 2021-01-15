#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buffer.h"
#include "parser.h"

#define EXPAND_STRING       ((uint8_t)0x1)
#define EXPAND_QUOTE        ((uint8_t)0x2)


#define open_paren(x)       (x == '(' || x == '[' || x == '{')
#define closed_paren(x)     (x == ')' || x == ']' || x == '}')

static void expand_input(const char *str, Buffer *bf)
{
    size_t len = strlen(str), paren = 0;
    uint8_t flags = 0;

    for (size_t i = 0; i < len; ++i)
    {
        if (flags & EXPAND_STRING)
        {
            writec_buffer(bf, str[i]);
            if (str[i] == '"' && str[i - 1] != '\\')
                flags &= ~EXPAND_STRING;
        }
        else
        {
            if (str[i] == '"')
            {
                writec_buffer(bf, str[i]);
                flags |= EXPAND_STRING;
            }
            else if (str[i] == '\'')
            {
                writes_buffer(bf, "(quote ");
                if (flags & EXPAND_QUOTE)   ++paren;
                else                        flags |= EXPAND_QUOTE;
            }
            else if (flags & EXPAND_QUOTE && str[i] == '(')
            {
                writec_buffer(bf, str[i]);
                ++paren;
            }
            else if (flags & EXPAND_QUOTE && str[i] == ')' && paren > 0)
            {
                writec_buffer(bf, str[i]);
                if (--paren == 0)
                {
                    writec_buffer(bf, ')');
                    flags &= ~EXPAND_QUOTE;
                }
            }
            else if (flags & EXPAND_QUOTE && isspace(str[i]) && paren == 0)
            {
                writec_buffer(bf, ')');
                writec_buffer(bf, str[i]);
                flags &= ~EXPAND_QUOTE;
            }
            else
            {
                writec_buffer(bf, str[i]);
            }
        }
    }

    if (flags & EXPAND_QUOTE)
    {
        for (size_t i = 0; i < paren; ++i)
            writec_buffer(bf, ')');
        writec_buffer(bf, ')');
    }
}

static size_t get_argc(const char* str, size_t begin, size_t end)
{
    size_t idx = begin, idx2, count = 0;

    while (idx < end)
    {
        while (isspace(str[begin])) ++idx;

        if (open_paren(str[idx]))
        {
            size_t paren = 1;

            for (idx2 = idx + 1; idx2 < end && paren > 0; ++idx2)
            {
                if (open_paren(str[idx2]))          ++paren;
                else if (closed_paren(str[idx2]))   --paren;
            }
        }
        else if (str[idx] == '\"')
        {
            for (idx2 = idx + 1; idx2 < end; ++idx2)
            {
                if (str[idx2] == '\"' && str[idx2 - 1] != '\\')
                {
                    ++idx2;
                    break;
                }
            }
        }
        else
        {
            for (idx2 = idx; idx2 < end && !isspace(str[idx2]); ++idx2);
        }

        ++count;
        for (idx = idx2; idx < end && isspace(str[idx]); ++idx);
    }

    return count;
}

static MinimAst* parse_str_node(const char* str, size_t begin, size_t end)
{
    MinimAst* node;
    size_t last = end - 1;
    char *tmp;

    if (open_paren(str[begin]) && closed_paren(str[last]))
    {
        size_t i = begin + 1, j;

        init_ast_op(&node, get_argc(str, i, last), 0);
        if (node->argc != 0)
        {
            for (size_t idx = 0; idx < node->argc; ++idx)
            {
                if (open_paren(str[i]))
                {
                    size_t paren = 1;

                    for (j = i + 1; paren != 0 && j < last; ++j)
                    {
                        if (open_paren(str[j]))         ++paren;
                        else if (closed_paren(str[j]))  --paren;
                    }
                }
                else if (str[i] == '\"')
                {
                    for (j = i + 1; j < last; ++j)
                    {
                        if (str[j] == '\"' && str[j - 1] != '\\')
                        {
                            ++j;
                            break;
                        }
                    }
                }
                else
                {
                    for (j = i; j < last && !isspace(str[j]); ++j);
                }

                node->children[idx] = parse_str_node(str, i, j);
                for (i = j; i < last && isspace(str[i]); ++i);
            }
        }
    }
    else if (str[begin] == '\"' && str[last] == '\"')
    {
        size_t len = end - begin;
        bool bad = false;

        for (size_t i = begin + 1; i < last; ++i)
        {
            if (str[i] == '\"' && str[i - 1] != '\\')
                bad = true;
        }
        
        if (bad)
        {
            tmp = malloc(50 * sizeof(char));
            strcpy(tmp, "Malformed string");
            init_ast_node(&node, tmp, MINIM_AST_ERR);
            free(tmp);
        }
        else
        {
            tmp = malloc((len + 1) * sizeof(char));
            strncpy(tmp, &str[begin], len);
            tmp[len] = '\0';
            init_ast_node(&node, tmp, 0);
            free(tmp);
        }
    }
    else if (!open_paren(str[begin]) && !closed_paren(str[last]))
    {
        size_t len = end - begin;
        bool space = false;

        for (size_t i = begin + 1; i < end; ++i)
        {
            if (str[i] == ' ')
                space = true;
        }

        if (space)
        {
            tmp = malloc(50 * sizeof(char));
            strcpy(tmp, "Unexpected space encountered");
            init_ast_node(&node, tmp, MINIM_AST_ERR);
            free(tmp);
        }
        else
        {
            tmp = malloc((len + 1) * sizeof(char));
            strncpy(tmp, &str[begin], len);
            tmp[len] = '\0';
            init_ast_node(&node, tmp, 0);
            free(tmp);
        }
    }
    else
    {
        tmp = malloc(50 * sizeof(char));
        strcpy(tmp, "Unmatched parenthesis");
        init_ast_node(&node, tmp, MINIM_AST_ERR);
        free(tmp);
    }

    return node;
}

static void expand_postpass(MinimAst *ast)
{
    for (size_t i = 0; i < ast->argc; ++i)
        expand_postpass(ast->children[i]);
    
    if (ast->argc == 5 && ast->children[1]->sym && ast->children[3]->sym &&
        strcmp(ast->children[1]->sym, ".") == 0 && strcmp(ast->children[3]->sym, ".") == 0)
    {
        free_ast(ast->children[1]);
        free_ast(ast->children[3]);

        ast->children[1] = ast->children[0];
        ast->children[0] = ast->children[2];
        ast->children[2] = ast->children[4];

        ast->children = realloc(ast->children, 3 * sizeof(MinimAst));
        ast->argc = 3;
    }
}

static void print_ast_errors(MinimAst* node)
{
    if (node->tags & MINIM_AST_ERR)
    {
        printf("Syntax error: %s\n", node->sym);
    }
    
    if (node->argc != 0)
    {
        for (size_t i = 0; i < node->argc; ++i)
            print_ast_errors(node->children[i]);
    }
}

//
//  Visible functions
// 

int parse_str(const char* str, MinimAst** psyntax)
{
    MinimAst *syntax;
    Buffer *bf;

    init_buffer(&bf);
    expand_input(str, bf);
    syntax = parse_str_node(bf->data, 0, strlen(bf->data));
    *psyntax = syntax;

    free_buffer(bf);
    expand_postpass(syntax);
    
    if (!ast_validp(syntax))
    {
        print_ast_errors(syntax);
        free_ast(syntax);
        return 0;
    } 
    
    return 1;
}