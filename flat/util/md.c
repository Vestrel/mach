/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * HISTORY
 * $Log:	md.c,v $
 * Revision 2.2  93/12/14  14:24:42  rvb
 * 	Resurrected and taught to deal with -I prefix's
 * 	[93/12/13            rvb]
 * 
 * Revision 2.7  92/04/06  23:09:14  rpd
 * 	Fixed dep_lookup_or_alloc to handle collisions properly.
 * 	[92/04/06            rpd]
 * 
 * Revision 2.6  92/04/04  17:32:11  rpd
 * 	Fixed get_filename.  It mishandled some ./ and ../ cases.
 * 	[92/04/04            rpd]
 * 
 * 	Fixed dep_lookup_or_alloc to initialize malloc'd memory.
 * 	Fixed bounds check in parse_dependency_line.
 * 	[92/04/03            rpd]
 * 
 * Revision 2.5  92/04/01  19:09:52  rpd
 * 	Made md work with .d files generated by gcc2.
 * 	Use binary tree and hash table to store stuff.
 * 	Use simple lookahead parser to crunch filenames.
 * 	Read everything into memory and throw it out in the end.
 * 	[92/02/09            jvh]
 * 
 * Revision 2.4  92/01/22  22:52:07  rpd
 * 	Look for *.D in addition to *.d.
 * 	[92/01/20            rpd]
 * 
 * Revision 2.3  92/01/17  14:25:17  rpd
 * 	Fixed to exit silently when given a single "*.d" argument.
 * 	[92/01/07            rpd]
 * 
 * 	Picked up Bryan Rosenburg's order_files code.
 * 	Fixed save_dot_o to allow multiple file names (for Mig).
 * 	[91/11/29            rpd]
 * 
 * Revision 2.2  91/03/18  18:22:51  mrt
 * 	Fixed parse_dep cannonicalization code to skip over leading
 * 	escaped newlines like gcc sometimes puts out. Also changed
 * 	to new copyright.
 * 	[91/01/29            mrt]
 * 
 * Revision 2.1  91/02/11  17:26:44  mrt
 * Created.
 * 
 * Revision 1.3  90/12/19  19:39:26  bww
 * 	Print error messages instead of scribbling all over memory
 * 	when there are too many dependencies.  Increase dependency
 * 	line buffer size from 1024 to 16000 characters.  Correctly
 * 	handle spaces between the result filename and the colon.
 * 	From "16-Dec-90  Michael Jones (mbj)" at CMU.  Delinted.
 * 	[90/12/19  19:38:39  bww]
 * 
 * Revision 1.2  90/04/10  14:59:31  bww
 * 	Added code to allow backquoted newlines in *.d files as are
 * 	created by gcc.  From "26-Feb-90  Zon Williamson (zon)" at CMU.
 * 	[90/04/10  14:58:59  bww]
 * 
 * Revision 1.1  90/02/19  02:14:29  bww
 * 	Mach Release 2.5
 * 	[90/02/19  02:14:26  bww]
 * 
 * Revision 1.4  89/08/25  13:10:16  mrt
 * 	Reversed order of test in parse_dep to check for component
 * 	existing before looking at it.
 * 	[89/08/25            mrt]
 * 
 * Revision 1.3  89/05/06  22:40:10  mrt
 * 	Cleanup for Mach 2.5
 * 
 *  1-Apr-88  Sue LoVerso (sue) at Encore Computer Corporation
 *	Added kludge to change any non .o initial target to .o.  Also,
 *	if it is a .s, skip it.  This is a temporary multimax-only fix
 *	until the multimax compiler can be convinced to do something
 *	more reasonable.
 *
 * 29-Apr-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	If specified -u file does not exist, assume it is empty and
 *	generate one.  As a sanity check, it must be possible to create
 *	the output file.
 *	Also, generalized fix below to handle any case of . as a
 *	file name.
 *
 * 25-Mar-87  Mary Thompson (mrt) at Carnegie Mellon
 *	Fixed up pathnamecanonicalization to recognize .// and
 *	drop the second / as well. mmax cpp generates this form.
 *
 *  6-Jan-87  Robert Baron (rvb) at Carnegie-Mellon University
 *	Fixed up pathname canonicalization to that ../../, etc would be
 *	handled correctly.
 *	Also made "force" on by default.
 *
 * 16-Mar-86  Robert Baron (rvb) at Carnegie-Mellon University
 *		Created 4/16/86
 */
