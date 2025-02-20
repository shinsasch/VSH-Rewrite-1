/* vim: set sts=2 ts=8 sw=2 tw=99 et: */
/*  Pawn compiler
 *
 *  Function and variable definition and declaration, statement parser.
 *
 *  Copyright (c) ITB CompuPhase, 1997-2006
 *
 *  This software is provided "as-is", without any express or implied warranty.
 *  In no event will the authors be held liable for any damages arising from
 *  the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1.  The origin of this software must not be misrepresented; you must not
 *      claim that you wrote the original software. If you use this software in
 *      a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *  2.  Altered source versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software.
 *  3.  This notice may not be removed or altered from any source distribution.
 *
 *  Version: $Id$
 */
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <amtl/am-unused.h>
#include <amtl/am-platform.h>

#if defined __WIN32__ || defined _WIN32 || defined __MSDOS__
  #include <conio.h>
  #include <io.h>
#endif

#if defined LINUX || defined __FreeBSD__ || defined __OpenBSD__ || defined DARWIN
  #include "sclinux.h"
  #include "binreloc.h" /* from BinReloc, see www.autopackage.org */
  #include <unistd.h>
#endif

#if defined FORTIFY
  #include <alloc/fortify.h>
#endif

#if defined __BORLANDC__ || defined __WATCOMC__
  #include <dos.h>
  static unsigned total_drives; /* dummy variable */
  #define dos_setdrive(i)       _dos_setdrive(i,&total_drives)
#elif defined _MSC_VER && defined _WIN32
  #include <direct.h>           /* for _chdrive() */
  #define dos_setdrive(i)       _chdrive(i)
#endif
#if defined __BORLANDC__
  #include <dir.h>              /* for chdir() */
#elif defined __WATCOMC__
  #include <direct.h>           /* for chdir() */
#endif
#if defined __WIN32__ || defined _WIN32 || defined _Windows
  #include <windows.h>
#endif

#include <time.h>
#if defined(SOURCEMOD_BUILD)
# include <sourcemod_version.h>
# define SOURCEPAWN_VERSION SOURCEMOD_VERSION
#endif

#ifdef __EMSCRIPTEN__
# include <emscripten.h>
#endif

#include "lstring.h"
#include "sc.h"
#include "sctracker.h"
#include "sp_symhash.h"
#define VERSION_STR "3.2.3636"
#define VERSION_INT 0x0302

int pc_anytag = 0;
int pc_functag = 0;
int pc_tag_string = 0;
int pc_tag_void = 0;
int pc_tag_object = 0;
int pc_tag_bool = 0;
int pc_tag_null_t = 0;
int pc_tag_nullfunc_t = 0;

static void resetglobals(void);
static void initglobals(void);
static char *get_extension(char *filename);
static void setopt(int argc,char **argv,char *oname,char *ename,char *pname,
                   char *codepage);
static void setconfig(char *root);
static void setcaption(void);
static void about(void);
static void setconstants(void);
static void parse(void);
static void dumplits(void);
static void dumpzero(int count);
static void declglb(declinfo_t *decl,int fpublic,int fstatic,int stock);
static void declstructvar(char *firstname,int fpublic, pstruct_t *pstruct);
static void declloc(int tokid);
static void dodelete();
static void decl_const(int table);
static void declstruct();
static void decl_enum(int table);
static cell needsub(int *tag,constvalue **enumroot);
static void initials(int ident,int tag,cell *size,int dim[],int numdim,
                     constvalue *enumroot);
static cell initarray(int ident,int tag,int dim[],int numdim,int cur,
                      int startlit,int counteddim[],constvalue *lastdim,
                      constvalue *enumroot,int *errorfound);
static cell initvector(int ident,int tag,cell size,int fillzero,
                       constvalue *enumroot,int *errorfound);
static void initials3(declinfo_t *decl);
static cell fix_char_size(declinfo_t *decl);
static cell init(int ident,int *tag,int *errorfound);
static symbol *funcstub(int tokid, declinfo_t *decl, const int *thistag);
static int newfunc(declinfo_t *decl, const int *thistag, int fpublic, int fstatic, int stock, symbol **symp);
static int declargs(symbol *sym, int chkshadow, const int *thistag);
static void doarg(symbol *sym, declinfo_t *decl, int offset, int chkshadow, arginfo *arg);
static void reduce_referrers(symbol *root);
static int testsymbols(symbol *root,int level,int testlabs,int testconst);
static void destructsymbols(symbol *root,int level);
static constvalue *find_constval_byval(constvalue *table,cell val);
static void statement(int *lastindent,int allow_decl);
static void compound(int stmt_sameline,int starttok);
static int test(int label,int parens,int invert);
static int doexpr(int comma,int chkeffect,int allowarray,int mark_endexpr,
                  int *tag,symbol **symptr,int chkfuncresult);
static int doexpr2(int comma,int chkeffect,int allowarray,int mark_endexpr,
                   int *tag,symbol **symptr,int chkfuncresult,value *lval);
static void doassert(void);
static void doexit(void);
static int doif(void);
static int dowhile(void);
static int dodo(void);
static int dofor(void);
static void doswitch(void);
static void doreturn(void);
static void dofuncenum(int listmode);
static void dotypedef();
static void dotypeset();
static void domethodmap(LayoutSpec spec);
static bool dousing();
static void dobreak(void);
static void docont(void);
static void dosleep(void);
static void addwhile(int *ptr);
static void delwhile(void);
static int *readwhile(void);
static void inst_datetime_defines(void);
static void inst_binary_name(char *binfname);
static int operatorname(char *name);
static int parse_new_typename(const token_t *tok);
static int parse_new_decl(declinfo_t *decl, const token_t *first, int flags);
static int reparse_old_decl(declinfo_t *decl, int flags);
static int reparse_new_decl(declinfo_t *decl, int flags);
static void check_void_decl(const declinfo_t *decl, int variable);

enum {
  TEST_PLAIN,           /* no parentheses */
  TEST_THEN,            /* '(' <expr> ')' or <expr> 'then' */
  TEST_DO,              /* '(' <expr> ')' or <expr> 'do' */
  TEST_OPT,             /* '(' <expr> ')' or <expr> */
};
static int norun      = 0;      /* the compiler never ran */
static int autozero   = 1;      /* if 1 will zero out the variable, if 0 omit the zeroing */
static int lastst     = 0;      /* last executed statement type */
static int nestlevel  = 0;      /* number of active (open) compound statements */
static int endlessloop= 0;      /* nesting level of endless loop */
static int rettype    = 0;      /* the type that a "return" expression should have */
static int skipinput  = 0;      /* number of lines to skip from the first input file */
static int verbosity  = 1;      /* verbosity level, 0=quiet, 1=normal, 2=verbose */
static int sc_reparse = 0;      /* needs 3th parse because of changed prototypes? */
static int sc_parsenum = 0;     /* number of the extra parses */
static int wq[wqTABSZ];         /* "while queue", internal stack for nested loops */
static int *wqptr;              /* pointer to next entry */
#if !defined SC_LIGHT
  static char sc_rootpath[_MAX_PATH];
  static char *sc_documentation=NULL;/* main documentation */
#endif
#if defined __WIN32__ || defined _WIN32 || defined _Windows
  static HWND hwndFinish = 0;
#endif

int glbstringread = 0;
char g_tmpfile[_MAX_PATH] = {0};

/*  "main" of the compiler
 */
int pc_compile(int argc, char *argv[])
{
#ifdef __EMSCRIPTEN__
  EM_ASM(
    if (ENVIRONMENT_IS_NODE) {
      FS.mkdir('/fakeroot');
      FS.mount(NODEFS, { root: '/' }, '/fakeroot');
      FS.chdir('/fakeroot/' + process.cwd());
    }
  );
#endif

  int entry,i,jmpcode;
  int retcode;
  char incfname[_MAX_PATH];
  char codepage[MAXCODEPAGE+1];
  void *inpfmark;
  int lcl_packstr,lcl_needsemicolon,lcl_tabsize,lcl_require_newdecls;
  char *ptr;

  /* set global variables to their initial value */
  initglobals();
  errorset(sRESET,0);
  errorset(sEXPRRELEASE,0);
  lexinit();

  /* make sure that we clean up on a fatal error; do this before the first
   * call to error(). */
  if ((jmpcode=setjmp(errbuf))!=0)
    goto cleanup;

  sp_Globals = NewHashTable();
  if (!sp_Globals)
    error(FATAL_ERROR_OOM);

  /* allocate memory for fixed tables */
  inpfname=(char*)malloc(_MAX_PATH);
  if (inpfname==NULL)
    error(FATAL_ERROR_OOM);         /* insufficient memory */
  litq=(cell*)malloc(litmax*sizeof(cell));
  if (litq==NULL)
    error(FATAL_ERROR_OOM);         /* insufficient memory */
  if (!phopt_init())
    error(FATAL_ERROR_OOM);         /* insufficient memory */

  setopt(argc,argv,outfname,errfname,incfname,codepage);
  strcpy(binfname,outfname);
  ptr=get_extension(binfname);
  if (ptr!=NULL && stricmp(ptr,".asm")==0)
    set_extension(binfname,".smx",TRUE);
  else
    set_extension(binfname,".smx",FALSE);
  /* set output names that depend on the input name */
  if (sc_listing)
    set_extension(outfname,".lst",TRUE);
  else
    set_extension(outfname,".asm",TRUE);
  if (strlen(errfname)!=0)
    remove(errfname);   /* delete file on startup */
  else if (verbosity>0)
    setcaption();
  setconfig(argv[0]);   /* the path to the include and codepage files */
  sc_ctrlchar_org=sc_ctrlchar;
  lcl_packstr=sc_packstr;
  lcl_needsemicolon=sc_needsemicolon;
  lcl_require_newdecls=sc_require_newdecls;
  lcl_tabsize=sc_tabsize;
  #if !defined NO_CODEPAGE
    if (!cp_set(codepage))      /* set codepage */
      error(FATAL_ERROR_NO_CODEPAGE);
  #endif
  /* optionally create a temporary input file that is a collection of all
   * input files
   */
  assert(get_sourcefile(0)!=NULL);  /* there must be at least one source file */
  if (get_sourcefile(1)!=NULL) {
    /* there are at least two or more source files */
    char *tname,*sname;
    void *ftmp,*fsrc;
    int fidx;
    #if defined __WIN32__ || defined _WIN32
      tname=_tempnam(NULL,"pawn");
    #elif defined __MSDOS__ || defined _Windows
      tname=tempnam(NULL,"pawn");
    #elif defined(MACOS) && !defined(__MACH__)
      /* tempnam is not supported for the Macintosh CFM build. */
      error(FATAL_ERROR_INVALID_INSN,get_sourcefile(1));
      tname=NULL;
      sname=NULL;
    #else
      char *buffer = strdup(P_tmpdir "/pawn.XXXXXX");
      close(mkstemp(buffer));
      tname=buffer;
    #endif
    ftmp=pc_createsrc(tname);
    for (fidx=0; (sname=get_sourcefile(fidx))!=NULL; fidx++) {
      unsigned char tstring[128];
      fsrc=pc_opensrc(sname);
      if (fsrc==NULL) {
        pc_closesrc(ftmp);
        remove(tname);
        strcpy(inpfname,sname); /* avoid invalid filename */
        error(FATAL_ERROR_READ,sname);
      } /* if */
      pc_writesrc(ftmp,(unsigned char*)"#file \"");
      pc_writesrc(ftmp,(unsigned char*)sname);
      pc_writesrc(ftmp,(unsigned char*)"\"\n");
      while (!pc_eofsrc(fsrc) && pc_readsrc(fsrc,tstring,sizeof tstring)) {
        pc_writesrc(ftmp,tstring);
      } /* while */
      pc_closesrc(fsrc);
    } /* for */
    pc_closesrc(ftmp);
    strcpy(inpfname,tname);
    strcpy(g_tmpfile,tname);
    free(tname);
  } else {
    strcpy(inpfname,get_sourcefile(0));
  } /* if */
  inpf_org=pc_opensrc(inpfname);
  if (inpf_org==NULL)
    error(FATAL_ERROR_READ,inpfname);
  freading=TRUE;
  outf=(FILE*)pc_openasm(outfname); /* first write to assembler file (may be temporary) */
  if (outf==NULL)
    error(FATAL_ERROR_WRITE,outfname);
  setconstants();               /* set predefined constants and tagnames */
  for (i=0; i<skipinput; i++)   /* skip lines in the input file */
    if (pc_readsrc(inpf_org,pline,sLINEMAX)!=NULL)
      fline++;                  /* keep line number up to date */
  skipinput=fline;
  sc_status=statFIRST;
  /* write starting options (from the command line or the configuration file) */
  if (sc_listing) {
    char string[150];
    sprintf(string,"#pragma ctrlchar 0x%02x\n"
                   "#pragma pack %s\n"
                   "#pragma semicolon %s\n"
                   "#pragma tabsize %d\n",
            sc_ctrlchar,
            sc_packstr ? "true" : "false",
            sc_needsemicolon ? "true" : "false",
            sc_tabsize);
    pc_writeasm(outf,string);
    setfiledirect(inpfname);
  } /* if */
  /* do the first pass through the file (or possibly two or more "first passes") */
  sc_parsenum=0;
  inpfmark=pc_getpossrc(inpf_org,NULL);
  do {
    /* reset "defined" flag of all functions and global variables */
    reduce_referrers(&glbtab);
    delete_symbols(&glbtab,0,TRUE,FALSE);
    #if !defined NO_DEFINE
      delete_substtable();
      inst_datetime_defines();
      inst_binary_name(binfname);
    #endif
    resetglobals();
    pstructs_free();
    funcenums_free();
    methodmaps_free();
    sc_ctrlchar=sc_ctrlchar_org;
    sc_packstr=lcl_packstr;
    sc_needsemicolon=lcl_needsemicolon;
    sc_require_newdecls=lcl_require_newdecls;
    sc_tabsize=lcl_tabsize;
    errorset(sRESET,0);
    /* reset the source file */
    inpf=inpf_org;
    freading=TRUE;
    pc_resetsrc(inpf,inpfmark); /* reset file position */
    fline=skipinput;            /* reset line number */
    sc_reparse=FALSE;           /* assume no extra passes */
    sc_status=statFIRST;        /* resetglobals() resets it to IDLE */

    if (strlen(incfname)>0) {
      if (strcmp(incfname,sDEF_PREFIX)==0) {
        plungefile(incfname,FALSE,TRUE);    /* parse "default.inc" */
      } else {
        if (!plungequalifiedfile(incfname)) /* parse "prefix" include file */
          error(FATAL_ERROR_READ,incfname);
      } /* if */
    } /* if */
    preprocess();                       /* fetch first line */
    parse();                            /* process all input */
    sc_parsenum++;
  } while (sc_reparse);

  /* second (or third) pass */
  sc_status=statWRITE;                  /* set, to enable warnings */

  if (sc_listing)
    goto cleanup;

  /* ??? for re-parsing the listing file instead of the original source
   * file (and doing preprocessing twice):
   * - close input file, close listing file
   * - re-open listing file for reading (inpf)
   * - open assembler file (outf)
   */

  /* reset "defined" flag of all functions and global variables */
  reduce_referrers(&glbtab);
  delete_symbols(&glbtab,0,TRUE,FALSE);
  funcenums_free();
  methodmaps_free();
  pstructs_free();
  #if !defined NO_DEFINE
    delete_substtable();
    inst_datetime_defines();
    inst_binary_name(binfname);
  #endif
  resetglobals();
  sc_ctrlchar=sc_ctrlchar_org;
  sc_packstr=lcl_packstr;
  sc_needsemicolon=lcl_needsemicolon;
  sc_require_newdecls=lcl_require_newdecls;
  sc_tabsize=lcl_tabsize;
  errorset(sRESET,0);
  /* reset the source file */
  inpf=inpf_org;
  freading=TRUE;
  pc_resetsrc(inpf,inpfmark);   /* reset file position */
  fline=skipinput;              /* reset line number */
  lexinit();                    /* clear internal flags of lex() */
  sc_status=statWRITE;          /* allow to write --this variable was reset by resetglobals() */
  writeleader(&glbtab);
  insert_dbgfile(inpfname);     /* attach to debug information */
  insert_inputfile(inpfname);   /* save for the error system */
  if (strlen(incfname)>0) {
    if (strcmp(incfname,sDEF_PREFIX)==0)
      plungefile(incfname,FALSE,TRUE);  /* parse "default.inc" (again) */
    else
      plungequalifiedfile(incfname);    /* parse implicit include file (again) */
  } /* if */
  preprocess();                         /* fetch first line */
  parse();                              /* process all input */
  /* inpf is already closed when readline() attempts to pop of a file */
  writetrailer();                       /* write remaining stuff */

  entry=testsymbols(&glbtab,0,TRUE,FALSE);  /* test for unused or undefined
                                             * functions and variables */
  if (!entry)
    error(13);                  /* no entry point (no public functions) */

cleanup:
  if (inpf!=NULL)               /* main source file is not closed, do it now */
    pc_closesrc(inpf);

  // Write the binary file.
  if (!(sc_asmfile || sc_listing) && errnum==0 && jmpcode==0) {
    pc_resetasm(outf);
    assemble(binfname, outf);
  }

  if (outf!=NULL) {
    pc_closeasm(outf,!(sc_asmfile || sc_listing));
    outf=NULL;
  } /* if */

#if !defined SC_LIGHT
    if (errnum==0 && strlen(errfname)==0) {
      if ((!norun && (sc_debug & sSYMBOLIC)!=0) || verbosity>=2) {
        pc_printf("Code size:         %8ld bytes\n", (long)code_idx);
        pc_printf("Data size:         %8ld bytes\n", (long)glb_declared*sizeof(cell));
        pc_printf("Stack/heap size:   %8ld bytes\n", (long)pc_stksize*sizeof(cell));
        pc_printf("Total requirements:%8ld bytes\n", (long)code_idx+(long)glb_declared*sizeof(cell)+(long)pc_stksize*sizeof(cell));
      } /* if */
    } /* if */
#endif

  if (g_tmpfile[0] != '\0') {
    remove(g_tmpfile);
  }
  if (inpfname!=NULL) {
    free(inpfname);
  } /* if */
  if (litq!=NULL)
    free(litq);
  phopt_cleanup();
  stgbuffer_cleanup();
  clearstk();
  assert(jmpcode!=0 || loctab.next==NULL);/* on normal flow, local symbols
                                           * should already have been deleted */
  delete_symbols(&loctab,0,TRUE,TRUE);    /* delete local variables if not yet
                                           * done (i.e. on a fatal error) */
  delete_symbols(&glbtab,0,TRUE,TRUE);
  DestroyHashTable(sp_Globals);
  delete_consttable(&tagname_tab);
  delete_consttable(&libname_tab);
  delete_aliastable();
  delete_pathtable();
  delete_sourcefiletable();
  delete_inputfiletable();
  delete_dbgstringtable();
  funcenums_free();
  methodmaps_free();
  pstructs_free();
  #if !defined NO_DEFINE
    delete_substtable();
  #endif
  #if !defined SC_LIGHT
    delete_docstringtable();
    if (sc_documentation!=NULL)
      free(sc_documentation);
  #endif
  delete_autolisttable();
  if (errnum!=0) {
    if (strlen(errfname)==0)
      pc_printf("\n%d Error%s.\n",errnum,(errnum>1) ? "s" : "");
    retcode=1;
  } else if (warnnum!=0){
    if (strlen(errfname)==0)
      pc_printf("\n%d Warning%s.\n",warnnum,(warnnum>1) ? "s" : "");
    retcode=0;          /* use "0", so that MAKE and similar tools continue */
  } else {
    retcode=jmpcode;
    if (retcode==0 && verbosity>=2)
      pc_printf("\nDone.\n");
  } /* if */
  #if defined __WIN32__ || defined _WIN32 || defined _Windows
    if (IsWindow(hwndFinish))
      PostMessageA(hwndFinish,RegisterWindowMessageA("PawnNotify"),retcode,0L);
  #endif
  #if defined FORTIFY
    Fortify_ListAllMemory();
  #endif
  return retcode;
}

int pc_addconstant(const char *name,cell value,int tag)
{
  errorset(sFORCESET,0);        /* make sure error engine is silenced */
  sc_status=statIDLE;
  add_constant(name,value,sGLOBAL,tag);
  return 1;
}

static void inst_binary_name(char *binfname)
{
  size_t i, len;
  char *binptr;
  char newpath[512], newname[512];

  binptr = NULL;
  len = strlen(binfname);
  for (i = len - 1; i < len; i--)
  {
    if (binfname[i] == '/'
#if defined WIN32 || defined _WIN32
      || binfname[i] == '\\'
#endif
      )
    {
      binptr = &binfname[i + 1];
      break;
    }
  }

  if (binptr == NULL)
  {
    binptr = binfname;
  }

  snprintf(newpath, sizeof(newpath), "\"%s\"", binfname);
  snprintf(newname, sizeof(newname), "\"%s\"", binptr);

  insert_subst("__BINARY_PATH__", newpath, 15);
  insert_subst("__BINARY_NAME__", newname, 15);
}

static void inst_datetime_defines(void)
{
  char date[64];
  char ltime[64];
  time_t td;
  struct tm *curtime;

  time(&td);
  curtime = localtime(&td);

#if defined __EMSCRIPTEN__
  snprintf(date, sizeof(date), "\"%02d/%02d/%04d\"", curtime->tm_mon + 1, curtime->tm_mday, curtime->tm_year + 1900);
  snprintf(ltime, sizeof(ltime), "\"%02d:%02d:%02d\"", curtime->tm_hour, curtime->tm_min, curtime->tm_sec);
#else
  strftime(date, 31, "\"%m/%d/%Y\"", curtime);
  strftime(ltime, 31, "\"%H:%M:%S\"", curtime);
#endif

  insert_subst("__DATE__", date, 8);
  insert_subst("__TIME__", ltime, 8);
}

const char *pc_typename(int tag)
{
  if (tag == 0)
    return "int";
  if (tag == sc_rationaltag)
    return "float";
  if (tag == pc_tag_string)
    return "char";
  return pc_tagname(tag);
}

const char *pc_tagname(int tag)
{
  constvalue *ptr=tagname_tab.next;
  for (; ptr; ptr=ptr->next) {
    if (TAGID(ptr->value) == TAGID(tag))
      return ptr->name;
  }
  return "__unknown__";
}

constvalue *pc_tagptr(const char *name)
{
  constvalue *ptr=tagname_tab.next;
  for (; ptr; ptr=ptr->next) {
    if (strcmp(name, ptr->name)==0)
      return ptr;
  }
  return NULL;
}

int pc_findtag(const char *name)
{
  constvalue *ptr=tagname_tab.next;
  for (; ptr; ptr=ptr->next) {
    if (strcmp(name,ptr->name)==0)
      return ptr->value;
  }
  return -1;
}

int pc_addtag(const char *name)
{
  int val;
  int flags = 0;

  if (name==NULL) {
    /* no tagname was given, check for one */
    char *nameptr;
    if (lex(&val,&nameptr)!=tLABEL) {
      lexpush();
      return 0;         /* untagged */
    } /* if */
    name = nameptr;
  } /* if */

  if (isupper(*name))
    flags |= FIXEDTAG;

  return pc_addtag_flags(name, flags);
}

int pc_addtag_flags(const char *name, int flags)
{
  constvalue *ptr;
  int last;

  assert((flags & FUNCTAG) || strchr(name,':')==NULL); /* colon should already have been stripped */
  last=0;
  ptr=tagname_tab.next;
  while (ptr!=NULL) {
    if (strcmp(name,ptr->name)==0) {
      // Update the flag set.
      ptr->value |= flags;
      return ptr->value;
    }
    if (TAGID(ptr->value) > last)
      last = TAGID(ptr->value);
    ptr = ptr->next;
  } /* while */

  /* tagname currently unknown, add it */
  int tag = (last + 1) | flags;
  append_constval(&tagname_tab, name, (cell)tag, 0);
  return tag;
}

static void resetglobals(void)
{
  /* reset the subset of global variables that is modified by the first pass */
  curfunc=NULL;         /* pointer to current function */
  lastst=0;             /* last executed statement type */
  nestlevel=0;          /* number of active (open) compound statements */
  rettype=0;            /* the type that a "return" expression should have */
  litidx=0;             /* index to literal table */
  stgidx=0;             /* index to the staging buffer */
  sc_labnum=0;          /* top value of (internal) labels */
  staging=0;            /* true if staging output */
  declared=0;           /* number of local cells declared */
  glb_declared=0;       /* number of global cells declared */
  code_idx=0;           /* number of bytes with generated code */
  ntv_funcid=0;         /* incremental number of native function */
  curseg=0;             /* 1 if currently parsing CODE, 2 if parsing DATA */
  freading=FALSE;       /* no input file ready yet */
  fline=0;              /* the line number in the current file */
  fnumber=0;            /* the file number in the file table (debugging) */
  fcurrent=0;           /* current file being processed (debugging) */
  sc_intest=FALSE;      /* true if inside a test */
  sideeffect=0;         /* true if an expression causes a side-effect */
  stmtindent=0;         /* current indent of the statement */
  indent_nowarn=FALSE;  /* do not skip warning "217 loose indentation" */
  sc_allowtags=TRUE;    /* allow/detect tagnames */
  sc_status=statIDLE;
  pc_addlibtable=TRUE;  /* by default, add a "library table" to the output file */
  pc_docexpr=FALSE;
  pc_deprecate=NULL;
  pc_memflags=0;
}

static void initglobals(void)
{
  resetglobals();

  sc_asmfile=FALSE;     /* do not create .ASM file */
  sc_listing=FALSE;     /* do not create .LST file */
  skipinput=0;          /* number of lines to skip from the first input file */
  sc_ctrlchar=CTRL_CHAR;/* the escape character */
  litmax=sDEF_LITMAX;   /* current size of the literal table */
  errnum=0;             /* number of errors */
  warnnum=0;            /* number of warnings */
  verbosity=1;          /* verbosity level, no copyright banner */
  sc_debug=sCHKBOUNDS|sSYMBOLIC;   /* sourcemod: full debug stuff */
  pc_optimize=sOPTIMIZE_DEFAULT;   /* sourcemod: full optimization */
  sc_packstr=TRUE;     /* strings are packed by default */
  sc_needsemicolon=FALSE;/* semicolon required to terminate expressions? */
  sc_require_newdecls = FALSE;
  sc_dataalign=sizeof(cell);
  pc_stksize=sDEF_AMXSTACK;/* default stack size */
  sc_tabsize=8;         /* assume a TAB is 8 spaces */
  sc_rationaltag=0;     /* assume no support for rational numbers */
  rational_digits=0;    /* number of fractional digits */

  outfname[0]='\0';     /* output file name */
  errfname[0]='\0';     /* error file name */
  inpf=NULL;            /* file read from */
  inpfname=NULL;        /* pointer to name of the file currently read from */
  outf=NULL;            /* file written to */
  litq=NULL;            /* the literal queue */
  glbtab.next=NULL;     /* clear global variables/constants table */
  loctab.next=NULL;     /*   "   local      "    /    "       "   */
  tagname_tab.next=NULL;/* tagname table */
  libname_tab.next=NULL;/* library table (#pragma library "..." syntax) */

  pline[0]='\0';        /* the line read from the input file */
  lptr=NULL;            /* points to the current position in "pline" */
  curlibrary=NULL;      /* current library */
  inpf_org=NULL;        /* main source file */

  wqptr=wq;             /* initialize while queue pointer */

#if !defined SC_LIGHT
  sc_documentation=NULL;
#endif
}

