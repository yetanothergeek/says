#include <unistd.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#include "common.h"

#define ProcMemInfo "/proc/meminfo"
#define ProcStat "/proc/stat"


typedef struct _PidRec {
  pid_t pid;
  unsigned long cpu;
} PidRec;


static unsigned long prev_pid_count=0;
static PidRec top_rec;
static int cpu_total;
static pid_t my_pid;


static PidRec *get_pid_list()
{
  static int curr_pid_count=0;
  unsigned long i;
  FILE *f;
  char fn[255]="\0";
  char s[1024];
  PidRec*rv=NULL;
  unsigned long scpu;
  glob_t g;
  char *p;
  int err=glob( "[1-9]*", 0, NULL, &g );
  if ( !err ) {
    prev_pid_count=curr_pid_count;
    curr_pid_count=g.gl_pathc;
    rv=calloc(g.gl_pathc+1, sizeof(PidRec));
    for ( i=0; i<g.gl_pathc; i++) {
      snprintf(fn, sizeof(fn)-1, "/proc/%s/stat", g.gl_pathv[i] );
      f=fopen(fn, "r");
      if (!f) { continue; }
      s[0]='\0';
      fread(s,sizeof(char), sizeof(s)-1, f);
      fclose(f);
      p=strchr(s,')');
      if (p) {
        sscanf(s, "%d", &(rv[i].pid));
        p+=4;
        sscanf(p, "%*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %lu %lu %*s",
                   &rv[i].cpu, &scpu);
        rv[i].cpu+=scpu;
      }
    }
    globfree(&g);
  }
  return rv;
}


static unsigned char get_percent(int part, int all)
{
  float rat;

  if ( !all ) { return 100; }
  rat=roundf( ((float)part / (float)all) *100);
  return (unsigned char) ((rat>100)?100:(rat<0)?0:rat);
}


static void get_command_name(char *pidname, char *buf, int bufsize)
{
  char fn[32]="\0";
  char s[256]="\0";
  char *p1, *p2;
  int len;
  FILE *f;

  buf[0]='\0';
  snprintf(fn, sizeof(fn)-1, "/proc/%s/stat", pidname);
  f=fopen(fn, "r");
  if (!f) { return; }
  fread(s, sizeof(char), sizeof(s)-1, f);
  fclose(f);
  p1=strchr(s, '(');
  if ( !p1 ) { return; }
  p2=strrchr(s, ')');
  if ( p1 > p2 ) {return; }
  len=(p2-p1)+1;
  if ( len > (bufsize-1)) { len=bufsize-1; }
  strncpy(buf, p1, len);
  buf[len]='\0';
}


static void clear_proc_str()
{
  memset(curr_inf.proc_str, ' ',sizeof(curr_inf.proc_str) );
  curr_inf.proc_str[sizeof(curr_inf.proc_str)-1]='\0';
}


static void get_top_pid(PidRec*curr_list, PidRec*prev_list)
{
  PidRec *iCurr,*iPrev;
  unsigned long i;
  if (!(prev_list&&curr_list)) {return;}
  for (iCurr=curr_list,i=0; iCurr->pid; iCurr++,i++) {
    pid_t pid=iCurr->pid;
    unsigned long N;
    if (pid==my_pid) {continue;}
    if ( (i<prev_pid_count) && (prev_list[i].pid==pid)) {
      iPrev=&prev_list[i]; /* We got lucky, the index is the same */
    } else { /* Else it gets expensive! */
      for (iPrev=prev_list; iPrev->pid; iPrev++) {
        if ( pid == iPrev->pid ) { break; }
      }
    }
    if (pid == iPrev->pid) {
      N=iCurr->cpu - iPrev->cpu;
      if ( N > 0 ) {
        cpu_total+=N;
        if ( N >  top_rec.cpu ) {
          top_rec.pid=pid;
          top_rec.cpu = N;
        }
      }
    }
  }
}