/*
 * File:	md.c							    *
 *									    *
 *	Updates makefiles from the .n dependency files generated by the     *
 *	-MD option to "cc" (and "cpp").					    *
 *									    *
 * Abstract:								    *
 *									    *
 *	Basically, "md" does two things:				    *
 *	1) It processes the raw dependency files produced by the cpp -MD    *
 *	   option.  There is one line in the file for every #include	    *
 *	   encountered, but there are repeats and patterns like		    *
 *	   .../dir1/../dir2 appear which should reduce to .../dir2	    *
 *	   Md canonicalizes and flushes repeats from the dependency	    *
 *	   list.  It also sorts the file names and "fills" them to a 78	    *
 *	   character line.						    *
 *	2) Md also updates the makefile directly with the dependency 	    *
 *	   information, so the .d file can be thrown away (-- -d option)    *
 *	   This is done to save space.  Md assumes that dependency 	    *
 *	   information in the makefile is sorted by .o file name and it	    *
 *	   procedes to merge in (add/or replace [as appropriate])  the new  *
 *	   dependency lines that it has generated.  For time effeciency,    *
 *	   Md assumes that any .d files it is given that were created 	    *
 *	   before the creation date of the "makefile" were processed 	    *
 *	   already.  It ignores them unless the force flag (-f) is given.   *
 *									    *
 * Arguments:								    *
 *									    *
 *	-d	delete the .d file after it is processed		    *
 *	-f	force an update of the dependencies in the makefile	    *
 *		even though the makefile is more recent than the .n file    *
 *		(This implies that md has been run already.)		    *
 *	-m	specify the makefile to be upgraded.  The defaults are	    *
 *		"makefile" and then "Makefile".				    *
 *	-u	like -m above, but the file will be created if necessary    *
 *	-o	specify an output file for the dependencies other than a    *
 *		makefile						    *
 *	-v	set the verbose flag					    *
 *	-x	expunge old dependency info from makefile		    *
 *	-D	subswitch for debugging.  can be followed by any of	    *
 *		"c", "d", "m", "o", "t", "D" meaning:			    *
 *		c	show file contents				    *
 *		d	show dependency output				    *
 * 		f	show file names parsed				    *
 *		m	show generation of makefile			    *
 *		o	show files being opened				    *
 *		t	show time comparisons				    *
 *		D	show very low level debugging			    *
 *									    *
 * Author:	Robert V. Baron						    *
 *		Copyright (c) 1986 by Robert V. Baron			    *
 *									    *
\* ************************************************************************ */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <strings.h>
#include <stdio.h>

#define LINESIZE 16000
#define OUTLINELEN 79
#define IObuffer 50000
#define SALUTATION "# Dependencies for File:"
#define SALUTATIONLEN (sizeof SALUTATION - 1)
#define OLDSALUTATION "# DO NOT DELETE THIS LINE"
#define OLDSALUTATIONLEN (sizeof OLDSALUTATION - 1)
#define N_TARGETS_PER_RULE 1000

#if __STDC__
typedef void *generic_pointer_t;
#else
typedef char *generic_pointer_t;
#endif

#define TRUE 1
#define FALSE 0
typedef struct btree_t {
	generic_pointer_t data;
	struct btree_t *left;
	struct btree_t *right;
} *btree_t;

typedef struct dependency_t {
	char 	*dot_o;
	btree_t	dep_tree;
} *dependency_t;

#define quit1(ret, fmt, arg1) { fprintf(stderr, fmt, arg1); exit(ret);}
#define quit2(ret, fmt, arg1, arg2) { fprintf(stderr, fmt, arg1, arg2); exit(ret);}

