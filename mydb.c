/*******************************************************************************

   These routines where writen in an attemp to give me more flexibility
   over the way I performed upgrades on the campus.

   Basically the final program enables me to get a list or to extract all files
   from a SGI distribution file that are aimed at a peticular CPU/GRAPHICS
   configuration for the IRIX machines.

   Althought this may be a precarious thing to do given the amount of
   informations I started with, I did it anyway, feeling that from the current
   discussion on the net about such topics, a similar capability would
   not be included in the installation software of IRIX (inst) in a short
   futur (and I have a lot of SGI/IRIX machines onder my wing,from challenges to
   4D/20 with all kinds of GFX configurations).

   usage: midb [-d dir] [-l] [-x] [-X] product mask description
          -d dir  : distribution directory
          -l      : list corresponding files
          -x      : extract corresponding files
          -X      : line 'x' put don't uncompress files
          product : product name ex: eoe1
          mask: mask of products to extract/list files from
                ex: eoe1* or eoe1.sw.*
          description:
                ex: CPUARCH=R3000 CPUBOARD=IP4 GFXBOARD=EXPRESS SUBGR=EXPRESS
                or/and a list of files to extract
                ex: usr/lib/libgl.a usr/etc/inetd.conf
          To get a description for a peticular host, take a look at
          /usr/etc/boot/clinst.dat and use the information returned by 'hinv'
          for that host.

   All operations associated with the specified files are writen to 'midb.ops'.
   These operations typically include 'touch', 'make', and 'dvhtool'.
   All files are extracted in directories relative to the current directory.
   All configuration specific files are in eoe* and maint*eoe*.

   WARNING: Even if most extractions/upgrades from one configuration to the
            an other are straight forward, this kind of manipulation can be
            dangerous and may result in a corrupted configuration.
            You should enter a value for all the configuration parameters
            i.e. CPUARCH, CPUBOARD, GFXBOARD, SUBGR
            YOU MOST KNOW WHAT YOURE DOING. This program can be a rope to
            help you out of a hole or to hang you with !
            There is NO GARANTY that it will work for you (even if it did for
            me).

   I will try to update the program with new versions of IRIX distribution
   files.

   For bugs reports and comments please e-mail to chouinar@centrcn.umontreal.ca
   If someone ever does write a piece of code that can get the proper CPUBOARD
   CPUARCH GFXBOPARD SUBGR values from the hardware inventory I could put that
   in the current code.
   If someone figures out what the f() atribut is for (in the .idb file) let
   me know. I did not need this knowledge for my purposes but

   For all I know, this program could become completely obsolete with 5.0, I
   did'nt check it out.

   Author: Luc Chouinard, Software Engineer
           Computer center, University of Montreal.


*******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#define MAXPATH  (PATH_MAX+1+NAME_MAX)
#define MAXMATCH 100
#define IDB_HEADER_LEN 13

/* possible GFXBOARD values */
#define GR_EXPRESS 1
#define GR_LIGHT   2
#define GR_CLOVER1 3
#define GR_CLOVER2 4
#define GR_STAPUFT 5
#define GR_SERVER  6
#define GR_ECLIPSE 7
#define GR_VENICE  8
#define GR_MGRAS   9

/* possible SUBGR values */
#define SG_EXPRESS  1
#define SG_LIGHT    2
#define SG_IP4G     4
#define SG_IP4GT    5
#define SG_IP5GT    6
#define SG_IP7GT    7
#define SG_IP17     8
#define SG_IP17SKY  9
#define SG_LG1MC    10
#define SG_SERVER   11
#define SG_MHPI     12
#define SG_ECLIPSE  13
#define SG_VENICE   14

/* possible values for CPUBOARD */
#define CPU_IP5     1
#define CPU_IP6     2
#define CPU_IP7     3
#define CPU_IP9     4
#define CPU_IP12    5
#define CPU_IP17    6
#define CPU_IP4     7
#define CPU_IP20    8
#define CPU_R2300   9

/* possible values for CPUARCH */
#define ARCH_R2300  1
#define ARCH_R3000  2
#define ARCH_R4000  3
#define ARCH_R8000  4
#define ARCH_R10000 5

