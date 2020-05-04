#include "mpc.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LASSERT(args, cond, fmt, ...)                                          \
  if (!(cond)) {                                                               \
    lval *err = lerr(fmt, ##__VA_ARGS__);                                      \
    lval_del(args);                                                            \
    return err;                                                                \
  }

#define LASSERT_TYPE(func, expected, index, args)                              \
  if (!(args->cell[index]->type == expected)) {                                \
    lval *err =                                                                \
        lerr("Function '%s' passed incorrect type: %s\nExpected QExpressions", \
             func, ltype_name(a->cell[0]->type), ltype_name(expected));      \
    lval_del(args);                                                            \
    return err;                                                                \
  }

#define LASSERT_NUM(func, num, type, args)                                     \
  if (!(args->count == num)) {                                                 \
    lval *err = lerr("Function '%s' passed incorrect number of arguments: "    \
                     "%d\nExpected %d %s",                                     \
                     func, a->count, num, type);                               \
    lval_del(args);                                                            \
    return err;                                                                \
  }

#define STR_ERR_SIZE 512
/*if we are compiling on Windows compile these functions*/
#ifdef _WIN32

#define STR_BUFF_SIZE 2048
static char buffer[STR_BUFF_SIZE];

char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, STR_BUFF_SIZE, stdin);
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
/*Foward Declarations*/
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval *(*lbuiltin)(lenv *, lval *);
struct lval {
  int type;
  long num;
  /* Error and Symbol types have some string data */
  char *err;
  char *sym;
  lbuiltin fun;
  /* Count and a Pointer to a list of "lval*" */
  int count;
  struct lval **cell;
};

struct lenv {
  int count;
  char **syms;
  lval **vals;
};

enum { LVAL_NUM, LVAL_FUN, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

char *ltype_name(int val) {
  switch (val) {
  case LVAL_ERR:
    return "Error";
    break;
  case LVAL_FUN:
    return "Function";
    break;
  case LVAL_SYM:
    return "Symbol";
    break;
  case LVAL_SEXPR:
    return "Symbolic Expression";
    break;
  case LVAL_QEXPR:
    return "QExpression";
    break;
  case LVAL_NUM:
    return "Number";
    break;
  default:
    break;
  }
  return "Unknown type";
}

/*Construct a pointer to a new Number lval*/
lval *lnum(long val) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = val;
  return v;
}

/*Construct a pointer to a new Function lval*/
lval *lfun(lbuiltin func) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
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
lval *lerr(char *fmt, ...) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, fmt);

  /*Allocate 512 bytes for our string*/
  v->err = malloc(STR_ERR_SIZE);

  vsprintf(v->err, fmt, va);
  /*Reallocate to actual space used by string*/
  v->err = realloc(v->err, strlen(v->err) + 1);

  va_end(va);
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

lval *lval_copy(lval *v) {
  lval *x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
  /*Copy Function and Numbers Directly*/
  case LVAL_FUN:
    x->fun = v->fun;
    break;
  case LVAL_NUM:
    x->num = v->num;
    break;

  /* Copy Strings using malloc and strcpy */
  case LVAL_ERR:
    x->err = malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err);
    break;
  case LVAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym);
    break;

  /* Copy List by copying each sub-expression */
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval *) * x->count);
    for (int i = 0; i < v->count; ++i) {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }
  return x;
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
  case LVAL_FUN:
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