static char *get_extension(char *filename)
{
  char *ptr;

  assert(filename!=NULL);
  ptr=strrchr(filename,'.');
  if (ptr!=NULL) {
    /* ignore extension on a directory or at the start of the filename */
    if (strchr(ptr,DIRSEP_CHAR)!=NULL || ptr==filename || *(ptr-1)==DIRSEP_CHAR)
      ptr=NULL;
  } /* if */
  return ptr;
}

/* set_extension
 * Set the default extension, or force an extension. To erase the
 * extension of a filename, set "extension" to an empty string.
 */
void set_extension(char *filename,const char *extension,int force)
{
  char *ptr;

  assert(extension!=NULL && (*extension=='\0' || *extension=='.'));
  assert(filename!=NULL);
  ptr=get_extension(filename);
  if (force && ptr!=NULL)
    *ptr='\0';          /* set zero terminator at the position of the period */
  if (force || ptr==NULL)
    strcat(filename,extension);
}

static const char *option_value(const char *optptr,char **argv,int argc,int *arg)
{
  const char *ptr;

  if (*(optptr+1)=='=' || *(optptr+1)==':')
    ptr=optptr+2;
  else
    ptr=optptr+1;

  if (strlen(ptr)==0 && *arg!=argc-1)
    ptr=argv[++*arg];

  return ptr;
}

static int toggle_option(const char *optptr,int option)
{
  switch ((*(optptr+1)=='=' || *(optptr+1)==':') ? *(optptr+2) : *(optptr+1)) {
  case '-':
    option=FALSE;
    break;
  case '+':
    option=TRUE;
    break;
  default:
    about();
  } /* switch */
  return option;
}

/* Parsing command line options is indirectly recursive: parseoptions()
 * calls parserespf() to handle options in a a response file and
 * parserespf() calls parseoptions() at its turn after having created
 * an "option list" from the contents of the file.
 */
static void parserespf(char *filename,char *oname,char *ename,char *pname,
                       char *codepage);

static void parseoptions(int argc,char **argv,char *oname,char *ename,char *pname,
                         char *codepage)
{
  char str[_MAX_PATH];
  const char *ptr;
  int arg,i,isoption;

  for (arg=1; arg<argc; arg++) {
#if DIRSEP_CHAR=='/'
    isoption= argv[arg][0]=='-';
#else
    isoption= argv[arg][0]=='/' || argv[arg][0]=='-';
#endif
    if (isoption) {
      ptr=&argv[arg][1];
      switch (*ptr) {
      case 'a':
        if (*(ptr+1)!='\0')
          about();
        sc_asmfile=TRUE;        /* skip last pass of making binary file */
        if (verbosity>1)
          verbosity=1;
        break;
      case 'c':
        strlcpy(codepage,option_value(ptr,argv,argc,&arg),MAXCODEPAGE);  /* set name of codepage */
        break;
#if defined dos_setdrive
      case 'D':                 /* set active directory */
        ptr=option_value(ptr,argv,argc,&arg);
        if (ptr[1]==':')
          dos_setdrive(toupper(*ptr)-'A'+1);    /* set active drive */
        chdir(ptr);
        break;
#endif
      case 'e':
        strlcpy(ename,option_value(ptr,argv,argc,&arg),_MAX_PATH); /* set name of error file */
        break;
      case 'E':
        sc_warnings_are_errors = true;
        break;
#if defined __WIN32__ || defined _WIN32 || defined _Windows
      case 'H':
        hwndFinish=(HWND)atoi(option_value(ptr,argv,argc,&arg));
        if (!IsWindow(hwndFinish))
          hwndFinish=(HWND)0;
        break;
#endif
      case 'h':
        sc_showincludes = 1;
        break;
      case 'i':
        strlcpy(str,option_value(ptr,argv,argc,&arg),sizeof str);  /* set name of include directory */
        i=strlen(str);
        if (i>0) {
          if (str[i-1]!=DIRSEP_CHAR) {
            str[i]=DIRSEP_CHAR;
            str[i+1]='\0';
          } /* if */
          insert_path(str);
        } /* if */
        break;
      case 'l':
        if (*(ptr+1)!='\0')
          about();
        sc_listing=TRUE;        /* skip second pass & code generation */
        break;
      case 'o':
        strlcpy(oname,option_value(ptr,argv,argc,&arg),_MAX_PATH); /* set name of (binary) output file */
        break;
      case 'O':
        pc_optimize=*option_value(ptr,argv,argc,&arg) - '0';
        if (pc_optimize<sOPTIMIZE_NONE || pc_optimize>=sOPTIMIZE_NUMBER || pc_optimize==sOPTIMIZE_NOMACRO)
          about();
        break;
      case 'p':
        strlcpy(pname,option_value(ptr,argv,argc,&arg),_MAX_PATH); /* set name of implicit include file */
        break;
      case 's':
        skipinput=atoi(option_value(ptr,argv,argc,&arg));
        break;
      case 't':
        sc_tabsize=atoi(option_value(ptr,argv,argc,&arg));
        break;
      case 'v':
        verbosity= isdigit(*option_value(ptr,argv,argc,&arg)) ? atoi(option_value(ptr,argv,argc,&arg)) : 2;
        if (sc_asmfile && verbosity>1)
          verbosity=1;
        break;
      case 'w':
        i=(int)strtol(option_value(ptr,argv,argc,&arg),(char **)&ptr,10);
        if (*ptr=='-')
          pc_enablewarning(i,0);
        else if (*ptr=='+')
          pc_enablewarning(i,1);
        else if (*ptr=='\0')
          pc_enablewarning(i,2);
        break;
      case '\\':                /* use \ instead for escape characters */
        sc_ctrlchar='\\';
        break;
      case '^':                 /* use ^ instead for escape characters */
        sc_ctrlchar='^';
        break;
      case ';':
        sc_needsemicolon=toggle_option(ptr,sc_needsemicolon);
        break;
      default:                  /* wrong option */
        about();
      } /* switch */
    } else if (argv[arg][0]=='@') {
      #if !defined SC_LIGHT
        parserespf(&argv[arg][1],oname,ename,pname,codepage);
      #endif
    } else if ((ptr=strchr(argv[arg],'='))!=NULL) {
      i=(int)(ptr-argv[arg]);
      if (i>sNAMEMAX) {
        i=sNAMEMAX;
        error(200,argv[arg],sNAMEMAX);  /* symbol too long, truncated to sNAMEMAX chars */
      } /* if */
      strlcpy(str,argv[arg],i+1);       /* str holds symbol name */
      i=atoi(ptr+1);
      add_constant(str,i,sGLOBAL,0);
    } else {
      strlcpy(str,argv[arg],sizeof(str)-5); /* -5 because default extension is ".sp" */
      set_extension(str,".sp",FALSE);
      insert_sourcefile(str);
      /* The output name is the first input name with a different extension,
       * but it is stored in a different directory
       */
      if (strlen(oname)==0) {
        if ((ptr=strrchr(str,DIRSEP_CHAR))!=NULL)
          ptr++;          /* strip path */
        else
          ptr=str;
        assert(strlen(ptr)<_MAX_PATH);
        strcpy(oname,ptr);
      } /* if */
      set_extension(oname,".asm",TRUE);
    } /* if */
  } /* for */
}

#if !defined SC_LIGHT
static void parserespf(char *filename,char *oname,char *ename,char *pname,
                       char *codepage)
{
#define MAX_OPTIONS     100
  FILE *fp;
  char *string, *ptr, **argv;
  int argc;
  long size;

  if ((fp=fopen(filename,"r"))==NULL)
    error(FATAL_ERROR_READ,filename);
  /* load the complete file into memory */
  fseek(fp,0L,SEEK_END);
  size=ftell(fp);
  fseek(fp,0L,SEEK_SET);
  assert(size<INT_MAX);
  if ((string=(char *)malloc((int)size+1))==NULL)
    error(FATAL_ERROR_OOM);                 /* insufficient memory */
  /* fill with zeros; in MS-DOS, fread() may collapse CR/LF pairs to
   * a single '\n', so the string size may be smaller than the file
   * size. */
  memset(string,0,(int)size+1);
  ke::Unused() << fread(string,1,(int)size,fp);
  fclose(fp);
  /* allocate table for option pointers */
  if ((argv=(char **)malloc(MAX_OPTIONS*sizeof(char*)))==NULL)
    error(FATAL_ERROR_OOM);                 /* insufficient memory */
  /* fill the options table */
  ptr=strtok(string," \t\r\n");
  for (argc=1; argc<MAX_OPTIONS && ptr!=NULL; argc++) {
    /* note: the routine skips argv[0], for compatibility with main() */
    argv[argc]=ptr;
    ptr=strtok(NULL," \t\r\n");
  } /* for */
  if (ptr!=NULL)
    error(FATAL_ERROR_ALLOC_OVERFLOW,"option table");
  /* parse the option table */
  parseoptions(argc,argv,oname,ename,pname,codepage);
  /* free allocated memory */
  free(argv);
  free(string);
}
#endif

static void setopt(int argc,char **argv,char *oname,char *ename,char *pname,
                   char *codepage)
{
  delete_sourcefiletable(); /* make sure it is empty */
  *oname='\0';
  *ename='\0';
  *pname='\0';
  *codepage='\0';
  strcpy(pname,sDEF_PREFIX);

  #if 0 /* needed to test with BoundsChecker for DOS (it does not pass
         * through arguments) */
    insert_sourcefile("test.p");
    strcpy(oname,"test.asm");
  #endif

  #if !defined SC_LIGHT
    /* first parse a "config" file with default options */
    if (argv[0]!=NULL) {
      char cfgfile[_MAX_PATH];
      char *ext;
      strcpy(cfgfile,argv[0]);
      if ((ext=strrchr(cfgfile,DIRSEP_CHAR))!=NULL) {
        *(ext+1)='\0';          /* strip the program filename */
        strcat(cfgfile,"pawn.cfg");
      } else {
        strcpy(cfgfile,"pawn.cfg");
      } /* if */
      if (access(cfgfile,4)==0)
        parserespf(cfgfile,oname,ename,pname,codepage);
    } /* if */
  #endif
  parseoptions(argc,argv,oname,ename,pname,codepage);
  if (get_sourcefile(0)==NULL)
    about();
}

#if defined __BORLANDC__ || defined __WATCOMC__
# pragma argsused
#endif
static void setconfig(char *root)
{
    char path[_MAX_PATH];
    char *ptr,*base;
    int len;

    /* add the default "include" directory */
#if defined KE_WINDOWS
      GetModuleFileNameA(NULL,path,_MAX_PATH);
#elif defined ENABLE_BINRELOC
      /* see www.autopackage.org for the BinReloc module */
      br_init_lib(NULL);
      ptr=br_find_exe("spcomp");
      strlcpy(path,ptr,sizeof path);
      free(ptr);
#elif defined __EMSCRIPTEN__
      if (EM_ASM_INT({
        if (ENVIRONMENT_IS_NODE) {
          stringToUTF8('/fakeroot' + __filename, $0, $1);
          return 1;
        }
        return 0;
      }, path, sizeof(path)) == 0 && root != NULL) {
        strlcpy(path, root, sizeof(path));
      }
#else
      if (root!=NULL)
        strlcpy(path,root,sizeof path); /* path + filename (hopefully) */
#endif

# if defined __MSDOS__
      /* strip the options (appended to the path + filename) */
      if ((ptr=strpbrk(path," \t/"))!=NULL)
        *ptr='\0';
# endif /* __MSDOS__ */

    /* terminate just behind last \ or : */
    if ((ptr=strrchr(path,DIRSEP_CHAR))!=NULL || (ptr=strchr(path,':'))!=NULL) {
      /* If there is no "\" or ":", the string probably does not contain the
       * path; so we just don't add it to the list in that case
       */
      *(ptr+1)='\0';
      base=ptr;
      strcat(path,"include");
      len=strlen(path);
      path[len]=DIRSEP_CHAR;
      path[len+1]='\0';
      /* see if it exists */
      if (access(path,0)!=0 && *base==DIRSEP_CHAR) {
        /* There is no "include" directory below the directory where the compiler
         * is found. This typically means that the compiler is in a "bin" sub-directory
         * and the "include" is below the *parent*. So find the parent...
         */
        *base='\0';
        if ((ptr=strrchr(path,DIRSEP_CHAR))!=NULL) {
          *(ptr+1)='\0';
          strcat(path,"include");
          len=strlen(path);
          path[len]=DIRSEP_CHAR;
          path[len+1]='\0';
        } else {
          *base=DIRSEP_CHAR;
        } /* if */
      } /* if */
      insert_path(path);

      /* same for the codepage root */
# if !defined NO_CODEPAGE
        *ptr='\0';
        if (!cp_path(path,"codepage"))
          error(FATAL_ERROR_INVALID_PATH,path);
# endif /* !NO_CODEPAGE */

      /* also copy the root path (for the XML documentation) */
# if !defined SC_LIGHT
        *ptr='\0';
        strcpy(sc_rootpath,path);
# endif /* !SC_LIGHT */
    } /* if */
}

static void setcaption(void)
{
  pc_printf("SourcePawn Compiler %s\n", SOURCEPAWN_VERSION);
  pc_printf("Copyright (c) 1997-2006 ITB CompuPhase\n");
  pc_printf("Copyright (c) 2004-2017 AlliedModders LLC\n\n");
}

static void about(void)
{
  if (strlen(errfname)==0) {
    setcaption();
    pc_printf("Usage:   spcomp <filename> [filename...] [options]\n\n");
    pc_printf("Options:\n");
    pc_printf("         -a       output assembler code\n");
    pc_printf("         -c<name> codepage name or number; e.g. 1252 for Windows Latin-1\n");
#if defined dos_setdrive
    pc_printf("         -Dpath   active directory path\n");
#endif
    pc_printf("         -e<name> set name of error file (quiet compile)\n");
#if defined __WIN32__ || defined _WIN32 || defined _Windows
    pc_printf("         -H<hwnd> window handle to send a notification message on finish\n");
#endif
    pc_printf("         -h       show included file paths\n");
    pc_printf("         -i<name> path for include files\n");
    pc_printf("         -l       create list file (preprocess only)\n");
    pc_printf("         -o<name> set base name of (P-code) output file\n");
    pc_printf("         -O<num>  optimization level (default=-O%d)\n",pc_optimize);
    pc_printf("             0    no optimization\n");
#if 0 /* not used for SourceMod */
    pc_printf("             1    JIT-compatible optimizations only\n");
#endif
    pc_printf("             2    full optimizations\n");
    pc_printf("         -p<name> set name of \"prefix\" file\n");
    pc_printf("         -s<num>  skip lines from the input file\n");
    pc_printf("         -t<num>  TAB indent size (in character positions, default=%d)\n",sc_tabsize);
    pc_printf("         -v<num>  verbosity level; 0=quiet, 1=normal, 2=verbose (default=%d)\n",verbosity);
    pc_printf("         -w<num>  disable a specific warning by its number\n");
    pc_printf("         -E       treat warnings as errors\n");
    pc_printf("         -\\       use '\\' for escape characters\n");
    pc_printf("         -^       use '^' for escape characters\n");
    pc_printf("         -;<+/->  require a semicolon to end each statement (default=%c)\n", sc_needsemicolon ? '+' : '-');
    pc_printf("         sym=val  define constant \"sym\" with value \"val\"\n");
    pc_printf("         sym=     define constant \"sym\" with value 0\n");
#if defined __WIN32__ || defined _WIN32 || defined _Windows || defined __MSDOS__
    pc_printf("\nOptions may start with a dash or a slash; the options \"-d0\" and \"/d0\" are\n");
    pc_printf("equivalent.\n");
#endif
    pc_printf("\nOptions with a value may optionally separate the value from the option letter\n");
    pc_printf("with a colon (\":\"), an equal sign (\"=\"), or a space (\" \"). That is, the options \"-d0\", \"-d=0\",\n");
    pc_printf("\"-d:0\", and \"-d 0\" are all equivalent. \"-;\" is an exception to this and cannot use a space.\n");
  } /* if */
  norun = 1;
  longjmp(errbuf,3);        /* user abort */
}

static void setconstants(void)
{
  int debug;

  assert(sc_status==statIDLE);
  append_constval(&tagname_tab,"_",0,0);/* "untagged" */
  append_constval(&tagname_tab,"bool",1,0);

  pc_anytag = pc_addtag("any");
  pc_functag = pc_addtag_flags("Function", FIXEDTAG|FUNCTAG);
  pc_tag_string = pc_addtag("String");
  sc_rationaltag = pc_addtag("Float");
  pc_tag_void = pc_addtag_flags("void", FIXEDTAG);
  pc_tag_object = pc_addtag_flags("object", FIXEDTAG|OBJECTTAG);
  pc_tag_bool = pc_addtag("bool");
  pc_tag_null_t = pc_addtag_flags("null_t", FIXEDTAG|OBJECTTAG);
  pc_tag_nullfunc_t = pc_addtag_flags("nullfunc_t", FIXEDTAG|OBJECTTAG);

  add_constant("true",1,sGLOBAL,1);     /* boolean flags */
  add_constant("false",0,sGLOBAL,1);
  add_constant("EOS",0,sGLOBAL,0);      /* End Of String, or '\0' */
  add_constant("INVALID_FUNCTION", -1, sGLOBAL, pc_tag_nullfunc_t);
  add_constant("cellbits",32,sGLOBAL,0);
  add_constant("cellmax",INT_MAX,sGLOBAL,0);
  add_constant("cellmin",INT_MIN,sGLOBAL,0);
  add_constant("charbits",sCHARBITS,sGLOBAL,0);
  add_constant("charmin",0,sGLOBAL,0);
  add_constant("charmax",~(-1UL << sCHARBITS) - 1,sGLOBAL,0);
  add_constant("ucharmax",(1 << (sizeof(cell)-1)*8)-1,sGLOBAL,0);

  add_constant("__Pawn",VERSION_INT,sGLOBAL,0);

  debug=0;
  if ((sc_debug & (sCHKBOUNDS | sSYMBOLIC))==(sCHKBOUNDS | sSYMBOLIC))
    debug=2;
  else if ((sc_debug & sCHKBOUNDS)==sCHKBOUNDS)
    debug=1;
  add_constant("debug",debug,sGLOBAL,0);
}

static void dodecl(const token_t *tok)
{
  declinfo_t decl;

  if (tok->id == tNATIVE || tok->id == tFORWARD) {
    parse_decl(&decl, DECLFLAG_MAYBE_FUNCTION);
    funcstub(tok->id, &decl, NULL);
    return;
  }

  int fpublic = FALSE, fstock = FALSE, fstatic = FALSE;
  switch (tok->id) {
    case tPUBLIC:
      fpublic = TRUE;
      break;
    case tSTOCK:
      fstock = TRUE;
      if (matchtoken(tSTATIC))
        fstatic = TRUE;
      break;
    case tSTATIC:
      fstatic = TRUE;

      // For compatibility, we must include this case. Though "stock" should
      // come first.
      if (matchtoken(tSTOCK))
        fstock = TRUE;
      break;
  }

  int flags = DECLFLAG_MAYBE_FUNCTION | DECLFLAG_VARIABLE | DECLFLAG_ENUMROOT;
  if (tok->id == tNEW)
    flags |= DECLFLAG_OLD;
  
  if (!parse_decl(&decl, flags)) {
    // Error will have been reported earlier. Reset |decl| so we don't crash
    // thinking tag -1 has every flag.
    decl.type.tag = 0;
  }

  // Hacky bag o' hints as to whether this is a variable decl.
  bool probablyVariable = tok->id == tNEW ||
                          decl.type.has_postdims ||
                          !lexpeek('(') ||
                          ((decl.type.usage & uCONST) == uCONST);

  if (!decl.opertok && probablyVariable) {
    if (tok->id == tNEW && decl.type.is_new)
      error(143);
    if (decl.type.tag & STRUCTTAG) {
      pstruct_t *pstruct = pstructs_find(pc_tagname(decl.type.tag));
      declstructvar(decl.name, fpublic, pstruct);
    } else {
      declglb(&decl, fpublic, fstatic, fstock);
    }
  } else {
    if (!newfunc(&decl, NULL, fpublic, fstatic, fstock, NULL)) {
      // Illegal function or declaration. Drop the line, reset literal queue.
      error(10);
      lexclr(TRUE);
      litidx = 0;
    }
  }
}

/*  parse       - process all input text
 *
 *  At this level, only static declarations and function definitions are legal.
 */
static void parse(void)
{
  token_t tok;

  while (freading){
    switch (lextok(&tok)) {
    case 0:
      /* ignore zero's */
      break;
    case tSYMBOL:
#if 0
      if (strcmp(tok.str, "class") == 0) {
        domethodmap(Layout_Class);
        break;
      }
#endif
      // Fallthrough.
    case tINT:
    case tOBJECT:
    case tCHAR:
    case tVOID:
    case tLABEL:
      lexpush();
      // Fallthrough.
    case tNEW:
    case tSTATIC:
    case tPUBLIC:
    case tSTOCK:
    case tOPERATOR:
    case tNATIVE:
    case tFORWARD:
    {
      dodecl(&tok);
      break;
    }
    case tFUNCTAG:
      dofuncenum(FALSE);
      break;
    case tTYPEDEF:
      dotypedef();
      break;
    case tTYPESET:
      dotypeset();
      break;
    case tSTRUCT:
      declstruct();
      break;
    case tCONST:
      decl_const(sGLOBAL);
      break;
    case tENUM:
      decl_enum(sGLOBAL);
      break;
    case tFUNCENUM:
      dofuncenum(TRUE);
      break;
    case tMETHODMAP:
      domethodmap(Layout_MethodMap);
      break;
    case tUSING:
      if (!dousing()) {
        lexclr(TRUE);
        litidx=0;
      }
      break;
    case '}':
      error(54);                /* unmatched closing brace */
      break;
    case '{':
      error(55);                /* start of function body without function header */
      break;
    default:
      if (freading) {
        error(10);              /* illegal function or declaration */
        lexclr(TRUE);           /* drop the rest of the line */
        litidx=0;               /* drop any literal arrays (strings) */
      } /* if */
    } /* switch */
  } /* while */
}

/*  dumplits
 *
 *  Dump the literal pool (strings etc.)
 *
 *  Global references: litidx (referred to only)
 */
static void dumplits(void)
{
  int j,k;

  if (sc_status==statSKIP)
    return;

  k=0;
  while (k<litidx){
    /* should be in the data segment */
    assert(curseg==2);
    defstorage();
    j=16;       /* 16 values per line */
    while (j && k<litidx){
      outval(litq[k], FALSE);
      stgwrite(" ");
      k++;
      j--;
      if (j==0 || k>=litidx)
        stgwrite("\n");         /* force a newline after 10 dumps */
      /* Note: stgwrite() buffers a line until it is complete. It recognizes
       * the end of line as a sequence of "\n\0", so something like "\n\t"
       * so should not be passed to stgwrite().
       */
    } /* while */
  } /* while */
}

/*  dumpzero
 *
 *  Dump zero's for default initial values
 */
static void dumpzero(int count)
{
  int i;

  if (sc_status==statSKIP || count<=0)
    return;
  assert(curseg==2);
  defstorage();
  i=0;
  while (count-- > 0) {
    outval(0, FALSE);
    i=(i+1) % 16;
    stgwrite((i==0 || count==0) ? "\n" : " ");
    if (i==0 && count>0)
      defstorage();
  } /* while */
}

/* declstruct - declare global struct symbols
 * 
 * global references: glb_declared (altered)
 */
static void declstructvar(char *firstname,int fpublic, pstruct_t *pstruct)
{
  char name[sNAMEMAX+1];
  int tok,i;
  cell val;
  char *str;
  int cur_litidx = 0;
  cell *values, *found;
  int usage;
  symbol *mysym,*sym;

  strcpy(name, firstname);

  values = (cell *)malloc(pstruct->argcount * sizeof(cell));
  found = (cell *)malloc(pstruct->argcount * sizeof(cell));
  
  memset(found, 0, sizeof(cell) * pstruct->argcount);

  //:TODO: Make this work with stock

  /**
   * Lastly, very lastly, we will insert a copy of this variable.
   * This is soley to expose the pubvar.
   */
  usage = uREAD|uCONST|uSTRUCT;
  if (fpublic)
    usage |= uPUBLIC;
  mysym = NULL;
  for (sym=glbtab.next; sym!=NULL; sym=sym->next) {
    if (strcmp(name, sym->name) == 0) {
      if ((sym->usage & uSTRUCT) && sym->vclass == sGLOBAL) {
        if (sym->usage & uDEFINE) {
          error(21, name);
        } else {
          if (sym->usage & uPUBLIC && !fpublic)
             error(42);
        }
      } else {
        error(21, name);
      }
      mysym = sym;
      break;
    }
  }
  if (!mysym)
    mysym=addsym(name, 0, iVARIABLE, sGLOBAL, pc_addtag(pstruct->name), usage);

  if (!matchtoken('=')) {
    matchtoken(';');
    /* Mark it as undefined instead */
    mysym->usage = uSTOCK|uSTRUCT;
    free(found);
    free(values);
    return;
  }

  mysym->usage = usage;
  needtoken('{');

  do {
    structarg_t *arg;
    /* Detect early exit */
    if (matchtoken('}')) {
      lexpush();
      break;
    }
    tok=lex(&val,&str);
    if (tok != tSYMBOL) {
      error(1, "-identifier-", str);
      continue;
    }
    arg=pstructs_getarg(pstruct,str);
    if (arg == NULL)
      error(96, str, sym->name);
    needtoken('=');
    cur_litidx = litidx;
    tok=lex(&val,&str);
    if (!arg) {
      continue;
    }
    if (tok == tSTRING) {
      assert(litidx != 0);
      if (arg->dimcount != 1)
      {
        error(48);
      } else if (arg->tag != pc_tag_string) {
        error(213);
      }
      values[arg->index] = glb_declared * sizeof(cell);
      glb_declared += (litidx-cur_litidx);
      found[arg->index] = 1;
    } else if (tok == tNUMBER || tok == tRATIONAL) {
      /* eat optional 'f' */
      matchtoken('f');
      if (arg->ident != iVARIABLE && arg->ident != iREFERENCE) {
        error(23);
      } else {
        if ((arg->tag == pc_addtag("Float") && tok == tNUMBER) ||
          (arg->tag == 0 && tok == tRATIONAL))
        {
          error(213);
        }
        if (arg->ident == iVARIABLE) {
          values[arg->index] = val;
        } else if (arg->ident == iREFERENCE) {
          values[arg->index] = glb_declared * sizeof(cell);
          glb_declared += 1;
          litadd(val);
          cur_litidx = litidx;
        }
        found[arg->index] = 1;
      }
    } else if (tok == tSYMBOL) {
      for (sym=glbtab.next; sym!=NULL; sym=sym->next) {
        if (sym->vclass != sGLOBAL)
          continue;
        if (strcmp(sym->name, str) == 0) {
          if (arg->ident == iREFERENCE && sym->ident != iVARIABLE) {
            error(97, str);
          } else if (arg->ident == iARRAY) {
            if (sym->ident != iARRAY) {
              error(97, str);
            } else {
              /* :TODO: We should check dimension sizes here... */
            }
          } else if (arg->ident == iREFARRAY) {
            if (sym->ident != iARRAY)
              error(97, str);
            /* :TODO: Check dimension sizes! */
          } else {
            error(97, str);
          }
          if (sym->tag != arg->tag)
            error(213);
          sym->usage |= uREAD;
          values[arg->index] = sym->addr();
          found[arg->index] = 1;
          refer_symbol(sym, mysym);
          break;
        }
      }
      if (!sym)
        error(17, str);
    } else {
      error(1, "-identifier-", str);
    }
  } while (matchtoken(','));
  needtoken('}');
  matchtoken(';');  /* eat up optional semicolon */

  for (i=0; i<pstruct->argcount; i++) {
    if (!found[i]) {
      structarg_t *arg = pstruct->args[i];
      if (arg->ident == iREFARRAY) {
        values[arg->index] = glb_declared * sizeof(cell);
        glb_declared += 1;
        litadd(0);
        cur_litidx = litidx;
      } else if (arg->ident == iVARIABLE) {
        values[arg->index] = 0;
      } else {
        /* :TODO: broken for iARRAY! (unused tho) */
      }
    }
  }

  mysym->setAddr(glb_declared * sizeof(cell));
  glb_declared += pstruct->argcount;

  for (i=0; i<pstruct->argcount; i++)
    litadd(values[i]);

  begdseg();
  dumplits();
  litidx=0;

  free(found);
  free(values);
}

