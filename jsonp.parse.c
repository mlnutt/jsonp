#define _GNU_SOURCE

//#define WRAP

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>

#include <stdarg.h>
#include <getopt.h>

//#include <str.h>

#include "jsonp.tab.h"

#define FIRST_YEAR "2021"

const char * copyright = "Copyright (c) Micah Leiff Nutt, " FIRST_YEAR "-%s\n"
                         "All rights reserved.\n\n";

#define PGM_NAME "jsonp"

#define VER_MAJOR 1
#define VER_MINOR 3
#define VER_DATE __DATE__

#define VER_RELEASE "release"

#define stricmp strcasecmp
#define strincmp strncasecmp

int is_bash_var_char(const char c) {
        return (isalnum((int)c) || (c == '_'));
}

static int is_bash_var(const char *s) {
        if ( isalpha(*s) || (*s == '_')) {
                for ( ++s; *s; ++s) {
                        if ( !is_bash_var_char(*s) ) {
                                break;
                        }
                }
        }

        return !*s;
}

static int timeout = 0;

/*
 * Wait msec milliseconds for input on file handle fileh
 */
int input_timeout(int fileh, unsigned int msec) {
	fd_set set;
	struct timeval timeout;
	int rc = 0;

	FD_ZERO(&set);
	FD_SET(fileh, &set);

	timeout.tv_sec  = (msec / 1000);
	timeout.tv_usec = (msec % 1000) * (1000000 / 1000);

	do {
		rc = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
	} while ((rc == -1) && (errno == EINTR));

	return rc;
}

/*
object
	{}
	{ members }
	array
members
	pair
	pair , members
pair
	string : value
array
	[]
	[ elements ]
elements
	value
	value, elements
value
	string
	number
	object
	array
	true
	false
	null
*/

//#define OPTIONS "0123456789=::a::bBce:E::hj::k:l::o:p::u::qsS:tT::w::v::V"
#define OPTIONS "0123456789=::a::bBce:E::hij::J::k:l::o:p::u::qsS:tT::w::v::"

typedef enum {
	OPT_ASSIGNMENT  = '=',
	OPT_ARRAY	= 'a',
	OPT_BASH_VARS   = 'b',
	OPT_BASH        = 'B',
	OPT_COUNT       = 'c',
	OPT_ESCAPE      = 'e',
	OPT_ENUMERATE   = 'E',
 	OPT_HELP        = 'h',
 	OPT_INSENSATIVE = 'i',
	OPT_JSON        = 'j',
	OPT_JSONV       = 'J',
	OPT_KEY         = 'k',
	OPT_LIST        = 'l',
	OPT_OUTPUT      = 'o',
	OPT_PREFIX      = 'p',
	OPT_QUIET	= 'q',
	OPT_NO_MESSAGES	= 's',
	OPT_STRING	= 'S',
	OPT_TYPE        = 't',
	OPT_TIMEOUT     = 'T',
	OPT_UNQUOTE     = 'u',
	OPT_VALUES_ONLY = 'v',
 	OPT_VERSION	= 'V',
	OPT_WRAP        = 'w',

//	OPT_VERBOSE     = 200,
} option_t;

static struct option long_options[] = {
	{"assignment`",	        optional_argument,	0,		OPT_ASSIGNMENT },
	{"array",		optional_argument,	0,		OPT_ARRAY },
	{"bash-variables",	no_argument,		0,		OPT_BASH_VARS },
	{"bash-vars",		no_argument,		0,		OPT_BASH_VARS },
	{"bash",	        no_argument,		0,		OPT_BASH },
	{"prefix",		optional_argument,	0,		OPT_PREFIX },
	{"unquote",		optional_argument,	0,		OPT_UNQUOTE },
	{"values-only",		optional_argument,	0,		OPT_VALUES_ONLY },
	{"wrap",		optional_argument,	0,		OPT_WRAP },

	{"count",	        no_argument,		0,		OPT_COUNT },
	{"enumerate",		optional_argument,	0,		OPT_ENUMERATE },
	{"key",			required_argument,	0,		OPT_KEY },
	{"attribute",		required_argument,	0,		OPT_KEY },
	{"name",		required_argument,	0,		OPT_KEY },
	{"field",		required_argument,	0,		OPT_KEY },
	{"type",		no_argument, 		0,		OPT_TYPE },
	{"kind",		no_argument, 		0,		OPT_TYPE },
	{"list",	        optional_argument,	0,		OPT_LIST },

	{"output",		required_argument,	0,		OPT_OUTPUT },
	{"string",		required_argument,	0,		OPT_STRING },

	{"help",		no_argument,		0,		OPT_HELP },
	{"case-insensative",	no_argument,		0,		OPT_INSENSATIVE },
	{"insensative",		no_argument,		0,		OPT_INSENSATIVE },
	{"json",		optional_argument,	0,		OPT_JSON },
	{"JSON",		optional_argument,	0,		OPT_JSONV },
	{"quiet",		no_argument,		0,		OPT_QUIET },
	{"silent",		no_argument,		0,		OPT_QUIET },
	{"no-messages",		no_argument,		0,		OPT_NO_MESSAGES },
	{"timeout",		optional_argument,	0,		OPT_TIMEOUT },
	//{"verbose",		no_argument,		0,		OPT_VERBOSE },
	{"version",		no_argument,		0,		OPT_VERSION },

	{"escape",		required_argument,	0,		OPT_ESCAPE },
	{0,			0,			0,		0 },
};

typedef enum {
	unquote_none   = 0x00,
	unquote_keys   = 0x01,
	unquote_values = 0x02,
	unquote_all    = unquote_keys | unquote_values,
} unquote_t;

static unquote_t unquote = unquote_none;