/* format and macros for a CPUARCH+CPUBOARD+GFXBOARD+SUBGR value
   bit 0-1 : CPUARCH
   bit 2-5 : CPUBOARD
   bit 6-9 : GFXBOARD
   bit 10-13: SUBGR
*/
#define CPUARCH_MASK   0x03
#define CPUARCH_POS    0
#define CPUBOARD_MASK  0x0f
#define CPUBOARD_POS   2
#define GFXBOARD_MASK  0x0f
#define GFXBOARD_POS   6
#define SUBGR_MASK     0x0f
#define SUBGR_POS      10
#define getall(val,mask,pos)  ((val>>pos)&mask)
#define get_cpuarch(val)  (getall(val,CPUARCH_MASK,CPUARCH_POS))
#define get_cpuboard(val) (getall(val,CPUBOARD_MASK,CPUBOARD_POS))
#define get_gfxboard(val) (getall(val,GFXBOARD_MASK,GFXBOARD_POS))
#define get_subgr(val)    (getall(val,SUBGR_MASK,SUBGR_POS))

/* LUTs for GR_, SG_ and CPU_ values */
typedef struct { const char *name; int val; } LUTENT;
static LUTENT gr_lut[]={
{"ECLIPSE", GR_ECLIPSE,},
{"EXPRESS", GR_EXPRESS,},
{"LIGHT",   GR_LIGHT,},
{"CLOVER1", GR_CLOVER1,},
{"CLOVER2", GR_CLOVER2,},
{"STAPUFT", GR_STAPUFT,},
{"SERVER",  GR_SERVER,},
{"VGXT",    GR_STAPUFT,},
{"SKYWR",   GR_STAPUFT,},
{"VENICE",  GR_VENICE,},
{"MGRAS",   GR_MGRAS,},
NULL,
};
static LUTENT sg_lut[]={
{"ECLIPSE", SG_ECLIPSE,},
{"EXPRESS", SG_EXPRESS,},
{"LIGHT",   SG_LIGHT,},
{"IP4G",    SG_IP4G,},
{"IP4GT",   SG_IP4GT,},
{"IP5GT",   SG_IP5GT,},
{"IP7GT",   SG_IP7GT,},
{"IP17",    SG_IP17,},
{"IP17SKY", SG_IP17SKY,},
{"LG1MC",   SG_LG1MC,},
{"SERVER",  SG_SERVER,},
{"VENICE",  SG_VENICE,},
{"MHPI",    SG_MHPI,},
{"SKYWR",   SG_IP17SKY,},
NULL,
};
static LUTENT cpu_lut[]={
{"IP4", CPU_IP4},
{"IP5", CPU_IP5},
{"IP6", CPU_IP6},
{"IP7", CPU_IP7},
{"IP9", CPU_IP9},
{"IP12", CPU_IP12},
{"IP17", CPU_IP17},
{"IP20", CPU_IP20},
{"R2300", CPU_R2300},
NULL,
};
static LUTENT arch_lut[]={
{"R2300", ARCH_R2300},
{"R4000", ARCH_R4000},
{"R3000", ARCH_R3000},
{"MIPS1", ARCH_R3000},
{"MIPS2", ARCH_R4000},
{"R8000", ARCH_R8000},
{"R10000", ARCH_R10000},
NULL,
};

#define IDB_FILE_NAME_STRING_SIZE 100
#define IDB_SUBPRODUCT_NAME_STRING_SIZE 100
#define IDB_USER_STRING_SIZE 30
#define IDB_GROUP_STRING_SIZE 30
#define IDB_CONFIG_STRING_SIZE 20
#define IDB_SUBGROUP_STRING_SIZE 30

typedef enum idb_entry_type_e
{
	IDB_TYPE_FILE = 'f',
	IDB_TYPE_DIRECTORY = 'd',
	IDB_TYPE_SYMLINK = 's',
	IDB_TYPE_BLOCK_SPECIAL = 'b',
	IDB_TYPE_CHARACTER_SPECIAL = 'c',
	IDB_TYPE_FIFO = 'F',
	IDB_TYPE_UNKNOWN = '?',
} idb_entry_type_t;

typedef struct idb_entry_s
{
   idb_entry_type_t type;
   mode_t mode;	/* octal permissions mode */
   char user[IDB_USER_STRING_SIZE];     /* user name - need to find a way to specify numerical uid here*/
   char group[IDB_GROUP_STRING_SIZE];    /* group name - need to find a way to specify numerical gid here */ 
   char *fname;       /* complete pathname */
   char *fname_jump_source /* jump source pathname */
   uint16_t sum;           /* chksum */
   size_t size;          /* original size */
   int matches[MAXMATCH]; /* machine model */
   int f;             /* mysterious f attribute ... */
   size_t cmpsize;       /* compressed size */
} idb_entry_t;

