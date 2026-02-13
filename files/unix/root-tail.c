/*
 * Copyright 2001 by Marco d'Itri <md@linux.it>
 *
 * Original version by Mike Baker, then maintained by pcg@goof.com.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#if HAS_REGEX
#include <regex.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* data structures */
struct logfile_entry {
    char *fname;		/* name of file                         */
    char *desc;			/* alternative description              */
    FILE *fp;			/* FILE struct associated with file     */
    ino_t inode;		/* inode of the file opened		*/
    off_t last_size;		/* file size at the last check		*/
    unsigned long color;	/* color to be used for printing        */
    struct logfile_entry *next;
};

struct linematrix {
    char *line;
    unsigned long color;
};


/* global variables */
unsigned int width = STD_WIDTH, listlen = STD_HEIGHT;
int win_x = LOC_X, win_y = LOC_Y;
int w = -1, h = -1, font_width, font_height, font_descent;
int do_reopen;
struct timeval interval = { 2, 400000 };	/* see Knuth */

/* command line options */
int opt_noinitial, opt_shade, opt_frame, opt_reverse, opt_nofilename,
    geom_mask, reload = 3600;
const char *command = NULL,
    *fontname = USE_FONT, *dispname = NULL, *def_color = DEF_COLOR;

struct logfile_entry *loglist = NULL, *loglist_tail = NULL;

Display *disp;
Window new_root, real_root;
GC WinGC;

#if HAS_REGEX
struct re_list {
    regex_t from;
    const char *to;
    struct re_list *next;
};
struct re_list *re_head, *re_tail;
#endif


/* prototypes */
void list_files(int);
void force_reopen(int);
void force_refresh(int);
void blank_window(int);
Window ToonGetRootWindow(Display *, int, Window *);


void InitWindow(void);
unsigned long GetColor(const char *);
void redraw(void);
void refresh(struct linematrix *, int, int);

void transform_line(char *s);
int lineinput(char *, int, FILE *);
void reopen(void);
void check_open_files(void);
FILE *openlog(struct logfile_entry *);
void main_loop(void);

void display_version(void);
void display_help(char *);
void install_signal(int, void (*)(int));
void *xstrdup(const char *);
void *xmalloc(size_t);
int daemonize(void);

/* signal handlers */
void list_files(int dummy)
{
    struct logfile_entry *e;

    fprintf(stderr, "Files opened:\n");
    for (e = loglist; e; e = e->next)
	fprintf(stderr, "\t%s (%s)\n", e->fname, e->desc);
}

void force_reopen(int dummy)
{
    do_reopen = 1;
}

void force_refresh(int dummy)
{
    XClearWindow(disp, new_root);
    redraw();
}

void blank_window(int dummy)
{
    XClearWindow(disp, new_root);
    XFlush(disp);
    exit(0);
}

/* X related functions */
unsigned long GetColor(const char *ColorName)
{
    XColor Color;
    XWindowAttributes Attributes;

    XGetWindowAttributes(disp, real_root, &Attributes);
    Color.pixel = 0;
    if (!XParseColor(disp, Attributes.colormap, ColorName, &Color))
	fprintf(stderr, "can't parse %s\n", ColorName);
    else if (!XAllocColor(disp, Attributes.colormap, &Color))
	fprintf(stderr, "can't allocate %s\n", ColorName);
    return Color.pixel;
}

void InitWindow(void)
{
    XGCValues gcv;
    Font font;
    unsigned long gcm;
    XFontStruct *info;
    int screen, ScreenWidth, ScreenHeight;

    if (!(disp = XOpenDisplay(dispname))) {
	fprintf(stderr, "Can't open display %s.\n", dispname);
	exit(1);
    }
    screen = DefaultScreen(disp);
    ScreenHeight = DisplayHeight(disp, screen);
    ScreenWidth = DisplayWidth(disp, screen);
    real_root = RootWindow(disp, screen);
    new_root = ToonGetRootWindow( disp, screen, &real_root );
    gcm = GCBackground;
    gcv.graphics_exposures = True;
    WinGC = XCreateGC(disp, new_root, gcm, &gcv);
    XMapWindow(disp, new_root);
    XSetForeground(disp, WinGC, GetColor(DEF_COLOR));

    font = XLoadFont(disp, fontname);
    XSetFont(disp, WinGC, font);
    info = XQueryFont(disp, font);
    font_width = info->max_bounds.width;
    font_descent = info->max_bounds.descent;
    font_height = info->max_bounds.ascent + font_descent;

    w = width * font_width;
    h = listlen * font_height;
    if (geom_mask & XNegative)
	win_x = win_x + ScreenWidth - w;
    if (geom_mask & YNegative)
	win_y = win_y + ScreenHeight - h;

    XSelectInput(disp, new_root, ExposureMask|FocusChangeMask);
}

