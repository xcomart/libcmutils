
/******************************************************************************
* fpattern.c
*	Functions for matching filename patterns to filenames.
*
* Usage
*	(See "fpattern.h".)
*
* Notes
*	These pattern matching capabilities are modeled after those found in
*	the UNIX command shells.
*
*	`DELIM' must be defined to 1 if pathname separators are to be handled
*	explicitly.
*
* History
*	1.00 1997-01-03 David Tribble.
*		First cut.
*	1.01 1997-01-03 David Tribble.
*		Added SUB pattern character.
*		Added fpattern_matchn().
*	1.02 1997-01-12 David Tribble.
*		Fixed missing lowercase matching cases.
*	1.03 1997-01-13 David Tribble.
*		Pathname separator code is now controlled by DELIM macro.
*	1.04 1997-01-14 David Tribble.
*		Added QUOTE macro.
*	1.05 1997-01-15 David Tribble.
*		Handles special case of empty pattern and empty filename.
*	1.06 1997-01-26 David Tribble.
*		Changed range negation character from '^' to '!', ala Unix.
*	1.07 1997-08-02 David Tribble.
*		Uses the 'FPAT_XXX' constants.
*	1.08 1998-06-28 David Tribble.
*		Minor fixed for MS-VC++ (5.0).
*
* Limitations
*	This code is copyrighted by the author, but permission is hereby
*	granted for its unlimited use provided that the original copyright
*	and authorship notices are retained intact.
*
*	Other queries can be sent to:
*	    dtribble@technologist.com
*	    david.tribble@beasys.com
*	    dtribble@flash.net
*
* Copyright 1997-1998 by David R. Tribble, all rights reserved.
*/


/* Identification */

//static const char	id[] = "@(#)lib/fpattern.c 1.08";

//static const char	copyright[] = "Copyright 1997-1998 David R. Tribble\n";


/* System includes */

#include <ctype.h>
#include <stddef.h>
#include "functions.h"

#if TEST
#include <locale.h>
#include <stdio.h>
#include <string.h>
#endif

#if defined(MSWIN)
#define UNIX	0
#define DOS	1
#else
#define UNIX	1
#define DOS	0
#endif


/* Manifest constants */

#define FPAT_QUOTE      '\\'    /* Quotes a special char        */
#define FPAT_QUOTE2     '`'     /* Quotes a special char        */
#define FPAT_DEL        '/'     /* Path delimiter               */
#define FPAT_DEL2       '\\'    /* Path delimiter               */
#define FPAT_DOT        '.'     /* Dot char                     */
#define FPAT_NOT        '!'     /* Exclusion                    */
#define FPAT_ANY        '?'     /* Any one char                 */
#define FPAT_CLOS       '*'     /* Zero or more chars           */
#define FPAT_CLOSP      '\x1A'  /* Zero or more nondelimiters	*/
#define FPAT_SET_L      '['     /* Set/range open bracket       */
#define FPAT_SET_R      ']'     /* Set/range close bracket      */
#define FPAT_SET_NOT    '!'     /* Set exclusion                */
#define FPAT_SET_THRU   '-'     /* Set range of chars           */


/* Local constants */

#ifndef NULL
#define NULL		((void *) 0)
#endif

#define SUB		FPAT_CLOSP

#ifndef DELIM
#define DELIM		0
#endif

#define DEL		FPAT_DEL

#if UNIX
#define DEL2		FPAT_DEL
#else /*DOS*/
#define DEL2		FPAT_DEL2
#endif

#if UNIX
#define QUOTE		FPAT_QUOTE
#else /*DOS*/
#define QUOTE		FPAT_QUOTE2
#endif


/* Local function macros */

#if UNIX
#define lowercase(c)	(c)
#else /*DOS*/
#define lowercase(c)	tolower(c)
#endif


/*-----------------------------------------------------------------------------
* fpattern_isvalid()
*	Checks that filename pattern 'pat' is a well-formed pattern.
*
* Returns
*	1 CMUTIL_True if 'pat' is a valid filename pattern, otherwise 0 CMUTIL_False.
*
* Caveats
*	If 'pat' is null, 0 (false) is returned.
*
*	If 'pat' is empty (""), 1 (true) is returned, and it is considered a
*	valid (but degenerate) pattern (the only filename it matches is the
*	empty ("") string).
*/

