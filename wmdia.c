///
///	@file wmdia.c		@brief	DIA Dockapp
///
///	Copyright (c) 2009 - 2011,2021 by Lutz Sammer.  All Rights Reserved.
///
///	Contributor(s):
///
///	This file is part of wmdia
///
///	License: AGPLv3
///
///	This program is free software: you can redistribute it and/or modify
///	it under the terms of the GNU Affero General Public License as
///	published by the Free Software Foundation, either version 3 of the
///	License.
///
///	This program is distributed in the hope that it will be useful,
///	but WITHOUT ANY WARRANTY; without even the implied warranty of
///	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///	GNU Affero General Public License for more details.
///
///	$Id$
////////////////////////////////////////////////////////////////////////////

/**
**	@mainpage
**
**	"dia" is a german word for "Reversal film".
**
**	@n @n
**	This is a small dockapp, which does nearly nothing. 
**	It just creates an empty window with a 62x62 background pixmap.
**
**	@n
**	When you click on the window, the command in the property "COMMAND" is
**	executed.  When you move the mouse in the window the text in the
**	property "TOOLTIP" is shown as tooltip.
**
**	@n
**	The main goal was to have a slideshow in a dockapp window, but it can
**	also be used to show films, webcam in the dockapp or just a simple
**	command button.
**
**	@n
**	With xprop can you change the command or tooltip:
**
**	xprop -name wmdia -format COMMAND 8s -set COMMAND "command" @n
**	xprop -name wmdia -format TOOLTIP 8s -set TOOLTIP "tooltip" @n
**
**	@n
**	With display can you change the background
**
**	display -resize 62x62 -bordercolor darkgray -border 31
**	    -gravity center -crop 62x62+0+0 -window wmdia picture.jpg
*/

////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/wait.h>

#include <xcb/xcb.h>
#define xcb_popcount buggy_xcb_popcount_fixup_1
#include <xcb/xcbext.h>
#undef xcb_popcount
#include <xcb/shape.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_pixel.h>

#include "wmdia.xpm"

////////////////////////////////////////////////////////////////////////////

static xcb_connection_t *Connection;	///< connection to X11 server
static xcb_screen_t *Screen;		///< our screen
static xcb_window_t Window;		///< our window
static xcb_gcontext_t NormalGC;		///< normal graphic context
static xcb_pixmap_t Pixmap;		///< our background pixmap
static xcb_pixmap_t Image;		///< drawing data

static xcb_atom_t CommandAtom;		///< "COMMAND" property
static xcb_atom_t TooltipAtom;		///< "TOOLTIP" property

static int Timeout = -1;		///< timeout in ms
static int WindowMode;			///< start in window mode
static const char *Name;		///< window/application name
static const char *FontTooltip;		///< font for tooltip

//{@
///	Called from event loop
static void HandleTimeout(void);
static void ButtonPress(void);
static void WindowEnter(void);
static void WindowLeave(void);
static void PropertyChanged(void);

//@}

static void DelTooltip(void);		///< forward define for Exit
static void HideTooltip(void);		///< forward define for expose

//@{
///	Default font for the tooltip
#ifndef FONT
#define FONT "-misc-fixed-medium-r-normal--20-*-75-75-c-*-iso8859-*"
#endif
#define aFONT "7x13"
#define bFONT "-*-bitstream vera sans-*-*-*-*-17-*-*-*-*-*-*-*"
//@}

////////////////////////////////////////////////////////////////////////////
//	XPM Stuff
////////////////////////////////////////////////////////////////////////////