char * 
detabificate (char *s)
{
  char * out;
  int i, j;

  out = malloc (8 * strlen (s) + 1);

  for(i = 0, j = 0; s[i]; i++) 
    {
      if (s[i] == '\t') 
        do 
          out[j++] = ' ';
        while (j % 8);      
      else
        out[j++] = s[i];
    }
  
  out[j] = '\0';
  return out;
}

/*
 * redraw does a complete redraw, rather than an update (i.e. the area
 * gets cleared first)
 * the rest is handled by regular refresh()'es
 */
void redraw(void)
{
    XClearArea(disp, new_root, win_x, win_y, w, h + font_descent + 2, True);
}

/* Just redraw everything without clearing (i.e. after an EXPOSE event) */
void refresh(struct linematrix *lines, int miny, int maxy)
{
    int lin;
    int offset = (listlen + 1) * font_height;
    unsigned long black_color = GetColor("black");

    miny -= win_y + font_height;
    maxy -= win_y - font_height;

    for (lin = listlen; lin--;)
      {
        char *temp;
  
        offset -= font_height;
  
        if (offset < miny || offset > maxy)
          continue;

#define LIN ((opt_reverse)?(listlen-lin-1):(lin))
        temp = detabificate (lines[LIN].line);
  
        if (opt_shade)
          {
            XSetForeground (disp, WinGC, black_color);
            XDrawString (disp, new_root, WinGC, win_x + 2, win_y + offset + 2,
                         temp, strlen (temp));
          }
  
        XSetForeground (disp, WinGC, lines[LIN].color);
        XDrawString (disp, new_root, WinGC, win_x, win_y + offset,
  		   temp, strlen (temp));
        
        free (temp);
    }

    if (opt_frame) {
	int bot_y = win_y + h + font_descent + 2;

	XDrawLine(disp, new_root, WinGC, win_x, win_y, win_x + w, win_y);
	XDrawLine(disp, new_root, WinGC, win_x + w, win_y, win_x + w, bot_y);
	XDrawLine(disp, new_root, WinGC, win_x + w, bot_y, win_x, bot_y);
	XDrawLine(disp, new_root, WinGC, win_x, bot_y, win_x, win_y);
    }
}

#if HAS_REGEX
void transform_line(char *s)
{
#ifdef I_AM_Md
    int i;
    if (1) {
	for (i = 16; s[i]; i++)
	    s[i] = s[i + 11];
    }
    s[i + 1] = '\0';
#endif

    if (transformre) {
	int i;
	regmatch_t matched[16];

	i = regexec(&transformre, string, 16, matched, 0);
	if (i == 0) {			/* matched */
	}
    }
}
#endif


/*
 * This routine should read 'width' characters and not more. However,
 * we really want to read width + 1 charachters if the last char is a '\n',
 * which we should remove afterwards. So, read width+1 chars and ungetc
 * the last character if it's not a newline. This means 'string' must be
 * width + 2 wide!
 */
int lineinput(char *string, int slen, FILE *f)
{
    int len;

    do {
	if (fgets(string, slen, f) == NULL)	/* EOF or Error */
	    return 0;

	len = strlen(string);
    } while (len == 0);

    if (string[len - 1] == '\n')
	string[len - 1] = '\0';			/* erase newline */
    else if (len >= slen - 1) {
	ungetc(string[len - 1], f);
	string[len - 1] = '\0';
    }

#if HAS_REGEX
    transform_line(string);
#endif
    return len;
}

/* input: reads file->fname
 * output: fills file->fp, file->inode
 * returns file->fp
 * in case of error, file->fp is NULL
 */
FILE *openlog(struct logfile_entry *file)
{
    struct stat stats;

    if ((file->fp = fopen(file->fname, "r")) == NULL) {
	file->fp = NULL;
	return NULL;
    }

    fstat(fileno(file->fp), &stats);
    if (S_ISFIFO(stats.st_mode)) {
	if (fcntl(fileno(file->fp), F_SETFL, O_NONBLOCK) < 0)
	    perror("fcntl"), exit(1);
	file->inode = 0;
    } else
	file->inode = stats.st_ino;

    if (opt_noinitial)
      fseek (file->fp, 0, SEEK_END);
    else if (stats.st_size > (listlen + 1) * width)
      {
        char dummy[255];

        fseek(file->fp, -((listlen + 2) * width), SEEK_END);
        /* the pointer might point halfway some line. Let's
           be nice and skip this damaged line */
        lineinput(dummy, sizeof(dummy), file->fp);
      }

    file->last_size = stats.st_size;
    return file->fp;
}

