(identifier) @variable

((identifier) @constant
 (#match? @constant "^[A-Z][A-Z0-9_]*$"))

["(" ")" "[" "]" "{" "}"] @punctuation
[";" "." "," ":" "::"] @punctuation.special

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
"namespace" @keyword
"class" @keyword
"constexpr" @keyword
"constinit" @keyword
"consteval" @keyword
"explicit" @keyword
"final" @keyword
"friend" @keyword
"mutable" @keyword
"noexcept" @keyword
"override" @keyword
"private" @keyword
"protected" @keyword
"public" @keyword
"template" @keyword
"typename" @keyword
"concept" @keyword
"requires" @keyword
"virtual" @keyword
"module" @keyword
"export" @keyword
"import" @keyword

"co_await" @keyword.directive
"co_yield" @keyword.directive
"co_return" @keyword.directive
"try" @keyword.directive
"catch" @keyword.directive
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
"throw" @keyword.directive
"new" @keyword.directive
"delete" @keyword.directive

"using" @keyword.directive
"#include" @keyword.directive
"#define" @keyword.directive
"#if" @keyword.directive
"#ifdef" @keyword.directive
"#ifndef" @keyword.directive
"#else" @keyword.directive
"#elif" @keyword.directive
"#endif" @keyword.directive

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

(number_literal) @number
(char_literal) @character
(string_literal) @string
(raw_string_literal) @string
(system_lib_string) @string

(true) @boolean
(false) @boolean
(null) @constant.builtin
("nullptr") @constant.builtin
(this) @constructor

(field_identifier) @property
(statement_identifier) @label
(type_identifier) @type
(primitive_type) @type.builtin
(sized_type_specifier) @type.builtin
(auto) @type.builtin

(namespace_identifier) @constructor
(module_name (identifier) @constructor)

(function_declarator
  declarator: (identifier) @function)

(function_declarator
  declarator: (qualified_identifier
    name: (identifier) @function))

(function_declarator
  declarator: (field_identifier) @function.method)

(operator_name) @function.method
(operator_cast) @function.method

(call_expression
  function: (identifier) @function)

(call_expression
  function: (qualified_identifier
    name: (identifier) @function))

(call_expression
  function: (field_expression
    field: (field_identifier) @function.method))

(template_function
  name: (identifier) @function)

(template_method
  name: (field_identifier) @function.method)

; Force ambiguous local function declarations to be variables
(compound_statement
  (declaration
    declarator: (function_declarator
      declarator: (identifier) @variable)))

(compound_statement
  (declaration
    declarator: (function_declarator
      parameters: (parameter_list
        (parameter_declaration
          type: (type_identifier) @variable)))))

((template_function
  name: (identifier) @type)
 (#match? @type "^(vector|string|map|set|list|array|deque|stack|queue|priority_queue|pair|tuple|unique_ptr|shared_ptr|weak_ptr)$"))

((call_expression
  function: (identifier) @type)
 (#match? @type "^(vector|string|map|set|list|array|deque|stack|queue|priority_queue|pair|tuple|unique_ptr|shared_ptr|weak_ptr)$"))

((call_expression
  function: (qualified_identifier
    name: (identifier) @type))
 (#match? @type "^(vector|string|map|set|list|array|deque|stack|queue|priority_queue|pair|tuple|unique_ptr|shared_ptr|weak_ptr)$"))

(comment) @comment