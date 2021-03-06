/* $Id: lint.h,v 1.19 2014/03/03 16:44:13 bzfkocht Exp $ */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*   File....: lint.h                                                        */
/*   Name....: Lint defines                                                  */
/*   Author..: Thorsten Koch                                                 */
/*   Copyright by Author, All rights reserved                                */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*
 * Copyright (C) 2001-2014 by Thorsten Koch <koch@zib.de>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _LINT_H_
#define _LINT_H_

/* Use this file only if we are linting
 */
#ifdef _lint

#ifdef __cplusplus
extern "C" {
#endif

/* Unfortunately strdup() is not a POSIX function.
 */
/*lint -sem(strdup, 1p && nulterm(1), @P == malloc(1P) && nulterm(@p)) */ 
extern char* strdup(const char* s);

/* It is not clear if isinf() and isnan() are already POSIX
 * or only in the next Draft.
 */
extern int isinf(double);
extern int isnan(double);
extern int isfinite(double);
extern int finite(double); /* This is probably not POSIX */

/*lint -esym(757, optarg, optind, opterr, optopt) */
/*lint -sem(getopt, 1n > 0 && 2p && 3p) */
extern int getopt(int argc, char* const argv[], const char* optstring);
extern char* optarg;
extern int optind;
extern int opterr;
extern int optopt;

/*lint -function(fopen, popen) */
extern FILE* popen(const char *command, const char *type);
/*lint -function(fclose, pclose) */
/*lint -esym(534, pclose) */
extern int   pclose(FILE *stream);
/*lint -sem(fsync, 1n >= 0, @n <= 0) */
extern int   fsync(int fd);

#ifdef __cplusplus
}
#endif
#endif /* _lint */

#if defined(__GNUC__) || defined(__CLANG__)
#define UNUSED __attribute__ ((unused))
#define NORETURN __attribute__ ((noreturn))
#else
#define UNUSED
#define NORETURN
#endif /* __GNUC__ || __CLANG__ */

#endif /* _LINT_H_ */



