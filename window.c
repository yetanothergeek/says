#include <string.h>
#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "common.h"
#include "window.h"

#define CLS_NAME "Says"

#define  MWM_HINTS_DECORATIONS      1 << 1
#define  WIN_STATE_STICKY           1 << 0
#define  WIN_STATE_FIXED_POSITION   1 << 8
#define  WIN_HINTS_SKIP_FOCUS       1 << 0
#define  WIN_HINTS_SKIP_WINLIST     1 << 1
#define  WIN_HINTS_SKIP_TASKBAR     1 << 2
#define  WIN_LAYER_ABOVE_DOCK  10
#define  WIN_LAYER_DOCK  2
#define NOT_A_DESKTOP 65535


WinInfo win;

static XTextProperty wmn = {(uchar*)APP_NAME,XA_STRING,8,sizeof(APP_NAME)-1};
static XClassHint wmch = {APP_NAME,CLS_NAME};


static char*  atom_names[] = {
    "_MOTIF_WM_HINTS",
    "_NET_CURRENT_DESKTOP",
    "_NET_WORKAREA",
    "_WIN_WORKSPACE",
    "_WIN_WORKAREA",
    "_WIN_HINTS",
    "_NET_WM_STRUT",
    "_NET_WM_STRUT_PARTIAL",
    "_WIN_LAYER",
    "_NET_NUMBER_OF_DESKTOPS",
    "_WIN_WORKSPACE_COUNT",
    "_WIN_STATE",
    "WM_STATE",
    "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_STATE",
    "_NET_WM_STATE_STAYS_ON_TOP"
};

#define ATOM_COUNT sizeof(atom_names)/sizeof(char*)


Atom atoms[ATOM_COUNT];


void toggle_strut()
{
  static uchar at_top=1;
  CARD32 strut[12] = {0,0,0,0,0,0,0,0,0,0,0,0}; /* left, right, top, bottom */
  enum {
    STRUT_LEFT    = 0,
    STRUT_RIGHT   = 1,
    STRUT_TOP     = 2,
    STRUT_BOTTOM  = 3
  };
  at_top= !at_top;
  if (at_top) {
    strut[STRUT_TOP] = win.height + 1 + win.work_area.y;
    strut[10] = win.work_area.width;
  } else {
    strut[STRUT_BOTTOM] = win.height + 3;
    strut[11] = win.work_area.width;
  }
  change_property(atoms[ai_NET_WM_STRUT], XA_CARDINAL, 32, &strut, 4 );
  change_property(atoms[ai_NET_WM_STRUT_PARTIAL], XA_CARDINAL, 32, &strut, 12 );

  XMoveResizeWindow( win.dpy, win.win, win.work_area.x,
                     at_top?win.work_area.y:(win.work_area.y+win.work_area.height)-win.height,
                     win.work_area.width, win.height);
}


static int get_net_workarea()
{
  unsigned long*data=NULL;
  Atom type_ret;
  int format_ret;
  unsigned long items_ret;
  unsigned long after_ret;
  unsigned long desktop;
  XGetWindowProperty( win.dpy, win.root, atoms[ai_NET_CURRENT_DESKTOP], 0, 1,
                      False, XA_CARDINAL, &type_ret, &format_ret,
                      &items_ret, &after_ret, (void*)&data );
  if ( data && items_ret ) {
    desktop= *data;
    XFree(data);
    data=NULL;
    XGetWindowProperty( win.dpy, win.root, atoms[ai_NET_WORKAREA],
                        desktop*4, 4, False, XA_CARDINAL, &type_ret,
                        &format_ret, &items_ret, &after_ret, (void*)&data );
    if ( data ) {
      if ( 4 == items_ret ) {
        win.work_area.x=data[0];
        win.work_area.y=data[1];
        win.work_area.width=data[2];
        win.work_area.height=data[3];
        XFree(data);
        return 1;
      }
      XFree(data);
    }
  }
  return 0;
}


