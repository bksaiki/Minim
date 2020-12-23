#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

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
        else if (*it == '\"')
        {
            for (it2 = it + 1; it2 != end; ++it2)
            {
                if (*it2 == '\"' && *(it2 - 1) != '\\')
                {
                    ++it2;
                    break;
                }
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

static MinimAst* parse_str_node(char* str)
{
    MinimAst* node;
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

        init_ast_op(&node, get_arg_len(str + 1, end), 0);
        if (node->argc != 0)
        {
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
                else if (*it == '\"')
                {
                    for (it2 = it + 1; it2 != end; ++it2)
                    {
                        if (*it2 == '\"' && *(it2 - 1) != '\\')
                        {
                            ++it2;
                            break;
                        }
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
    else if (*str == '\"' && *end == '\"')
    {
        bool bad = false;

        for (char* it = str; it != end; ++it)
        {
            if (*it == '\"' && it != str && *(it - 1) != '\\')
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
            strncpy(tmp, str, len);
            tmp[len] = '\0';
            init_ast_node(&node, tmp, 0);
            free(tmp);
        }
    }
    else if (*str != '(' && *end != ')')
    {
        bool space = false;

        for (char* it = str; it != end; ++it)
        {
            if (*it == ' ')
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
            strncpy(tmp, str, len);
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

static void print_ast_errors(MinimAst* node)
{
    if (node->tags & MINIM_AST_ERR)
    {
        printf("Syntax error: %s\n", node->sym);
    }
    
    if (node->argc != 0)
    {
        for (int i = 0; i < node->argc; ++i)
            print_ast_errors(node->children[i]);
    }
}

//
//  Visible functions
// 

int parse_str(char* str, MinimAst** syn)
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