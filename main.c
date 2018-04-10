#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


#include "common.h"
#include "window.h"

#define FONTSPEC_ENV "SAYS_FONTSPEC"
#define BGCOLOR_ENV  "SAYS_BGCOLOR"
#define FGCOLOR_ENV  "SAYS_FGCOLOR"
#define APPNAME_ENV  "SAYS_APPNAME"

Options opts;
SysInfo curr_inf;
static SysInfo prev_inf;

static char* oldpwd=NULL;
static uchar collapsed=0;

char* app_name=NULL;

static void handle_press(int x, int y, int button)
{
  switch(button) {
    case 1:{
     collapsed = !collapsed;
     if (collapsed) {
       XResizeWindow(win.dpy, win.win, win.char_width*3, win.height);
     } else {
       XResizeWindow(win.dpy, win.win, win.work_area.width, win.height);
       draw_string(" ",base, 0);
     }
     break;
    }
    case 2: {
      toggle_strut();
      if (collapsed) { handle_press(x,y,1); }
      break;
    }
    case 3: {
      if (oldpwd) chdir(oldpwd);
      exit(0);
    }
  }
}


static int handle_error(Display*d, XErrorEvent*ev)
{
  char err_buf[256];
  if ( !ev ) {
    fprintf(stderr, "%s: X Error: NULL event\n", APP_NAME);
    return -1;
  }
  if ( !d ) {
    fprintf(stderr, "%s: X Error: NULL display\n", APP_NAME );
    return -1;
  }
  memset(err_buf, '\0', sizeof(err_buf));
  XGetErrorText(d, ev->error_code, &err_buf[0], sizeof(err_buf)-1);
  fprintf( stderr, "%s: X error #%d %s\n",
           APP_NAME, ev->error_code, err_buf[0]?err_buf:"(Unkown Error)");
  fprintf( stderr,
   "type=%d display=%p resource=%ld serial=%ld error=%d request=%d minor=%d\n",
    ev->type, (void*)ev->display, ev->resourceid,  ev->serial,
    ev->error_code, ev->request_code, ev->minor_code );
  return -1;
}


static void draw_net_strings()
{
  colornames bg;
  if (collapsed) {
    draw_string(">> ",gray, 0);
    return;
  }
  switch (net_speed_status) {
    case nsDownload: {
      bg=green;
      break;
    }
    case nsUpload: {
      bg=red;
      break;
    }
    default:{
      bg=gray;
      break;
    }
  }
  if (opts.show_bps) { draw_string(curr_inf.bps_str, bg,    win.tabs.bps); }
  if (opts.show_lip) { draw_string(curr_inf.lip_str, base, win.tabs.lip); }
  if (opts.show_rip) { draw_string(curr_inf.rip_str, base, win.tabs.rip); }
}


static colornames set_color_from_pct(uchar pct)
{
  if ( pct > 90 ) { return red; }
  if ( pct > 80 ) { return redorange; }
  if ( pct > 70 ) { return orange; }
  if ( pct > 60 ) { return yelloworange; }
  if ( pct > 50 ) { return yellow; }
  if ( pct > 40 ) { return yellowgreen; }
  if ( pct > 30 ) { return green; }
  if ( pct > 20 ) { return bluegreen; }
  if ( pct > 10 ) { return blue; }
  return violet;
}


static uchar percent(double numer, double denom)
{
  uchar rv;
  if (!denom) {return 0;}
  rv = (uchar) ((((double)numer)/((double)denom))*100);
  return (rv<100)?rv:100;
}


static char erase_proc_str[PROC_W];


