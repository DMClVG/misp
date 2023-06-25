/*************************************************************************/
/* MISP                                                                  */
/* Copyright (C) 2023                                                    */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */
/*************************************************************************/

#include "misp.h"
#include "defs.h"
#include "opc.h"
#include "parser.h"
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CELL_SIZE 9 /* uint64_t + uint8_t (NO PADDING)*/

void
misp_list_get (misp_t *M, cell_t list, cell_t *cell, size_t i)
{
  CELL_READ (&M->mem[(LIST_PTR (list) + i) * CELL_SIZE], cell);
}

void
misp_list_set (misp_t *M, cell_t list, cell_t cell, size_t i)
{
  CELL_WRITE (&M->mem[(LIST_PTR (list) + i) * CELL_SIZE], cell);
}

// [a, b[
void
misp_list_sub (misp_t *M, cell_t list, cell_t *sub, size_t a, size_t b)
{
  *sub = LIST ((b - a), (LIST_PTR (list) + a));
}

void
misp_env_parent (misp_t *M, cell_t *parent)
{
  misp_list_get (M, M->env, parent, 0);
}

void
misp_env_node (misp_t *M, cell_t *node)
{
  misp_list_get (M, M->env, node, 1);
}

void
misp_env_args (misp_t *M, cell_t *args)
{
  misp_list_get (M, M->env, args, 2);
}

void
misp_env_trap (misp_t *M, cell_t *trap)
{
  misp_list_get (M, M->env, trap, 4);
}

void
misp_env_stack (misp_t *M, cell_t *stack)
{
  misp_list_get (M, M->env, stack, 3);
}

void
misp_env_push (misp_t *M, cell_t cell)
{
  cell_t stack;
  misp_env_stack (M, &stack);

  stack
      = LIST (LIST_LEN (stack) + 1, LIST_PTR (stack)); // TODO: check overflow

  misp_list_set (M, stack, cell, LIST_LEN (stack) - 1);
  misp_list_set (M, M->env, stack, 3);
}

void
misp_env_pop (misp_t *M, size_t amount)
{
  cell_t stack;
  misp_env_stack (M, &stack);

  stack = LIST (LIST_LEN (stack) - amount,
                LIST_PTR (stack)); // TODO: check underflow?
  misp_list_set (M, M->env, stack, 3);
}

void
misp_env_get (misp_t *M, cell_t *cell, size_t i)
{
  cell_t stack;
  misp_env_stack (M, &stack);
  misp_list_get (M, stack, cell, i);
}

void
misp_env_top (misp_t *M, cell_t *top)
{
  cell_t stack;
  misp_env_stack (M, &stack);
  misp_list_sub (M, M->env, top, 5 + LIST_LEN (stack), LIST_LEN (M->env));
}

void
misp_env_begin (misp_t *M, cell_t node, cell_t args, cell_t trap)
{
  cell_t newenv;
  misp_env_top (M, &newenv);
  misp_list_set (M, newenv, M->env, 0); // parent
  misp_list_set (M, newenv, node, 1);   // node
  misp_list_set (M, newenv, args, 2);   // args

  cell_t stack;
  misp_list_sub (M, newenv, &stack, 5, 5);

  misp_list_set (M, newenv, stack, 3); // stack
  misp_list_set (M, newenv, trap, 4);  // trap

  M->env = newenv;
}

void
misp_env_ret (misp_t *M, cell_t ret)
{
  cell_t parent;
  misp_env_parent (M, &parent); // jump to parent
  M->env = parent;
  if (!LIST_LEN (M->env))
    {
      M->halted = true;
      return;
    }
  misp_env_push (M, ret);
}

void misp_debug (misp_t *M, cell_t c);

#define check_is_num(M, node, c)                                              \
  {                                                                           \
    if (!IS_NUM (c))                                                          \
      {                                                                       \
        M->halted = true;                                                     \
        M->panic_code = (misp_panic_t){ MISP_PANIC_TYPE_ERROR, node };        \
        return;                                                               \
      }                                                                       \
  }

