# Dedos

This repository contains the codebase for the DeDOS runtime and global controller

## Code style

A summary of code style guidelines follows. For any other questions not 
addressed by the following, refer to Google's C++ style guide:
[[https://google.github.io/styleguide/cppguide.html]].

### Summary of code style guidelines

#### Whitespace
Four-space indent. Eight-space if continuing a line. No tabs. No trailing spaces.
The following two lines in a .vimrc file will help:
```
:highlight ExtraWhitespace ctermbg=darkgreen guibg=darkgreen
:match ExtraWhitespace /\s\+\%#\@<!$/
```

#### Short functions
If a function exceeds ~40 lines, try to break it up into smaller functions,
if doing so does not harm structure or significantly impact efficiency

#### Maintain scope using headers
Where possible, declare in headers only those functions which are meant to be
"public" (part of the external interface). Helper functions, intended only to 
be used within that file, need not be declared in the header.

#### Comment style
We will be using
[https://www.stack.nl/~dimitri/doxygen/manual/docblocks.html doxygen]
for automatic documentation generation.

Use the Javadoc style (`/** Explanation */`) before function definitions.
If commenting after a variable makes things clearer, use
`/**< description */` instead.

Be sure to provide documentation for every public function and variable 
declaration in header files.
More verbose documentation can be provided by the definition. We will be using
the 
[https://www.stack.nl/~dimitri/doxygen/manual/config.html#cfg_javadoc_autobrief JAVADOC_AUTOBRIEF]
feature, which extracts up to the first period as the function's brief description.

Example:

```c

/** Provides an example of a variable */
int i_am_an_int;

char first_part;  /**< This description comes after the variable */
char second_part; /**< It sometimes makes things easier to read */

/** 
 * This is a brief description of what the function does.
 * If more details are provided, they go here. Even more details...
 * @param something the input to be transformed.
 * @param output the output is placed in this variable
 * @return 0 on success, -1 on error
 */
int i_am_function(int something, char *output);
```

#### Self-contained headers
Headers should `#include` all files that are necessary to compile the header,
such that they should not be affected by the order in which they are included.

#### 100 character Line length
Try to keep your lines to a maximum of 100 characters. 