#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

static MinimAstNode* parse_str_r(char* str)
{
    MinimAstNode* node = malloc(sizeof(MinimAstNode));
    char* end = str + strlen(str) - 1;

    node->argc = 0;
    node->children = NULL;
    
    if (*str == '(' && *end == ')')
    {
        char *it = str + 1, *it2, *tmp;

        for (it2 = it; it2 != end && !isspace(*it2); ++it2);
        tmp = malloc((it2 - it + 1) * sizeof(char));
        strncpy(tmp, it, it2 - it);
        it = it2 + 1;
        it2 = it;

        node->sym = tmp;
        node->argc = get_arg_len(str + 1, end);
        if (node->argc == 0)
        {
            node->children = NULL;
        }
        else
        {
            node->children = malloc(node->argc * sizeof(MinimAstNode*));
            for (int idx = 0; it < end; ++idx)
            {
                if (*it == '(')
                {
                    int paren = 1;
                    for (it2 = it + 1; paren != 0 && it2 != end; ++it2)
                    {
                        if (*it2 == '(')         ++paren;
                        else if (*it2 == ')')    --paren;
                    }

                    tmp = malloc((it2 - it + 1) * sizeof(char));
                    strncpy(tmp, it, it2 - it);
                    node->children[idx] = parse_str_r(tmp);
                    
                    if (isspace(*it2))  it = it2 + 1;
                    else                it = it2;
                }
                else
                {
                    for (it2 = it; it2 != end && !isspace(*it2); ++it2);
                    tmp = malloc((it2 - it + 1) * sizeof(char));
                    strncpy(tmp, it, it2 - it);
                    node->children[idx] = parse_str_r(tmp);
                    it = it2 + 1;
                }
            }
        }

        node->state = MINIM_AST_VALID;
        node->tag = MINIM_AST_OP;
    }
    else if (*str != '(' && *end != ')')
    {
        node->sym = str;
        node->state = MINIM_AST_VALID;
        node->tag = MINIM_AST_VAL;
        
    }
    else
    {
        node->sym = "Unbalanced parenthesis";
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

    free(node);
}


int parse_str(const char* str, MinimAstWrapper* syn)
{
    MinimAstNode* node;
    char *cp;
    int len;

    len = strlen(str);
    cp = malloc((len + 1) * sizeof(char));
    strncpy(cp, str, len);

    node = parse_str_r(cp);
    free(cp);

    if (!valid_ast(node))
    {
        print_ast_errors(node);
        free_ast(node);
        syn->node = NULL;
        return 0;
    } 
    else
    {
        syn->node = node;
        return 1;
    }
}

void print_ast(MinimAstNode *node)
{
    printf("<syntax: ");
    print_ast_node(node);
    printf(">");
}