#define check_is_list(M, node, c)                                             \
  {                                                                           \
    if (!IS_LIST (c))                                                         \
      {                                                                       \
        M->halted = true;                                                     \
        M->panic_code = (misp_panic_t){ MISP_PANIC_TYPE_ERROR, node };        \
        return;                                                               \
      }                                                                       \
  }

#define check_is_in_bounds(M, node, c, idx)                                   \
  {                                                                           \
    if (NUM_VAL (idx) >= LIST_LEN (c))                                        \
      {                                                                       \
        M->halted = true;                                                     \
        M->panic_code = (misp_panic_t){ MISP_PANIC_OUT_OF_BOUNDS, node };     \
        return;                                                               \
      }                                                                       \
  }

#define check_param_count(M, params, len)                                     \
  if (LIST_LEN (params) len)                                                  \
    {                                                                         \
      M->halted = true;                                                       \
      M->panic_code = (misp_panic_t){ MISP_PANIC_BAD_NODE_PARAMS, node };     \
      return;                                                                 \
    }

#define eval(M, c)                                                            \
  if (IS_LIST (c))                                                            \
    {                                                                         \
      cell_t args, trap;                                                      \
      misp_env_args (M, &args);                                               \
      misp_env_trap (M, &trap);                                               \
      misp_env_begin (M, c, args, trap);                                      \
      return;                                                                 \
    }                                                                         \
  else                                                                        \
    {                                                                         \
      misp_env_push (M, c);                                                   \
      return;                                                                 \
    }

#define eval_params(M, params, stack)                                         \
  if (LIST_LEN (stack) < LIST_LEN (params))                                   \
    {                                                                         \
      cell_t param;                                                           \
      misp_list_get (M, params, &param, LIST_LEN (stack));                    \
      eval (M, param);                                                        \
    }

#define IS_TRUE(c) ((IS_NUM (c) && NUM_VAL (c)) || LIST_LEN (c))
#define PANIC(code, node)                                                     \
  (misp_panic_t) { code, node }

void
misp_init (misp_t *M, uint8_t *mem, size_t mem_size, cell_t init)
{
  M->mem = mem;
  M->mem_size = mem_size;

  M->halted = false;
  M->trapped = false;
  M->panic_code = PANIC (MISP_PANIC_NO, LIST_NULL);

  M->env = LIST (400, 200);

  misp_env_begin (M, init, LIST (mem_size / CELL_SIZE, 0), LIST_NULL);
  misp_list_set (M, M->env, LIST_NULL, 0);
}

static int64_t
do_numop (uint64_t op, int64_t a, int64_t b)
{
  switch (op)
    {
    case MISP_OPC_NADD:
      return a + b;
    case MISP_OPC_NSUB:
      return a - b;
    case MISP_OPC_NMUL:
      return a * b;
    case MISP_OPC_NDIV:
      return a / b;
    case MISP_OPC_NREM:
      return a % b;
    case MISP_OPC_NMOD:
      return (a % b + b) % b;
    case MISP_OPC_NAND:
      return a & b;
    case MISP_OPC_NOR:
      return a | b;
    case MISP_OPC_NXOR:
      return a ^ b;
    case MISP_OPC_NLSR:
      return a < b;
    case MISP_OPC_NGRT:
      return a > b;
    case MISP_OPC_NLSREQ:
      return a <= b;
    case MISP_OPC_NGRTEQ:
      return a >= b;
    }
  return 0;
}