/*  declglb     - declare global symbols
 *
 *  Declare a static (global) variable. Global variables are stored in
 *  the DATA segment.
 *
 *  global references: glb_declared     (altered)
 */
static void declglb(declinfo_t *decl,int fpublic,int fstatic,int fstock)
{
  int ispublic;
  cell cidx;
  ucell address;
  int glb_incr;
  int slength=0;
  short filenum;
  symbol *sym;
#if !defined NDEBUG
  cell glbdecl=0;
#endif

  assert(!fpublic || !fstatic);         /* may not both be set */
  filenum=fcurrent;                     /* save file number at the start of the declaration */

  for (;;) {
    typeinfo_t *type = &decl->type;

    check_void_decl(decl, TRUE);

    ispublic=fpublic;
    if (decl->name[0]==PUBLIC_CHAR) {
      ispublic=TRUE;                    /* implicitly public variable */
      assert(!fstatic);
    }
    slength = fix_char_size(decl);
    sym=findconst(decl->name,NULL);
    if (sym==NULL) {
      sym=findglb(decl->name);
    } /* if */
    /* we have either:
     * a) not found a matching variable (or rejected it, because it was a shadow)
     * b) found a global variable and we were looking for that global variable
     */
    if (sym!=NULL && (sym->usage & uDEFINE)!=0)
      error(21,decl->name);     /* symbol already defined */
    /* if this variable is never used (which can be detected only in the
     * second stage), shut off code generation
     */
    cidx=0;             /* only to avoid a compiler warning */
    if (sc_status==statWRITE && sym!=NULL && (sym->usage & (uREAD | uWRITTEN))==0) {
      sc_status=statSKIP;
      cidx=code_idx;
      #if !defined NDEBUG
        glbdecl=glb_declared;
      #endif
    } /* if */
    begdseg();          /* real (initialized) data in data segment */
    assert(litidx==0 || !cc_ok());  /* literal queue should be empty */
    assert(litidx==0 || !cc_ok());  /* literal queue should be empty (again) */
    if (type->ident == iREFARRAY) {
      // Dynamc array in global scope.
      assert(type->is_new);
      error(162);
    }
    initials3(decl);
    if (type->tag == pc_tag_string && type->numdim == 1 && !type->dim[type->numdim - 1]) {
      slength = glbstringread;
    }
    assert(type->size>=litidx || !cc_ok());
    if (type->numdim == 1)
      type->dim[0] = (int)type->size;
    address=sizeof(cell)*glb_declared;
    glb_incr=(int)type->size;
    if (type->size != CELL_MAX && address==sizeof(cell)*glb_declared) {
      dumplits();       /* dump the literal queue */
      dumpzero((int)(type->size)-litidx);
    } /* if */
    litidx=0;
    if (sym==NULL) {    /* define only if not yet defined */
      sym=addvariable3(decl,address,sGLOBAL,slength);
    } else {            /* if declared but not yet defined, adjust the variable's address */
      sym->setAddr(address);
      sym->codeaddr=code_idx;
      sym->usage|=uDEFINE;
    } /* if */
    assert(sym!=NULL);
    if (ispublic)
      sym->usage|=uPUBLIC|uREAD;
    if (decl->type.usage & uCONST)
      sym->usage|=uCONST;
    if (fstock)
      sym->usage|=uSTOCK;
    if (fstatic)
      sym->fnumber=filenum;
    if (sc_status==statSKIP) {
      sc_status=statWRITE;
      code_idx=cidx;
      assert(glb_declared==glbdecl);
    } else {
      glb_declared+=glb_incr;   /* add total number of cells (if added to the end) */
    } /* if */

    if (!matchtoken(','))
      break;

    if (decl->type.is_new)
      reparse_new_decl(decl, DECLFLAG_VARIABLE|DECLFLAG_ENUMROOT);
    else
      reparse_old_decl(decl, DECLFLAG_VARIABLE|DECLFLAG_ENUMROOT);
  };
  needtoken(tTERM);    /* if not comma, must be semicolumn */
}

static bool parse_local_array_initializer(typeinfo_t *type, int *curlit, int *slength)
{
  *curlit = litidx; /* save current index in the literal table */
  if (type->numdim && !type->dim[type->numdim-1])
    type->size = 0;
  initials(type->ident,type->tag,&type->size,type->dim,type->numdim,type->enumroot);
  if (type->tag == pc_tag_string && type->numdim == 1 && !type->dim[type->numdim - 1])
    *slength = glbstringread;
  if (type->size == 0)
    return false;
  if (type->numdim == 1)
    type->dim[0] = type->size;
  return true;
}

/*  declloc     - declare local symbols
 *
 *  Declare local (automatic) variables. Since these variables are relative
 *  to the STACK, there is no switch to the DATA segment. These variables
 *  cannot be initialized either.
 *
 *  global references: declared   (altered)
 *                     funcstatus (referred to only)
 */
static void declloc(int tokid)
{
  symbol *sym;
  value lval = {0};
  int cur_lit=0;
  int staging_start;
  int slength = 0;
  int fstatic = (tokid == tSTATIC);
  declinfo_t decl;

  int declflags = DECLFLAG_VARIABLE | DECLFLAG_ENUMROOT | DECLFLAG_DYNAMIC_ARRAYS;
  if (tokid == tNEW || tokid == tDECL)
    declflags |= DECLFLAG_OLD;
  else if (tokid == tNEWDECL)
    declflags |= DECLFLAG_NEW;

  parse_decl(&decl, declflags);

  for (;;) {
    typeinfo_t *type = &decl.type;

    slength=0;

    if (decl.name[0]==PUBLIC_CHAR)
      error(56, decl.name);             /* local variables cannot be public */

    /* Note: block locals may be named identical to locals at higher
     * compound blocks (as with standard C); so we must check (and add)
     * the "nesting level" of local variables to verify the
     * multi-definition of symbols.
     */
    if ((sym=findloc(decl.name))!=NULL && sym->compound==nestlevel)
      error(21, decl.name);             /* symbol already defined */

    /* Although valid, a local variable whose name is equal to that
     * of a global variable or to that of a local variable at a lower
     * level might indicate a bug.
     */
    if (((sym=findloc(decl.name)) != NULL && sym->compound != nestlevel) ||
        findglb(decl.name) != NULL)
    {
      error(219, decl.name);            /* variable shadows another symbol */
    }

    slength = fix_char_size(&decl);

    if (fstatic && type->ident == iREFARRAY)
      error(165);

    if (type->ident == iARRAY || fstatic) {
      if (!parse_local_array_initializer(type, &cur_lit, &slength))
        return;
    }
    /* reserve memory (on the stack) for the variable */
    if (fstatic) {
      /* write zeros for uninitialized fields */
      while (litidx < cur_lit + type->size)
        litadd(0);
      sym=addvariable2(decl.name,(cur_lit+glb_declared)*sizeof(cell),type->ident,sSTATIC,
                       type->tag,type->dim,type->numdim,type->idxtag,slength);
    } else if (type->ident!=iREFARRAY) {
      declared+=type->size;     /* variables are put on stack, adjust "declared" */
      sym=addvariable2(decl.name,-declared*sizeof(cell),type->ident,sLOCAL,type->tag,
                       type->dim,type->numdim,type->idxtag,slength);
      if (type->ident == iVARIABLE) {
        assert(!staging);
        stgset(TRUE);           /* start stage-buffering */
        assert(stgidx==0);
        staging_start=stgidx;
      } /* if */
      markexpr(sLDECL,decl.name,-declared*sizeof(cell)); /* mark for better optimization */
      modstk(-type->size * sizeof(cell));
      markstack(MEMUSE_STATIC, type->size);
      assert(curfunc!=NULL);
      assert((curfunc->usage & uNATIVE)==0);
      if (curfunc->x.stacksize<declared+1)
        curfunc->x.stacksize=declared+1;  /* +1 for PROC opcode */
    } else if (type->ident == iREFARRAY) {
      // Generate the symbol so we can access its stack address during initialization.
      declared+=1; /* one cell for address */
      sym=addvariable(decl.name,-declared*sizeof(cell),type->ident,sLOCAL,type->tag,type->dim,type->numdim,type->idxtag);

      // If we're new-style, a REFARRAY indicates prefix brackets. We need to
      // be initialized since we don't support fully dynamic arrays yet; i.e.,
      // "int[] x;" doesn't have any sensible semantics. There are two
      // acceptable initialization sequences: "new <type>" and a string
      // literal. In other cases (such as a fixed-array literal), we error.
      //
      // For now, we only implement the string literal initializer.
      if (type->is_new && needtoken('=')) {
        if (type->isCharArray() && !lexpeek(tNEW)) {
          // Error if we're assigning something other than a string literal.
          needtoken(tSTRING);

          // Note: the genarray call pushes the result array into the stack
          // slot of our local variable - we can access |sym| after.
          //
          // push.c      N
          // genarray    1
          // const.pri   DAT + offset
          // load.s.alt  sym->addr()
          // movs        N * sizeof(cell)
          int cells = litidx - cur_lit;
          pushval(cells);
          genarray(1, false);
          ldconst((cur_lit + glb_declared) * sizeof(cell), sPRI);
          copyarray(sym, cells * sizeof(cell));
        } else if (matchtoken(tNEW)) {
          int tag = 0;
          if (parse_new_typename(NULL, &tag)) {
            if (tag != type->tag)
              error(164, pc_typename(tag), pc_typename(type->tag));
          }

          for (int i = 0; i < type->numdim; i++) {
            if (!needtoken('['))
              break;

            value val;
            symbol *child;
            int ident = doexpr2(
              TRUE, FALSE, TRUE, FALSE,
              &type->idxtag[i],
              &child, 0, &val);
            if (i == type->numdim - 1 && type->tag == pc_tag_string)
              stradjust(sPRI);
            pushreg(sPRI);
            
            switch (ident) {
              case iVARIABLE:
              case iEXPRESSION:
              case iARRAYCELL:
              case iCONSTEXPR:
              case iREFERENCE:  
                break;
              default:
                error(29);
                break;
            }

            if (!needtoken(']'))
              break;
          }
          
          genarray(type->numdim, true);
        } else if (lexpeek('{')) {
          // Dynamic array with fixed initializer.
          error(160);

          // Parse just to clear the tokens. First give '=' back.
          lexpush();
          if (!parse_local_array_initializer(type, &cur_lit, &slength))
            return;
        } else {
          // Give the '=' back so we error later.
          lexpush();
        }
      }

      /* genarray() pushes the address onto the stack, so we don't need to call modstk() here! */
      markheap(MEMUSE_DYNAMIC, 0);
      markstack(MEMUSE_STATIC, 1);
      assert(curfunc != NULL && ((curfunc->usage & uNATIVE) == 0));
      if (curfunc->x.stacksize<declared+1)
        curfunc->x.stacksize=declared+1;  /* +1 for PROC opcode */
    } /* if */
    /* now that we have reserved memory for the variable, we can proceed
     * to initialize it */
    assert(sym!=NULL);          /* we declared it, it must be there */
    sym->compound=nestlevel;    /* for multiple declaration/shadowing check */
    if (type->usage & uCONST)
      sym->usage|=uCONST;
    if (!fstatic) {             /* static variables already initialized */
      if (type->ident == iVARIABLE) {
        /* simple variable, also supports initialization */
        int ctag = type->tag;   /* set to "tag" by default */
        int explicit_init=FALSE;/* is the variable explicitly initialized? */
        int cident = type->ident;
        if (matchtoken('=')) {
          if (!autozero)
            error(10);
          cident=doexpr(FALSE,FALSE,FALSE,FALSE,&ctag,NULL,TRUE);
          explicit_init=TRUE;
        } else {
          if (autozero)
            ldconst(0,sPRI);      /* uninitialized variable, set to zero */
        } /* if */
        if (autozero) {
          /* now try to save the value (still in PRI) in the variable */
          lval.sym=sym;
          lval.ident=iVARIABLE;
          lval.constval=0;
          lval.tag=type->tag;
          check_userop(NULL,ctag,lval.tag,2,NULL,&ctag);
          store(&lval);
          markexpr(sEXPR,NULL,0); /* full expression ends after the store */
        }
        assert(staging);        /* end staging phase (optimize expression) */
        stgout(staging_start);
        stgset(FALSE);
        if (!matchtag_string(cident, ctag))
          matchtag(type->tag,ctag,TRUE);
        /* if the variable was not explicitly initialized, reset the
         * "uWRITTEN" flag that store() set */
        if (!explicit_init)
          sym->usage &= ~uWRITTEN;
      } else if (type->ident != iREFARRAY) {
        /* an array */
        assert(cur_lit>=0 && cur_lit<=litidx && litidx<=litmax);
        assert(type->size > 0 && type->size >= sym->dim.array.length);
        assert(type->numdim > 1 || type->size == sym->dim.array.length);
        if (autozero) {
          /* final literal values that are zero make no sense to put in the literal
           * pool, because values get zero-initialized anyway; we check for this,
           * because users often explicitly initialize strings to ""
           */
          while (litidx>cur_lit && litq[litidx-1]==0)
            litidx--;
          /* if the array is not completely filled, set all values to zero first */
          if (litidx - cur_lit < type->size && (ucell)type->size < CELL_MAX)
            fillarray(sym, type->size * sizeof(cell), 0);
        }
        if (cur_lit<litidx) {
          /* check whether the complete array is set to a single value; if
           * it is, more compact code can be generated */
          cell first=litq[cur_lit];
          int i;
          for (i=cur_lit; i<litidx && litq[i]==first; i++)
            /* nothing */;
          if (i==litidx) {
            /* all values are the same */
            fillarray(sym,(litidx-cur_lit)*sizeof(cell),first);
            litidx=cur_lit;     /* reset literal table */
          } else {
            /* copy the literals to the array */
            ldconst((cur_lit+glb_declared)*sizeof(cell),sPRI);
            copyarray(sym,(litidx-cur_lit)*sizeof(cell));
          } /* if */
        } /* if */
      } /* if */
    } /* if */

    if (!matchtoken(','))
      break;

    if (decl.type.is_new)
      reparse_new_decl(&decl, declflags);
    else
      reparse_old_decl(&decl, declflags);
  }
  needtoken(tTERM);    /* if not comma, must be semicolumn */
  return;
}

/* this function returns the maximum value for a cell in case of an error
 * (invalid dimension).
 */
static cell calc_arraysize(int dim[],int numdim,int cur)
{
  cell subsize;
  ucell newsize;

  /* the return value is in cells, not bytes */
  assert(cur>=0 && cur<=numdim);
  if (cur==numdim)
    return 0;
  subsize=calc_arraysize(dim,numdim,cur+1);
  newsize=dim[cur]+dim[cur]*subsize;
  if ((ucell)subsize>=CELL_MAX || newsize>=CELL_MAX || newsize*sizeof(cell)>=CELL_MAX)
    return CELL_MAX;
  return newsize;
}

static cell gen_indirection_vecs(array_info_t *ar, int dim, cell cur_offs)
{
  int i;
  cell write_offs = cur_offs;
  cell *data_offs = ar->data_offs;

  cur_offs += ar->dim_list[dim];

  /**
   * Dimension n-x where x > 2 will have sub-vectors.  
   * Otherwise, we just need to reference the data section.
   */
  if (ar->dim_count > 2 && dim < ar->dim_count - 2)
  {
    /**
     * For each index at this dimension, write offstes to our sub-vectors.
     * After we write one sub-vector, we generate its sub-vectors recursively.
     * At the end, we're given the next offset we can use.
     */
    for (i = 0; i < ar->dim_list[dim]; i++)
    {
      ar->base[write_offs] = (cur_offs - write_offs) * sizeof(cell);
      write_offs++;
      ar->cur_dims[dim] = i;
      cur_offs = gen_indirection_vecs(ar, dim + 1, cur_offs);
    }
  } else if (ar->dim_count > 1) {
    /**
     * In this section, there are no sub-vectors, we need to write offsets 
     * to the data.  This is separate so the data stays in one big chunk.
     * The data offset will increment by the size of the last dimension, 
     * because that is where the data is finally computed as.  But the last 
     * dimension can be of variable size, so we have to detect that.
     */
    if (ar->dim_list[dim + 1] == 0)
    {
      int vec_start = 0;

      /**
       * Using the precalculated offsets, compute an index into the last 
       * dimension array.
       */
      for (i = 0; i < dim; i++)
      {
        vec_start += ar->cur_dims[i] * ar->dim_offs_precalc[i];
      }

      /**
       * Now, vec_start points to a vector of last dimension offsets for 
       * the preceding dimension combination(s).
       * I.e. (1,2,i,j) in [3][4][5][] will be:
       *  j = 1*(4*5) + 2*(5) + i, and the parenthetical expressions are 
       * precalculated for us so we can easily generalize here.
       */
      for (i = 0; i < ar->dim_list[dim]; i++)
      {
        ar->base[write_offs] = (*data_offs - write_offs) * sizeof(cell);
        write_offs++;
        *data_offs = *data_offs + ar->lastdim_list[vec_start + i];
      }
    } else {
      /**
       * The last dimension size is constant.  There's no extra work to 
       * compute the last dimension size.
       */
      for (i = 0; i < ar->dim_list[dim]; i++)
      {
        ar->base[write_offs] = (*data_offs - write_offs) * sizeof(cell);
        write_offs++;
        *data_offs = *data_offs + ar->dim_list[dim + 1];
      }
    }
  }

  return cur_offs;
}

static cell calc_indirection(const int dim_list[], int dim_count, int dim)
{
  cell size = dim_list[dim];

  if (dim < dim_count - 2)
  {
    size += dim_list[dim] * calc_indirection(dim_list, dim_count, dim + 1);
  }

  return size;
}

static void adjust_indirectiontables(int dim[],int numdim,int cur,cell increment,
                                     int startlit,constvalue *lastdim,int *skipdim)
{
  /* Find how many cells the indirection table will be */
  cell tbl_size;
  int *dyn_list = NULL;
  int cur_dims[sDIMEN_MAX];
  cell dim_offset_precalc[sDIMEN_MAX];
  array_info_t ar;

  if (numdim == 1)
  {
    return;
  }

  tbl_size = calc_indirection(dim, numdim, 0);
  memset(cur_dims, 0, sizeof(cur_dims));

  /**
   * Flatten the last dimension array list -- this makes 
   * things MUCH easier in the indirection calculator.
   */
  if (lastdim)
  {
    int i;
    constvalue *ld = lastdim->next;

    /* Get the total number of last dimensions. */
    for (i = 0; ld != NULL; i++, ld = ld->next)
    {
      /* Nothing */
    }
    /* Store them in an array instead of a linked list. */
    dyn_list = (int *)malloc(sizeof(int) * i);
    for (i = 0, ld = lastdim->next;
       ld != NULL;
       i++, ld = ld->next)
    {
      dyn_list[i] = ld->value;
    }

    /**
     * Pre-calculate all of the offsets.  This speeds up and simplifies 
     * the indirection process.  For example, if we have an array like:
     * [a][b][c][d][], and given (A,B,C), we want to find the size of 
     * the last dimension [A][B][C][i], we must do:
     *
     * list[A*(b*c*d) + B*(c*d) + C*(d) + i]
     *
     * Generalizing this algorithm in the indirection process is expensive, 
     * so we lessen the need for nested loops by pre-computing the parts:
     * (b*c*d), (c*d), and (d).
     *
     * In other words, finding the offset to dimension N at index I is 
     * I * (S[N+1] * S[N+2] ... S[N+n-1]) where S[] is the size of dimension
     * function, and n is the index of the last dimension.
     */
    for (i = 0; i < numdim - 1; i++)
    {
      int j;

      dim_offset_precalc[i] = 1;
      for (j = i + 1; j < numdim - 1; j++)
      {
        dim_offset_precalc[i] *= dim[j];
      }
    }

    ar.dim_offs_precalc = dim_offset_precalc;
    ar.lastdim_list = dyn_list;
  } else {
    ar.dim_offs_precalc = NULL;
    ar.lastdim_list = NULL;
  }

  ar.base = &litq[startlit];
  ar.data_offs = &tbl_size;
  ar.dim_list = dim;
  ar.dim_count = numdim;
  ar.cur_dims = cur_dims;

  gen_indirection_vecs(&ar, 0, 0);

  free(dyn_list);
}

/*  initials
 *
 *  Initialize global objects and local arrays.
 *    size==array cells (count), if 0 on input, the routine counts the number of elements
 *    tag==required tagname id (not the returned tag)
 *
 *  Global references: litidx (altered)
 */
static void initials2(int ident,int tag,cell *size,int dim[],int numdim,
                     constvalue *enumroot, int eq_match_override, int curlit_override)
{
  int ctag;
  cell tablesize;
  int curlit=(curlit_override == -1) ? litidx : curlit_override;
  int err=0;

  if (eq_match_override == -1) {
    eq_match_override = matchtoken('=');
  }

  if (numdim > 2) {
    int d, hasEmpty = 0;
    for (d = 0; d < numdim; d++) {
      if (dim[d] == 0)
        hasEmpty++;
    }
    /* Work around ambug 4977 where indirection vectors are computed wrong. */
    if (hasEmpty && hasEmpty < numdim-1 && dim[numdim-1]) {
      error(101);
      /* This will assert with something like [2][][256] from a separate bug.
       * To prevent this assert, automatically wipe the rest of the dims.
       */
      for (d = 0; d < numdim - 1; d++)
        dim[d] = 0;
    }
  }

  if (!eq_match_override) {
    assert(ident!=iARRAY || numdim>0);
    if (ident==iARRAY && dim[numdim-1]==0) {
      /* declared as "myvar[];" which is senseless (note: this *does* make
       * sense in the case of a iREFARRAY, which is a function parameter)
       */
      error(9);         /* array has zero length -> invalid size */
    } /* if */
    if (ident==iARRAY) {
      assert(numdim>0 && numdim<=sDIMEN_MAX);
      *size=calc_arraysize(dim,numdim,0);
      if (*size==(cell)CELL_MAX) {
        error(9);       /* array is too big -> invalid size */
        return;
      } /* if */
      /* first reserve space for the indirection vectors of the array, then
       * adjust it to contain the proper values
       * (do not use dumpzero(), as it bypasses the literal queue)
       */
      for (tablesize=calc_arraysize(dim,numdim-1,0); tablesize>0; tablesize--)
        litadd(0);
      if (dim[numdim-1]!=0)     /* error 9 has already been given */
        adjust_indirectiontables(dim,numdim,0,0,curlit,NULL,NULL);
    } /* if */
    return;
  } /* if */

  if (ident==iVARIABLE) {
    assert(*size==1);
    init(ident,&ctag,NULL);
    matchtag(tag,ctag,TRUE);
  } else {
    assert(numdim>0);
    if (numdim==1) {
      *size=initvector(ident,tag,dim[0],FALSE,enumroot,NULL);
    } else {
      int errorfound=FALSE;
      int counteddim[sDIMEN_MAX];
      int idx;
      constvalue lastdim={NULL,"",0,0};     /* sizes of the final dimension */
      int skipdim=0;

      if (dim[numdim-1]!=0)
        *size=calc_arraysize(dim,numdim,0); /* calc. full size, if known */
      /* already reserve space for the indirection tables (for an array with
       * known dimensions)
       * (do not use dumpzero(), as it bypasses the literal queue)
       */
      for (tablesize=calc_arraysize(dim,numdim-1,0); tablesize>0; tablesize--)
        litadd(0);
      /* now initialize the sub-arrays */
      memset(counteddim,0,sizeof counteddim);
      initarray(ident,tag,dim,numdim,0,curlit,counteddim,&lastdim,enumroot,&errorfound);
      /* check the specified array dimensions with the initializer counts */
      for (idx=0; idx<numdim-1; idx++) {
        if (dim[idx]==0) {
          dim[idx]=counteddim[idx];
        } else if (counteddim[idx]<dim[idx]) {
          error(52);            /* array is not fully initialized */
          err++;
        } else if (counteddim[idx]>dim[idx]) {
          error(18);            /* initialization data exceeds declared size */
          err++;
        } /* if */
      } /* for */
      if (numdim>1 && dim[numdim-1]==0 && !errorfound && err==0) {
        /* also look whether, by any chance, all "counted" final dimensions are
         *  the same value; if so, we can store this
         */
        constvalue *ld=lastdim.next;
        int count=0,match,total,d;
        for (ld=lastdim.next; ld!=NULL; ld=ld->next) {
          assert(strtol(ld->name,NULL,16)==count % dim[numdim-2]);  /* index is stored in the name, it should match the sequence */
          if (count==0)
            match=(int)ld->value;
          else if (match!=ld->value)
            break;
          count++;
        }
        total=dim[numdim-2];
        for (d=numdim-3; d>=0; d--)
          total*=dim[d];
        if (count>0 && count==total)
          dim[numdim-1]=match;
      } /* if */
      /* after all arrays have been initalized, we know the (major) dimensions
       * of the array and we can properly adjust the indirection vectors
       */
      if (err==0)
        adjust_indirectiontables(dim,numdim,0,0,curlit,&lastdim,&skipdim);
      delete_consttable(&lastdim);  /* clear list of minor dimension sizes */
    } /* if */
  } /* if */

  if (*size==0)
    *size=litidx-curlit;        /* number of elements defined */
}

