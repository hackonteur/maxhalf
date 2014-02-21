/* swapmonitor is an XLib based helper to swap windows between
   the two monitors of a Xinerama-like dislpay */
/*

    Copyright (C) 2008 - Julien Bramary

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define verbose 1
#define v_printf(...) if (verbose) { \
    fprintf(stderr, __VA_ARGS__); \
}

XEvent event;
Display *disp;
unsigned long grflags;
long mask;
int x, y, new_x, new_y, new_width, new_height;
XWindowAttributes active_atr;
XWindowAttributes root_atr;

void move() {
  event.xclient.message_type = XInternAtom(disp, "_NET_MOVERESIZE_WINDOW", False);
  event.xclient.data.l[0] = grflags;
  event.xclient.data.l[1] = (unsigned long)new_x;
  event.xclient.data.l[2] = (unsigned long)new_y;
  event.xclient.data.l[3] = (unsigned long)new_width;
  event.xclient.data.l[4] = (unsigned long)new_height;
  XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event);

  v_printf("Move: %ux%u @ (%d,%d) in %ux%u\n", new_width, active_atr.height, new_x, new_y, root_atr.width, root_atr.height);

}


int main (int argc, char **argv) {
  
  int ret = EXIT_SUCCESS;
  int actual_format_return;
  
  unsigned long nitems_return;
  unsigned long bytes_after_return;
  
  unsigned char *data;
  
  Window win = (Window)0;
  Window w_dum = (Window)0;
  
  // Check we can get the display to avoid looking stupid later
  if (! (disp = XOpenDisplay(NULL))) {
    fputs("Cannot open display.\n", stderr);
    return EXIT_FAILURE;
  }
  
  // Define Atoms
  Atom ret_atom = (Atom)0;
  Atom maximized_vertically   = XInternAtom( disp, "_NET_WM_STATE_MAXIMIZED_VERT", False );
  Atom maximized_horizontally = XInternAtom( disp, "_NET_WM_STATE_MAXIMIZED_HORZ", False );
  
  // Get active window
  XGetWindowProperty(disp, DefaultRootWindow(disp), XInternAtom(disp, "_NET_ACTIVE_WINDOW", False),
                     0, 1024, False, XA_WINDOW, &ret_atom,
                     &actual_format_return, &nitems_return, &bytes_after_return, &data);
	
  win = *((Window *)data);
  XFree(data);
  
  // Get coordinates
  XGetWindowAttributes(disp, win, &active_atr);
  XTranslateCoordinates (disp, win, active_atr.root, active_atr.x, active_atr.y, &x, &y, &w_dum);
	
	
  v_printf("%ux%u @ (%d,%d)\n", active_atr.width, active_atr.height, x, y);

  // Get resolution
  XGetWindowAttributes(disp, active_atr.root, &root_atr);
  v_printf("Resolution: %ux%u\n", root_atr.width, root_atr.height);
	

  // Determine maximized state
  Bool isMaxVert = False;
  Bool isMaxHorz = False;
  XGetWindowProperty (disp, win, XInternAtom(disp, "_NET_WM_STATE", False),
                      0, 1024, False, XA_ATOM, &ret_atom, 
                      &actual_format_return, &nitems_return, &bytes_after_return, &data);
	

  unsigned long *buff = (unsigned long *)data;
  for (; nitems_return; nitems_return--) {
    if ( *buff == (unsigned long) maximized_vertically) {
      isMaxVert = True;
      v_printf("Is Vertically maximized\n");
    }
    else if ( *buff == (unsigned long) maximized_horizontally) {
      isMaxHorz = True;
      v_printf("Is Horizontally maximized\n");
    }
    buff++;
  }
  XFree(data);

  // Calculate new X
  // Move to the side of the display that the middle of the window is on
  new_x = ( (x+(active_atr.width/2)) > (root_atr.width/2) ) ? root_atr.width/2 : 0;
    
  // Use root gravity
  grflags = 0;

  new_y = 0;

  // Is there a better way to figure out decoration padding?
  new_width = root_atr.width/2 - 8;
	
  grflags |= 0x100; // Move x
  grflags |= 0x200; // Move y
  grflags |= 0x400; // Set width

  // With emacs, if we set the vertical maximization it resizes itself to be an integer number of lines and unsets
  // the vertical maximization. This makes the window just slightly smaller than the display height. When a window
  // is already almost the height of the display then we don't really want to fully maximize it. Instead we set
  // set the height to half of the display height.
  Bool wantMaxVert = False;
  if (!isMaxVert && ((active_atr.height + 100)) > root_atr.height) {
    new_height = root_atr.height / 2;
    grflags |= 0x800; // Set height
    v_printf("Setting height\n");
    wantMaxVert = False;
  } else {
    new_height = active_atr.height;
    v_printf("Toggling vertical maximization\n");
    wantMaxVert = !isMaxVert;
  }

  mask = SubstructureRedirectMask | SubstructureNotifyMask;
  event.xclient.type = ClientMessage;
  event.xclient.serial = 0;
  event.xclient.send_event = True;
  event.xclient.window = win;
  event.xclient.format = 32;


  // De-maximize first, seems necessary in some cases
  event.xclient.message_type = XInternAtom(disp, "_NET_WM_STATE", False);
  event.xclient.data.l[0] = (unsigned long)0;
  event.xclient.data.l[1] = isMaxVert ? (unsigned long)maximized_vertically : 0;
  event.xclient.data.l[2] = isMaxHorz ? (unsigned long)maximized_horizontally : 0;
	
  XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event);

  // actual move
  move();
  
  
  
  // restore maximization state
  event.xclient.message_type = XInternAtom(disp, "_NET_WM_STATE", False);
  event.xclient.data.l[0] = (unsigned long)1;
  event.xclient.data.l[1] = wantMaxVert ? (unsigned long)maximized_vertically : 0;
  event.xclient.data.l[2] = 0;
  
  XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event);
  
  XCloseDisplay(disp);
  
  return ret;
}
