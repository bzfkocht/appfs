#include <assert.h>
#include <ctype.h>   // isspace
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "allocate.h"
#include "linear_program.h"
#include "num_type.h"

#define MAX_LINE_LEN   512  // Maximum input line length

#define GET_SEC(a, b) ((double) (b - a) / (double)CLOCKS_PER_SEC)

/* the file parser can have 3 different states, reading #rows, #cols, or
 * parsing a constraint
 */
enum parser_state {READ_ROWS, READ_COLS, READ_CONSTRAINTS};

/* describes the "format" of a constraint:
 * <= -> LEQ
 * = -> EQ
 * >= -> GEQ
 */
enum constraint_type {LEQ, EQ, GEQ};

struct linear_program {
    int rows;
    int cols;
    num_t** matrix;
    num_t* vector;
    int* constraint_types;
};

/* FIXME too simple, are there any other properties to check? */
bool lp_is_valid(LinearProgram* lp) {
    if (NULL == lp) {
        return false;
    }

    if (lp->rows <= 0 || lp->cols <= 0) {
        return false;
    }

    return lp &&
        lp->cols &&
        lp->rows &&
        lp->matrix &&
        lp->vector &&
        lp->constraint_types;
}

LinearProgram* lp_new(int rows, int cols) {
    assert(rows > 0);
    assert(cols > 0);

    LinearProgram* lp = allocate(1, sizeof(*lp));
    lp->rows = rows;
    lp->cols = cols;

    num_t** matrix = allocate(rows, sizeof(*matrix));

    int i;
    // initialize every row with 0s
    for (i = 0; i < rows; i++) {
        matrix[i] = allocate(cols, sizeof(*matrix[i]));
    }

    num_t* vector = allocate(rows, sizeof(*vector));
    int* constraint_types = allocate(rows, sizeof(*constraint_types));

    lp->matrix = matrix;
    lp->vector = vector;
    lp->constraint_types = constraint_types;

    assert(lp_is_valid(lp));
    return lp;
}

void lp_free(LinearProgram* lp) {
    assert(lp_is_valid(lp));
    int i;
    for (i = 0; i < lp->rows; i++) {
        deallocate(lp->matrix[i]);
    }

    deallocate(lp->matrix);
    deallocate(lp->vector);
    deallocate(lp->constraint_types);
    deallocate(lp);
}

char* skip_spaces(char* s) {
    while(isspace(*s)) {
        s++;
    }

    return s;
}

char* parse_type(char* s, int row, LinearProgram* lp) {
    s = skip_spaces(s);
    if (!*s) {
        return NULL;
    }
    if (*s == '<') {
        if (*(s+1) != '=') {
            return NULL;
        }
        lp->constraint_types[row] = LEQ;
        s+=2;
        return s;
    }

    if (*s == '>') {
        if (*(s+1) != '=') {
            return NULL;
        }
        lp->constraint_types[row] = GEQ;
        s+=2;
        return s;
    }

    if (*s == '=') {
        lp->constraint_types[row] = EQ;
        s++;
        return s;
    }

    fprintf(stderr, "invalid constraint\n");
    return NULL;
}

bool can_overflow(LinearProgram* lp) {
    assert(lp_is_valid(lp));

    for(int r = 0; r < lp->rows; r++)
    {
        num_t row_max = 0;
        num_t row_min = 0;

        for(int c = 0; c < lp->cols; c++)
        {
            num_t val = lp->matrix[r][c];

            if (val > 0)
            {
                if (row_max < MAX_COEF_VAL - val) {
                    row_max += val;
                } else {
                    fprintf(stderr, "Error: row %d numerical overflow\n", r);
                    return true;
                }
            } else if (val < 0) {
                if (row_min > MIN_COEF_VAL - val) {
                    row_min += val;
                } else {
                    fprintf(stderr, "Error: row %d numerical negative overflow\n", r);
                    return true;
                }
            } else {
                assert(val == 0);
            }
        }
    }

    return false;
}

/* parses a line of the file
 * tries to set the corresponding row in the matrix
 * returns false on error
 */
bool parse_row(char* s, int row, LinearProgram* lp) {
    assert(lp_is_valid(lp));
    assert(row >= 0);
    assert(row < lp->rows);

    int i;
    char* end_ptr;
    for (i = 0; i < lp->cols; i++) {
        num_t num = parse_num(s, &end_ptr);

        if (!is_num_valid(num, s, end_ptr)) {
            return false;
        }

        lp->matrix[row][i] = num;
        s = end_ptr;
    }


    s = parse_type(s, row, lp);

    if (NULL == s) {
        return false;
    }

    num_t num = parse_num(s, &end_ptr);
    if (!is_num_valid(num, s, end_ptr)) {
        return false;
    }
    s = end_ptr;

    s = skip_spaces(s);

    if ('\0' != *s) {
        return false;
    }

    lp->vector[row] = num;

    assert(lp_is_valid(lp));
    return true;
}