static void initials(int ident,int tag,cell *size,int dim[],int numdim,
                     constvalue *enumroot)
{
  initials2(ident, tag, size, dim, numdim, enumroot, -1, -1);
}

static void initials3(declinfo_t *decl)
{
  typeinfo_t *type = &decl->type;
  initials(type->ident, type->tag, &type->size, type->dim, type->numdim, type->enumroot);
}

static cell initarray(int ident,int tag,int dim[],int numdim,int cur,
                      int startlit,int counteddim[],constvalue *lastdim,
                      constvalue *enumroot,int *errorfound)
{
  cell dsize,totalsize;
  int idx,abortparse;

  assert(cur>=0 && cur<numdim);
  assert(startlit>=0);
  assert(cur+2<=numdim);/* there must be 2 dimensions or more to do */
  assert(errorfound!=NULL && *errorfound==FALSE);
  totalsize=0;
  needtoken('{');
  for (idx=0,abortparse=FALSE; !abortparse; idx++) {

    /* In case the major dimension is zero, we need to store the offset
     * to the newly detected sub-array into the indirection table; i.e.
     * this table needs to be expanded and updated.
     * In the current design, the indirection vectors for a multi-dimensional
     * array are adjusted after parsing all initializers. Hence, it is only
     * necessary at this point to reserve space for an extra cell in the
     * indirection vector.
     */
    if (dim[cur]==0) {
      litinsert(0,startlit);
    } else if (idx>=dim[cur]) {
      error(18);            /* initialization data exceeds array size */
      *errorfound=TRUE;
      break;
    } /* if */
    if (cur+2<numdim) {
      dsize=initarray(ident,tag,dim,numdim,cur+1,startlit,counteddim,
                      lastdim,enumroot,errorfound);
    } else {
      dsize=initvector(ident,tag,dim[cur+1],TRUE,enumroot,errorfound);
      /* The final dimension may be variable length. We need to keep the
       * lengths of the final dimensions in order to set the indirection
       * vectors for the next-to-last dimension.
       */
      append_constval(lastdim,itoh(idx),dsize,0);
    } /* if */
    totalsize+=dsize;
    if (*errorfound || !matchtoken(','))
      abortparse=TRUE;
    {
      // We need this since, lex() could add a string to the literal queue,
      // which totally messes up initvector's state tracking. What a mess.
      AutoDisableLiteralQueue disable;
      if (lexpeek('}'))
        abortparse=TRUE;
    }
  } /* for */
  needtoken('}');
  assert(counteddim!=NULL);
  if (counteddim[cur]>0) {
    if (idx<counteddim[cur]) {
      error(52);                /* array is not fully initialized */
      *errorfound=TRUE;
    } else if (idx>counteddim[cur]) {
      error(18);                /* initialization data exceeds declared size */
      *errorfound=TRUE;
    }
  } /* if */
  counteddim[cur]=idx;

  return totalsize+dim[cur];    /* size of sub-arrays + indirection vector */
}

/*  initvector
 *  Initialize a single dimensional array
 */
static cell initvector(int ident,int tag,cell size,int fillzero,
                       constvalue *enumroot,int *errorfound)
{
  cell prev1=0,prev2=0;
  int ellips=FALSE;
  int curlit=litidx;
  int rtag,ctag;

  assert(ident==iARRAY || ident==iREFARRAY);
  if (matchtoken('{')) {
    constvalue *enumfield=(enumroot!=NULL) ? enumroot->next : NULL;
    do {
      int fieldlit=litidx;
      int matchbrace,i;
      if (matchtoken('}')) {    /* to allow for trailing ',' after the initialization */
        lexpush();
        break;
      } /* if */
      if ((ellips=matchtoken(tELLIPS))!=0)
        break;
      /* for enumeration fields, allow another level of braces ("{...}") */
      matchbrace=0;             /* preset */
      ellips=0;
      if (enumfield!=NULL)
        matchbrace=matchtoken('{');
      for ( ;; ) {
        prev2=prev1;
        prev1=init(ident,&ctag,errorfound);
        if (!matchbrace)
          break;
        if ((ellips=matchtoken(tELLIPS))!=0)
          break;
        if (!matchtoken(',')) {
          needtoken('}');
          break;
        } /* for */
      } /* for */
      /* if this array is based on an enumeration, fill the "field" up with
       * zeros, and toggle the tag
       */
      if (enumroot!=NULL && enumfield==NULL) {
        error(227);             /* more initializers than enum fields */
        if (errorfound!=NULL)
          *errorfound=TRUE;
      }
      rtag=tag;                 /* preset, may be overridden by enum field tag */
      if (enumfield!=NULL) {
        cell step;
        int cmptag=enumfield->index;
        symbol *symfield=findconst(enumfield->name,&cmptag);
        if (cmptag>1) {
          error(91,enumfield->name); /* ambiguous constant, needs tag override */
          if (errorfound!=NULL)
            *errorfound=TRUE;
        }
        assert(symfield!=NULL);
        assert(fieldlit<litidx);
        if (litidx-fieldlit>symfield->dim.array.length) {
          error(228);           /* length of initializer exceeds size of the enum field */
          if (errorfound!=NULL)
            *errorfound=TRUE;
        }
        if (ellips) {
          step=prev1-prev2;
        } else {
          step=0;
          prev1=0;
        } /* if */
        for (i=litidx-fieldlit; i<symfield->dim.array.length; i++) {
          prev1+=step;
          litadd(prev1);
        } /* for */
        rtag=symfield->x.tags.index;  /* set the expected tag to the index tag */
        enumfield=enumfield->next;
      } /* if */
      matchtag(rtag,ctag,TRUE);
    } while (matchtoken(',')); /* do */
    needtoken('}');
  } else {
    if (!lexpeek('}'))
    {
      init(ident,&ctag,errorfound);
      matchtag(tag,ctag,TRUE);
    }
  } /* if */
  /* fill up the literal queue with a series */
  if (ellips) {
    cell step=((litidx-curlit)==1) ? (cell)0 : prev1-prev2;
    if (size==0 || (litidx-curlit)==0) {
      error(41);                /* invalid ellipsis, array size unknown */
      if (errorfound!=NULL)
        *errorfound=TRUE;
    } else if ((litidx-curlit)==(int)size) {
      error(18);                /* initialization data exceeds declared size */
      if (errorfound!=NULL)
        *errorfound=TRUE;
    }
    while ((litidx-curlit)<(int)size) {
      prev1+=step;
      litadd(prev1);
    } /* while */
  } /* if */
  if (fillzero && size>0) {
    while ((litidx-curlit)<(int)size)
      litadd(0);
  } /* if */
  if (size==0) {
    size=litidx-curlit;                 /* number of elements defined */
  } else if (litidx-curlit>(int)size) { /* e.g. "myvar[3]={1,2,3,4};" */
    error(18);                  /* initialization data exceeds declared size */
    if (errorfound!=NULL)
      *errorfound=TRUE;
    litidx=(int)size+curlit;    /* avoid overflow in memory moves */
  } /* if */
  return size;
}

/*  init
 *
 *  Evaluate one initializer.
 */
static cell init(int ident,int *tag,int *errorfound)
{
  cell i = 0;

  if (matchtoken(tSTRING)){
    /* lex() automatically stores strings in the literal table (and
     * increases "litidx") */
    if (ident==iVARIABLE) {
      error(6);         /* must be assigned to an array */
      if (errorfound!=NULL)
        *errorfound=TRUE;
      litidx=1;         /* reset literal queue */
    } /* if */
    *tag=pc_tag_string;
  } else if (exprconst(&i,tag,NULL)){
    litadd(i);          /* store expression result in literal table */
  } else {
    if (errorfound!=NULL)
      *errorfound=TRUE;
  } /* if */
  return i;
}

/*  needsub
 *
 *  Get required array size
 */
static cell needsub(int *tag,constvalue **enumroot)
{
  cell val;
  symbol *sym;

  assert(tag!=NULL);
  *tag=0;
  if (enumroot!=NULL)
    *enumroot=NULL;         /* preset */
  if (matchtoken(']'))      /* we have already seen "[" */
    return 0;               /* zero size (like "char msg[]") */

  exprconst(&val,tag,&sym); /* get value (must be constant expression) */
  if (val<0) {
    error(9);               /* negative array size is invalid; assumed zero */
    val=0;
  } /* if */
  needtoken(']');

  if (enumroot!=NULL) {
    /* get the field list for an enumeration */
    assert(*enumroot==NULL);/* should have been preset */
    assert(sym==NULL || sym->ident==iCONSTEXPR);
    if (sym!=NULL && (sym->usage & uENUMROOT)==uENUMROOT) {
      assert(sym->dim.enumlist!=NULL);
      *enumroot=sym->dim.enumlist;
    } /* if */
  } /* if */

  return val;               /* return array size */
}

/*  decl_const  - declare a single constant
 *
 */
static void decl_const(int vclass)
{
  char constname[sNAMEMAX+1];
  cell val;
  token_t tok;
  int exprtag;
  int symbolline;

  do {
    int orgfline;

    // Since spcomp is terrible, it's hard to use parse_decl() here - there
    // are all sorts of restrictions on const. We just implement some quick
    // detection instead.
    int tag = 0;
    switch (lextok(&tok)) {
      case tINT:
      case tOBJECT:
      case tCHAR:
        tag = parse_new_typename(&tok);
        break;
      case tLABEL:
        tag = pc_addtag(tok.str);
        break;
      case tSYMBOL:
        // See if we can peek ahead another symbol.
        if (lexpeek(tSYMBOL)) {
          // This is a new-style declaration.
          tag = parse_new_typename(&tok);
        } else {
          // Otherwise, we got "const X ..." so the tag is int. Give the
          // symbol back to the lexer so we get it as the name.
          lexpush();
        }
        break;
      default:
        error(122);
        break;
    }

    if (expecttoken(tSYMBOL, &tok))
      strcpy(constname, tok.str);
    else
      strcpy(constname, "__unknown__");

    symbolline=fline;                   /* save line where symbol was found */
    needtoken('=');
    exprconst(&val,&exprtag,NULL);      /* get value */

    /* add_constant() checks for duplicate definitions */
    /* temporarily reset the line number to where the symbol was defined */
    orgfline=fline;
    fline=symbolline;
    matchtag(tag,exprtag,FALSE);
    fline=orgfline;

    add_constant(constname,val,vclass,tag);
  } while (matchtoken(',')); /* enddo */   /* more? */
  needtoken(tTERM);
}

static void check_struct_name(const char *name)
{
  LayoutSpec spec = deduce_layout_spec_by_name(name);
  if (!can_redef_layout_spec(spec, Layout_PawnStruct))
    error(110, name, layout_spec_name(spec));
  if (!isupper(*name))
    error(109, "struct");
}

/*
 * declstruct - declare a struct type
 */
static void declstruct(void)
{
  cell val;
  char *str;
  int tok;
  pstruct_t *pstruct;

  /* get the explicit tag (required!) */
  tok = lex(&val,&str);
  if (tok != tSYMBOL) {
    error(93);
  } else {
    check_struct_name(str);
  }

  pstruct = pstructs_add(str);

  pc_addtag_flags(pstruct->name, STRUCTTAG|FIXEDTAG);

  needtoken('{');
  do {
    if (matchtoken('}')) {
      /* Quick exit */
      lexpush();
      break;
    }

    declinfo_t decl;
    memset(&decl, 0, sizeof(decl));

    decl.type.ident = iVARIABLE;
    decl.type.size = 1;
    if (!needtoken(tPUBLIC) || !parse_new_decl(&decl, NULL, DECLFLAG_FIELD)) {
      // skip the rest of the line.
      lexclr(TRUE);
      break;
    }

    structarg_t arg;
    memset(&arg, 0, sizeof(arg));

    arg.tag = decl.type.tag;
    arg.dimcount = decl.type.numdim;
    memcpy(arg.dims, decl.type.dim, sizeof(int) * arg.dimcount);
    strcpy(arg.name, decl.name);
    arg.fconst = !!(decl.type.usage & uCONST);
    arg.ident = decl.type.ident;
    if (arg.ident == iARRAY)
      arg.ident = iREFARRAY;

    if (pstructs_addarg(pstruct, &arg) == NULL)
      error(103, arg.name, layout_spec_name(Layout_PawnStruct));

    require_newline(TRUE);
  } while (!lexpeek('}'));
  needtoken('}');
  matchtoken(';');  /* eat up optional semicolon */
}

// Consumes a line, returns FALSE if EOF hit.
static int consume_line()
{
  int val;
  char *str;

  // First check for EOF.
  if (lex(&val, &str) == 0)
    return FALSE;
  lexpush();

  while (!matchtoken(tTERM)) {
    // Check for EOF.
    if (lex(&val, &str) == 0)
      return FALSE;
  }

  return TRUE;
}

static int parse_new_typename(const token_t *tok)
{
  token_t tmp;

  if (!tok) {
    lextok(&tmp);
    tok = &tmp;
  }

  switch (tok->id) {
    case tINT:
      return 0;
    case tCHAR:
      return pc_tag_string;
    case tVOID:
      return pc_tag_void;
    case tOBJECT:
      return pc_tag_object;
    case tLABEL:
      error(120);
      return pc_addtag(tok->str);
    case tSYMBOL:
    {
      if (strcmp(tok->str, "float") == 0)
        return sc_rationaltag;
      if (strcmp(tok->str, "bool") == 0)
        return pc_tag_bool;
      int tag = pc_findtag(tok->str);
      if (tag == sc_rationaltag) {
        error(98, "Float", "float");
      } else if (tag == pc_tag_string) {
        error(98, "String", "char");
      } else if (tag == 0) {
        error(98, "_", "int");
      } else if (tag == -1) {
        error(139, tok->str);
        tag = 0;
      } else if (tag != pc_anytag) {
        // Perform some basic filters so we can start narrowing down what can
        // be used as a type.
        if (!(tag & TAGTYPEMASK))
          error(139, tok->str);
      }
      return tag;
    }
  }

  error(122);
  return -1;
}

bool parse_new_typename(const token_t *tok, int *tagp)
{
  int tag = parse_new_typename(tok);
  if (tag >= 0)
    *tagp = tag;
  else
    *tagp = 0;
  return true;
}

static int parse_new_typeexpr(typeinfo_t *type, const token_t *first, int flags)
{
  token_t tok;

  if (first)
    tok = *first;
  else
    lextok(&tok);

  if (tok.id == tCONST) {
    if (type->usage & uCONST)
      error(138);
    type->usage |= uCONST;
    lextok(&tok);
  }

  if (!parse_new_typename(&tok, &type->tag))
    return FALSE;

  // Note: we could have already filled in the prefix array bits, so we check
  // that ident != iARRAY before looking for an open bracket.
  if (type->ident != iARRAY && matchtoken('[')) {
    do {
      if (type->numdim == sDIMEN_MAX) {
        error(53);
        break;
      }
      type->dim[type->numdim++] = 0;
      if (!matchtoken(']')) {
        error(140);
        return FALSE;
      }
    } while (matchtoken('['));
    type->ident = iREFARRAY;
    type->size = 0;
  }

  if (flags & DECLFLAG_ARGUMENT) {
    if (matchtoken('&')) {
      if (type->ident == iARRAY) {
        error(137);
        return FALSE;
      }
      type->ident = iREFERENCE;
    }
  }

  return TRUE;
}

static void parse_old_array_dims(declinfo_t *decl, int flags)
{
  typeinfo_t *type = &decl->type;
  constvalue **enumrootp;

  // Illegal declaration (we'll have a name since ref requires decl).
  if (type->ident == iREFERENCE)
    error(67, decl->name);

  if (flags & DECLFLAG_ENUMROOT)
    enumrootp = &type->enumroot;
  else
    enumrootp = NULL;

  if (flags & DECLFLAG_DYNAMIC_ARRAYS) {
    // This is a huge hack for declloc() - we'll generate the array code right
    // into the staging buffer if needed.
    cell staging_ptr;
    int staging_index;

    int was_staging = staging;
    if (!was_staging)
      stgset(TRUE);
    stgget(&staging_index, &staging_ptr);

    type->size = 0;

    do {
      if (type->numdim == sDIMEN_MAX) {
        error(53);
        return;
      }

      if (type->numdim > 0) {
        // Push the last dimension size, which is in PRI.
        pushreg(sPRI);
      }

      if (matchtoken(']')) {
        ldconst(0, sPRI);
        type->idxtag[type->numdim] = 0;
        type->dim[type->numdim] = 0;
        type->numdim++;
        continue;
      }

      value val;
      symbol *sym;
      int ident = doexpr2(
        TRUE, FALSE, FALSE, FALSE,
        &type->idxtag[type->numdim],
        &sym, 0, &val
      );

      if (ident == iVARIABLE || ident == iEXPRESSION || ident == iARRAYCELL || ident == iREFERENCE) {
        type->size = -1;
        type->dim[type->numdim] = 0;
      } else if (ident == iCONSTEXPR) {
        if (val.constval > 0) {
          if (type->size != -1)
            type->size = val.constval;
          type->dim[type->numdim] = val.constval;
        } else {
          error(9);
        }
        if (sym && sym->usage & uENUMROOT)
          type->enumroot = sym->dim.enumlist;
        type->idxtag[type->numdim] = sym ? sym->tag : 0;
      } else {
        error(29);
      }

      type->numdim++;
      needtoken(']');
    } while (matchtoken('['));

    if (type->size >= 0) {
      // Everything was constant. Drop the emitted assembly.
      type->ident = iARRAY;
      stgdel(staging_index, staging_ptr);
    } else {
      if (type->tag == pc_tag_string)
        stradjust(sPRI);
      pushreg(sPRI);
      genarray(type->numdim, autozero);
      type->ident = iREFARRAY;
      type->size = 0;
      if (type->is_new) {
        // Fixed array with dynamic size.
        error(161, pc_typename(type->tag));
      }
    }

    stgout(staging_index);
    if (!was_staging)
      stgset(FALSE);
  } else {
    do {
      if (type->numdim == sDIMEN_MAX) {
        error(53);
        return;
      }

      type->size = needsub(&type->idxtag[type->numdim], enumrootp);
      if (type->size > INT_MAX)
        error(FATAL_ERROR_INT_OVERFLOW);

      type->dim[type->numdim++] = type->size;
    } while (matchtoken('['));

    type->ident = iARRAY;
  }

  decl->type.has_postdims = TRUE;
}

static int parse_old_decl(declinfo_t *decl, int flags)
{
  token_t tok;
  typeinfo_t *type = &decl->type;

  if (matchtoken(tCONST)) {
    if (type->usage & uCONST)
      error(138);
    type->usage |= uCONST;
  }

  int tags[MAXTAGS], numtags = 0;
  if (flags & DECLFLAG_ARGUMENT) {
    if (matchtoken('&'))
      type->ident = iREFERENCE;

    // grammar for multitags is:
    //   multi-tag ::= '{' (symbol (',' symbol)*)? '}' ':'
    if (matchtoken('{')) {
      while (numtags < MAXTAGS) {
        int tag = 0;

        if (!matchtoken('_')) {
          // If we don't get the magic tag '_', then we should have a symbol.
          if (expecttoken(tSYMBOL, &tok))
            tag = pc_addtag(tok.str);
        }
        tags[numtags++] = tag;

        if (matchtoken('}'))
          break;
        needtoken(',');
      }
      needtoken(':');
    }
    if (numtags > 1)
      error(158);
  }
  
  if (numtags == 0) {
    if (matchtoken2(tLABEL, &tok))
      tags[numtags++] = pc_addtag(tok.str);
    else
      tags[numtags++] = 0;
  }

  // All finished with tag stuff.
  type->tag = tags[0];

  // Look for varargs and end early.
  if (matchtoken(tELLIPS)) {
    type->ident = iVARARGS;
    return TRUE;
  }

  if (flags & DECLMASK_NAMED_DECL) {
    if ((flags & DECLFLAG_MAYBE_FUNCTION) && matchtoken(tOPERATOR)) {
      decl->opertok = operatorname(decl->name);
      if (decl->opertok == 0)
        strcpy(decl->name, "__unknown__");
    } else {
      if (!lexpeek(tSYMBOL)) {
        extern const char *sc_tokens[];
        switch (lextok(&tok)) {
          case tOBJECT:
          case tCHAR:
          case tVOID:
          case tINT:
            if (lexpeek(tSYMBOL)) {
              error(143);
            } else {
              error(157, sc_tokens[tok.id - tFIRST]);
              strcpy(decl->name, sc_tokens[tok.id - tFIRST]);
            }
            break;
          default:
            lexpush();
            break;
        }
      }
      if (expecttoken(tSYMBOL, &tok))
        strcpy(decl->name, tok.str);
      else if (decl->name[0] == '\0')
        strcpy(decl->name, "__unknown__");
    }
  }

  if ((flags & DECLMASK_NAMED_DECL) && !decl->opertok) {
    if (matchtoken('['))
      parse_old_array_dims(decl, flags);
  }

  if (sc_require_newdecls)
    error(147);

  return TRUE;
}

static int reparse_old_decl(declinfo_t *decl, int flags)
{
  int usage = decl->type.usage & uCONST;

  memset(decl, 0, sizeof(*decl));
  decl->type.ident = iVARIABLE;
  decl->type.size = 1;
  decl->type.usage |= usage;

  return parse_old_decl(decl, flags);
}

static int parse_new_decl(declinfo_t *decl, const token_t *first, int flags)
{
  token_t tok;

  if (!parse_new_typeexpr(&decl->type, first, flags))
    return FALSE;

  decl->type.is_new = TRUE;

  if (flags & DECLMASK_NAMED_DECL) {
    if ((flags & DECLFLAG_ARGUMENT) && matchtoken(tELLIPS)) {
      decl->type.ident = iVARARGS;
      return TRUE;
    }

    if ((flags & DECLFLAG_MAYBE_FUNCTION) && matchtoken(tOPERATOR)) {
      decl->opertok = operatorname(decl->name);
      if (decl->opertok == 0)
        strcpy(decl->name, "__unknown__");
    } else {
      if (!expecttoken(tSYMBOL, &tok)) {
        strcpy(decl->name, "__unknown__");
        return FALSE;
      }
      strcpy(decl->name, tok.str);
    }
  }

  if (flags & DECLMASK_NAMED_DECL) {
    if (matchtoken('[')) {
      if (decl->type.numdim == 0)
        parse_old_array_dims(decl, flags);
      else
        error(121);
    }
  }

  return TRUE;
}

static int reparse_new_decl(declinfo_t *decl, int flags)
{
  token_t tok;
  if (expecttoken(tSYMBOL, &tok))
    strcpy(decl->name, tok.str);

  if (decl->type.has_postdims) {
    // We have something like:
    //    int x[], y...
    //
    // Reset the fact that we saw an array.
    decl->type.numdim = 0;
    decl->type.enumroot = NULL;
    decl->type.ident = iVARIABLE;
    decl->type.size = 1;
    decl->type.has_postdims = false;
    if (matchtoken('['))
      parse_old_array_dims(decl, flags);
  } else {
    if (matchtoken('[')) {
      if (decl->type.numdim > 0)
          error(121);
      parse_old_array_dims(decl, flags);
    } else if (decl->type.numdim) {
      // Reset dimension sizes.
      memset(decl->type.dim, 0, sizeof(decl->type.dim[0]) * decl->type.numdim);
    }
  }

  return TRUE;
}

// Parse a declaration.
//
// Grammar for named declarations is:
//    "const"? symbol ('[' ']')* '&'? symbol
//  | "const"? label? '&'? symbol '[' ']'
//
int parse_decl(declinfo_t *decl, int flags)
{
  token_ident_t ident;

  memset(decl, 0, sizeof(*decl));

  decl->type.ident = iVARIABLE;
  decl->type.size = 1;

  // Match early varargs as old decl.
  if (lexpeek(tELLIPS))
    return parse_old_decl(decl, flags);

  // Must attempt to match const first, since it's a common prefix.
  if (matchtoken(tCONST))
    decl->type.usage |= uCONST;

  // Sometimes we know ahead of time whether the declaration will be old, for
  // example, if preceded by tNEW or tDECL.
  if (flags & DECLFLAG_OLD)
    return parse_old_decl(decl, flags);
  if (flags & DECLFLAG_NEW)
    return parse_new_decl(decl, NULL, flags);

  // If parsing an argument, there are two simple checks for whether this is a
  // new or old-style declaration.
  if ((flags & DECLFLAG_ARGUMENT) && (lexpeek('&') || lexpeek('{')))
    return parse_old_decl(decl, flags);

  // Another dead giveaway is there being a label or typeless operator.
  if (lexpeek(tLABEL) || lexpeek(tOPERATOR))
    return parse_old_decl(decl, flags);

  // Otherwise, we have to eat a symbol to tell.
  if (matchsymbol(&ident)) {
    if (lexpeek(tSYMBOL) ||
        lexpeek(tOPERATOR) ||
        lexpeek('&') ||
        lexpeek(tELLIPS))
    {
      // A new-style declaration only allows array dims or a symbol name, so
      // this is a new-style declaration.
      return parse_new_decl(decl, &ident.tok, flags);
    }

    if ((flags & DECLMASK_NAMED_DECL) && matchtoken('[')) {
      // Oh no - we have to parse array dims before we can tell what kind of
      // declarator this is. It could be either:
      //    "x[] y" (new-style), or
      //    "y[],"  (old-style)
      parse_old_array_dims(decl, flags);

      if (matchtoken(tSYMBOL) || matchtoken('&')) {
        // This must be a newdecl, "x[] y" or "x[] &y", the latter of which
        // is illegal, but we flow it through the right path anyway.
        lexpush();
        decl->type.has_postdims = false;
        return parse_new_decl(decl, &ident.tok, flags);
      }

      // The most basic - "x[]" and that's it. Well, we know it has no tag and
      // we know its name. We might as well just complete the entire decl.
      strcpy(decl->name, ident.name);
      decl->type.tag = 0;
      return TRUE;
    }

    // Give the symbol back to the lexer. This is an old decl.
    lexpush();
    return parse_old_decl(decl, flags);
  }

  // All else has failed. Probably got a type keyword. New-style.
  return parse_new_decl(decl, NULL, flags);
}

