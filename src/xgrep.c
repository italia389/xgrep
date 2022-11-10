// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// xgrep.c		Grep utility using XRE library.

#include "stdos.h"
#include "xre.h"
#include "cxl/excep.h"
#include "cxl/getSwitch.h"
#include "cxl/string.h"
#include "cxl/fastio.h"
#include "cxl/lib.h"
#include "cxl/bmsearch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

// Option flags.
#define OptZero		0x0001		// -0 switch specified.
#define OptCount	0x0002		// -count switch specified.
#define OptForceHdr	0x0004		// -force-hdr switch specified.
#define OptInvert	0x0008		// -invert-match switch specified.
#define OptLineNum	0x0010		// -line-num switch specified.
#define OptLit		0x0020		// -lit switch specified.
#define OptMatchLim	0x0040		// -max-matches switch specified.
#define OptNoHdr	0x0080		// -no-hdr switch specified.
#define OptOnlyFile	0x0100		// -only-file switch specified.
#define OptQuiet	0x0200		// -quiet switch specified.
#define OptSkip		0x0400		// -skip switch specified.

// Global definitions and data.
enum h_type {h_copyright = 1, h_version, h_usage, h_help};

// Container for a regular expression.
typedef struct Pattern {
	struct Pattern *next;
	regex_t regex;
	BMPat plain;
	const char *pat;
	} Pattern;

// Switch options.
typedef struct {
	ushort flags;			// Boolean switches specified.
	int cflagsRE;			// Regular expression compilation flags.
	uint cflagsBM;			// Plain text (Boyer-Moore) compilation flags.
	ssize_t maxMatches;		// File line-match limit.
	size_t nmatch;			// Number of subexpression pattern matches needed for display.
	Pattern *patHead, *patTail;	// Head and tail of linked list of compiled RE patterns.
	} Options;

static char *Myself;
static Options options = {0, REG_ENHANCED, 0, 0, 0, NULL, NULL};

// Exit program with specified type of help information.
static void helpExit(enum h_type t) {
	char *info;

	if(t == h_version) {
		if(asprintf(&info, "%s 1.0.0 (GPLv3) [linked with %s, %s]", Myself, cxlvers(), xrevers()) < 0)
			excep(ExError | ExErrNo);
		}
	else if(t == h_copyright) {
		if(asprintf(&info, "%s (c) Copyright 2022 Richard W. Marinelli", Myself) < 0)
			excep(ExError | ExErrNo);
		}
	else {		// h_usage or h_help
		char *usage;

		if(asprintf(&usage, "Usage:\n    %s {-?, -usage | -C, -copyright | -help | -V, -version}\n"
		 "    %s [-0] [-c, -count] [-E, -no-enhanced] [-e p] [-H, -force-hdr]\n"
		 "     [-h, -no-hdr] [-i, -ignore-case] [-L, -lit] [-l, -only-filename]\n"
		 "     [-m, -max-matches n] [-n, -line-num] [-o, -only-matching] [-p, -pat p]\n"
		 "     [-q, -quiet] [-s, -skip] [-v, -invert-match] [pat] [file ...]",
		 Myself, Myself) < 0)
			excep(ExError | ExErrNo);
		if(t == h_usage)
			info = usage;
		else {
			if(asprintf(&info, "\
%s -- Regular expression search utility.\n\n\
%s\n\n\
Switches and arguments:\n\
  -0			Read list of filenames to search from standard input, separated by null\n\
			characters, instead of from the command line.  Switch is expected to be\n\
			used in concert with the -print0 switch of find(1).\n\
  -?, -usage		Display usage and exit.\n\
  -C, -copyright	Display copyright information and exit.\n\
  -c, -count		Display count of selected lines only.\n\
  -E, -no-enhanced	Disable enhanced features in search patterns, otherwise, enable.\n\
  -e p			Alias for -pat switch.  Switch may be repeated.\n\
  -H, -force-hdr	Always print filename header with output lines.\n\
  -h, -no-hdr		Suppress printing of filename header with output lines.\n\
  -help			Display detailed help and exit.\n\
  -i, -ignore-case	Perform case-insensitive matching, otherwise, case-sensitive.\n\
  -L, -lit		Interpret patterns as literal text instead of regular expressions.\n\
  -l, -only-filename	Display names of files containing selected lines only.\n\
  -m, -max-matches n	Stop scanning each file after n matches are found.\n\
  -n, -line-num		Precede each selected line by its line number.\n\
  -o, -only-matching	Display only the matching portion of the lines.\n\
  -p, -pat p		Use pattern p for searching.  Switch may be repeated.\n\
  -q, -quiet		Quiet mode: suppress normal output.  Scanning stops when first match is\n\
			found.\n\
  -s, -skip		Skip directories and files that cannot be opened (ignore errors).\n\
  -V, -version		Display program version and exit.\n\
  -v, -invert-match	Select lines not matching any of the patterns.\n\n\
  pat			Pattern to use for searching.\n\
  file			Input file, otherwise standard input.\n\
\nNotes:\n\
 1. Either a -pat switch or pat argument must be specified.  If the former, the pat argument\n\
    must be omitted.\n\
 2. A line in a file is selected if at least one pattern matches the line without its\n\
    terminating newline.\n\
 3. The -line-num and -only-matching switches are ignored if -count, -only-filename, or -quiet\n\
    switch is specified.\n\
 4. Program exits with either 0 -> one or more lines were selected, 1 -> no lines were selected,\n\
    or 2 -> an error occurred.", Myself, usage) < 0)
				excep(ExError | ExErrNo);
			}
		}

	excep(ExExit | ExCustom, info);
	}