typedef enum {
	wrap_none    = 0x00,
	wrap_dollar  = 0x01,
	wrap_objects = 0x02,
	wrap_arrays  = 0x04,
	wrap_values  = 0x08,
	wrap_all     = wrap_objects | wrap_arrays | wrap_values,
} wrap_t;

static wrap_t wrap   = wrap_none;

static int open_wrap   = 0;

static int bash_vars   = 0;
static int value_only  = 0;
static int strip_array = 0;

static int count_elements = 0;
static int element_count  = 0;
static int get_element_no = 0;

static int count_keys    = 0;
static int key_count     = 0;
static int get_key_no    = 0;
static int got_key       = 0;
static int insensative   = 0;
static int found_key     = 0;
static int list_keys     = 0;
static int list_elements = 0;
static int get_type      = 0;

static int quiet           = 0;
static int quiet_arg       = 0;
static int no_messages     = 0;
static int no_messages_arg = 0;
static int verbose         = 0;

static int level      = 0;
static int in_array   = 0;
static int open_quote = 0;
static int show_type  = 0;

static int enumerate  = 0;
static int iterate    = 0;
static int j_before_u = 1;
static int is_value   = 0;

static char *prefix_json  = "JSON_";
static char *prefix_type  = "JTYP_";
static int  custom_prefix = 0;

/*
 * string buffers
 */
static char *prefix         = 0;
static char *get_key        = 0;
static char *assignment     = 0;
static char *key_name       = 0;
static char *unquoted_text  = 0;
static char *escaped_text   = 0;
static char *bash_key_text  = 0;
static char *string         = 0;
static char *array_beg      = 0;
static char *array_sep      = 0;
static char *array_end      = 0;
static char *enum_fmt       = 0;
static char *enum_cmd       = 0;
static char *enum_itm       = 0;
static char *output_buf     = 0;
static char *esc_chars      = 0;

typedef enum {
	json_unknown,
	json_any,
	json_object,
	json_array,
} json_t;

static json_t json  = json_unknown;
static json_t check = json_unknown;

static int check_type = 0;

static int   lookahead      = 0;
static char *lookahead_text = 0;
static char *_yytext        = 0;

/*
 * Begin Forward declarations 
 */

static char *_escape_pattern(char *text, char pattern);
static char *escape_quotes(char *text);
static char *escape_chars(char *esc_chars, char *text);

static char *bash_key(char *text);
static char *unquote_value(char *text);
static char *key(char *text);

static void json_type(int type);

static int yylookahead(void);
static int _yylex(void);

static int elements(int yy);
static int array(int yy);
static int value(int yy);
static int pair(int yy);
static int keys(int yy);
static int object(int yy);

static int run_enum_cmd(char *enum_fmt, char *enum_itm, unsigned enum_cnt);

static void _free(void **ptr);
static void init(void);
static void cleanup(int rc);

static void output(const char *fmt, ...);
static void err_msg(const char *fmt, ...);
static void wrn_msg(const char *fmt, ...);
static void vbs_msg(const char *fmt, ...);

static void version(void);
static void little_help(void);
static void help(void);

static inline int c_todigit(const char c);

/*
 * End Forward Declarations
 */

static void output(const char *fmt, ...) {
	va_list valist;

	if (!quiet) {
		va_start(valist, fmt);
		vprintf(fmt, valist);
		va_end(valist);
	} else  if (enumerate || iterate) {
		if ( enumerate ) {
			if ( get_type && !show_type ) {
				return;
			}
			if ( get_element_no && (element_count != get_element_no)) {
				return;
			}
		}
		if ( iterate ) {
			if (is_value && (list_keys && (list_keys != 3))) {
				return;
			}

			if (is_value && get_type && !show_type) {
				return;
			}
		}
		va_start(valist, fmt);
		vasprintf(&output_buf, fmt, valist);
		va_end(valist);
	
		asprintf(&enum_itm, "%s%s", enum_itm ? enum_itm : "", output_buf);
	}
}

static void err_msg(const char *fmt, ...) {
	va_list valist;

	if (!no_messages) {
		va_start(valist, fmt);
		fprintf(stderr, "Error: ");
		vfprintf(stderr, fmt, valist);
		fprintf(stderr, "\n");
		va_end(valist);
	}

	cleanup(1);
}

static void wrn_msg(const char *fmt, ...) {
	va_list valist;

	if (!no_messages) {
		va_start(valist, fmt);
		fprintf(stderr, "Warning: ");
		vfprintf(stderr, fmt, valist);
		fprintf(stderr, "\n");
		va_end(valist);
	}
}

static void vbs_msg(const char *fmt, ...) {
	va_list valist;

	if (verbose) {
		va_start(valist, fmt);
		vprintf(fmt, valist);
		printf("\n");
		va_end(valist);
	}
}

static inline int is_special(char c, char *s) {
	return strchr(s, c) != NULL;
}

static char *escape_chars(char *esc_chars, char *text) {
	char *ptr;
	char *new_text;
	int   count = 0;

	for (ptr = text; *ptr; ++ptr) {
		if (is_special(*ptr, esc_chars))
			++count;
	}	

	if (!count) {
		return text;
	}

	new_text = escaped_text = (char *) realloc(escaped_text, sizeof(char) * (strlen(text) + count + 1));

	for (ptr = text; *ptr; ++ptr) {
		
		if (is_special(*ptr, esc_chars)) {
			*new_text++ = *esc_chars;
		}
		*new_text++ = *ptr;
	}
	*new_text = 0;

	return escaped_text;
}
/*
 * Replace all "pattern" chars with an escaped "pattern" char
 */