static void check_void_decl(const declinfo_t *decl, int variable)
{
  if (decl->type.tag != pc_tag_void)
    return;

  if (variable) {
    error(144);
    return;
  }

  if (decl->type.numdim > 0) {
    error(145);
    return;
  }
}

// If a name is too long, error and truncate.
void check_name_length(char *original)
{
  if (strlen(original) > sNAMEMAX) {
    char buffer[METHOD_NAMEMAX + 1];
    strcpy(buffer, original);
    buffer[sNAMEMAX] = '\0';
    error(123, original, buffer);
    original[sNAMEMAX] = '\0';
  }
}

static void make_primitive(typeinfo_t *type, int tag)
{
  memset(type, 0, sizeof(*type));
  type->tag = tag;
  type->ident = iVARIABLE;
}

symbol *parse_inline_function(methodmap_t *map,
                              const typeinfo_t *type,
                              const char *name,
                              int is_native,
                              int is_ctor,
                              bool is_static)
{
  declinfo_t decl;
  memset(&decl, 0, sizeof(decl));

  if (is_ctor) {
    make_primitive(&decl.type, map->tag);
  } else {
    decl.type = *type;
  }
  decl.type.is_new = TRUE;

  const int *thistag = NULL;
  if (!is_ctor && !is_static)
    thistag = &map->tag;

  // Build a new symbol. Construct a temporary name including the class.
  char fullname[METHOD_NAMEMAX + 1];
  strcpy(fullname, map->name);
  strcat(fullname, ".");
  strcat(fullname, name);
  check_name_length(fullname);
  strcpy(decl.name, fullname);

  symbol *target = NULL;
  if (is_native) {
    target = funcstub(tMETHODMAP, &decl, thistag);
  } else {
    int lcl_require_newdecls = sc_require_newdecls;

    sc_require_newdecls = TRUE;
    int ok = newfunc(&decl, thistag, TRUE, FALSE, FALSE, &target);
    sc_require_newdecls = lcl_require_newdecls;

    if (!ok)
      return NULL;
    if (!target || (target->usage & uFORWARD)) {
      error(10);
      return NULL;
    }
  }
  return target;
}

int parse_property_accessor(const typeinfo_t *type, methodmap_t *map, methodmap_method_t *method)
{
  token_ident_t ident;
  int is_native = FALSE;

  needtoken(tPUBLIC);
  if (!matchsymbol(&ident)) {
    if (!matchtoken(tNATIVE)) {
      error(125);
      return FALSE;
    }
    is_native = TRUE;
    if (!needsymbol(&ident))
      return FALSE;
  }

  int getter = (strcmp(ident.name, "get") == 0);
  int setter = (strcmp(ident.name, "set") == 0);

  if (!getter && !setter) {
    error(125);
    return FALSE;
  }

  typeinfo_t voidtype;
  char tmpname[METHOD_NAMEMAX + 1];
  strcpy(tmpname, method->name);
  if (getter)
    strcat(tmpname, ".get");
  else
    strcat(tmpname, ".set");

  const typeinfo_t *ret_type;
  if (getter) {
    ret_type = type;
  } else {
    make_primitive(&voidtype, pc_tag_void);
    ret_type = &voidtype;
  }

  symbol* target = parse_inline_function(map, ret_type, tmpname, is_native, FALSE, false);

  if (!target)
    return FALSE;

  if (getter && method->getter) {
    error(126, "getter", method->name);
    return FALSE;
  }
  if (setter && method->setter) {
    error(126, "setter", method->name);
    return FALSE;
  }

  if (getter) {
    method->getter = target;

    // Cannot have extra arguments.
    if (target->dim.arglist[0].ident && target->dim.arglist[1].ident)
      error(127);
  } else {
    method->setter = target;

    // Must have one extra argument taking the return type.
    arginfo *arg = &target->dim.arglist[1];
    if (arg->ident != iVARIABLE ||
        arg->hasdefault ||
        arg->tag != type->tag)
    {
      error(150, pc_tagname(type->tag));
      return FALSE;
    }
    if (target->dim.arglist[2].ident) {
      error(150, pc_tagname(type->tag));
      return FALSE;
    }
  }

  require_newline(target->usage & uNATIVE);
  return TRUE;
}

methodmap_method_t *parse_property(methodmap_t *map)
{
  typeinfo_t type;
  token_ident_t ident;
  methodmap_method_t *method;

  memset(&type, 0, sizeof(type));
  if (!parse_new_typeexpr(&type, NULL, 0))
    return NULL;
  if (type.numdim > 0)
    error(82);

  if (!needsymbol(&ident))
    return NULL;

  method = (methodmap_method_t *)calloc(1, sizeof(methodmap_method_t));
  strcpy(method->name, ident.name);
  method->target = NULL;
  method->getter = NULL;
  method->setter = NULL;

  if (matchtoken('{')) {
    while (!matchtoken('}')) {
      if (!parse_property_accessor(&type, map,method))
        lexclr(TRUE);
    }

    require_newline(FALSE);
  }

  return method;
}

methodmap_method_t *parse_method(methodmap_t *map)
{
  int maybe_ctor = 0;
  int is_ctor = 0;
  int is_native = 0;
  bool is_static = false;
  const char *spectype = layout_spec_name(map->spec);

  if (matchtoken(tSTATIC))
    is_static = true;

  // This stores the name of the method (for destructors, we add a ~).
  token_ident_t ident;
  strcpy(ident.name, "__unknown__");

  typeinfo_t type;
  memset(&type, 0, sizeof(type));

  // Destructors cannot be static.
  int got_symbol;

  is_native = matchtoken(tNATIVE);
  got_symbol = matchsymbol(&ident);

  if (matchtoken('~'))
    error(118);

  if (got_symbol && matchtoken('(')) {
    // ::= ident '('

    // Push the '(' token back for declargs().
    maybe_ctor = TRUE;
    lexpush();
  } else {
    // The first token of the type expression is either the symbol we
    // predictively parsed earlier, or it's been pushed back into the
    // lex buffer.
    const token_t *first = got_symbol ? &ident.tok : NULL;

    // Parse for type expression, priming it with the token we predicted
    // would be an identifier.
    if (!parse_new_typeexpr(&type, first, 0))
      return NULL;

    // Now, we should get an identifier.
    if (!needsymbol(&ident))
      return NULL;

    // If the identifier is a constructor, error, since the user specified
    // a type.
    if (strcmp(ident.name, map->name) == 0)
      error(99, "constructor");
  }

  // Do some preliminary verification of ctor names.
  if (maybe_ctor) {
    if (strcmp(ident.name, map->name) == 0)
      is_ctor = TRUE;
    else
      error(114, "constructor", spectype, map->name);
  }

  if (is_ctor && is_static) {
    // Constructors may not be static.
    error(175);
  }

  // Natives will hit a similar error on their own, so we only check numdim
  // if we're not parsing a native.
  if (!is_native && type.numdim > 0)
    error(83);

  symbol* target = parse_inline_function(map, &type, ident.name, is_native, is_ctor, is_static);

  if (!target)
    return NULL;

  methodmap_method_t *method = (methodmap_method_t *)calloc(1, sizeof(methodmap_method_t));
  strcpy(method->name, ident.name);
  method->target = target;
  method->getter = NULL;
  method->setter = NULL;
  method->is_static = is_static;

  // If the symbol is a constructor, we bypass the initial argument checks.
  if (is_ctor) {
    if (map->ctor)
      error(113, map->name);

    map->ctor = method;
  }

  require_newline(target->usage & uNATIVE);
  return method;
}

void declare_methodmap_symbol(methodmap_t* map, bool can_redef)
{
  if (can_redef) {
    symbol *sym = findglb(map->name);
    if (sym && sym->ident != iMETHODMAP) {
      // We should only hit this on the first pass. Assert really hard that
      // we're about to kill an enum definition and not something random.
      assert(sc_status == statFIRST);
      assert(sym->ident == iCONSTEXPR);
      assert(TAGID(map->tag) == TAGID(sym->tag));

      sym->ident = iMETHODMAP;

      // Kill previous enumstruct properties, if any.
      if (sym->usage & uENUMROOT) {
        for (constvalue *cv = sym->dim.enumlist; cv; cv = cv->next) {
          symbol *csym = findglb(cv->name);
          if (csym &&
              csym->ident == iCONSTEXPR &&
              csym->parent == sym &&
              (csym->usage & uENUMFIELD))
          {
            csym->usage &= ~uENUMFIELD;
            csym->parent = NULL;
          }
        }
        delete_consttable(sym->dim.enumlist);
        free(sym->dim.enumlist);
        sym->dim.enumlist = NULL;
      }
    } else if (!sym) {
      sym = addsym(
        map->name,    // name
        0,            // addr
        iMETHODMAP,   // ident
        sGLOBAL,      // vclass
        map->tag,     // tag
        uDEFINE);     // usage
    }
    sym->methodmap = map;
  }
}

static void declare_handle_intrinsics()
{
  // Must not have an existing Handle methodmap.
  if (methodmap_find_by_name("Handle")) {
    error(156);
    return;
  }

  int tag = pc_addtag_flags("Handle", FIXEDTAG | METHODMAPTAG);
  methodmap_t *map = methodmap_add(nullptr, Layout_MethodMap, "Handle", tag);
  map->nullable = true;

  declare_methodmap_symbol(map, true);

  if (symbol* sym = findglb("CloseHandle")) {
    map->dtor = (methodmap_method_t*)calloc(1, sizeof(methodmap_method_t));
    map->dtor->target = sym;
    strcpy(map->dtor->name, "~Handle");

    methodmap_method_t* close = (methodmap_method_t*)calloc(1, sizeof(methodmap_method_t));
    close->target = sym;
    strcpy(close->name, "Close");

    methodmap_add_method(map, close);
  }
}

static bool dousing()
{
  token_ident_t ident;
  if (!needsymbol(&ident))
    return false;
  if (strcmp(ident.name, "__intrinsics__") != 0) {
    error(156);
    return false;
  }
  if (!needtoken('.'))
    return false;
  if (!needsymbol(&ident))
    return false;
  if (strcmp(ident.name, "Handle") != 0) {
    error(156);
    return false;
  }

  declare_handle_intrinsics();
  require_newline(TRUE);
  return true;
}

/**
 * domethodmap - declare a method map for OO-ish syntax.
 *
 */
static void domethodmap(LayoutSpec spec)
{
  int val;
  char *str;
  methodmap_t *parent = NULL;
  const char *spectype = layout_spec_name(spec);

  // methodmap ::= "methodmap" symbol ("<" symbol)? "{" methodmap-body "}"
  char mapname[sNAMEMAX + 1];
  if (lex(&val, &str) != tSYMBOL)
    error(93);
  strcpy(mapname, str);

  if (!isupper(*mapname))
    error(109, spectype);

  LayoutSpec old_spec = deduce_layout_spec_by_name(mapname);
  bool can_redef = can_redef_layout_spec(spec, old_spec);
  if (!can_redef)
    error(110, mapname, layout_spec_name(old_spec));

  int old_nullable = matchtoken(tNULLABLE);

  if (matchtoken('<')) {
    if (lex(&val, &str) != tSYMBOL) {
      error(93);
      return;
    }

    if ((parent = methodmap_find_by_name(str)) == NULL) {
      error(102, spectype, str);
    } else if (parent->spec != spec) {
      error(129);
    }
  }

  int tag = 0;
  if (spec == Layout_MethodMap)
    tag = pc_addtag_flags(mapname, FIXEDTAG | METHODMAPTAG);
  else
    tag = pc_addtag_flags(mapname, FIXEDTAG | OBJECTTAG);
  methodmap_t *map = methodmap_add(parent, spec, mapname, tag);

  if (old_nullable)
    map->keyword_nullable = old_nullable;

  declare_methodmap_symbol(map, can_redef);

  needtoken('{');
  while (!matchtoken('}')) {
    token_t tok;
    methodmap_method_t *method = NULL;

    if (lextok(&tok) == tPUBLIC) {
      method = parse_method(map);
    } else if (tok.id == tSYMBOL && strcmp(tok.str, "property") == 0) {
      method = parse_property(map);
    } else {
      error(124);
    }

    if (method) {
      // Check that a method with this name doesn't already exist.
      for (size_t i = 0; i < map->nummethods; i++) {
        if (strcmp(map->methods[i]->name, method->name) == 0) {
          error(103, method->name, spectype);
          method = NULL;
          break;
        }
      }
    }

    if (!method) {
      if (!consume_line())
        return;
      continue;
    }

    methodmap_add_method(map, method);
  }

  require_newline(TRUE);
}

class AutoStage
{
 public:
  AutoStage() : lcl_staging_(FALSE)
  {
    if (!staging) {
      stgset(TRUE);
      lcl_staging_ = TRUE;
      lcl_stgidx_ = stgidx;
      assert(stgidx == 0);
    }
  }
  ~AutoStage() {
    if (lcl_staging_) {
      stgout(lcl_stgidx_);
      stgset(FALSE);
    }
  }

 private:
  int lcl_staging_;
  int lcl_stgidx_;
};

// delete ::= "delete" expr
static void dodelete()
{
  AutoStage staging_on;

  svalue sval;
  int ident = lvalexpr(&sval);
  needtoken(tTERM);

  switch (ident) {
    case iFUNCTN:
    case iREFFUNC:
      error(167, "functions");
      return;

    case iARRAY:
    case iREFARRAY:
    case iARRAYCELL:
    case iARRAYCHAR:
    {
      symbol *sym = sval.val.sym;
      if (!sym || sym->dim.array.level > 0) {
        error(167, "arrays");
        return;
      }
      break;
    }
  }

  if (sval.val.tag == 0) {
    error(167, "integers");
    return;
  }

  methodmap_t *map = methodmap_find_by_tag(sval.val.tag);
  if (!map) {
    error(115, "type", pc_tagname(sval.val.tag));
    return;
  }

  {
    methodmap_t *iter = map;
    while (iter) {
      if (iter->dtor) {
        map = iter;
        break;
      }
      iter = iter->parent;
    }
  }

  if (!map || !map->dtor) {
    error(115, layout_spec_name(map->spec), map->name);
    return;
  }

  // Only zap non-const lvalues.
  int zap = sval.lvalue;
  if (zap && sval.val.sym && (sval.val.sym->usage & uCONST))
    zap = FALSE;

  int popaddr = FALSE;
  methodmap_method_t *accessor = NULL;
  if (sval.lvalue) {
    if (zap) {
      switch (sval.val.ident) {
        case iACCESSOR:
          // rvalue() removes iACCESSOR so we store it locally.
          accessor = sval.val.accessor;
          if (!accessor->setter) {
            zap = FALSE;
            break;
          }
          pushreg(sPRI);
          popaddr = TRUE;
          break;
        case iARRAYCELL:
        case iARRAYCHAR:
          pushreg(sPRI);
          popaddr = TRUE;
          break;
      }
    }
    
    rvalue(&sval.val);
  }

  // push.pri
  // push.c 1
  // sysreq.c N 1
  // stack 8
  pushreg(sPRI);
  {
    ffcall(map->dtor->target, NULL, 1);

    // Only mark usage if we're not skipping codegen.
    if (sc_status != statSKIP)
      markusage(map->dtor->target, uREAD);
  }

  if (zap) {
    if (popaddr)
      popreg(sALT);

    // Store 0 back.
    ldconst(0, sPRI);
    if (accessor)
      invoke_setter(accessor, FALSE);
    else
      store(&sval.val);
  }

  markexpr(sEXPR, NULL, 0);
}

/**
 * function-type ::= "(" function-type-inner ")"
 *                 | function-type-inner
 * function-type-inner ::= "function" type-expr "(" new-style-args ")"
 */
static void parse_function_type(functag_t *type)
{
  memset(type, 0, sizeof(*type));

  int lparen = matchtoken('(');
  needtoken(tFUNCTION);

  parse_new_typename(NULL, &type->ret_tag);
  type->usage = uPUBLIC;

  needtoken('(');

  while (!matchtoken(')')) {
    declinfo_t decl;

    // Initialize.
    memset(&decl, 0, sizeof(decl));
    decl.type.ident = iVARIABLE;

    parse_new_decl(&decl, NULL, DECLFLAG_ARGUMENT);

    // Eat optional symbol name.
    matchtoken(tSYMBOL);

    // Error once when we're past max args.
    if (type->argcount == SP_MAX_EXEC_PARAMS) {
      error(45);
      continue;
    }

    // Account for strings.
    fix_char_size(&decl);

    funcarg_t *arg = &type->args[type->argcount++];
    arg->tagcount = 1;
    arg->tags[0] = decl.type.tag;
    arg->dimcount = decl.type.numdim;
    memcpy(arg->dims, decl.type.dim, arg->dimcount * sizeof(decl.type.dim[0]));
    arg->fconst = (decl.type.usage & uCONST) ? TRUE : FALSE;
    if (decl.type.ident == iARRAY)
      arg->ident = iREFARRAY;
    else
      arg->ident = decl.type.ident;

    if (!matchtoken(',')) {
      needtoken(')');
      break;
    }
  }

  if (lparen)
    needtoken(')');

  require_newline(TRUE);
  errorset(sRESET, 0);
}

static void dotypedef()
{
  token_ident_t ident;
  if (!needsymbol(&ident))
    return;

  int prev_tag = pc_findtag(ident.name);
  if (prev_tag != -1 && !(prev_tag & FUNCTAG))
    error(94);

  needtoken('=');

  funcenum_t *def = funcenums_add(ident.name);

  functag_t type;
  parse_function_type(&type);
  functags_add(def, &type);
}

// Unsafe typeset - only supports function types. This is a transition hack for SP2.
static void dotypeset()
{
  token_ident_t ident;
  if (!needsymbol(&ident))
    return;

  int prev_tag = pc_findtag(ident.name);
  if (prev_tag != -1 && !(prev_tag & FUNCTAG))
    error(94);

  funcenum_t *def = funcenums_add(ident.name);
  needtoken('{');
  while (!matchtoken('}')) {
    functag_t type;
    parse_function_type(&type);
    functags_add(def, &type);
  }

  require_newline(TRUE);
}

/**
 * dofuncenum - declare function enumerations
 */
static void dofuncenum(int listmode)
{
  cell val;
  char *str;
  // char *ptr;
  char tagname[sNAMEMAX+1];
  constvalue *cur;
  funcenum_t *fenum = NULL;
  int i;
  int newStyleTag = 0;
  int isNewStyle = 0;

  /* get the explicit tag (required!) */
  int l = lex(&val,&str);
  if (l != tSYMBOL) {
    if (listmode == FALSE && l == tPUBLIC) {
      isNewStyle = TRUE;
      switch (lex(&val, &str)) {
      case tOBJECT:
        newStyleTag = pc_tag_object;
        break;
      case tINT:
        newStyleTag = 0;
        break;
      case tVOID:
        newStyleTag = pc_tag_void;
        break;
      case tCHAR:
        newStyleTag = pc_tag_string;
        break;
      case tLABEL:
        newStyleTag = pc_addtag(str);
        break;
      case tSYMBOL:
        // Check whether this is new-style declaration.
        // we'll port this all to parse_decl() sometime.
        if (lexpeek('('))
          lexpush();
        else
          newStyleTag = pc_addtag(str);
        break;
      default:
        error(93);
      }

      if (!needtoken(tSYMBOL)) {
        lexclr(TRUE);
        litidx = 0;
        return;
      }
      l = tokeninfo(&val, &str);
    } else {
      error(93);
    }
  }

  /* This tag can't already exist! */
  cur=tagname_tab.next;
  while (cur) {
    if (strcmp(cur->name, str) == 0) {
      /* Another bad one... */
      if (!(cur->value & FUNCTAG))
        error(94);
      break;
    }
    cur = cur->next;
  }
  strcpy(tagname, str);

  fenum = funcenums_add(tagname);

  if (listmode)
    needtoken('{');
  do {
    functag_t func;
    if (listmode && matchtoken('}')) {
      /* Quick exit */
      lexpush();
      break;
    }
    memset(&func, 0, sizeof(func));
    if (!isNewStyle) {
      func.ret_tag = pc_addtag(NULL);  /* Get the return tag */
      l = lex(&val, &str);
      /* :TODO:
       * Right now, there is a problem.  We can't pass non-public function pointer addresses around,
       * because the address isn't known until the final reparse.  Unfortunately, you can write code
       * before the address is known, because Pawn's compiler isn't truly multipass.
       *
       * When you call a function, there's no problem, because it does not write the address.
       * The assembly looks like this:
       *   call MyFunction
       * Then, only at assembly time (once all passes are done), does it know the address.
       * I do not see any solution to this because there is no way I know to inject the function name
       * rather than the constant value.  And even if we could, we'd have to change the assembler recognize that.
       */
      if (l == tPUBLIC) {
        func.usage = uPUBLIC;
      } else {
        error(1, "-public-", str);
      }
    } else {
      func.ret_tag = newStyleTag;
      func.usage = uPUBLIC;
    }
    needtoken('(');
    do {
      funcarg_t *arg = &(func.args[func.argcount]);

      /* Quick exit */
      if (matchtoken(')')) {
        lexpush();
        break;
      }
      l = lex(&val, &str);
      if (l == '&') {
        if ((arg->ident != iVARIABLE && arg->ident != 0) || arg->tagcount > 0)
          error(1, "-identifier-", "&");
        arg->ident = iREFERENCE;
      } else if (l == tCONST) {
        if ((arg->ident != iVARIABLE && arg->ident != 0) || arg->tagcount > 0)
          error(1, "-identifier-", "const");
        arg->fconst=TRUE;
      } else if (l == tLABEL) {
        if (arg->tagcount > 0)
          error(1, "-identifier-", "-tagname-");
        arg->tags[arg->tagcount++] = pc_addtag(str);
        l=tLABEL;
      } else if (l == tSYMBOL) {
        if (func.argcount >= SP_MAX_EXEC_PARAMS)
          error(45);
        if (str[0] == PUBLIC_CHAR)
          error(56, str);
        if (matchtoken('['))
        {
          cell size;
          if (arg->ident == iREFERENCE)
            error(67, str);
          do {
            constvalue *enumroot;
            int ignore_tag;
            if (arg->dimcount == sDIMEN_MAX) {
              error(53);
              break;
            }
            size = needsub(&ignore_tag, &enumroot);
            arg->dims[arg->dimcount] = size;
            arg->dimcount += 1;
          } while (matchtoken('['));
          /* Handle strings */
          if ((arg->tagcount == 1 && arg->tags[0] == pc_tag_string)
            && arg->dims[arg->dimcount-1])
          {
            arg->dims[arg->dimcount-1] = (size + sizeof(cell)-1) / sizeof(cell);
          }
          arg->ident=iREFARRAY;
        } else if (arg->ident == 0) {
          arg->ident = iVARIABLE;
        }
      
        if (matchtoken('=')) {
          needtoken('0');
          arg->ommittable = TRUE;
          func.ommittable = TRUE;
        } else if (func.ommittable) {
          error(95);
        }
        func.argcount++;
      } else if (l == tELLIPS) {
        if (arg->ident == iVARIABLE)
          error(10);
        arg->ident = iVARARGS;
        func.argcount++;
      } else {
        error(10);
      }
    } while (l == '&' || l == tLABEL || l == tCONST || (l != tELLIPS && matchtoken(',')));
    needtoken(')');
    for (i=0; i<func.argcount; i++) {
      if (func.args[i].tagcount == 0) {
        func.args[i].tags[0] = 0;
        func.args[i].tagcount = 1;
      }
    }
    functags_add(fenum, &func);
    if (!listmode)
      break;
  } while (matchtoken(','));
  if (listmode)
    needtoken('}');
  matchtoken(';'); /* eat an optional semicolon.  nom nom nom */
  errorset(sRESET, 0);
}

/*  decl_enum   - declare enumerated constants
 *
 */
static void decl_enum(int vclass)
{
  char enumname[sNAMEMAX+1],constname[sNAMEMAX+1];
  cell val,value,size;
  char *str;
  int tag,explicittag;
  cell increment,multiplier;
  LayoutSpec spec;
  symbol *enumsym = nullptr;
  constvalue *enumroot = nullptr;

  /* get an explicit tag, if any (we need to remember whether an explicit
   * tag was passed, even if that explicit tag was "_:", so we cannot call
   * pc_addtag() here
   */
  if (lex(&val,&str)==tLABEL) {
    if (pc_findtag(str) == 0) {
      error(169);
      tag = 0;
      explicittag = FALSE;
    } else {
      int flags = ENUMTAG;
      if (isupper(*str))
        flags |= FIXEDTAG;
      tag = pc_addtag_flags(str, flags);
      spec = deduce_layout_spec_by_tag(tag);
      if (!can_redef_layout_spec(spec, Layout_Enum))
        error(110, str, layout_spec_name(spec));
      explicittag=TRUE;
    }
  } else {
    lexpush();
    tag=0;
    explicittag=FALSE;
  } /* if */

  /* get optional enum name (also serves as a tag if no explicit tag was set) */
  if (lex(&val,&str)==tSYMBOL) {        /* read in (new) token */
    strcpy(enumname,str);               /* save enum name (last constant) */
    if (!explicittag) {
      int flags = ENUMTAG;
      if (isupper(*str))
        flags |= FIXEDTAG;
      tag=pc_addtag_flags(enumname, flags);
      spec = deduce_layout_spec_by_tag(tag);
      if (!can_redef_layout_spec(spec, Layout_Enum))
        error(110, enumname, layout_spec_name(spec));
    } else {
      error(168);

      spec = deduce_layout_spec_by_name(enumname);
      if (!can_redef_layout_spec(spec, Layout_Enum))
        error(110, enumname, layout_spec_name(spec));
    }
  } else {
    lexpush();                          /* analyze again */
    enumname[0]='\0';
  } /* if */

  /* get increment and multiplier */
  increment=1;
  multiplier=1;
  if (matchtoken('(')) {
    if (matchtoken(taADD)) {
      exprconst(&increment,NULL,NULL);
    } else if (matchtoken(taMULT)) {
      exprconst(&multiplier,NULL,NULL);
    } else if (matchtoken(taSHL)) {
      exprconst(&val,NULL,NULL);
      while (val-->0)
        multiplier*=2;
    } /* if */
    needtoken(')');
  } /* if */

  if (strlen(enumname)>0) {
    if (vclass == sGLOBAL) {
      if ((enumsym = findglb(enumname)) != NULL) {
        // If we were previously defined as a methodmap, don't overwrite the
        // symbol. Otherwise, flow into add_constant where we will error.
        if (enumsym->ident != iMETHODMAP)
          enumsym = nullptr;
      }
    }

    if (!enumsym) {
      /* create the root symbol, so the fields can have it as their "parent" */
      enumsym=add_constant(enumname,0,vclass,tag);
      if (enumsym!=NULL)
        enumsym->usage |= uENUMROOT;
      /* start a new list for the element names */
      if ((enumroot=(constvalue*)malloc(sizeof(constvalue)))==NULL)
        error(FATAL_ERROR_OOM);                       /* insufficient memory (fatal error) */
      memset(enumroot,0,sizeof(constvalue));
    }
  } else {
    enumsym=NULL;
    enumroot=NULL;
  } /* if */

  // If this enum is for a methodmap, forget the symbol so code below doesn't
  // build an enum struct.
  if (enumsym && enumsym->ident == iMETHODMAP)
    enumsym = NULL;

  needtoken('{');
  /* go through all constants */
  value=0;                              /* default starting value */
  do {
    int idxtag,fieldtag;
    symbol *sym;
    if (matchtoken('}')) {              /* quick exit if '}' follows ',' */
      lexpush();
      break;
    } /* if */
    idxtag=pc_addtag(NULL);             /* optional explicit item tag */
    if (needtoken(tSYMBOL)) {           /* read in (new) token */
      tokeninfo(&val,&str);             /* get the information */
      strcpy(constname,str);            /* save symbol name */
    } else {
      constname[0]='\0';
    } /* if */
    size=increment;                     /* default increment of 'val' */
    fieldtag=0;                         /* default field tag */
    if (matchtoken('[')) {
      exprconst(&size,&fieldtag,NULL);  /* get size */
      needtoken(']');
    } /* if */
    /* :TODO: do we need a size modifier here for pc_tag_string? */
    if (matchtoken('='))
      exprconst(&value,NULL,NULL);      /* get value */
    /* add_constant() checks whether a variable (global or local) or
     * a constant with the same name already exists
     */
    sym=add_constant(constname,value,vclass,tag);
    if (sym==NULL)
      continue;                         /* error message already given */
    /* set the item tag and the item size, for use in indexing arrays */
    sym->x.tags.index=idxtag;
    sym->x.tags.field=fieldtag;
    sym->dim.array.length=size;
    sym->dim.array.level=0;
    sym->dim.array.slength=0;
    sym->parent=enumsym;
    /* add the constant to a separate list as well */
    if (enumroot!=NULL) {
      sym->usage |= uENUMFIELD;
      append_constval(enumroot,constname,value,tag);
    } /* if */
    if (multiplier==1)
      value+=size;
    else
      value*=size*multiplier;
  } while (matchtoken(','));
  needtoken('}');       /* terminates the constant list */
  matchtoken(';');      /* eat an optional ; */

  /* set the enum name to the "next" value (typically the last value plus one) */
  if (enumsym) {
    assert((enumsym->usage & uENUMROOT)!=0);
    enumsym->setAddr(value);
    /* assign the constant list */
    assert(enumroot!=NULL);
    enumsym->dim.enumlist=enumroot;
  } /* if */
}

