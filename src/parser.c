#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buffer.h"
#include "parser.h"

#define EXPAND_STRING       ((uint8_t)0x1)
#define EXPAND_QUOTE        ((uint8_t)0x2)

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
    size_t len = strlen(str);

    end = str + len - 1;
    if (*str == '(' && *end == ')')
    {
        char *it = str + 1, *it2;

        init_ast_op(&node, get_arg_len(str + 1, end), 0);
        if (node->argc != 0)
        {
            for (size_t idx = 0; it < end; ++idx)
            {
                if (*it == '(' || (*it == '\'' && *(it + 1) == '('))
                {
                    size_t paren = 1;

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
        for (size_t i = 0; i < node->argc; ++i)
            print_ast_errors(node->children[i]);
    }
}

//
//  Visible functions
// 

int parse_str(const char* str, MinimAst** syn)
{
    Buffer *bf;

    init_buffer(&bf);
    expand_input(str, bf);
    *syn = parse_str_node(bf->data);
    free_buffer(bf);
    
    if (!ast_validp(*syn))
    {
        print_ast_errors(*syn);
        free_ast(*syn);
        syn = NULL;
        return 0;
    } 
    
    return 1;
}