qsort_strcmp(a, b)
	generic_pointer_t *a, *b;
{
	extern int strcmp();

	return strcmp(*a, *b);
}

qsort_dep_cmp(a, b)
	generic_pointer_t *a, *b;
{
	extern int strcmp();
	dependency_t d1, d2;

	d1 = (dependency_t)*a;
	d2 = (dependency_t)*b;
	
	if (!d1)
	    return -1;
	if (!d2)
	    return 1;
	return strcmp(d1->dot_o, d2->dot_o);
}

char *outfile = (char *) 0;	/* generate dependency file */
FILE *out;
char *makefile = (char *) 0;	/* user supplied makefile name */
char *real_mak_name;		/* actual makefile name (if not supplied) */
char shadow_mak_name[MAXPATHLEN]; /* changes done here then renamed */
#define INCS 20
int IncLim = 0;			/* no Includes specified */
char *Inc[INCS];		/* Includes to strip off */
int  IncLen[INCS];
FILE *mak;			/* for reading makefile */
FILE *makout;			/* for writing shadow */
char makbuf[LINESIZE];		/* one line buffer for makefile */
struct stat makstat;		/* stat of makefile for time comparisons */
int mak_eof = 0;			/* eof seen on makefile */
FILE *find_mak(), *temp_mak();

int delete = 0;			/* -d delete dependency file */
int debug = 0;
int	D_contents = 0;		/* print file contents */
int	D_depend = 0;		/* print dependency outputting */
int	D_file = 0;		/* print parsed file names */
int	D_make = 0;		/* print makefile processing info */
int	D_open = 0;		/* print after succesful open */
int	D_time = 0;		/* print time comparison info */
int force = 1;			/* always update dependency info */
int update = 0;			/* it's ok if the -m file does not exist */
int verbose = 0;		/* tell me something */
int expunge = 0;		/* first flush dependency stuff from makefile */

char *program_name;
#define HASH_SIZE 1000
dependency_t hash_table[HASH_SIZE];
char *malloc();