// This simpler version of matchtag() only checks whether two tags represent
// the same type. Because methodmaps are attached to types and are not actually
// types themselves, we strip out the methodmap bit in case a methodmap was
// seen later than another instance of a tag.
static int compare_tag(int tag1, int tag2)
{
  return (tag1 & (~METHODMAPTAG)) == (tag2 & (~METHODMAPTAG));
}

/*
 *  Finds a function in the global symbol table or creates a new entry.
 *  It does some basic processing and error checking.
 */
symbol *fetchfunc(char *name)
{
  symbol *sym;

  if ((sym=findglb(name))!=0) {   /* already in symbol table? */
    if (sym->ident!=iFUNCTN) {
      error(21,name);                     /* yes, but not as a function */
      return NULL;                        /* make sure the old symbol is not damaged */
    } else if ((sym->usage & uNATIVE)!=0) {
      error(21,name);                     /* yes, and it is a native */
    } /* if */
    assert(sym->vclass==sGLOBAL);
  } else {
    /* don't set the "uDEFINE" flag; it may be a prototype */
    sym=addsym(name,code_idx,iFUNCTN,sGLOBAL,0,0);
    assert(sym!=NULL);          /* fatal error 103 must be given on error */
    /* assume no arguments */
    sym->dim.arglist=(arginfo*)calloc(1, sizeof(arginfo));
    /* set library ID to NULL (only for native functions) */
    sym->x.lib=NULL;
    /* set the required stack size to zero (only for non-native functions) */
    sym->x.stacksize=1;         /* 1 for PROC opcode */
  } /* if */
  if (pc_deprecate!=NULL) {
    assert(sym!=NULL);
    sym->flags|=flgDEPRECATED;
    if (sc_status==statWRITE) {
      if (sym->documentation!=NULL) {
        free(sym->documentation);
        sym->documentation=NULL;
      } /* if */
      sym->documentation=pc_deprecate;
    } else {
      free(pc_deprecate);
    } /* if */
    pc_deprecate=NULL;
  } /* if */

  return sym;
}

/* This routine adds symbolic information for each argument.
 */
static void define_args(void)
{
  symbol *sym;

  /* At this point, no local variables have been declared. All
   * local symbols are function arguments.
   */
  sym=loctab.next;
  while (sym!=NULL) {
    assert(sym->ident!=iLABEL);
    assert(sym->vclass==sLOCAL);
    markexpr(sLDECL,sym->name,sym->addr()); /* mark for better optimization */
    sym=sym->next;
  } /* while */
}

static int operatorname(char *name)
{
  int opertok;
  char *str;
  cell val;

  assert(name!=NULL);

  /* check the operator */
  opertok=lex(&val,&str);
  switch (opertok) {
  case '+':
  case '-':
  case '*':
  case '/':
  case '%':
  case '>':
  case '<':
  case '!':
  case '~':
  case '=':
    name[0]=(char)opertok;
    name[1]='\0';
    break;
  case tINC:
    strcpy(name,"++");
    break;
  case tDEC:
    strcpy(name,"--");
    break;
  case tlEQ:
    strcpy(name,"==");
    break;
  case tlNE:
    strcpy(name,"!=");
    break;
  case tlLE:
    strcpy(name,"<=");
    break;
  case tlGE:
    strcpy(name,">=");
    break;
  default:
    name[0]='\0';
    error(7);           /* operator cannot be redefined (or bad operator name) */
    return 0;
  } /* switch */

  return opertok;
}

static int operatoradjust(int opertok,symbol *sym,char *opername,int resulttag)
{
  int tags[2]={0,0};
  int count=0;
  arginfo *arg;
  char tmpname[sNAMEMAX+1];
  symbol *oldsym;

  if (opertok==0)
    return TRUE;

  assert(sym!=NULL && sym->ident==iFUNCTN && sym->dim.arglist!=NULL);
  /* count arguments and save (first two) tags */
  while (arg=&sym->dim.arglist[count], arg->ident!=0) {
    if (count<2) {
      tags[count]=arg->tag;
    } /* if */
    if (opertok=='~' && count==0) {
      if (arg->ident!=iREFARRAY)
        error(73,arg->name);/* must be an array argument */
    } else {
      //if (arg->ident!=iVARIABLE)
        //error(66,arg->name);/* must be non-reference argument */
    } /* if */
    if (arg->hasdefault)
      error(59,arg->name);  /* arguments of an operator may not have a default value */
    count++;
  } /* while */

  /* for '!', '++' and '--', count must be 1
   * for '-', count may be 1 or 2
   * for '=', count must be 1, and the resulttag is also important
   * for all other (binary) operators and the special '~' operator, count must be 2
   */
  switch (opertok) {
  case '!':
  case '=':
  case tINC:
  case tDEC:
    if (count!=1)
      error(62);      /* number or placement of the operands does not fit the operator */
    break;
  case '-':
    if (count!=1 && count!=2)
      error(62);      /* number or placement of the operands does not fit the operator */
    break;
  default:
    if (count!=2)
      error(62);      /* number or placement of the operands does not fit the operator */
  } /* switch */

  if (tags[0]==0 && ((opertok!='=' && tags[1]==0) || (opertok=='=' && resulttag==0)))
    error(64);        /* cannot change predefined operators */

  /* change the operator name */
  assert(strlen(opername)>0);
  operator_symname(tmpname,opername,tags[0],tags[1],count,resulttag);
  if ((oldsym=findglb(tmpname))!=NULL) {
    int i;
    if ((oldsym->usage & uDEFINE)!=0) {
      char errname[2*sNAMEMAX+16];
      funcdisplayname(errname,tmpname);
      error(21,errname);        /* symbol already defined */
    } /* if */
    sym->usage|=oldsym->usage;  /* copy flags from the previous definition */
    for (i=0; i<oldsym->numrefers; i++)
      if (oldsym->refer[i]!=NULL)
        refer_symbol(sym,oldsym->refer[i]);
    delete_symbol(&glbtab,oldsym);
  } /* if */
  RemoveFromHashTable(sp_Globals, sym);
  strcpy(sym->name,tmpname);
  sym->hash=NameHash(sym->name);/* calculate new hash */
  AddToHashTable(sp_Globals, sym);

  /* operators should return a value, except the '~' operator */
  if (opertok!='~')
    sym->usage |= uRETVALUE;

  return TRUE;
}

static int check_operatortag(int opertok,int resulttag,char *opername)
{
  assert(opername!=NULL && strlen(opername)>0);
  switch (opertok) {
  case '!':
  case '<':
  case '>':
  case tlEQ:
  case tlNE:
  case tlLE:
  case tlGE:
    if (resulttag!=pc_tag_bool) {
      error(63,opername,"bool:"); /* operator X requires a "bool:" result tag */
      return FALSE;
    } /* if */
    break;
  case '~':
    if (resulttag!=0) {
      error(63,opername,"_:");    /* operator "~" requires a "_:" result tag */
      return FALSE;
    } /* if */
    break;
  } /* switch */
  return TRUE;
}

static char *tag2str(char *dest,int tag)
{
  assert(tag>=0);
  sprintf(dest,"0%x",tag);
  return isdigit(dest[1]) ? &dest[1] : dest;
}

char *operator_symname(char *symname,const char *opername,int tag1,int tag2,int numtags,int resulttag)
{
  char tagstr1[10], tagstr2[10];
  int opertok;

  assert(numtags>=1 && numtags<=2);
  opertok= (opername[1]=='\0') ? opername[0] : 0;
  if (opertok=='=')
    sprintf(symname,"%s%s%s",tag2str(tagstr1,resulttag),opername,tag2str(tagstr2,tag1));
  else if (numtags==1 || opertok=='~')
    sprintf(symname,"%s%s",opername,tag2str(tagstr1,tag1));
  else
    sprintf(symname,"%s%s%s",tag2str(tagstr1,tag1),opername,tag2str(tagstr2,tag2));
  return symname;
}

static int parse_funcname(char *fname,int *tag1,int *tag2,char *opname)
{
  char *ptr,*name;
  int unary;

  /* tags are only positive, so if the function name starts with a '-',
   * the operator is an unary '-' or '--' operator.
   */
  if (*fname=='-') {
    *tag1=0;
    unary=TRUE;
    ptr=fname;
  } else {
    *tag1=(int)strtol(fname,&ptr,16);
    unary= ptr==fname;  /* unary operator if it doesn't start with a tag name */
  } /* if */
  assert(!unary || *tag1==0);
  assert(*ptr!='\0');
  for (name=opname; !isdigit(*ptr); )
    *name++ = *ptr++;
  *name='\0';
  *tag2=(int)strtol(ptr,NULL,16);
  return unary;
}

constvalue *find_tag_byval(int tag)
{
  return find_constval_byval(&tagname_tab, tag);
}

char *funcdisplayname(char *dest,char *funcname)
{
  int tags[2];
  char opname[10];
  constvalue *tagsym[2];
  int unary;

  if (isalpha(*funcname) || *funcname=='_' || *funcname==PUBLIC_CHAR || *funcname=='\0') {
    if (dest!=funcname)
      strcpy(dest,funcname);
    return dest;
  } /* if */

  unary=parse_funcname(funcname,&tags[0],&tags[1],opname);
  tagsym[1]=find_tag_byval(tags[1]);
  assert(tagsym[1]!=NULL);
  if (unary) {
    sprintf(dest,"operator%s(%s:)",opname,tagsym[1]->name);
  } else {
    tagsym[0]=find_tag_byval(tags[0]);
    assert(tagsym[0]!=NULL);
    /* special case: the assignment operator has the return value as the 2nd tag */
    if (opname[0]=='=' && opname[1]=='\0')
      sprintf(dest,"%s:operator%s(%s:)",tagsym[0]->name,opname,tagsym[1]->name);
    else
      sprintf(dest,"operator%s(%s:,%s:)",opname,tagsym[0]->name,tagsym[1]->name);
  } /* if */
  return dest;
}

static cell char_array_cells(cell size)
{
  return (size + sizeof(cell) - 1) / sizeof(cell);
}

static cell fix_char_size(declinfo_t *decl)
{
  typeinfo_t *type = &decl->type;
  if (type->tag == pc_tag_string && type->numdim && type->dim[type->numdim - 1]) {
    cell slength = type->dim[type->numdim - 1];
    type->dim[type->numdim - 1] = char_array_cells(type->size);
    return slength;
  }
  return 0;
}

static symbol *funcstub(int tokid, declinfo_t *decl, const int *thistag)
{
  char *str;
  cell val;
  symbol *sym;
  int fnative = (tokid == tNATIVE || tokid == tMETHODMAP);
  int fpublic = (tokid == tPUBLIC);

  lastst=0;
  litidx=0;                     /* clear the literal pool */
  assert(loctab.next==NULL);    /* local symbol table should be empty */

  fix_char_size(decl);

  if (decl->opertok)
    check_operatortag(decl->opertok, decl->type.tag, decl->name);

  needtoken('(');               /* only functions may be native/forward */

  sym=fetchfunc(decl->name);
  if (sym==NULL)
    return NULL;
  if ((sym->usage & uPROTOTYPED)!=0 && !compare_tag(sym->tag, decl->type.tag))
    error(25);
  if ((sym->usage & uDEFINE) == 0) {
    // As long as the function stays undefined, update its address and tag.
    sym->setAddr(code_idx);
    sym->tag = decl->type.tag;
  }

  if (fnative) {
    sym->usage=(char)(uNATIVE | uRETVALUE | uDEFINE | (sym->usage & uPROTOTYPED));
    sym->x.lib=curlibrary;
  } else if (fpublic) {
    sym->usage|=uPUBLIC;
  } /* if */
  sym->usage|=uFORWARD;

  declargs(sym, FALSE, thistag);
  /* "declargs()" found the ")" */
  if (!operatoradjust(decl->opertok,sym,decl->name,decl->type.tag))
    sym->usage &= ~uDEFINE;

  /* for a native operator, also need to specify an "exported" function name;
   * for a native function, this is optional
   */
  if (fnative) {
    if (decl->opertok!=0) {
      needtoken('=');
      lexpush();        /* push back, for matchtoken() to retrieve again */
    } /* if */
    if (matchtoken('=')) {
      /* allow number or symbol */
      if (matchtoken(tSYMBOL)) {
        tokeninfo(&val,&str);
        insert_alias(sym->name,str);
      } else {
        exprconst(&val,NULL,NULL);
        sym->setAddr(val);
        /* At the moment, I have assumed that this syntax is only valid if
         * val < 0. To properly mix "normal" native functions and indexed
         * native functions, one should use negative indices anyway.
         * Special code for a negative index in sym->addr() exists in SC4.C
         * (ffcall()) and in SC6.C (the loops for counting the number of native
         * variables and for writing them).
         */
      } /* if */
    } /* if */
  } /* if */

  // Don't look for line endings if we're inline.
  if (tokid != tMETHODMAP)
    needtoken(tTERM);

  /* attach the array to the function symbol */
  if (decl->type.numdim>0)
    error(141);

  litidx=0;                     /* clear the literal pool */
  delete_symbols(&loctab,0,TRUE,TRUE);/* clear local variables queue */

  return sym;
}

/*  newfunc    - begin a function
 *
 *  This routine is called from "parse" and tries to make a function
 *  out of the following text
 *
 *  Global references: funcstatus,lastst,litidx
 *                     rettype  (altered)
 *                     curfunc  (altered)
 *                     declared (altered)
 *                     glb_declared (altered)
 */
static int newfunc(declinfo_t *decl, const int *thistag, int fpublic, int fstatic, int stock, symbol **symp)
{
  symbol *sym;
  int argcnt,funcline;
  int opererror;
  cell cidx,glbdecl;
  short filenum;

  assert(litidx==0 || !cc_ok());    /* literal queue should be empty */
  litidx=0;             /* clear the literal pool (should already be empty) */
  lastst=0;             /* no statement yet */
  cidx=0;               /* just to avoid compiler warnings */
  glbdecl=0;
  assert(loctab.next==NULL);    /* local symbol table should be empty */
  filenum=fcurrent;     /* save file number at the start of the declaration */

  if (symp)
    *symp = NULL;

  check_void_decl(decl, FALSE);

  if (decl->opertok) {
    check_operatortag(decl->opertok, decl->type.tag, decl->name);
  } /* if */

  /* check whether this is a function or a variable declaration */
  if (!matchtoken('('))
    return FALSE;
  /* so it is a function, proceed */
  funcline=fline;               /* save line at which the function is defined */
  if (decl->name[0]==PUBLIC_CHAR) {
    fpublic=TRUE;               /* implicitly public function */
    if (stock)
      error(42);                /* invalid combination of class specifiers */
  } /* if */

  if ((sym=fetchfunc(decl->name)) == NULL)
    return TRUE;

  // Not a valid function declaration if native.
  if (sym->usage & uNATIVE)
    return TRUE;

  // If the function has not been prototyed, set its tag.
  if (!(sym->usage & uPROTOTYPED))
    sym->tag = decl->type.tag;

  // As long as the function stays undefined, update its address.
  if (!(sym->usage & uDEFINE))
    sym->setAddr(code_idx);

  if (fpublic)
    sym->usage|=uPUBLIC;
  if (fstatic)
    sym->fnumber=filenum;

  if (sym->usage & (uPUBLIC | uFORWARD)) {
    if (decl->type.numdim > 0)
      error(141);
  }

  /* if the function was used before being declared, and it has a tag for the
   * result, add a third pass (as second "skimming" parse) because the function
   * result may have been used with user-defined operators, which have now
   * been incorrectly flagged (as the return tag was unknown at the time of
   * the call)
   */
  if ((sym->usage & (uPROTOTYPED | uREAD))==uREAD && sym->tag!=0) {
    int curstatus=sc_status;
    sc_status=statWRITE;  /* temporarily set status to WRITE, so the warning isn't blocked */
#if 0 /* SourceMod - silly, should be removed in first pass, so removed */
    error(208);
#endif
    sc_status=curstatus;
    sc_reparse=TRUE;      /* must add another pass to "initial scan" phase */
  } /* if */

  /* declare all arguments */
  argcnt = declargs(sym, TRUE, thistag);
  opererror = !operatoradjust(decl->opertok, sym, decl->name, decl->type.tag);
  if (strcmp(decl->name, uMAINFUNC)==0) {
    if (argcnt>0)
      error(5);         /* "main()" functions may not have any arguments */
    sym->usage|=uREAD;  /* "main()" is the program's entry point: always used */
  } /* if */

  if ((sym->usage & uDEFINE)!=0)
    error(21, sym->name);

  /* "declargs()" found the ")"; if a ";" appears after this, it was a
   * prototype */
  if (matchtoken(';')) {
    if (sym->usage & uPUBLIC)
      error(10);
    sym->usage|=uFORWARD;
    if (!sc_needsemicolon)
      error(10);       /* old style prototypes used with optional semicolumns */
    delete_symbols(&loctab,0,TRUE,TRUE);  /* prototype is done; forget everything */
    return TRUE;
  } /* if */
  /* so it is not a prototype, proceed */

  /* if this is a function that is not referred to (this can only be detected
   * in the second stage), shut code generation off */
  if (sc_status==statWRITE && (sym->usage & uREAD)==0 && !fpublic) {
    cidx=code_idx;
    glbdecl=glb_declared;

    sc_status=statSKIP;

    // If this is a method, output errors even if it's unused.
    if (thistag && *thistag != -1)
      sc_err_status = TRUE;
  } /* if */

  if ((sym->flags & flgDEPRECATED) != 0 && (sym->usage & uSTOCK) == 0) {
    const char *ptr= (sym->documentation!=NULL) ? sym->documentation : "";
    error(234, decl->name, ptr);  /* deprecated (probably a public function) */
  } /* if */
  begcseg();
  sym->usage|=uDEFINE;  /* set the definition flag */
  if (stock)
    sym->usage|=uSTOCK;
  if (decl->opertok != 0 && opererror)
    sym->usage &= ~uDEFINE;
  startfunc(sym->name); /* creates stack frame */
  insert_dbgline(funcline);
  setline(FALSE);
  declared=0;           /* number of local cells */
  resetstacklist();
  resetheaplist();
  rettype=(sym->usage & uRETVALUE);      /* set "return type" variable */
  curfunc=sym;
  define_args();        /* add the symbolic info for the function arguments */
  #if !defined SC_LIGHT
    if (matchtoken('{')) {
      lexpush();
    } else {
      // We require '{' for new methods.
      if (decl->type.is_new)
        needtoken('{');
    } /* if */
  #endif
  statement(NULL,FALSE);

  if ((rettype & uRETVALUE)!=0) {
    sym->usage|=uRETVALUE;
  } else {
    if (sym->tag == pc_tag_void &&
        (sym->usage & uFORWARD) &&
        !decl->type.tag &&
        !decl->type.is_new)
    {
      // We got something like:
      //    forward void X();
      //    public X()
      //
      // Switch our decl type to void.
      decl->type.tag = pc_tag_void;
    }
  }

  // Check that return tags match.
  if ((sym->usage & uPROTOTYPED) && !compare_tag(sym->tag, decl->type.tag)) {
    int old_fline = fline;
    fline = funcline;
    error(180, type_to_name(sym->tag), type_to_name(decl->type.tag));
    fline = old_fline;
  }

  if (declared!=0) {
    /* This happens only in a very special (and useless) case, where a function
     * has only a single statement in its body (no compound block) and that
     * statement declares a new variable
     */
    popstacklist(1);
    declared=0;
  } /* if */
  if (lastst!=tRETURN) {
    ldconst(0,sPRI);
    ffret();
    if ((sym->usage & uRETVALUE)!=0) {
      char symname[2*sNAMEMAX+16];  /* allow space for user defined operators */
      funcdisplayname(symname,sym->name);
      error(209,symname);       /* function should return a value */
    } /* if */
  } /* if */
  endfunc();
  sym->codeaddr=code_idx;
  if (litidx) {                 /* if there are literals defined */
    glb_declared+=litidx;
    begdseg();                  /* flip to DATA segment */
    dumplits();                 /* dump literal strings */
    litidx=0;
  } /* if */
  testsymbols(&loctab,0,TRUE,TRUE);     /* test for unused arguments and labels */
  delete_symbols(&loctab,0,TRUE,TRUE);  /* clear local variables queue */
  assert(loctab.next==NULL);
  curfunc=NULL;
  if (sc_status==statSKIP) {
    sc_status=statWRITE;
    code_idx=cidx;
    glb_declared=glbdecl;
    sc_err_status=FALSE;
  } /* if */
  if (symp)
    *symp = sym;
  return TRUE;
}

static int argcompare(arginfo *a1,arginfo *a2)
{
  int result,level;

  result=1;
  if (result)
    result= a1->ident==a2->ident;           /* type/class */
  if (result)
    result= a1->usage==a2->usage;           /* "const" flag */
  if (result)
    result= compare_tag(a1->tag, a2->tag);
  if (result)
    result= a1->numdim==a2->numdim;         /* array dimensions & index tags */
  for (level=0; result && level<a1->numdim; level++)
    result= a1->dim[level]==a2->dim[level];
  for (level=0; result && level<a1->numdim; level++)
    result= compare_tag(a1->idxtag[level], a2->idxtag[level]);
  if (result)
    result= a1->hasdefault==a2->hasdefault; /* availability of default value */
  if (a1->hasdefault) {
    if (a1->ident==iREFARRAY) {
      if (result)
        result= a1->defvalue.array.size==a2->defvalue.array.size;
      if (result)
        result= a1->defvalue.array.arraysize==a2->defvalue.array.arraysize;
      /* ??? should also check contents of the default array (these troubles
       * go away in a 2-pass compiler that forbids double declarations, but
       * Pawn currently does not forbid them) */
    } else {
      if (result) {
        result= a1->defvalue.val==a2->defvalue.val;
      } /* if */
    } /* if */
    if (result)
      result= compare_tag(a1->defvalue_tag, a2->defvalue_tag);
  } /* if */
  return result;
}

/*  declargs()
 *
 *  This routine adds an entry in the local symbol table for each argument
 *  found in the argument list. It returns the number of arguments.
 */
