#include <X11/Xutil.h>

#define APP_NAME "says"



typedef enum {
  ai_MOTIF_WM_HINTS,
  ai_NET_CURRENT_DESKTOP,
  ai_NET_WORKAREA,
  ai_WIN_WORKSPACE,
  ai_WIN_WORKAREA,
  ai_WIN_HINTS,
  ai_NET_WM_STRUT,
  ai_NET_WM_STRUT_PARTIAL,
  ai_WIN_LAYER,
  ai_NET_NUMBER_OF_DESKTOPS,
  ai_WIN_WORKSPACE_COUNT,
  ai_WIN_STATE,
  ai_WM_STATE,
  ai_NET_WM_WINDOW_TYPE,
  ai_NET_WM_WINDOW_TYPE_DOCK,
  ai_NET_WM_STATE,
  ai_NET_WM_STATE_STAYS_ON_TOP
} AtomID;

extern Atom atoms[];

typedef struct {
  int id;
  unsigned short width;
  unsigned short height;
} ScreenInfo;

typedef struct {
  unsigned short hms, bps, lip, rip, cpu, ram, swp, proc;
} TabStops;


typedef struct {
  Display *dpy;
  Window root;
  Window win;
  unsigned short height;
  unsigned short top;
  ScreenInfo scr;
  XRectangle work_area;
  TabStops tabs;
  char char_width;
  unsigned int frame_color;
  Colormap cmap;
} WinInfo;


extern WinInfo win;

#define FONT_NAME_LEN 128
extern char font_name[FONT_NAME_LEN];

void toggle_strut();
void create_window();

#define change_property(atom,kind,fmt,ret,count) \
  ( XChangeProperty(win.dpy, win.win, atom, kind, fmt, \
    PropModeReplace, (uchar*)ret, count) )



typedef enum {
  violet,         blue,     bluegreen,   green,   yellowgreen, yellow,
  yelloworange,   orange,   redorange,   red,     gray,        base
} colornames;

#define colornames_last base

void resource_init ();
void draw_string(char *s, colornames bg, int bg_x);

extern char *back_color;
extern char *fore_color;