Lexing Test
===========

The goal is to see how quickly one can lex and parse a simple fake language.
The repo is named "lexing test" in recognition of the fact that the lexing part is probably harder to do quickly.

Problem Statement
-----------------

Your input files are guaranteed to be 7-bit ASCII.
The language in question has the following syntax:
```rust
// Line comments.

/*
/*
Block comments...
*/
... that nest.
*/

// The only top-level construct is let:
let identifier := expression;

// Juxtaposition is a left-associative application operator.
// The following two must parse the same.
let example1 := function arg1 arg2 arg3;
let example2 := ((function arg1) arg2) arg3;
let example3 := but (this (parses differently));

// The keyword "fun" makes a lambda. For simplicity only a single argument is allowed.
let example4 := fun argname => body;
let example5 := (fun a => (fun b => fun c => c b) a fun b => a d) e;

// We allow string literals, because they're important in real lexing.
let example6 := fun f => f "string literal";

// Additionally, we may escape a close quote.
let example7 := "Still \" in the \\ string\n";

// ... and that's it.
```

In the ASCII version an identifier may begin with `[a-zA-Z_]` and may continue with any of `[a-zA-Z0-9_]`, except it may not be either of the keywords `let` or `fun`.
There is no need to sanity check anything when parsing (e.g. no need to check that identifiers aren't redefined, or that things make any sense).
Note that `//`, `/*`, and `*/` may appear inside of strings, but that block comments should *not* skip a `/*` or `*/` inside of what could be interpreted as a string.

Setup
-----

Simply run `python generate.py` to populate `files/` with the test files (either Python 2 or Python 3).
The generated files should be byte-for-byte identical every time, and should match the hashes below.

```
$ time python3 generate.py
Filling: files/source_1k.txt
Filling: files/source_10k.txt
Filling: files/source_100k.txt
Filling: files/source_1M.txt

real	0m41.659s
user	0m41.276s
sys	0m0.381s
$ sha256sum files/*
0b3ddda756b27b3de09db5813075e2f3725756744fc4b46f018cf1f4c82002a4  files/source_100k.txt
4d9e60e282a3cba5b8bfbdc84c37d45c6fb5664367e5205ad07a5faa351b6e87  files/source_10k.txt
e20e3b141c8bfdc3067219a9c3fa0ad9df9a0f5fa613b7ea40c28b86d0ebcfcf  files/source_1k.txt
3c30598ffc388b77f3c4d6c3cc85b3f172f6625a8ca679f7676dc0504e9b643c  files/source_1M.txt
```

Each file contains ~60% source lines, ~15% blank lines, and ~25% comment lines, where the number of source lines is exactly equal to the number in the path.
For example, `files/source_1M.txt` will contain exactly 1,000,000 source lines, 277,626 blank lines, and 458,004 lines of comments.

Edge Cases
----------

The juxtaposition of string literals does *not* concatenate them (like it would in C), and is just a normal (albeit nonsensical) function application.
That is, you must parse these two the same:
```rust
let example8 := "foo" "bar";
let example9 := (("foo") ("bar"));
```

Multiline strings are not allowed.
The only escapes that I will generate inside of a string literal are `\\`, `\n`, and `\"`.
You are free to report an error in all other cases.

Your program mustn't get confused by code that appears in comments, nor `//`, `/*`, or `*/` inside of string literals.
There is one small edge case here, which is that inside of a block comment you should just naively decrement the nesting level at each `*/` you see, regardless of whether or not it locally looks like it's inside of a string literal.
This is the same behavior as Rust.
To be very explicit, your code *should* fail on the following (as Rust would):
```rust
/*
let string_literal := "*/";

Whoops! We're no longer in a block comment! This is a syntax error!
*/
```

License
-------

All code here is released by me (Peter Schmidt-Nielsen) dually under the CC0 license and the MIT license.

To Do
-----

I'd like to add a Unicode version of the test, where you're required to appropriately lex assuming UTF-8, find the character boundaries, accept Unicode identifiers, and NFKC normalise them.