static int declargs(symbol *sym, int chkshadow, const int *thistag)
{
  int argcnt,oldargcnt;
  arginfo arg;

  /* if the function is already defined earlier, get the number of arguments
   * of the existing definition
   */
  oldargcnt=0;
  if ((sym->usage & uPROTOTYPED)!=0)
    while (sym->dim.arglist[oldargcnt].ident!=0)
      oldargcnt++;
  argcnt=0;                             /* zero aruments up to now */

  if (thistag && *thistag != -1) {
    arginfo *argptr;
    if ((sym->usage & uPROTOTYPED) == 0) {
      // Allocate space for a new argument, then terminate.
      sym->dim.arglist = (arginfo *)realloc(sym->dim.arglist, (argcnt + 2) * sizeof(arginfo));
      memset(&sym->dim.arglist[argcnt + 1], 0, sizeof(arginfo));

      argptr = &sym->dim.arglist[argcnt];
      memset(argptr, 0, sizeof(*argptr));
      strcpy(argptr->name, "this");
      argptr->ident = iVARIABLE;
      argptr->tag = *thistag;
      argptr->usage = uCONST;
    } else {
      argptr = &sym->dim.arglist[0];
    }

    symbol *sym = addvariable2(
      argptr->name,
      (argcnt+3)*sizeof(cell),
      argptr->ident,
      sLOCAL,
      argptr->tag,
      argptr->dim,
      argptr->numdim,
      argptr->idxtag,
      0
    );
    sym->usage |= uCONST;
    markusage(sym, uREAD);

    argcnt++;
  }

  /* the '(' parantheses has already been parsed */
  if (!matchtoken(')')){
    do {                                /* there are arguments; process them */
      declinfo_t decl;

      parse_decl(&decl, DECLFLAG_ARGUMENT|DECLFLAG_ENUMROOT);

      check_void_decl(&decl, TRUE);

      if (decl.type.ident == iVARARGS) {
        if ((sym->usage & uPROTOTYPED)==0) {
          /* redimension the argument list, add the entry iVARARGS */
          sym->dim.arglist=(arginfo*)realloc(sym->dim.arglist,(argcnt+2)*sizeof(arginfo));
          if (sym->dim.arglist==0)
            error(FATAL_ERROR_OOM);                 /* insufficient memory */
          memset(&sym->dim.arglist[argcnt+1],0,sizeof(arginfo));  /* keep the list terminated */
          sym->dim.arglist[argcnt].ident=iVARARGS;
          sym->dim.arglist[argcnt].hasdefault=FALSE;
          sym->dim.arglist[argcnt].defvalue.val=0;
          sym->dim.arglist[argcnt].defvalue_tag=0;
          sym->dim.arglist[argcnt].tag = decl.type.tag;
        } else {
          if (argcnt>oldargcnt || sym->dim.arglist[argcnt].ident!=iVARARGS)
            error(25);          /* function definition does not match prototype */
        } /* if */
        argcnt++;
        continue;
      }

      if (argcnt>=SP_MAX_CALL_ARGUMENTS)
        error(45);
      if (decl.name[0] == PUBLIC_CHAR)
        error(56, decl.name); /* function arguments cannot be public */

      if (decl.type.ident == iARRAY)
        decl.type.ident = iREFARRAY;
      /* Stack layout:
       *   base + 0*sizeof(cell)  == previous "base"
       *   base + 1*sizeof(cell)  == function return address
       *   base + 2*sizeof(cell)  == number of arguments
       *   base + 3*sizeof(cell)  == first argument of the function
       * So the offset of each argument is "(argcnt+3) * sizeof(cell)".
       */
      doarg(sym,&decl,(argcnt+3)*sizeof(cell),chkshadow,&arg);

//    if ((sym->usage & uPUBLIC) && arg.hasdefault)
//      error(59, decl.name); /* arguments of a public function may not have a default value */

      if ((sym->usage & uPROTOTYPED)==0) {
        /* redimension the argument list, add the entry */
        sym->dim.arglist=(arginfo*)realloc(sym->dim.arglist,(argcnt+2)*sizeof(arginfo));
        if (sym->dim.arglist==0)
          error(FATAL_ERROR_OOM);                 /* insufficient memory */
        memset(&sym->dim.arglist[argcnt+1],0,sizeof(arginfo));  /* keep the list terminated */
        sym->dim.arglist[argcnt]=arg;
      } else {
        /* check the argument with the earlier definition */
        if (argcnt>oldargcnt || !argcompare(&sym->dim.arglist[argcnt],&arg))
          error(181, arg.name);          /* function argument does not match prototype */
        /* may need to free default array argument and the tag list */
        if (arg.ident==iREFARRAY && arg.hasdefault)
          free(arg.defvalue.array.data);
      } /* if */
      argcnt++;
    } while (matchtoken(','));
    /* if the next token is not ",", it should be ")" */
    needtoken(')');
  } /* if */

  sym->usage|=uPROTOTYPED;
  errorset(sRESET,0);           /* reset error flag (clear the "panic mode")*/
  return argcnt;
}

/*  doarg       - declare one argument type
 *
 *  this routine is called from "declargs()" and adds an entry in the local
 *  symbol table for one argument.
 *
 *  "fpublic" indicates whether the function for this argument list is public.
 *  The arguments themselves are never public.
 */
static void doarg(symbol *fun, declinfo_t *decl, int offset, int chkshadow, arginfo *arg)
{
  symbol *argsym;
  int slength=0;
  typeinfo_t *type = &decl->type;

  // Otherwise we get very weird line number ranges, anything to the current fline.
  errorset(sEXPRMARK,0);

  strcpy(arg->name, decl->name);
  arg->hasdefault=FALSE;        /* preset (most common case) */
  arg->defvalue.val=0;          /* clear */
  arg->defvalue_tag=0;
  arg->numdim=0;
  if (type->ident == iREFARRAY) {
    arg->numdim = type->numdim;
    memcpy(arg->dim, type->dim, sizeof(int) * type->numdim);
    memcpy(arg->idxtag, type->idxtag, sizeof(int) * type->numdim);
    if (type->tag == pc_tag_string) {
      slength = arg->dim[arg->numdim - 1];
      arg->dim[arg->numdim - 1] = (type->size + sizeof(cell) - 1) / sizeof(cell);
    }
    if (matchtoken('=')) {
      assert(litidx==0);        /* at the start of a function, this is reset */
      /* Check if there is a symbol */
      if (matchtoken(tSYMBOL)) {
        symbol *sym;
        char *name;
        cell val;
        tokeninfo(&val,&name);
        if ((sym=findglb(name)) == NULL) {
          error(17, name);      /* undefined symbol */
        } else {
          arg->hasdefault=TRUE; /* argument as a default value */
          memset(&arg->defvalue, 0, sizeof(arg->defvalue));
          arg->defvalue.array.data=NULL;
          arg->defvalue.array.addr=sym->addr();
          arg->defvalue_tag=sym->tag;
          if (sc_status==statWRITE && (sym->usage & uREAD)==0) {
            markusage(sym, uREAD);
          }
        }
      } else {
        if (type->is_new && !type->has_postdims && lexpeek('{')) {
          // Dynamic array with fixed initializer.
          error(160);
        }
        initials2(type->ident, type->tag, &type->size, arg->dim, arg->numdim, type->enumroot, 1, 0);
        assert(type->size >= litidx);
        /* allocate memory to hold the initial values */
        arg->defvalue.array.data=(cell *)malloc(litidx*sizeof(cell));
        if (arg->defvalue.array.data!=NULL) {
          int i;
          memcpy(arg->defvalue.array.data,litq,litidx*sizeof(cell));
          arg->hasdefault=TRUE;   /* argument has default value */
          arg->defvalue.array.size=litidx;
          arg->defvalue.array.addr=-1;
          /* calulate size to reserve on the heap */
          arg->defvalue.array.arraysize=1;
          for (i=0; i<arg->numdim; i++)
            arg->defvalue.array.arraysize*=arg->dim[i];
          if (arg->defvalue.array.arraysize < arg->defvalue.array.size)
            arg->defvalue.array.arraysize = arg->defvalue.array.size;
        } /* if */
        litidx=0;                 /* reset */
      }
    } else {
      if (type->is_new && type->has_postdims) {
        for (int i = 0; i < type->numdim; i++) {
          if (type->dim[i] <= 0) {
            // Fixed-array with unknown size.
            error(159);
            break;
          }
        }
      }
    }
  } else {
    if (matchtoken('=')) {
      assert(type->ident==iVARIABLE || type->ident==iREFERENCE);
      arg->hasdefault=TRUE;     /* argument has a default value */
      exprconst(&arg->defvalue.val,&arg->defvalue_tag,NULL);
      matchtag(type->tag, arg->defvalue_tag, TRUE);
    } /* if */
  } /* if */
  arg->ident=(char)type->ident;
  arg->usage=type->usage;
  arg->tag = type->tag;
  argsym=findloc(decl->name);
  if (argsym!=NULL) {
    error(21, decl->name);      /* symbol already defined */
  } else {
    if (chkshadow && (argsym=findglb(decl->name))!=NULL && argsym->ident!=iFUNCTN)
      error(219, decl->name);   /* variable shadows another symbol */
    /* add details of type and address */
    argsym=addvariable2(decl->name,offset,type->ident,sLOCAL,type->tag,
                       arg->dim,arg->numdim,arg->idxtag,slength);
    argsym->compound=0;
    if (type->ident==iREFERENCE)
      argsym->usage|=uREAD;     /* because references are passed back */
    if (fun->usage & (uPUBLIC|uSTOCK|uCALLBACK))
      argsym->usage|=uREAD;     /* arguments of public functions are always "used" */
    if (type->usage & uCONST)
      argsym->usage|=uCONST;
  } /* if */

  errorset(sEXPRRELEASE,0);
}

static int count_referrers(symbol *entry)
{
  int i,count;

  count=0;
  for (i=0; i<entry->numrefers; i++)
    if (entry->refer[i]!=NULL)
      count++;
  return count;
}

/* Every symbol has a referrer list, that contains the functions that use
 * the symbol. Now, if function "apple" is accessed by functions "banana" and
 * "citron", but neither function "banana" nor "citron" are used by anyone
 * else, then, by inference, function "apple" is not used either.
 */
static void reduce_referrers(symbol *root)
{
  int i,restart;
  symbol *sym,*ref;

  do {
    restart=0;
    for (sym=root->next; sym!=NULL; sym=sym->next) {
      if (sym->parent!=NULL)
        continue;                 /* hierarchical data type */
      if (sym->ident==iFUNCTN
          && (sym->usage & uNATIVE)==0
          && (sym->usage & uPUBLIC)==0 && strcmp(sym->name,uMAINFUNC)!=0
          && count_referrers(sym)==0)
      {
        sym->usage&=~(uREAD | uWRITTEN);  /* erase usage bits if there is no referrer */
        /* find all symbols that are referred by this symbol */
        for (ref=root->next; ref!=NULL; ref=ref->next) {
          if (ref->parent!=NULL)
            continue;             /* hierarchical data type */
          assert(ref->refer!=NULL);
          for (i=0; i<ref->numrefers && ref->refer[i]!=sym; i++)
            /* nothing */;
          if (i<ref->numrefers) {
            assert(ref->refer[i]==sym);
            ref->refer[i]=NULL;
            restart++;
          } /* if */
        } /* for */
      } else if ((sym->ident==iVARIABLE || sym->ident==iARRAY)
                 && (sym->usage & uPUBLIC)==0
                 && sym->parent==NULL
                 && count_referrers(sym)==0)
      {
        sym->usage&=~(uREAD | uWRITTEN);  /* erase usage bits if there is no referrer */
      } /* if */
    } /* for */
    /* after removing a symbol, check whether more can be removed */
  } while (restart>0);
}

/*  testsymbols - test for unused local or global variables
 *
 *  "Public" functions are excluded from the check, since these
 *  may be exported to other object modules.
 *  Labels are excluded from the check if the argument 'testlabs'
 *  is 0. Thus, labels are not tested until the end of the function.
 *  Constants may also be excluded (convenient for global constants).
 *
 *  When the nesting level drops below "level", the check stops.
 *
 *  The function returns whether there is an "entry" point for the file.
 *  This flag will only be 1 when browsing the global symbol table.
 */
static int testsymbols(symbol *root,int level,int testlabs,int testconst)
{
  char symname[2*sNAMEMAX+16];
  int entry=FALSE;

  symbol *sym=root->next;
  symbol *parent;
  while (sym!=NULL) {
    if (sym->compound < level) {
      parent = sym->parent;
      if (parent == NULL || (parent->ident != iARRAY && parent->ident != iREFARRAY))
        break;
      /* This is one dimension of a multidimensional array. Find the top symbol. */
      while (parent->parent != NULL && (parent->parent->ident == iARRAY || parent->parent->ident == iREFARRAY)) {
        parent = parent->parent;
      }
      /* Only the top symbol gets the compound level set. */
      if (parent->compound < level)
        break;
    }
    switch (sym->ident) {
    case iLABEL:
      if (testlabs) {
        if ((sym->usage & uDEFINE)==0) {
          error(19,sym->name);      /* not a label: ... */
        } else if ((sym->usage & uREAD)==0) {
          errorset(sSETFILE,sym->fnumber);
          errorset(sSETLINE,sym->lnumber);
          error(203,sym->name);     /* symbol isn't used: ... */
        } /* if */
      } /* if */
      break;
    case iFUNCTN:
      if ((sym->usage & (uDEFINE | uREAD | uNATIVE | uSTOCK | uPUBLIC))==uDEFINE) {
        funcdisplayname(symname,sym->name);
        if (strlen(symname)>0) {
          errorset(sSETFILE,sym->fnumber);
          errorset(sSETLINE,sym->lnumber);
          error(203,symname);       /* symbol isn't used ... (and not public/native/stock) */
        }
      } /* if */
      if ((sym->usage & uPUBLIC)!=0 || strcmp(sym->name,uMAINFUNC)==0)
        entry=TRUE;                 /* there is an entry point */
      /* also mark the function to the debug information */
      if (((sym->usage & uREAD)!=0 || (sym->usage & uPUBLIC)!=0) && (sym->usage & uNATIVE)==0)
        insert_dbgsymbol(sym);
      break;
    case iCONSTEXPR:
      if (testconst && (sym->usage & uREAD)==0) {
        errorset(sSETFILE,sym->fnumber);
        errorset(sSETLINE,sym->lnumber);
        error(203,sym->name);       /* symbol isn't used: ... */
      } /* if */
      break;
    case iMETHODMAP:
      // Ignore usage on methodmaps.
      break;
    default:
      /* a variable */
      if (sym->parent!=NULL)
        break;                      /* hierarchical data type */
      if ((sym->usage & (uWRITTEN | uREAD | uSTOCK))==0) {
        errorset(sSETFILE,sym->fnumber);
        errorset(sSETLINE,sym->lnumber);
        error(203,sym->name);  /* symbol isn't used (and not stock) */
      } else if ((sym->usage & (uREAD | uSTOCK | uPUBLIC))==0) {
        errorset(sSETFILE,sym->fnumber);
        errorset(sSETLINE,sym->lnumber);
        error(204,sym->name);       /* value assigned to symbol is never used */
#if 0 // ??? not sure whether it is a good idea to force people use "const"
      } else if ((sym->usage & (uWRITTEN | uPUBLIC | uCONST))==0 && sym->ident==iREFARRAY) {
        errorset(sSETFILE,sym->fnumber);
        errorset(sSETLINE,sym->lnumber);
        error(214,sym->name);       /* make array argument "const" */
#endif
      } /* if */
      /* also mark the variable (local or global) to the debug information */
      if ((sym->usage & (uWRITTEN | uREAD))!=0 && (sym->usage & uNATIVE)==0)
        insert_dbgsymbol(sym);
    } /* if */
    sym=sym->next;
  } /* while */

  errorset(sEXPRRELEASE, 0); /* clear error data */
  errorset(sRESET, 0);
  return entry;
}

static cell calc_array_datasize(symbol *sym, cell *offset)
{
  cell length;

  assert(sym!=NULL);
  assert(sym->ident==iARRAY || sym->ident==iREFARRAY);
  length=sym->dim.array.length;
  if (sym->dim.array.level > 0) {
    cell sublength=calc_array_datasize(finddepend(sym),offset);
    if (offset!=NULL)
      *offset=length*(*offset+sizeof(cell));
    if (sublength>0)
      length*=length*sublength;
    else
      length=0;
  } else {
    if (offset!=NULL)
      *offset=0;
  } /* if */
  return length;
}

static void destructsymbols(symbol *root,int level)
{
  cell offset=0;
  int savepri=FALSE;
  symbol *sym=root->next;
  while (sym!=NULL && get_actual_compound(sym)>=level) {
    if (sym->ident==iVARIABLE || sym->ident==iARRAY) {
      char symbolname[16];
      symbol *opsym;
      cell elements;
      /* check that the '~' operator is defined for this tag */
      operator_symname(symbolname,"~",sym->tag,0,1,0);
      if ((opsym=findglb(symbolname))!=NULL) {
        /* save PRI, in case of a return statment */
        if (!savepri) {
          pushreg(sPRI);        /* right-hand operand is in PRI */
          savepri=TRUE;
        } /* if */
        /* if the variable is an array, get the number of elements */
        if (sym->ident==iARRAY) {
          elements=calc_array_datasize(sym,&offset);
          /* "elements" can be zero when the variable is declared like
           *    new mytag: myvar[2][] = { {1, 2}, {3, 4} }
           * one should declare all dimensions!
           */
          if (elements==0)
            error(46,sym->name);        /* array size is unknown */
        } else {
          elements=1;
          offset=0;
        } /* if */
        pushval(elements);
        /* call the '~' operator */
        address(sym,sPRI);
        addconst(offset);       /* add offset to array data to the address */
        pushreg(sPRI);
        assert(opsym->ident==iFUNCTN);
        ffcall(opsym,NULL,2);
        if (sc_status!=statSKIP)
          markusage(opsym,uREAD);   /* do not mark as "used" when this call itself is skipped */
        if ((opsym->usage & uNATIVE)!=0 && opsym->x.lib!=NULL)
          opsym->x.lib->value += 1; /* increment "usage count" of the library */
      } /* if */
    } /* if */
    sym=sym->next;
  } /* while */
  /* restore PRI, if it was saved */
  if (savepri)
    popreg(sPRI);
}

static constvalue *insert_constval(constvalue *prev,constvalue *next,const char *name,cell val,
                                   int index)
{
  constvalue *cur;

  if ((cur=(constvalue*)malloc(sizeof(constvalue)))==NULL)
    error(FATAL_ERROR_OOM);       /* insufficient memory (fatal error) */
  memset(cur,0,sizeof(constvalue));
  if (name!=NULL) {
    assert(strlen(name)<=sNAMEMAX);
    strcpy(cur->name,name);
  } /* if */
  cur->value=val;
  cur->index=index;
  cur->next=next;
  prev->next=cur;
  return cur;
}

constvalue *append_constval(constvalue *table,const char *name,cell val,int index)
{
  constvalue *cur,*prev;

  /* find the end of the constant table */
  for (prev=table, cur=table->next; cur!=NULL; prev=cur, cur=cur->next)
    /* nothing */;
  return insert_constval(prev,NULL,name,val,index);
}

constvalue *find_constval(constvalue *table,char *name,int index)
{
  constvalue *ptr = table->next;

  while (ptr!=NULL) {
    if (strcmp(name,ptr->name)==0 && ptr->index==index)
      return ptr;
    ptr=ptr->next;
  } /* while */
  return NULL;
}

static constvalue *find_constval_byval(constvalue *table,cell val)
{
  constvalue *ptr = table->next;

  while (ptr!=NULL) {
    if (ptr->value==val)
      return ptr;
    ptr=ptr->next;
  } /* while */
  return NULL;
}

#if 0   /* never used */
static int delete_constval(constvalue *table,char *name)
{
  constvalue *prev = table;
  constvalue *cur = prev->next;

  while (cur!=NULL) {
    if (strcmp(name,cur->name)==0) {
      prev->next=cur->next;
      free(cur);
      return TRUE;
    } /* if */
    prev=cur;
    cur=cur->next;
  } /* while */
  return FALSE;
}
#endif

void delete_consttable(constvalue *table)
{
  constvalue *cur=table->next, *next;

  while (cur!=NULL) {
    next=cur->next;
    free(cur);
    cur=next;
  } /* while */
  memset(table,0,sizeof(constvalue));
}

/*  add_constant
 *
 *  Adds a symbol to the symbol table. Returns NULL on failure.
 */
symbol *add_constant(const char *name,cell val,int vclass,int tag)
{
  symbol *sym;

  /* Test whether a global or local symbol with the same name exists. Since
   * constants are stored in the symbols table, this also finds previously
   * defind constants. */
  sym=findglb(name);
  if (!sym)
    sym=findloc(name);
  if (sym) {
    int redef=0;
    if (sym->ident!=iCONSTEXPR)
      redef=1;                  /* redefinition a function/variable to a constant is not allowed */
    if ((sym->usage & uENUMFIELD)!=0) {
      /* enum field, special case if it has a different tag and the new symbol is also an enum field */
      constvalue *tagid;
      symbol *tagsym;
      if (sym->tag==tag)
        redef=1;                /* enumeration field is redefined (same tag) */
      tagid=find_tag_byval(tag);
      if (tagid==NULL) {
        redef=1;                /* new constant does not have a tag */
      } else {
        tagsym=findconst(tagid->name,NULL);
        if (tagsym==NULL || (tagsym->usage & uENUMROOT)==0)
          redef=1;              /* new constant is not an enumeration field */
      } /* if */
      /* in this particular case (enumeration field that is part of a different
       * enum, and non-conflicting with plain constants) we want to be able to
       * redefine it
       */
      if (!redef)
        goto redef_enumfield;
    } else if (!compare_tag(sym->tag, tag)) {
      redef=1;                  /* redefinition of a constant (non-enum) to a different tag is not allowed */
    } /* if */
    if (redef) {
      error(21,name);           /* symbol already defined */
      return NULL;
    } else if (sym->addr()!=val) {
      error(201,name);          /* redefinition of constant (different value) */
      sym->setAddr(val);        /* set new value */
    } /* if */
    /* silently ignore redefinitions of constants with the same value & tag */
    return sym;
  } /* if */

  /* constant doesn't exist yet (or is allowed to be redefined) */
redef_enumfield:
  sym=addsym(name,val,iCONSTEXPR,vclass,tag,uDEFINE);
  assert(sym!=NULL);            /* fatal error 103 must be given on error */
  if (sc_status == statIDLE)
    sym->usage |= uPREDEF;
  return sym;
}

/*  statement           - The Statement Parser
 *
 *  This routine is called whenever the parser needs to know what statement
 *  it encounters (i.e. whenever program syntax requires a statement).
 */
static void statement(int *lastindent,int allow_decl)
{
  int tok,save;
  cell val;
  char *st;

  if (!freading) {
    error(36);                  /* empty statement */
    return;
  } /* if */
  errorset(sRESET,0);

  tok=lex(&val,&st);
  if (tok!='{') {
    insert_dbgline(fline);
    setline(TRUE);
  } /* if */
  /* lex() has set stmtindent */
  if (lastindent!=NULL && tok!=tLABEL) {
    if (*lastindent>=0 && *lastindent!=stmtindent && !indent_nowarn && sc_tabsize>0)
      error(217);               /* loose indentation */
    *lastindent=stmtindent;
    indent_nowarn=FALSE;        /* if warning was blocked, re-enable it */
  } /* if */

  if (tok == tSYMBOL) {
    // We reaaaally don't have enough lookahead for this, so we cheat and try
    // to determine whether this is probably a declaration.
    int is_decl = FALSE;
    if (matchtoken('[')) {
      if (lexpeek(']'))
        is_decl = TRUE;
      lexpush();
    } else if (lexpeek(tSYMBOL)) {
      is_decl = TRUE;
    }

    if (is_decl) {
      if (!allow_decl) {
        error(3);
        return;
      }
      lexpush();
      autozero = TRUE;
      lastst = tNEW;
      declloc(tNEWDECL);
      return;
    }
  }

  switch (tok) {
  case 0:
    /* nothing */
    break;
  case tINT:
  case tVOID:
  case tCHAR:
  case tOBJECT:
    lexpush();
    // Fall-through.
  case tDECL:
  case tSTATIC:
  case tNEW:
    if (!allow_decl) {
      error(3);
      break;
    }
    autozero = (tok != tDECL);
    lastst = (tok == tDECL) ? tDECL : tNEW;
    declloc(tok);
    break;
  case tDELETE:
    dodelete();
    lastst=tDELETE;
    break;
  case '{':
  case tBEGIN:
    save=fline;
    if (!matchtoken('}')) {       /* {} is the empty statement */
      compound(save==fline,tok);
    } else {
      lastst = tEMPTYBLOCK;
    }
    /* lastst (for "last statement") does not change 
       you're not my father, don't tell me what to do */
    break;
  case ';':
    error(36);                  /* empty statement */
    break;
  case tIF:
    lastst=doif();
    break;
  case tWHILE:
    lastst=dowhile();
    break;
  case tDO:
    lastst=dodo();
    break;
  case tFOR:
    lastst=dofor();
    break;
  case tSWITCH:
    doswitch();
    lastst=tSWITCH;
    break;
  case tCASE:
  case tDEFAULT:
    error(14);     /* not in switch */
    break;
  case tRETURN:
    doreturn();
    lastst=tRETURN;
    break;
  case tBREAK:
    dobreak();
    lastst=tBREAK;
    break;
  case tCONTINUE:
    docont();
    lastst=tCONTINUE;
    break;
  case tEXIT:
    doexit();
    lastst=tEXIT;
    break;
  case tASSERT:
    doassert();
    lastst=tASSERT;
    break;
  case tSLEEP:
    dosleep();
    lastst=tSLEEP;
    break;
  case tCONST:
    decl_const(sLOCAL);
    break;
  case tENUM:
    decl_enum(sLOCAL);
    break;
  default:          /* non-empty expression */
    lexpush();      /* analyze token later */
    doexpr(TRUE,TRUE,TRUE,TRUE,NULL,NULL,FALSE);
    needtoken(tTERM);
    lastst=tEXPR;
  } /* switch */
}

static void compound(int stmt_sameline,int starttok)
{
  int indent=-1;
  cell save_decl=declared;
  int count_stmt=0;
  int block_start=fline;  /* save line where the compound block started */
  int endtok;

  pushstacklist();
  pushheaplist();
  /* if there is more text on this line, we should adjust the statement indent */
  if (stmt_sameline) {
    int i;
    const unsigned char *p=lptr;
    /* go back to the opening brace */
    while (*p!=starttok) {
      assert(p>pline);
      p--;
    } /* while */
    assert(*p==starttok);  /* it should be found */
    /* go forward, skipping white-space */
    p++;
    while (*p<=' ' && *p!='\0')
      p++;
    assert(*p!='\0'); /* a token should be found */
    stmtindent=0;
    for (i=0; i<(int)(p-pline); i++)
      if (pline[i]=='\t' && sc_tabsize>0)
        stmtindent += (int)(sc_tabsize - (stmtindent+sc_tabsize) % sc_tabsize);
      else
        stmtindent++;
  } /* if */

  endtok=(starttok=='{') ? '}' : tEND;
  nestlevel+=1;                 /* increase compound statement level */
  while (matchtoken(endtok)==0){/* repeat until compound statement is closed */
    if (!freading){
      error(30,block_start);    /* compound block not closed at end of file */
      break;
    } else {
      if (count_stmt>0 && (lastst==tRETURN || lastst==tBREAK || lastst==tCONTINUE || lastst==tENDLESS))
        error(225);             /* unreachable code */
      statement(&indent,TRUE);  /* do a statement */
      count_stmt++;
    } /* if */
  } /* while */
  if (lastst!=tRETURN)
    destructsymbols(&loctab,nestlevel);
  if (lastst!=tRETURN) {
    popheaplist();
    popstacklist(1);
  } else {
    popheaplist();
    popstacklist(0);
  }
  testsymbols(&loctab,nestlevel,FALSE,TRUE);        /* look for unused block locals */
  declared=save_decl;
  delete_symbols(&loctab,nestlevel,FALSE,TRUE);     /* erase local symbols, but
                                                     * retain block local labels
                                                     * (within the function) */
  nestlevel-=1;                 /* decrease compound statement level */
}

/*  doexpr
 *
 *  Global references: stgidx   (referred to only)
 */
static int doexpr(int comma,int chkeffect,int allowarray,int mark_endexpr,
                  int *tag,symbol **symptr,int chkfuncresult)
{
  return doexpr2(comma,chkeffect,allowarray,mark_endexpr,tag,symptr,chkfuncresult,NULL);
}

/*  doexpr2
 *
 *  Global references: stgidx   (referred to only)
 */