CMUTIL_Bool CMUTIL_PatternIsValid(const char *pat)
{
    int		len;

    /* Check args */
    if (pat == NULL)
        return CMUTIL_False;

    /* Verify that the pattern is valid */
    for (len = 0;  pat[len] != '\0';  len++)
    {
        switch (pat[len])
        {
        case FPAT_SET_L:
            /* Char set */
            len++;
            if (pat[len] == FPAT_SET_NOT)
                len++;			/* Set negation */

            while (pat[len] != FPAT_SET_R)
            {
                if (pat[len] == QUOTE)
                    len++;		/* Quoted char */
                if (pat[len] == '\0')
                    return CMUTIL_False;	/* Missing closing bracket */
                len++;

                if (pat[len] == FPAT_SET_THRU)
                {
                    /* Char range */
                    len++;
                    if (pat[len] == QUOTE)
                        len++;		/* Quoted char */
                    if (pat[len] == '\0')
                        return CMUTIL_False;	/* Missing closing bracket */
                    len++;
                }

                if (pat[len] == '\0')
                    return CMUTIL_False;	/* Missing closing bracket */
            }
            break;

        case QUOTE:
            /* Quoted char */
            len++;
            if (pat[len] == '\0')
                return CMUTIL_False;		/* Missing quoted char */
            break;

        case FPAT_NOT:
            /* Negated pattern */
            len++;
            if (pat[len] == '\0')
                return CMUTIL_False;		/* Missing subpattern */
            break;

        default:
            /* Valid character */
            break;
        }
    }

    return CMUTIL_True;
}


/*-----------------------------------------------------------------------------
* fpattern_submatch()
*	Attempts to match subpattern 'pat' to subfilename 'fname'.
*
* Returns
*	1 (true) if the subfilename matches, otherwise 0 CMUTIL_False.
*
* Caveats
*	This does not assume that 'pat' is well-formed.
*
*	If 'pat' is empty (""), the only filename it matches is the empty ("")
*	string.
*
*	Some non-empty patterns (e.g., "") will match an empty filename ("").
*/

CMUTIL_STATIC CMUTIL_Bool CMUTIL_PatternSubmatch(
        const char *pat, const char *fname)
{
    int		fch;
    int		pch;
    int		i;
    int		yes, match;
    int		lo, hi;

    /* Attempt to match subpattern against subfilename */
    while (*pat != '\0')
    {
        fch = *fname;
        pch = *pat;
        pat++;

        switch (pch)
        {
        case FPAT_ANY:
            /* Match a single char */
        #if DELIM
            if (fch == DEL  ||  fch == DEL2  ||  fch == '\0')
                return CMUTIL_False;
        #else
            if (fch == '\0')
                return CMUTIL_False;
        #endif
            fname++;
            break;

        case FPAT_CLOS:
            /* Match zero or more chars */
            i = 0;
        #if DELIM
            while (fname[i] != '\0'  &&
                    fname[i] != DEL  &&  fname[i] != DEL2)
                i++;
        #else
            while (fname[i] != '\0')
                i++;
        #endif
            while (i >= 0)
            {
                if (CMUTIL_PatternSubmatch(pat, fname+i))
                    return CMUTIL_True;
                i--;
            }
            return CMUTIL_False;

        case SUB:
            /* Match zero or more chars */
            i = 0;
            while (fname[i] != '\0'  &&
        #if DELIM
                    fname[i] != DEL  &&  fname[i] != DEL2  &&
        #endif
                    fname[i] != '.')
                i++;
            while (i >= 0)
            {
                if (CMUTIL_PatternSubmatch(pat, fname+i))
                    return CMUTIL_True;
                i--;
            }
            return CMUTIL_False;

        case QUOTE:
            /* Match a quoted char */
            pch = *pat;
            if (lowercase(fch) != lowercase(pch)  ||  pch == '\0')
                return CMUTIL_False;
            fname++;
            pat++;
            break;

        case FPAT_SET_L:
            /* Match char set/range */
            yes = CMUTIL_True;
            if (*pat == FPAT_SET_NOT)
            {
               pat++;
               yes = CMUTIL_False;	/* Set negation */
            }

            /* Look for [s], [-], [abc], [a-c] */
            match = yes? CMUTIL_False:CMUTIL_True;
            while (*pat != FPAT_SET_R  &&  *pat != '\0')
            {
                if (*pat == QUOTE)
                    pat++;	/* Quoted char */

                if (*pat == '\0')
                    break;
                lo = *pat++;
                hi = lo;

                if (*pat == FPAT_SET_THRU)
                {
                    /* Range */
                    pat++;

                    if (*pat == QUOTE)
                        pat++;	/* Quoted char */

                    if (*pat == '\0')
                        break;
                    hi = *pat++;
                }

                if (*pat == '\0')
                    break;

                /* Compare character to set range */
                if (lowercase(fch) >= lowercase(lo)  &&
                    lowercase(fch) <= lowercase(hi))
                    match = yes;
            }

            if (!match)
                return CMUTIL_False;

            if (*pat == '\0')
                return CMUTIL_False;		/* Missing closing bracket */

            fname++;
            pat++;
            break;

        case FPAT_NOT:
            /* Match only if rest of pattern does not match */
            if (*pat == '\0')
                return CMUTIL_False;		/* Missing subpattern */
            return CMUTIL_PatternSubmatch(pat, fname)? CMUTIL_False:CMUTIL_True;

#if DELIM
        case DEL:
    #if DEL2 != DEL
        case DEL2:
    #endif
            /* Match path delimiter char */
            if (fch != DEL  &&  fch != DEL2)
                return CMUTIL_False;
            fname++;
            break;
#endif

        default:
            /* Match a (non-null) char exactly */
            if (lowercase(fch) != lowercase(pch))
                return CMUTIL_False;
            fname++;
            break;
        }
    }

    /* Check for complete match */
    if (*fname != '\0')
        return CMUTIL_False;

    /* Successful match */
    return CMUTIL_True;
}