static char *_escape_pattern(char *text, char pattern) {
	char replacement[3] = "\\ ";
	char *ptr;
	char *location;
	char *new_text;
	int   count = 0;

	replacement[1] = pattern;

	for (ptr = text; location = strchr(ptr, pattern); ptr = location + 1) {
		count++;
	}

	if (!count) {
		return text;
	}

	new_text = escaped_text = (char *) realloc(escaped_text, sizeof(char) * (strlen(text) + count + 1));

	for (ptr = text; location = strchr(ptr, pattern); ptr = location + 1) {
		int skip = location - ptr;

		strncpy(new_text, ptr, skip);
		new_text += skip;
		
		strncpy(new_text, replacement, 2);
		new_text += 2;
	}
	strcpy(new_text, ptr);

	return escaped_text;
}

/*
 * Replace all ' characters with \'
 */
static char *escape_quotes(char *text) {

	return _escape_pattern(text, '\'');
}

/*
 * Replace all invalid bash variable characters with an _
 */
static char *bash_key(char *text) {
	static const char replacement = '_';
	int changes = 0;
	char *ptr;

	if (!bash_vars) {
		return text;
	}

	if (*text == '"') {
		asprintf(&bash_key_text, "%s", text + 1);

		if (bash_key_text[strlen(bash_key_text) - 1] == '"') {
			bash_key_text[strlen(bash_key_text)-1] = 0;
		}
	} else {
		asprintf(&bash_key_text, "%s", text);
	}

	ptr = bash_key_text;

	if (!prefix && !isalpha(*ptr) && (*ptr != '_')) {
		if (isdigit(*ptr)) {
			++changes;
			asprintf(&bash_key_text, "_%s", bash_key_text);
			ptr = bash_key_text;
		} else {
			++changes;
			*ptr = replacement;
		}
	}

	for (++ptr; *ptr; ++ptr) {
		if (!is_bash_var_char(*ptr)) {
			++changes;
			*ptr = replacement;
		}
	}

	if ((*bash_key_text == '_') && !*(bash_key_text + 1)) {
		++changes;
		asprintf(&bash_key_text, "__");
	}

	if (changes) {
		wrn_msg("%s changed to \"%s\"", text, bash_key_text);
	}

	return bash_key_text;
}

/*
 * Remove surronding " characters from a JSON value.
 */
static char *unquote_value(char *text) {

	if (level != 1)
		return text;

	if (!(unquote & unquote_values) || *text != '"') {
		return text;
	}

	asprintf(&unquoted_text, "%s", text + 1);

	if (unquoted_text[strlen(unquoted_text) - 1] == '"') {
		unquoted_text[strlen(unquoted_text)-1] = 0;
	}

	return unquoted_text;
}

/*
 * Possibly turn the key into a bash valid variabe or escape the ' in the key and possibly prepend the key with a prefix...
 */
static char *key(char *text) {

	if (bash_vars) {
		text = bash_key(text);
	} 

	if (unquote & unquote_keys) {
		asprintf(&key_name, "%s%s", prefix ? prefix : "", text + ((unquote & unquote_keys) && (*text == '"') ? 1 : 0));
		if (key_name[strlen(key_name) - 1] == '"') {
			key_name[strlen(key_name) - 1] = 0;
		}

	} else {
		asprintf(&key_name, "%s%s%s", *text == '"' ? "\"" : "", prefix ? prefix : "", text + (*text == '"' ? 1 : 0) );
	}

	return key_name;
}

/*
 * Look Ahead at the next token to be returned by lex.
 */
static int yylookahead(void) {

	lookahead = yylex();
	
	asprintf(&lookahead_text, "%s", yytext);

	return lookahead;
}

/*
 * yylex() wrapper to return a Look Ahead, if needed, and to print out the token's corresponding string if necessary.
 */
static int _yylex(void) {
	int yy;

	if (lookahead) {
		yy = lookahead;
		asprintf(&_yytext, "%s", lookahead_text);
		lookahead = 0;
	} else {
		yy = yylex();
		asprintf(&_yytext, "%s", yytext);
	}
	return yy;
}

/*
 * Output the type of JSON item...
 */
static void json_type(int type) {

	switch (type) {
		case STRING:
			output("%s", "STRING");
			break;
		case null:
			output("%s", "null");
			break;
		case true:
		case false:
			output("%s", "BOOLEAN");
			break;
		case NUMBER:
			output("%s", "NUMBER");
			break;
		case BEGIN_OBJECT:
			output("%s", "OBJECT");
			break;
		case BEGIN_ARRAY:
			output("%s", "ARRAY");
			break;
		default:
			return;
	}
	output("%c", ((iterate || enumerate) ? 0 : (list_keys == 1) ? ' ' : '\n'));
}

static int run_enum_cmd(char *enum_fmt, char *enum_itm, unsigned enum_cnt) {
	int rc = 0;
	static FILE *pp = NULL;

	if (!enum_itm || !*enum_itm) {
		return rc;	
	}

	quiet = quiet_arg;

	if (j_before_u) {
		asprintf(&enum_cmd, enum_fmt, escape_chars(esc_chars, enum_itm), enum_cnt); 
	} else {
		asprintf(&enum_cmd, enum_fmt, enum_cnt, escape_chars(esc_chars, enum_itm)); 
	}

	if ((pp = popen(enum_cmd, "r"))) { 
		#define RESULT_SIZE 4096
		static char result[RESULT_SIZE];

		while (fgets(result, RESULT_SIZE, pp) != NULL) {
   			output("%s", result);
		}
		rc = pclose(pp);
//printf("rc = %d\n", rc/256);
	} else {
		rc = -1;
	}

	*enum_itm = 0;

	quiet = 1;

	return rc;
}
/*
 * Begin Recursive Descent parser functions...
 */

/*
 * Array elements...
 */