main(argc,argv)
	int argc;
	char **argv;
{
	int i, ac;
	char **av;

	program_name = *argv;
	{
		register char *cp = program_name;
		while (*cp) if (*cp++ == '/') program_name = cp;
	}

	for ( argv++ ; --argc ; argv++ ) { 
		register char *token = *argv;
		
		if (*token++ != '-' || !*token)
		    break;
		else { 
			register int flag;
			
			for ( ; flag = *token++ ; ) {
				switch (flag) {
				case 'd':
					delete++;
					break;
				case 'f':
					force++;
					break;
				case 'u':
					update++;
				case 'm':
					makefile = *++argv;
					if (--argc < 0) 
					    usage();
					break;
				case 'o':
					outfile = *++argv;
					if (--argc < 0) 
					    usage();
					break;
				case 'v':
					verbose++;
					break;
				case 'x':
					expunge++;
					break;
				case 'I':
					if (!strcmp(token,"."))
						;
					else if (IncLim > INCS-1) {
						fprintf(stderr, "%s: too many includes dropping %s\n",
							program_name, token);
					} else {
						Inc[IncLim] = token;
						IncLen[IncLim] = strlen(token);
						IncLim++;
					}
					goto newtoken;
       				case 'D':
					for ( ; flag = *token++ ; )
						switch (flag) {
						case 'c':
							D_contents++;
							break;
						case 'd':
							D_depend++;
							break;
						case 'f':
							D_file++;
							break;
						case 'm':
							D_make++;
							break;
						case 'o':
							D_open++;
							break;
						case 't':
							D_time++;
							break;
						case 'D':
							debug++;
							break;
						default:
							goto letters;
						}
					goto newtoken;
				default:
					usage();
				}
letters: ;
			}
		}
newtoken: ;
	}

	if (!expunge && argc < 1) 
	    usage();
	if ((int) outfile && (int) makefile)	/* not both */
	    usage();

	/*
	 *      Unfortunately the sh expands *.d to *.d
	 *      if there are no .d files (instead of to no files).
	 *      To prevent the usage "md *.d" in a Makefile
	 *      from causing spurious errors, we special-case it.
	 *      We also look for "*.D".
	 */

	if ((argc == 1) &&
	    ((strcmp(argv[0], "*.d") == 0) ||
	     (strcmp(argv[0], "*.D") == 0)))
	    exit(0);

	if ((int) outfile) {
		if ((out = fopen(outfile, "w")) == NULL) {
			fprintf(stderr, "%s: outfile = \"%s\" ", 
				program_name, outfile);
			perror("fopen");
			(void) fflush(stderr);
			exit(1);
		} else if (D_open)
			printf("%s: opened outfile \"%s\"\n", 
			       program_name, outfile);
	} else if (mak = find_mak(makefile)) {
		out = temp_mak();
	} else if (mak_eof   /* non existent file == mt file */
		   && (out = temp_mak())) {
		;
	} else if (makefile) {
		fprintf(stderr, 
			"%s: makefile \"%s\" can not be opened or stat'ed\n",
			program_name, makefile);
		exit(2);
	} 

	if(verbose)
	    fprintf(stderr, "%s: Opened file \"%s\"\n", 
		    program_name, makefile ? makefile : outfile);

	for (av = argv, ac = argc; ac--; av++) {
		FILE *infile, *open_file();
		
		if((infile = open_file(*av)) == NULL) {
			if (verbose)
			    fprintf(stderr, 
				    "%s: warning: couldn't open \"%s\"\n", 
				    program_name, *av);
			continue;
		}
		lexer_set_file(infile);
		while(parse_dependency_line()) 
		    ;
		fclose(infile);
	}
	if(verbose)
	    fprintf(stderr, "%s: Parsing done\n", program_name);

	output_stuff(mak, out);
	if(verbose)
	    fprintf(stderr, "%s: Output done\n", program_name);

	if (makefile && (rename(shadow_mak_name, real_mak_name) < 0)) {
		perror(real_mak_name);
		exit(1);
	}

	/* if all went well and -d flag given then delete .d files */
	if (delete) {
		if(verbose)
		    fprintf(stderr, "%s: Removing files\n", program_name);

		for (av = argv, ac = argc; ac--; av++) {
			(void) unlink(*av);
		}
	}
	exit(0);
}

usage()
{
	fprintf(stderr, "usage: %s -m <makefile> | -o <outputfile> [ -f ] [ -Dcdfmot ] [ -v ] <file1> ... <filen>\n", program_name);
	exit(1);
}

#define LINELEN 78

output_stuff(in, out)
	FILE *in, *out;
{
	int i;
	char *not_eof;
	dependency_t dep;
	char buf[LINESIZE+1];
	
	buf[0] = '\0';

	/* search for first salutation */
	if (in)
	    while ((not_eof = fgets(buf, LINESIZE, in))
		   && strncmp(buf, SALUTATION, SALUTATIONLEN))
		fputs(buf, out);
	else
	    not_eof = NULL;

	/* sort targets */
	qsort(hash_table, HASH_SIZE, sizeof(dependency_t), qsort_dep_cmp);

	for (i = 0; i < HASH_SIZE; i++) {
		dep = hash_table[i];
		if (!dep)
		    continue;
		if(D_depend)
		    fprintf(stderr, "Outputting dependency for \"%s\"\n", 
			    dep->dot_o);
		if (not_eof && strncmp(buf + SALUTATIONLEN + 1, dep->dot_o, 
				       strlen(dep->dot_o)))
		{
			fputs(buf, out);
			while((not_eof = fgets(buf, LINESIZE, in))
			      && (strncmp(buf, SALUTATION, SALUTATIONLEN) 
				  || strncmp(buf + SALUTATIONLEN + 1, 
					     dep->dot_o, strlen(dep->dot_o))))
			{
				fputs(buf, out);
			}
		}
		output_dependency(out, dep);

		/* skip to next salutation */
		if (not_eof)
		    while (((not_eof = fgets(buf, LINESIZE, in)) != NULL)
			   && strncmp(buf, SALUTATION, SALUTATIONLEN))
			;
	}

	/* Copy rest of make file */
	if (not_eof) {
		fputs(buf, out);
		while (fgets(buf, LINESIZE, in))	
		    fputs(buf, out);
		;
	}
	if (in)
	    fclose(in);
	fclose(out);
}