// Save given search pattern in option table (for later compilation).  Return status.
static int savePat(char *pat) {
	Pattern *pPattern;

	// Allocate memory for pattern container.
	if((pPattern = (Pattern *) malloc(sizeof(Pattern))) == NULL) {
		cxlExcep.flags |= ExcepMem;
		return emsgsys(-1);
		}

	// Save the pattern, mark plain text pattern inactive, and link container into the table.
	pPattern->pat = pat;
	pPattern->next = (Pattern *) (pPattern->plain.pat = NULL);
	if(options.patHead == NULL)
		options.patHead = options.patTail = pPattern;
	else
		options.patTail = options.patTail->next = pPattern;

	return 0;
	}

// Perform program initialization.  Return status or help code.
static int init(int *pArgCount, char ***pArgList) {
	static const char				// Switch names.
		*sw_0[] = {"0", NULL},
		*sw_copyright[] = {"copyright", "C", NULL},
		*sw_count[] = {"count", "c", NULL},
		*sw_force_hdr[] = {"force-hdr", "H", NULL},
		*sw_help[] = {"help", NULL},
		*sw_ignore_case[] = {"ignore-case", "i", NULL},
		*sw_invert_match[] = {"invert-match", "v", NULL},
		*sw_line_num[] = {"line-num", "n", NULL},
		*sw_lit[] = {"lit", "L", NULL},
		*sw_max_matches[] = {"max-matches", "m", NULL},
		*sw_no_enhanced[] = {"no-enhanced", "E", NULL},
		*sw_no_hdr[] = {"no-hdr", "h", NULL},
		*sw_only_filename[] = {"only-filename", "l", NULL},
		*sw_only_matching[] = {"only-matching", "o", NULL},
		*sw_pat[] = {"pat", "p", "e", NULL},
		*sw_quiet[] = {"quiet", "q", NULL},
		*sw_skip[] = {"skip", "s", NULL},
		*sw_usage[] = {"usage", "?", NULL},
		*sw_version[] = {"version", "V", NULL};
	static Switch cmdSwitchTable[] = {		// Command switch table.
/*1*/		{sw_0, SF_NoArg},
/*2*/		{sw_copyright, SF_NoArg},
/*3*/		{sw_count, SF_NoArg},
/*4*/		{sw_force_hdr, SF_NoArg},
/*5*/		{sw_help, SF_NoArg},
/*6*/		{sw_ignore_case, SF_NoArg},
/*7*/		{sw_invert_match, SF_NoArg},
/*8*/		{sw_line_num, SF_NoArg},
/*9*/		{sw_lit, SF_NoArg},
/*10*/		{sw_max_matches, SF_RequiredArg | SF_NumericArg},
/*11*/		{sw_no_enhanced, SF_NoArg},
/*12*/		{sw_no_hdr, SF_NoArg},
/*13*/		{sw_only_filename, SF_NoArg},
/*14*/		{sw_only_matching, SF_NoArg},
/*15*/		{sw_pat, SF_RequiredArg | SF_AllowRepeat},
/*16*/		{sw_quiet, SF_NoArg},
/*17*/		{sw_skip, SF_NoArg},
/*18*/		{sw_usage, SF_NoArg},
/*19*/		{sw_version, SF_NoArg}};
	SwitchResult result;
	Switch *pSwitch = cmdSwitchTable;
	Pattern *pPattern;
	char *str;
	int rtnCode;

	// Get my name.
	str = *(*pArgList)++;
	if((Myself = strrchr(str, '/')) == NULL)
		Myself = str;
	else
		++Myself;

	// Get command switches.
	if(--*pArgCount == 0)
		return h_help;
	while((rtnCode = getSwitch(pArgCount, pArgList, &pSwitch, elementsof(cmdSwitchTable), &result)) > 0)
		switch(rtnCode) {
			case 1:		// -0
				options.flags |= OptZero;
				break;
			case 2:		// -copyright.
				return h_copyright;
			case 3:		// -count
				options.flags |= OptCount;
				break;
			case 4:		// -force-hdr
				options.flags |= OptForceHdr;
				break;
			case 5:		// -help
				return h_help;
			case 6:		// -ignore-case
				options.cflagsRE |= REG_ICASE;
				options.cflagsBM |= PatIgnoreCase;
				break;
			case 7:		// -invert-match
				options.flags |= OptInvert;
				break;
			case 8:		// -line-num
				options.flags |= OptLineNum;
				break;
			case 9:		// -lit
				options.flags |= OptLit;
				break;
			case 10:	// -max-matches
				if(estrtoul(result.arg, 10, &options.maxMatches) < 0)
					return -1;
				if(options.maxMatches == 0)
					return emsg(-1, "-max-matches value must be greater than zero");
				options.flags |= OptMatchLim;
				break;
			case 11:	// -no-enhanced
				options.cflagsRE &= ~REG_ENHANCED;
				break;
			case 12:	// -no-hdr
				options.flags |= OptNoHdr;
				break;
			case 13:	// -only-filename
				options.flags |= OptOnlyFile;
				break;
			case 14:	// -only-matching
				options.nmatch = 1;
				break;
			case 15:	// -pat
				if(savePat(result.arg) < 0)
					return -1;
				break;
			case 16:	// -quiet
				options.flags |= OptQuiet;
				break;
			case 17:	// -skip
				options.flags |= OptSkip;
				break;
			case 18:	// usage.
				return h_usage;
			default:	// version.
				return h_version;
			}

	// Switch error?
	if(rtnCode < 0)
		return rtnCode;
	if((options.flags & (OptForceHdr | OptNoHdr)) == (OptForceHdr | OptNoHdr))
		return emsg(-1, "Conflicting switches: -force-hdr and -no-hdr");
	if((options.flags & (OptNoHdr | OptOnlyFile)) == (OptNoHdr | OptOnlyFile))
		return emsg(-1, "Conflicting switches: -no-hdr and -only-file");

	// Get pattern from command line if no -pat switch.
	if(options.patHead == NULL) {
		if(*pArgCount == 0)
			return emsg(-1, "No search pattern specified");
		if(savePat(*(*pArgList)++) < 0)
			return -1;
		--*pArgCount;
		}

	// Error if -0 switch and file argument(s) specified.
	if((options.flags & OptZero) && *pArgCount > 0)
		return emsg(-1, "File argument(s) not allowed with -0 switch");

	// Compile pattern(s).  If -lit switch or plain text, compile with bmcomp().
	pPattern = options.patHead;
	do {
		if(!(options.flags & OptLit)) {
			if((rtnCode = xregcomp(&pPattern->regex, pPattern->pat, options.cflagsRE)) != 0)
				return emsgf(-1, "Invalid RE '%s': %s", pPattern->pat, xregmsg(rtnCode));
			if(pPattern->regex.pflags & PropHaveRegical)
				continue;
			}
		char *pat, patBuf[strlen(pPattern->pat) + 1];

		if(!(options.flags & OptLit) && (pPattern->regex.pflags & PropHaveEscLit))
			(void) strconv(pat = patBuf, pPattern->pat, NULL, '\0');	// Can't fail.
		else
			pat = (char *) pPattern->pat;
		if(bmcomp(&pPattern->plain, pat, options.cflagsBM) != 0)
			return -1;
		} while((pPattern = pPattern->next) != NULL);

	// Success.
	return 0;
	}