typedef struct idb_entry_info_s
{
	int nmatch;
	char *postop;      /* sh command to perform afler installation of the package */
	char *exitop;      /* sh command to perform afler installation on exit of inst */
	char  config[IDB_CONFIG_STRING_SIZE];  /* config mode update, noupdate, suggest */
	char subgrp[IDB_SUBGROUP_STRING_SIZE];   /* subgroup in subproduct ex: util, tutedge etc...*/
	off_t offset;        /* offset in subproduct file */
	char *symval;
	struct sfile *next;
	void *sub;
} idb_entry_info_t;

/* to define a subproduct */
typedef struct subproduct_s
{
   char name[IDB_SUBPRODUCT_NAME_STRING_SIZE];    /* the name of the subproduct ex: man sw ... */
   FILE* file;            /* file handle to the product file */
   size_t num_entries;        /* the total number of files in that subproduct */
   off_t current_offset;        /* current calculated offset (while reading .idb file */
   idb_entry_t *entries;
   idb_entry_info_t *entry_info;
} subproduct_t;

/* to define a file */
typedef struct idb_file_s
{
   idb_entry_t* entries;
   idb_entry_info_t* entry_info;
   size_t num_entries;
   char name[IDB_FILE_NAME_STRING_SIZE];    /* the name of the product  ex: dev*/
   size_t num_files;        /* the total number of files in the product */
   size_t num_subs;          /* number of subproduct associated with that product ex: dev.man dev.sw ... */
   subproduct_t *subs;          /* pointer to first subproduct */
} idb_file_t;


#define TOSUB(pt) ((SUB*)pt->sub)

/**
   allocate new subproduct struct
**/
static char distd[MAXPATH];
static subproduct_t *creat_sub(char *name)
{
subproduct_t *sub;
char dir[MAXPATH];

   if(!(sub=(SUB*)malloc(sizeof(SUB)))) return 0;
   sub->nfiles=0;
   sub->next=0;
   strcpy(sub->name, name);
   /* open the corresponding file */
   sprintf(dir, "%s/%s", distd, name);
   if((sub->fh=open(dir, O_RDONLY))<=0) return 0;
   sub->curoff=IDB_HEADER_LEN;
   return sub;
}

/**
   get a pointer to the specified subproduct in the idb.
**/
static SUB *getsub(IDB *idb, char *name)
{
SUB *sub;
char *pt, sname[100];

   /* extract the subproduct from the subgroup */
   for(pt=name; *pt&&*pt!='.'; pt++);
   if(!*pt++) return 0;
   for(; *pt&&*pt!='.'; pt++);
   if(!*pt) return 0;
   strncpy(sname, name, pt-name);
   sname[pt-name]='\0';

   if(!idb->nsub) { idb->nsub=1; return idb->sub=creat_sub(sname); }
   else
   {

      for(sub=idb->sub; sub; sub=sub->next)
         if(!strcmp(sub->name, sname))
            return sub;
   }
   sub=idb->sub;
   idb->sub=creat_sub(sname);
   idb->sub->next=sub;
   idb->nsub++;
   return idb->sub;
}

/**
   Get a value out of a lut
**/
static int getlut(LUTENT *lut, char *val)
{
   while(lut->name){if(!strcmp(lut->name, val)) return lut->val; lut++;}
   fprintf(stderr, "Bad value specified (%s)\n", val);
   exit(1);
}
/**
   Get a string out of a lut
**/
static const char* getstr(LUTENT *lut, int val)
{
   while(lut->name){if(lut->val==val) return lut->name; lut++;}
   fprintf(stderr, "Bad value specified (%d)\n", val);
   exit(1);
}
/**
   Get an index for that variable.
**/
static int last_mask=0;
static int getval(char *type, char *val)
{
   if(!strcmp(type, "CPUBOARD"))
      {last_mask=0xffff<<CPUBOARD_POS; return getlut(cpu_lut, val)<<CPUBOARD_POS;}
   else if(!strcmp(type, "CPUARCH"))
      {last_mask=0xffff<<CPUARCH_POS;  return getlut(arch_lut, val)<<CPUARCH_POS;}
   else if(!strcmp(type, "SUBGR"))
      {last_mask=0xffff<<SUBGR_POS;  return getlut(sg_lut, val)<<SUBGR_POS;}
   else if(!strcmp(type, "GFXBOARD"))
      {last_mask=0xffff<<GFXBOARD_POS;  return getlut(gr_lut, val)<<GFXBOARD_POS;}
   else
   {
      fprintf(stderr, "Unknown variable type (%s)\n", type);
      exit(1);
   }
}
static char unparse(IDB_FILE *sfile)
{
int i, val;

   for(size_t n = 0; n < sfile->num_entries; n++)
   {
	   for(i=0;i<sfile->nmatch;i++)
	   {
		   printf("   ");
		   if((val=get_cpuarch(sfile->entries[n]->matches[i]))) printf("CPUARCH=%s ", getstr(arch_lut, val));
		   if((val=get_cpuboard(sfile->entries[n]->matches[i]))) printf("CPUBOARD=%s ", getstr(arch_lut, val));
		   if((val=get_gfxboard(sfile->entries[n]->matches[i]))) printf("GFXBOARD=%s ", getstr(gr_lut, val));
		   if((val=get_subgr(sfile->entries[n]->matches[i]))) printf("SUBGR=%s ", getstr(sg_lut, val));
		   printf("\n");
	   }
   }
}