/**
**	Convert XPM graphic to xcb_image.
**
**	@param connection	XCB connection to X11 server
**	@param colormap		window colormap
**	@param depth		image depth
**	@param transparent	pixel for transparent color
**	@param data		XPM graphic data
**	@param[out] mask	bitmap mask for transparent
**
**	@returns image create from the XPM data.
**
**	@warning supports only a subset of XPM formats.
*/
static xcb_image_t *XcbXpm2Image(xcb_connection_t * connection,
    xcb_colormap_t colormap, uint8_t depth, uint32_t transparent,
    const char *const *data, uint8_t ** mask)
{
    // convert table: ascii hex nibble to binary
    static const uint8_t hex[128] =
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
	0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    int w;
    int h;
    int colors;
    int bytes_per_color;
    char dummy;
    int i;
    xcb_alloc_color_cookie_t cookies[256];
    int color_to_pixel[256];
    uint32_t pixels[256];
    xcb_image_t *image;
    int mask_width;
    const char *line;
    int x;
    int y;

    if (sscanf(*data, "%d %d %d %d %c", &w, &h, &colors, &bytes_per_color,
	    &dummy) != 4) {
	fprintf(stderr, "unparsable XPM header\n");
	abort();
    }
    if (colors < 1 || colors > 255) {
	fprintf(stderr, "XPM: wrong number of colors %d\n", colors);
	abort();
    }
    if (bytes_per_color != 1) {
	fprintf(stderr, "%d-byte XPM files not supported\n", bytes_per_color);
	abort();
    }
    data++;

    //
    //	Read color table, send alloc color requests
    //
    for (i = 0; i < colors; i++) {
	int id;

	line = *data;
	id = *line++;
	color_to_pixel[id] = i;		// maps xpm color char to pixel
	cookies[i].sequence = 0;
	while (*line) {			// multiple choices for color
	    int r;
	    int g;
	    int b;
	    int n;
	    int type;

	    n = strspn(line, " \t");	// skip white space
	    type = line[n];
	    if (!type) {
		continue;		// whitespace upto end of line
	    }
	    if (type != 'c' && type != 'm') {
		fprintf(stderr, "unknown XPM pixel type '%c' in \"%s\"\n",
		    type, *data);
		abort();
	    }
	    line += n + 1;
	    n = strspn(line, " \t");	// skip white space
	    line += n;
	    if (!strncasecmp(line, "none", 4)) {
		line += 4;
		color_to_pixel[id] = -1;	// transparent
		continue;
	    } else if (*line != '#') {
		fprintf(stderr, "unparsable XPM color spec: \"%s\"\n", line);
		abort();
	    } else {
		line++;
		r = (hex[line[0] & 0xFF] << 4) | hex[line[1] & 0xFF];
		line += 2;
		g = (hex[line[0] & 0xFF] << 4) | hex[line[1] & 0xFF];
		line += 2;
		b = (hex[line[0] & 0xFF] << 4) | hex[line[1] & 0xFF];
		line += 2;
	    }

	    // 8bit rgb -> 16bit
	    r = (65535 * (r & 0xFF) / 255);
	    b = (65535 * (b & 0xFF) / 255);
	    g = (65535 * (g & 0xFF) / 255);

	    // FIXME: should i use _unchecked here?
	    if ((depth != 1 && type == 'c') || (depth == 1 && type == 'm')) {
		if (cookies[i].sequence) {
		    fprintf(stderr, "XPM multiple color spec: \"%s\"\n", line);
		    abort();
		}
		cookies[i] =
		    xcb_alloc_color_unchecked(connection, colormap, r, g, b);
	    }
	}
	data++;
    }

    //
    //	Fetch the replies
    //
    for (i = 0; i < colors; i++) {
	xcb_alloc_color_reply_t *reply;

	if (cookies[i].sequence) {
	    reply = xcb_alloc_color_reply(connection, cookies[i], NULL);
	    if (!reply) {
		fprintf(stderr, "unable to allocate XPM color\n");
		abort();
	    }
	    pixels[i] = reply->pixel;
	    free(reply);
	} else {
	    // transparent or error
	    pixels[i] = 0UL;
	}
    }

    if (depth == 1) {
	transparent = 1;
    }

    image =
	xcb_image_create_native(connection, w, h,
	(depth == 1) ? XCB_IMAGE_FORMAT_XY_BITMAP : XCB_IMAGE_FORMAT_Z_PIXMAP,
	depth, NULL, 0L, NULL);
    if (!image) {			// failure
	return image;
    }
    //
    //	Allocate empty mask (if mask is requested)
    //
    mask_width = (w + 7) / 8;		// make gcc happy
    if (mask) {
	i = mask_width * h;
	*mask = malloc(i);
	if (!mask) {			// malloc failure
	    mask = NULL;
	} else {
	    memset(*mask, 255, i);
	}
    }
    //
    //	Copy each pixel from xpm into the image, while creating the mask
    //
    for (y = 0; y < h; y++) {
	line = *data++;
	for (x = 0; x < w; x++) {
	    i = color_to_pixel[*line++ & 0xFF];
	    if (i == -1) {		// marks transparent
		xcb_image_put_pixel(image, x, y, transparent);
		if (mask) {
		    (*mask)[(y * mask_width) + (x >> 3)] &= (~(1 << (x & 7)));
		}
	    } else {
		xcb_image_put_pixel(image, x, y, pixels[i]);
	    }
	}
    }
    return image;
}

