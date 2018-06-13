# Name Resolution and Typechecking

## Motivation

The initial design of a pipeline style flow from text to program execution is not suitable when the typesystem becomes advanced enough that name resolution depends on typechecking and vice versa.

Instead, a circular execution model has to be used. We attempt to resolve as many names/references as possible, and move to typechecking when we get stuck. Then our typechecker should attempt to derive as many types as possible. This result should then allow the name resolver to make further progress. As long as each step in this process results in new resolved names or new calculated types our analysis is progressing. An error state is reached when we are not progressing but have unresolved names and uncalculable type expressions

Example of circularity

Example of error

## Data

To enable our resolver to use the results of the typechecker and vice versa we need a common data model. This model must be able to describe the results of both stages. To satisfy this requirement we must be able to store two pieces of information.

- Given a reference, what variable declaration does it reference
- Given a type, what members does it have (if any)

The first of these requirements requires further detail on how variable declarations are stored and differentiated. References will always point to the variable declaration in the closest scope. More detail on this will be discussed elsewhere, but it is enough to say that this approach is commonly used. The result of our name resolution will be a set of pairs, where each pair consists of a reference and the name it points to. To actually use the result in our typechecker we need a way to relate names (both references and declarations) to positions in the AST. We avoid using direct pointers to nodes as this will strongly couple the result of name resolution to the existing/in memory AST data. A simple solution is to store the number of scopes one must go up to find the environment where the declaration resides. 

The second requirement must similarly differentiate types when their names are the same. The method is actually the same as before.

By storing our data such that it can be used on its own, both stages will be able to use the results of the other. Another advantage is that we can store the results and use them at a later point, for example when we wish to implement incremental analysis.

## Implementation

We define the inner workings of our two stages, and a few operations that will allow us to work with the two data formats we defined above.

### Name resolution
Resolving a name means to find location in the AST where that variable or type was declared.

- `resolve_reference(variable_name, current_scope): location in AST of declaration`
- `resolve_type(type_name): qualified (full) type name`
- `get_type(qualified_type_name): type data object`

### Typechecking

-
-
-


