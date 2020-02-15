#include "mpc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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


enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

/*Construct a pointer to a new Number lval*/
lval *lnum(long val) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = val;
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
  case LVAL_SEXPR:
    
    /*If Sexpr delete all elements inside*/
    for(int i = 0; i < v->count; ++i) {
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



lval *lval_read(mpc_ast_t *t) {
  if (strstr(t->tag, "number")) {
    return lval_read_num(t);
  }
  if (strstr(t->tag, "symbol")) {
    return lsym(t->contents);
  }

  /*If root (>) or sexpr then create empty list*/
  lval *x = NULL;
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
    if (strcmp(t->children[i]->tag, "regex") == 0) {
      continue;
    }
    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}


void lval_print(lval *val); // Foward declaration


void lval_expr_print(lval *v, char* open, char* close) {
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
  case LVAL_SEXPR:
    lval_expr_print(val, "( ", " )");
    break;
  default:
    printf("Error: Unknown value!");
  }
}


// print a lispy value followed by a newline
void lval_println(lval *val) {
  lval_print(val);
  putchar('\n');
}


lval eval(lval*);


//lval eval_op(lval lh, char *op,
//           lval rh); // left hand and right hand of the operator
lval* lval_eval_sexpr(lval* v);
//lval* lval_take(lval* v, int i);
lval* lval_eval(lval* v);
//lval* lval_pop(lval* v, int i);
//lval* builtin_op(lval* v, char *sym);
int main(int argc, char **argv) {
  // define Polish grammar
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");
  mpca_lang(MPCA_LANG_DEFAULT, "						\
			number: /-?[0-9]+/; 				\
			symbol: '+' | '-' | '/' | '*' | '%';  \
			sexpr: '(' <expr>* ')' ;			\
			expr: <number> | <symbol> | <sexpr> ;	\
			lispy: /^/ <expr>* /$/; 		\
			",
            Number, Symbol, Sexpr, Expr, Lispy);
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to exit\n");

  while (1) {
    char *input = readline("> ");
    add_history(input);
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      // lval result = eval(r.output);
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
  mpc_cleanup(4, Number, Symbol, Sexpr, Expr, Lispy);
  return 0;
}

lval* lval_eval(lval* v) {
  /* Evaluate Sexpression */
  if(v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
  return v;
}

/* Pop an item from the list */
lval* lval_pop(lval* v, int i) {
  /* Find the item at i */ 
  lval* x = v->cell[i];

  /* Shift memory after the item at "i" over the top */
  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count -i -1));
  /* Decrease the count of itmes in the list */
  v->count--;

  /* Reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lval* )*v->count);
  return x;
}

/* Take an item from the list and delete the rest */
lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

/* Evaluate the given lval and return the result */
lval* builtin_op(lval* v, char *sym)
{
  lval* result = NULL;
  if(v->count==0){return lerr("Operator has no operands");}
  if(v->count==1 && v->cell[0]->type==LVAL_NUM) {
    if(strcmp(sym, "+")) {return lnum(result->num);}
    else if(strcmp(sym, "-")){return lnum(-result->num);}
    else {
      return lerr(strcat("Cannot perform unary ", sym));
    }
  }
  if(v->count>=1 && v->cell[0]->type!=LVAL_NUM) {
    return lerr("Invalid operand");
  }

  if(v->count>2) {return lerr("Only binary operations are supported"); }
  if (strcmp(sym, "+") == 0) {
    return lnum(v->cell[0]->num + v->cell[1]->num);
  }
  if (strcmp(sym, "*") == 0) {
    return lnum(v->cell[0]->num * v->cell[1]->num);
  }
  if (strcmp(sym, "/") == 0) {
    return v->cell[1]->num == 0 ? lerr("Division by zero") : lnum(v->cell[0]->num / v->cell[1]->num);
  }
  if (strcmp(sym, "-") == 0) {
    return lnum(v->cell[0]->num - v->cell[1]->num);
  }
  if (strcmp(sym, "%") == 0) {
    return v->cell[1]->num == 0 ? lnum(v->cell[0]->num): lnum(v->cell[0]->num % v->cell[1]->num);
  }
   
  return lerr("Invalid operand");
}


lval* lval_eval_sexpr(lval* v)
{
  /*Evaluate Children*/
  for(int i = 0; i<v->count; ++i) {
	v->cell[i] = lval_eval(v->cell[i]);
  }

  /*Error Checking */
  for (int i=0; i<v->count; ++i) {
  	if(v->cell[i]->type== LVAL_ERR) {return lval_take(v, i);}
  }

  /*Empty Expression*/
  if(v->count == 0) {return v;}

  /*Single Expression*/
  if(v->count == 1) {return lval_take(v, 0); }

  /*Ensure First Element is a Symbol */
  lval* f = lval_pop(v, 0);
  if(f->type != LVAL_SYM) {
  	lval_del(f); lval_del(v);
	return lerr("S-expression Does not start with a symbol");
  }


  /* Call builtin with operator */
  lval* result = builtin_op(v, f->sym);
  lval_del(f);
  return result;
}


/*
lval eval(mpc_ast_t *t) {
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lnum(x) : lerr(LERR_BAD_NUM);
  }

  // The operator is always the second child
  char *op = t->children[1]->contents;

  // We store the third child in x
  lval x = eval(t->children[2]);

  int i = 3;
  while (strstr(t->children[i]->tag, "exp")) {
    x = eval_op(x, op, eval(t->children[i]));
    ++i;
  }
  return x;
}
lval eval_op(lval lh, char *op, lval rh) {
  // first check if we've got an error
  if (lh.type == LVAL_ERR) {
    return lh;
  }
  if (rh.type == LVAL_ERR) {
    return rh;
  }

  if (strcmp(op, "+") == 0) {
    return lnum(lh.val + rh.val);
  }
  if (strcmp(op, "*") == 0) {
    return lnum(lh.val * rh.val);
  }
  if (strcmp(op, "/") == 0) {
    return rh.val == 0 ? lerr(LERR_DIV_ZERO) : lnum(lh.val / rh.val);
  }
  if (strcmp(op, "-") == 0) {
    return lnum(lh.val - rh.val);
  }
  if (strcmp(op, "%") == 0) {
    return rh.val == 0 ? lh : lnum(lh.val % rh.val);
  }
  return lerr(LERR_INV_OP);
}
*/
