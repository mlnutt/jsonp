#ifndef JSONP_TAB_H
#define JSONP_TAB_H

#include <stddef.h>
#include <stdio.h>

enum yytokentype {
	BEGIN_OBJECT = 200,
	END_OBJECT,
	NUMBER,
	STRING,
	true,
	false,
	null,
	BEGIN_ARRAY,
	END_ARRAY,
	COMMA,
	COLON,
};

#ifndef YYSTYPE
typedef char * YYSTYPE;
#endif

extern YYSTYPE yytext;

extern int yylex(void);

#ifndef YY_SIZE_T
extern int yyleng;
#else
#ifndef YY_TYPEDEF_YY_SIZE_T
#define YY_TYPEDEF_YY_SIZE_T
typedef size_t yy_size_t;
#endif
#extern yy_size_t yyleng;
extern int yyleng;
#endif

extern FILE* yyin;

#undef yy_size_t

#endif /* JSONP_TAB_H  */