// Process a file.  Return -1 if error, otherwise number of matches found.
static ssize_t processFile(FastFile *otpFile, FastFile *inpFile, char *inpFilename, bool singleFile) {
	int rtnCode;
	Pattern *pPattern;
	regmatch_t regMatch;
	char *match;
	ssize_t recLen;
	size_t len, recCount = 0;
	ssize_t selectCount = 0;
	bool selected;
	bool printFilename = (options.flags & OptForceHdr) || (!singleFile && !(options.flags & OptNoHdr));

	// Read file.
	while((recLen = ffgets(inpFile)) > 0) {
		++recCount;
		(void) ffchomp(inpFile);

		// Check all patterns for a match.
		selected = (options.flags & OptInvert) != 0;
		pPattern = options.patHead;
		do {
			if(pPattern->plain.pat != NULL) {
				if((match = (char *) bmexec(&pPattern->plain, inpFile->lineBuf)) != NULL)
					goto Match;
				}
			else {
				rtnCode = xregexec(&pPattern->regex, inpFile->lineBuf, options.nmatch, &regMatch, 0);
				if(rtnCode == 0) {
Match:
					// Line matched.
					selected = !selected;
					break;
					}
				else if(rtnCode != REG_NOMATCH)
					// Error.
					return emsgf(-1, "%s, matching pattern '%s'", xregmsg(rtnCode), pPattern->pat);
				}
			} while((pPattern = pPattern->next) != NULL);

		if(selected) {
			// Line was selected; that is, either found a match and -v was not specified, or no pattern matched and
			// -v was specified.  Return count now if first selected line is all that is needed (-l or -q switch
			// specified).
			++selectCount;
			if(options.flags & (OptOnlyFile | OptQuiet))
				return selectCount;

			// Display line (or just text that matched, if appropriate) if -count (and -quiet) switches were not
			// specified.  Precede line by filename and line number if corresponding option(s) set.
			if(!(options.flags & OptCount)) {
				if(printFilename)
					ffprintf(otpFile, "%s:", inpFilename);
				if(options.flags & OptLineNum)
					ffprintf(otpFile, "%ld:", recCount);
				if(options.nmatch == 0)
					ffputs(inpFile->lineBuf, otpFile);
				else {
					if(pPattern->plain.pat != NULL)
						len = pPattern->plain.len;
					else {
						match = inpFile->lineBuf + regMatch.rm_so;
						len = regMatch.rm_eo - regMatch.rm_so;
						}
					ffwrite(match, len, otpFile);
					}
				ffputc('\n', otpFile);
				}

			// Hit match limit?
			if(selectCount == options.maxMatches)
				return selectCount;
			}
		}

	// Read error?
	if(recLen < 0)
		return -1;

	return selectCount;
	}

