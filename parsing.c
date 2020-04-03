#include "mpc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LASSERT(args, cond, err)                                               \
  if (!(cond)) {                                                               \
    lval_del(args);                                                            \
    return lerr(err);                                                          \
  }
/*if we are compiling on Windows compile these functions*/
#ifdef _WIN32

static char buffer[2048];

char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

void add_history(char *unused) {}

#endif
#ifdef __linux__
#include <readline/history.h>
#include <readline/readline.h>
#endif

typedef struct lval {
  int type;
  long num;
  /* Error and Symbol types have some string data */
  char *err;
  char *sym;
  /* Count and a Pointer to a list of "lval*" */
  int count;
  struct lval **cell;
} lval;

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

/*Construct a pointer to a new Number lval*/
lval *lnum(long val) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = val;
  return v;
}

/*Construct a pointer to a new Qexpr lval*/
lval *lqexpr() {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/*Construct a pointer to a new Error lval*/
lval *lerr(char *err) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(sizeof(strlen(err) + 1));
  strcpy(v->err, err);
  return v;
}

/*Construct a pointer to a new Symbol lval*/
lval *lsym(char *sym) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(sizeof(strlen(sym) + 1));
  strcpy(v->sym, sym);
  return v;
}

/*Construct a pointer to a new Sexpr lval*/
lval *lsexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    break;
  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;
  case LVAL_QEXPR:
  case LVAL_SEXPR:

    /*If Sexpr delete all elements inside*/
    for (int i = 0; i < v->count; ++i) {
      lval_del(v->cell[i]);
    }
    /* Also free the memory allocated to the pointers */
    free(v->cell);
    break;
  }
  /* Free the memory allocated for the "lval" struct itself */
  free(v);
}

lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lnum(x) : lerr("invalid number");
}

lval *lval_add(lval *v, lval *x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}
void lval_println(lval *);
lval *lval_read(mpc_ast_t *t) {
  if (strstr(t->tag, "number")) {
    return lval_read_num(t);
  }
  if (strstr(t->tag, "symbol")) {
    return lsym(t->contents);
  }
  /*If root (>) or sexpr then create empty list*/
  lval *x = NULL;
  if (strstr(t->tag, "qexpr")) {
    x = lqexpr();
  }
  if (strcmp(t->tag, ">") == 0) {
    x = lsexpr();
  }
  if (strstr(t->tag, "sexpr")) {
    x = lsexpr();
  }
  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; ++i) {
    if (strcmp(t->children[i]->contents, "(") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->contents, ")") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->contents, "{") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->contents, "}") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->tag, "regex") == 0) {
      continue;
    }
    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}

void lval_print(lval *val); // Foward declaration

void lval_expr_print(lval *v, char *open, char *close) {
  printf("%s", open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
    /*Don't print trailing space if last element*/
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  printf("%s", close);
}

void lval_print(lval *val) {
  switch (val->type) {
  case LVAL_NUM:
    printf("%li", val->num);
    break;
  case LVAL_ERR:
    printf("Error: %s", val->err);
    break;
  case LVAL_SYM:
    printf("%s", val->sym);
    break;
  case LVAL_QEXPR:
    lval_expr_print(val, "{ ", " }");
    break;
  case LVAL_SEXPR:
    lval_expr_print(val, "( ", " )");
    break;
  default:
    printf("Error: Unknown value!");
    break;
  }
}

// print a lispy value followed by a newline
void lval_println(lval *val) {
  lval_print(val);
  putchar('\n');
}

lval eval(lval *);

lval *lval_eval_sexpr(lval *v);

lval *lval_eval(lval *v);
;
int main(int argc, char **argv) {
  // define Polish grammar

  /*The symbols for evaluating QExpr are defined below
   * 	list: Takes one or more arguments and returns a new Q-Expression
   * containing the arguments head: Takes a Q-Expression and returns a
   * Q-Expression with only the first element tail: Takes a Q-Expression and
   * returns a Q-Expression with the first element removed join: Takes one or
   * more Q-Expressions and returns a Q-Expression of them conjoined together
   * 	eval: Takes a Q-Expression and evaluates it as if it were a S-Expression
   *
   * */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");
  mpca_lang(MPCA_LANG_DEFAULT,
            "						\
			number: /-?[0-9]+/; 				\
			symbol: '+' | '-' | '/' | '*' | '%' | \"head\" | \"tail\" | \"join\" | \"eval\" | \"list\";  \
			sexpr: '(' <expr>* ')' ;			\
			qexpr: '{' <expr>* '}' ;									\
			expr: <number> | <symbol> | <sexpr> | <qexpr> ;	\
			lispy: /^/ <expr>* /$/; 		\
			",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to exit\n");

  while (1) {
    char *input = readline("> ");
    add_history(input);
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval *result = lval_eval(lval_read(r.output));
      lval_println(result);
      lval_del(result);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }
  /*Undefine and Delete our Parsers*/
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}

lval *lval_eval(lval *v) {
  /* Evaluate Sexpression */
  if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(v);
  }
  return v;
}

/* Pop an item from the list */
lval *lval_pop(lval *v, int i) {
  /* Find the item at i */
  lval *x = v->cell[i];

  /* Shift memory after the item at "i" over the top */
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval *) * (v->count - i - 1));
  /* Decrease the count of items in the list */
  v->count--;

  /* Reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  return x;
}

/* Take an item from the list and delete the rest */
lval *lval_take(lval *v, int i) {
  lval *x = lval_pop(v, i);
  lval_del(v);
  return x;
}

