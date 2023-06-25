#include "parser.h"
#include "defs.h"
#include "misp.h"
#include "opc.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct buf
{
  uint8_t *p;
  size_t size;
  size_t capacity;
};

static bool
insert (struct buf *buf, cell_t c)
{
  if (buf->size == buf->capacity)
    {
      buf->capacity = buf->capacity * 2;
      buf->p = realloc (buf->p, CELL_SIZE * buf->capacity);
      if (!buf->p)
        {
          return false;
        }
    }

  CELL_WRITE (&buf->p[buf->size * CELL_SIZE], c);
  buf->size++;
  return true;
}

struct kw
{
  const char *name;
  uint64_t code;
};

static struct kw kws[] = { { "+", MISP_OPC_NADD },
                           { "-", MISP_OPC_NSUB },
                           { "/", MISP_OPC_NDIV },
                           { "*", MISP_OPC_NMUL },
                           { "%", MISP_OPC_NMOD },
                           { "#", MISP_OPC_LLEN },
                           { "remainder", MISP_OPC_NREM },
                           { "sublist", MISP_OPC_LSUB },
                           { "getl", MISP_OPC_LGET },
                           { "setl", MISP_OPC_LSET },
                           { "intersect", MISP_OPC_LINT },
                           { "debug", MISP_OPC_DBUG },
                           { "set", MISP_OPC_SET },
                           { "get", MISP_OPC_GET },
                           { "cond", MISP_OPC_COND },
                           { "loop", MISP_OPC_LOOP },
                           { "eval", MISP_OPC_EVAL },
                           { "quote", MISP_OPC_QUOTE },
                           { "do", MISP_OPC_DO },
                           /* { "trap", MISP_OPC_TRAP }, */
                           /* { "climb", MISP_OPC_CLIMB }, */
                           { "=", MISP_OPC_EQ },
                           { ">", MISP_OPC_NGRT },
                           { "<", MISP_OPC_NLSR },
                           { "<=", MISP_OPC_NLSREQ },
                           { ">=", MISP_OPC_NGRTEQ },
                           { "and", MISP_OPC_NAND },
                           { "xor", MISP_OPC_NXOR },
                           { "or", MISP_OPC_NOR },
                           { "not", MISP_OPC_NNOT },
                           { "let", MISP_OPC_LET },
                           { NULL, 0 } };

#define NUMERAL(n) (n >= '0' && n <= '9')

cell_t
parse_num (const char **s)
{
  return NUM (strtol (*s, s, 0));
}

cell_t
parse_keyword (const char **s)
{
  for (struct kw *kw = &kws[0]; kw->name; kw++)
    {
      size_t kwlen = strlen (kw->name);
      if (strncmp (kw->name, *s, kwlen) == 0)
        {
          *s += kwlen;
          return NUM (kw->code);
        }
    }
  return NUM (66);
}

cell_t
parse_list (const char **s, struct buf *buf)
{
  struct buf params;
  params.capacity = 8;
  params.size = 0;
  params.p = calloc (params.capacity, CELL_SIZE);

  while (**s)
    {
      if (**s == ')')
        {
          (*s)++;
          break;
        }
      else if (**s == '(')
        {
          (*s)++;
          insert (&params, parse_list (s, buf));
        }
      else if (isspace (**s))
        {
          (*s)++;
          continue;
        }
      else if (NUMERAL (**s)
               || ((**s == '+' || **s == '-') && NUMERAL ((*s)[1])))
        {
          insert (&params, parse_num (s));
        }
      else
        {
          insert (&params, parse_keyword (s));
        }
    }
  cell_t list = LIST (params.size, buf->size);
  memcpy (&buf->p[buf->size * CELL_SIZE], params.p, params.size * CELL_SIZE);
  buf->size += params.size;
  free (params.p);
  return list;
}

misp_parser_error_type_t
misp_parse_string (const char *s, uint8_t *tree[], size_t *tree_size,
                   cell_t *root)
{
  struct buf buf;
  buf.capacity = 4096;
  buf.size = 0;
  buf.p = calloc (buf.capacity, CELL_SIZE);

  if (*s == '(')
    {
      s++;
      *root = parse_list (&s, &buf);
    }
  else if (*s)
    {
      *root = parse_num (&s);
    }
  else
    {
      *root = LIST_NULL;
    }

  *tree = buf.p;
  *tree_size = buf.size * CELL_SIZE;

  return MISP_PARSER_ERROR_OK;
}
