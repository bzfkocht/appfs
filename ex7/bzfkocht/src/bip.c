/**@file      
 * @brief     BIP Enumerator 
 * @author    Thorsten Koch
 * @date      04 Jan 2015
 * @copyright (c) 2015 by Thorsten Koch <koch@zib.de>
 *
 * This files contains the core reoutines for the enumerator,
 * the definition if the data structure, and the supporting
 * routines to read files and manipulate the data structure.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <float.h>
#include <fenv.h>

#include "lint.h"
#include "mshell.h"  
#include "splitline.h"
#include "bip.h"

#define BIP_MAX_COLS   32
#define BIP_MAX_ROWS   128

#define MAX_LINE_LEN   512  ///< Maximum input line length

#define DEBRUIJN32     0x077CB531U

typedef enum { READ_ERROR, READ_COLS, READ_ROWS, READ_COEF } LINE_MODE;

struct binary_program                              ///< structure to store a binary program
{
   int       rows;                                 ///< number of rows (constraints)
   int       cols;                                 ///< number of columns (variables)
   double    ar      [BIP_MAX_ROWS][BIP_MAX_COLS]; ///< coefficient matrix store row wise
   double    rhs     [BIP_MAX_ROWS];               ///< right hand side
   Sense     sense   [BIP_MAX_ROWS];               ///< equation sense   

   int       equs;                                 ///< number of equations, -1 = preprocessing not done yet
   double    ac      [BIP_MAX_COLS][BIP_MAX_ROWS]; ///< coef matrix stored column wise
   double    rhs_ord [BIP_MAX_ROWS];               ///< reordered right hand side
  
   double    min_coef_val;                         ///< biggest integer we can represent
   double    max_coef_val;                         ///< smallest integer we can represent
   int       verb_level;                           ///< verbosity level
   int       read_rows;                            ///< rows read so far (only need for reading routine)

};

#ifndef NDEBUG // the following functions are only used within asserts.

/** Check whether BIP data structure is consistent.
 *
 *  If BIP is in stage BIP_STAGE_INIT then row == cols == 0.
 *  If BIP is in stage BIP_STAGE_READ then equs == -1.
 *  We are always checking at least the level requested by stage,
 *  and at least the real stage.
 *
 *  @return true if ok, false otherwise.
 */
static bool bip_is_valid(const BIP* bip)
{
   if (NULL == bip)
      return false;

   if (bip->max_coef_val <=  bip->min_coef_val) // bip_init not called.
      return false;

   if (bip->rows < 0 || bip->rows > BIP_MAX_ROWS)
      return false;

   if (bip->cols < 0 || bip->cols > BIP_MAX_COLS)
      return false;

   if ((bip->rows > 0 && bip->cols == 0) || (bip->rows == 0 && bip->cols > 0))
      return false;
   
   if (bip->equs > bip->rows)
      return false;

   int nzo_r = 0;
   int nzo_c = 0;
   
   for(int r = 0; r < bip->rows; r++)
   {
      for(int c = 0; c < bip->cols; c++)
      {
         if (bip->ar[r][c] < bip->min_coef_val || bip->ar[r][c] > bip->max_coef_val) 
            return false;
         
         if (bip->rhs[r] < bip->min_coef_val || bip->rhs[r] > bip->max_coef_val) 
            return false;

         if (bip->ar[r][c] != 0.0)
            nzo_r++;
         
         if (bip->ac[c][r] != 0.0)
            nzo_c++;
      }
   }
   if (nzo_r != nzo_c)
      return false;
   
   return true;
}

/** Check whether a particular vector is a feasible solution to a BIP.
 *  @return true if feasible, false otherwise.
 */
static bool bip_is_feasible(const BIP* bip, const unsigned int x)
{
   assert(bip_is_valid(bip));
   
   int r;
   
   for(r = 0; r < bip->rows; r++)
   {
      unsigned int bit = 1;
      double       lhs = 0;
      
      for(int c = 0; c < bip->cols; c++)
      {
         if (x & bit)
            lhs += bip->ar[r][c];

         assert(fetestexcept(FE_ALL_EXCEPT & ~FE_INEXACT) == 0);

         bit += bit;
      }
      assert(bip->sense[r] == EQ || bip->sense[r] == LE || bip->sense[r] == GE);
      
      if (  (bip->sense[r] == EQ && fabs(lhs - bip->rhs[r]) > 1e-6) // remember all number have bveen scaled to integer  
         || (bip->sense[r] == LE && lhs >  bip->rhs[r])
         || (bip->sense[r] == GE && lhs <  bip->rhs[r]))
         break;
   }
   return r == bip->rows;
}

#endif // !NDEBUG