////////////////////////////////////////////////////////////////////////////

/**
**	Create pixmap.
**
**	@param data		XPM data
**	@param[out] mask	pixmap for data
**
**	@returns pixmap created from data.
*/
static xcb_pixmap_t CreatePixmap(const char *const *data, xcb_pixmap_t * mask)
{
    xcb_pixmap_t pixmap;
    uint8_t *bitmap;
    xcb_image_t *image;

    image =
	XcbXpm2Image(Connection, Screen->default_colormap, Screen->root_depth,
	0UL, data, mask ? &bitmap : NULL);
    if (!image) {
	fprintf(stderr, "Can't create image\n");
	abort();
    }
    if (mask) {
	*mask =
	    xcb_create_pixmap_from_bitmap_data(Connection, Window, bitmap,
	    image->width, image->height, 1, 0, 0, NULL);
	free(bitmap);
    }
    // now get data from image and build a pixmap...
    pixmap = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, Screen->root_depth, pixmap, Window,
	image->width, image->height);
    xcb_image_put(Connection, pixmap, NormalGC, image, 0, 0, 0);

    xcb_image_destroy(image);

    return pixmap;
}

////////////////////////////////////////////////////////////////////////////

/**
**	Loop
*/
static void Loop(void)
{
    struct pollfd fds[1];
    xcb_generic_event_t *event;
    int n;

    fds[0].fd = xcb_get_file_descriptor(Connection);
    fds[0].events = POLLIN | POLLPRI;

    for (;;) {
	n = poll(fds, 1, Timeout);
	if (n < 0) {
	    return;
	}
	if (n) {
	    if (fds[0].revents & (POLLIN | POLLPRI)) {
		while ((event = xcb_poll_for_event(Connection))) {

		    switch (XCB_EVENT_RESPONSE_TYPE(event)) {
			case XCB_EXPOSE:
			    // collapse multi expose
			    if (!((xcb_expose_event_t *) event)->count) {
				// FIXME: redraw the tooltip
				HideTooltip();

				//xcb_clear_area(Connection, 0, Window, 0, 0, 64, 64);
				// flush the request
				//xcb_flush(Connection);
			    }
			    break;
			case XCB_ENTER_NOTIFY:
			    WindowEnter();
			    break;
			case XCB_LEAVE_NOTIFY:
			    WindowLeave();
			    break;
			case XCB_BUTTON_PRESS:
			    ButtonPress();
			    break;
			case XCB_PROPERTY_NOTIFY:
			    PropertyChanged();
			    break;
			case XCB_DESTROY_NOTIFY:
			    // window closed, exit application
			    return;
			default:
			    // Unknown event type, ignore it
			    printf("unknown event type %d\n",
				XCB_EVENT_RESPONSE_TYPE(event));
			    break;
		    }

		    free(event);
		}
		// No event, can happen, but we must check for close
		if (xcb_connection_has_error(Connection)) {
		    return;
		}
	    }
	} else {
	    HandleTimeout();
	}
    }
}