static int get_win_workarea()
{
  unsigned long*data=NULL;
  Atom type_ret;
  int format_ret;
  unsigned long items_ret;
  unsigned long after_ret;

  data=NULL;
  XGetWindowProperty( win.dpy, win.root, atoms[ai_WIN_WORKAREA], 0, 4, False,
                      XA_CARDINAL, &type_ret, &format_ret, &items_ret,
                      &after_ret, (void*)&data );
  if ( data ) {
    if ( 4 == items_ret ) {
      win.work_area.x=data[0];
      win.work_area.y=data[1];
      win.work_area.width=data[2];
      win.work_area.height=data[3];
      XFree(data);
      return 1;
    }
    XFree(data);
  }
  return 0;
}


static int get_default_workarea()
{
  win.work_area.x=0;
  win.work_area.y=0;
  win.work_area.width=win.scr.width;
  win.work_area.height=win.scr.height;
  return 1;
}


int get_work_area()
{
  return get_net_workarea()||get_win_workarea()||get_default_workarea();
}



typedef struct {
  CARD32 flags;
  CARD32 functions;
  CARD32 decorations;
  INT32 input_mode;
  CARD32 status;
} MwmHints;


void create_window()
{
  MwmHints mwm;
  XSizeHints size_hints;
  XWMHints wmhints;
  XSetWindowAttributes att;
  CARD32 win_width=win.scr.width;
  int win_left=0;
  unsigned int card;
  Atom atom;
  XInternAtoms(win.dpy, atom_names, ATOM_COUNT, 0, &atoms[0]);
  att.background_pixel= win.frame_color;
  att.event_mask= ButtonPressMask | ExposureMask;

  win.top = win.scr.height-win.height;
  win.win = XCreateWindow( win.dpy, win.root,
                            win_left, win.top, win_width, win.height,
                            0, CopyFromParent, InputOutput,
                            XDefaultVisual(win.dpy, win.scr.id),
                            CWBackPixel | CWEventMask, &att );

  XSetWMName(win.dpy, win.win, &wmn);
  XSetWMIconName(win.dpy, win.win, &wmn);
  XSetClassHint(win.dpy, win.win, &wmch);

  get_work_area();

  atom=atoms[ai_NET_WM_WINDOW_TYPE_DOCK];
  change_property(atoms[ai_NET_WM_WINDOW_TYPE], XA_ATOM, 32, &atom, 1);

  atom=atoms[ai_NET_WM_STATE_STAYS_ON_TOP];
  change_property(atoms[ai_NET_WM_STATE], XA_ATOM, 32, &atom, 1);

  card=WIN_STATE_STICKY | WIN_STATE_FIXED_POSITION;
  change_property(atoms[ai_WIN_STATE], XA_CARDINAL, 32, &card,1);

  card= WIN_HINTS_SKIP_FOCUS | WIN_HINTS_SKIP_WINLIST | WIN_HINTS_SKIP_TASKBAR;
  change_property(atoms[ai_WIN_HINTS], XA_CARDINAL, 32, &card, 1);

  mwm.flags=0;
  memset(&mwm, '\0', sizeof(mwm));

  mwm.flags= MWM_HINTS_DECORATIONS;   /* borderless motif hint */
  change_property( atoms[ai_MOTIF_WM_HINTS], atoms[ai_MOTIF_WM_HINTS], 32,
                   &mwm, sizeof(MwmHints) / 4 );

  size_hints.flags= PPosition;   /* tell the WM to obey the window position */
  change_property( XA_WM_NORMAL_HINTS,  XA_WM_SIZE_HINTS, 32, &size_hints,
                   sizeof (XSizeHints) / 4 );

  wmhints.flags= InputHint;   /* Make the window unfocusable */
  wmhints.input= 0;
  change_property(XA_WM_HINTS, XA_WM_HINTS, 32, &wmhints, sizeof(XWMHints) / 4 );

  card=WIN_LAYER_DOCK;
  change_property(atoms[ai_WIN_LAYER], XA_CARDINAL, 32, &card, 1);

  XMapWindow(win.dpy, win.win);

  toggle_strut();
  if (opts.start_at_top) { toggle_strut(); }

}