btree_t btree_alloc(data)
	generic_pointer_t data;
{
	btree_t new;

	new = (btree_t) malloc(sizeof(*new));
	if (!new)
	    quit1(1, "%s: Can't malloc in btree_alloc\n", program_name);
	new->data = data;
	new->left = NULL;
	new->right = NULL;
	return new;
}

btree_insert(tree, data, cmp)
	btree_t tree;
	generic_pointer_t data;
	int (*cmp)();
{
	int x;

	if (tree == NULL)
	    quit1(1, "%s: btree_insert: NULL tree!\n", program_name);

	x = (*cmp)(data, tree->data);
	if (x == 0)
	    return FALSE;
	if (x < 0) {
		if (tree->left)
		    return btree_insert(tree->left, data, cmp);
		else {
			tree->left = btree_alloc(data);
			return TRUE;
		}
	} else {
		if (tree->right)
		    return btree_insert(tree->right, data, cmp);
		else {
			tree->right = btree_alloc(data);
			return TRUE;
		}
	}
	/*NOTREACHED*/
}

btree_walk(tree, fun, arg)
	btree_t tree;
	int (*fun)();
	generic_pointer_t arg;
{
	if (!tree)
	    return;
	btree_walk(tree->left, fun, arg);
	(*fun)(tree->data, arg);
	btree_walk(tree->right, fun, arg);
}

int xxx_out_column;
dependency_t xxx_out_dep;

do_output_dependency(s, out)
	char *s;
	FILE *out;
{
	xxx_out_column += strlen(s) + 1;
	if (xxx_out_column > LINELEN) {
		fprintf(out, "\n%s:", xxx_out_dep->dot_o);
		xxx_out_column = strlen(s) + strlen(xxx_out_dep->dot_o) + 2;
	}
	fprintf(out, " %s", s);
}

output_dependency(out, dep)
	FILE *out;
	dependency_t dep;
{
	fprintf(out, "%s %s", SALUTATION, dep->dot_o);
	xxx_out_column = LINELEN + 1; /* XXX force target to be printed */
	xxx_out_dep = dep;

	btree_walk(dep->dep_tree, do_output_dependency, out);
	fprintf(out, "\n");
}

char *current_file_name = "";

FILE *open_file(file)
	char *file;
{
	FILE *fp;
	fp = fopen(file, "r");
	current_file_name = file;
	if (D_open)
	    fprintf(stderr, "%s: Opened file %s\n", program_name, file);
	return fp;
}

extern char *get_filename();

hash_init()
{
	int i;

	for (i = 0; i < HASH_SIZE; i++)
	    hash_table[i] = NULL;
}

unsigned hash_val(key)
	char *key;
{
	char *s;
	unsigned val;

	val = 0;
	for(s = key; *s; s++) {
		val += *s;
		val %= HASH_SIZE;
	}
	return val;
}

char *mystrdup(source)
     char *source;
{
	char *s;
	s = malloc((unsigned)strlen(source) + 1);
	if (!s)
	    quit1(1, "%s: Can't malloc\n", program_name);
	return strcpy(s, source);
}

dependency_t dep_lookup_or_alloc(dot_o)
	char *dot_o;
{
	unsigned hv, i;
	dependency_t dep;

	i = hv = hash_val(dot_o);
	do {
		dep = hash_table[i];
		if (!dep) {
			dep = (dependency_t)malloc(sizeof(*dep));
			if (!dep)
			    quit1(1, "%s: can't malloc\n", program_name);
			dep->dot_o = mystrdup(dot_o);
			dep->dep_tree = NULL;
			hash_table[i] = dep;
			return dep;
		}

		if (!strcmp(dep->dot_o, dot_o))
			return dep;

		i = (i + 1) % HASH_SIZE;
	} while (i != hv);

	quit2(1, "%s: %s: Hash table overflow. Too many targets\n",
	     program_name, current_file_name);
	/*NOTREACHED*/
}