/**
**	Init the application.
**
**	@param argc	number of arguments
**	@param argv	arguments vector
*/
static int Init(int argc, char *const argv[])
{
    const char *display_name;
    xcb_connection_t *connection;
    xcb_screen_iterator_t iter;
    int screen_nr;
    xcb_screen_t *screen;
    xcb_gcontext_t normal;
    uint32_t mask;
    uint32_t values[3];
    xcb_pixmap_t pixmap;
    xcb_window_t window;
    xcb_size_hints_t size_hints;
    xcb_icccm_wm_hints_t wm_hints;
    int i;
    int n;
    char *s;
    char *buf;

    display_name = getenv("DISPLAY");

    //	Open the connection to the X server.
    //	use the DISPLAY environment variable as the default display name
    connection = xcb_connect(NULL, &screen_nr);
    if (!connection || xcb_connection_has_error(connection)) {
	fprintf(stderr, "Can't connect to X11 server on %s\n", display_name);
	return -1;
    }
    //	Get the requested screen number
    iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
    for (i = 0; i < screen_nr; ++i) {
	xcb_screen_next(&iter);
    }
    screen = iter.data;

    //	Create normal graphic context
    normal = xcb_generate_id(connection);
    mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    values[0] = screen->black_pixel;
    values[1] = screen->white_pixel;
    values[2] = 0;
    xcb_create_gc(connection, normal, screen->root, mask, values);

    pixmap = xcb_generate_id(connection);
    xcb_create_pixmap(connection, screen->root_depth, pixmap, screen->root, 64,
	64);

    //	Create the window
    window = xcb_generate_id(connection);

    mask = XCB_CW_BACK_PIXMAP | XCB_CW_EVENT_MASK;
    values[0] = pixmap;
    values[1] =
	XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
	XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
	XCB_EVENT_MASK_PROPERTY_CHANGE;

    xcb_create_window(connection,	// Connection
	XCB_COPY_FROM_PARENT,		// depth (same as root)
	window,				// window Id
	screen->root,			// parent window
	0, 0,				// x, y
	64, 64,				// width, height
	0,				// border_width
	XCB_WINDOW_CLASS_INPUT_OUTPUT,	// class
	screen->root_visual,		// visual
	mask, values);			// mask, values

    // XSetWMNormalHints
    size_hints.flags = 0;		// FIXME: bad lib design
    xcb_icccm_size_hints_set_position(&size_hints, 1, 0, 0);
    xcb_icccm_size_hints_set_size(&size_hints, 1, 64, 64);
    xcb_icccm_set_wm_normal_hints(connection, window, &size_hints);

    i = strlen(Name);
    buf = alloca(i + sizeof("wmdia") + 2);
    strncpy(buf, Name, i + 1);
    strcpy(buf + i + 1, "wmdia");
    xcb_icccm_set_wm_class(connection, window, i + 1 + sizeof("wmdia"), buf);

    xcb_icccm_set_wm_name(connection, window, XCB_ATOM_STRING, 8, i, Name);
    xcb_icccm_set_wm_icon_name(connection, window, XCB_ATOM_STRING, 8, i, Name);

    // XSetWMHints
    wm_hints.flags = 0;
    xcb_icccm_wm_hints_set_icon_pixmap(&wm_hints, pixmap);
    xcb_icccm_wm_hints_set_window_group(&wm_hints, window);
    xcb_icccm_wm_hints_set_withdrawn(&wm_hints);
    if (WindowMode) {
	xcb_icccm_wm_hints_set_none(&wm_hints);
    }
    xcb_icccm_set_wm_hints(connection, window, &wm_hints);

    // XSetCommand (see xlib source)
    for (n = i = 0; i < argc; ++i) {	// length of string prop
	n += strlen(argv[i]) + 1;
    }
    s = alloca(n);
    for (n = i = 0; i < argc; ++i) {	// copy string prop
	strcpy(s + n, argv[i]);
	n += strlen(s + n) + 1;
    }
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window,
    	XCB_ATOM_WM_COMMAND, XCB_ATOM_STRING, 8, n, s);

    //	Map the window on the screen
    xcb_map_window(connection, window);

    //	Make sure commands are sent
    xcb_flush(connection);

    Connection = connection;
    Screen = screen;
    Window = window;
    NormalGC = normal;
    Pixmap = pixmap;

    return 0;
}