/** Preprocess read in data and fill BIP data structure.
 *  - Reorders rows, equations first, counts equations.
 *  - Changes >= to <= by multiplying rows by -1
 *  - Copies row wise coefficient matrix into column wise data structure
 *  - Scales all coefficients to be integer
 */
static void bip_preprocess(BIP* bip)
{
   bip->equs = 0;
   
   /* copy equations to col order and remove GE
    */
   int row_cnt = 0;
      
   for(int r = 0; r < bip->rows; r++)
   {
      if (bip->sense[r] == EQ)
      {
         bip->rhs_ord[row_cnt] = bip->rhs[r];
         
         for(int c = 0; c < bip->cols; c++)
            bip->ac[c][row_cnt] = bip->ar[r][c];

         bip->equs++;
         row_cnt++;
      }
   }
   /* copy rest
    */
   for(int r = 0; r < bip->rows; r++)
   {
      switch(bip->sense[r])
      {
      case LE :
         bip->rhs_ord[row_cnt] = bip->rhs[r];
         
         for(int c = 0; c < bip->cols; c++)
            bip->ac[c][row_cnt] = bip->ar[r][c];

         row_cnt++;
         break;
      case GE :
         bip->rhs_ord[row_cnt] = -bip->rhs[r];
         
         for(int c = 0; c < bip->cols; c++)
            bip->ac[c][row_cnt] = -bip->ar[r][c];

         row_cnt++;
         break;
      case EQ :
         break;
      default :
         assert(0); //lint !e506 Constant value Boolean
      }
   }
   assert(bip->rows == row_cnt);
     
   for(int r = 0; r < bip->rows; r++)
   {
      double scale = 0.0;

      /* Compute scaling factor to remove fractions
       */
      for(int c = 0; c < bip->cols; c++)
      {
         double x = fabs(bip->ac[c][r]) - floor(fabs(bip->ac[c][r]));

         if (x > 0.0)
         {
            if (VERB_DEBUG <= bip->verb_level )
               printf("[%d,%d] %g\n", r, c, x);
         
            x = ceil(-log10(x));
            
            if (x > scale)
               scale = x;
         }
      }
      /* If necessary, scale row
       */
      if (scale > 0)
      {
         scale = pow(10.0, scale);
         
         for(int c = 0; c < bip->cols; c++)
            bip->ac[c][r] *= scale;
            
         bip->rhs_ord[r] *= scale;

         if (VERB_NORMAL <= bip->verb_level)
            printf("Reordered row %d has been scaled with factor %g\n", r, scale);
      }
   }
}
   
static bool bip_can_overflow(const BIP* bip)
{
   for(int r = 0; r < bip->rows; r++)
   {
      double row_max = 0;
      double row_min = 0;
      
      for(int c = 0; c < bip->cols; c++)
      {
         double val = bip->ar[r][c];
         
         if (val > 0)
         {
            if (row_max >= bip->max_coef_val - val)
               return fprintf(stderr, "Error: row %d numerical overflow\n", r), true;
               
            row_max += val;
         }
         else if (val < 0)
         {
            if (row_min <= bip->min_coef_val - val)
               return fprintf(stderr, "Error: row %d numerical negative overflow\n", r), true;

            row_min += val;
         }
         else
         {
            assert(val == 0);
         }
      }
   }
   return false;
}


/** Process one input line.
 *  Depending on the current LINE_MODE parse the line and check for all detactable errors.
 * 
 *  @return The LINE_MODE for the next line from the input file
 */  
