#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "window.h"

/* 
  Don't change the double-slash in the default font_name !
  It is a placeholder for the font size !!!
*/

#if XFT
#include <X11/Xft/Xft.h>
char font_name[FONT_NAME_LEN] = "Mono:size=//";
static  XftFont* ft_fnt=NULL;
#else
char font_name[FONT_NAME_LEN] =  "-misc-fixed-medium-r-normal-*-//-*-*-*-*-*-*-*";
#define FALLBACK_FONT "fixed"
static XFontStruct *xfnt=NULL;
#endif


char *back_color = NULL;
char *fore_color = NULL;

static char char_top;
static GC fore_gc;


typedef struct {
  uchar  r, g, b;
} RGB;


static RGB paper_colors[] = {
  { 0xA0, 0x60, 0xF0 },  /* violet */
  { 0x80, 0x80, 0xFF },  /* blue */
  { 0x00, 0xC0, 0xC0 },  /* bluegreen */
  { 0x00, 0xC0, 0x00 },  /* green */
  { 0xB0, 0xF0, 0x00 },  /* yellowgreen */
  { 0xF0, 0xF0, 0x80 },  /* yellow */
  { 0xFF, 0xE0, 0x00 },  /* yelloworange */
  { 0xFF, 0xC0, 0x80 },  /* orange */
  { 0xFF, 0x90, 0x00 },  /* redorange */
  { 0xE8, 0x30, 0x50 },  /* red */
  { 0xCC, 0xCC, 0xCC },  /* gray */
  { 0x00, 0x00, 0x00 }   /* base */
};


static RGB pencil_colors[] = {
  { 0xFF, 0xFF, 0x80 },  /* ~violet */
  { 0xFF, 0xFF, 0x00 },  /* ~blue */
  { 0xFF, 0xFF, 0xE0 },  /* ~bluegreen */
  { 0xFF, 0xFF, 0xE0 },  /* ~green */
  { 0x00, 0x00, 0x00 },  /* ~yellowgreen */
  { 0x00, 0x00, 0x00 },  /* ~yellow */
  { 0x00, 0x00, 0x00 },  /* ~yelloworange */
  { 0x00, 0x00, 0x00 },  /* ~orange */
  { 0xFF, 0xFF, 0xC0 },  /* ~redorange */
  { 0xFF, 0xFF, 0xE0 },  /* ~red */
  { 0x00, 0x00, 0x00 },  /* ~gray */
  { 0xFF, 0xFF, 0x00 }   /* ~base */
};


static unsigned int paper_palette[colornames_last+1];
static unsigned int pencil_palette[colornames_last+1];


static int next_x(int curr_x, int curr_w)
{
  return (curr_x + ( curr_w * win.char_width ));
}



static void set_tab_stops() {
  win.tabs.hms   = win.char_width*2;
  win.tabs.bps   = next_x(win.tabs.hms, opts.show_hms?HMS_W:0);
  win.tabs.lip   = next_x(win.tabs.bps, opts.show_bps?BPS_W:0);
  win.tabs.rip   = next_x(win.tabs.lip, opts.show_lip?LIP_W:0);
  win.tabs.ram   = next_x(win.tabs.rip, opts.show_rip?RIP_W:0);
  win.tabs.swp   = next_x(win.tabs.ram, opts.show_ram?RAM_W:0);
  win.tabs.cpu   = next_x(win.tabs.swp, opts.show_swp?SWAP_W:0);
  win.tabs.proc  = next_x(win.tabs.cpu, opts.show_cpu?CPU_W:0);
}



static void invert_color(XColor *dst, const XColor *src)
{
  dst->red =   65535-src->red;
  dst->green = 65535-src->green;
  dst->blue =  65535-src->blue;
  if ((dst->red   > 0) && (dst->red   < 32768)) { dst->red   += 16384; }
  if ((dst->green > 0) && (dst->green < 32768)) { dst->green += 16384; }
  if ((dst->blue  > 0) && (dst->blue  < 32768)) { dst->blue  += 16384; }
}