// Get next filename argument from argument list or standard input, store it in *pInpFilename, and return 1.  If none left,
// return zero.  If error occurs, return -1.
static int nextArg(int *pArgCount, char ***pArgList, FastFile *filenameFile, char **pInpFilename) {

	if(!(options.flags & OptZero)) {
		if(*pArgCount == 0)
			return 0;
		*pInpFilename = *(*pArgList)++;		// May be "-".
		--*pArgCount;
		}
	else {
		ssize_t recLen;

		if((recLen = ffgets(filenameFile)) <= 0)
			return recLen;			// EOF or error.
		*pInpFilename = filenameFile->lineBuf;
		}

	return 1;
	}

// Return true if filename not NULL and is a directory, otherwise false.
static bool isDir(char *filename) {

	if(filename != NULL) {
		struct stat s;

		if(stat(filename, &s) == 0 && (s.st_mode & S_IFMT) == S_IFDIR)
			return true;
		}
	return false;
	}

// Process files (which must be read from standard input if -0 switch specified).  Return -1 if error, otherwise 0 (success, at
// least one line selected) or 1 (success, no lines selected).
static int processFiles(int argCount, char **argList) {
	FastFile *inpFile = NULL, *otpFile, *filenameFile = NULL;
	char *inpFilename;
	int rtnCode, anySelected = 1;
	bool singleFile = !(options.flags & OptZero) && argCount <= 1;
	bool onePass = false;
	ssize_t selectCount, totalSelectCount = 0;

	// Open standard output as a fast file.
	if((otpFile = ffopen(NULL, 'w')) == NULL)
		return -1;

	// Open standard input as a fast file if -0 switch was specified.
	if(options.flags & OptZero) {
		if((filenameFile = ffopen(NULL, 'r')) == NULL || ffsetdelim(FFDelimNUL, filenameFile) != 0)
			return -1;
		}
	else if(argCount == 0) {
		inpFilename = NULL;
		onePass = true;
		goto OpenInp;
		}

	// Open input files in sequence and process them.
	for(;;) {
		if((rtnCode = nextArg(&argCount, &argList, filenameFile, &inpFilename)) < 0)
			return -1;
		if(rtnCode == 0)
			break;		// No files left.
OpenInp:
		// Skip directories if -skip switch.
		if((options.flags & OptSkip) && isDir(inpFilename))
			goto NextFile;

		// Open file for input.
		if((inpFile = ffopen(inpFilename, 'r')) == NULL) {
			if(!(options.flags & OptSkip))
				return -1;

			// Ignore error.
			eclear();
			goto NextFile;
			}

		// Process file.
		if(inpFilename == NULL)
			inpFilename = "(stdin)";
		if((selectCount = processFile(otpFile, inpFile, inpFilename, singleFile)) < 0 || ffclose(inpFile) != 0)
			return -1;
		if(selectCount > 0) {
			anySelected = 0;
			totalSelectCount += selectCount;
			}

		// Display filename and number of lines selected if appropriate.
		if(!(options.flags & (OptQuiet | OptNoHdr))) {
			if((options.flags & OptCount) || (selectCount > 0 && (options.flags & OptOnlyFile))) {
				ffputs(inpFilename, otpFile);
				if(options.flags & OptCount)
					ffprintf(otpFile, ":%ld", selectCount);
				ffputc('\n', otpFile);
				}
			}
NextFile:
		if(onePass)
			break;
		}

	// Error if -0 switch and no filenames read from standard input.
	if((options.flags & OptZero) && inpFile == NULL)
		return emsg(-1, "Empty filename list with -0 switch");

	// Display total number of lines selected from all files, if applicable.
	if(!(options.flags & OptQuiet) && (options.flags & (OptCount | OptNoHdr)) == (OptCount | OptNoHdr))
		ffprintf(otpFile, "%lu\n", totalSelectCount);

	// Close output file and return result.
	return ffclose(otpFile) != 0 ? -1 : anySelected;
	}

// Begin program.
int main(int argCount, char **argList) {
	int rtnCode;

	// Get switches and prepare to process files.
	if((rtnCode = init(&argCount, &argList)) < 0)
		goto ErrExit;
	else if(rtnCode > 0)
		helpExit(rtnCode);

	// Check if -0 switch specified and file arguments also specified (which is an error).
	if((options.flags & OptZero) && argCount > 0) {
		(void) emsg(-1, "File argument(s) not allowed with -0 switch");
		goto ErrExit;
		}

	// Process files.  Return 2 if error.
	if((rtnCode = processFiles(argCount, argList)) < 0) {
ErrExit:
		excep(ExError | ExNoExit | ExMessage);
		exit(2);
		}

	// Done.  Return 1 (failure) if no lines were selected, otherwise zero.
	return rtnCode;
	}