// taken from ex4_readline.c
// adapted to special needs
LinearProgram *new_lp_from_file(const char* filename) {
    assert(NULL != filename);
    assert(0 < strlen(filename));

    int rows = 0;
    int cols;
    char* end_ptr = NULL;
    LinearProgram* lp = NULL;

    /* counts the constraint that were read from file
     * if constraints < rows -> error
     * */
    int constraints = 0;

    /* used to mark distinguish the current state of the parser */
    int parser_state = READ_COLS;

    FILE* fp;
    char  buf[MAX_LINE_LEN];
    char* s;
    int   lines = 0;

    if (NULL == (fp = fopen(filename, "r")))
    {
        fprintf(stderr, "Can't open file %s\n", filename);
        return NULL;
    }

    while(NULL != (s = fgets(buf, sizeof(buf), fp)))
    {
        char* t = strpbrk(s, "#\n\r");

        lines++;

        if (NULL != t) /* else line is not terminated or too long */
            *t = '\0';  /* clip comment or newline */

        /* Skip over leading space
        */
        s = skip_spaces(s);

        /* Skip over empty lines
         */
        if (!*s) { /* <=> (*s == '\0') */
            continue;
        }

        /* line is nonempty, so try to parse data
         */
        if (parser_state == READ_COLS) {
            cols = (int) strtol(s, &end_ptr, 10);

            if (cols <= 0) {
                fprintf(stderr, "please specify a positive number of cols.\n");
                goto read_error;
            }

            parser_state = READ_ROWS;
        } else if (parser_state == READ_ROWS) {
            rows = (int) strtol(s, &end_ptr, 10);

            if (rows <= 0) {
                fprintf(stderr, "please specify a positive number of rows.\n");
                goto read_error;
            }

            lp = lp_new(rows, cols);
            parser_state = READ_CONSTRAINTS;
        } else {
            /* stop if a row does not match the format */
            if (constraints >= rows) {
                lp_free(lp);
                fprintf(stderr, "too many constraints");
                goto read_error;
            }

            bool valid_format = parse_row(s, constraints, lp);

            if (!valid_format) {
                lp_free(lp);
                goto read_error;
            }
            constraints++;
        }

    }
    fclose(fp);

    if (constraints != rows) {
        fprintf(stderr, "speciefied #(rows) does not match: %d expected, %d found\n", rows, constraints);
        return NULL;
    }

    if (can_overflow(lp)) {
        return NULL;
    }

    printf("%d lines\n", lines);
    return lp;

read_error:
    printf("error in line %d\n", lines);
    fclose(fp);
    return NULL;
}

/* print a solution vector */
void __print_config(num_t* configuration, int len) {
    assert(0 < len);

    int j;
    for (j = 0; j < len; j++) {
        print_num(configuration[j]);
    }

    printf("\n");
}

/* return the lexicographically next 0-1 vector */
void next_configuration(num_t* configuration, int len) {
    assert(0 < len);

    int i;
    for (i = 0; i < len; i++) {
        if (configuration[i]) {
            configuration[i] = 0;
        } else {
            configuration[i] = 1;
            break;
        }
    }
}

bool __is_feasible_sum(num_t sum, int row, LinearProgram* lp) {
    assert(lp_is_valid(lp));
    assert(row >= 0);
    assert(row < lp->rows);

    if (lp->constraint_types[row] == LEQ && lp->vector[row] < sum) {
        return false;
    } else if (lp->constraint_types[row] == GEQ && lp->vector[row] > sum) {
        return false;
    } else if (lp->constraint_types[row] == EQ && lp->vector[row] != sum) {
        return false;
    }
    return true;
}

/* check if a vector is a feasible solution to the lp */
bool is_feasible(num_t* configuration, LinearProgram* lp) {
    assert(lp_is_valid(lp));

    int i, j;
    for (i = 0; i < lp->rows; i++) {
        num_t sum = 0;
        for (j = 0; j < lp->cols; j++) {
            sum += configuration[j] * lp->matrix[i][j];
        }

        if (!__is_feasible_sum(sum, i, lp)) {
            return false;
        }
    }
    return true;
}

void __print_constraint_type(int row, LinearProgram* lp) {
    assert(lp_is_valid(lp));
    assert(row >= 0);
    assert(row < lp->rows);

    if (lp->constraint_types[row] == LEQ) {
        printf("<= ");
    } else if (lp->constraint_types[row] == GEQ) {
        printf(">= ");
    } else if (lp->constraint_types[row] == EQ) {
        printf("= ");
    } else {
        /* should never happen */
        printf("\nassigned unknown constraint type, fatal error\n");
        abort();
    }
}

void print_matrix(LinearProgram* lp) {
    assert(lp_is_valid(lp));

    printf("nvars: %d\n", lp->cols);
    printf("nconss: %d\n", lp->rows);

    int i, j;
    for (i = 0; i < lp->rows; i++) {
        for (j = 0; j < lp->cols; j++) {
            print_num(lp->matrix[i][j]);
        }

        __print_constraint_type(i, lp);

        print_num(lp->vector[i]);
        printf("\n");
    }
}

/* print all 0-1 solutions to the lp into the outstream */
void print_bin_solutions_lp(LinearProgram* lp) {
    num_t* configuration = allocate(lp->cols, sizeof(*configuration));
    unsigned long count = 1UL << lp->cols;
    unsigned int feasible_solutions = 0;

    print_matrix(lp);
    printf("\n");

    clock_t start = clock();

    unsigned int i;
    for (i = 0; i < count; i++) {
        if (is_feasible(configuration, lp)) {
            __print_config(configuration, lp->cols);
            feasible_solutions++;
        }
        next_configuration(configuration, lp->cols);
    }

    double elapsed = GET_SEC(start, clock());
    printf("Checked %lu vectors in %.3f s = %.3f kvecs/s\n",
            count, elapsed, (double) count / elapsed / 1000.0);

    deallocate(configuration);
    printf("found %u feasible solutions\n", feasible_solutions);
}