/**
   Get the variable value
**/
static int getvar(char *assign)
{
char *vname, *value;

   vname=assign;
   if(!(value=strchr(assign, '=')))
   {
      if(!strncmp(vname, "IP", 2))
      {
         /* this is a MACH entry, makeit a CPUBOARD entry */
         value=vname;
         vname="CPUBOARD";
      }else if(vname[0]=='R')
      {
         /* this is a MACH entry, makeit a CPUARCH entry */
         value=vname;
         vname="CPUARCH";
      }else
      {
         fprintf(stderr, "Bad assignment (%s)\n", assign);
         exit(1);
      }
   }
   else *value++='\0';
   /* some Nil values are specified in packages ?! ex: mach(GFXBOARD=CLOVER2 SUBGR=) */
   if(!value[0]) return 0;
   return getval(vname, value);
}
/**
   Parse the atributes.
   atr : list of attributes ex: "sum(18156) size(32816) f(2748824473) cmpsize(14496)"
   subgrp : subgroup in subproduct ex: c.man.util
   sfile : data structure for that file.
**/
static void finval(char *atr)
{fprintf(stderr, "Invalide format for attributes: (%s)\n", atr); exit(1);}
static int getint(char *val) { int v; if(!val || sscanf(val, "%d", &v) != 1) {
      fprintf(stderr, "Invalide Integer value (%s)\n", val); exit(1); } return v; }