/*-----------------------------------------------------------------------------
* fpattern_match()
*	Attempts to match pattern 'pat' to filename 'fname'.
*
* Returns
*	1 CMUTIL_True if the filename matches, otherwise 0 CMUTIL_False.
*
* Caveats
*	If 'fname' is null, zero CMUTIL_False is returned.
*
*	If 'pat' is null, zero CMUTIL_False is returned.
*
*	If 'pat' is empty (""), the only filename it matches is the empty
*	string ("").
*
*	If 'fname' is empty, the only pattern that will match it is the empty
*	string ("").
*
*	If 'pat' is not a well-formed pattern, zero CMUTIL_False is returned.
*
*	Upper and lower case letters are treated the same; alphabetic
*	characters are converted to lower case before matching occurs.
*	Conversion to lower case is dependent upon the current locale setting.
*/

CMUTIL_Bool CMUTIL_PatternMatch(const char *pat, const char *fname)
{
    /* Check args */
    if (fname == NULL)
        return CMUTIL_False;

    if (pat == NULL)
        return CMUTIL_False;

    /* Verify that the pattern is valid, and get its length */
    if (!CMUTIL_PatternIsValid(pat))
        return CMUTIL_False;

    /* Attempt to match pattern against filename */
    if (fname[0] == '\0')
        return (pat[0] == '\0');	/* Special case */
    return CMUTIL_PatternSubmatch(pat, fname);
}


/*-----------------------------------------------------------------------------
* fpattern_matchn()
*	Attempts to match pattern 'pat' to filename 'fname'.
*	This operates like fpattern_match() except that it does not verify that
*	pattern 'pat' is well-formed, assuming that it has been checked by a
*	prior call to fpattern_isvalid().
*
* Returns
*	1 CMUTIL_True if the filename matches, otherwise 0 CMUTIL_False.
*
* Caveats
*	If 'fname' is null, zero CMUTIL_False is returned.
*
*	If 'pat' is null, zero CMUTIL_False is returned.
*
*	If 'pat' is empty (""), the only filename it matches is the empty ("")
*	string.
*
*	If 'pat' is not a well-formed pattern, unpredictable results may occur.
*
*	Upper and lower case letters are treated the same; alphabetic
*	characters are converted to lower case before matching occurs.
*	Conversion to lower case is dependent upon the current locale setting.
*
* See also
*	fpattern_match().
*/

CMUTIL_Bool CMUTIL_PatternMatchN(const char *pat, const char *fname)
{
    /* Check args */
    if (fname == NULL)
        return CMUTIL_False;

    if (pat == NULL)
        return CMUTIL_False;

    /* Assume that pattern is well-formed */

    /* Attempt to match pattern against filename */
    return CMUTIL_PatternSubmatch(pat, fname);
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/* End fpattern.c */
