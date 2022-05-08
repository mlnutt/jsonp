%{

#include "jsonp.tab.h"

%}

DIGIT_1to9	[1-9]
DIGIT		[0-9]
DIGITS		{DIGIT}+
INTEGER		{DIGIT}|{DIGIT_1to9}{DIGITS}|-{DIGIT}|-{DIGIT_1to9}{DIGITS}
DECIMALS	[.]{DIGITS}
EXPONENT	{E}{DIGITS}
E		[eE][+-]?
HEX_DIGIT	[0-9a-fA-F]
NUMBER		{INTEGER}|{INTEGER}{DECIMALS}|{INTEGER}{EXPONENT}|{INTEGER}{DECIMALS}{EXPONENT}
UNESCAPED_CHAR	[ -!#-\[\]-~]
ESCAPED_CHAR	\\["\\bfnrt/]
UNICODE_CHAR	\\u{HEX_DIGIT}{HEX_DIGIT}{HEX_DIGIT}{HEX_DIGIT}
CHAR		{UNESCAPED_CHAR}|{ESCAPED_CHAR}|{UNICODE_CHAR}
CHARS		{CHAR}+
QUOTE		["]

%%

{QUOTE}{QUOTE} | 
{QUOTE}{CHARS}{QUOTE} {
	return STRING;
};

{NUMBER} {
	return NUMBER;
}

null {
	return null;
};

true {
	return true;
};

false {
	return false;
};

, {
	return COMMA;
};

: {
	return COLON;
};

\{ {
	return BEGIN_OBJECT;
};

\} {
	return END_OBJECT;
};

\[ {
	return BEGIN_ARRAY;
};

\] {
	return END_ARRAY;
};

[ \t\n\r]+ /* whitespace */;

. {
	fprintf(stderr, "Unexpected character: %c\n", *yytext);
	exit(1);
}

%%

