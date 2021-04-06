#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/buffer.h"
#include "parser.h"

#define EXPAND_QUOTE        ((uint8_t)0x1)

#define open_paren(x)       (x == '(' || x == '[' || x == '{')
#define closed_paren(x)     (x == ')' || x == ']' || x == '}')

static size_t get_argc(const char* str, size_t begin, size_t end)
{
    size_t idx = begin, idx2, count = 0;

    while (idx < end)
    {
        while (isspace(str[idx])) ++idx;

        if (open_paren(str[idx]) || (idx + 1 < end && str[idx] == '\'' && open_paren(str[idx + 1])))
        {
            size_t paren = 1;

            if (str[idx] == '\'') ++idx;
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

static void move_row_col(const char *str, size_t begin, size_t end, size_t *r, size_t *c)
{
    for (size_t i = begin; i < end; ++i)
    {
        if (str[i] == '\n')
        {
            ++(*r);
            (*c) = 1;
        }
        else
        {
            ++(*c);
        }
    }
}

static MinimAst*
parse_str_node(const char* str, size_t begin, size_t end,
               const char *lname, size_t row, size_t col,
               size_t paren, uint8_t eflags)
{
    MinimAst *node, *node2;
    size_t last = end - 1;
    char *tmp;

    while (isspace(str[begin]))
    {
        if (str[begin] == '\n')
        {
            ++row;
            col = 1;
        }
        ++begin;
    }

    if (begin >= end)
    {
        init_ast_node(&node, "Unexpected end of string", MINIM_AST_ERR);
        node->argc = row;
    }
    else if (str[begin] == '\'')
    {
        init_ast_node(&node2, "quote", 0);
        init_syntax_loc(&node2->loc, lname);
        node2->loc->row = row;
        node2->loc->col = col;

        init_ast_op(&node, 2, 0);
        node->children[0] = node2;
        node->children[1] = parse_str_node(str, begin + 1, end, lname, row, col + 1,
                              paren + 1, eflags & EXPAND_QUOTE);
    }
    else if (open_paren(str[begin]) && closed_paren(str[last]))
    {
        size_t i = begin + 1;
        size_t j, idx;

        if (eflags & EXPAND_QUOTE)
        {
            init_ast_op(&node, get_argc(str, i, last) + 1, 0);
            init_ast_node(&node->children[0], "quote", 0);
            init_syntax_loc(&node->children[0]->loc, lname);
            node->children[0]->loc->row = row;
            node2->children[0]->loc->col = col;
            idx = 1;
        }
        else
        {
            init_ast_op(&node, get_argc(str, i, last), 0);
            idx = 0;
        }

        for (; idx < node->argc; ++idx)
        {
            if (open_paren(str[i]) || (i + 1 < end && str[i] == '\'' && open_paren(str[i + 1])))
            {
                size_t paren = 1;

                if (str[i] == '\'') j = i + 2;
                else                j = i + 1;

                for (; paren != 0 && j < last; ++j)
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
            
            move_row_col(str, i, j, &row, &col);
            node->children[idx] = parse_str_node(str, i, j, lname, row, col, paren, eflags);
            for (i = j; i < last && isspace(str[i]); ++i);
        }
    }
    else if (str[begin] == '\"' && str[last] == '\"')
    {
        size_t len = end - begin;
        bool bad = false;

        for (size_t i = begin + 1; i < last; ++i)
        {   
            // unbalanced string quotes or space
            if (str[i] == '\"' && str[i - 1] != '\\')
                bad = true;
        }
        
        if (bad)
        {
            init_ast_node(&node, "Malformed string", MINIM_AST_ERR);
            node->argc = row;
        }
        else
        {
            tmp = malloc((len + 1) * sizeof(char));
            strncpy(tmp, &str[begin], len);
            tmp[len] = '\0';
            init_ast_node(&node, tmp, 0);
            init_syntax_loc(&node->loc, lname);
            node->loc->row = row;
            node->loc->col = col;
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
            init_ast_node(&node, "Unexpected space encountered", MINIM_AST_ERR);
            node->argc = row;
        }
        else
        {
            tmp = malloc((len + 1) * sizeof(char));
            strncpy(tmp, &str[begin], len);
            tmp[len] = '\0';
            init_ast_node(&node, tmp, 0);
            init_syntax_loc(&node->loc, lname);
            node->loc->row = row;
            node->loc->col = col;
            free(tmp);
        }
    }
    else
    {
        init_ast_node(&node, "Unmatched parenthesis", MINIM_AST_ERR);
        node->argc = row;
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

//
//  Visible functions
// 

int parse_str(const char* str, MinimAst** psyntax)
{
    SyntaxLoc *loc;
    int status;

    init_syntax_loc(&loc, "???");
    status = parse_expr_loc(str, psyntax, loc);
    free_syntax_loc(loc);

    return status;
}

int parse_expr_loc(const char* str, MinimAst** psyntax, SyntaxLoc *loc)
{
    MinimAst *syntax;

    syntax = parse_str_node(str, 0, strlen(str), loc->name, loc->row, loc->col, 0, 0);
    *psyntax = syntax;

    if (!ast_validp(syntax))
    {
        printf("Syntax: %s\n", syntax->sym);
        printf("  at %s\n", str);
        printf("  in %s:%lu\n", loc->name, syntax->argc);
        syntax->argc = 0;
        return 0;
    } 
    
    expand_postpass(syntax);
    return 1;
}