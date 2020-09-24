#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

static bool valid_ast(MinimAstNode* node)
{
    if (node->state != MINIM_AST_VALID)
        return false;

    if (node->argc != 0)
    {
        for (int i = 0; i < node->argc; ++i)
        {
            if (!valid_ast(node->children[i]))
                return false;
        }
    }

    return true;
}

static int get_arg_len(char* start, char* end)
{
    char *it = start, *it2;
    int paren, count = 0;

    while (it != end)
    {
        while (*it == '\'' || *it == ',')   ++it;  // special characters

        if (isspace(*it))
        {
            ++count;
            ++it;
        }
        else if (*it == '(')
        {
            paren = 1;
            for (it2 = it + 1; paren != 0 && it2 != end; ++it2)
            {
                if (*it2 == '(')        ++paren;
                else if (*it2 == ')')   --paren;
            }
            
            it = it2;
        }
        else
        {
            ++it;
        }
    }

    return count;
}

static MinimAstNode* construct_ast_node()
{
    MinimAstNode *node = malloc(sizeof(MinimAstNode));
    node->sym = NULL;
    node->argc = 0;
    node->children = NULL;
    return node;
}

static MinimAstNode* parse_str_r(char* str)
{
    MinimAstNode* node = construct_ast_node();
    char *end = str + strlen(str) - 1;
    char *tmp;
    int len;

    if (*str == '\'')
    {
        node->sym = malloc(6 * sizeof(char));
        node->children = malloc(sizeof(MinimAstNode*));
        node->state = MINIM_AST_VALID;
        node->tag = MINIM_AST_OP;
        node->argc = 1;
        strcpy(node->sym, "quote");

        len = strlen(str);
        node->children[0] = construct_ast_node();
        node->children[0]->sym = malloc(len * sizeof(char));
        node->state = MINIM_AST_VALID;
        node->tag = MINIM_AST_OP;
        node->argc = 0;
        strncpy(node->children[0]->sym, str + 1, len - 1);
    }
    else if (*str == '(' && *end == ')')
    {
        char *it = str + 1, *it2;

        for (it2 = it; it2 != end && !isspace(*it2); ++it2);
        tmp = calloc(sizeof(char), it2 - it + 1);
        strncpy(tmp, it, it2 - it);
        node->sym = tmp;
        node->argc = get_arg_len(str + 1, end);

        it = it2 + 1;
        it2 = it;

        if (node->argc != 0)
        {
            node->children = malloc(node->argc * sizeof(MinimAstNode*));
            for (int idx = 0; it < end; ++idx)
            {
                while (*it == '\'' || *it == ',')   ++it;  // special characters

                if (*it == '(')
                {
                    int paren = 1;
                    for (it2 = it + 1; paren != 0 && it2 != end; ++it2)
                    {
                        if (*it2 == '(')         ++paren;
                        else if (*it2 == ')')    --paren;
                    }
                }
                else
                {
                    for (it2 = it; it2 != end && !isspace(*it2); ++it2);
                }

                tmp = calloc(sizeof(char), it2 - it + 1);
                strncpy(tmp, it, it2 - it);
                node->children[idx] = parse_str_r(tmp);
                free(tmp);

                if (isspace(*it2))  it = it2 + 1;
                else                it = it2;
            }
        }

        node->state = MINIM_AST_VALID;
        node->tag = MINIM_AST_OP;
    }
    else if (*str != '(' && *end != ')')
    {
        len = strlen(str);
        tmp = calloc(sizeof(char), len + 1);
        strncpy(tmp, str, len);

        node->sym = tmp;
        node->state = MINIM_AST_VALID;
        node->tag = MINIM_AST_VAL;
        
    }
    else
    {
        tmp = malloc(50 * sizeof(char));
        strcpy(tmp, "Unmatched parenthesis");
        node->sym = tmp;
        node->state = MINIM_AST_ERROR;
        node->tag = MINIM_AST_NONE;
    }

    return node;
}

static void print_ast_errors(MinimAstNode* node)
{
    if (node->state == MINIM_AST_ERROR)
    {
        printf("Syntax error: %s\n", node->sym);
    }
    
    if (node->argc != 0)
    {
        for (int i = 0; i < node->argc; ++i)
            print_ast_errors(node->children[i]);
    }
}

void print_ast_node(MinimAstNode *node)
{
    if (node->argc == 0)
    {
        if (node->tag == MINIM_AST_OP)      printf("(%s)", node->sym);
        else                                printf("%s", node->sym);
    }
    else
    {
        printf("(%s", node->sym);
        for (int i = 0; i < node->argc; ++i)
        {
            printf(" ");
            print_ast_node(node->children[i]);
        }

        printf(")");
    }
}

//
//  Visible functions
// 

void free_ast(MinimAstNode* node)
{
    if (node->argc != 0)
    {
        for (int i = 0; i < node->argc; ++i)
            free_ast(node->children[i]);
    }

    if (node->sym != NULL)      free(node->sym);
    if (node->children != NULL) free(node->children);
    free(node);
}


int parse_str(char* str, MinimAstNode** syn)
{
    *syn = parse_str_r(str);
    
    if (!valid_ast(*syn))
    {
        print_ast_errors(*syn);
        free_ast(*syn);
        syn = NULL;
        return 0;
    } 
    else
    
    return 1;
}

void print_ast(MinimAstNode *node)
{
    printf("<syntax: ");
    print_ast_node(node);
    printf(">");
}