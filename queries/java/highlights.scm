; Variables
(identifier) @variable

; Constants
((identifier) @constant
 (#match? @constant "^_*[A-Z][A-Z0-9_]*$"))

; Punctuation
["(" ")" "[" "]" "{" "}"] @punctuation
[";" "." "," ":" "@" "..."] @punctuation.special

; Keywords
"abstract" @keyword
"class" @keyword
"enum" @keyword
"extends" @keyword
"final" @keyword
"implements" @keyword
"interface" @keyword
"native" @keyword
"private" @keyword
"protected" @keyword
"public" @keyword
"record" @keyword
"static" @keyword
"strictfp" @keyword
"synchronized" @keyword
"transient" @keyword
"volatile" @keyword
"requires" @keyword
"exports" @keyword
"opens" @keyword
"uses" @keyword
"provides" @keyword
"with" @keyword
"to" @keyword
"transitive" @keyword
"permits" @keyword
"sealed" @keyword
"non-sealed" @keyword
"instanceof" @keyword
"open" @keyword

"try" @keyword.directive
"catch" @keyword.directive
"finally" @keyword.directive
"break" @keyword.directive
"case" @keyword.directive
"continue" @keyword.directive
"do" @keyword.directive
"else" @keyword.directive
"for" @keyword.directive
"if" @keyword.directive
"return" @keyword.directive
"switch" @keyword.directive
"while" @keyword.directive
"throw" @keyword.directive
"throws" @keyword.directive
"yield" @keyword.directive
"new" @keyword.directive
"when" @keyword.directive
"assert" @keyword.directive
"default" @keyword.directive
"import" @keyword.directive
"package" @keyword.directive
"module" @keyword.directive

; Operators
"--" @operator
"-" @operator
"-=" @operator
"->" @operator
"=" @operator
"!=" @operator
"!" @operator
"^" @operator
"^=" @operator
"*" @operator
"*=" @operator
"/" @operator
"/=" @operator
"%" @operator
"%=" @operator
"&" @operator
"&=" @operator
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
"|" @operator
"|=" @operator
"?" @operator
"<<" @operator
">>" @operator
">>>" @operator

; Literals
[
  (decimal_integer_literal)
  (hex_integer_literal)
  (octal_integer_literal)
  (binary_integer_literal)
  (decimal_floating_point_literal)
  (hex_floating_point_literal)
] @number

[
  (character_literal)
  (string_literal)
] @string
(escape_sequence) @string.escape

[
  (true)
  (false)
  (null_literal)
] @constant.builtin

(this) @variable.builtin
(super) @variable.builtin

; Properties / Labels
(field_access field: (identifier) @property)
(labeled_statement (identifier) @label)

; Types
(type_identifier) @type
(class_declaration name: (identifier) @type)
(interface_declaration name: (identifier) @type)
(enum_declaration name: (identifier) @type)
(record_declaration name: (identifier) @type)
(annotation_type_declaration name: (identifier) @type)
[
  (integral_type)
  (floating_point_type)
  (boolean_type)
  (void_type)
] @type

; Functions & Methods
(method_declaration
  name: (identifier) @function.method)

(method_invocation
  name: (identifier) @function.method)

(constructor_declaration
  name: (identifier) @function.method)

; Annotations
(annotation
  name: (identifier) @type)
  
(marker_annotation
  name: (identifier) @type)

; Comments (Fixes the offset 2616 error)
[
  (line_comment)
  (block_comment)
] @comment