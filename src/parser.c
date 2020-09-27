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

    if (node->count != 0)
    {
        for (int i = 0; i < node->count; ++i)
        {
            if (!valid_ast(node->children[i]))
                return false;
        }
    }

    return true;
}

static int get_arg_len(char* start, char* end)
{
    char *it, *it2;
    int paren, count = 0;

    it = start;
    while (it != end)
    {
        while (isspace(*it) || *it == '\'' || *it == ',')   // special characters
            ++it;  

        if (*it == '(')
        {
            paren = 1;
            for (it2 = it + 1; it2 != end && paren != 0; ++it2)
            {
                if (*it2 == '(')        ++paren;
                else if (*it2 == ')')   --paren;
            }
        }
        else
        {
            for (it2 = it; it2 != end && !isspace(*it2); ++it2);
        }

        ++count;
        for (it = it2; it != end && isspace(*it); ++it);
    }

    return count;
}

static void init_ast_node(MinimAstNode** pnode)
{
    MinimAstNode *node = malloc(sizeof(MinimAstNode));
    node->sym = NULL;
    node->count = 0;
    node->children = NULL;
    *pnode = node;
}

static MinimAstNode* parse_str_node(char* str)
{
    MinimAstNode* node;
    char *end, *tmp;
    int len = strlen(str);

    end = str + len - 1;
    if (*str == '\'')
    {
        tmp = malloc((len + 8) * sizeof(char));
        strcpy(tmp, "(quote ");
        strcat(tmp, str + 1);
        strcat(tmp, ")");
        node = parse_str_node(tmp);
        free(tmp);
    }
    else if (*str == '(' && *end == ')')
    {
        char *it = str + 1, *it2;

        init_ast_node(&node);
        node->count = get_arg_len(str + 1, end);
        node->state = MINIM_AST_VALID;
        node->tag = MINIM_AST_OP;

        if (node->count != 0)
        {
            node->children = malloc(node->count * sizeof(MinimAstNode*));
            for (int idx = 0; it < end; ++idx)
            {
                if (*it == '(' || (*it == '\'' && *(it + 1) == '('))
                {
                    int paren = 1;

                    if (*it == '\'')    it2 = it + 2;
                    else                it2 = it + 1;

                    for (; paren != 0 && it2 != end; ++it2)
                    {
                        if (*it2 == '(')         ++paren;
                        else if (*it2 == ')')    --paren;
                    }
                }
                else
                {
                    for (it2 = it; it2 != end && !isspace(*it2); ++it2);
                }

                tmp = malloc((it2 - it + 1) * sizeof(char));
                strncpy(tmp, it, it2 - it);
                tmp[it2 - it] = '\0';

                node->children[idx] = parse_str_node(tmp);
                for (it = it2; it != end && isspace(*it); ++it);
                free(tmp);
            }
        }
    }
    else if (*str != '(' && *end != ')')
    {
        tmp = malloc((len + 1) * sizeof(char));
        strncpy(tmp, str, len);
        tmp[len] = '\0';

        init_ast_node(&node);
        node->sym = tmp;
        node->state = MINIM_AST_VALID;
        node->tag = MINIM_AST_VAL;
    }
    else
    {
        tmp = malloc(50 * sizeof(char));
        strcpy(tmp, "Unmatched parenthesis");

        init_ast_node(&node);
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
    
    if (node->count != 0)
    {
        for (int i = 0; i < node->count; ++i)
            print_ast_errors(node->children[i]);
    }
}

void print_ast_node(MinimAstNode *node)
{
    if (node->count == 0)
    {
        printf("%s", node->sym);
    }
    else
    {
        printf("(");
        print_ast_node(node->children[0]);
        for (int i = 1; i < node->count; ++i)
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

void copy_ast(MinimAstNode **dest, MinimAstNode *src)
{
    MinimAstNode* node;

    node = malloc(sizeof(MinimAstNode));
    node->count = src->count;
    node->state = src->state;
    node->tag = src->tag;

    if (src->sym)
    {
        node->sym = malloc((strlen(src->sym) + 1) * sizeof(char));
        strcpy(node->sym, src->sym);
    }
    else
    {
        node->sym = NULL;
    }

    if (src->count != 0)
    {
        node->children = malloc(src->count * sizeof(MinimAstNode*));
        for (int i = 0; i < src->count; ++i)
            copy_ast(&node->children[i], src->children[i]);
    }
    else
    {
        node->children = NULL;
    }

    *dest = node;
}

void free_ast(MinimAstNode* node)
{
    if (node->count != 0)
    {
        for (int i = 0; i < node->count; ++i)
        {
            if (node->children[i])
                free_ast(node->children[i]);
        }
    }

    if (node->sym != NULL)          free(node->sym);
    if (node->children != NULL)     free(node->children);
    free(node);
}


int parse_str(char* str, MinimAstNode** syn)
{
    *syn = parse_str_node(str);
    
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
    printf("<syntax:");
    print_ast_node(node);
    printf(">");
}