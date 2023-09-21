#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <term.h>

enum type { NUMBER, FUNC, OP };

enum func { COS, SIN, TAN, ACOS, ASIN, ATAN };

enum ops { LEFT_BRACKET, RIGHT_BRACKET, PLUS, MINUS, TIMES, DIVIDE };

struct token {
  enum type t;
  union {
    double number;
    enum ops op;
    enum func function;
  } data;
  struct token *next_tok;
  struct token *prev_tok;
};

// Create a new token, and update last_token.
// Handles the case where *token_list is null.
// TODO: benchmark this, which is faster:
// pass pointer to new
// pass arg on stack
// inline

struct token_list {
  struct token *first;
  struct token *last;
};

static inline void token_append(struct token_list *token_list,
                                struct token new) {
  if (token_list->first == NULL) {
    // List has no items yet!
    token_list->first = malloc(sizeof(struct token));
    token_list->last = token_list->first;
    *(token_list->first) = new;
    token_list->last->next_tok = NULL;
    token_list->last->prev_tok = NULL;
  } else {
    assert(token_list->last);
    struct token *new_alloc = malloc(sizeof(struct token));
    (*new_alloc) = new;
    struct token *old_last = token_list->last;
    token_list->last->next_tok = new_alloc;
    token_list->last = new_alloc;
    token_list->last->next_tok = NULL;
    token_list->last->prev_tok = old_last;
  }
}

static inline void token_delete(struct token_list *token_list,
                                struct token *token) {
  if (token->next_tok) {
    token->next_tok->prev_tok = token->prev_tok;
  }
  if (token->prev_tok) {
    token->prev_tok->next_tok = token->next_tok;
  }

  if (token_list->first == token) {
    token_list->first = token->next_tok;
  }

  if (token_list->last == token) {
    token_list->last = token->prev_tok;
  }

  free(token);
}

// get an enum ops from a char
static inline enum ops op_get_from_chr(char chr) {
  switch (chr) {
  case '(':
    return LEFT_BRACKET;
  case ')':
    return RIGHT_BRACKET;
  case '+':
    return PLUS;
  case '-':
    return MINUS;
  case '*':
    return TIMES;
  case '/':
    return DIVIDE;
  }
  assert(0);
}

// This could be a while loop, but this macro would need to insert the
// assignment in the end somehow The extra } is so this macro can be used like
// ITERATE_OVER_TOKENS {
#define ITERATE_OVER_TOKENS(current_token)                                     \
  for (check_again = false, current_token = token_list->first;;                \
       current_token = check_again ? current_token : current_token->next_tok,  \
      check_again = false)

