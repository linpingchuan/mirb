#include "parser.h"

struct node *parse_if(struct parser *parser);
struct node *parse_unless(struct parser *parser);
struct node *parse_ternary_if(struct parser *parser);
struct node *parse_conditional(struct parser *parser);
struct node *parse_case(struct parser *parser);