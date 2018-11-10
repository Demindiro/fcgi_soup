#ifndef TEMPLATE_H
#define TEMPLATE_H


#include <stddef.h>
#include "dict.h"


/*
 * A template replaces keys denoted by {{KEY}} with the corresponding value
 * in a dictionary or includes/excludes a section depending on a condition
 * denoted by {%cond%}
 */
typedef struct template {
	size_t   count;
	char   **parts;
	size_t  *lengths;
	char   **args;
	int     *flags;
} template;

/*
 * Creates a new template.
 */
int template_create(template *temp, const char *text);

/*
 * Frees a template.
 */
void template_free(template *temp);

/*
 * Parses a template by replacing keys with the corresponding values in the
 * dictionary. If a key does not map to any value, it is substituded by NULL.
 */
char *template_parse(const template *temp, const dictionary *dict);

#endif
