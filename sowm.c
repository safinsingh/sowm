// sowm - An itsy bitsy floating window manager.

#include <X11/Xlib.h>
#include <X11/XF86keysym.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "sowm.h"

static client       *list = {0}, *ws_list[6] = {0}, *cur;
static int          ws = 1, sw, sh, wx, wy, numlock = 0;
static unsigned int ww, wh;
static int s;

static Display      *d;
static XButtonEvent mouse;
static Window       root;

static void (*events[LASTEvent])(XEvent *e) = {
    [ButtonPress]      = button_press,
    [ButtonRelease]    = button_release,
    [ConfigureRequest] = configure_request,
    [KeyPress]         = key_press,
    [MapRequest]       = map_request,
    [MappingNotify]    = mapping_notify,
    [DestroyNotify]    = notify_destroy,
    [EnterNotify]      = notify_enter,
    [MotionNotify]     = notify_motion,
    [ClientMessage]    = client_message
};

enum atoms_net {
    NetSupported,
    NetNumberOfDesktops,
    NetCurrentDesktop,
    NetClientList,
    NetWMWindowType,
    NetWMWindowTypeDialog,
    NetWMWindowTypeMenu,
    NetWMWindowTypeTooltip,
    NetWMWindowTypeNotification,
    NetWMState,
    NetLast
};


// thanks berrywm
static Atom net_atom[NetLast];

#include "config.h"

unsigned long getcolor(const char *col) {
    Colormap m = DefaultColormap(d, s);
    XColor c;
    return (!XAllocNamedColor(d, m, col, &c, &c))?0:c.pixel;
}

int win_class_contains(Window w, char* needle) {
    XClassHint class_hint;
    int found = 0;

    if (XGetClassHint(d, w, &class_hint)) {
        if (class_hint.res_class) {
            found |= strstr(class_hint.res_class, needle) != NULL;
            XFree(class_hint.res_class);
        }
        if (class_hint.res_name) {
            found |= strstr(class_hint.res_name, needle) != NULL;
            XFree(class_hint.res_name);
        }
    }
    return found;
}

int is_bar(Window w) {
    return win_class_contains(w, barname);
}

int is_auxiliary_window(Window w) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom *prop = NULL;

    if (XGetWindowProperty(d, w, net_atom[NetWMWindowType], 0, sizeof(Atom), False, XA_ATOM, 
                           &actual_type, &actual_format, &nitems, &bytes_after, (unsigned char **) &prop) == Success) {
        if (nitems > 0) {
            // Check if the window is a dialog, menu, tooltip, or notification
            if (*prop == net_atom[NetWMWindowTypeDialog] ||
                *prop == net_atom[NetWMWindowTypeMenu] ||
                *prop == net_atom[NetWMWindowTypeTooltip] ||
                *prop == net_atom[NetWMWindowTypeNotification]) {
                XFree(prop);
                return 1;
            }
        }
        XFree(prop);
    }

    return 0;
}

int is_visible_window(Window w) {
    XWindowAttributes attr;
    XGetWindowAttributes(d, w, &attr);

    // Check if the window is viewable (mapped and visible) and not override-redirect
    if (attr.map_state != IsViewable || attr.override_redirect) {
        return 0;
    }

    // Skip 1x1 windows (or similarly tiny ones)
    if (attr.width <= 1 && attr.height <= 1) {
        return 0;
    }

    return 1;
}

int is_useless_window(Window w) {
    return is_bar(w) || !is_visible_window(w) || is_auxiliary_window(w);
}

void win_focus(client *c) {
    if (is_useless_window(c->w)) return;    cur = c;

    XSetInputFocus(d, cur->w, RevertToParent, CurrentTime);
}

void notify_destroy(XEvent *e) {
    win_del(e->xdestroywindow.window);

    if (list) win_focus(list->prev);
}

void notify_enter(XEvent *e) {
    while(XCheckTypedEvent(d, EnterNotify, e));

    for win if (c->w == e->xcrossing.window) win_focus(c);
}

