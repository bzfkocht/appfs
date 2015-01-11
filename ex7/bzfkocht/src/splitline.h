/**@file      
 * @brief     Split line into fields Header
 * @author    Thorsten Koch
 * @date      04 Jan 2014
 * @copyright (c) 2015 by Thorsten Koch <koch@zib.de>
 *
 * Takes a string, copies it, from the copy removes any non-printabel characters,
 * cuts it off when certain comment characters are found. The remainin string is split
 * into fields separated by whitespace. The an array of string pointers pointing to the
 * beginning of the fields is constructed. Each field is null terminated.
 */  
#ifndef _SPLITLINE_H_
#define _SPLITLINE_H_

typedef struct line_fields LFS; ///< line filed structure

/*lint -sem(lfs_free, custodial(1), 1p == 1) */
extern void lfs_free(LFS* lfs);

/*lint -sem(lfs_split_line, inout(1), nulterm(2), nulterm(3), @p) */
extern LFS* lfs_split_line(LFS* lfs, const char* line, const char* comment);

/*lint -sem(lfs_fields_used, 1p == 1, @n >= 0) */
extern int  lfs_fields_used(const LFS* lfs);

/*lint -sem(lfs_field, 1p == 1 && 2n >= 0, nulterm(@p)) */
extern const char* lfs_field(const LFS* lfs, int fno);

/*lint -sem(lfs_print, 1p == 1 && 2p == 1) */
extern void lfs_print(const LFS* lfs, FILE* fp);

#endif /* _SPLITLINE_H_ */