static void set_base_colors()
{
  XColor fg,bg;

  memset(&bg,0,sizeof(XColor));
  if (back_color && !XParseColor(win.dpy, win.cmap, back_color, &bg)) {
    back_color=NULL;
  }
  if (fore_color && !XParseColor(win.dpy, win.cmap, fore_color, &fg)) {
    fore_color=NULL;
  }
  if (back_color) {
    XAllocColor(win.dpy, win.cmap, &bg);
    if (!fore_color) { invert_color(&fg,&bg); }
    XAllocColor(win.dpy, win.cmap, &fg);
  } else {
    if (!fore_color) { XParseColor(win.dpy, win.cmap, "#FFFFFF", &fg); }
    XAllocColor(win.dpy, win.cmap, &fg);
    invert_color(&bg,&fg);
    XAllocColor(win.dpy, win.cmap, &bg);
  }
  paper_palette[base]=  bg.pixel;
  pencil_palette[base]= fg.pixel;

  pencil_colors[base].r=fg.red   >> 8;
  pencil_colors[base].g=fg.green >> 8;
  pencil_colors[base].b=fg.blue  >> 8;

  win.frame_color=bg.pixel;
}


void resource_init ()
{
  XGCValues gcv;
  XColor xcl;
  colornames i;

  gcv.graphics_exposures = False;
  unsigned long gc_flags=GCGraphicsExposures;

  for (i=0; i<colornames_last; i++) {
    xcl.red=   paper_colors[i].r << 8;
    xcl.green= paper_colors[i].g << 8;
    xcl.blue=  paper_colors[i].b << 8;
    XAllocColor(win.dpy, win.cmap, &xcl);
    paper_palette[i]=xcl.pixel;
  }
  for (i=0; i<colornames_last; i++) {
    xcl.red=   pencil_colors[i].r << 8;
    xcl.green= pencil_colors[i].g << 8;
    xcl.blue=  pencil_colors[i].b << 8;
    XAllocColor(win.dpy, win.cmap, &xcl);
    pencil_palette[i]=xcl.pixel;
  }

  set_base_colors();

#if XFT
  XGlyphInfo exts;
  ft_fnt = XftFontOpenName (win.dpy, win.scr.id, font_name);
  char_top = ft_fnt->ascent + ((win.height - (ft_fnt->ascent + ft_fnt->descent)) / 2);
  XftTextExtents8(win.dpy,ft_fnt,(uchar*)"9",1,&exts);
  win.char_width = exts.xOff;
#else
  xfnt=XLoadQueryFont(win.dpy, font_name);
  if ( !xfnt ) {
    fprintf( stderr, "%s: ** Warning ** Failed to load font \"%s\"\n",
             APP_NAME, font_name );
    xfnt=XLoadQueryFont(win.dpy, FALLBACK_FONT);
    if ( !xfnt ) {
      fprintf( stderr, "%s: ** Warning ** Failed to load font \"%s\"\n",
               APP_NAME, FALLBACK_FONT );
    }
  }
  win.char_width = xfnt->max_bounds.width;
  char_top = (xfnt->max_bounds.ascent+xfnt->max_bounds.descent)-2;
  win.height = char_top+4;
  gcv.font = xfnt->fid;
  gc_flags |= GCFont;
#endif
  fore_gc=XCreateGC(win.dpy, win.root, gc_flags, &gcv);
  set_tab_stops();
}


#if XFT

void draw_string(char *s, colornames bg, int bg_x)
{
  static XftDraw* xftdraw=NULL;
  XftColor col;
  col.color.red   = pencil_colors[bg].r << 8;
  col.color.green = pencil_colors[bg].g << 8;
  col.color.blue  = pencil_colors[bg].b << 8;
  col.color.alpha = 0xffff;

  int bg_width=(strlen(s)+1.5)*win.char_width;
  if (!xftdraw) {
    xftdraw = XftDrawCreate(win.dpy, win.win, DefaultVisual(win.dpy, win.scr.id), win.cmap);
  }
  XSetForeground(win.dpy, fore_gc, paper_palette[bg]);
  XFillRectangle(win.dpy, win.win, fore_gc, bg_x, 0, bg_width, win.height);
  XftDrawString8(xftdraw, &col, ft_fnt, bg_x+win.char_width, char_top, (uchar*)s, strlen(s));
  XSync(win.dpy,win.scr.id);
}

#else

void draw_string(char *s, colornames bg, int bg_x)
{
  int bg_width = XTextWidth(xfnt, s, strlen(s)-1) + ( win.char_width * 2 );
  XSetForeground(win.dpy, fore_gc, paper_palette[bg]);
  XFillRectangle(win.dpy, win.win, fore_gc, bg_x, 0, bg_width, win.height);
  XSetForeground(win.dpy, fore_gc, pencil_palette[bg]);
  XDrawString(win.dpy, win.win, fore_gc, bg_x+win.char_width, char_top, s, strlen(s));
}

#endif