static int parse(char *atr, char *subgrp, IDB_FILE *sfile)
{
char *var;

   for(size_t n = 0; n < sfile->num_entries; n++)
   {
   strncpy(sfile->other[n]->subgrp, subgrp, sizeof(sfile->other[n]->subgrp));
   if(!(var=strtok(atr, " \t(\n"))) finval(atr);
   do
   {
      if(!strcmp(var, "sum")) sfile->sum=getint(strtok(NULL, " \t)\n"));
      else if(!strcmp(var, "size")) sfile->size=getint(strtok(NULL, " \t)\n"));
      else if(!strcmp(var, "noshare")) continue;
      else if(!strcmp(var, "f")) sfile->f=getint(strtok(NULL, " \t)\n"));
      else if(!strcmp(var, "config")) strncpy(sfile->config, strtok(NULL, ")\n"), sizeof(sfile->config));
      else if(!strcmp(var, "postop") || !strcmp(var, "exitop"))
      {
      char *cmd, *pt, *m;
      int len;

         pt=cmd=var+strlen(var)+1;
         again:
         while(*pt!=')') pt++;
         if(*(pt-1)==':') {pt++; goto again;}

         len=pt-cmd;
         if(!(m=malloc(len+1)))
         {fprintf(stderr, "mem error#2\n"); exit(1);}

         strncpy(m, cmd, len);
         m[len]='\0';
         if(!strcmp(var, "postop")) sfile->postop=m;
         else sfile->exitop=m;

         while((pt=strtok(NULL, ")")) && pt[strlen(pt)-1]==':');

      }
      else if(!strcmp(var, "symval"))
      {
      char *field=strtok(NULL, ")\n");
         if(!(sfile->symval=malloc(strlen(field)+1))) return 0;
         strcpy(sfile->symval, field);
      }
      else if(!strcmp(var, "cmpsize"))
      {
         sfile->cmpsize=getint(strtok(NULL, " \t)\n"));
         sfile->offset=TOSUB(sfile)->curoff;
         TOSUB(sfile)->curoff=sfile->offset+sfile->cmpsize+strlen(sfile->fname)+2;
      }
      else if(!strcmp(var, "mach"))
      {
      char *assign;
      int end=0, *match;

         /* we scan all the assignments */
         sfile->nmatch=1;
         sfile->matches[0]=0;
         match=sfile->matches;
         while(!end && (assign=strtok(NULL, " \t\n")))
         {
         int val;

            if(assign[strlen(assign)-1]==')'){assign[strlen(assign)-1]='\0'; end=1;}
            val=getvar(assign);
            /* if bits are already set in the upper fields is alreay set
               we change mask reseting all the bits from that mask upward
               This takes care of expression like:
               CPUBOARD=IP4 GFXBOARD=EXPRESS SUBGR=EXPRESS GFXBOARD=...
               In which more then one graphice configuration is supplied
               for a single cpu */
            if(*match & last_mask)
            {
               if(sfile->nmatch == MAXMATCH)
               {fprintf(stderr, "Too many matches (%s)!\n", sfile->fname);exit(1);}
               /* keep valid bits */
               *(match+1)=(*match)&(~last_mask);
               match++;
               sfile->nmatch++;
            }
            *match+=val;
         }
      }
      else
      {
      char *pt;
         fprintf(stderr, "\nWarning : untreated atribute %s file %s\n", var, sfile->fname);
         while((pt=strtok(NULL, ")")) && pt[strlen(pt)-1]==':');
      }
   } while(var=strtok(NULL, " \t(\n")); 
}
   return 1;
}
/**
   This function will read in the content of a IDB file and set
   a database according to the parsed information.
**/
static IDB *read_idb(char *name)
{
char fname[MAXPATH];
char line[4000];
FILE *file;
IDB *idb;
int counter, len;

   sprintf(fname, "%s/%s.idb", distd, name);
   if(!(file=fopen(fname, "r")))
   {
      fprintf(stderr, "Can't open file (%s) : %s\n", fname, strerror(errno));
      exit(1);
   }

   fseek(file, 0, SEEK_END);
   len=ftell(file);
   counter=0;
   fseek(file, 0, SEEK_SET);

   /* allocate memory */
   if(!(idb=(IDB*)malloc(sizeof(IDB)))) return 0;
   strcpy(idb->name, name);
   idb->nfiles=0;
   idb->nsub=0;

   printf("Reading %s...\n", fname);
   const char IDB_TYPE_FILE = 'f';
   const char IDB_TYPE_DIRECTORY = 'd';
   char ch;
   int reval = 0;
   while(retval = fread(&ch, 1, sizeof(char), file) == 1)
   {
   	SFILE *sfile = calloc(1, sizeof(SFILE));
	SUB *sub;
	char* field;
	sfile->nmatch=0;
	sfile->postop=sfile->extitop=0;
	sfile->config[0]='\0';
	sfile->type = ch;

      /* mode */
      field=strtok(NULL, " \t\n");
      if(!field) return 0;
      sscanf(field, "%o", &sfile->mode);
      /* owner */
      field=strtok(NULL, " \t\n");
      if(!field) return 0;
      strcpy(sfile->user, field);
      /* group */
      field=strtok(NULL, " \t\n");
      if(!field) return 0;
      strcpy(sfile->group, field);
      /* complet pathname */
      field=strtok(NULL, " \t\n");
      if(!field) return 0;
      if(!(sfile->fname=malloc(strlen(field)+1))) return 0;
      strcpy(sfile->fname, field);
      /* jump source pathname */
      field=strtok(NULL, " \t\n");
      if(!field) return 0;
      /* subproduct name */
      field=strtok(NULL, " \t\n");
      /* get pointer to that subproduct */
      if(!(sub=getsub(idb, field))) return 0;
      if(!sub->nfiles) sub->files=sfile;
      else
      {
      SFILE *sf;
         sf=sub->files;
         sub->files=sfile;
         sfile->next=sf;
      }
      sub->nfiles++;
      sfile->sub=(void*)sub;
      /* parse the atributes */
      if(sfile->type != 'd' && !parse(&field[strlen(field)+1], field, sfile)) return 0;
	switch(sfile->type)
	{
		case IDB_TYPE_FILE:
			if(fscanf(file, "%o %s %s %s %s %s sum(%zu) size(%zu) mach(GFXBOARD=%s) f(%zu) cmpsize(%zu)\n", &sfile->mode, sfile->user, sfile->group, sfile->fname, sfile->j) != 11);
			{
				fprintf(stderr, "Can't read idb line entry of type file (%s) : %s\n", fname, strerror(errno));
				fclose(file);
				exit(EXIT_FAILURE);
			}
			break;
		case IDB_TYPE_DIRECTORY:
			if(fscanf(file, "%o %s %s %s %s %s mach(GFXBOARD=%s)\n") != 7); 
			{
				fprintf(stderr, "Can't read idb line entry of type directory (%s) : %s\n", fname, strerror(errno));
				fclose(file);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			break;
	}
   }
{
      fprintf(stderr, "Can't read idb line entry type (%s) : %s\n", fname, strerror(errno));
      fclose(file);
      exit(EXIT_FAILURE);
   }
      if(!((counter++)%100)) {printf("\r%zd%%", ftell(file)*100/len); fflush(stdout);}

   }
   printf("\r%zd%%\n", ftell(file)*100/len);
   fflush(stdout);
   return idb;
}
/**
   This program can be used to list/extract files from
   an product distribution package.
**/
static int usage()
{
   fprintf(stderr, "%s", "\
usage: midb [-d dir] [-l] [-x] [-X] product mask description\n\
       -d dir  : distribution directory\n\
       -l      : list corresponding files\n\
       -x      : extract corresponding files\n\
       -X      : line 'x' put don't uncompress files\n\
       product : product name ex: eoe1\n\
       mask: mask of products to extract/list files from\n\
             ex: eoe1* or eoe1.sw.*\n\
       description:\n\
             ex: CPUARCH=R3000 CPUBOARD=IP4 GFXBOARD=EXPRESS SUBGR=EXPRESS\n\
             or a list of files to extract\n\
             ex: usr/lib/libgl.a usr/etc/inetd.conf\n\
       To get a description for a peticular host, take a look at\n\
       /usr/etc/boot/clinst.dat and use the information returned by 'hinv'\n\
       for that host.\n"
);
   exit(1);
}
/**
   Match with expression.
**/
static int globexpr, globarch, globcpu, globgfx, globsub;
static int matchexpr(int nmatch, int *matches)
{
   while(nmatch--)
   {
   int m, sm;
      m=matches[nmatch];
      if(
          (!(sm=((m>>CPUARCH_POS)&CPUARCH_MASK)) || sm==globarch) &&
          (!(sm=((m>>CPUBOARD_POS)&CPUBOARD_MASK)) || sm==globcpu) &&
          (!(sm=((m>>GFXBOARD_POS)&GFXBOARD_MASK)) || sm==globgfx) &&
          (!(sm=((m>>SUBGR_POS)&SUBGR_MASK)) || sm==globsub)
      )
      {
         return 1;
      }
   } return 0;
}
/**
   Get the file from the product database.
**/
#define IOBSIZ 10240
static int dodecomp=1;

