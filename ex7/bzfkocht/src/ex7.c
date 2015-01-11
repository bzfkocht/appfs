/**@file      
 * @brief     Appfs Exercise 7: BIP Enumerator
 * @author    Thorsten Koch
 * @date      04 Jan 2015
 * @version   3.0.0
 * @copyright (c) 2015 by Thorsten Koch <koch@zib.de>
 */

/**@mainpage Advanced (practical) Programming (for Scientists) Exercise 7

\section sec1 What do do?

Write a program in C, which enumerates all solutions to a binary
program of the form:
\f[ Ax\leq b, x\in\{0,1\} \f]

with \f$ A \f$ being a \f$ m\times n \f$ matrix, and \f$ b \f$ a \f$ m \f$ vector, 
with \f$ m,n\leq 32 \f$.

Example:

\f{eqnarray*}{
2 x_1 + 3 x_2 + 5 x_3 - 4 x_4 &\leq& 8\\
3 x_1 - 6 x_2 + 8 x_4 & \leq & 10\\
x_3 + x_4 &\leq& 1
\f}
The input format for the above would be:

\verbatim
4  # columns (variables)
4  # rows (constraints)
 2   3    5    4   <= 8
 3   6    0    8   <= 10
 0   0    1    1    = 1
12.5 0.3 22.12 0.0 >= 17.5
\endverbatim

Everything after an \# in a line should be discarded as a comment.

The program should
- read the data from a file and write out all solution vectors
- be able to cope with all kinds of inputs and either work or give an error message
- never crash
- be able to work with \f$ \leq \f$, \f$ \geq \f$, and \f$ = \f$
- measure the computing time for the enumeration
- be split into more than one file 
- have a Makefile with additional targets: clean, check
*/   

#include <unistd.h> /* getopt belongs to POSIX.1-2001. Does only work if gcc is not set to strict ansi */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "lint.h"
#include "mshell.h" 
#include "bip.h"

#define GET_SEC(a, b)  ((double)(b - a) / (double)CLOCKS_PER_SEC) 

/** Print a feasible solution to a BIP given as bitvector.
 *  If we are not at least in #VERB_VERBOSE mode, print nothing,
 *  otherwise decode the bits given in the solution vector x and
 *  print the solution.
 *  The lowest bit corresponds to the first variable
 */
static void report_sol(
   const BIP*      bip,       ///< [in] binary program
   unsigned int    x)         ///< [in] solution vector
{
   assert(NULL != bip);

   if (VERB_VERBOSE <= bip_verbosity_level(bip))
   {
      printf("%8x: ", x);

      unsigned int bit = 1;
      
      for(int col = 0; col < bip_cols(bip); col++, bit += bit)
         printf("%c ", (x & bit) ? '1' : '0');

      putchar('\n');
   }
}

/** Program to enumerate all solutions to a binary program with at most 32 variables.
 *  First read the file, enumerate the solutions.
 */
int main(int argc, char** argv) 
{
   static const char* const banner = 
      "***************************************\n"  \
      "* EX7 - APPFS BIP Enumeration Program *\n"  \
      "* Copyright (C) 2014 by Thorsten Koch *\n"  \
      "***************************************\n";  

   static const char* const options = "hv:V";
   static const char* const usage   = "usage: %s [options] file.dat\n";

   static const char* const help    =
      "\n"                                  
      "  -h             show this help.\n"                          
      "  -v[0-4]        verbosity level: 0 = quiet, 1 = default, up to 5 = debug\n" 
      "  -V             print program version\n"                        
      "  file.dat       is the name of the BIP input file.\n"           
      "\n"
      ; 

   assert(sizeof(int)  >= 4); //lint !e506 Constant value Boolean: we assume int  to be at least 32 bit   
   assert(sizeof(long) >= 8); //lint !e506 Constant value Boolean: we assume long to be at least 64 bit

   /** Set verbosity level for the program.
    *  This variable might be used globally, or it has to be passed
    *  an any single function that produces output.
    */
   int verb_level = VERB_NORMAL;
   int ret_code   = EXIT_FAILURE;
   int option;
   
   while((option = getopt(argc, argv, options)) != -1)
   {
      switch(option)
      {
      case 'h' : // help
         printf(banner);
         printf(usage, argv[0]);
         puts(help);
         return EXIT_SUCCESS;
         
      case 'v' : // set verbosity level
         verb_level = atoi(optarg);

         if (verb_level < VERB_QUIET || verb_level > VERB_DEBUG)
            return fprintf(stderr, usage, argv[0]), EXIT_FAILURE;

         break;

      case 'V' : // Version
         printf("%s\n", VERSION);
         return EXIT_SUCCESS;

      case '?': // anything else
         fprintf(stderr, usage, argv[0]);
         return EXIT_SUCCESS;
         
      default :
         abort();
      }
   }
   /* Is there an argument with the filename left?
    */
   if ((argc - optind) < 1)
   {
      fprintf(stderr, usage, argv[0]);      
      return EXIT_FAILURE;
   }
   if (verb_level >= VERB_NORMAL)
      printf(banner);

   BIP* bip = bip_init(verb_level);
   
   if (!bip_read(bip, argv[optind]))
   {
      if (VERB_CHATTER <= verb_level)
         bip_print(bip, stdout);

      clock_t       start     = clock();
      long          solutions = bip_enumerate(bip, report_sol);
      unsigned long count     = 1Lu << bip_cols(bip);   
      double        elapsed   = GET_SEC(start, clock());
      
      if (VERB_NORMAL <= verb_level)
      {
         printf("Checked %lu vectors in %.3f s = %.3f kvecs/s\n",
            count, elapsed, (double)count / elapsed / 1000.0);
         
         printf("Found %ld feasible solutions\n", solutions);
      }
      ret_code = EXIT_SUCCESS;
   }
   bip_exit(bip);   

   mem_maximum(stdout);
   
   return ret_code;
}