void notify_motion(XEvent *e) {
    if (!mouse.subwindow || cur->wrsz == WIN_RSZ_FS) return;

    while(XCheckTypedEvent(d, MotionNotify, e));

    int xd = e->xbutton.x_root - mouse.x_root;
    int yd = e->xbutton.y_root - mouse.y_root;

    XMoveResizeWindow(d, mouse.subwindow,
        wx + (mouse.button == 1 ? xd : 0),
        wy + (mouse.button == 1 ? yd : 0),
        MAX(1, ww + (mouse.button == 3 ? xd : 0)),
        MAX(1, wh + (mouse.button == 3 ? yd : 0)));
}

void key_press(XEvent *e) {
    KeySym keysym = XkbKeycodeToKeysym(d, e->xkey.keycode, 0, 0);

    for (unsigned int i=0; i < sizeof(keys)/sizeof(*keys); ++i)
        if (keys[i].keysym == keysym &&
            mod_clean(keys[i].mod) == mod_clean(e->xkey.state))
            keys[i].function(keys[i].arg);
}

void button_press(XEvent *e) {
    if (!e->xbutton.subwindow) return;

    win_size(e->xbutton.subwindow, &wx, &wy, &ww, &wh);
    XRaiseWindow(d, e->xbutton.subwindow);
    mouse = e->xbutton;
}

void button_release(XEvent *e) {
    mouse.subwindow = 0;
}

// adding always makes a ws active; polybar only cares _NET_CLIENT_LIST>0
void ewmh_set_ws_active() {
    XChangeProperty(d, root, net_atom[NetClientList], XA_WINDOW, 32, PropModeReplace, (unsigned char *)cur->w, 1);
}

void win_add(Window w) {
    client *c;

    if (!(c = (client *) calloc(1, sizeof(client))))
        exit(1);

    c->w = w;

    if (list) {
        list->prev->next = c;
        c->prev          = list->prev;
        list->prev       = c;
        c->next          = list;

    } else {
        list = c;
        list->prev = list->next = list;
    }

    ws_save(ws);
    // ewmh_set_ws_active();
}

// only on del do we need to check if the ws has 0 clients
void ewmh_set_ws_inactive() {
    Window empty_list[1] = {0};
    XChangeProperty(d, root, net_atom[NetClientList], XA_WINDOW, 32, PropModeReplace, (unsigned char *)empty_list, 0);
}

void win_del(Window w) {
    client *x = 0;

    for win if (c->w == w) x = c;

    if (!list || !x)  return;
    if (x->prev == x) {
        list = cur = 0;
        // ewmh_set_ws_inactive();
    }
    if (list == x)    list = x->next;
    if (x->next)      x->next->prev = x->prev;
    if (x->prev)      x->prev->next = x->next;

    free(x);
    ws_save(ws);
}

void win_kill(const Arg arg) {
    if (cur) XKillClient(d, cur->w);
}

void win_center(const Arg arg) {
    if (!cur) return;

    win_size(cur->w, &(int){0}, &(int){0}, &ww, &wh);
    XMoveWindow(d, cur->w, (sw - ww) / 2, (sh - wh) / 2);
}

void cur_win_resize_proc(win_resized_ty_t target_wrsz, int x, int y, unsigned int w, unsigned int h) {
    if (!cur) return;

    if (cur->wrsz == target_wrsz) {
        XMoveResizeWindow(d, cur->w, cur->wx, cur->wy, cur->ww, cur->wh);
        cur->wrsz = WIN_RSZ_OG;
        return;
    } else if (cur->wrsz == WIN_RSZ_OG) {
        win_size(cur->w, &cur->wx, &cur->wy, &cur->ww, &cur->wh);
    }
    XMoveResizeWindow(d, cur->w, x, y, w, h);
    cur->wrsz = target_wrsz; 
}

void win_snap_left(const Arg arg) {
    cur_win_resize_proc(WIN_RSZ_SNL, 0, SCREEN_TOP, sw / 2, sh - BAR_SIZE);
}

void win_snap_right(const Arg arg) {
    cur_win_resize_proc(WIN_RSZ_SNR, sw / 2, SCREEN_TOP, sw / 2, sh - BAR_SIZE);
}

void win_fs(const Arg arg) {
    cur_win_resize_proc(WIN_RSZ_FS, 0, SCREEN_TOP, sw, sh - BAR_SIZE);
    XRaiseWindow(d, cur->w);
}