static int doexpr2(int comma,int chkeffect,int allowarray,int mark_endexpr,
                  int *tag,symbol **symptr,int chkfuncresult,value *lval)
{
  int index,ident;
  int localstaging=FALSE;
  cell val;

  if (!staging) {
    stgset(TRUE);               /* start stage-buffering */
    localstaging=TRUE;
    assert(stgidx==0);
  } /* if */
  index=stgidx;
  errorset(sEXPRMARK,0);
  do {
    /* on second round through, mark the end of the previous expression */
    if (index!=stgidx)
      markexpr(sEXPR,NULL,0);
    sideeffect=FALSE;
    ident=expression(&val,tag,symptr,chkfuncresult,lval);
    if (!allowarray && (ident==iARRAY || ident==iREFARRAY))
      error(33,"-unknown-");    /* array must be indexed */
    if (chkeffect && !sideeffect)
      error(215);               /* expression has no effect */
  } while (comma && matchtoken(',')); /* more? */
  if (mark_endexpr)
    markexpr(sEXPR,NULL,0);     /* optionally, mark the end of the expression */
  errorset(sEXPRRELEASE,0);
  if (localstaging) {
    stgout(index);
    stgset(FALSE);              /* stop staging */
  } /* if */
  return ident;
}

/*  exprconst
 */
int exprconst(cell *val,int *tag,symbol **symptr)
{
  int ident,index;
  cell cidx;

  stgset(TRUE);         /* start stage-buffering */
  stgget(&index,&cidx); /* mark position in code generator */
  errorset(sEXPRMARK,0);
  ident=expression(val,tag,symptr,FALSE,NULL);
  stgdel(index,cidx);   /* scratch generated code */
  stgset(FALSE);        /* stop stage-buffering */
  if (ident!=iCONSTEXPR) {
    error(8);           /* must be constant expression */
    if (val!=NULL)
      *val=0;
    if (tag!=NULL)
      *tag=0;
    if (symptr!=NULL)
      *symptr=NULL;
  } /* if */
  errorset(sEXPRRELEASE,0);
  return (ident==iCONSTEXPR);
}

/*  test
 *
 *  In the case a "simple assignment" operator ("=") is used within a test,
 *  the warning "possibly unintended assignment" is displayed. This routine
 *  sets the global variable "sc_intest" to true, it is restored upon termination.
 *  In the case the assignment was intended, use parentheses around the
 *  expression to avoid the warning; primary() sets "sc_intest" to 0.
 *
 *  Global references: sc_intest (altered, but restored upon termination)
 */
static int test(int label,int parens,int invert)
{
  int index,tok;
  cell cidx;
  int ident,tag;
  int endtok;
  cell constval;
  symbol *sym;
  int localstaging=FALSE;

  if (!staging) {
    stgset(TRUE);               /* start staging */
    localstaging=TRUE;
    #if !defined NDEBUG
      stgget(&index,&cidx);     /* should start at zero if started locally */
      assert(index==0);
    #endif
  } /* if */

  PUSHSTK_I(sc_intest);
  sc_intest=TRUE;
  endtok=0;
  if (parens!=TEST_PLAIN) {
    if (matchtoken('('))
      endtok=')';
    else if (parens==TEST_THEN)
      endtok=tTHEN;
    else if (parens==TEST_DO)
      endtok=tDO;
  } /* if */
  do {
    stgget(&index,&cidx);       /* mark position (of last expression) in
                                 * code generator */
    ident=expression(&constval,&tag,&sym,TRUE,NULL);
    tok=matchtoken(',');
    if (tok)
      markexpr(sEXPR,NULL,0);
  } while (tok); /* do */
  if (endtok!=0)
    needtoken(endtok);
  if (ident==iARRAY || ident==iREFARRAY) {
    if (sym)
      error(33, sym->name); /* array must be indexed */
    else
      error(29);                /* invalid expression */
  } /* if */
  if (ident==iCONSTEXPR) {      /* constant expression */
    int testtype=0;
    sc_intest=(short)POPSTK_I();/* restore stack */
    stgdel(index,cidx);
    if (constval) {             /* code always executed */
      error(206);               /* redundant test: always non-zero */
      testtype=tENDLESS;
    } else {
      error(205);               /* redundant code: never executed */
      jumplabel(label);
    } /* if */
    if (localstaging) {
      stgout(0);                /* write "jumplabel" code */
      stgset(FALSE);            /* stop staging */
    } /* if */
    return testtype;
  } /* if */
  if (tag!=0 && tag!=pc_tag_bool) {
    if (check_userop(lneg,tag,0,1,NULL,&tag))
      invert= !invert;          /* user-defined ! operator inverted result */
  }
  if (invert)
    jmp_ne0(label);             /* jump to label if true (different from 0) */
  else
    jmp_eq0(label);             /* jump to label if false (equal to 0) */
  markexpr(sEXPR,NULL,0);       /* end expression (give optimizer a chance) */
  sc_intest=(short)POPSTK_I();  /* double typecast to avoid warning with Microsoft C */
  if (localstaging) {
    stgout(0);                  /* output queue from the very beginning (see
                                 * assert() when localstaging is set to TRUE) */
    stgset(FALSE);              /* stop staging */
  } /* if */
  return 0;
}

static int doif(void)
{
  int flab1,flab2;
  int ifindent;
  int lastst_true;

  ifindent=stmtindent;          /* save the indent of the "if" instruction */
  flab1=getlabel();             /* get label number for false branch */
  test(flab1,TEST_THEN,FALSE);  /* get expression, branch to flab1 if false */
  statement(NULL,FALSE);        /* if true, do a statement */
  if (!matchtoken(tELSE)) {     /* if...else ? */
    setlabel(flab1);            /* no, simple if..., print false label */
  } else {
    lastst_true=lastst;         /* save last statement of the "true" branch */
    /* to avoid the "dangling else" error, we want a warning if the "else"
     * has a lower indent than the matching "if" */
    if (stmtindent<ifindent && sc_tabsize>0)
      error(217);               /* loose indentation */
    flab2=getlabel();
    if (lastst!=tRETURN)
      jumplabel(flab2);         /* "true" branch jumps around "else" clause, unless the "true" branch statement already jumped */
    setlabel(flab1);            /* print false label */
    statement(NULL,FALSE);      /* do "else" clause */
    setlabel(flab2);            /* print true label */
    /* if both the "true" branch and the "false" branch ended with the same
     * kind of statement, set the last statement id to that kind, rather than
     * to the generic tIF; this allows for better "unreachable code" checking
     */
    if (lastst==lastst_true)
      return lastst;
  } /* if */
  return tIF;
}

static int dowhile(void)
{
  int wq[wqSIZE];               /* allocate local queue */
  int save_endlessloop,retcode;

  save_endlessloop=endlessloop;
  addwhile(wq);                 /* add entry to queue for "break" */
  setlabel(wq[wqLOOP]);         /* loop label */
  /* The debugger uses the "break" opcode to be able to "break" out of
   * a loop. To make sure that each loop has a break opcode, even for the
   * tiniest loop, set it below the top of the loop
   */
  setline(TRUE);
  endlessloop=test(wq[wqEXIT],TEST_DO,FALSE);/* branch to wq[wqEXIT] if false */
  statement(NULL,FALSE);        /* if so, do a statement */
  jumplabel(wq[wqLOOP]);        /* and loop to "while" start */
  setlabel(wq[wqEXIT]);         /* exit label */
  delwhile();                   /* delete queue entry */

  retcode=endlessloop ? tENDLESS : tWHILE;
  endlessloop=save_endlessloop;
  return retcode;
}

/*
 *  Note that "continue" will in this case not jump to the top of the loop, but
 *  to the end: just before the TRUE-or-FALSE testing code.
 */
static int dodo(void)
{
  int wq[wqSIZE],top;
  int save_endlessloop,retcode;

  save_endlessloop=endlessloop;
  addwhile(wq);           /* see "dowhile" for more info */
  top=getlabel();         /* make a label first */
  setlabel(top);          /* loop label */
  statement(NULL,FALSE);
  needtoken(tWHILE);
  setlabel(wq[wqLOOP]);   /* "continue" always jumps to WQLOOP. */
  setline(TRUE);
  endlessloop=test(wq[wqEXIT],TEST_OPT,FALSE);
  jumplabel(top);
  setlabel(wq[wqEXIT]);
  delwhile();
  needtoken(tTERM);

  retcode=endlessloop ? tENDLESS : tDO;
  endlessloop=save_endlessloop;
  return retcode;
}

static int dofor(void)
{
  int wq[wqSIZE],skiplab;
  cell save_decl;
  int save_nestlevel,save_endlessloop;
  int index,endtok;
  int *ptr;

  save_decl=declared;
  save_nestlevel=nestlevel;
  save_endlessloop=endlessloop;
  pushstacklist();
  pushheaplist();
  
  addwhile(wq);
  skiplab=getlabel();
  endtok= matchtoken('(') ? ')' : tDO;
  if (matchtoken(';')==0) {
    /* new variable declarations are allowed here */
    token_t tok;

    switch (lextok(&tok)) {
      case tINT:
      case tCHAR:
      case tOBJECT:
      case tVOID:
        lexpush();
        // Fallthrough.
      case tNEW:
        /* The variable in expr1 of the for loop is at a
         * 'compound statement' level of it own.
         */
        nestlevel++;
        autozero=1;
        declloc(tok.id); /* declare local variable */
        break;
      case tSYMBOL:
      {
        // See comment in statement() near tSYMBOL.
        int is_decl = FALSE;
        if (matchtoken('[')) {
          if (lexpeek(']'))
            is_decl = TRUE;
          lexpush();
        } else if (lexpeek(tSYMBOL)) {
          is_decl = TRUE;
        }

        if (is_decl) {
          lexpush();
          nestlevel++;
          autozero=1;
          declloc(tSYMBOL);
          break;
        }

        // Fall-through to default!
      }
      default:
        lexpush();
        doexpr(TRUE,TRUE,TRUE,TRUE,NULL,NULL,FALSE);  /* expression 1 */
        needtoken(';');
        break;
    }
  } /* if */
  /* Adjust the "declared" field in the "while queue", in case that
   * local variables were declared in the first expression of the
   * "for" loop. These are deleted in separately, so a "break" or a "continue"
   * must ignore these fields.
   */
  ptr=readwhile();
  assert(ptr!=NULL);
  /*ptr[wqBRK]=(int)declared;
   *ptr[wqCONT]=(int)declared;
   */
  ptr[wqBRK] = stackusage->list_id;
  ptr[wqCONT] = stackusage->list_id;
  jumplabel(skiplab);               /* skip expression 3 1st time */
  setlabel(wq[wqLOOP]);             /* "continue" goes to this label: expr3 */
  setline(TRUE);
  /* Expressions 2 and 3 are reversed in the generated code: expression 3
   * precedes expression 2. When parsing, the code is buffered and marks for
   * the start of each expression are insterted in the buffer.
   */
  assert(!staging);
  stgset(TRUE);                     /* start staging */
  assert(stgidx==0);
  index=stgidx;
  stgmark(sSTARTREORDER);
  stgmark((char)(sEXPRSTART+0));    /* mark start of 2nd expression in stage */
  setlabel(skiplab);                /* jump to this point after 1st expression */
  if (matchtoken(';')) {
    endlessloop=1;
  } else {
    endlessloop=test(wq[wqEXIT],TEST_PLAIN,FALSE);/* expression 2 (jump to wq[wqEXIT] if false) */
    needtoken(';');
  } /* if */
  stgmark((char)(sEXPRSTART+1));    /* mark start of 3th expression in stage */
  if (!matchtoken(endtok)) {
    doexpr(TRUE,TRUE,TRUE,TRUE,NULL,NULL,FALSE);    /* expression 3 */
    needtoken(endtok);
  } /* if */
  stgmark(sENDREORDER);             /* mark end of reversed evaluation */
  stgout(index);
  stgset(FALSE);                    /* stop staging */
  statement(NULL,FALSE);
  jumplabel(wq[wqLOOP]);
  setlabel(wq[wqEXIT]);
  delwhile();

  popheaplist();

  assert(nestlevel>=save_nestlevel);
  if (nestlevel>save_nestlevel) {
    /* Clean up the space and the symbol table for the local
     * variable in "expr1".
     */
    destructsymbols(&loctab,nestlevel);
    popstacklist(1);
    testsymbols(&loctab,nestlevel,FALSE,TRUE);  /* look for unused block locals */
    declared=save_decl;
    delete_symbols(&loctab,nestlevel,FALSE,TRUE);
    nestlevel=save_nestlevel;     /* reset 'compound statement' nesting level */
  } else {
    popstacklist(0);
  } /* if */

  index=endlessloop ? tENDLESS : tFOR;
  endlessloop=save_endlessloop;
  return index;
}

/* The switch statement is incompatible with its C sibling:
 * 1. the cases are not drop through
 * 2. only one instruction may appear below each case, use a compound
 *    instruction to execute multiple instructions
 * 3. the "case" keyword accepts a comma separated list of values to
 *    match
 *
 * SWITCH param
 *   PRI = expression result
 *   param = table offset (code segment)
 *
 */
static void doswitch(void)
{
  int lbl_table,lbl_exit,lbl_case;
  int swdefault,casecount;
  int tok,endtok;
  cell val;
  char *str;
  constvalue caselist = { NULL, "", 0, 0};   /* case list starts empty */
  constvalue *cse,*csp;
  char labelname[sNAMEMAX+1];

  endtok= matchtoken('(') ? ')' : tDO;
  doexpr(TRUE,FALSE,FALSE,FALSE,NULL,NULL,TRUE);/* evaluate switch expression */
  needtoken(endtok);
  /* generate the code for the switch statement, the label is the address
   * of the case table (to be generated later).
   */
  lbl_table=getlabel();
  lbl_case=0;                   /* just to avoid a compiler warning */
  ffswitch(lbl_table);

  if (matchtoken(tBEGIN)) {
    endtok=tEND;
  } else {
    endtok='}';
    needtoken('{');
  } /* if */
  lbl_exit=getlabel();          /* get label number for jumping out of switch */
  swdefault=FALSE;
  casecount=0;
  do {
    tok=lex(&val,&str);         /* read in (new) token */
    switch (tok) {
    case tCASE:
      if (swdefault!=FALSE)
        error(15);        /* "default" case must be last in switch statement */
      lbl_case=getlabel();
      PUSHSTK_I(sc_allowtags);
      sc_allowtags=FALSE; /* do not allow tagnames here */
      do {
        casecount++;

        /* ??? enforce/document that, in a switch, a statement cannot start
         *     with a label. Then, you can search for:
         *     * the first semicolon (marks the end of a statement)
         *     * an opening brace (marks the start of a compound statement)
         *     and search for the right-most colon before that statement
         *     Now, by replacing the ':' by a special COLON token, you can
         *     parse all expressions until that special token.
         */

        exprconst(&val,NULL,NULL);
        /* Search the insertion point (the table is kept in sorted order, so
         * that advanced abstract machines can sift the case table with a
         * binary search). Check for duplicate case values at the same time.
         */
        for (csp=&caselist, cse=caselist.next;
             cse!=NULL && cse->value<val;
             csp=cse, cse=cse->next)
          /* nothing */;
        if (cse!=NULL && cse->value==val)
          error(40,val);                /* duplicate "case" label */
        /* Since the label is stored as a string in the "constvalue", the
         * size of an identifier must be at least 8, as there are 8
         * hexadecimal digits in a 32-bit number.
         */
        #if sNAMEMAX < 8
          #error Length of identifier (sNAMEMAX) too small.
        #endif
        assert(csp!=NULL);
        assert(csp->next==cse);
        insert_constval(csp,cse,itoh(lbl_case),val,0);
        if (matchtoken(tDBLDOT)) {
          error(1, ":", "..");
        } /* if */
      } while (matchtoken(','));
      needtoken(':');                   /* ':' ends the case */
      sc_allowtags=(short)POPSTK_I();   /* reset */
      setlabel(lbl_case);
      statement(NULL,FALSE);
      jumplabel(lbl_exit);
      break;
    case tDEFAULT:
      if (swdefault!=FALSE)
        error(16);         /* multiple defaults in switch */
      lbl_case=getlabel();
      setlabel(lbl_case);
      needtoken(':');
      swdefault=TRUE;
      statement(NULL,FALSE);
      /* Jump to lbl_exit, even thouh this is the last clause in the
       * switch, because the jump table is generated between the last
       * clause of the switch and the exit label.
       */
      jumplabel(lbl_exit);
      break;
    default:
      if (tok!=endtok) {
        error(2);
        indent_nowarn=TRUE; /* disable this check */
        tok=endtok;     /* break out of the loop after an error */
      } /* if */
    } /* switch */
  } while (tok!=endtok);

  #if !defined NDEBUG
    /* verify that the case table is sorted (unfortunatly, duplicates can
     * occur; there really shouldn't be duplicate cases, but the compiler
     * may not crash or drop into an assertion for a user error). */
    for (cse=caselist.next; cse!=NULL && cse->next!=NULL; cse=cse->next)
      assert(cse->value <= cse->next->value);
  #endif
  /* generate the table here, before lbl_exit (general jump target) */
  setlabel(lbl_table);
  assert(swdefault==FALSE || swdefault==TRUE);
  if (swdefault==FALSE) {
    /* store lbl_exit as the "none-matched" label in the switch table */
    strcpy(labelname,itoh(lbl_exit));
  } else {
    /* lbl_case holds the label of the "default" clause */
    strcpy(labelname,itoh(lbl_case));
  } /* if */
  ffcase(casecount,labelname,TRUE);
  /* generate the rest of the table */
  for (cse=caselist.next; cse!=NULL; cse=cse->next)
    ffcase(cse->value,cse->name,FALSE);

  setlabel(lbl_exit);
  delete_consttable(&caselist); /* clear list of case labels */
}

static void doassert(void)
{
  int flab1,index;
  cell cidx;

  if ((sc_debug & sCHKBOUNDS)!=0) {
    flab1=getlabel();           /* get label number for "OK" branch */
    test(flab1,TEST_PLAIN,TRUE);/* get expression and branch to flab1 if true */
    insert_dbgline(fline);      /* make sure we can find the correct line number */
    ffabort(xASSERTION);
    setlabel(flab1);
  } else {
    stgset(TRUE);               /* start staging */
    stgget(&index,&cidx);       /* mark position in code generator */
    do {
      expression(NULL,NULL,NULL,FALSE,NULL);
      stgdel(index,cidx);       /* just scrap the code */
    } while (matchtoken(','));
    stgset(FALSE);              /* stop staging */
  } /* if */
  needtoken(tTERM);
}

static int is_variadic(symbol *sym)
{
  assert(sym->ident==iFUNCTN);
  arginfo *arg = sym->dim.arglist;
  while (arg->ident) {
    if (arg->ident == iVARARGS)
      return TRUE;
    arg++;
  }
  return FALSE;
}

/*  doreturn
 *
 *  Global references: rettype  (altered)
 */
static void doreturn(void)
{
  int tag,ident;
  int level;
  symbol *sym,*sub;

  if (!matchtoken(tTERM)) {
    if (curfunc->tag == pc_tag_void)
      error(88);
    /* "return <value>" */
    if ((rettype & uRETNONE)!=0)
      error(78);                        /* mix "return;" and "return value;" */
    ident=doexpr(TRUE,FALSE,TRUE,FALSE,&tag,&sym,TRUE);
    needtoken(tTERM);
    if (ident==iARRAY && sym==NULL) {
      /* returning a literal string is not supported (it must be a variable) */
      error(39);
      ident=iCONSTEXPR;                 /* avoid handling an "array" case */
    } /* if */
    /* see if this function already has a sub type (an array attached) */
    sub=finddepend(curfunc);
    assert(sub==NULL || sub->ident==iREFARRAY);
    if ((rettype & uRETVALUE)!=0) {
      int retarray=(ident==iARRAY || ident==iREFARRAY);
      /* there was an earlier "return" statement in this function */
      if ((sub==NULL && retarray) || (sub!=NULL && !retarray))
        error(79);                      /* mixing "return array;" and "return value;" */
      if (retarray && (curfunc->usage & uPUBLIC)!=0)
        error(90,curfunc->name);        /* public function may not return array */
    } /* if */
    rettype|=uRETVALUE;                 /* function returns a value */
    /* check tagname with function tagname */
    assert(curfunc!=NULL);
    if (!matchtag_string(ident, tag))
      matchtag(curfunc->tag,tag,TRUE);
    if (ident==iARRAY || ident==iREFARRAY) {
      int dim[sDIMEN_MAX], numdim = 0;
      cell arraysize;
      assert(sym!=NULL);
      if (sub!=NULL) {
        assert(sub->ident==iREFARRAY);
        /* this function has an array attached already; check that the current
         * "return" statement returns exactly the same array
         */
        level=sym->dim.array.level;
        if (sub->dim.array.level!=level) {
          error(48);                    /* array dimensions must match */
        } else {
          for (numdim=0; numdim<=level; numdim++) {
            dim[numdim]=(int)sub->dim.array.length;
            if (sym->dim.array.length!=dim[numdim])
              error(47);    /* array sizes must match */
            if (numdim<level) {
              sym=finddepend(sym);
              sub=finddepend(sub);
              assert(sym!=NULL && sub!=NULL);
              /* ^^^ both arrays have the same dimensions (this was checked
               *     earlier) so the dependend should always be found
               */
            } /* if */
          } /* for */
        } /* if */
      } else {
        int idxtag[sDIMEN_MAX];
        int argcount, slength=0;
        /* this function does not yet have an array attached; clone the
         * returned symbol beneath the current function
         */
        sub=sym;
        assert(sub!=NULL);
        level=sub->dim.array.level;
        for (numdim=0; numdim<=level; numdim++) {
          dim[numdim]=(int)sub->dim.array.length;
          idxtag[numdim]=sub->x.tags.index;
          if (numdim<level) {
            sub=finddepend(sub);
            assert(sub!=NULL);
          } /* if */
          /* check that all dimensions are known */
          if (dim[numdim]<=0)
            error(46,sym->name);
        } /* for */
        if (sym->tag==pc_tag_string && numdim!=0)
          slength=dim[numdim-1];
        /* the address of the array is stored in a hidden parameter; the address
         * of this parameter is 1 + the number of parameters (times the size of
         * a cell) + the size of the stack frame and the return address
         *   base + 0*sizeof(cell)         == previous "base"
         *   base + 1*sizeof(cell)         == function return address
         *   base + 2*sizeof(cell)         == number of arguments
         *   base + 3*sizeof(cell)         == first argument of the function
         *   ...
         *   base + ((n-1)+3)*sizeof(cell) == last argument of the function
         *   base + (n+3)*sizeof(cell)     == hidden parameter with array address
         */
        assert(curfunc!=NULL);
        assert(curfunc->dim.arglist!=NULL);
        for (argcount=0; curfunc->dim.arglist[argcount].ident!=0; argcount++)
          /* nothing */;
        sub=addvariable2(curfunc->name,(argcount+3)*sizeof(cell),iREFARRAY,sGLOBAL,curfunc->tag,dim,numdim,idxtag,slength);
        sub->parent=curfunc;
      } /* if */
      /* get the hidden parameter, copy the array (the array is on the heap;
       * it stays on the heap for the moment, and it is removed -usually- at
       * the end of the expression/statement, see expression() in SC3.C)
       */
      if (is_variadic(curfunc)) {
        load_hidden_arg();
      } else {
        address(sub,sALT);           /* ALT = destination */
      }
      arraysize=calc_arraysize(dim,numdim,0);
      memcopy(arraysize*sizeof(cell));  /* source already in PRI */
      /* moveto1(); is not necessary, callfunction() does a popreg() */
    } /* if */
  } else {
    /* this return statement contains no expression */
    ldconst(0,sPRI);
    if ((rettype & uRETVALUE)!=0) {
      char symname[2*sNAMEMAX+16];      /* allow space for user defined operators */
      assert(curfunc!=NULL);
      funcdisplayname(symname,curfunc->name);
      error(209,symname);               /* function should return a value */
    } /* if */
    rettype|=uRETNONE;                  /* function does not return anything */
  } /* if */
  destructsymbols(&loctab,0);           /* call destructor for *all* locals */
  genheapfree(-1);
  genstackfree(-1);                     /* free everything on the stack */
  ffret();
}

static void dobreak(void)
{
  int *ptr;

  endlessloop=0;      /* if we were inside an endless loop, we just jumped out */
  ptr=readwhile();      /* readwhile() gives an error if not in loop */
  needtoken(tTERM);
  if (ptr==NULL)
    return;
  destructsymbols(&loctab,nestlevel);
  genstackfree(ptr[wqBRK]);
  jumplabel(ptr[wqEXIT]);
}

static void docont(void)
{
  int *ptr;

  ptr=readwhile();      /* readwhile() gives an error if not in loop */
  needtoken(tTERM);
  if (ptr==NULL)
    return;
  destructsymbols(&loctab,nestlevel);
  genstackfree(ptr[wqCONT]);
  genheapfree(ptr[wqCONT]);
  jumplabel(ptr[wqLOOP]);
}

static void doexit(void)
{
  int tag=0;

  if (matchtoken(tTERM)==0){
    doexpr(TRUE,FALSE,FALSE,TRUE,&tag,NULL,TRUE);
    needtoken(tTERM);
  } else {
    ldconst(0,sPRI);
  } /* if */
  ldconst(tag,sALT);
  destructsymbols(&loctab,0);           /* call destructor for *all* locals */
  ffabort(xEXIT);
}

static void dosleep(void)
{
  int tag=0;

  if (matchtoken(tTERM)==0){
    doexpr(TRUE,FALSE,FALSE,TRUE,&tag,NULL,TRUE);
    needtoken(tTERM);
  } else {
    ldconst(0,sPRI);
  } /* if */
  ldconst(tag,sALT);
  ffabort(xSLEEP);

  /* for stack usage checking, mark the use of the sleep instruction */
  pc_memflags |= suSLEEP_INSTR;
}

static void addwhile(int *ptr)
{
  int k;

  ptr[wqBRK]=stackusage->list_id;     /* stack pointer (for "break") */
  ptr[wqCONT]=stackusage->list_id;    /* for "continue", possibly adjusted later */
  ptr[wqLOOP]=getlabel();
  ptr[wqEXIT]=getlabel();
  if (wqptr>=(wq+wqTABSZ-wqSIZE))
    error(FATAL_ERROR_ALLOC_OVERFLOW,"loop table");    /* loop table overflow (too many active loops)*/
  k=0;
  while (k<wqSIZE){     /* copy "ptr" to while queue table */
    *wqptr=*ptr;
    wqptr+=1;
    ptr+=1;
    k+=1;
  } /* while */
}

static void delwhile(void)
{
  if (wqptr>wq)
    wqptr-=wqSIZE;
}

static int *readwhile(void)
{
  if (wqptr<=wq){
    error(24);          /* out of context */
    return NULL;
  } else {
    return (wqptr-wqSIZE);
  } /* if */
}

bool typeinfo_t::isCharArray() const
{
  return numdim == 1 && tag == pc_tag_string;
}