/**
**	Exit the application.
*/
static void Exit(void)
{
    DelTooltip();

    xcb_destroy_window(Connection, Window);
    Window = 0;

    xcb_free_gc(Connection, NormalGC);

    xcb_free_pixmap(Connection, Pixmap);
    if (Image) {
	xcb_free_pixmap(Connection, Image);
    }

    xcb_disconnect(Connection);
    Connection = NULL;
}

////////////////////////////////////////////////////////////////////////////
//	App Stuff
////////////////////////////////////////////////////////////////////////////

/**
**	Execute command without waiting.
*/
static int System(char *cmd)
{
    int pid;
    int status;
    extern char **environ;

    if (!cmd) {				// No command
	return 1;
    }
    if ((pid = fork()) == -1) {		// fork failed
	return -1;
    }
    if (!pid) {				// child
	if (!(pid = fork())) {		// child of child
	    char *argv[4];

	    argv[0] = "sh";
	    argv[1] = "-c";
	    argv[2] = cmd;
	    argv[3] = 0;
	    execve("/bin/sh", argv, environ);
	    exit(0);
	}
	exit(0);
    }
    wait(&status);			// wait for first child
    return 0;
}

// ------------------------------------------------------------------------- //
//	Tooltip

xcb_window_t Tooltip;			///< tooltip window
xcb_font_t Font;			///< tooltip font
xcb_gcontext_t FontGC;			///< font graphic context
int TooltipShown;			///< flag tooltip is shown

/**
**	Get origin of window.
**
**	@param window	get origin of this window
**	@param[out] x	absolute X position of the window
**	@param[out] y	absolute Y position of the window
*/
static void WindowOrigin(xcb_window_t window, int *x, int *y)
{
    xcb_window_t parent;
    xcb_generic_error_t *error;
    xcb_get_geometry_reply_t *geom;
    xcb_query_tree_reply_t *tree;
    xcb_translate_coordinates_reply_t *trans;
    int rx;
    int ry;

    // geometry of the window relative to parent
    geom =
	xcb_get_geometry_reply(Connection,
	xcb_get_geometry_unchecked(Connection, window), NULL);
    if (!geom) {
	fprintf(stderr, "xcb_get_geometry() failed\n");
	return;
    }
    rx = geom->x;
    ry = geom->y;
    free(geom);

    // get the parent of the window
    tree =
	xcb_query_tree_reply(Connection, xcb_query_tree(Connection, window),
	&error);
    if (!tree || error) {
	fprintf(stderr, "xcb_query_tree() failed\n");
	return;
    }

    parent = tree->parent;
    free(tree);

    // translated coordinates 
    trans =
	xcb_translate_coordinates_reply(Connection,
	xcb_translate_coordinates_unchecked(Connection, parent, Screen->root,
	    rx, ry), NULL);
    if (!trans) {
	fprintf(stderr, "xcb_translate_coordinates() failed\n");
	return;
    }

    rx = trans->dst_x;
    ry = trans->dst_y;
    free(trans);

    if (x) {
	*x = rx;
    }
    if (y) {
	*y = ry;
    }
}