void reopen(void)
{
    struct logfile_entry *e;

    for (e = loglist; e; e = e->next) {
	if (!e->inode)
	    continue;			/* skip stdin */

	if (e->fp)
	    fclose(e->fp);
	/* if fp is NULL we will try again later */
	openlog(e);
    }

    do_reopen = 0;
}

void check_open_files(void)
{
    struct logfile_entry *e;
    struct stat stats;

    for (e = loglist; e; e = e->next) {
	if (!e->inode)
	    continue;				/* skip stdin */

	if (stat(e->fname, &stats) < 0) {	/* file missing? */
	    sleep(1);
	    if (e->fp)
		fclose(e->fp);
	    if (openlog(e) == NULL)
		break;
	}

	if (stats.st_ino != e->inode)	{	/* file renamed? */
	    if (e->fp)
		fclose(e->fp);
	    if (openlog(e) == NULL)
		break;
	}

	if (stats.st_size < e->last_size) {	/* file truncated? */
	    fseek(e->fp, 0, SEEK_SET);
	    e->last_size = stats.st_size;
	}
    }
}

#define SCROLL_UP(lines, listlen)				\
{								\
    int cur_line;						\
    for (cur_line = 0; cur_line < (listlen - 1); cur_line++) {	\
	strcpy(lines[cur_line].line, lines[cur_line + 1].line);	\
	lines[cur_line].color = lines[cur_line + 1].color;	\
    }								\
}

void main_loop(void)
{
    struct linematrix *lines = xmalloc(sizeof(struct linematrix) * listlen);
    int lin, miny, maxy, buflen;
    char *buf;
    time_t lastreload;
    Region region = XCreateRegion();
    XEvent xev;

    maxy = 0;
    miny = win_y + h;
    buflen = width + 2;
    buf = xmalloc(buflen);
    lastreload = time(NULL);

    /* Initialize linematrix */
    for (lin = 0; lin < listlen; lin++) {
	lines[lin].line = xmalloc(buflen);
	strcpy(lines[lin].line, "~");
	lines[lin].color = GetColor(def_color);
    }

    if (!opt_noinitial)
	while (lineinput(buf, buflen, loglist->fp) != 0) {
	    SCROLL_UP(lines, listlen);
	    /* print the next line */
	    strcpy(lines[listlen - 1].line, buf);
	}

    for (;;) {
	int need_update = 0;
	struct logfile_entry *current;
	static struct logfile_entry *lastprinted = NULL;

	/* read logs */
	for (current = loglist; current; current = current->next) {
	    if (!current->fp)
		continue;		/* skip missing files */

	    clearerr(current->fp);

	    while (lineinput(buf, buflen, current->fp) != 0) {
		/* print filename if any, and if last line was from
		   different file */
		if (!opt_nofilename &&
			!(lastprinted && lastprinted == current) &&
			current->desc[0]) {
		    SCROLL_UP(lines, listlen);
		    sprintf(lines[listlen - 1].line, "[%s]", current->desc);
		    lines[listlen - 1].color = current->color;
		}

		SCROLL_UP(lines, listlen);
		strcpy(lines[listlen - 1].line, buf);
		lines[listlen - 1].color = current->color;

		lastprinted = current;
		need_update = 1;
	    }
	}

	if (need_update)
	    redraw();
	else {
	    XFlush(disp);
	    if (!XPending(disp)) {
		fd_set fdr;
		struct timeval to = interval;

		FD_ZERO(&fdr);
		FD_SET(ConnectionNumber(disp), &fdr);
		select(ConnectionNumber(disp) + 1, &fdr, 0, 0, &to);
	    }
	}

	check_open_files();

	if (do_reopen)
	    reopen();

	/* we ignore possible errors due to window resizing &c */
	while (XPending(disp)) {
	    XNextEvent(disp, &xev);
	    switch (xev.type) {
	    case Expose:
		{
		    XRectangle r;

		    r.x = xev.xexpose.x;
		    r.y = xev.xexpose.y;
		    r.width = xev.xexpose.width;
		    r.height = xev.xexpose.height;
		    XUnionRectWithRegion(&r, region, region);
		    if (miny > r.y)
			miny = r.y;
		    if (maxy < r.y + r.height)
			maxy = r.y + r.height;
		}
		break;
	    default:
#ifdef DEBUGMODE
		fprintf(stderr, "PANIC! Unknown event %d\n", xev.type);
#endif
		break;
	    }
	}

	/* reload if requested */
	if (reload && lastreload + reload < time(NULL)) {
	    if (command)
		system(command);

	    reopen();
	    lastreload = time(NULL);
	}

	if (!XEmptyRegion(region)) {
	    XSetRegion(disp, WinGC, region);
	    refresh(lines, miny, maxy);
	    XDestroyRegion(region);
	    region = XCreateRegion();
	    maxy = 0;
	    miny = win_y + h;
	}
    }
}


