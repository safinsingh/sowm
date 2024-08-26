#include <X11/Xlib.h>

#define win        (client *t=0, *c=list; c && t!=list->prev; t=c, c=c->next)
#define win_back   (client *t=0, *c=list->prev; c && t!=list; t=c, c=c->prev)
#define ws_save(W) ws_list[W] = list
#define ws_sel(W)  list = ws_list[ws = W]
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

#define win_size(W, gx, gy, gw, gh) \
    XGetGeometry(d, W, &(Window){0}, gx, gy, gw, gh, \
                 &(unsigned int){0}, &(unsigned int){0})

// Taken from DWM. Many thanks. https://git.suckless.org/dwm
#define mod_clean(mask) (mask & ~(numlock|LockMask) & \
        (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

typedef struct {
    const char** com;
    const int i;
    const Window w;
} Arg;

struct key {
    unsigned int mod;
    KeySym keysym;
    void (*function)(const Arg arg);
    const Arg arg;
};

typedef enum win_resized_ty {
    WIN_RSZ_OG  = 0,
    WIN_RSZ_SNL = 1 << 0,
    WIN_RSZ_SNR = 1 << 1,
    WIN_RSZ_FS  = 1 << 2
} win_resized_ty_t;

typedef struct client {
    struct client *next, *prev;
    // wrsz = "window resized to" type
    win_resized_ty_t wrsz;
    int wx, wy;
    unsigned int ww, wh;
    Window w;
} client;

void button_press(XEvent *e);
void button_release(XEvent *e);
void configure_request(XEvent *e);
void input_grab(Window root);
void key_press(XEvent *e);
void map_request(XEvent *e);
void mapping_notify(XEvent *e);
void notify_destroy(XEvent *e);
void notify_enter(XEvent *e);
void notify_motion(XEvent *e);
void client_message(XEvent* e);
void run(const Arg arg);
void win_add(Window w);
void win_center(const Arg arg);
void win_snap_left(const Arg arg);
void win_snap_right(const Arg arg);
void win_split2(const Arg arg);
void win_swap2(const Arg arg);
void win_del(Window w);
void win_fs(const Arg arg);
void win_focus(client *c);
void win_kill(const Arg arg);
void win_prev(const Arg arg);
void win_next(const Arg arg);
void win_to_ws(const Arg arg);
void ws_go(const Arg arg);

static int xerror() { return 0; }