/* return TRUE if a line was successfully parsed */
parse_dependency_line()
{
	dependency_t	dot_o_list[N_TARGETS_PER_RULE];
	dependency_t dep;
	char *s;
	int i, ndoto;

	skip_white_and_comments();

	/* First parse target list (left of colon) */
	for(ndoto = 0; (s = get_filename()) && *s; ndoto++) {
		if (ndoto >= N_TARGETS_PER_RULE)
		    quit2(1, "%s: %s: Too many targets\n", 
			 program_name, current_file_name);
		dep = dep_lookup_or_alloc(s); /* never NULL */
		dot_o_list[ndoto] = dep;
	}
	if (ndoto == 0)
	    return FALSE;

	skip_spaces();
	if (get_character() != ':')
	    quit2(1, "%s: %s: Syntax error, ':' expected\n", program_name, 
		 current_file_name);

	/* Then parse dependencies (right of colon) */
	while ((s = get_filename()) && *s) {
		s = mystrdup(s);
		for (i = 0; i < ndoto; i++) {
			dep = dot_o_list[i];
			if (dep->dep_tree == NULL)
			    dep->dep_tree = btree_alloc(s);
			else if (!btree_insert(dep->dep_tree, s, strcmp))
			    free(s); /* already there */
		}
		skip_spaces();
	}
	return TRUE;
}

char *get_filename()
{
	static	char buf[MAXPATHLEN];

	char *stack[MAXNAMLEN];
	char *sp = buf;
	int depth;
	int c;

	/* skip spaces until we get to something */
	skip_spaces();

	/* for absolute file names, suck up initial slash */
	if (lookahead() == '/') {
		*sp++ = get_character();
	}

	depth = 0;
	stack[depth] = sp;

	/*
	 *	We maintain the invariant that stack[depth] is a pointer
	 *	to just AFTER the last slash.  For depth zero, this means
	 *	a pointer to the start of the file name (relative name)
	 *	or a pointer to just after the root slash (absolute name).
	 */

	while (((c = lookahead()) != EOF) &&
	       (strchr("\n\t :", c) == NULL)) {
		c = get_character();
		*sp++ = c;
		if (c == '/') {
			/*
			 *	We simplify filenames as follows:
			 *		<prefix>/
			 *		<prefix>./
			 *		<prefix><dir>../
			 *	all become
			 *		<prefix>
			 *	XXX The last simplification is dangerous...
			 */

			if ((sp - stack[depth] == 1) &&
			    !strncmp(stack[depth], "/", 1)) {
				/* we have <prefix>/ */

				sp = stack[depth];
			} else if ((sp - stack[depth] == 2) &&
				   !strncmp(stack[depth], "./", 2)) {
				/* we have <prefix>./ */

				sp = stack[depth];
			} else if ((sp - stack[depth] == 3) &&
				   !strncmp(stack[depth], "../", 3)) {
				if (depth > 0) {
					/* we have <prefix><dir>../ */

					sp = stack[--depth];
				} else {
					/* we have initial ../ */

					stack[depth] = sp;
				}
			} else {
				/* save end of directory */

				stack[++depth] = sp;
			}
		}
	}
	*sp = '\0';
	if (D_file && *buf)
	    fprintf(stderr, "FILE: \"%s\"\n", buf);
	if (IncLim) {
		int i;
		for (i = 0; i < IncLim; i++) {
			int len = IncLen[i];
			if (buf[len] == '/' && !strncmp(Inc[i], buf, len)) {
				return &buf[len+1];
			}
		}
	}
	return buf;
}

skip_spaces()
{
	while (strchr("\t ", lookahead()) != NULL)
	    get_character();
}


skip_white_and_comments()
{
	int c;

 again:
	while (strchr("\n\t ", c = lookahead()) != NULL)
	    get_character();
	if ((c == '#') || ((c == '\n') && (lookahead() == '#'))) {
		while (lookahead() != '\n')
		    get_character();
		goto again;
	}
}