int main(int argc, char *argv[])
{
    int i;
    int opt_daemonize = 0;
#if HAS_REGEX
    char *transform = NULL;
#endif

    /* window needs to be initialized before colorlookups can be done */
    /* just a dummy to get the color lookups right */
    geom_mask = NoValue;
    InitWindow();

    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-' && argv[i][1] != '\0' && argv[i][1] != ',') {
	    if (!strcmp(argv[i], "--?") ||
		!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
		display_help(argv[0]);
	    else if (!strcmp(argv[i], "-V"))
		display_version();
	    else if (!strcmp(argv[i], "-g") || !strcmp(argv[i], "-geometry"))
		geom_mask = XParseGeometry(argv[++i],
			&win_x, &win_y, &width, &listlen);
	    else if (!strcmp(argv[i], "-display"))
		dispname = argv[++i];
	    else if (!strcmp(argv[i], "-font") || !strcmp(argv[i], "-fn"))
		fontname = argv[++i];
#if HAS_REGEX
	    else if (!strcmp(argv[i], "-t"))
		transform = argv[++i];
#endif
	    else if (!strcmp(argv[i], "-fork") || !strcmp(argv[i], "-f"))
		opt_daemonize = 1;
	    else if (!strcmp(argv[i], "-reload")) {
		reload = atoi(argv[++i]);
		command = argv[++i];
	    }
	    else if (!strcmp(argv[i], "-shade"))
		opt_shade = 1;
	    else if (!strcmp(argv[i], "-frame"))
		opt_frame = 1;
	    else if (!strcmp(argv[i], "-no-filename"))
		opt_nofilename = 1;
	    else if (!strcmp(argv[i], "-reverse"))
		opt_reverse = 1;
	    else if (!strcmp(argv[i], "-color"))
		def_color = argv[++i];
	    else if (!strcmp(argv[i], "-noinitial"))
		opt_noinitial = 1;
	    else if (!strcmp(argv[i], "-interval") || !strcmp(argv[i], "-i")) {
		double iv = atof(argv[++i]);

		interval.tv_sec = (int) iv;
		interval.tv_usec = (iv - interval.tv_sec) * 1e6;
	    } else {
		fprintf(stderr, "Unknown option '%s'.\n"
			"Try --help for more information.\n", argv[i]);
		exit(1);
	    }
	} else {		/* it must be a filename */
	    struct logfile_entry *e;
	    const char *fname, *desc, *fcolor = def_color;
	    char *p;

	    /* this is not foolproof yet (',' in filenames are not allowed) */
	    fname = desc = argv[i];
	    if ((p = strchr(argv[i], ','))) {
		*p = '\0';
		fcolor = p + 1;

		if ((p = strchr(fcolor, ','))) {
		    *p = '\0';
		    desc = p + 1;
		}
	    }

	    e = xmalloc(sizeof(struct logfile_entry));
	    if (argv[i][0] == '-' && argv[i][1] == '\0') {
		if ((e->fp = fdopen(0, "r")) == NULL)
		    perror("fdopen"), exit(1);
		if (fcntl(0, F_SETFL, O_NONBLOCK) < 0)
		    perror("fcntl"), exit(1);
		e->fname = NULL;
		e->inode = 0;
		e->desc = xstrdup("stdin");
	    } else {
		int l;

		e->fname = xstrdup(fname);
		if (openlog(e) == NULL)
		    perror(fname), exit(1);

		l = strlen(desc);
		if (l > width - 2)		/* must account for [ ] */
		    l = width - 2;
		e->desc = xmalloc(l + 1);
		memcpy(e->desc, desc, l);
		*(e->desc + l) = '\0';
	    }

	    e->color = GetColor(fcolor);
	    e->next = NULL;

	    if (!loglist)
		loglist = e;
	    if (loglist_tail)
		loglist_tail->next = e;
	    loglist_tail = e;
	}
    }

    if (!loglist) {
	fprintf(stderr, "You did not specify any files to tail\n"
		"use %s --help for help\n", argv[0]);
	exit(1);
    }

