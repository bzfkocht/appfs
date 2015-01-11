/**@file      
 * @brief     Split line into fields  
 * @author    Thorsten Koch
 * @date      04 Jan 2015
 * @copyright (c) 2015 by Thorsten Koch <koch@zib.de>
 *
 * Copy a string and compute an array of pointers, pointing to the
 * parts of a string that were separated by white space.
 * We call each part a field. Each field is terminate by '\0'.
 * If the string contains any character from a list if 'comment characters' end
 * the processing with the last character before the first occurance of any the
 * comment characters.
 *
 * Example:
 * String="Hi there, how are you # more text"
 * Comment characters="@#%"
 *
 * The result should be an array of pointers, pointing to
 * "Hi", "there,", "how", "are", "you".
 * All whitespace is removed, as are all unprintable characters.
 */  
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "mshell.h"
#include "splitline.h"

struct line_fields    ///< fields of a (input)t line, i.e. string data structure
{
   int         size;  ///< maximum number of fields allocated
   int         used;  ///< number of fields used so far
   char*       line;  ///< pointer to the input line
   char**      field; ///< array of pointer [size] to the fields of the line
};

#define LFS_INITIAL_SIZE   100   ///< initial allocation size of the line

#ifndef NDEBUG // the following functions are only used within asserts.

/** Check whether the LFS data structure is consistent
 */
static bool is_valid(const LFS* lfs)
{
   if (NULL == lfs || NULL == lfs->line || NULL == lfs->field)
      return false;

   /* Initial size has at least to be allocated, the allocated size has to be
    * less than the used size, and the used size has to be non-negative.
    */
   if (lfs->size < LFS_INITIAL_SIZE || lfs->size < lfs->used || lfs->used < 0)
      return false;
   
   return true;   
}

#endif // NDEBUG

/** Free an LFS data structure
 */
void lfs_free(LFS* lfs)
{
   assert(is_valid(lfs));

   free(lfs->line);
   free(lfs->field);
   free(lfs);      
}

/** Given a line, return an LFS data structure containing the pointers to the fields.
 */
LFS* lfs_split_line(
   LFS*        lfs,      ///< [in,out] a previous used LFS or NULL
   const char* line,     ///< [in] the input line to process
   const char* comment)  ///< [in] a list of characters that ends the part of the line to be processed
{
   assert(NULL != line);
   assert(NULL != comment);

   /* Do we need a new LFS data structure?
    */
   if (NULL == lfs)
   {
      lfs        = calloc(1, sizeof(*lfs));
      lfs->size  = LFS_INITIAL_SIZE;
      lfs->field = calloc((size_t)lfs->size, sizeof(*lfs->field));
   }
   else /* no, just free the old line */
   {
      assert(is_valid(lfs));
      
      lfs->used = 0;
      
      free(lfs->line);
   }
   lfs->line = strdup(line); 

   /* clip the line at the first occurence of a character from the comment string
    */
   char* s = strpbrk(lfs->line, comment); 

   if (NULL != s) /* else line is not terminated or too long */
      *s = '\0';  /* clip comment */
       
   /* Check for garbage characters and remove them, this includes "\n", "\r"
    */
   for(s = lfs->line; *s != '\0'; s++)
      if (!isprint(*s))
         *s = ' ';

   /* We have out line and skip over initial white space
    */
   for(s = lfs->line; isspace(*s); s++)
      ;

   /* As long as there is something left from the linem, do
    */
   while(*s != '\0')
   {
      assert(lfs->used <= lfs->size);

      /* If we are running out of space, we increase the allocation
       */
      if (lfs->used == lfs->size)
      {
         lfs->size *= 2;
         lfs->field = realloc(lfs->field, sizeof(*lfs->field) * (size_t)lfs->size);
      }
      assert(!isspace(*s));

      /* start the new field
       */
      lfs->field[lfs->used] = s;
      lfs->used++;

      /* As long as we are not at the end and not running into white space
       * we proceed. 
       */
      while(*s != '\0' && !isspace(*s))
         s++;

      /* We are at the end of the field. Either the input string is
       * at the end anyway, ...
       */
      if (*s != '\0')
      {
         /* ... or, we add a '\0' and skip any whitespace to the next field
          */
         *s++ = '\0';
      
         while(isspace(*s))
            s++;
      }
   }
   return lfs;   
}

/** Get number of fields used
 *  @return Number of fields (>= 0)
 */
int lfs_fields_used(const LFS* lfs)
{
   assert(is_valid(lfs));

   return lfs->used;
}

/** Get a particular field
 *  @return pointer to field number fno
 */
const char* lfs_field(
   const LFS* lfs,
   int        fno)        ///< [in] field number (0 <= fno < lfs_used_fields())
{
   assert(is_valid(lfs));

   /* Design decision:
    */
   assert(fno >= 0);
   assert(fno <  lfs->used);

   /* The above could be dependend on the input.
    * we have to make sure  it is not.
    * otherwise:
    * if (fno >= lfs->used) return NULL
    * problem: we always have to check.
    * Design by contract
    */
   return lfs->field[fno];
}

/** Print content of LFS structure.
 *  Useful for debugging purposes
 */
void lfs_print(const LFS* lfs, FILE* fp)
{
   assert(is_valid(lfs));
   assert(NULL != fp);

   for(int i = 0; i < lfs->used; i++)
      fprintf(fp, "Field %3d: \"%s\"\n", i, lfs->field[i]);
}