lenv *lenv_new() {
  lenv *e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv *e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lval *lenv_get(lenv *e, lval *k) {
  /* Iterate over all items in environment */
  for (int i = 0; i < e->count; i++) {
    /*Check if the stored string matches the symbol string */
    /* If it does, return a copy of the value */
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  /* If no symbol found return error */
  return lerr("unbound symbol!");
}

void lenv_put(lenv *e, lval *k, lval *v) {
  /* Iterate over all items in environment */
  /* This is to see if variable already exists */
  for (int i = 0; i < e->count; i++) {
    /* If variable is found delete item at that position */
    /* And replace with variable supplied by user */
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  /* If no existing entry found allocate space for new entry */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval *) * e->count);
  e->syms = realloc(e->syms, sizeof(char *) * e->count);

  /* Copy contents of lval and symbol string into new location */
  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
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

/* Parsing */
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

/* Printing lvals */
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
  case LVAL_FUN:
    printf("<function>");
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

lval *lval_eval_sexpr(lenv *e, lval *v);

lval *lval_eval(lenv *, lval *v);
void lenv_add_builtins(lenv *);

int main(int argc, char **argv) {
  // define Polish grammar

  /*The symbols for evaluating QExpr are defined below
   * list: Takes one or more arguments and returns a new Q-Expression containing
   * the arguments head: Takes a Q-Expression and returns and Q-Expression with
   * only the first element tail: Takes a Q-Expression and returns a
   * Q-Expression with the first element removed join: Takes one or more
   * Q-Expressions and returns a Q-Expression of them conjoined together eval:
   * Takes a Q-Expression and evaluates it as if it were a S-Expression def: Put
   * a new variable into the environment
   *
   * */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");
  mpca_lang(MPCA_LANG_DEFAULT, "						\
			number: /-?[0-9]+/; 				\
			symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;	 \
			sexpr: '(' <expr>* ')' ;			\
			qexpr: '{' <expr>* '}' ;									\
			expr: <number> | <symbol> | <sexpr> | <qexpr> ;	\
			lispy: /^/ <expr>* /$/; 		\
			",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to exit\n");

  lenv *e = lenv_new();
  lenv_add_builtins(e);

  while (1) {
    char *input = readline("> ");
    add_history(input);
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {

      lval *result = lval_eval(e, lval_read(r.output));
      lval_println(result);
      lval_del(result);

      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }
  lenv_del(e);
  /*Undefine and Delete our Parsers*/
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}

lval *lval_eval(lenv *e, lval *v) {
  if (v->type == LVAL_SYM) {
    lval *x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  /* Evaluate Sexpression */
  if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(e, v);
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
lval *builtin_op(lenv *e, lval *v, char *sym) {
  /*Make sure we have numbers only*/
  for (int i = 0; i < v->count; ++i) {
    if (v->cell[i]->type != LVAL_NUM) {
      lval_del(v);
      return lerr("Invalid operand: %s\nExpected numbers only",
                  v->cell[i]->type);
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

lval *builtin_add(lenv *e, lval *a) { return builtin_op(e, a, "+"); }

lval *builtin_sub(lenv *e, lval *a) { return builtin_op(e, a, "-"); }

lval *builtin_mul(lenv *e, lval *a) { return builtin_op(e, a, "*"); }

lval *builtin_div(lenv *e, lval *a) { return builtin_op(e, a, "/"); }

lval *builtin_def(lenv *e, lval *a) {
  LASSERT_TYPE("def", LVAL_QEXPR, 0, a);

  /* First argument is symbol list */
  lval *syms = a->cell[0];

  /* Ensure all elements of first list are symbols */
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
            "Function 'def' cannot define non-symbols");
  }

  /* Check correct number of symbols and values */
  LASSERT(a, syms->count == a->count - 1,
          "Function 'def' cannot define incorrect number of values to symbols");

  /* Assign copies of values to symbols */
  for (int i = 0; i < syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i + 1]);
  }
  lval_del(a);
  return lsexpr();
}

lval *builtin_head(lenv *e, lval *a) {

  /*Check Error Conditions*/
  LASSERT_NUM("tail", 1, "Qexpr", a);

  /*Check for valid type(QExp)r*/
  LASSERT_TYPE("head", LVAL_QEXPR, 0, a);

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

lval *builtin_tail(lenv *e, lval *a) {
  /*Check Error Conditions*/
  LASSERT_NUM("tail", 1, "Qexpr", a);

  /*Check for valid type(QExpr*/
  LASSERT_TYPE("tail", LVAL_QEXPR, 0, a);

  /*Ensure Qexpr is not empty*/
  LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}");

  /*Take the first element*/
  lval *v = lval_take(a, 0);

  /*Delete the first element and return*/
  lval_del(lval_pop(v, 0));

  return v;
}

lval *builtin_list(lenv *e, lval *a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval *builtin_eval(lenv *e, lval *a) {
  /*Check Error Conditions*/
  LASSERT_NUM("eval", 1, "QExpr", a);

  /*Check for valid type(QExpr)*/
  LASSERT_TYPE("eval", LVAL_QEXPR, 0, a);

  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
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

lval *builtin_join(lenv *e, lval *a) {
  /*Ensure that we only have QExpr*/
  for (int i = 0; i < a->count; ++i) {
    LASSERT_TYPE("join", LVAL_QEXPR, i, a);
  }

  lval *x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval *builtin(lenv *e, lval *a, char *func) {
  if (strcmp("list", func) == 0) {
    return builtin_list(e, a);
  }
  if (strcmp("tail", func) == 0) {
    return builtin_tail(e, a);
  }
  if (strcmp("head", func) == 0) {
    return builtin_head(e, a);
  }
  if (strcmp("eval", func) == 0) {
    return builtin_eval(e, a);
  }
  if (strcmp("join", func) == 0) {
    return builtin_join(e, a);
  }
  if (strstr("+-/*%", func)) {
    return builtin_op(e, a, func);
  }
  lval_del(a);
  return lerr("Unknown function");
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
  lval *k = lsym(name);
  lval *v = lfun(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(lenv *e) {
  /* List Functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "def", builtin_def);

  /* Mathematical Functions */

  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
}

lval *lval_eval_sexpr(lenv *e, lval *v) {
  /*Evaluate Children*/
  for (int i = 0; i < v->count; ++i) {
    v->cell[i] = lval_eval(e, v->cell[i]);
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

  /*Ensure First Element is a Function after evaluation */
  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval_del(f);
    lval_del(v);
    return lerr("fist element is not a function");
  }

  /* If so call function to get result */
  lval *result = f->fun(e, v);
  lval_del(f);
  return result;
}