#if HAS_REGEX
    if (transform) {
	int i;

	transformre = xmalloc(sizeof(transformre));
	i = regcomp(&transformre, transform, REG_EXTENDED);
	if (i != 0) {
	    char buf[512];

	    regerror(i, &transformre, buf, sizeof(buf));
	    fprintf(stderr, "Cannot compile regular expression: %s\n", buf);
	}
    }
#endif

    InitWindow();

    install_signal(SIGINT, blank_window);
    install_signal(SIGQUIT, blank_window);
    install_signal(SIGTERM, blank_window);
    install_signal(SIGHUP, force_reopen);
    install_signal(SIGUSR1, list_files);
    install_signal(SIGUSR2, force_refresh);

    if (opt_daemonize)
	daemonize();

    main_loop();

    exit(1);			/* to make gcc -Wall stop complaining */
}

void install_signal(int sig, void (*handler)(int))
{
    struct sigaction action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if (sigaction(sig, &action, NULL) < 0)
	fprintf(stderr, "sigaction(%d): %s\n", sig, strerror(errno)), exit(1);
}

void *xstrdup(const char *string)
{
    void *p;

    while ((p = strdup(string)) == NULL) {
        fprintf(stderr, "Memory exausted.");
	sleep(10);
    }
    return p;
}

void *xmalloc(size_t size)
{
    void *p;

    while ((p = malloc(size)) == NULL) {
        fprintf(stderr, "Memory exausted.");
	sleep(10);
    }
    return p;
}

void display_help(char *myname)
{
    printf("Usage: %s [options] file1[,color[,desc]] "
	   "[file2[,color[,desc]] ...]\n", myname);
    printf(" -g | -geometry geometry   -g WIDTHxHEIGHT+X+Y\n"
	    " -color    color           use color $color as default\n"
	    " -reload sec command       reload after $sec and run command\n"
	    "                           by default -- 3 mins\n"
	    " -font FONTSPEC            (-fn) font to use\n"
	    " -f | -fork                fork into background\n"
	    " -reverse                  print new lines at the top\n"
	    " -shade                    add shading to font\n"
	    " -noinitial                don't display the last file lines on\n"
	    "                           startup\n"
	    " -i | -interval seconds    interval between checks (fractional\n"
	    "                           values o.k.). Default 3\n"
	    " -V                        display version information and exit\n"
	    "\n");
    printf("Example:\n%s -g 80x25+100+50 -font fixed /var/log/messages,green "
	 "/var/log/secure,red,'ALERT'\n", myname);
    exit(0);
}

void display_version(void) {
    printf("root-tail version " VERSION "\n");
    exit(0);
}

int daemonize(void) {
    switch (fork()) {
    case -1:
	return -1;
    case 0:
	break;
    default:
	_exit(0);
    }

    if (setsid() == -1)
	return -1;

    return 0;
}