void draw_mem_strings(int update)
{
  colornames bg;
  if (collapsed) { return; }
  if (update) { get_mem_info(); }
  if (opts.show_cpu && ( (!update) || (curr_inf.cpu_pct != prev_inf.cpu_pct))) {
    bg=set_color_from_pct(curr_inf.cpu_pct);
    draw_string(curr_inf.cpu_str, bg, win.tabs.cpu);
  }
  if (opts.show_bat && ( (!update) || (curr_inf.bat_pct != prev_inf.bat_pct)
          || (curr_inf.bat_stat != prev_inf.bat_stat) )) {
    bg=set_color_from_pct(100-curr_inf.bat_pct);
    draw_string(curr_inf.bat_str, bg, win.tabs.bat);
  }
  if (opts.show_ram && ( (!update) || (curr_inf.ram_used != prev_inf.ram_used))) {
    bg=set_color_from_pct(percent(curr_inf.ram_used, curr_inf.ram_total));
    draw_string(curr_inf.ram_str, bg, win.tabs.ram);
  }
  if (opts.show_swp && ( (!update) || (curr_inf.swap_used != prev_inf.swap_used)
          || ( curr_inf.swap_total != prev_inf.swap_total ) )) {
    bg=set_color_from_pct(percent(curr_inf.swap_used,curr_inf.swap_total));
    draw_string(curr_inf.swap_str, bg, win.tabs.swp);
  }
  if (opts.show_hog) {
    draw_string( erase_proc_str, base, win.tabs.proc);
    if ( curr_inf.proc_str[0] != ' ' ) {
      draw_string( curr_inf.proc_str, gray, win.tabs.proc);
    }
  }
  memcpy(&prev_inf, &curr_inf, sizeof(SysInfo));
}


void draw_time_string()
{
  if (opts.show_hms && !collapsed) {
    time_t now;
    struct tm *loctime;
    now = time(NULL);
    loctime = localtime(&now);
    strftime(curr_inf.hms_str, HMS_W, "%H:%M:%S ", loctime);
    draw_string( curr_inf.hms_str, base, win.tabs.hms);
  }
}


static int run ()
{
  int running=1;
  XEvent ev;
  struct timespec req,rem;

  win.dpy=XOpenDisplay (NULL);
  if ( !win.dpy ) {
    fprintf(stderr, "Can't open display !\n");
    return 1;
  }
  win.scr.id=DefaultScreen(win.dpy);
  win.scr.height=DisplayHeight(win.dpy, win.scr.id);
  win.scr.width=DisplayWidth(win.dpy, win.scr.id);
  win.root=RootWindow(win.dpy, win.scr.id);
  win.cmap=DefaultColormap(win.dpy, win.scr.id);
  XSetErrorHandler(handle_error);

  resource_init();
  create_window();
  XSync(win.dpy, 0);
  while (running) {
    while ( XPending(win.dpy) ) {
      XNextEvent(win.dpy, &ev);
      switch (ev.type) {
        case ButtonPress: {
          handle_press(ev.xbutton.x, ev.xbutton.y, ev.xbutton.button);
          break;
        }
        case Expose:{
          draw_net_strings();
          draw_mem_strings(0);
          draw_time_string();
          break;
        }
        default:{}
      }
    }
    req.tv_sec=1;
    req.tv_nsec=0;
    nanosleep(&req, &rem);
    get_net_info();
    if ( net_speed_status != nsUnchanged ) { draw_net_strings(); }
    draw_mem_strings(1);
    draw_time_string();
  }
  return 0;
}


static void set_fontsize(char *dst, char *src )
{
  if (dst) {
    if (strlen(src)<=2) {
      dst[0]=src[0];
      if (src[1]) {
        dst[1]=src[1];
      } else {
        memmove(&dst[1],&dst[2],strlen(&dst[1]));
      }
    } else {
      fprintf(stderr, "%s: Invalid font size \"%s\"\n", APP_NAME, src);
    }
  } else {
    fprintf( stderr, "%s: Can't set font size, %s already specified.\n",
                 APP_NAME, FONTSPEC_ENV);
  }
}