static LINE_MODE process_line(
   LINE_MODE  mode,               ///< [in] current LINE_MODE
   const LFS* lfs,                ///< [in] speparated fields of the input line
   const int  line_no,            ///< [in] current line number
   BIP*       bip)                ///< [in,out] BIP to store 
{
   /* If line is not empty
    */
   if (lfs_fields_used(lfs) > 0)
   {
      double val;
      char*  t;
      
      /* do processing here:
       * mode tells which kind of line is next.
       */
      switch(mode)
      {
      case READ_COLS :
         if (lfs_fields_used(lfs) != 1)
            return fprintf(stderr, "Error line %d: Got %d fields, expected 1\n",
               line_no, lfs_fields_used(lfs)),
               READ_ERROR;

         val = strtod(lfs_field(lfs, 0), &t);

         if (errno == ERANGE || isnan(val) || isinf(val) || rint(val) != val 
            || val < 1.0 || (int)val > BIP_MAX_COLS || *t != '\0') //lint !e777 Testing floats for equality
            return fprintf(stderr, "Error line=%d Number of cols %s=%f out of range or not integer %s\n",
               line_no, lfs_field(lfs, 0), val, errno ? strerror(errno) : ""),
               READ_ERROR;

         bip->cols = (int)val;
         mode      = READ_ROWS;
         break;

      case READ_ROWS :
         if (lfs_fields_used(lfs) != 1)
            return fprintf(stderr, "Error line %d: Got %d fields, expected 1\n",
               line_no, lfs_fields_used(lfs)),
               READ_ERROR;

         val = strtod(lfs_field(lfs, 0), &t);

         if (errno == ERANGE || isnan(val) || isinf(val) || rint(val) != val 
            || val < 1.0 || (int)val > BIP_MAX_ROWS || *t != '\0') //lint !e777 Testing floats for equality
            return fprintf(stderr, "Error line=%d Number of rows %s=%f out of range or not integer %s\n",
               line_no, lfs_field(lfs, 0), val, errno ? strerror(errno) : ""),
               READ_ERROR;

         bip->rows = (int)val;
         mode          = READ_COEF;
         break;

      case READ_COEF :
         if (bip->read_rows >= bip->rows)
            return fprintf(stderr, "Error line=%d Expetced %d rows, got more\n",
               line_no, bip->rows),
               READ_ERROR;

         if (lfs_fields_used(lfs) != bip->cols + 2)
            return fprintf(stderr, "Error line %d: Got %d fields, expected %d\n",
               line_no, lfs_fields_used(lfs), bip->cols + 2),
               READ_ERROR;

         for(int col = 0; col < bip->cols + 2; col++)
         {
            if (col != bip->cols)
            {
               val = strtod(lfs_field(lfs, col), &t);

               if (errno == ERANGE || isnan(val) || isinf(val)
                  || val > bip->max_coef_val
                  || val < bip->min_coef_val
                  || *t != '\0')
                  return fprintf(stderr, "Error line=%d Number %s=%f out of range: %s\n",
                     line_no, lfs_field(lfs, col), val, errno ? strerror(errno) : ""),
                     READ_ERROR;

               if (col < bip->cols)
                  bip->ar[bip->read_rows][col] = val;
               else
                  bip->rhs[bip->read_rows] = val;
            }
            else
            {
               const char* sense = lfs_field(lfs, bip->cols);
         
               if (!strcmp(sense, "<="))
                  bip->sense[bip->read_rows] = LE;
               else if (!strcmp(sense, ">="))
                  bip->sense[bip->read_rows] = GE;
               else if (!strcmp(sense, "=") || !strcmp(sense, "=="))
                  bip->sense[bip->read_rows] = EQ;
               else
                  return fprintf(stderr, "Error line=%d Expected <=, >=, ==, got \"%s\"\n",
                     line_no, sense),
                     READ_ERROR;
            }
         }
         bip->read_rows++;
         break;

      default :
         abort();
      }
   }
   return mode;
}


/** Allocate and initialize a new BIP data structure.
 */
BIP* bip_init(
   int verb_level)   ///< verbosity level 
{
   BIP* bip          = calloc(1, sizeof(*bip));
   bip->max_coef_val =  pow(10.0, (double)DBL_DIG);
   bip->min_coef_val = -pow(10.0, (double)DBL_DIG);
   bip->verb_level   = verb_level;

   assert(bip_is_valid(bip));
   
   return bip;
}

/** Deallocate BIP data structure.
 */
void bip_exit(BIP* bip)
{
   assert(bip_is_valid(bip));
   
   mem_check(bip);
   
   free(bip);
}


/** Read a bip file.
 * Format example:
 *
 * 4 # cols (variables)
 * 3 # rows (constraints)
 * 2 3 5 4 <= 8
 * 3 6 0 8 <= 10
 * 0 0 1 1 <= 1
 *
 * comments (\#) and empty lines are ignored.
 *
 * @param filename name of file to read
 * @return ptr to BIP data structure
 */
int bip_read(BIP* bip, const char* filename)
{
   assert(bip_is_valid(bip));
   assert(NULL != filename);
   assert(0    <  strlen(filename));
   
   char      buf[MAX_LINE_LEN];
   char*     s;
   FILE*     fp;
   LFS*      lfs   = NULL;
   int       lines = 0;
   LINE_MODE mode  = READ_COLS;
   
   if (NULL == (fp = fopen(filename, "r")))
      return fprintf(stderr, "Can't open file %s\n", filename), -1;

   printf("Reading %s\n", filename);

   while(mode != READ_ERROR && NULL != (s = fgets(buf, sizeof(buf), fp)))
   {
      lines++;
      
      lfs  = lfs_split_line(lfs, s, "#");

      if (VERB_DEBUG <= bip->verb_level)
         lfs_print(lfs, stderr);
      
      mode = process_line(mode, lfs, lines, bip);
   }
   fclose(fp);

   if (NULL != lfs)
      lfs_free(lfs);
   
   if (READ_ERROR == mode)
      return -1;

   if (bip->cols == 0 || bip->rows == 0 || bip->read_rows < bip->rows)
      return fprintf(stderr, "Error: unexpected EOF\n"), -1;
   
   assert(bip->read_rows == bip->rows);

   printf("Read %d rows, %d cols\n", bip->read_rows, bip->cols);

   if (bip_can_overflow(bip))
      return -1;

   bip_preprocess(bip);

   assert(bip_is_valid(bip));
   
   return 0;
}