// find last two (non-bar) focused clients
int find_last2(client* cs[2]) {
    if (!cur) return 0;

    client* front = cur;
    client* c = cur;
    int found = 0;

    do {
        if (!is_useless_window(c->w)) {
            cs[found++] = c;
            if (found == 2) return 1;
        }
        c = c->prev;
    } while (c != front);
    return 0;
}

void win_split2(const Arg arg) {
    client* cs[2];
    if (find_last2(cs)) {
        client* cur_client = cur;
        cur = cs[0];
        win_snap_left((Arg){0});
        cur = cs[1];
        win_snap_right((Arg){0});
        cur = cur_client;
    }
}

// find last two snapped clients [L,R] ==> returns first wrsz
int find_last2_snapped(client* cs[2]) {
    if (!cur) return 0;

    client* front = cur;
    client* c = cur;
    win_resized_ty_t first_wrsz = 0;

    do {
        if (!is_useless_window(c->w)) {
            if (c->wrsz == WIN_RSZ_SNL || c->wrsz == WIN_RSZ_SNR) {
                if (first_wrsz == 0) {
                    first_wrsz = c->wrsz;
                    cs[0] = c;
                } else if (c->wrsz != first_wrsz) {
                    cs[1] = c;
                    return first_wrsz;
                }
            }
        }
        c = c->prev;
    } while (c != front);
    return 0;
}

void win_swap2(const Arg arg) {
    client* cs[2];
    win_resized_ty_t first_wrsz;


    if ((first_wrsz = find_last2_snapped(cs))) {
        client* cur_client = cur;
        int first_snr = first_wrsz == WIN_RSZ_SNR; // waow
        cur = cs[first_snr];
        win_snap_right((Arg){0});
        cur = cs[!first_snr];
        win_snap_left((Arg){0});
        cur = cur_client;
    }
}

void win_to_ws(const Arg arg) {
    int tmp = ws;

    if (arg.i == tmp) return;

    ws_sel(arg.i);
    win_add(cur->w);
    ws_save(arg.i);

    ws_sel(tmp);
    XUnmapWindow(d, cur->w);
    win_del(cur->w);
    ws_save(tmp);

    if (list) win_focus(list);
}

void win_prev(const Arg arg) {
    if (!cur) return;

    client *c = cur->prev;
    while (is_useless_window(c->w)) {
        c = c->prev;
        if (c == cur) return; // Prevent infinite loop
    }

    XRaiseWindow(d, c->w);
    win_focus(c);
}

void win_next(const Arg arg) {
    if (!cur) return;

    client *c = cur->next;
    while (is_useless_window(c->w)) {
        c = c->next;
        if (c == cur) return; // Prevent infinite loop
    }

    XRaiseWindow(d, c->w);
    win_focus(c);
}

void ewmh_set_current_desktop() {
    long desktop = ws - 1;
    XChangeProperty(d, root, net_atom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &desktop, 1);
}

void ws_go(const Arg arg) {
    int tmp = ws;

    if (arg.i == ws) return;

    ws_save(ws);
    ws_sel(arg.i);

    for win XMapWindow(d, c->w);

    ws_sel(tmp);

    for win if (!is_bar(c->w)) XUnmapWindow(d, c->w);

    ws_sel(arg.i);

    if (list) win_focus(list); else cur = 0;

    ewmh_set_current_desktop();
}

void client_message(XEvent* e) {
    XClientMessageEvent* ev = &e->xclient;
    if (ev->message_type == net_atom[NetCurrentDesktop]) {
        unsigned long desktop = ev->data.l[0] + 1;
        ws_go((Arg){.i=desktop});
    }
}

void configure_request(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;

    XConfigureWindow(d, ev->window, ev->value_mask, &(XWindowChanges) {
        .x          = ev->x,
        .y          = ev->y,
        .width      = ev->width,
        .height     = ev->height,
        .sibling    = ev->above,
        .stack_mode = ev->detail
    });
}