/**
**	Create new tooltip.
*/
static void NewTooltip(void)
{
    xcb_window_t window;
    xcb_font_t font;
    xcb_alloc_color_reply_t *alloc_color;
    xcb_generic_error_t *error;
    uint32_t mask;
    uint32_t values[5];

    //
    //	allocate YELLOW pixel.
    //
    alloc_color =
	xcb_alloc_color_reply(Connection, xcb_alloc_color(Connection,
	    Screen->default_colormap, 0xD000, 0xD000, 0), NULL);

    //	Create the window
    window = xcb_generate_id(Connection);

    mask =
	XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
	XCB_CW_SAVE_UNDER | XCB_CW_EVENT_MASK;
    values[0] = alloc_color->pixel;
    values[1] = Screen->black_pixel;
    values[2] = 1;
    values[3] = 1;
    values[4] = XCB_EVENT_MASK_BUTTON_PRESS;

    xcb_create_window(Connection,	// Connection
	XCB_COPY_FROM_PARENT,		// depth (same as root)
	window,				// window Id
	Screen->root,			// parent window
	1, 1,				// x, y
	160, 16,			// width, height
	1,				// border_width
	XCB_WINDOW_CLASS_INPUT_OUTPUT,	// class
	XCB_COPY_FROM_PARENT,		// visual
	mask, values);			// mask, values

    //
    //	Find and load font
    //
    font = xcb_generate_id(Connection);
    error =
	xcb_request_check(Connection, xcb_open_font_checked(Connection, font,
	    strlen(FontTooltip), FontTooltip));
    if (error) {
	fprintf(stderr, "Can't open font %d\n", error->error_code);
	exit(-1);
    }
    // FIXME: try alternative fonts

    // create graphics context
    FontGC = xcb_generate_id(Connection);
    mask = XCB_GC_FOREGROUND | XCB_GC_FONT | XCB_GC_GRAPHICS_EXPOSURES;
    values[0] = Screen->black_pixel;
    values[1] = font;
    values[2] = 0;

    xcb_create_gc(Connection, FontGC, window, mask, values);

    free(alloc_color);

    Tooltip = window;
    Font = font;
}

/**
**	Delete tooltip
*/
static void DelTooltip(void)
{
    if (Tooltip) {
	xcb_destroy_window(Connection, Tooltip);
	xcb_free_gc(Connection, FontGC);
	// FIXME: free color
	xcb_close_font(Connection, Font);
    }
}