void
misp_execute (misp_t *M)
{
  if (M->halted)
    {
      return;
    }

  cell_t node, stack;
  misp_env_node (M, &node);
  misp_env_stack (M, &stack);

  if (!IS_LIST (node) || LIST_LEN (node) < 1)
    {
      M->halted = true;
      M->panic_code = (misp_panic_t){ MISP_PANIC_BAD_NODE, node };
      return;
    }

  cell_t op, params;
  misp_list_get (M, node, &op, 0);
  misp_list_sub (M, node, &params, 1, LIST_LEN (node));

  if (IS_NUM (op))
    {
      uint64_t opc = NUM_VAL (op);
      if (opc >= 20 && opc <= 32)
        {
          cell_t ret, a, b;

          eval_params (M, params, stack);
          misp_env_get (M, &a, 0);
          misp_env_get (M, &b, 1);

          check_is_num (M, node, a);
          check_is_num (M, node, b);

          ret = NUM (do_numop (opc, NUM_VAL (a), NUM_VAL (b)));

          misp_env_ret (M, ret);
          return;
        }
      switch (opc)
        {
        case MISP_OPC_QUOTE: // return unevaluated argument
          {
            cell_t ret;

            misp_list_get (M, params, &ret, 0);

            misp_env_ret (M, ret);
          }
          break;
        case MISP_OPC_DO:
          {
            cell_t ret;
            eval_params (M, params, stack);
            misp_env_get (M, &ret, LIST_LEN (stack) - 1);
            misp_env_ret (M, ret);
          }
          break;
        case MISP_OPC_LET:
          {
            cell_t ret, binds, body;
            misp_list_sub (M, params, &binds, 0, LIST_LEN (params) - 1);
            eval_params (M, binds, stack);

            if (LIST_LEN (binds) == LIST_LEN (stack))
              {
                misp_list_get (M, params, &body, LIST_LEN (params) - 1);
                misp_env_begin (M, body, stack, LIST_NULL);
              }
            else
              {
                misp_env_get (M, &ret, LIST_LEN (stack) - 1);
                misp_env_ret (M, ret);
              }
          }
          break;
        case MISP_OPC_GET:
          {
            cell_t ret, args, idx;
            eval_params (M, params, stack);
            misp_env_get (M, &idx, 0);
            misp_env_args (M, &args);

            check_is_num (M, node, idx);
            check_is_in_bounds (M, node, args, idx);

            misp_list_get (M, args, &ret, NUM_VAL (idx));

            misp_env_ret (M, ret);
          }
          break;
        case MISP_OPC_SET:
          {
            cell_t args, idx, cell;
            eval_params (M, params, stack);
            misp_env_get (M, &idx, 0);
            misp_env_get (M, &cell, 1);
            misp_env_args (M, &args);

            check_is_num (M, node, idx);
            check_is_in_bounds (M, node, args, idx);

            misp_list_set (M, args, cell, NUM_VAL (idx));

            misp_env_ret (M, cell);
          }
          break;
        case MISP_OPC_NNOT:
          {
            cell_t ret, cell;
            eval_params (M, params, stack);
            misp_env_get (M, &cell, 0);

            check_is_num (M, node, cell);

            ret = NUM (~NUM_VAL (cell));

            misp_env_ret (M, cell);
          }
          break;
        case MISP_OPC_COND:
          {
            cell_t ret, condbd, cond;
            eval_params (M, params, stack);
            misp_env_get (M, &condbd, 0);

            switch (LIST_LEN (stack) - LIST_LEN (params))
              {
              case 0:
                {
                  eval (M, condbd);
                }
                break;
              case 1:
                {
                  cell_t bd;
                  misp_env_get (M, &cond, 3);
                  if (IS_TRUE (cond))
                    {
                      misp_env_get (M, &bd, 1);
                    }
                  else
                    {
                      misp_env_get (M, &bd, 2);
                    }
                  eval (M, bd);
                }
                break;
              case 2:
                {
                  misp_env_get (M, &ret, 4);
                  misp_env_ret (M, ret);
                }
                break;
              }
          }
          break;
        case MISP_OPC_LOOP:
          {
            cell_t ret, cond_body, body;
            eval_params (M, params, stack);
            misp_env_get (M, &cond_body, 0);
            misp_env_get (M, &body, 1);

            switch (LIST_LEN (stack) - LIST_LEN (params))
              {
              case 0:
                {
                  eval (M, cond_body);
                }
                break;
              case 1:
                {
                  cell_t cond;
                  misp_env_get (M, &cond, 2);
                  if (IS_TRUE (cond))
                    {
                      eval (M, body);
                    }
                  else
                    {
                      misp_env_ret (M, LIST (0, 0));
                    }
                }
                break;
              case 2:
                {
                  misp_env_pop (M, 2);
                }
                break;
              }
          }
          break;
        case MISP_OPC_DBUG:
          {
            cell_t cell;
            eval_params (M, params, stack);
            misp_env_get (M, &cell, 0);

            misp_debug (M, cell);
            printf ("\n");

            misp_env_ret (M, cell);
          }
          break;
        case MISP_OPC_LLEN:
          {
            cell_t ret, list;

            eval_params (M, params, stack);
            misp_env_get (M, &list, 0);

            check_is_list (M, node, list);

            ret = NUM (LIST_LEN (list));

            misp_env_ret (M, ret);
          }
          break;
        case MISP_OPC_LSUB:
          {
            cell_t ret, list, a, b;

            eval_params (M, params, stack);
            misp_env_get (M, &list, 0);

            check_is_list (M, node, list);
            check_is_num (M, node, a);
            check_is_num (M, node, b);
            check_is_in_bounds (M, node, list, a);
            if (NUM_VAL (b) > LIST_LEN (list))
              {
                M->halted = true;
                M->panic_code
                    = (misp_panic_t){ MISP_PANIC_OUT_OF_BOUNDS, node };
                return;
              }

            misp_list_sub (M, list, &ret, NUM_VAL (a), NUM_VAL (b));

            misp_env_ret (M, ret);
          }
          break;
        case MISP_OPC_LGET:
          {
            cell_t ret, list, idx;

            eval_params (M, params, stack);

            misp_env_get (M, &list, 0);
            misp_env_get (M, &idx, 1);

            check_is_list (M, node, list);
            check_is_num (M, node, idx);
            check_is_in_bounds (M, node, list, idx);

            misp_list_get (M, list, &ret, NUM_VAL (idx));

            misp_env_ret (M, ret);
          }
          break;
        case MISP_OPC_LSET:
          {
            cell_t ret, list, idx, cell;

            eval_params (M, params, stack);

            misp_env_get (M, &list, 0);
            misp_env_get (M, &idx, 1);
            misp_env_get (M, &cell, 2);

            check_is_list (M, node, list);
            check_is_num (M, node, idx);
            check_is_in_bounds (M, node, list, idx);

            misp_list_set (M, list, cell, NUM_VAL (idx));

            misp_env_ret (M, ret);
          }
          break;
        case MISP_OPC_EQ:
          {
            cell_t ret, a, b;

            eval_params (M, params, stack);

            misp_env_get (M, &a, 0);
            misp_env_get (M, &b, 1);
            if (IS_LIST (a))
              {
                check_is_list (M, node, b);

                ret = NUM ((LIST_LEN (a) == LIST_LEN (b)
                            && LIST_PTR (a) == LIST_PTR (b)));
              }
            else
              {
                check_is_num (M, node, b);
                ret = NUM (NUM_VAL (a) == NUM_VAL (b));
              }

            misp_env_ret (M, ret);
          }
          break;
        case MISP_OPC_EQN:
          {
            cell_t ret, a, b;

            eval_params (M, params, stack);

            misp_env_get (M, &a, 0);
            misp_env_get (M, &b, 1);
            if (IS_LIST (a))
              {
                check_is_list (M, node, b);

                ret = NUM (!(LIST_LEN (a) == LIST_LEN (b)
                             && LIST_PTR (a) == LIST_PTR (b)));
              }
            else
              {
                check_is_num (M, node, b);
                ret = NUM (!(NUM_VAL (a) != NUM_VAL (b)));
              }

            misp_env_ret (M, ret);
          }
          break;
        case MISP_OPC_EVAL:
          {
            cell_t ret, cell;
            check_param_count (M, params, == 1);
            eval_params (M, params, stack);
            misp_env_get (M, &cell, 0);
            if (LIST_LEN (stack) == 1)
              {
                eval (M, cell);
              }
            else
              {
                misp_env_get (M, &ret, 1);
                misp_env_ret (M, ret);
              }
          }
          break;
        default:
          {

            M->halted = true;
            M->panic_code = (misp_panic_t){ MISP_PANIC_INVALID_OPC, node };
            return;
          }
          break;
        }
    }
}