static int extract(SFILE *sfile)
{
char *buf;
int fh=TOSUB(sfile)->fh;

   /* get the file out of the subproduct */
   /* seek to the proper location */
   if(lseek(fh, sfile->offset, SEEK_SET) < 0)
   {
      fprintf(stderr, "Can't seek on file (%s/%s)\n", distd, TOSUB(sfile)->name);
      exit(1);
   }
   if(!(buf=malloc(IOBSIZ)))
   if(lseek(fh, sfile->offset, SEEK_SET) < 0)
   {
      fprintf(stderr, "Can't allocate io buffer\n");
      exit(1);
   }
   /* verify the name found there */
   if(read(fh, buf, strlen(sfile->fname)+2)!=strlen(sfile->fname)+2)
   {
      fprintf(stderr, "Can't read file (%s/%s) : %s\n", distd, TOSUB(sfile)->name, strerror(errno));
      exit(1);
   }
   if(strncmp(&buf[2], sfile->fname, strlen(sfile->fname)))
   {
      fprintf(stderr, "Out of sync on file (%s/%s.idb)\n", distd, TOSUB(sfile)->name);
      exit(1);
   }
   /* decompress it */
   {
   int nbytes=0, size=sfile->cmpsize;
   int hout;
   char name[MAXPATH], cmd[100+MAXPATH];

      sprintf(name, "%s.Z", sfile->fname);
      if(!(hout=open(name, O_WRONLY+O_CREAT+O_TRUNC, sfile->mode)) < 0)
      {
         fprintf(stderr, "Can't creat/open file (%s): %s\n", name, strerror(errno));
         exit(1);
      }
      while(nbytes<size)
      {
      int n;

         n=IOBSIZ>(size-nbytes)?size-nbytes:IOBSIZ;
         if(read(fh, buf, n) != n)
         {
            fprintf(stderr, "Error reading while extracting file : %s!\n", strerror(errno));
            exit(1);
         }
         if(write(hout, buf, n) !=n)
         {
            fprintf(stderr, "Error writing while extracting file : %s!\n", strerror(errno));
            exit(1);
         }
         nbytes+=n;
      }
      close(hout);
      if(dodecomp)
      {
         sprintf(cmd, "/usr/bsd/uncompress %s", name);
         if(system(cmd)!=0)
         {
            fprintf(stderr, "Error decompressing file : %s!\n",strerror(errno));
            exit(1);
         }
         /* in case it is still there */
         unlink(name);
      }
   } free(buf);
}
static int dopath(char *p)
{
char *pi=p;

   while(*p)
   {
      if(*p=='/')
      {
         *p='\0';
         mkdir(pi, 0755);
         *p='/';
      }
      p++;
   }
}
/** Extract from GNU cpio package **/

