/**
 * @file local_files.h
 *
 * Access local files within the repo
 */

#ifndef LOCAL_fILES_H_
#define LOCAL_fILES_H_

/** Sets the directory of the executable so we can get relaitve paths */
int set_local_directory(char *dir);
/** Gets a file relative to the path of the executable */
int get_local_file(char *out, char *file);

#endif