void map_request(XEvent *e) {
    Window w = e->xmaprequest.window;

    XSelectInput(d, w, StructureNotifyMask|EnterWindowMask);
    win_add(w);
    cur = list->prev;

    if (wx + wy == 0) win_center((Arg){0});

    int fs = 0;
    for (int i = 0; auto_fullscreen[i] != 0; i++) {
        if (win_class_contains(w, auto_fullscreen[i])) { win_fs((Arg){0}); fs = 1; break; }
    }
    if (!fs) {win_center((Arg){0});}
    win_size(cur->w, &cur->wx, &cur->wy, &cur->ww, &cur->wh);

    XMapWindow(d, w);
    win_focus(list->prev);
}

void mapping_notify(XEvent *e) {
    XMappingEvent *ev = &e->xmapping;

    if (ev->request == MappingKeyboard || ev->request == MappingModifier) {
        XRefreshKeyboardMapping(ev);
        input_grab(root);
    }
}

void run(const Arg arg) {
    if (fork()) return;
    if (d) close(ConnectionNumber(d));

    setsid();
    execvp((char*)arg.com[0], (char**)arg.com);
}

void input_grab(Window root) {
    unsigned int i, j, modifiers[] = {0, LockMask, numlock, numlock|LockMask};
    XModifierKeymap *modmap = XGetModifierMapping(d);
    KeyCode code;

    for (i = 0; i < 8; i++)
        for (int k = 0; k < modmap->max_keypermod; k++)
            if (modmap->modifiermap[i * modmap->max_keypermod + k]
                == XKeysymToKeycode(d, 0xff7f))
                numlock = (1 << i);

    XUngrabKey(d, AnyKey, AnyModifier, root);

    for (i = 0; i < sizeof(keys)/sizeof(*keys); i++)
        if ((code = XKeysymToKeycode(d, keys[i].keysym)))
            for (j = 0; j < sizeof(modifiers)/sizeof(*modifiers); j++)
                XGrabKey(d, code, keys[i].mod | modifiers[j], root,
                        True, GrabModeAsync, GrabModeAsync);

    for (i = 1; i < 4; i += 2)
        for (j = 0; j < sizeof(modifiers)/sizeof(*modifiers); j++)
            XGrabButton(d, i, MOD | modifiers[j], root, True,
                ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
                GrabModeAsync, GrabModeAsync, 0, 0);

    XFreeModifiermap(modmap);
}

int main(void) {
    XEvent ev;

    if (!(d = XOpenDisplay(0))) exit(1);

    signal(SIGCHLD, SIG_IGN);
    XSetErrorHandler(xerror);

    int s = DefaultScreen(d);
    root  = RootWindow(d, s);
    sw    = XDisplayWidth(d, s);
    sh    = XDisplayHeight(d, s);
         
    XSelectInput(d,  root, SubstructureRedirectMask);
    XDefineCursor(d, root, XCreateFontCursor(d, 68));

    // ewmh atoms
    net_atom[NetSupported]              = XInternAtom(d, "_NET_SUPPORTED", False);
    net_atom[NetNumberOfDesktops]       = XInternAtom(d, "_NET_NUMBER_OF_DESKTOPS", False);
    net_atom[NetCurrentDesktop]         = XInternAtom(d, "_NET_CURRENT_DESKTOP", False);
    net_atom[NetClientList]             = XInternAtom(d, "_NET_CLIENT_LIST", False);
    net_atom[NetWMWindowType]           = XInternAtom(d, "_NET_WM_WINDOW_TYPE", False);
    net_atom[NetWMWindowTypeDialog]     = XInternAtom(d, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    net_atom[NetWMWindowTypeMenu]       = XInternAtom(d, "_NET_WM_WINDOW_TYPE_MENU", False);
    net_atom[NetWMWindowTypeTooltip]    = XInternAtom(d, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
    net_atom[NetWMWindowTypeNotification] = XInternAtom(d, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    net_atom[NetWMState]                = XInternAtom(d, "_NET_WM_STATE", False);

    XChangeProperty(d, root, net_atom[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *)net_atom, NetLast);
    long number_of_desktops = sizeof(ws_list) / sizeof(ws_list[0]);
    XChangeProperty(d, root, net_atom[NetNumberOfDesktops], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&number_of_desktops, 1);

    input_grab(root);

    while (1 && !XNextEvent(d, &ev)) // 1 && will forever be here.
        if (events[ev.type]) events[ev.type](&ev);
}