/* toon_root.c - finding the correct background window / virtual root
 * Copyright (C) 1999-2001  Robin Hogan
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Since xpenguins version 2.1, the ToonGetRootWindow() function
 * attempts to find the window IDs of
 *
 * 1) The background window that is behind the toplevel client
 *    windows; this is the window that we draw the toons on.
 *
 * 2) The parent window of the toplevel client windows; this is used
 *    by ToonLocateWindows() to build up a map of the space that the
 *    toons can occupy.
 * 
 * In simple (sensible?) window managers (e.g. blackbox, sawfish, fvwm
 * and countless others), both of these are the root window. The other
 * more complex scenarios that ToonGetRootWindow() attempts to cope
 * with are:
 *
 * Some `virtual' window managers (e.g. amiwm, swm and tvtwm) that
 * reparent all client windows to a desktop window that sits on top of
 * the root window. This desktop window is easy to find - we just look
 * for a property __SWM_VROOT in the immediate children of the root
 * window that contains the window ID of this desktop window. The
 * desktop plays both roles (1 and 2 above). This functionality was
 * detected in xpenguins 1.x with the vroot.h header file.
 *
 * Enlightenment (0.16) can have a number of desktops with different
 * backgrounds; client windows on these are reparented, except for
 * Desktop 0 which is the root window. Therefore versions less than
 * 2.1 of xpenguins worked on Desktop 0 but not on any others. To fix
 * this we look for a root-window property _WIN_WORKSPACE which
 * contains the numerical index of the currently active desktop. The
 * active desktop is then simply the immediate child of the root
 * window that has a property ENLIGHTENMENT_DESKTOP set to this value.
 *
 * KDE 2.0: Oh dear. The kdesktop is a program separate from the
 * window manager that launches a window which sits behind all the
 * other client windows and has all the icons on it. Thus the other
 * client windows are still children of the root window, but we want
 * to draw to the uppermost window of the kdesktop. This is difficult
 * to find - it is the great-great-grandchild of the root window and
 * in KDE 2.0 has nothing to identify it from its siblings other than
 * its size. KDE 2.1+ usefully implements the __SWM_VROOT property in
 * a child of the root window, but the client windows are still
 * children of the root window. A problem is that the penguins erase
 * the desktop icons when they walk which is a bit messy. The icons
 * are not lost - they reappear when the desktop window gets an expose
 * event (i.e. move some windows over where they were and back again).
 *
 * Nautilus (GNOME 1.4+): Creates a background window to draw icons
 * on, but does not reparent the client windows. The toplevel window
 * of the desktop is indicated by the root window property
 * NAUTILUS_DESKTOP_WINDOW_ID, but then we must descend down the tree
 * from this toplevel window looking for subwindows that are the same
 * size as the screen. The bottom one is the one to draw to. Hopefully
 * one day Nautilus will implement __SWM_VROOT in exactly the same way
 * as KDE 2.1+.
 *
 * Other cases: CDE, the common desktop environment. This is a
 * commercial product that has been packaged with Sun (and other)
 * workstations. It typically implements four virtual desktops but
 * provides NO properties at all for apps such as xpenguins to use to
 * work out where to draw to. Seeing as Sun are moving over to GNOME,
 * CDE use is on the decline so I don't have any current plans to try
 * and get xpenguins to work with it.
 *
 * As a note to developers of window managers and big screen hoggers
 * like kdesktop, please visit www.freedesktop.org and implement their
 * Extended Window Manager Hints spec that help pagers and apps like
 * xpenguins and xearth to find their way around. In particular,
 * please use the _NET_CURRENT_DESKTOP and _NET_VIRTUAL_ROOTS
 * properties if you reparent any windows (e.g. Enlightenment). Since
 * no window managers that I know yet use these particular hints, I
 * haven't yet added any code to parse them.  */


// #include "toon.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <stdio.h>
#include <string.h>

/* Time to throw up. Here is a kludgey function that recursively calls
 * itself (up to a limit) to find the window ID of the KDE desktop to
 * draw on. It works with KDE 2.0, but since KDE 2.0 is less stable
 * than Windows 95, I don't expect many people to remain using it now
 * that 2.1 is available, which implements __SWM_VROOT and makes this
 * function redundant. This is the hierarchy we're trying to traverse:
 *
 * -> The root window
 * 0 -> window with name="KDE Desktop"
 * 1   -> window with no name
 * 2     -> window with name="KDE Desktop" & _NET_WM_WINDOW_TYPE_DESKTOP
 * 3       -> window with no name and width >= width of screen
 * 
 * The last window in the hierarchy is the one to draw to.  The
 * numbers show the value of the `depth' argument.  */
static
Window
__ToonGetKDEDesktop(Display *display, int screen, Window window,
		    Atom atom, char *atomname, int depth)
{
  char *name = NULL;
  Atom *wintype = NULL;
  Window winreturn = 0;
  unsigned long nitems, bytesafter;
  Atom actual_type;
  int actual_format;
  Window rootReturn, parentReturn, *children;
  unsigned int nChildren;
  char go_deeper = 0;

  if (XFetchName(display, window, &name)) {
    if (strcasecmp(name, "KDE Desktop") == 0) {
      /* Presumably either at depth 0 or 2 */
      if (XGetWindowProperty(display, window, atom, 0, 1,
			     False, XA_ATOM,
			     &actual_type, &actual_format,
			     &nitems, &bytesafter,
			     (unsigned char **) &wintype) == Success
	  && wintype) {
	char *tmpatomname = XGetAtomName(display, *wintype);
	if (tmpatomname) {
	  if (strcmp(atomname, tmpatomname) == 0 && depth == 2) {
	    /* OK, at depth 2 */
	    go_deeper = 1;
	  }
	  XFree((char *) tmpatomname);
	}
      }
      else if (depth < 2) {
	go_deeper = 1;
      }
    }
    else if (depth == 1) {
      go_deeper = 1;
    }
    XFree((char *) name);
  }
  else if (depth == 1) {
    go_deeper = 1;
  }

  /* If go_deeper is 1 then there is a possibility that the background
   * window is a descendant of the current window; otherwise we're
   * barking up the wrong tree. */
  if (go_deeper && XQueryTree(display, window, &rootReturn,
			      &parentReturn, &children,
			      &nChildren)) {
    int i;
    for (i = 0; i < nChildren; ++i) {
      /* children[i] is now at depth 3 */
      if (depth == 2) {
	XWindowAttributes attributes;
	if (XGetWindowAttributes(display, children[i], &attributes)) {
	  if (attributes.width >= DisplayWidth(display, screen)/2
	      && attributes.height > 0) {
	    /* Found it! */
	    winreturn = children[i];
	    break;
	  }
	}
      }
      else if ((winreturn = __ToonGetKDEDesktop(display, screen,
						children[i],
						atom, atomname,
						depth+1))) {
	break;
      }
    }
    XFree((char *) children);
  }

  return winreturn;
}