/* Evaluate the given lval and return the result */
lval *builtin_op(lval *v, char *sym) {
  /*Make sure we have numbers only*/
  for (int i = 0; i < v->count; ++i) {
    if (v->cell[i]->type != LVAL_NUM) {
      lval_del(v);
      return lerr("Invalid operand. Expected numbers only");
    }
  }

  /* Make sure we have operands*/
  if (v->count == 0) {
    lval_del(v);
    return lerr("Operator has no operands");
  }

  /*We cannot perform binary %*/
  if (v->count > 2 && (strcmp(sym, "%") == 0)) {
    lval_del(v);
    return lerr("Modulo support is only binary");
  }

  /*Take the first cell from v*/
  lval *x = lval_pop(v, 0);
  if (v->count == 0 && (strcmp(sym, "-") == 0)) {
    lval_del(v);
    x->num = -x->num;
    return x;
  }

  while (v->count > 0) {
    lval *y = lval_pop(v, 0);
    if (strcmp(sym, "+") == 0) {
      x->num += y->num;
    }
    if (strcmp(sym, "*") == 0) {
      x->num *= y->num;
    }
    if (strcmp(sym, "/") == 0) {
      if (y->num == 0)
        return lerr("Division by zero");
      else
        x->num /= y->num;
    }
    if (strcmp(sym, "-") == 0) {
      x->num -= y->num;
    }
    if (strcmp(sym, "%") == 0) {
      if (y->num == 0)
        return lerr("Division by zero");
      else
        x->num %= y->num;
    }
    lval_del(y);
  }

  lval_del(v);
  return x;
}

lval *builtin_head(lval *a) {

  /*Check Error Conditions*/
  LASSERT(a, a->count == 1, "Function 'head' passed to many arguments");

  /*Check for valid type(QExpr*/
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'head' passed incorrect type");

  /*Ensure Qexpr is not empty*/
  LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}");

  /*Otherwise take first argument*/
  lval *v = lval_take(a, 0);

  /*Delete all elements that are not head and return*/
  while (v->count > 1) {
    lval_del(lval_pop(v, 1));
  }

  return v;
}

lval *builtin_tail(lval *a) {
  /*Check Error Conditions*/
  LASSERT(a, a->count == 1, "Function 'tail' passed to many arguments");

  /*Check for valid type(QExpr*/
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'tail' passed incorrect type");

  /*Ensure Qexpr is not empty*/
  LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}");

  /*Take the first element*/
  lval *v = lval_take(a, 0);

  /*Delete the first element and return*/
  lval_del(lval_pop(v, 0));

  return v;
}

lval *builtin_list(lval *a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval *builtin_eval(lval *a) {
  /*Check Error Conditions*/
  LASSERT(a, a->count == 1, "Function 'eval' passed to many arguments");

  /*Check for valid type(QExpr)*/
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'eval' passed incorrect type");

  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

lval *lval_join(lval *x, lval *y) {
  /*For each cell in 'y' add it to 'x'*/
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  /*Delete the empty 'y' and return 'x'*/
  lval_del(y);
  return x;
}

lval *builtin_join(lval *a) {
  for (int i = 0; i < a->count; ++i) {
    LASSERT(a->cell[i], a->cell[i]->type == LVAL_QEXPR,
            "function 'join' passed incorrect type.");
  }

  lval *x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval *builtin(lval *a, char *func) {
  if (strcmp("list", func) == 0) {
    return builtin_list(a);
  }
  if (strcmp("tail", func) == 0) {
    return builtin_tail(a);
  }
  if (strcmp("head", func) == 0) {
    return builtin_head(a);
  }
  if (strcmp("eval", func) == 0) {
    return builtin_eval(a);
  }
  if (strcmp("join", func) == 0) {
    return builtin_join(a);
  }
  if (strstr("+-/*%", func)) {
    return builtin_op(a, func);
  }
  lval_del(a);
  return lerr("Unknown function");
}

lval *lval_eval_sexpr(lval *v) {
  /*Evaluate Children*/
  for (int i = 0; i < v->count; ++i) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  /*Error Checking */
  for (int i = 0; i < v->count; ++i) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  /*Empty Expression*/
  if (v->count == 0) {
    return v;
  }

  /*Single Expression*/
  if (v->count == 1) {
    return lval_take(v, 0);
  }

  /*Ensure First Element is a Symbol */
  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    return lerr("S-expression Does not start with a symbol");
  }

  /* Call builtin with operator */
  lval *result = builtin(v, f->sym);
  lval_del(f);
  return result;
}