void
misp_debug (misp_t *M, cell_t c)
{
  static int depth = 0;
  depth++;
  bool shorthand = depth >= 3;

  if (IS_NUM (c))
    {
      printf ("%lu", NUM_VAL (c));
    }
  else
    {

      printf ("(");
      if (!shorthand)
        {
          for (int i = 0; i < LIST_LEN (c); i++)
            {
              if (i)
                {
                  printf (" ");
                }
              if (i > 10)
                {
                  printf ("...");
                  break;
                }
              cell_t s;
              misp_list_get (M, c, &s, i);
              misp_debug (M, s);
            }
        }
      else
        {
          printf ("%lu:0x%lx", LIST_LEN (c), LIST_PTR (c));
        }
      printf (")");
    }
  depth--;
}

void
misp_debug_env (misp_t *M)
{
  cell_t node, args, stack;
  misp_env_node (M, &node);
  misp_env_args (M, &args);
  misp_env_stack (M, &stack);

  printf ("NODE: ");
  misp_debug (M, node);
  printf ("\n");
  printf ("ARGS: ");
  misp_debug (M, args);
  printf ("\n");
  printf ("STACK: ");
  misp_debug (M, stack);
  printf ("\n");
}

int
main (int argc, const char *argv[])

{
  bool debug = false;
  if (argc < 2)
    {
      printf ("MISP [-v] [-d] input\n");
      return 0;
    }
  for (int i = 1; i < argc; i++)
    {
      if (!strcmp ("-d", argv[i]) || !strcmp ("--debug", argv[i]))
        {
          debug = true;
        }
      else if (!strcmp ("-v", argv[i]) || !strcmp ("--version", argv[i]))
        {
          printf ("MISP %s\n", MISP_VERSION);
          printf ("Copyright (C) 2023\n"
                  "License GPLv3+: GNU GPL version 3 or later "
                  "<https://gnu.org/licenses/gpl.html>\n"
                  "This is free software: you are free to change and "
                  "redistribute it.\n"
                  "There is NO WARRANTY, to the extent permitted by law.\n");

          return 0;
        }
    }

  const char *input_path = argv[argc - 1];

  FILE *input_file = fopen (input_path, "rb");
  if (!input_file)
    {
      fprintf (stderr, "Cannot find file %s\n", input_path);
      return -1;
    }
  fseek (input_file, 0, SEEK_END);
  size_t input_size = ftell (input_file);
  fseek (input_file, 0, SEEK_SET);

  char *input = malloc (input_size);
  fread (input, input_size, 1, input_file);
  fclose (input_file);

  cell_t init;
  uint8_t *code;
  size_t code_size;
  misp_parse_string (input, &code, &code_size, &init);
  printf ("Parsed successfully\n");

  misp_t M;
  size_t mem_size = code_size + 1024 * 9;
  uint8_t *mem = calloc (mem_size, sizeof (uint8_t));

  memcpy (mem, code, code_size);
  misp_init (&M, mem, mem_size, init);
  cell_t thing;
  misp_list_get (&M, M.env, &thing, 3);

  if (debug)
    {
      misp_debug_env (&M);
    }
  while (!M.halted)
    {
      char i;
      if (debug)
        {
          scanf ("%c", &i);
          system ("clear");
        }
      misp_execute (&M);
      if (debug)
        {
          misp_debug_env (&M);
        }
    }
  if (M.panic_code.type)
    {
      printf ("PANIC: %d\n", M.panic_code.type);
      misp_debug_env (&M);
    }
  free (input);
  free (code);

  return 0;
}