/**
**	Draw poly text. 
**
**	This a simple version of xcb_poly_text_8, which works only with
**	one string with lessequal 254 characters.
**
**	drawable: DRAWABLE
**	gc: GCONTEXT
**	x, y: INT16
**	items: LISTofTEXTITEM8)
**
**	TEXTELT8: TEXTELT8: [delta: INT8 string: STRING8]
**
**	FONT: 5 bytes 255 + FontID
**
**	Len8 Delta8 String8 
**
**	@note the xcb_poly_text_8 of libxcb didn't work as expected,
**	atleast in versions <= 1.6
**
*/
xcb_void_cookie_t xcb_poly_text_8_simple(xcb_connection_t * c,
    xcb_drawable_t drawable, xcb_gcontext_t gc, int16_t x, int16_t y,
    uint32_t len, const char *str)
{
    static const xcb_protocol_request_t xcb_req = {
	/* count */ 5,
	/* ext */ 0,
	/* opcode */ XCB_POLY_TEXT_8,
	/* isvoid */ 1
    };
    struct iovec xcb_parts[7];
    uint8_t xcb_lendelta[2];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_text_8_request_t xcb_out;

    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;

    xcb_lendelta[0] = len;
    xcb_lendelta[1] = 0;

    xcb_parts[2].iov_base = (char *)&xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_parts[4].iov_base = xcb_lendelta;
    xcb_parts[4].iov_len = sizeof(xcb_lendelta);
    xcb_parts[5].iov_base = (char *)str;
    xcb_parts[5].iov_len = len * sizeof(int8_t);

    xcb_parts[6].iov_base = 0;
    xcb_parts[6].iov_len = -(xcb_parts[4].iov_len + xcb_parts[5].iov_len) & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

/**
**	Show tooltip
**
**	@param len	length of text to display
**	@param str	text to display
*/
static void ShowTooltip(int len, const char *str)
{
    uint32_t mask;
    uint32_t values[5];
    xcb_generic_error_t *error;
    xcb_char2b_t *chars;
    xcb_query_text_extents_reply_t *query_text_extents;
    int i;
    int x;
    int y;
    int th;
    int tw;

    //
    //	Tooltip text length.
    //
    chars = alloca(len * 2);
    for (i = 0; i < len; ++i) {		// convert 8 -> 16
	chars[i].byte1 = 0;
	chars[i].byte2 = str[i];
    }

    query_text_extents =
	xcb_query_text_extents_reply(Connection,
	xcb_query_text_extents(Connection, Font, len, chars), &error);
    if (!query_text_extents || error) {
	fprintf(stderr, "Can't query text extents\n");
	return;
    }

    /*
       printf("%d, %d A=%d D=%d\n", query_text_extents->draw_direction,
       query_text_extents->length, query_text_extents->font_ascent,
       query_text_extents->font_descent);
       printf("A=%d, D=%d, W=%d L=%d R=%d\n",
       query_text_extents->overall_ascent,
       query_text_extents->overall_descent,
       query_text_extents->overall_width,
       query_text_extents->overall_left,
       query_text_extents->overall_right);
     */

    th = 4 + query_text_extents->font_ascent +
	query_text_extents->font_descent;
    if (th < 16) {
	th = 16;
    }
    tw = query_text_extents->overall_width + 16;
    if (tw < 16) {
	tw = 16 * 100;
    }
    //
    //	Move and show tooltip
    //
    x = y = 0;
    WindowOrigin(Window, &x, &y);

    if (x + tw > Screen->width_in_pixels) {	// on screen
	x -= tw;
	if (x < 0) {
	    x = 0;
	}
    }
    if (y + th > Screen->height_in_pixels) {
	y = Screen->height_in_pixels - th;
    }
    // RAISE window
    mask =
	XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
	XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_STACK_MODE;
    values[0] = x + 32;
    values[1] = y + 32;
    values[2] = tw;
    values[3] = th;
    values[4] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(Connection, Tooltip, mask, values);
    xcb_map_window(Connection, Tooltip);

    //
    //	Draw text
    //
    xcb_poly_text_8_simple(Connection, Tooltip, FontGC, 8,
	4 + (query_text_extents->font_descent +
	    query_text_extents->font_ascent - th) / 2 +
	query_text_extents->font_ascent, len, str);

    xcb_flush(Connection);

    TooltipShown = 1;
    Timeout = 5 * 1000;
}

/**
**	Show tooltip
*/
static void HideTooltip(void)
{
    if (TooltipShown) {
	xcb_unmap_window(Connection, Tooltip);
	xcb_flush(Connection);

	TooltipShown = 0;
	Timeout = -1;
    }
}

// ------------------------------------------------------------------------- //

/**
**	Timeout call back.
*/
static void HandleTimeout(void)
{
    // Remove the tooltip, if still shown
    HideTooltip();
}

/**
**	Button press call back.
*/
static void ButtonPress(void)
{
    char *cmd;
    xcb_get_property_cookie_t cookie;
    xcb_icccm_get_text_property_reply_t prop;

    cookie = xcb_icccm_get_text_property_unchecked(Connection, Window,
    	CommandAtom);
    if (xcb_icccm_get_text_property_reply(Connection, cookie, &prop, NULL)) {
	if (prop.name_len) {
	    cmd = alloca(prop.name_len + 1);
	    strcpy(cmd, prop.name);
	    cmd[prop.name_len] = '\0';

	    System(cmd);
	}

	xcb_icccm_get_text_property_reply_wipe(&prop);
    }
}

/**
**	Window enter
*/
static void WindowEnter(void)
{
    xcb_get_property_cookie_t cookie;
    xcb_icccm_get_text_property_reply_t prop;

    if (TooltipShown) {
	Timeout = 5 * 1000;
	return;
    }
    // FIXME: don't show tooltip at once, small delay is better
    if (!Tooltip) {
	NewTooltip();
    }
    //
    //	Get property "TOOLTIP" attached to our window.
    //
    cookie = xcb_icccm_get_text_property_unchecked(Connection, Window, TooltipAtom);
    if (xcb_icccm_get_text_property_reply(Connection, cookie, &prop, NULL)) {
	if (prop.name_len) {
	    ShowTooltip(prop.name_len, prop.name);
	} else {
	    ShowTooltip(sizeof("No tooltip set!") - 1, "No tooltip set!");
	}

	xcb_icccm_get_text_property_reply_wipe(&prop);
    } else {
	ShowTooltip(sizeof("Error tooltip") - 1, "Error tooltip");
    }
}

/**
**	Window leave
*/
static void WindowLeave(void)
{
    if (Timeout == -1) {
	HideTooltip();
    }
}

/**
**	Property changed
*/
static void PropertyChanged(void)
{
    if (TooltipShown) {
	TooltipShown = 0;
	WindowEnter();
    }
}

/**
**	Prepare our graphic data.
*/
static void PrepareData(void)
{
    xcb_intern_atom_cookie_t cookies[2];
    xcb_intern_atom_reply_t * reply;
    xcb_pixmap_t shape;

    cookies[0] = xcb_intern_atom_unchecked(Connection, 0,
    	sizeof("COMMAND") - 1 , "COMMAND");
    cookies[1] = xcb_intern_atom_unchecked(Connection, 0,
    	sizeof("TOOLTIP") - 1 , "TOOLTIP");

    Image = CreatePixmap((void *)wmdia_xpm, &shape);
    // Copy background part
    xcb_copy_area(Connection, Image, Pixmap, NormalGC, 0, 0, 0, 0, 64, 64);
    if (shape) {
	xcb_shape_mask(Connection, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING,
	    Window, 0, 0, shape);
	xcb_free_pixmap(Connection, shape);
    }

    HandleTimeout();

    //
    //	Prepare atoms for our properties
    //
    
    if ((reply = xcb_intern_atom_reply(Connection, cookies[0], NULL))) {
	CommandAtom = reply->atom;
	free(reply);
    }
    if ((reply = xcb_intern_atom_reply(Connection, cookies[1], NULL))) {
	TooltipAtom = reply->atom;
	free(reply);
    }
}

// ------------------------------------------------------------------------- //

/**
**	Print version.
*/
static void PrintVersion(void)
{
    printf("wmdia DIA dockapp Version " VERSION
#ifdef GIT_REV
	"(GIT-" GIT_REV ")"
#endif
	", (c) 2009 - 2011 by Lutz Sammer\n"
	"\tLicense AGPLv3: GNU Affero General Public License version 3\n");
}

/**
**	Print usage.
*/
static void PrintUsage(void)
{
    printf("Usage: wmdia [-e cmd] [-f font] [-h] [-n name] [-w]\n"
	"\t-e cmd\tExecute command after setup\n"
	"\t-f font\tFont for tooltip\n" "\t-h\tDisplay this text\n"
	"\t-n name\tChange window name (default wmdia)\n"
	"\t-w\tStart in window mode\n" "Only idiots print usage on stderr!\n");
}

/**
**	Main entry point.
**
**	@param argc	number of arguments
**	@param argv	arguments vector
*/
int main(int argc, char *const argv[])
{
    char *execute_cmd;

    execute_cmd = NULL;
    Name = "wmdia";
    FontTooltip = FONT;			// setup defaults

    //
    //	Parse arguments.
    //
    for (;;) {
	switch (getopt(argc, argv, "h?-e:f:n:w")) {
	    case 'e':			// execute command
		execute_cmd = optarg;
		continue;
	    case 'f':			// font of tooltip
		FontTooltip = optarg;
		continue;
	    case 'n':			// change window name
		Name = optarg;
		continue;
	    case 'w':			// window mode
		WindowMode = 1;
		continue;

	    case EOF:
		break;
	    case '?':
	    case 'h':			// help usage
		PrintVersion();
		PrintUsage();
		return 0;
	    case '-':
		fprintf(stderr, "We need no long options\n");
		PrintUsage();
		return -1;
	    case ':':
		PrintVersion();
		fprintf(stderr, "Missing argument for option '%c'\n", optopt);
		return -1;
	    default:
		PrintVersion();
		fprintf(stderr, "Unknown option '%c'\n", optopt);
		return -1;
	}
	break;
    }
    if (optind < argc) {
	PrintVersion();
	while (optind < argc) {
	    fprintf(stderr, "Unhandled argument '%s'\n", argv[optind++]);
	}
	return -1;
    }

    if (Init(argc, argv) < 0) {
	return -1;
    }
    PrepareData();
    if (execute_cmd) {
	System(execute_cmd);
    }
    // show initial content
    xcb_clear_area(Connection, 0, Window, 0, 0, 64, 64);
    // flush the request
    xcb_flush(Connection);

    Loop();
    Exit();

    return 0;
}