static int elements(int yy) {

	if (level == 1) {
		if (get_type && list_elements) {
			show_type = 1;
		}
		if (get_element_no || count_elements) {
			if (++element_count == get_element_no) {
				if (!list_elements) {
					quiet = enumerate || iterate ? 1 : quiet_arg; //mln
					no_messages = no_messages_arg;
				}
			}
		}
	}

	yy = value(yy);

	if (level == 1) {
		if (enumerate) {
			run_enum_cmd(enum_fmt, enum_itm, element_count);
		} else if (get_element_no && (element_count == get_element_no)) {
			quiet = no_messages = 1;
		}
	}

	yy = _yylex();

	if (yy == COMMA) {
#ifdef WRAP
		if (level == 1) {
#else
		if ((level == 1) || ((level == 2) && value_only)) {
#endif
			//if (value_only || (!enumerate && !iterate)) {
			if (value_only || !enumerate) {
				if ( list_elements == 1 ) {
					output(" ");
				} else if ( list_elements == 2 ) {
					output("\n");
				} else {
					//if ( !enumerate || ( enumerate && (level != 1) )) {
						output("%s", array_sep);
					//}
				}
			} 
		} else {
			output(",");
		}

		yy = elements(yy = _yylex());
	}

	return yy;
}

/*
 * Array [ ... ]...
 */
static int array(int yy) {

#ifdef WRAP
	if (++level == 1) {
#else
	if ((++level == 1) || ((level == 2) && value_only)) {
#endif
		in_array = 1;
		if (list_elements || enumerate) {
			if (get_type) {
				show_type = 1;
			}
		} 

		if ( !list_elements && !enumerate ) {
			if (!strip_array) {
				if ( wrap & wrap_arrays ) {
					if (wrap & wrap_objects) { // If we're wrapping the base array we don't wrap any of its' object elements...
						wrap -= wrap_objects;
					}

					if ( wrap & wrap_dollar ) {
						output("$");
					}
					output("'");
					open_wrap = 1;
				}
				output("%s ", array_beg);
			}
		}
	} else {
		if ( level == 2 && !open_wrap && (wrap || strip_array || list_elements || enumerate )) {
			if ( wrap & wrap_arrays ) {
				if ( wrap & wrap_dollar ) {
					output("$");
				}
				output("'");
				open_wrap = 1;
			}
		}
		output("[");
	}

	yy = _yylex();

	if (yy != END_ARRAY) {
		yy = elements(yy);
	}
	
	if (yy == END_ARRAY) {
#ifdef WRAP
		if (level == 1) {
#else
		if ((level == 1) || ((level == 2) && value_only)) {
#endif
			if (!list_elements && !enumerate) {
				if (!strip_array) {
					output(" %s", array_end);
					if (wrap & wrap_arrays) {
						output("'");
						open_wrap = 0;
					}
				}
			}
		} else {
			output("]");
			if ((level == 2) && open_wrap && (wrap || strip_array || list_elements || enumerate)) {
				if (wrap & wrap_arrays) {
					output("'");
					open_wrap = 0;
				}
			}
		}

		if (level-- == 1) {
			in_array = 0;
		}
	}

	return yy;
}

/* Value:
 * 	STRING
 * 	true
 * 	false
 * 	null
 * 	NUMBER
 * 	OBJECT
 * 	ARRAY
 */
static int value(int yy) {

	if (get_element_no && get_type) {
		if (element_count == get_element_no) {
			if (show_type) {
				quiet = (enumerate || iterate) ? 1 : quiet_arg; //mln
				json_type(yy);
				show_type = 0;
			}
			quiet = 1;
		}
	} else if (show_type || (get_type && got_key)) {
		quiet = (enumerate || iterate) ? 1 : quiet_arg; //mln
		json_type(yy);
		show_type = 0;
		quiet = 1;

		if (get_type && got_key) {
			got_key = iterate ? 1 : 0;
		}
	}

	switch (yy) {
		case STRING:
		case true:
		case false:
		case null:
		case NUMBER: {
			if ((level == 1) && (wrap & wrap_values)) {
				if (wrap & wrap_dollar) {
					output("$");
				}
				output("'");
			}
			output("%s", unquote_value(_yytext));
			if ((level == 1) && (wrap & wrap_values)) {
				output("'");
			}
			break;
		}
		case BEGIN_OBJECT: {
			yy = object(yy);
			break;
		}
		case BEGIN_ARRAY: {
			yy = array(yy);
			break;
		}
		default:
			err_msg("Expected a value but got \"%s\"", *_yytext ? _yytext : "EOF");
			break;
	}

	return yy;
}

/* Pair:
 * 	STRING:value
 */
static int pair(int yy) {

	if (yy == STRING) {
		if (++level == 1) {
			if (++key_count == get_key_no) {
				if (!list_keys) {
					quiet = (enumerate || iterate) ? 1 : quiet_arg; //mln
					no_messages = no_messages_arg;
				}
				if (get_type) {
					show_type = 1;
				}
			} else if (get_key) {
				if (insensative ? (stricmp(get_key, _yytext) == 0) :  (strcmp(get_key, _yytext) == 0)) {
					found_key = got_key = 1;
					quiet = (enumerate || iterate) ? 1 : quiet_arg;
					no_messages = no_messages_arg;
				}
			} 

			if (list_keys) {
				if (get_type && (!get_key || iterate)) {
					show_type=1;
					if (!value_only) {
						quiet = (enumerate || iterate) ? 1 : quiet_arg; //mln
						output("%s%s", key(_yytext), assignment);
						quiet = 1;
					}
				} else {
					if (!get_key_no || (get_key_no && (key_count == get_key_no))) {
						quiet = (enumerate || iterate) ? 1 : quiet_arg; //mln
						output("%s%s", key(_yytext), (iterate ? "" : (list_keys == 1) ? " " : "\n"));
						quiet = 1;
					}
				}
			} else if (!value_only) {
				output("%s%s", key(_yytext), yylookahead() == COLON ? assignment : "");
			}
		} else {
			output("%s", _yytext);
			if ( yylookahead() == COLON ) {
				output(":");
			}
		}

		yy = _yylex();
	
		if (yy == COLON) {
			++is_value;
			yy = value(yy = _yylex());

			if (level-- == 1) {
				if (!iterate )
					output("\n");
				if ((key_count == get_key_no) || got_key) {
					quiet = no_messages = 1;
				}
			}
			if (is_value-- == 1) {
				if (iterate) {
					if ((!get_key_no && !get_key) || ((key_count == get_key_no) || got_key)) {
						run_enum_cmd(enum_fmt, enum_itm, key_count);
						got_key = 0;
					}
					*enum_itm = 0;
				} 
			}
		} else {
			err_msg("Expected \'COLON\' but got \"%s\"", *_yytext ? _yytext : "EOF");
		}
	} else {
		err_msg("Expected \'STRING\' but got \"%s\"", *_yytext ? _yytext : "EOF");
	}
		
	return yy;
}

/*
 * Keys
 * 	STRING:value...
 */
static int keys(int yy) {
	
	yy = pair(yy);

	yy = _yylex();

	if (yy == COMMA) {
		if ( level > 0) {
			output(",");
		}
		yy = keys(yy = _yylex());
	}

	return yy;
}

/*
 * Object:
 * 	{ keys... }
 */
static int object(int yy) {
	if (level == 1) {
 		if (in_array && !open_quote++) {
			output("%s{", wrap & wrap_objects ? (wrap & wrap_dollar ? "$'" : "'") : "");
		} else if (!in_array) {
			output("%s{", wrap & wrap_objects ? (wrap & wrap_dollar ? "$'" : "'"): "");
			open_quote = 1;
		} else {
			output("{");
		}
	} else if (level > 0){
		output("{");
	}

	yy = _yylex();

	if (yy != END_OBJECT) {
		yy = keys(yy);
	}
	
	if (level > 0) {
		if ( !(iterate && list_keys) )
			output("}");
	}
	if (in_array){
		if (!--open_quote) {
			output("%s", wrap & wrap_objects ? "'" : "");
		}
	} else if (level == 1) {
		output("%s ", wrap & wrap_objects ? "'" : "");
		open_quote = 0;
	} 

	return yy;
}

/*
 * Free an allocated memory block and reinstantiate the ptr to zero
 */
static void _free(void **ptr) {

	if (*ptr) {
		free(*ptr);
		*ptr = 0;
	}
}

/*
 * Free the allocated memor blocks and exit with any errorcode.
 */
static void cleanup(int rc) {

	_free((void **) &string);

	_free((void **) &_yytext);

	_free((void **) &lookahead_text);

	_free((void **) &unquoted_text);

	_free((void **) &key_name);

	_free((void **) &escaped_text);

	_free((void **) &esc_chars);

	_free((void **) &get_key);

	_free((void **) &prefix);

	_free((void **) &array_beg);

	_free((void **) &array_sep);

	_free((void **) &array_end);

	_free((void **) &assignment);

	_free((void **) &bash_key_text);

	_free((void **) &enum_itm);
	_free((void **) &output_buf);

	_free((void **) &enum_cmd);
	_free((void **) &enum_fmt);

	exit(rc);
}

static void version(void) {
	char *this_year = strrchr(VER_DATE, ' ') + 1;

	printf("\n");
	printf("%s Version %i.%i %s (Built %s)\n\n", PGM_NAME, VER_MAJOR, VER_MINOR, VER_RELEASE, VER_DATE);
	printf(copyright, strcmp(this_year, FIRST_YEAR) ? this_year : "\b ");
	printf("There is NO WARRANTY, to the extent permitted by applicable law.\n");
}

static void little_help(void) {

	printf("\n");
	printf("Usage: %s [OPTION]...\n", PGM_NAME);
	printf("Try `%s --help' for more information.\n", PGM_NAME);
}

static void help(void) {

	version();

	printf("\n");
	printf("Usage: %s [OPTION]... [INFILE]\n", PGM_NAME);

	printf("\nOutput Formating:\n");
	printf("\t-=,  --assignment[=STRING]\tOutput STRING as assignment token instead of \":\" (default is \"=\")\n");
	printf("\t-a   --array[=STRING]\t\tOutput characters for arrays instead of \"[,]\" (default is \"( )\")\n");
	printf("\t-b,  --bash-variables\t\tOutput bash-friendly variable names, i.e., [_a-zA-Z][_a-zA-Z0-9]+ (substituting '_' for invalid characters)\n");
	printf("\t-B,  --bash\t\t\tAlias for --wrap=object$ --unquote=keys --array-chars=\"( )\" --assignment=\"=\" --bash-variables\n");
	printf("\t-p,  --prefix[=STRING]\t\tPrefix key name (Default is \"%s\" or \"%s\" if --type)\n", prefix_json, prefix_type);
	printf("\t-u,  --unquote[=WHAT]\t\tStrip double quotes from WHAT: \"none\" \"keys\" \"values\" or \"all\" (default)\n");
	printf("\t-v,  --values-only[=strip]\tOutput only the values of \"key:value\" pairs; optionally strip surrounding array characters\n");
	printf("\t-w,  --wrap[=WHAT]\t\tWrap single quotes around base objects, arrays, values (optionaly prefix with a bash-friendly \"$\"); where WHAT is \"all[$]\", \"objects[$]\", \"arrays[$]\", or \"values[$]\"\n"); 
	printf("\nOutput Control\n");
	printf("\t-c,  --count\t\t\tOutput the number of base keys (or array elements)\n");
	printf("\t-k,  --key=STRING\t\tOutput only the key matching STRING\n");
	printf("\t-l,  --list[=1]\t\t\tOutput only the base keys (or array elements); optionally output on one line\n");
	printf("\t-t,  --type\t\t\tOutput the \"type\" of keys (or array elements) instead of values; can use with --json, --key, --list (default) or -NUM\n");
	printf("\t-NUM\t\t\t\tOutput only the nth key (or nth array element)\n");
	printf("\nInput/Output\n");
	printf("\t-o,  --output=OUTFILE\t\tOutput to OUTFILE instead of stdout\n");
	printf("\t-S,  --string=STRING\t\tParse STRING instead of stdin or INFILE\n");
	printf("\nMiscellaneous:\n");
	printf("\t-e,  --escape=CHARS\t\tEscape these CHARS when the enumerate CMD is invoked, the first char is the escape symbol which will also be escaped (default \"%s\")\n", escape_chars(esc_chars, esc_chars));
	printf("\t-E,  --enumerate[=CMD]\t\tEnumerate \"CMD\" for base keys or array elements (default is \"%s\")\n", escape_chars(esc_chars, enum_fmt));
	printf("\t-h,  --help\t\t\tDisplay this help and exit\n");
	printf("\t-i,  --case-insensative\t\tBe case insensative when using --key option\n");
	printf("\t-j,  --json[=KIND]\t\tValidate input as JSON object or JSON array; where KIND is \"object\", \"array\", or \"any\" (default)\n");
	printf("\t-J,  --JSON[=KIND]\t\tSame as --json but is verbose\n");
	printf("\t-q   --quiet, --silent\t\tSuppress output\n");
	printf("\t-s   --no-messages\t\tSuppress error and warning messages\n");
	printf("\t-T   --timeout[=TIME]\t\tWait TIME seconds before timing out and exiting (5 seconds default)\n");
	printf("\t-V   --version\t\t\tDisplay version information and exit\n");
//	printf("\t     --verbose\t\t\tBe verbose about --json validation\n");

	cleanup(0);
}

static void init(void) {

	asprintf(&array_beg, "%c", '[');
	asprintf(&array_sep, "%c", ',');
	asprintf(&array_end, "%c", ']');

	asprintf(&assignment, "%c", ':');

	asprintf(&esc_chars, "%s", "\\\""); // first char is the "escape char" which also will be escaped...

	asprintf(&enum_fmt, "%s", "echo \"%s\""); 
}

static inline int c_todigit(const char c) {
	int n = -1;

	if (isdigit(c)) {
		n = c - '0';
	}

	return n;
}

int main(int argc, char *argv[]) {
	int c;
	int yy;
	int option_index = 0;
	int rc = 0;
	int arg;

	init();

	optind = 0;

	while ( ((c = getopt_long(argc, argv, OPTIONS, long_options, &option_index)) != -1)) {
		static int new_number = 0; 
		int        number;

		if ((number = c_todigit(c)) >= 0) { // -NUM
			if (new_number == 0) {
				get_key_no = 0;
			}

			new_number *= 10;
			new_number += number;

			get_key_no = new_number;

			continue;
		} 

		new_number = 0;

		switch(c) {
			case OPT_HELP: // -h, --help
				help();
				break;
			case OPT_VERSION: // -V, --version
				version();
				cleanup(0);
				break;
			case OPT_STRING:
				asprintf(&string, "%s", optarg);

				yyin = fmemopen(string, strlen(string), "r");
				break;
			case OPT_OUTPUT:
				freopen(optarg, "w", stdout);
				break;
			case OPT_JSONV:
			//case OPT_VERBOSE:
				if (quiet_arg && get_type) {
					err_msg("Conflicting --quiet and --verbose arguments");
				}
				verbose = 1;
				//break;
			case OPT_JSON:
				if (optarg) {
					if (strcasecmp("array", optarg) == 0) {
						check = json_array;
					} else if (strcasecmp("object", optarg) == 0) {
						check = json_object;
					} else if (strcasecmp("any", optarg) == 0) {
						check = json_any;
					} else {
						err_msg("Optional --json argument values are \"any\", \"object\" or \"array\"\n");
					}
				} else {
					check = json_any;
				}
				if (list_keys) {
					err_msg("Conflicting --list and --json arguments");
				} else if (count_keys) {
					err_msg("Conflicting --count and --json arguments");
				} else if (get_key_no) {
					err_msg("Conflicting -%d and --json arguments", get_key_no);
				} else if (get_key) {
					err_msg("Conflicting --key=%s and --json arguments", get_key);
				} else if (enumerate) {
					err_msg("Conflicting --enumerate=\"%s\" and --json arguments", escape_chars(esc_chars, enum_fmt), get_key_no);
				}

				quiet = 1;
				break;
			case OPT_BASH: // -B, --bash
				bash_vars = 1;

 				wrap |= wrap_objects | wrap_dollar;
				unquote |= unquote_keys;
				*array_beg = '('; 
				*array_sep = ' ';; 
				*array_end = ')';; 
				strcpy(assignment, "=");
				break;
			case OPT_BASH_VARS: // -b, --bash-variables
				bash_vars = 1;
				break;
			case OPT_ASSIGNMENT: // -a, --assignment 
				if (optarg) {
					asprintf(&assignment, "%s", optarg);
				} else {
					strcpy(assignment, "=");
				}
				break;
			case OPT_PREFIX: // -p, --prefix (prefix pair identifier strings - default is "JSON_" or "JTYP_")
				if (optarg) {
					asprintf(&prefix, "%s", optarg);
					custom_prefix = 1;
				} else if (!custom_prefix) {
					asprintf(&prefix, "%s", get_type ? prefix_type : prefix_json);
				}

				//unquote |= unquote_keys;
				break;
			case OPT_COUNT: // -c, --count-keys (return the number of "level 1" keys)
				if (list_keys) {
					err_msg("Conflicting --list and --count arguments");
				} else if (get_key_no) {
					err_msg("Conflicting -%d and --count arguments", get_key_no);
				} else if (get_key) {
					err_msg("Conflicting --key=%s and --count arguments", get_key);
				} else if (get_type) {
					err_msg("Conflicting --type  and --count arguments");
				} else if (check != json_unknown) {
					err_msg("Conflicting --json and --count arguments");
				}

				quiet = 1;
				count_keys = 1;
				break;
			case OPT_QUIET: // -q, --quiet (suppress output)
				if (verbose && get_type) {
					err_msg("Conflicting --verbose and --quiet arguments");
				}
				quiet = quiet_arg = 1;
				break;
			case OPT_NO_MESSAGES: // -s, --no-messages (suppress error messages)
				no_messages_arg = 1;
				break;
			case OPT_INSENSATIVE: // -i, --case-insensative (for --key)
				insensative = 1;
				break;
			case OPT_KEY: // -k, --key (return only value for matching pair identifier string)
				if (get_key) {
					err_msg("Conflicting --key=%s and --key=\"%s\" arguments", get_key, optarg);
				} else if (list_keys) {
					err_msg("Conflicting --list and --key=\"%s\" arguments", optarg);
				} else if (get_key_no) {
					err_msg("Conflicting -%d and --key=\"%s\" arguments", get_key_no, optarg);
				} else if (count_keys) {
					err_msg("Conflicting --count and --key=\"%s\" arguments", optarg);
				} else if (check != json_unknown) {
					err_msg("Conflicting --json and --key=\"%s\" arguments", optarg);
				}

				quiet = 1;
				asprintf(&get_key, "\"%s\"", optarg);
				break;
			case OPT_TYPE: // -t, --type (return the type of key)
				if (count_keys) {
					err_msg("Conflicting --count and --type arguments");
				} else if (quiet_arg && verbose) {
					err_msg("Conflicting --quiet and --verbose arguments");
				} else if (list_keys) {
					err_msg("Conflicting --list and --type arguments");
				}

				if (prefix && !custom_prefix) {
					asprintf(&prefix, "%s", prefix_type);
				}

				quiet = 1;
				get_type=1;
				break;
			case OPT_VALUES_ONLY: // -v, --values-only
				if (optarg) {
					if (strcasecmp(optarg, "strip") == 0 || strcasecmp(optarg, "no-arrays") == 0) {
						strip_array = 1;
					} else {
						err_msg("Optional --value-only argument value is \"strip\" \n");
					}
				}
				value_only = 1;
				break;
			case OPT_WRAP: // -w, --wrap
				if (optarg) {
					if ((strcmp(optarg, "objects") == 0) || (strcmp(optarg, "object") == 0)) {
						wrap |= wrap_objects;
					} else if ((strcasecmp(optarg, "objects$") == 0) || (strcasecmp(optarg, "object$") == 0)) {
						wrap |= wrap_objects | wrap_dollar;
					} else if ((strcasecmp(optarg, "arrays") == 0) || (strcasecmp(optarg, "array") == 0)) {
						wrap |= wrap_arrays;
					} else if ((strcasecmp(optarg, "array$") == 0) || (strcasecmp(optarg, "arrays$") == 0)) {
						wrap |= wrap_arrays | wrap_dollar;
					} else if ((strcasecmp(optarg, "values") == 0) || (strcasecmp(optarg, "array") == 0)) {
						wrap |= wrap_values;
					} else if ((strcasecmp(optarg, "value$") == 0) || (strcasecmp(optarg, "values$") == 0)) {
						wrap |= wrap_values | wrap_dollar;
					} else if ((strcasecmp(optarg, "all") == 0)) {
						wrap |= wrap_all;
					} else if ((strcasecmp(optarg, "all$") == 0)) {
						wrap |= wrap_all | wrap_dollar;
					} else {
						err_msg("Optional --wrap argument values are \"objects[$]\", \"arrays[$]\" or \"all[$]\"\n");
					}
				} else {
					wrap |= wrap_objects;
				}

				break;
			case OPT_UNQUOTE: // -u, --unquote
				if (optarg) {
					if (strcasecmp(optarg, "none") == 0 || strcasecmp(optarg, "nothing") == 0 || strcasecmp(optarg, "off") == 0) {
						unquote = unquote_none;
					} else if (strcasecmp(optarg, "keys") == 0 || strcasecmp(optarg, "fields") == 0 || strcasecmp(optarg, "attributes") == 0 || strcasecmp(optarg, "names") == 0) {
						unquote = unquote_keys;
					} else if (strcasecmp(optarg, "values") == 0) {
						unquote = unquote_values;
					} else if (strcasecmp(optarg, "all") == 0 || strcasecmp(optarg, "everything") == 0 || strcasecmp(optarg, "on") == 0) {
						unquote = unquote_all;
					} else {
						err_msg("Optional --unquote argument values are \"none\", \"keys\", \"values\" or \"all\"\n");
					}
				} else {
					unquote = unquote_all;
				}
				break;
			case OPT_ARRAY: // -a, --array
				if (optarg) {
					if (strlen(optarg) != 3) {
						err_msg("STRING must be 3 characters\n");
					} else {
						*array_beg = optarg[0]; 
						*array_sep = optarg[1]; 
						*array_end = optarg[2]; 
					}
				} else {
					*array_beg = '('; 
					*array_sep = ' ';; 
					*array_end = ')';; 
				}
				break;
			case OPT_ENUMERATE: // -E, --enumerate
				enumerate = 1;
				if (optarg) {
					char *j_ptr = 0;
					char *u_ptr = 0;

					asprintf(&enum_fmt, "%s", optarg);

					if (j_ptr = strstr(enum_fmt, "%J")) {
						*(j_ptr + 1) = 's';
					} else {
						j_ptr = strstr(enum_fmt, "%s");
					}

					if (u_ptr = strstr(enum_fmt, "%I")) {
						*(u_ptr + 1) = 'u';
					} else {
						u_ptr = strstr(enum_fmt, "%u");
					}

					j_before_u = (j_ptr && u_ptr) ? (j_ptr < u_ptr) : (u_ptr ? 0 : 1) ;
				}
				break;
			case OPT_LIST: // -l, --list
				if (optarg) {
					if (strcasecmp(optarg, "1") == 0) {
						list_keys = 1;
					} else {
						err_msg("Optional --list-keys argument value is \"1\"\n");
					}
				} else {
					list_keys = 2;
				}

				if (count_keys) {
					err_msg("Conflicting --count and --list arguments");
				} else if (get_key) {
					err_msg("Conflicting --key=%s and --list arguments", get_key);
				} else if (check != json_unknown) {
					err_msg("Conflicting --json and --list arguments");
				} else if (get_type) {
					err_msg("Conflicting --type and --list arguments");
				} 

				quiet = 1;
				break;

			case OPT_ESCAPE: // -e, --escape
				asprintf(&esc_chars, "%s", optarg);
				break;
			case OPT_TIMEOUT: // -T, --timeout
				if (optarg) {
					char *ptr = 0;

					errno = 0;
					timeout = strtol(optarg, &ptr, 10);

					if ( ((timeout == 0) && (errno != 0)) || (ptr && (*ptr != 0)) || (timeout < 0) ) {
						err_msg("Invalid argument --timeout=\"%s\"", optarg);
					}
				} else {
					timeout = 5000;
				}
				break;
			case 0: 
			case '?':
			default:
				rc = 1;
				break;
		}
	}

	no_messages = no_messages_arg;

	if (check && get_type) {
		check_type = 1;
		get_type = 0;
	}

	if (get_key_no) {
		if (count_keys) {
			err_msg("Conflicting --count and -%d arguments", get_key_no);
		} else if (get_key) {
			err_msg("Conflicting --key=%s and -%d arguments", get_key, get_key_no);
		} else if (check != json_unknown) {
			err_msg("Conflicting --json and -%d arguments", get_key_no);
		}
		quiet = no_messages = 1;
	}

	for (arg = optind; (arg < argc); arg++) {
		static int got_in = 0;
			
		if (!got_in++ && !string) {
			if ((yyin = fopen(argv[arg], "r")) == 0) {
				no_messages = 0;
				err_msg("Could not open file \"%s\" for input", argv[arg]);
			}
		} else {
			no_messages = 0;
			err_msg("Only one source can be parsed at a time");
		}
	}

	if (!rc) {
		if (timeout && !(yyin || input_timeout(STDIN_FILENO, timeout))) {
			err_msg("Timeout waiting for input");
		}

		yy = _yylex();

		if (yy == BEGIN_OBJECT) { // "{ ... }"

#ifdef WRAP
			if ( value_only ) {
				--level;
			}
#endif

			json = json_object;

			if (get_key) {
				no_messages = 1;
			} else if (get_type && !list_keys && !get_key_no && !check) {
				list_keys=3;
			}

			if (iterate = enumerate) {
				if (get_type) {
					list_keys = 3;
				}
				count_keys = quiet = 1;
				enumerate = 0;
			}

			yy = object(yy);

			if (yy != END_OBJECT) {
				no_messages = no_messages_arg;
				err_msg("Expected '}' but got \"%s\"", *_yytext ? _yytext : "EOF");
			}
		} else if (yy == BEGIN_ARRAY) { // "[ ... ]"
			if (get_key) {
				no_messages = 0;
				err_msg("Conflicting --key=%s with array object", get_key);
			} else if (get_key_no) {
				get_element_no = get_key_no;
		 		get_key_no = 0;
				quiet = no_messages = 1;
			} else if (list_keys) {
				quiet = quiet_arg;
				no_messages = no_messages_arg;
				list_elements = list_keys;
				list_keys = 0;
			} 
			if (get_type) {
				quiet = 1;
				
				list_elements = 2;
			}

			count_elements = count_keys;
			count_keys     = 0;

			if (enumerate) {
				quiet = 1;
				count_elements = 1;
			}

			json = json_array;

			yy = array(yy);
	
			if (yy != END_ARRAY) {
				err_msg("Expected ']' but got \"%s\"", *_yytext ? _yytext : "EOF");
			}
		} else { 
			err_msg("Expected '{' or '[' but got \"%s\"", *_yytext ? _yytext : "EOF");
		}

		if (yy = _yylex()) { //Should be at EOF...
			err_msg("Expecting EOF but got \"%s\"", *_yytext ? _yytext : "EOF");
		}
	
		if (check) {
			if (check_type) {
				quiet = quiet_arg;
				output("%s", json == json_object ? "OBJECT" : "ARRAY");
			}

			if ((check == json_array) && (json != json_array)) {
				vbs_msg("Source is a valid JSON object but not a JSON array");
				rc = 1;
			} else if ((check == json_object) && (json == json_array)) {
				vbs_msg("Source is a valid JSON array and not a JSON object");
				rc = 1;
			} else {
				vbs_msg("Source is a valid JSON %s", json == json_object ? "object" : "array");
			}
		} else {
			quiet = quiet_arg;
			no_messages = no_messages_arg;

			if (count_keys && !iterate) {
				output("%d", key_count);
			} else if (count_elements && !enumerate) {
				output("%d", element_count);
			} else if (get_key && !found_key) {
				err_msg("%s is not a key", get_key);
			} else if (get_key_no > key_count) {
				err_msg("There %s only %d key%s", key_count > 1 ? "are" : "is", key_count, key_count > 1 ? "s" : "");
			} else if (get_element_no > element_count) {
				err_msg("There %s only %d element%s", element_count > 1 ? "are" : "is", element_count, element_count > 1 ? "s" : "");
			}
		}	 
	}

	cleanup(rc);
}
