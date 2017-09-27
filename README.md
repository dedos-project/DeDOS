# Dedos

This repository contains the codebase for the DeDOS runtime and global controller

## Description

The goal of this project is to create fundamentally new defenses against distributed
denial-of-service (DDoS) attacks that can provide far greater resilience to these attacks
compared to existing solutions. Today's responses to DDoS attacks largely rely on old-school
network-based filtering or scrubbing, which are slow and manual, and cannot handle new attacks.
DeDOS takes a radically different approach that combines techniques from declarative programming,
program analysis, and real-time resource allocation in the cloud.

More details are available at [http://dedos.cis.upenn.edu/](http://dedos.cis.upenn.edu/)

## Building
Available build commands are:
```bash
make runtime            # Builds the rt executable
make global_controller  # Builds the global_controller executable
make                    # Bulds both rt and global_controller
make test               # Runs both global_controller and runtime test scripts
make clean              # Executes both global_controller and runtime's "clean"
```

## Code layout
The global controller and runtime are built off of a partially-shared codebase, though each
are built with their own makefile (`global_controller.mk` and `runtime.mk`, respectively).

Code interacting with only the global controller should go in the directory
`src/global_controller/`, and with the runtime in `src/runtime/`, while code shared
between the two executables goes in `src/common`. Application-specific MSU code is located
in `src/msus/<application_name>/`. C or C++ files placed in the source directories are
automatically discovered, compiled, and linked by the makefiles.

Tests are placed in the `test/` folder, and should match the subdirectory and file of the
file under test. For example, the code testing `src/common/dfg.c` should be placed in the file
`src/common/Test_dfg.c`. This ensures that linking is performed properly. More details are 
available on the wiki.

## Contributing

We will be using the git-flow model for code contributions.

See
[here](http://nvie.com/posts/a-successful-git-branching-model/)
for a thorough description, and
[here](https://datasift.github.io/gitflow/IntroducingGitFlow.html)
for a brief overview.

In brief, when contributing a new feature, use the following protocol:
* Branch from dev into a branch for your feature
* Make changes and commit within feature branch
* Merge (**with --no-ff**) back into dev
* Delete the feature branch
* Push back to `origin dev`

*Note: --no-ff ensures that the history of a branch remains consistant, 
and that it does not simply overwrite the history of the merged branch. 
See [here](http://nvie.com/img/merge-without-ff@2x.png) for a consise 
explanation*

That would look like:
```bash
$ git checkout -b myfeature dev
$ # Make your changes here
$ git commit
$ git checkout dev
$ git merge --no-ff myfeature
$ git push origin dev
```

In addition, please make small commits --
the commit message should be able to summarize all
changes made in a single message.

Prior to releases, the dev branch will be merged back into `origin master` and
tagged appropriately.

## Code style

A summary of code style guidelines follows. For any other questions not
addressed by the following, refer to
[Google's C++ style guide](https://google.github.io/styleguide/cppguide.html).

##### Whitespace
Four-space indent. Eight-space if continuing a line. No tabs. No trailing spaces.
The following two lines in a .vimrc file will help:
```vim
:highlight ExtraWhitespace ctermbg=darkgreen guibg=darkgreen
:match ExtraWhitespace /\s\+\%#\@<!$/
```

##### Short functions
If a function exceeds ~40 lines, try to break it up into smaller functions,
if doing so does not harm structure or significantly impact efficiency

##### Maintain scope using headers
Where possible, declare in headers only those functions which are meant to be
part of the external interface. Declare functions as `static` which are not meant to be used
externally. This helps to protect the global namespace.

##### Comment style
We will be using
[doxygen](https://www.stack.nl/~dimitri/doxygen/manual/docblocks.html)
for automatic documentation generation.

Use the Javadoc style (`/** Explanation */`) before function definitions.
If commenting after a variable makes things clearer, use
`/**< description */` instead.

Be sure to provide documentation for every public function and variable
declaration in header files.
More verbose documentation can be provided by the definition. We will be using
the
[JAVADOC_AUTOBRIEF](https://www.stack.nl/~dimitri/doxygen/manual/config.html#cfg_javadoc_autobrief)
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

##### Self-contained headers
Headers should `#include` all files that are necessary to compile the header,
such that they should not be affected by the order in which they are included.

Always include the header for the corresponding `.c`/`.cc` file _first_. Other
necessary project includes should follow, followed by library includes. This helps
to ensure that headers are not dependent on additional library includes.

##### 100 character Line length
Try to keep your lines to a maximum of 100 characters. Use the following line
in .vimrc as a subtle reminder
```vim
:set colorcolumn=100
```