static void get_top_proc(int really)
{
  static PidRec *prev_list=NULL;
  static char pidname[8];
  PidRec *this_list;
  char cmdname[256];
  if (really) {
    this_list=get_pid_list();
    memset(&top_rec, '\0', sizeof(PidRec));
    cpu_total=0;
    if (0==my_pid) {my_pid=getpid();}
    get_top_pid(this_list, prev_list);
    if ( top_rec.cpu > 0 ) {
      snprintf(pidname, sizeof(pidname)-1, "%d", top_rec.pid);
      get_command_name(pidname, cmdname, sizeof(cmdname));
      snprintf(&curr_inf.proc_str[0], sizeof(curr_inf.proc_str)-1, "%d %s %d%%  ",
        top_rec.pid, cmdname, get_percent(top_rec.cpu, cpu_total));
    } else { clear_proc_str(); }
    if ( prev_list ) { free(prev_list); }
    prev_list=this_list;
  } else { clear_proc_str(); }
}


static uint64_t get_cpu_usage()
{
  uint64_t rv=0;
  char s[1024]="\0";
  char *p;
  FILE *f=fopen(ProcStat, "r");

  if (!f) { return 0; }
  fread(s,sizeof(char), sizeof(s)-1, f);
  fclose(f);
  p=strchr(s, ' ');
  if (!p) {return 0; }
  while (' ' == *p) {p++; }
  sscanf(p, "%*d %*d %*d "LLU" %*s", &rv );
  return rv;
}


static void get_cpu_info()
{
  static uint64_t old_cpu_val=0;
  uint64_t new_val=get_cpu_usage();
  float diff= 1 - (((float)(new_val - old_cpu_val)) / 100.0 );

  diff=roundf(diff*100);
  curr_inf.cpu_pct=(diff>100)?100:(diff<0)?0:(unsigned char)diff;
  old_cpu_val=new_val;
  snprintf( curr_inf.cpu_str, sizeof(curr_inf.cpu_str)-1, 
            "cpu:%d%%%10s", curr_inf.cpu_pct, " ");
}


static unsigned long parse_mem_field(char *rec, int *cnt)
{
  unsigned long rv=0;
  char *p=strchr(rec, ':')+1;
  while (' '==*p) {p++;}
  sscanf(p, "%lu", &rv);
  (*cnt)--;
  return(rv/1024);
}


static void get_mem_string()
{
  static char *line=NULL;
  static size_t count=0;
  FILE*f;
  int still_need=(opts.show_ram*2)+(opts.show_swp*2);
  f=fopen(ProcMemInfo, "r");
  if (!f) {return;}
  while (still_need && (getline(&line, &count, f)>=0)) {
    if (count) {
      switch (line[0]) {
        case 'M':{
          if (opts.show_ram) {
            if (strncmp(line, "MemTotal:", 9) == 0 ) {
             curr_inf.ram_total = parse_mem_field(line, &still_need);
            } else {
              if (strncmp(line, "MemFree:", 8) == 0) {
                curr_inf.ram_used =
                  curr_inf.ram_total - parse_mem_field(line, &still_need);
              }
            }
          }
          break;
        }
        case 'S':{
          if (opts.show_swp) {
            if (strncmp(line, "SwapTotal:", 10) == 0 ) {
             curr_inf.swap_total = parse_mem_field(line, &still_need);
            } else {
             if (strncmp(line, "SwapFree:", 9) == 0) {
               curr_inf.swap_used =
                 curr_inf.swap_total - parse_mem_field(line, &still_need);
             }
            }
          }
          break;
        }
      }
    }
  }
  fclose(f);
  snprintf(curr_inf.ram_str, sizeof(curr_inf.ram_str)-1, "ram:%d/%d %16s",
    curr_inf.ram_used, curr_inf.ram_total, " ");
  snprintf(curr_inf.swap_str, sizeof(curr_inf.swap_str)-1,"swap:%d/%d %16s",
    curr_inf.swap_used, curr_inf.swap_total, " ");
}



void get_mem_info()
{
  static int check_top_proc=1;
  if (opts.show_cpu || opts.show_hog) { get_cpu_info(); }
  if (opts.show_ram || opts.show_swp) { get_mem_string(); }
  if (opts.show_hog && (check_top_proc>=3)) {
    check_top_proc = 1;
    get_top_proc( curr_inf.cpu_pct > 25 );
  } else check_top_proc++;
}