/* Look for the Nautilus desktop window to draw to, given the toplevel
 * window of the Nautilus desktop. Basically recursively calls itself
 * looking for subwindows the same size as the root window. */
static
Window
__ToonGetNautilusDesktop(Display *display, int screen, Window window,
			 int depth)
{
  Window rootReturn, parentReturn, *children;
  Window winreturn = window;
  unsigned int nChildren;

  if (depth > 5) {
    return ((Window) 0);
  }
  else if (XQueryTree(display, window, &rootReturn, &parentReturn,
		 &children, &nChildren)) {
    int i;
    for (i = 0; i < nChildren; ++i) {
      XWindowAttributes attributes;
      if (XGetWindowAttributes(display, children[i], &attributes)) {
	if (attributes.width == DisplayWidth(display, screen)
	    && attributes.height == DisplayHeight(display, screen)) {
	  /* Found a possible desktop window */
	  winreturn = __ToonGetNautilusDesktop(display, screen,
					       children[i], depth+1);
	}
      }  
    }
    XFree((char *) children);
  }
  return winreturn;
}


/* 
 * Returns the window ID of the `background' window on to which the
 * toons should be drawn. Also returned (in clientparent) is the ID of
 * the parent of all the client windows, since this may not be the
 * same as the background window. If no recognised virtual window
 * manager or desktop environment is found then the root window is
 * returned in both cases. The string toon_message contains
 * information about the window manager that was found.
 */
