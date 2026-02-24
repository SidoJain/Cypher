(identifier) @variable

((identifier) @constant
 (#match? @constant "^[A-Z][A-Z\\d_]*$"))

"const" @keyword
"enum" @keyword
"extern" @keyword
"inline" @keyword
"sizeof" @keyword
"static" @keyword
"struct" @keyword
"typedef" @keyword
"union" @keyword
"volatile" @keyword

"break" @keyword.directive
"case" @keyword.directive
"continue" @keyword.directive
"default" @keyword.directive
"do" @keyword.directive
"else" @keyword.directive
"for" @keyword.directive
"if" @keyword.directive
"return" @keyword.directive
"switch" @keyword.directive
"while" @keyword.directive

"#define" @keyword.directive
"#elif" @keyword.directive
"#else" @keyword.directive
"#endif" @keyword.directive
"#if" @keyword.directive
"#ifdef" @keyword.directive
"#ifndef" @keyword.directive
"#include" @keyword.directive
(preproc_directive) @keyword.directive

"--" @operator
"-" @operator
"-=" @operator
"->" @operator
"=" @operator
"!=" @operator
"^" @operator
"^=" @operator
"*" @operator
"/" @operator
"/=" @operator
"&" @operator
"&&" @operator
"+" @operator
"++" @operator
"+=" @operator
"<" @operator
"<=" @operator
"==" @operator
">" @operator
">=" @operator
"||" @operator
":" @operator
"?" @operator

"." @delimiter
";" @delimiter

(string_literal) @string
(system_lib_string) @string

(null) @constant
(true) @boolean
(false) @boolean
(number_literal) @number
(char_literal) @character

(field_identifier) @property
(statement_identifier) @label
(type_identifier) @type
(primitive_type) @type.builtin
(sized_type_specifier) @type.builtin

(call_expression
  function: (identifier) @function)
(call_expression
  function: (field_expression
    field: (field_identifier) @function))
(function_declarator
  declarator: (identifier) @function)
(preproc_function_def
  name: (identifier) @function.special)

(comment) @comment