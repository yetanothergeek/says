#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "common.h"



typedef char ipstr[IP_NAMESIZE];
static unsigned int idle_secs;


static void set_idle_message()
{
  unsigned int secs = ++idle_secs;
  unsigned int hour;
  unsigned int min;
  hour=secs / 3600;
  secs -= hour * 3600;
  min=secs / 60;
  secs -= min * 60;
  snprintf( &curr_inf.bps_str[0], sizeof(curr_inf.bps_str)-1,
            "idle %02u:%02u:%02u %*s", hour, min, secs,
            (int)sizeof(curr_inf.bps_str), " ");
}


static const char* ProcNetDev = "/proc/net/dev";

static int read_proc_net_dev(u_int64_t *vRx, u_int64_t *vTx)
{
  static int prev_mtime=0;
  struct stat info;
  int rv=0;
  *vRx=0;
  *vTx=0;
  stat(ProcNetDev, &info);
  if ( info.st_mtime != prev_mtime ) {
    FILE *f;
    static char *line=NULL;
    static size_t count=0;
    static char srch_str[32]="\0";
    static int srch_len=0;
    prev_mtime=info.st_mtime;
    if (!srch_str[0]) {
     snprintf(srch_str, sizeof(srch_str)-1, "%s:", curr_inf.if_name);
     srch_len=strlen(srch_str);
    }
    f=fopen(ProcNetDev, "r");
    if ( NULL == f ) {
      fprintf(stderr, "Can't read %s\n", ProcNetDev);
      return 0;
     }
    while (getline(&line, &count, f)>=0) {
      char *p=line;
      while (' ' == *p) {p++;}
      if (strncmp(p, srch_str, srch_len)==0) {
        rv=1;
        break;
      }
    }
    fclose(f);
    if (rv) {
      char *p=strchr(line, ':');
      if (p) {
        p++;
        while (' ' == *p) {p++;}
        sscanf( p,
          LLU" %*d %*d %*d %*d %*d %*d %*d "LLU" %*d %*d %*d %*d %*d %*d %*d",
          vRx, vTx );
      }
    }
  }
  return rv;
}



static int get_net_speed(int *rx_out, int *tx_out)
{
  static u_int64_t rx_prev=0;
  static u_int64_t tx_prev=0;
  u_int64_t rx_curr, tx_curr;
  int rv = read_proc_net_dev(&rx_curr, &tx_curr);
  *rx_out = rx_prev? (rx_curr-rx_prev) : 0;
  *tx_out = tx_prev? (tx_curr-tx_prev) : 0;
  rx_prev=rx_curr;
  tx_prev=tx_curr;
  return rv;
}


static int get_ip_string(int sd, int ctl,  ipstr dest, struct ifreq *ifr)
{
  char *p;
  int rv = ioctl(sd, ctl, ifr);
  if ( rv >= 0 ) {
    p=inet_ntoa(
      ((struct sockaddr_in*)&(ifr->ifr_ifru.ifru_addr))->sin_addr
    );
    strncpy(dest, p, IP_NAMESIZE-1);
  }
  return (rv>=0)?1:0;
}

#define do_clear(s) ( snprintf(curr_inf.s, sizeof(curr_inf.s)-1, \
                          "%*s", (int)sizeof(curr_inf.s), " ") )

#define clear_address() ( do_clear(lip_str),do_clear(rip_str))

static void get_address_strings()
{
  ipstr local="\0";
  ipstr remote="\0";
  int bcast = 0;
  int ok = 0;
  int sock = -1;
  struct ifreq ifr;

  strncpy( ifr.ifr_ifrn.ifrn_name, curr_inf.if_name, IF_NAMESIZE-1 );
  ifr.ifr_ifru.ifru_addr.sa_family = AF_INET;
  memset(&local[0], '\0', sizeof(local));
  memset(&remote[0], '\0',sizeof(remote));
  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if ( sock >= 0 ) {
    ok=get_ip_string(sock, SIOCGIFADDR, local, &ifr);
    if ( ok ) {
      ok=get_ip_string(sock, SIOCGIFDSTADDR, remote, &ifr);
      if (local[0] && (strcmp(local, remote)==0)) {
        ok=get_ip_string(sock, SIOCGIFBRDADDR, remote, &ifr);
        bcast=1;
      }
    }
    close(sock);
  }

  ok=(ok && local[0] && remote[0]);

  if (ok) {
    snprintf( curr_inf.lip_str, sizeof(curr_inf.lip_str)-1, "%s%*s",
              local, (int)sizeof(curr_inf.lip_str), "");

    snprintf(curr_inf.rip_str,sizeof(curr_inf.rip_str)-1, "%s%s%*s",
      bcast?"  bcast:":"    ptp:", remote, (int)sizeof(curr_inf.rip_str), "");
  } else { clear_address();  }
}