Window
ToonGetRootWindow(Display *display, int screen, Window *clientparent)
{
  Window background = 0; /* The return value */
  Window root = RootWindow(display, screen);
  Window rootReturn, parentReturn, *children;
  Window *toplevel = (Window *) 0;
  unsigned int nChildren;
  unsigned long nitems, bytesafter;
  Atom actual_type;
  int actual_format;
  unsigned long *workspace = NULL;
  unsigned long *desktop = NULL;
  Atom NAUTILUS_DESKTOP_WINDOW_ID = XInternAtom(display,
			   "NAUTILUS_DESKTOP_WINDOW_ID",
						False);

  *clientparent = root;

  if (XGetWindowProperty(display, root,
			 NAUTILUS_DESKTOP_WINDOW_ID,
			 0, 1, False, XA_WINDOW,
			 &actual_type, &actual_format,
			 &nitems, &bytesafter,
			 (unsigned char **) &toplevel) == Success
      && toplevel) {
    /* Nautilus is running */
    background = __ToonGetNautilusDesktop(display, screen,
					  *toplevel, 0);
    XFree((char *) toplevel);
    if (background) {
      printf("Drawing to Nautilus Desktop");
    }
  }

  /* Next look for a virtual root or a KDE Desktop */
  if (!background
      && XQueryTree(display, root, &rootReturn, &parentReturn,
		    &children, &nChildren)) {
    int i;
    Atom _NET_WM_WINDOW_TYPE = XInternAtom(display, 
			     "_NET_WM_WINDOW_TYPE",
					   False);
    Atom __SWM_VROOT = XInternAtom(display, "__SWM_VROOT", False);
      
    for (i = 0; i < nChildren && !background; ++i) {
      Window *newroot = (Window *) 0;
      if (XGetWindowProperty(display, children[i],
			     __SWM_VROOT, 0, 1, False, XA_WINDOW,
			     &actual_type, &actual_format,
			     &nitems, &bytesafter,
			     (unsigned char **) &newroot) == Success
	  && newroot) {
	/* Found a window with a __SWM_VROOT property that contains
	 * the window ID of the virtual root. Now we must check
	 * whether it is KDE (2.1+) or not. If it is KDE then it does
	 * not reparent the clients. If the root window has the
	 * _NET_SUPPORTED property but not the _NET_VIRTUAL_ROOTS
	 * property then we assume it is KDE. */
	Atom _NET_SUPPORTED = XInternAtom(display,
					  "_NET_SUPPORTED",
					  False);
	Atom *tmpatom;
	if (XGetWindowProperty(display, root,
			       _NET_SUPPORTED, 0, 1, False,
			       XA_ATOM, &actual_type, &actual_format,
			       &nitems, &bytesafter,
			       (unsigned char **) &tmpatom) == Success
	    && tmpatom) {
	  Window *tmpwindow = (Window *) 0;
	  Atom _NET_VIRTUAL_ROOTS = XInternAtom(display,
						"_NET_VIRTUAL_ROOTS",
						False);
	  XFree((char *) tmpatom);
	  if (XGetWindowProperty(display, root,
				 _NET_VIRTUAL_ROOTS, 0, 1, False,
				 XA_WINDOW, &actual_type, &actual_format,
				 &nitems, &bytesafter,
				 (unsigned char **) &tmpwindow) != Success
	      || !tmpwindow) {
	    /* Must be KDE 2.1+ */
	    printf("Drawing to KDE Desktop");
	    background = *newroot;
	  }
	  else if (tmpwindow) {
	    XFree((char *) tmpwindow);
	  }
	}

	if (!background) {  
	  /* Not KDE: assume windows are reparented */
	  printf("Drawing to virtual root window");
	  background = *clientparent = *newroot;
	}
	XFree((char *) newroot);
      }
      else if ((background = __ToonGetKDEDesktop(display, screen, children[i],
						 _NET_WM_WINDOW_TYPE,
						 "_NET_WM_WINDOW_TYPE_DESKTOP",
						 0))) {
	/* Found a KDE 2.0 desktop and located the background window */
	/* Note that the clientparent is still the root window */
	printf( "Drawing to KDE desktop");
      }
    }
    XFree((char *) children);
  }

  if (!background) {
    /* Look for a _WIN_WORKSPACE property, used by Enlightenment */
    Atom _WIN_WORKSPACE = XInternAtom(display, "_WIN_WORKSPACE", False);
    if (XGetWindowProperty(display, root, _WIN_WORKSPACE,
			   0, 1, False, XA_CARDINAL,
			   &actual_type, &actual_format,
			   &nitems, &bytesafter,
			   (unsigned char **) &workspace) == Success
	&& workspace) {
      /* Found a _WIN_WORKSPACE property - this is the desktop to look for.
       * For now assume that this is Enlightenment.
       * We're looking for a child of the root window that has an
       * ENLIGHTENMENT_DESKTOP atom with a value equal to the root window's
       * _WIN_WORKSPACE atom. */
      Atom ENLIGHTENMENT_DESKTOP = XInternAtom(display, 
					       "ENLIGHTENMENT_DESKTOP",
					       False);
      /* First check to see if the root window is the current desktop... */
      if (XGetWindowProperty(display, root,
			     ENLIGHTENMENT_DESKTOP, 0, 1,
			     False, XA_CARDINAL,
			     &actual_type, &actual_format,
			     &nitems, &bytesafter,
			     (unsigned char **) &desktop) == Success
	  && desktop && *desktop == *workspace) {
	/* The root window is the current Enlightenment desktop */
	printf( "Drawing to Enlightenment Desktop %lu (the root window)");
	background = root;
	XFree((char *) desktop);
      }
      /* Now look at each immediate child window of root to see if it is
       * the current desktop */
      else if (XQueryTree(display, root, &rootReturn, &parentReturn,
			  &children, &nChildren)) {
	int i;
	for (i = 0; i < nChildren; ++i) {
	  if (XGetWindowProperty(display, children[i],
				 ENLIGHTENMENT_DESKTOP, 0, 1,
				 False, XA_CARDINAL,
				 &actual_type, &actual_format,
				 &nitems, &bytesafter,
				 (unsigned char **) &desktop) == Success
	      && desktop && *desktop == *workspace) {
	    /* Found current Enlightenment desktop */
	    printf("Drawing to Enlightenment Desktop %lu");
	    background = *clientparent = children[i];
	    XFree((char *) desktop);
	  }
	}
	XFree((char *) children);
      }
      XFree((char *) workspace);
    }
  }
  if (background) {
    return background;
  }
  else {
    return root;
  }
}