/* Match the pattern PATTERN against the string TEXT;
   return 1 if it matches, 0 otherwise.
   A match means the entire string TEXT is used up in matching.

   In the pattern string, `*' matches any sequence of characters,
   `?' matches any character, [SET] matches any character in the specified set,
   [!SET] matches any character not in the specified set.

   A set is composed of characters or ranges; a range looks like
   character hyphen character (as in 0-9 or A-Z).
   [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
   Any other character in the pattern must be matched exactly.

   To suppress the special syntactic significance of any of `[]*?!-\',
   and match the character exactly, precede it with a `\'.

   If DOT_SPECIAL is nonzero,
   `*' and `?' do not match `.' at the beginning of TEXT.  */

/* Like glob_match, but match PATTERN against any final segment of TEXT.  */
static int glob_match_after_star();
static int glob_match (char *pattern, char *text, int dot_special)
{
  char *p = pattern, *t = text;
  char c;

  while ((c = *p++) != '\0')
{
    switch (c)
      {
      case '?':
        if (*t == '\0' || (dot_special && t == text && *t == '.'))
          return 0;
        else
          ++t;
        break;

      case '\\':
        if (*p++ != *t++)
          return 0;
        break;

      case '*':
        if (dot_special && t == text && *t == '.')
          return 0;
        return glob_match_after_star (p, t);

      case '[':
        {
          register char c1 = *t++;
          int invert;

          if (c1 == '\0')
            return 0;

          invert = (*p == '!');

          if (invert)
            p++;

          c = *p++;
          while (1)
            {
              register char cstart = c, cend = c;

              if (c == '\\')
                {
                  cstart = *p++;
                  cend = cstart;
                }

              if (cstart == '\0')
                return 0;       /* Missing ']'. */

              c = *p++;

              if (c == '-')
                {
                  cend = *p++;
                  if (cend == '\\')
                    cend = *p++;
                  if (cend == '\0')
                    return 0;
                  c = *p++;
                }
              if (c1 >= cstart && c1 <= cend)
                goto match;
              if (c == ']')
                break;
            }
          if (!invert)
            return 0;
          break;

        match:
          /* Skip the rest of the [...] construct that already matched.  */
          while (c != ']')
            {
              if (c == '\0')
                return 0;
              c = *p++;
              if (c == '\0')
                return 0;
              if (c == '\\')
                p++;
            }
          if (invert)
            return 0;
          break;
        }

      default:
        if (c != *t++)
          return 0;
      }
} return *t == '\0';
}

static int glob_match_after_star (char *pattern, char *text)
{
  char *p = pattern, *t = text;
  char c, c1;

  while ((c = *p++) == '?' || c == '*')
    if (c == '?' && *t++ == '\0')
      return 0;

  if (c == '\0')
    return 1;

  if (c == '\\')
    c1 = *p;
  else
    c1 = c;

  --p;
  while (1)
    {
      if ((c == '[' || *t == c1) && glob_match (p, t, 0))
        return 1;
      if (*t++ == '\0')
        return 0;
    }
}