static void usage(char *app, uchar code )
{
  printf(
"\n\
Usage: %s [-tkbierscpl] [interface] [fontsize]\n\
  -t: place bar at Top of screen\n\
  -k: exclude clocK\n\
  -b: exclude Bandwidth\n\
  -i: exclude local IP address\n\
  -e: exclude remote Endpoint address\n\
  -a: exclude bAttery usage\n\
  -r: exclude Ram usage\n\
  -s: exclude Swap usage\n\
  -c: exclude Cpu usage\n\
  -p: exclude top Process info\n\
  -l: List available interfaces and exit\n\
\n\
[interface] is a name like \"ppp0\" or \"eth1\"\n\
[fontsize] is a number, likely: 8,10,12,14,18,24\n\
\n\
Mouse buttons:1=fold/open; 2=top/bottom; 3=quit\n\
\n\
Environment variables:\n\
  "FONTSPEC_ENV": Sets a custom font\n\
  "BGCOLOR_ENV":  Sets base color\n\
  "FGCOLOR_ENV":  Sets text color for IP and clock\n\
  "APPNAME_ENV":  Sets a custom window name\n\
\n\
",
  app);
  exit(code);
}



int main(int argc, char *argv[])
{
  int i;
  char *fontsize=NULL;
  char *env_font=getenv(FONTSPEC_ENV);

  back_color=getenv(BGCOLOR_ENV);
  fore_color=getenv(FGCOLOR_ENV);
  app_name=getenv(APPNAME_ENV);

  win.height=18;
  win.top=4;

  opts.show_bps = 1;
  opts.show_lip = 1;
  opts.show_rip = 1;
  opts.show_ram = 1;
  opts.show_swp = 1;
  opts.show_cpu = 1;
  opts.show_hog = 1;
  opts.show_hms = 1;
  opts.show_bat = 1;
  opts.start_at_top = 0;
  memset(&curr_inf, 0, sizeof(curr_inf));
  memset(&prev_inf, 0, sizeof(prev_inf));
  curr_inf.lip_str[0] = ' ';
  curr_inf.rip_str[0] = ' ';
  curr_inf.cpu_pct=100;

  curr_inf.proc_str[0]=' ';
  memset(erase_proc_str, ' ', sizeof(erase_proc_str));
  erase_proc_str[sizeof(erase_proc_str)-1]='\0';
  if (env_font && *env_font) {
    strncpy(font_name, env_font, sizeof(font_name)-1);
  } else {
    fontsize=strstr(font_name, "//");
    memmove(fontsize, DEFAULT_FONTSIZE, 2);
  }
  for (i=1; i<argc; i++) {
    if (argv[i] && argv[i][0]) {
      if ('-' == argv[i][0])  {
        char *c;
        for (c=argv[i]+1; *c; c++) {
          switch (*c) {
            case 't':{ opts.start_at_top=1; break;}
            case 'b':{ opts.show_bps=0; break; }
            case 'i':{ opts.show_lip=0; break; }
            case 'e':{ opts.show_rip=0; break; }
            case 'r':{ opts.show_ram=0; break; }
            case 's':{ opts.show_swp=0; break; }
            case 'c':{ opts.show_cpu=0; break; }
            case 'p':{ opts.show_hog=0; break; }
            case 'k':{ opts.show_hms=0; break; }
            case 'a':{ opts.show_bat=0; break; }
            case 'h':{ usage(argv[0], 0); break; }
            case 'l':{ show_ifnames(); return 0; }
            default:{
              fprintf(stderr, "Unknown Option: -%c\n", *c);
              fprintf(stderr, "Try `%s -h' for help\n", argv[0]);
              exit(1);
            }
          }
        }
      }  else {
        if ( (argv[i][0]>='0') && (argv[i][0]<='9') ) {
          set_fontsize(fontsize, argv[i]);
        } else {
         strncpy(curr_inf.if_name, argv[i], sizeof(curr_inf.if_name)-1);
        }
      }
    }
  }
  if ((opts.show_bps||opts.show_lip||opts.show_rip)&&(!curr_inf.if_name[0])) {
    choose_ifname(curr_inf.if_name);
    if (!curr_inf.if_name[0]) {
      strcpy(curr_inf.if_name, DEFAULT_IFNAME);
      fprintf(stderr, 
        "WARNING: No interface specified or detected, falling back to \"%s\"\n",
        curr_inf.if_name);
    } else {
      fprintf(stderr, "NOTE: Interface not specified, assuming \"%s\"\n", curr_inf.if_name);
    }
  }
  init_batfiles();
  chdir("/proc");
  oldpwd=getenv("PWD");
  if (oldpwd) {oldpwd=strdup(oldpwd);}
  return run();
}