#define TOKEN_ITERATE_CHECK(current_token, stop_at_rbracket)                   \
  if (current_token == NULL) {                                                 \
    if (stop_at_rbracket) {                                                    \
      puts("Syntax error: unclosed (.");                                       \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
    break;                                                                     \
  } else if (current_token->t == OP &&                                         \
             current_token->data.op == RIGHT_BRACKET) {                        \
    if (stop_at_rbracket) {                                                    \
      break;                                                                   \
    } else {                                                                   \
      puts("Syntax error: unexpected ).");                                     \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
    break;                                                                     \
  }

#define ARGS_N(M, ...) ARGS_N__(__VA_ARGS__, 6, 5, 4, 3, 2, 1)(M, __VA_ARGS__)

#define ARGS_N__(_1, _2, _3, _4, _5, _6, X, ...) ARGS_##X

#define ARGS_1(M, X) M(X)
#define ARGS_2(M, X, ...) M(X) || ARGS_1(M, __VA_ARGS__)
#define ARGS_3(M, X, ...) M(X) || ARGS_2(M, __VA_ARGS__)
#define ARGS_4(M, X, ...) M(X) || ARGS_3(M, __VA_ARGS__)
#define ARGS_5(M, X, ...) M(X) || ARGS_4(M, __VA_ARGS__)
#define ARGS_6(M, X, ...) M(X) || ARGS_5(M, __VA_ARGS__)

#define TOK_CHECK(X) next_tok->data.op == X

#define TOK_CHECK_CONDITION(...) ARGS_N(TOK_CHECK, __VA_ARGS__)

#define TOKEN_CONTINUE_IF_NOT_INFIX(...)                                       \
  if (current_token->t != NUMBER)                                              \
    continue;                                                                  \
                                                                               \
  struct token *next_tok = current_token->next_tok;                            \
  if (next_tok == NULL ||                                                      \
      !(next_tok->t == OP && (TOK_CHECK_CONDITION(__VA_ARGS__))))              \
    continue;                                                                  \
                                                                               \
  enum ops infix_op = next_tok->data.op;                                       \
  struct token *next_next_tok = next_tok->next_tok;                            \
  if (next_next_tok == NULL || next_next_tok->t != NUMBER) {                   \
    puts("Invalid RHS for infix op.");                                         \
    exit(EXIT_FAILURE);                                                        \
  }

#define TOKEN_INFIX_LOOP_END()                                                 \
  token_delete(token_list, next_next_tok);                                     \
  token_delete(token_list, next_tok);                                          \
  check_again = true;

void parse_tokens(struct token_list *token_list, bool stop_at_rbracket) {
  struct token *current_token;
  bool check_again;

  // Parse functions, rbracket closes function, add OP=comma

  // First, evaluate brackets.
  ITERATE_OVER_TOKENS(current_token) {
    TOKEN_ITERATE_CHECK(current_token, stop_at_rbracket);
    if (current_token->t == OP && current_token->data.op == LEFT_BRACKET) {
      if (current_token->next_tok == NULL) {
        puts("Syntax error: ) expected.");
        exit(EXIT_FAILURE);
      }
      struct token_list list_copy = *token_list;
      list_copy.first = current_token->next_tok;
      parse_tokens(&list_copy, true);

      assert(current_token->next_tok && current_token->next_tok->t == NUMBER);
      assert(current_token->next_tok->next_tok &&
             current_token->next_tok->next_tok->t == OP &&
             current_token->next_tok->next_tok->data.op == RIGHT_BRACKET);
      // Delete left and right brackets, keep only result.
      token_delete(token_list, current_token->next_tok->next_tok);
      struct token *next_tok = current_token->next_tok;
      token_delete(token_list, current_token);
      current_token = next_tok;
    }
  }

  // Call functions

  // Apply prefix +/- operators
  ITERATE_OVER_TOKENS(current_token) {
    TOKEN_ITERATE_CHECK(current_token, stop_at_rbracket);
    if (current_token->t == OP &&
        (current_token->data.op == PLUS || current_token->data.op == MINUS)) {
      // If the previous token does not exist or is not a number, and the next
      // token is a number then this is a prefix operator
      // We could probably report errors like plus as the last token here, but
      // they will get checked next
      if (current_token->prev_tok == NULL ||
          current_token->prev_tok->t != NUMBER) {
        struct token *next_tok = current_token->next_tok;
        if (next_tok != NULL && next_tok->t == NUMBER) {
          if (current_token->data.op == MINUS)
            next_tok->data.number *= -1;

          token_delete(token_list, current_token);
        }
      }
    }
  }

  // Apply infix *//
  ITERATE_OVER_TOKENS(current_token) {
    TOKEN_ITERATE_CHECK(current_token, stop_at_rbracket);
    TOKEN_CONTINUE_IF_NOT_INFIX(TIMES, DIVIDE);

    if (infix_op == TIMES) {
      current_token->data.number *= next_next_tok->data.number;
    } else {
      current_token->data.number /= next_next_tok->data.number;
    }
    TOKEN_INFIX_LOOP_END();
  }

  // Apply infix +/-
  ITERATE_OVER_TOKENS(current_token) {
    TOKEN_ITERATE_CHECK(current_token, stop_at_rbracket);
    TOKEN_CONTINUE_IF_NOT_INFIX(PLUS, MINUS);

    if (infix_op == PLUS) {
      current_token->data.number += next_next_tok->data.number;
    } else {
      current_token->data.number -= next_next_tok->data.number;
    }
    TOKEN_INFIX_LOOP_END();
  }
}

int main() {
  struct token_list token_list = {.first = NULL, .last = NULL};
  unsigned int current_chr;

  struct termios term, term_bak;
  tcgetattr(fileno(stdin), &term);

  term_bak = term;

  term.c_lflag &= ~ECHO;
  term.c_lflag &= ~ICANON;
  tcsetattr(fileno(stdin), 0, &term);

  int token_idx = 0;
  while ((current_chr = getchar()) != '\n') {
    switch (current_chr) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
      putchar(current_chr);
      current_chr -= 48;
      if (token_list.last && token_list.last->t == NUMBER) {
        token_list.last->data.number =
            token_list.last->data.number * 10 + current_chr;
      } else {
        token_idx++;
        token_append(
            &token_list,
            (struct token){.t = NUMBER, .data = {.number = current_chr}});
      }
      break;
    }
    case '(':
    case ')':
    case '+':
    case '-':
    case '*':
    case '/':
      putchar(current_chr);
      token_append(&token_list,
                   (struct token){
                       .t = OP, .data = {.op = op_get_from_chr(current_chr)}});
      break;
    }
    fflush(stdout);
  }
  puts("");
  tcsetattr(fileno(stdin), 0, &term_bak);

  parse_tokens(&token_list, false);

  assert(token_list.first == token_list.last && token_list.first->t == NUMBER);
  printf("%f\n", token_list.first->data.number);
}