static int nfiles=0;
static char *files[100];
/* Is that one of the files we want */
static int inlist(SFILE *sfile)
{
int i;
   for(i=0;i<nfiles;i++)
   {
      if(!strcmp(files[i], sfile->fname)) return 1;
   } return 0;
}
int main(int argc, char **argv)
{
extern char *optarg;
extern int optind;
char mask[100];
int c, ops=0;
int i, doextract=0, dolist=0;
IDB *idb;
SUB *sub;

   strcpy(distd, ".");

   while ((c = getopt(argc, argv, "lXxd:")) != -1)
   {
      switch (c) {
      case 'd':
           strcpy(distd, optarg);
           break;
      case 'l':
           if(doextract)
           {
oneof:
              fprintf(stderr, "%s", "Only one of -l, -x or -X please.\n");
              exit(1);
           }
           else dolist++;
           break;
      case 'X':
           if(dolist) goto oneof;
           else doextract++;
           break;
      case 'x':
           if(dolist) goto oneof;
           else {doextract++; dodecomp++;}
           break;
      }
   }
   if(!dolist && !doextract)
   {
      fprintf(stderr, "%s", "At least one of -l or -x please.\n");
      exit(1);
   }

   if(argc-optind < 2) usage();

   if(!(idb=read_idb(argv[optind])))
   {
      fprintf(stderr, "%s", "Error IDB database !\n");
      exit(1);
   }
   strcpy(mask, argv[optind+1]);

   /* evaluate specified description or cumulate file names */
   globexpr=0;
   for(i=optind+2;i<argc;i++)
   {
      if(strchr(argv[i], '=')) globexpr+=getvar(argv[i]);
      else files[nfiles++]=argv[i];
   }

   globarch=(globexpr>>CPUARCH_POS)&CPUARCH_MASK;
   globcpu=(globexpr>>CPUBOARD_POS)&CPUBOARD_MASK;
   globgfx=(globexpr>>GFXBOARD_POS)&GFXBOARD_MASK;
   globsub=(globexpr>>SUBGR_POS)&SUBGR_MASK;

   /* foreach subproduct */
   for(sub=idb->sub;sub;sub=sub->next)
   {
   SFILE *sfile;

      for(sfile=sub->files;sfile;sfile=sfile->next)
      {
         uid_t uid = getpwnam(sfile->user)->pw_uid;
	 gid_t gid = getgrnam(sfile->group)->gr_gid; 

         /* look for a match */
         if(glob_match(mask, sfile->subgrp, 0))
         {
            if((!globexpr || matchexpr(sfile->nmatch, sfile->matches)) && (!nfiles || inlist(sfile)))
            {
               printf("%s\n", sfile->fname);
               /* if mach attribute specified and user requested this file whitout specification
                  then give more info */
               if(nfiles && !globexpr) unparse(sfile);
               /* do we extract it ? */
               if(doextract)
               {
                  dopath(sfile->fname);
                  switch(sfile->type)
                  {
                  case 'f':
                     extract(sfile);
                     chown(sfile->fname, uid, gid);
                  break;
                  case 'd':
                     mkdir(sfile->fname, sfile->mode);
                     chown(sfile->fname, uid, gid);
                  break;
                  case 'l':
                     link(sfile->symval, sfile->fname);
                  break;
                  default:
                  break;
                  }
               }
               if(sfile->postop || sfile->exitop) ops=1;
            }
         }
      }
   }
   /* print out the commands to execute */
   if(ops)
   {
   FILE *opf;

      if(!(opf=fopen("midb.ops", "a")))
      {fprintf(stderr, "Can't open command file!\n"); exit(1);}
      fprintf(opf, "\
#! /bin/sh\n\
rbase=\"\"\n\
instmode=normal\n\
# NB : vhdev must be set to the volume header partition of system disk.\n\
#      ex: /dev/dsk/dks0d1vh\n");
      for(sub=idb->sub;sub;sub=sub->next)
      {
      SFILE *sfile;

         for(sfile=sub->files;sfile;sfile=sfile->next)
         {
            /* look for a match */
            if(glob_match(mask, sfile->subgrp, 0) && matchexpr(sfile->nmatch, sfile->matches))
            {
                if(sfile->postop) fprintf(opf, "eval %s\n", sfile->postop);
                if(sfile->exitop) fprintf(opf, "eval %s\n", sfile->exitop);
            }
         }
      }
      chmod("midb.ops", 0700);
      printf("\
   File \"midb.ops\" contains commands associated with those files.\n\
   Take a look at it and perform the action they imply on the destination host\n\
   after the update.\n");
   }
   exit(0);
}
