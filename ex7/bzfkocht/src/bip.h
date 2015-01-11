/**@file      
 * @brief     BIP management header
 * @author    Thorsten Koch
 * @date      04 Jan 2015
 * @copyright (c) 2015 by Thorsten Koch <koch@zib.de>
 *
 * Naming: If the function does something there is a verb.
 * If it just reports a property of the data structure the implicit 'get' is omitted.
 */  
#ifndef _BIP_H_
#define _BIP_H_

#define VERB_QUIET   0   ///< no output
#define VERB_NORMAL  1   ///< normal output
#define VERB_VERBOSE 2   ///< all useful information 
#define VERB_CHATTER 3   ///< what ever is going on
#define VERB_DEBUG   4   ///< including information only useful for debugging

typedef struct binary_program BIP;

typedef enum { LE = 0, GE = 1, EQ = 2 } Sense;

typedef void (*Report_fun)(const BIP* bip, unsigned int x);

//lint -sem(bip_init, @p == 1)
extern BIP* bip_init(int verb_level);
//lint -sem(bip_exit, custodial(1), 1p == 1) 
extern void bip_exit(BIP* bip);
//lint -sem(bip_read, inout(1), 1p == 1, nulterm(2)) 
extern int  bip_read(BIP* bip, const char* filename);
//lint -sem(bip_print, 1p == 1 && 2p == 1) 
extern void bip_print(const BIP* bip, FILE* fp);
//lint -sem(bip_cols, 1p == 1, @n > 0) 
extern int  bip_cols(const BIP* bip);
//lint -sem(bip_verbosity, 1p == 1) 
extern int  bip_verbosity_level(const BIP* bip);
//lint -sem(bip_enumerate, 1p == 1 && 3p == 1) 
extern long bip_enumerate(const BIP* bip, Report_fun report_sol);

#endif // _BIP_H_