/** Print Binary Program from BIP.
 */
void bip_print(const BIP* bip, FILE* fp)
{
   const char* sense[3] = { "<=", ">=", "==" };
      
   assert(bip_is_valid(bip));
   assert(NULL != fp);

   for(int r = 0; r < bip->rows; r++)
   {
      for(int c = 0; c < bip->cols; c++)
         fprintf(fp, "%g ", bip->ar[r][c]);
      fprintf(fp, "%s %g\n", sense[bip->sense[r]], bip->rhs[r]);
   }
}

/** Get number of columns in BIP.
 */
int bip_cols(const BIP* bip)
{
   assert(bip_is_valid(bip));

   return bip->cols;
}

/* Get verbosity level.
 */
int bip_verbosity_level(const BIP* bip)
{
   assert(bip_is_valid(bip));

   return bip->verb_level;
}

/** Check feasibility of a solution.
 */
static inline int check_feasibility(
   const BIP*    bip,
   unsigned int  x,        ///< solution bit vector
   const double* r,       ///< row residuals
   Report_fun    report_sol) ///< function to report solution
{
   assert(bip_is_valid(bip));
   assert(NULL != r);
   assert(NULL != report_sol);
      
   int k;
   
   for(k = 0; (k < bip->equs) && (r[k] == 0.0); k++)
      ;

   if (k == bip->equs)
      for(; (k < bip->rows) && (r[k] <= 0.0); k++)
         ;
   
   if (k < bip->rows)
      return 0;
   
   assert(bip_is_feasible(bip, x));

   (*report_sol)(bip, x);

   return 1;
}

/** Enumerate all feasible solution to the given BIP and report the solutions.
 *
 * @return Number of feasible solutions.
 */
long bip_enumerate(
   const BIP* bip,        ///< BIP to enumerate
   Report_fun report_sol) ///< callback function to report the solutions
{
   unsigned int index32[32] =
   { 
       0,  1, 28,  2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17,  4,  8,
      31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18,  6, 11,  5, 10,  9
   }; 

   const int     cols = bip->cols;
   const int     rows = bip->rows;

   assert(bip_is_valid(bip));
   assert(sizeof(int) == 4); //lint !e506 constant value Boolean
   
   /* Interesting question whether there is no or one solution.
    */
   if (cols == 0)
      return 0;
   
   long          sol_count = 0;
   unsigned int  x         = 0;  /* current bitvector */
   unsigned int  n         = 1;  /* node number  */
   unsigned int  negn;    /* negn = -n */
   const double* modcol;
   int           k;
   double        r[rows];

   for(k = 0; k < rows; k++)
      r[k] = -bip->rhs_ord[k];

   /* Check whether 0 is a feasible solution
    */
   sol_count += check_feasibility(bip, x, r, report_sol);

   /* Starting with x = 0000, n = 0001, negn = 1111, the algorithm
    * enumerates all x vectors by always doing only one flip, which means
    * we only have to update the residium for a single column.
    * This is done as follows:
    *   1. updatemask = n & negn (always only one bit set!)
    *   2. modcol = bit number which is set in updatemask
    *   2. n++, negn--
    *   2. xor-bitflip: x ^= updatemask
    *   3. add or substract col[modcol] from activities and check feasibility
    *   4. as long as negn != 0, goto 1.
    * Thereby, the x vectors are scanned in the following order:
    *   0000, 0001, 0011, 0010, 0110, 0111, 0101, 0100, 
    *   1100, 1101, 1111, 1110, 1010, 1011, 1001, 1000
    */
   negn  = 1u << (cols - 1);
   negn += (negn - 1);
   
   while(negn != 0)
   {
      unsigned int updatemask = n & negn;

      /* http://en.wikipedia.org/wiki/De_Bruijn_sequence
       */
      unsigned int colidx = index32[(uint32_t)(updatemask * DEBRUIJN32) >> 27];

      /* ((n & negn) == (n & -n)) */
      assert(((n + negn) & ((1u << (cols - 1)) + ((1u << (cols - 1)) - 1))) == 0);

      ++n;
      --negn;
      x ^= updatemask;

      modcol = bip->ac[colidx];
      
      if  (x & updatemask)
      {
         for(k = 0; k < rows; k++)
            r[k] += modcol[k];
      }
      else /* bit changed from 1 to 0 */
      {
         for(k = 0; k < rows; k++)
            r[k] -= modcol[k];
      }
      sol_count += check_feasibility(bip, x, r, report_sol);
   }
   return sol_count;
}


