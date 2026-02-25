(identifier) @variable

((identifier) @constant
 (#match? @constant "^[A-Z][A-Z0-9_]*$"))

(import_statement name: (dotted_name (identifier) @module))
(import_from_statement module_name: (dotted_name (identifier) @module))
(import_from_statement name: (dotted_name (identifier) @variable))
(aliased_import alias: (identifier) @module)

(none) @constant.builtin
(true) @constant.builtin
(false) @constant.builtin
(integer) @number
(float) @number
(comment) @comment
(string) @string

(interpolation "{" @punctuation.special "}" @punctuation.special) @embedded

"-" @operator
"-=" @operator
"!=" @operator
"*" @operator
"**" @operator
"**=" @operator
"*=" @operator
"/" @operator
"//" @operator
"//=" @operator
"/=" @operator
"&" @operator
"&=" @operator
"%" @operator
"%=" @operator
"^" @operator
"^=" @operator
"+" @operator
"->" @operator
"+=" @operator
"<" @operator
"<<" @operator
"<<=" @operator
"<=" @operator
"<>" @operator
"=" @operator
":=" @operator
"==" @operator
">" @operator
">=" @operator
">>" @operator
">>=" @operator
"|" @operator
"|=" @operator
"~" @operator
"@=" @operator

"and" @keyword.directive
"in" @keyword.directive
"not" @keyword.directive
"or" @keyword.directive
"as" @keyword.directive
"assert" @keyword.directive
"continue" @keyword.directive
"break" @keyword.directive
"async" @keyword.directive
"await" @keyword.directive
"del" @keyword.directive
"elif" @keyword.directive
"else" @keyword.directive
"except" @keyword.directive
"finally" @keyword.directive
"for" @keyword.directive
"from" @keyword.directive
"import" @keyword.directive
"if" @keyword.directive
"match" @keyword.directive
"case" @keyword.directive
"return" @keyword.directive
"try" @keyword.directive
"while" @keyword.directive
"with" @keyword.directive
"pass" @keyword.directive
"yield" @keyword.directive
"raise" @keyword.directive

"class" @keyword
"def" @keyword
"is" @keyword
"global" @keyword
"lambda" @keyword
"nonlocal" @keyword

(attribute attribute: (identifier) @property)
(class_definition name: (identifier) @type)
(function_definition name: (identifier) @function)

(decorator) @function
(call function: (identifier) @function)
(call function: (attribute attribute: (identifier) @function.method))
(call function: (attribute object: (call function: (identifier) @constructor)))
(parameters (identifier) @variable.parameter)