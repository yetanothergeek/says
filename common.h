#define IDLE_THRESHOLD 100 /* Any BPS below this considered idle */
#define HOT_THRESHOLD 1024 /* Any BPS above this is in color */

#define DEFAULT_IP_STR  " "
#define DEFAULT_IFNAME "eth0"
#define OFFLINE_MSG          "     -- offline --     "

#ifdef XFT
# define DEFAULT_FONTSIZE "8"
#else
# define DEFAULT_FONTSIZE "12"
#endif

void init_batfiles();

typedef enum { nsIdle, nsDownload, nsUpload, nsUnchanged } NetSpeedStatus;

extern NetSpeedStatus net_speed_status;

typedef struct {
  int show_bps;
  int show_lip;
  int show_rip;
  int show_ram;
  int show_swp;
  int show_cpu;
  int show_hog;
  int show_hms;
  int show_bat;
  int start_at_top;
} Options;

extern Options opts;

#define BPS_W 26
#define LIP_W 16
#define RIP_W 24
#define CPU_W 12
#define RAM_W  18
#define SWAP_W 18
#define PROC_W 64
#define HMS_W 12
#define BAT_W 12

typedef struct {
  unsigned char cpu_pct;
  unsigned char cpu_cnt;
  unsigned char bat_pct;
  unsigned char bat_stat;
  int ram_total, ram_used;
  int swap_total, swap_used;
  char bps_str[BPS_W]; /* bytes per second */
  char lip_str[LIP_W]; /* local ip address */
  char rip_str[RIP_W]; /* remote ip address */
  char cpu_str[CPU_W];
  char ram_str[RAM_W];
  char swap_str[SWAP_W];
  char proc_str[PROC_W];
  char bat_str[BAT_W];
  char if_name[32];
  char hms_str[HMS_W];
} SysInfo;


extern SysInfo curr_inf;


void get_net_info();
void get_mem_info();

#include <net/if.h>
#define IP_NAMESIZE 16

typedef struct {
  char name[IF_NAMESIZE];
  char addr[IP_NAMESIZE];
  char score;
} IfInfo;


void show_ifnames();
void choose_ifname(char*dst);


#if __WORDSIZE == 64
#  define LLU "%lu"
#else
#  define LLU "%llu"
#endif

typedef unsigned char uchar;

