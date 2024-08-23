#ifndef CONFIG_H
#define CONFIG_H

#define MOD Mod4Mask

#define BAR_SIZE 24
#define BAR_BOTTOM (1)
#define SCREEN_TOP (BAR_BOTTOM ? 0 : BAR_SIZE)
const char* barname = "polybar";

//xclasshint->res_name [0]
const char* auto_fullscreen[] = {"brave", "code", 0};

const char* menu[]    = {"rofi", "-show", "run",    0};
const char* term[]    = {"alacritty",               0};
const char* scrot[]   = {"scr",                     0};
const char* briup[]   = {"bri", "10", "+",          0};
const char* bridown[] = {"bri", "10", "-",          0};
const char* ex[]      = {"pkill", "sowm",           0};
const char* voldown[] = {"pactl", "set-sink-volume", "0", "-5%",    0};
const char* volup[]   = {"pactl", "set-sink-volume", "0", "+5%",    0};
const char* volmute[] = {"pactl", "set-sink-mute", "0", "toggle",   0};

static struct key keys[] = {
    {MOD,      XK_w,   win_kill,        {0}},
    {MOD,      XK_c,   win_center,      {0}},
    {MOD,      XK_f,   win_fs,          {0}},

    {MOD,           XK_l,   win_snap_left,   {0}},
    {MOD,           XK_r,   win_snap_right,  {0}},
    {MOD,           XK_s,   win_split2,      {0}},
    {MOD|ShiftMask, XK_s,   win_swap2,       {0}},
    
    {Mod1Mask,           XK_Tab, win_next,   {0}},
    {Mod1Mask|ShiftMask, XK_Tab, win_prev,   {0}},

    {MOD, XK_space,     run, {.com = menu}},
    {MOD, XK_p,         run, {.com = scrot}},
    {MOD, XK_Return,    run, {.com = term}},

    {MOD|ShiftMask, XK_e,    run, {.com = ex}},

    {0,   XF86XK_AudioLowerVolume,  run, {.com = voldown}},
    {0,   XF86XK_AudioRaiseVolume,  run, {.com = volup}},
    {0,   XF86XK_AudioMute,         run, {.com = volmute}},
    {0,   XF86XK_MonBrightnessUp,   run, {.com = briup}},
    {0,   XF86XK_MonBrightnessDown, run, {.com = bridown}},

    {MOD,           XK_1, ws_go,     {.i = 1}},
    {MOD|ShiftMask, XK_1, win_to_ws, {.i = 1}},
    {MOD,           XK_2, ws_go,     {.i = 2}},
    {MOD|ShiftMask, XK_2, win_to_ws, {.i = 2}},
    {MOD,           XK_3, ws_go,     {.i = 3}},
    {MOD|ShiftMask, XK_3, win_to_ws, {.i = 3}},
    {MOD,           XK_4, ws_go,     {.i = 4}},
    {MOD|ShiftMask, XK_4, win_to_ws, {.i = 4}},
    {MOD,           XK_5, ws_go,     {.i = 5}},
    {MOD|ShiftMask, XK_5, win_to_ws, {.i = 5}},
    {MOD,           XK_6, ws_go,     {.i = 6}},
    {MOD|ShiftMask, XK_6, win_to_ws, {.i = 6}},
};

#endif
