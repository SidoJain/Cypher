; Variables
(identifier) @variable

; Constants
((identifier) @constant
 (#match? @constant "^_*[A-Z][A-Z0-9_]*$"))

(const_spec
  name: (identifier) @constant)

; Punctuation
["(" ")" "[" "]" "{" "}"] @punctuation
[";" "." "," ":"] @punctuation.special

; Keywords
"chan" @keyword
"const" @keyword
"func" @keyword
"interface" @keyword
"map" @keyword
"package" @keyword
"struct" @keyword
"type" @keyword
"var" @keyword

"defer" @keyword.directive
"go" @keyword.directive
"break" @keyword.directive
"case" @keyword.directive
"continue" @keyword.directive
"default" @keyword.directive
"else" @keyword.directive
"fallthrough" @keyword.directive
"for" @keyword.directive
"goto" @keyword.directive
"if" @keyword.directive
"import" @keyword.directive
"range" @keyword.directive
"return" @keyword.directive
"select" @keyword.directive
"switch" @keyword.directive

; Operators
[
  "--"
  "-"
  "-="
  ":="
  "!"
  "!="
  "..."
  "*"
  "*="
  "/"
  "/="
  "&"
  "&&"
  "&="
  "%"
  "%="
  "^"
  "^="
  "+"
  "++"
  "+="
  "<-"
  "<"
  "<<"
  "<<="
  "<="
  "="
  "=="
  ">"
  ">="
  ">>"
  ">>="
  "|"
  "|="
  "||"
  "~"
] @operator

; Literals
[
  (int_literal)
  (float_literal)
  (imaginary_literal)
] @number

[
  (interpreted_string_literal)
  (raw_string_literal)
  (rune_literal)
] @string

(escape_sequence) @string.escape

[
  (true)
  (false)
  (nil)
  (iota)
] @constant.builtin

; Properties / Labels
(field_identifier) @property
(label_name) @label

; Types
(type_identifier) @type

((type_identifier) @type
 (#match? @type "^(bool|byte|complex64|complex128|error|float32|float64|int|int8|int16|int32|int64|rune|string|uint|uint8|uint16|uint32|uint64|uintptr)$"))

; Functions & Methods
(method_elem
  name: (field_identifier) @function.method)

(function_declaration
  name: (identifier) @function)

(method_declaration
  name: (field_identifier) @function.method)

(call_expression
  function: (identifier) @function)

(call_expression
  function: (identifier) @function.builtin
  (#match? @function.builtin "^(append|cap|close|complex|copy|delete|imag|len|make|new|panic|print|println|real|recover)$"))

(call_expression
  function: (selector_expression
    field: (field_identifier) @function.method))

; Comments
(comment) @comment