static void get_bps_string()
{
  int avg_rx, avg_tx;
  char PrevMsg[BPS_W];
  static int rx_s[3]={0,0,0}; /* Store last 3 readings for average */
  static int tx_s[3]={0,0,0};
  net_speed_status=nsIdle;
  strcpy(PrevMsg,curr_inf.bps_str);
  memmove(&rx_s[0],&rx_s[1], 2*sizeof(int));
  memmove(&tx_s[0],&tx_s[1], 2*sizeof(int));
  if (get_net_speed(&rx_s[2], &tx_s[2])) {
    if ( rx_s[0] || rx_s[1] || rx_s[2] ) {
      avg_rx=(rx_s[0]+rx_s[1]+rx_s[2] )/3;
      avg_tx=(tx_s[0]+tx_s[1]+tx_s[2] )/3;
      if ( (avg_rx > IDLE_THRESHOLD) || (avg_tx > IDLE_THRESHOLD) ) {
        idle_secs=0;
        if ( ( avg_rx > HOT_THRESHOLD ) || ( avg_tx > HOT_THRESHOLD ) ) {
          net_speed_status=( avg_tx > avg_rx )?nsUpload:nsDownload;
        }
        snprintf( curr_inf.bps_str, sizeof(curr_inf.bps_str)-1,
                  "Rx:%-7d Tx:%-7d %*s", avg_rx, avg_tx,
                  (int)sizeof(curr_inf.bps_str), " ");
      } else { set_idle_message(); }
    } else { set_idle_message(); }
  } else {
     snprintf( curr_inf.bps_str, sizeof(curr_inf.bps_str)-1, "%s%*s",
               OFFLINE_MSG, (int)sizeof(curr_inf.bps_str)-1, " " );
     idle_secs=0;
     clear_address();
  }
  if ( strcmp(curr_inf.bps_str,PrevMsg) == 0 ) { net_speed_status=nsUnchanged; }
}


void get_net_info()
{
  if ((opts.show_lip||opts.show_rip)&&(' '==curr_inf.lip_str[0])) {
    get_address_strings();
  }
  if (opts.show_bps) { get_bps_string(); }
}



static IfInfo* get_ifnames()
{
  FILE *f=fopen(ProcNetDev, "r");
  if (f) {
    static char *line=NULL;
    static size_t count=0;
    IfInfo *result=NULL;
    IfInfo *pinf;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock>=0) {
      int n=0;
      int i=0;
      int c;
      while ((c=fgetc(f))!=EOF) { if (c=='\n') n++;}
      if (n>0) {
        result=(IfInfo*)calloc(n,sizeof(IfInfo));
        fseek(f,0,SEEK_SET);
        while (getline(&line, &count, f)>=0) {
          char *p1=line;
          char *p2=NULL;
          while ( ' ' == *p1 ) {p1++;}
          p2=strchr(p1,':');
          if ((p2>p1)&&(i<n)) {
            pinf=&result[i];
            i++;
            *p2='\0';
            strncpy(pinf->name, p1, IF_NAMESIZE-1);
            if ( sock >= 0 ) {
              struct ifreq ifr;
              strncpy( ifr.ifr_ifrn.ifrn_name, p1, IF_NAMESIZE-1 );
              ifr.ifr_ifru.ifru_addr.sa_family = AF_INET;
              get_ip_string(sock, SIOCGIFADDR, pinf->addr, &ifr);
            }
          }
        }
      }
      close(sock);
    }
    fclose(f);
    return result;
  }
  return NULL;
}


void choose_ifname(char*dst)
{
  IfInfo* list=get_ifnames();
  IfInfo* pinf;
  IfInfo* choice=NULL;

  if (!list) return;
  for (pinf=list;pinf->name[0]; pinf++) {
    if (strcmp(pinf->name,"lo")!=0) { pinf->score++; }
  }
  for (pinf=list;pinf->name[0]; pinf++) {
    if (pinf->addr) { pinf->score++; }
  }
  int hi=0;
  for (pinf=list;pinf->name[0]; pinf++) {
    if (pinf->score>=hi) {
      hi=pinf->score;
      choice=pinf;
    }
  }
  if (choice) {
    strncpy(dst,choice->name,31);
  }
  free(list);
}


void show_ifnames() {
  IfInfo* list=get_ifnames();
  IfInfo* pinf;
  if (list) {
    printf("%s\n", "Available interfaces:");
    for (pinf=list;pinf->name[0]; pinf++) {
      printf("  %s\t(%s)\n", pinf->name, pinf->addr[0]?pinf->addr:"unassigned");
    }
    free(list);
  } else {
    fprintf(stderr,"%s\n", "No interfaces found.");
  }
}