int get_character()
{
	register int c;
	
	/* throw away quoted newlines */
	while (((c = get_raw_character()) == '\\') 
	       && (lookahead() == '\n'))
	{
		get_raw_character(); /* throw newline away */
		skip_spaces();	/* XXX gcc sometimes inserts spaces here */
	}
	return c;
}

#define LOOKAHEAD_NONE -2

static int saved_lookahead = LOOKAHEAD_NONE;
static FILE *lexer_fp = NULL;

lexer_set_file(fp)
	FILE *fp;
{
	saved_lookahead = LOOKAHEAD_NONE;
	lexer_fp = fp;
}

int get_raw_character()
{
	register int c;

	if (saved_lookahead ==  LOOKAHEAD_NONE)
	    c = getc(lexer_fp);
	else {
		c = saved_lookahead;
		saved_lookahead = LOOKAHEAD_NONE;
	}
/*	fprintf(stderr, "RAW: '%c'\n", c); */
	return c;
}

int lookahead()
{
	register int c;

	if (saved_lookahead ==  LOOKAHEAD_NONE)
	    c = get_character();
	else
	    c = saved_lookahead;

	saved_lookahead = c;
/*	fprintf(stderr, "LOOKAHEAD: '%c'\n", c); */
	return c;
}


		/* process makefile */
FILE *
find_mak(file)
char *file;
{
FILE *mak;

	if ((int) file) {
		if ((mak = fopen(file, "r")) != NULL) {
			real_mak_name = file;
		} else if (update) {
			mak_eof = 1;
			real_mak_name = file;
			return NULL;
		} else {
			fprintf(stderr, "%s: file = \"%s\" ", 
				program_name, file);
			perror("fopen");
			(void) fflush(stdout);
			(void) fflush(stderr);
			return NULL;
		}
	} else {
		if ((mak = fopen("makefile", "r")) != NULL) {
			real_mak_name = "makefile";
		} else if ((mak = fopen("Makefile", "r")) != NULL) {
			real_mak_name = "Makefile";
		} else return NULL;
	}

	if (fstat(fileno(mak), &makstat) < 0) {
		fprintf(stderr, "%s: file = \"%s\" ", 
			program_name, real_mak_name);
		perror("stat");
		(void) fflush(stderr);
		return NULL;
	}
	if (D_open)
		printf("%s: opened makefile \"%s\"\n", 
		       program_name, real_mak_name);
	if (D_time)
		printf("%s: makefile time = %ld\n", 
		       program_name, makstat.st_mtime);

	return mak;
}

FILE *temp_mak()
{
	FILE *mak;

	(void) strcpy(shadow_mak_name, real_mak_name);
	(void) strcat(shadow_mak_name, ".md");

	if ((mak = fopen(shadow_mak_name, "w")) == NULL) {
		fprintf(stderr, "%s: file = \"%s\" ", 
			program_name, shadow_mak_name);
		perror("fopen");
		(void) fflush(stdout);
		(void) fflush(stderr);
		return NULL;
	}
	if (D_open)
		printf("%s: opened makefile.md \"%s\"\n", 
		       program_name, shadow_mak_name);

	return mak;
}

expunge_mak(makin, makout)
	register FILE *makin, *makout;
{
	register int len = SALUTATIONLEN;
	register int oldlen = OLDSALUTATIONLEN;

	if (D_make)
		printf("expunging in \"%s\"  ", real_mak_name);

	while (fgets(makbuf, LINESIZE, makin) != NULL) {
		if (D_make && D_contents)
			printf("%s: \"%s\"\n", real_mak_name, makbuf);
		if (! strncmp(makbuf, SALUTATION, len) ||
		    ! strncmp(makbuf, OLDSALUTATION, oldlen))
			break;
		else
			fputs(makbuf, makout);
	}
	mak_eof = 1;
	if (mak_eof)
		(void) fclose(makin);
	if (D_make)
		printf("eof = %d str = \"%s\"", mak_eof, makbuf);
}

