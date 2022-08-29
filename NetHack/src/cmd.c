/* NetHack 3.6	cmd.c	$NHDT-Date: 1575245052 2019/12/02 00:04:12 $  $NHDT-Branch: NetHack-3.6 $:$NHDT-Revision: 1.350 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2013. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"
#include "func_tab.h"

/* Macros for meta and ctrl modifiers:
 *   M and C return the meta/ctrl code for the given character;
 *     e.g., (C('c') is ctrl-c
 */
#ifndef M
#ifndef NHSTDC
#define M(c) (0x80 | (c))
#else
#define M(c) ((c) - 128)
#endif /* NHSTDC */
#endif

#ifndef C
#define C(c) (0x1f & (c))
#endif

#define unctrl(c) ((c) <= C('z') ? (0x60 | (c)) : (c))
#define unmeta(c) (0x7f & (c))

#ifdef ALTMETA
STATIC_VAR boolean alt_esc = FALSE;
#endif

struct cmd Cmd = { 0 }; /* flag.h */

extern const char *hu_stat[];  /* hunger status from eat.c */
extern const char *enc_stat[]; /* encumbrance status from botl.c */

#ifdef UNIX
/*
 * Some systems may have getchar() return EOF for various reasons, and
 * we should not quit before seeing at least NR_OF_EOFS consecutive EOFs.
 */
#if defined(SYSV) || defined(DGUX) || defined(HPUX)
#define NR_OF_EOFS 20
#endif
#endif

#define CMD_TRAVEL (char) 0x90
#define CMD_CLICKLOOK (char) 0x8F

#ifdef DEBUG
extern int NDECL(wiz_debug_cmd_bury);
#endif

#ifdef DUMB /* stuff commented out in extern.h, but needed here */
extern int NDECL(doapply);            /**/
extern int NDECL(dorub);              /**/
extern int NDECL(dojump);             /**/
extern int NDECL(doextlist);          /**/
extern int NDECL(enter_explore_mode); /**/
extern int NDECL(dodrop);             /**/
extern int NDECL(doddrop);            /**/
extern int NDECL(dodown);             /**/
extern int NDECL(doup);               /**/
extern int NDECL(donull);             /**/
extern int NDECL(dowipe);             /**/
extern int NDECL(docallcnd);          /**/
extern int NDECL(dotakeoff);          /**/
extern int NDECL(doremring);          /**/
extern int NDECL(dowear);             /**/
extern int NDECL(doputon);            /**/
extern int NDECL(doddoremarm);        /**/
extern int NDECL(dokick);             /**/
extern int NDECL(dofire);             /**/
extern int NDECL(dothrow);            /**/
extern int NDECL(doeat);              /**/
extern int NDECL(done2);              /**/
extern int NDECL(vanquished);         /**/
extern int NDECL(doengrave);          /**/
extern int NDECL(dopickup);           /**/
extern int NDECL(ddoinv);             /**/
extern int NDECL(dotypeinv);          /**/
extern int NDECL(dolook);             /**/
extern int NDECL(doprgold);           /**/
extern int NDECL(doprwep);            /**/
extern int NDECL(doprarm);            /**/
extern int NDECL(doprring);           /**/
extern int NDECL(dopramulet);         /**/
extern int NDECL(doprtool);           /**/
extern int NDECL(dosuspend);          /**/
extern int NDECL(doforce);            /**/
extern int NDECL(doopen);             /**/
extern int NDECL(doclose);            /**/
extern int NDECL(dosh);               /**/
extern int NDECL(dodiscovered);       /**/
extern int NDECL(doclassdisco);       /**/
extern int NDECL(doset);              /**/
extern int NDECL(dotogglepickup);     /**/
extern int NDECL(dowhatis);           /**/
extern int NDECL(doquickwhatis);      /**/
extern int NDECL(dowhatdoes);         /**/
extern int NDECL(dohelp);             /**/
extern int NDECL(dohistory);          /**/
extern int NDECL(doloot);             /**/
extern int NDECL(dodrink);            /**/
extern int NDECL(dodip);              /**/
extern int NDECL(dosacrifice);        /**/
extern int NDECL(dopray);             /**/
extern int NDECL(dotip);              /**/
extern int NDECL(doturn);             /**/
extern int NDECL(doredraw);           /**/
extern int NDECL(doread);             /**/
extern int NDECL(dosave);             /**/
extern int NDECL(dosearch);           /**/
extern int NDECL(doidtrap);           /**/
extern int NDECL(dopay);              /**/
extern int NDECL(dosit);              /**/
extern int NDECL(dotalk);             /**/
extern int NDECL(docast);             /**/
extern int NDECL(dovspell);           /**/
extern int NDECL(dotelecmd);          /**/
extern int NDECL(dountrap);           /**/
extern int NDECL(doversion);          /**/
extern int NDECL(doextversion);       /**/
extern int NDECL(doswapweapon);       /**/
extern int NDECL(dowield);            /**/
extern int NDECL(dowieldquiver);      /**/
extern int NDECL(dozap);              /**/
extern int NDECL(doorganize);         /**/
#endif /* DUMB */

static int NDECL((*timed_occ_fn));

STATIC_PTR int NDECL(dosuspend_core);
STATIC_PTR int NDECL(dosh_core);
STATIC_PTR int NDECL(doherecmdmenu);
STATIC_PTR int NDECL(dotherecmdmenu);
STATIC_PTR int NDECL(doprev_message);
STATIC_PTR int NDECL(timed_occupation);
STATIC_PTR int NDECL(doextcmd);
STATIC_PTR int NDECL(dotravel);
STATIC_PTR int NDECL(doterrain);
STATIC_PTR int NDECL(wiz_wish);
STATIC_PTR int NDECL(wiz_identify);
STATIC_PTR int NDECL(wiz_intrinsic);
STATIC_PTR int NDECL(wiz_map);
STATIC_PTR int NDECL(wiz_makemap);
STATIC_PTR int NDECL(wiz_genesis);
STATIC_PTR int NDECL(wiz_where);
STATIC_PTR int NDECL(wiz_detect);
STATIC_PTR int NDECL(wiz_panic);
STATIC_PTR int NDECL(wiz_polyself);
STATIC_PTR int NDECL(wiz_level_tele);
STATIC_PTR int NDECL(wiz_level_change);
STATIC_PTR int NDECL(wiz_show_seenv);
STATIC_PTR int NDECL(wiz_show_vision);
STATIC_PTR int NDECL(wiz_smell);
STATIC_PTR int NDECL(wiz_show_wmodes);
STATIC_DCL void NDECL(wiz_map_levltyp);
STATIC_DCL void NDECL(wiz_levltyp_legend);
#if defined(__BORLANDC__) && !defined(_WIN32)
extern void FDECL(show_borlandc_stats, (winid));
#endif
#ifdef DEBUG_MIGRATING_MONS
STATIC_PTR int NDECL(wiz_migrate_mons);
#endif
STATIC_DCL int FDECL(size_monst, (struct monst *, BOOLEAN_P));
STATIC_DCL int FDECL(size_obj, (struct obj *));
STATIC_DCL void FDECL(count_obj, (struct obj *, long *, long *,
                                  BOOLEAN_P, BOOLEAN_P));
STATIC_DCL void FDECL(obj_chain, (winid, const char *, struct obj *,
                                  BOOLEAN_P, long *, long *));
STATIC_DCL void FDECL(mon_invent_chain, (winid, const char *, struct monst *,
                                         long *, long *));
STATIC_DCL void FDECL(mon_chain, (winid, const char *, struct monst *,
                                  BOOLEAN_P, long *, long *));
STATIC_DCL void FDECL(contained_stats, (winid, const char *, long *, long *));
STATIC_DCL void FDECL(misc_stats, (winid, long *, long *));
STATIC_PTR int NDECL(wiz_show_stats);
STATIC_DCL boolean FDECL(accept_menu_prefix, (int NDECL((*))));
STATIC_PTR int NDECL(wiz_rumor_check);
STATIC_PTR int NDECL(doattributes);

STATIC_DCL void FDECL(enlght_out, (const char *));
STATIC_DCL void FDECL(enlght_line, (const char *, const char *, const char *,
                                    const char *));
STATIC_DCL char *FDECL(enlght_combatinc, (const char *, int, int, char *));
STATIC_DCL void FDECL(enlght_halfdmg, (int, int));
STATIC_DCL boolean NDECL(walking_on_water);
STATIC_DCL boolean FDECL(cause_known, (int));
STATIC_DCL char *FDECL(attrval, (int, int, char *));
STATIC_DCL void FDECL(background_enlightenment, (int, int));
STATIC_DCL void FDECL(basics_enlightenment, (int, int));
STATIC_DCL void FDECL(characteristics_enlightenment, (int, int));
STATIC_DCL void FDECL(one_characteristic, (int, int, int));
STATIC_DCL void FDECL(status_enlightenment, (int, int));
STATIC_DCL void FDECL(attributes_enlightenment, (int, int));

STATIC_DCL void FDECL(add_herecmd_menuitem, (winid, int NDECL((*)),
                                             const char *));
STATIC_DCL char FDECL(here_cmd_menu, (BOOLEAN_P));
STATIC_DCL char FDECL(there_cmd_menu, (BOOLEAN_P, int, int));
STATIC_DCL char *NDECL(parse);
STATIC_DCL void FDECL(show_direction_keys, (winid, CHAR_P, BOOLEAN_P));
STATIC_DCL boolean FDECL(help_dir, (CHAR_P, int, const char *));

static const char *readchar_queue = "";
static coord clicklook_cc;
/* for rejecting attempts to use wizard mode commands */
static const char unavailcmd[] = "不可用的命令'%s'.";
/* for rejecting #if !SHELL, !SUSPEND */
static const char cmdnotavail[] = "'%s'命令不可用.";

STATIC_PTR int
doprev_message(VOID_ARGS)
{
    return nh_doprev_message();
}

/* Count down by decrementing multi */
STATIC_PTR int
timed_occupation(VOID_ARGS)
{
    (*timed_occ_fn)();
    if (multi > 0)
        multi--;
    return multi > 0;
}

/* If you have moved since initially setting some occupations, they
 * now shouldn't be able to restart.
 *
 * The basic rule is that if you are carrying it, you can continue
 * since it is with you.  If you are acting on something at a distance,
 * your orientation to it must have changed when you moved.
 *
 * The exception to this is taking off items, since they can be taken
 * off in a number of ways in the intervening time, screwing up ordering.
 *
 *      Currently:      Take off all armor.
 *                      Picking Locks / Forcing Chests.
 *                      Setting traps.
 */
void
reset_occupations()
{
    reset_remarm();
    reset_pick();
    reset_trapset();
}

/* If a time is given, use it to timeout this function, otherwise the
 * function times out by its own means.
 */
void
set_occupation(fn, txt, xtime)
int NDECL((*fn));
const char *txt;
int xtime;
{
    if (xtime) {
        occupation = timed_occupation;
        timed_occ_fn = fn;
    } else
        occupation = fn;
    occtxt = txt;
    occtime = 0;
    return;
}

STATIC_DCL char NDECL(popch);

/* Provide a means to redo the last command.  The flag `in_doagain' is set
 * to true while redoing the command.  This flag is tested in commands that
 * require additional input (like `throw' which requires a thing and a
 * direction), and the input prompt is not shown.  Also, while in_doagain is
 * TRUE, no keystrokes can be saved into the saveq.
 */
#define BSIZE 20
static char pushq[BSIZE], saveq[BSIZE];
static NEARDATA int phead, ptail, shead, stail;

STATIC_OVL char
popch()
{
    /* If occupied, return '\0', letting tgetch know a character should
     * be read from the keyboard.  If the character read is not the
     * ABORT character (as checked in pcmain.c), that character will be
     * pushed back on the pushq.
     */
    if (occupation)
        return '\0';
    if (in_doagain)
        return (char) ((shead != stail) ? saveq[stail++] : '\0');
    else
        return (char) ((phead != ptail) ? pushq[ptail++] : '\0');
}

char
pgetchar() /* courtesy of aeb@cwi.nl */
{
    register int ch;

    if (iflags.debug_fuzzer)
        return randomkey();
    if (!(ch = popch()))
        ch = nhgetch();
    return (char) ch;
}

/* A ch == 0 resets the pushq */
void
pushch(ch)
char ch;
{
    if (!ch)
        phead = ptail = 0;
    if (phead < BSIZE)
        pushq[phead++] = ch;
    return;
}

/* A ch == 0 resets the saveq.  Only save keystrokes when not
 * replaying a previous command.
 */
void
savech(ch)
char ch;
{
    if (!in_doagain) {
        if (!ch)
            phead = ptail = shead = stail = 0;
        else if (shead < BSIZE)
            saveq[shead++] = ch;
    }
    return;
}

/* here after # - now read a full-word command */
STATIC_PTR int
doextcmd(VOID_ARGS)
{
    int idx, retval;
    int NDECL((*func));

    /* keep repeating until we don't run help or quit */
    do {
        idx = get_ext_cmd();
        if (idx < 0)
            return 0; /* quit */

        func = extcmdlist[idx].ef_funct;
        if (!wizard && (extcmdlist[idx].flags & WIZMODECMD)) {
            You("不能做那个.");
            return 0;
        }
        if (iflags.menu_requested && !accept_menu_prefix(func)) {
            pline("'%s'前缀对%s命令没作用.",
                  visctrl(Cmd.spkeys[NHKF_REQMENU]),
                  extcmdlist[idx].ef_txt);
            iflags.menu_requested = FALSE;
        }
        retval = (*func)();
    } while (func == doextlist);

    return retval;
}

/* here after #? - now list all full-word commands and provid
   some navigation capability through the long list */
int
doextlist(VOID_ARGS)
{
    register const struct ext_func_tab *efp;
    char buf[BUFSZ], searchbuf[BUFSZ], promptbuf[QBUFSZ];
    winid menuwin;
    anything any;
    menu_item *selected;
    int n, pass;
    int menumode = 0, menushown[2], onelist = 0;
    boolean redisplay = TRUE, search = FALSE;
    static const char *headings[] = { "扩展命令",
                                      "调试扩展命令" };

    searchbuf[0] = '\0';
    menuwin = create_nhwindow(NHW_MENU);

    while (redisplay) {
        redisplay = FALSE;
        any = zeroany;
        start_menu(menuwin);
        add_menu(menuwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                 "扩展命令列表", MENU_UNSELECTED);
        add_menu(menuwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                 "", MENU_UNSELECTED);

        Strcpy(buf, menumode ? "显示" : "隐藏");
        Strcat(buf, "不自动完成的命令");
        if (!menumode)
            Strcat(buf, " (这些没有标[A])");
        any.a_int = 1;
        add_menu(menuwin, NO_GLYPH, &any, 'a', 0, ATR_NONE, buf,
                 MENU_UNSELECTED);

        if (!*searchbuf) {
            any.a_int = 2;
            /* was 's', but then using ':' handling within the interface
               would only examine the two or three meta entries, not the
               actual list of extended commands shown via separator lines;
               having ':' as an explicit selector overrides the default
               menu behavior for it; we retain 's' as a group accelerator */
            add_menu(menuwin, NO_GLYPH, &any, ':', 's', ATR_NONE,
                     "搜索扩展命令", MENU_UNSELECTED);
        } else {
            Strcpy(buf, "全部显示, 清除搜索");
            if (strlen(buf) + strlen(searchbuf) + strlen(" (\"\")") < QBUFSZ)
                Sprintf(eos(buf), " (\"%s\")", searchbuf);
            any.a_int = 3;
            /* specifying ':' as a group accelerator here is mostly a
               statement of intent (we'd like to accept it as a synonym but
               also want to hide it from general menu use) because it won't
               work for interfaces which support ':' to search; use as a
               general menu command takes precedence over group accelerator */
            add_menu(menuwin, NO_GLYPH, &any, 's', ':', ATR_NONE,
                     buf, MENU_UNSELECTED);
        }
        if (wizard) {
            any.a_int = 4;
            add_menu(menuwin, NO_GLYPH, &any, 'z', 0, ATR_NONE,
                     onelist ? "在单独的部分中显示调试命令"
                     : "按字母顺序显示所有命令, 包括调试命令",
                     MENU_UNSELECTED);
        }
        any = zeroany;
        add_menu(menuwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                 "", MENU_UNSELECTED);
        menushown[0] = menushown[1] = 0;
        n = 0;
        for (pass = 0; pass <= 1; ++pass) {
            /* skip second pass if not in wizard mode or wizard mode
               commands are being integrated into a single list */
            if (pass == 1 && (onelist || !wizard))
                break;
            for (efp = extcmdlist; efp->ef_txt; efp++) {
                int wizc;

                if ((efp->flags & CMD_NOT_AVAILABLE) != 0)
                    continue;
                /* if hiding non-autocomplete commands, skip such */
                if (menumode == 1 && (efp->flags & AUTOCOMPLETE) == 0)
                    continue;
                /* if searching, skip this command if it doesn't match */
                if (*searchbuf
                    /* first try case-insensitive substring match */
                    && !strstri(efp->ef_txt, searchbuf)
                    && !strstri(efp->ef_desc, searchbuf)
                    /* wildcard support; most interfaces use case-insensitve
                       pmatch rather than regexp for menu searching */
                    && !pmatchi(searchbuf, efp->ef_txt)
                    && !pmatchi(searchbuf, efp->ef_desc))
                    continue;
                /* skip wizard mode commands if not in wizard mode;
                   when showing two sections, skip wizard mode commands
                   in pass==0 and skip other commands in pass==1 */
                wizc = (efp->flags & WIZMODECMD) != 0;
                if (wizc && !wizard)
                    continue;
                if (!onelist && pass != wizc)
                    continue;

                /* We're about to show an item, have we shown the menu yet?
                   Doing menu in inner loop like this on demand avoids a
                   heading with no subordinate entries on the search
                   results menu. */
                if (!menushown[pass]) {
                    Strcpy(buf, headings[pass]);
                    add_menu(menuwin, NO_GLYPH, &any, 0, 0,
                             iflags.menu_headings, buf, MENU_UNSELECTED);
                    menushown[pass] = 1;
                }
                Sprintf(buf, " %-14s %-3s %s",
                        efp->ef_txt,
                        (efp->flags & AUTOCOMPLETE) ? "[A]" : " ",
                        efp->ef_desc);
                add_menu(menuwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                         buf, MENU_UNSELECTED);
                ++n;
            }
            if (n)
                add_menu(menuwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                         "", MENU_UNSELECTED);
        }
        if (*searchbuf && !n)
            add_menu(menuwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                     "没有匹配项", MENU_UNSELECTED);

        end_menu(menuwin, (char *) 0);
        n = select_menu(menuwin, PICK_ONE, &selected);
        if (n > 0) {
            switch (selected[0].item.a_int) {
            case 1: /* 'a': toggle show/hide non-autocomplete */
                menumode = 1 - menumode;  /* toggle 0 -> 1, 1 -> 0 */
                redisplay = TRUE;
                break;
            case 2: /* ':' when not searching yet: enable search */
                search = TRUE;
                break;
            case 3: /* 's' when already searching: disable search */
                search = FALSE;
                searchbuf[0] = '\0';
                redisplay = TRUE;
                break;
            case 4: /* 'z': toggle showing wizard mode commands separately */
                search = FALSE;
                searchbuf[0] = '\0';
                onelist = 1 - onelist;  /* toggle 0 -> 1, 1 -> 0 */
                redisplay = TRUE;
                break;
            }
            free((genericptr_t) selected);
        } else {
            search = FALSE;
            searchbuf[0] = '\0';
        }
        if (search) {
            Strcpy(promptbuf, "扩展命令列表搜索短语");
            Strcat(promptbuf, "?");
            getlin(promptbuf, searchbuf);
            (void) mungspaces(searchbuf);
            if (searchbuf[0] == '\033')
                searchbuf[0] = '\0';
            if (*searchbuf)
                redisplay = TRUE;
            search = FALSE;
        }
    }
    destroy_nhwindow(menuwin);
    return 0;
}

#if defined(TTY_GRAPHICS) || defined(CURSES_GRAPHICS)
#define MAX_EXT_CMD 200 /* Change if we ever have more ext cmds */

/*
 * This is currently used only by the tty interface and is
 * controlled via runtime option 'extmenu'.  (Most other interfaces
 * already use a menu all the time for extended commands.)
 *
 * ``# ?'' is counted towards the limit of the number of commands,
 * so we actually support MAX_EXT_CMD-1 "real" extended commands.
 *
 * Here after # - now show pick-list of possible commands.
 */
int
extcmd_via_menu()
{
    const struct ext_func_tab *efp;
    menu_item *pick_list = (menu_item *) 0;
    winid win;
    anything any;
    const struct ext_func_tab *choices[MAX_EXT_CMD + 1];
    char buf[BUFSZ];
    char cbuf[QBUFSZ], prompt[QBUFSZ], fmtstr[20];
    int i, n, nchoices, acount;
    int ret, len, biggest;
    int accelerator, prevaccelerator;
    int matchlevel = 0;
    boolean wastoolong, one_per_line;

    ret = 0;
    cbuf[0] = '\0';
    biggest = 0;
    while (!ret) {
        i = n = 0;
        any = zeroany;
        /* populate choices */
        for (efp = extcmdlist; efp->ef_txt; efp++) {
            if ((efp->flags & CMD_NOT_AVAILABLE)
                || !(efp->flags & AUTOCOMPLETE)
                || (!wizard && (efp->flags & WIZMODECMD)))
                continue;
            if (!matchlevel || !strncmp(efp->ef_txt, cbuf, matchlevel)) {
                choices[i] = efp;
                if ((len = (int) strlen(efp->ef_desc)) > biggest)
                    biggest = len;
                if (++i > MAX_EXT_CMD) {
#if (NH_DEVEL_STATUS != NH_STATUS_RELEASED)
                    impossible(
      "Exceeded %d extended commands in doextcmd() menu; 'extmenu' disabled.",
                               MAX_EXT_CMD);
#endif /* NH_DEVEL_STATUS != NH_STATUS_RELEASED */
                    iflags.extmenu = 0;
                    return -1;
                }
            }
        }
        choices[i] = (struct ext_func_tab *) 0;
        nchoices = i;
        /* if we're down to one, we have our selection so get out of here */
        if (nchoices  <= 1) {
            ret = (nchoices == 1) ? (int) (choices[0] - extcmdlist) : -1;
            break;
        }

        /* otherwise... */
        win = create_nhwindow(NHW_MENU);
        start_menu(win);
        Sprintf(fmtstr, "%%-%ds", biggest + 15);
        prompt[0] = '\0';
        wastoolong = FALSE; /* True => had to wrap due to line width
                             * ('w' in wizard mode) */
        /* -3: two line menu header, 1 line menu footer (for prompt) */
        one_per_line = (nchoices < ROWNO - 3);
        accelerator = prevaccelerator = 0;
        acount = 0;
        for (i = 0; choices[i]; ++i) {
            accelerator = choices[i]->ef_txt[matchlevel];
            if (accelerator != prevaccelerator || one_per_line)
                wastoolong = FALSE;
            if (accelerator != prevaccelerator || one_per_line
                || (acount >= 2
                    /* +4: + sizeof " or " - sizeof "" */
                    && (strlen(prompt) + 4 + strlen(choices[i]->ef_txt)
                        /* -6: enough room for 1 space left margin
                         *   + "%c - " menu selector + 1 space right margin */
                        >= min(sizeof prompt, COLNO - 6)))) {
                if (acount) {
                    /* flush extended cmds for that letter already in buf */
                    Sprintf(buf, fmtstr, prompt);
                    any.a_char = prevaccelerator;
                    add_menu(win, NO_GLYPH, &any, any.a_char, 0, ATR_NONE,
                             buf, FALSE);
                    acount = 0;
                    if (!(accelerator != prevaccelerator || one_per_line))
                        wastoolong = TRUE;
                }
            }
            prevaccelerator = accelerator;
            if (!acount || one_per_line) {
                Sprintf(prompt, "%s%s [%s]", wastoolong ? "or " : "",
                        choices[i]->ef_txt, choices[i]->ef_desc);
            } else if (acount == 1) {
                Sprintf(prompt, "%s%s or %s", wastoolong ? "or " : "",
                        choices[i - 1]->ef_txt, choices[i]->ef_txt);
            } else {
                Strcat(prompt, " or ");
                Strcat(prompt, choices[i]->ef_txt);
            }
            ++acount;
        }
        if (acount) {
            /* flush buf */
            Sprintf(buf, fmtstr, prompt);
            any.a_char = prevaccelerator;
            add_menu(win, NO_GLYPH, &any, any.a_char, 0, ATR_NONE, buf,
                     FALSE);
        }
        Sprintf(prompt, "Extended Command: %s", cbuf);
        end_menu(win, prompt);
        n = select_menu(win, PICK_ONE, &pick_list);
        destroy_nhwindow(win);
        if (n == 1) {
            if (matchlevel > (QBUFSZ - 2)) {
                free((genericptr_t) pick_list);
#if (NH_DEVEL_STATUS != NH_STATUS_RELEASED)
                impossible("Too many chars (%d) entered in extcmd_via_menu()",
                           matchlevel);
#endif
                ret = -1;
            } else {
                cbuf[matchlevel++] = pick_list[0].item.a_char;
                cbuf[matchlevel] = '\0';
                free((genericptr_t) pick_list);
            }
        } else {
            if (matchlevel) {
                ret = 0;
                matchlevel = 0;
            } else
                ret = -1;
        }
    }
    return ret;
}
#endif /* TTY_GRAPHICS */

/* #monster command - use special monster ability while polymorphed */
int
domonability(VOID_ARGS)
{
    if (can_breathe(youmonst.data))
        return dobreathe();
    else if (attacktype(youmonst.data, AT_SPIT))
        return dospit();
    else if (youmonst.data->mlet == S_NYMPH)
        return doremove();
    else if (attacktype(youmonst.data, AT_GAZE))
        return dogaze();
    else if (is_were(youmonst.data))
        return dosummon();
    else if (webmaker(youmonst.data))
        return dospinweb();
    else if (is_hider(youmonst.data))
        return dohide();
    else if (is_mind_flayer(youmonst.data))
        return domindblast();
    else if (u.umonnum == PM_GREMLIN) {
        if (IS_FOUNTAIN(levl[u.ux][u.uy].typ)) {
            if (split_mon(&youmonst, (struct monst *) 0))
                dryup(u.ux, u.uy, TRUE);
        } else
            There("没有喷泉.");
    } else if (is_unicorn(youmonst.data)) {
        use_unicorn_horn((struct obj *) 0);
        return 1;
    } else if (youmonst.data->msound == MS_SHRIEK) {
        You("尖叫.");
        if (u.uburied)
            pline("不幸的是声音没有很好地通过岩石.");
        else
            aggravate();
    } else if (youmonst.data->mlet == S_VAMPIRE)
        return dopoly();
    else if (Upolyd)
        pline("你可能有的特殊能力顶多是纯粹的倒挂.");
    else
        You("在正常的外貌中没有特殊的能力!");
    return 0;
}

int
enter_explore_mode(VOID_ARGS)
{
    if (wizard) {
        You("在向导模式中.");
    } else if (discover) {
        You("已经是在探索模式中.");
    } else {
#ifdef SYSCF
#if defined(UNIX)
        if (!sysopt.explorers || !sysopt.explorers[0]
            || !check_user_string(sysopt.explorers)) {
            You("cannot access explore mode.");
            return 0;
        }
#endif
#endif
        pline(
        "注意!  进入探索模式后就不能回到正常游戏.");
        if (paranoid_query(ParanoidQuit,
                           "你想进入探索模式吗?")) {
            clear_nhwindow(WIN_MESSAGE);
            You("现在是无分数探索模式.");
            discover = TRUE;
        } else {
            clear_nhwindow(WIN_MESSAGE);
            pline("回到正常游戏.");
        }
    }
    return 0;
}

/* ^W command - wish for something */
STATIC_PTR int
wiz_wish(VOID_ARGS) /* Unlimited wishes for debug mode by Paul Polderman */
{
    if (wizard) {
        boolean save_verbose = flags.verbose;

        flags.verbose = FALSE;
        makewish();
        flags.verbose = save_verbose;
        (void) encumber_msg();
    } else
        pline(unavailcmd, visctrl((int) cmd_from_func(wiz_wish)));
    return 0;
}

/* ^I command - reveal and optionally identify hero's inventory */
STATIC_PTR int
wiz_identify(VOID_ARGS)
{
    if (wizard) {
        iflags.override_ID = (int) cmd_from_func(wiz_identify);
        /* command remapping might leave #wizidentify as the only way
           to invoke us, in which case cmd_from_func() will yield NUL;
           it won't matter to display_inventory()/display_pickinv()
           if ^I invokes some other command--what matters is that
           display_pickinv() and xname() see override_ID as nonzero */
        if (!iflags.override_ID)
            iflags.override_ID = C('I');
        (void) display_inventory((char *) 0, FALSE);
        iflags.override_ID = 0;
    } else
        pline(unavailcmd, visctrl((int) cmd_from_func(wiz_identify)));
    return 0;
}

/* #wizmakemap - discard current dungeon level and replace with a new one */
STATIC_PTR int
wiz_makemap(VOID_ARGS)
{
    if (wizard) {
        struct monst *mtmp;
        boolean was_in_W_tower = In_W_tower(u.ux, u.uy, &u.uz);

        rm_mapseen(ledger_no(&u.uz));
        for (mtmp = fmon; mtmp; mtmp = mtmp->nmon) {
            if (mtmp->isgd) { /* vault is going away; get rid of guard */
                mtmp->isgd = 0;
                mongone(mtmp);
            }
            if (DEADMONSTER(mtmp))
                continue;
            if (mtmp->isshk)
                setpaid(mtmp);
            /* TODO?
             *  Reduce 'born' tally for each monster about to be discarded
             *  by savelev(), otherwise replacing heavily populated levels
             *  tends to make their inhabitants become extinct.
             */
        }
        if (Punished) {
            ballrelease(FALSE);
            unplacebc();
        }
        /* reset lock picking unless it's for a carried container */
        maybe_reset_pick((struct obj *) 0);
        /* reset interrupted digging if it was taking place on this level */
        if (on_level(&context.digging.level, &u.uz))
            (void) memset((genericptr_t) &context.digging, 0,
                          sizeof (struct dig_info));
        /* reset cached targets */
        iflags.travelcc.x = iflags.travelcc.y = 0; /* travel destination */
        context.polearm.hitmon = (struct monst *) 0; /* polearm target */
        /* escape from trap */
        reset_utrap(FALSE);
        check_special_room(TRUE); /* room exit */
        u.ustuck = (struct monst *) 0;
        u.uswallow = 0;
        u.uinwater = 0;
        u.uundetected = 0; /* not hidden, even if means are available */
        dmonsfree(); /* purge dead monsters from 'fmon' */
        /* keep steed and other adjacent pets after releasing them
           from traps, stopping eating, &c as if hero were ascending */
        keepdogs(TRUE); /* (pets-only; normally we'd be using 'FALSE' here) */

        /* discard current level; "saving" is used to release dynamic data */
        savelev(-1, ledger_no(&u.uz), FREE_SAVE);
        /* create a new level; various things like bestowing a guardian
           angel on Astral or setting off alarm on Ft.Ludios are handled
           by goto_level(do.c) so won't occur for replacement levels */
        mklev();

        vision_reset();
        vision_full_recalc = 1;
        cls();
        /* was using safe_teleds() but that doesn't honor arrival region
           on levels which have such; we don't force stairs, just area */
        u_on_rndspot((u.uhave.amulet ? 1 : 0) /* 'going up' flag */
                     | (was_in_W_tower ? 2 : 0));
        losedogs();
        kill_genocided_monsters();
        /* u_on_rndspot() might pick a spot that has a monster, or losedogs()
           might pick the hero's spot (only if there isn't already a monster
           there), so we might have to move hero or the co-located monster */
        if ((mtmp = m_at(u.ux, u.uy)) != 0)
            u_collide_m(mtmp);
        initrack();
        if (Punished) {
            unplacebc();
            placebc();
        }
        docrt();
        flush_screen(1);
        deliver_splev_message(); /* level entry */
        check_special_room(FALSE); /* room entry */
#ifdef INSURANCE
        save_currentstate();
#endif
    } else {
        pline(unavailcmd, "#wizmakemap");
    }
    return 0;
}

/* ^F command - reveal the level map and any traps on it */
STATIC_PTR int
wiz_map(VOID_ARGS)
{
    if (wizard) {
        struct trap *t;
        long save_Hconf = HConfusion, save_Hhallu = HHallucination;

        HConfusion = HHallucination = 0L;
        for (t = ftrap; t != 0; t = t->ntrap) {
            t->tseen = 1;
            map_trap(t, TRUE);
        }
        do_mapping();
        HConfusion = save_Hconf;
        HHallucination = save_Hhallu;
    } else
        pline(unavailcmd, visctrl((int) cmd_from_func(wiz_map)));
    return 0;
}

/* ^G command - generate monster(s); a count prefix will be honored */
STATIC_PTR int
wiz_genesis(VOID_ARGS)
{
    if (wizard)
        (void) create_particular();
    else
        pline(unavailcmd, visctrl((int) cmd_from_func(wiz_genesis)));
    return 0;
}

/* ^O command - display dungeon layout */
STATIC_PTR int
wiz_where(VOID_ARGS)
{
    if (wizard)
        (void) print_dungeon(FALSE, (schar *) 0, (xchar *) 0);
    else
        pline(unavailcmd, visctrl((int) cmd_from_func(wiz_where)));
    return 0;
}

/* ^E command - detect unseen (secret doors, traps, hidden monsters) */
STATIC_PTR int
wiz_detect(VOID_ARGS)
{
    if (wizard)
        (void) findit();
    else
        pline(unavailcmd, visctrl((int) cmd_from_func(wiz_detect)));
    return 0;
}

/* ^V command - level teleport */
STATIC_PTR int
wiz_level_tele(VOID_ARGS)
{
    if (wizard)
        level_tele();
    else
        pline(unavailcmd, visctrl((int) cmd_from_func(wiz_level_tele)));
    return 0;
}

/* #levelchange command - adjust hero's experience level */
STATIC_PTR int
wiz_level_change(VOID_ARGS)
{
    char buf[BUFSZ] = DUMMY;
    int newlevel = 0;
    int ret;

    getlin("你想设置为多少等级?", buf);
    (void) mungspaces(buf);
    if (buf[0] == '\033' || buf[0] == '\0')
        ret = 0;
    else
        ret = sscanf(buf, "%d", &newlevel);

    if (ret != 1) {
        pline1(Never_mind);
        return 0;
    }
    if (newlevel == u.ulevel) {
        You("已经是那个等级了.");
    } else if (newlevel < u.ulevel) {
        if (u.ulevel == 1) {
            You("已经达到最低等级.");
            return 0;
        }
        if (newlevel < 1)
            newlevel = 1;
        while (u.ulevel > newlevel)
            losexp("# 改变等级");
    } else {
        if (u.ulevel >= MAXULEV) {
            You("你已经达到最高等级.");
            return 0;
        }
        if (newlevel > MAXULEV)
            newlevel = MAXULEV;
        while (u.ulevel < newlevel)
            pluslvl(FALSE);
    }
    u.ulevelmax = u.ulevel;
    return 0;
}

/* #panic command - test program's panic handling */
STATIC_PTR int
wiz_panic(VOID_ARGS)
{
    if (iflags.debug_fuzzer) {
        u.uhp = u.uhpmax = 1000;
        u.uen = u.uenmax = 1000;
        return 0;
    }
    if (paranoid_query(ParanoidQuit,
                       "Do you want to call panic() and end your game?"))
        panic("Crash test.");
    return 0;
}

/* #polyself command - change hero's form */
STATIC_PTR int
wiz_polyself(VOID_ARGS)
{
    polyself(1);
    return 0;
}

/* #seenv command */
STATIC_PTR int
wiz_show_seenv(VOID_ARGS)
{
    winid win;
    int x, y, v, startx, stopx, curx;
    char row[COLNO + 1];

    win = create_nhwindow(NHW_TEXT);
    /*
     * Each seenv description takes up 2 characters, so center
     * the seenv display around the hero.
     */
    startx = max(1, u.ux - (COLNO / 4));
    stopx = min(startx + (COLNO / 2), COLNO);
    /* can't have a line exactly 80 chars long */
    if (stopx - startx == COLNO / 2)
        startx++;

    for (y = 0; y < ROWNO; y++) {
        for (x = startx, curx = 0; x < stopx; x++, curx += 2) {
            if (x == u.ux && y == u.uy) {
                row[curx] = row[curx + 1] = '@';
            } else {
                v = levl[x][y].seenv & 0xff;
                if (v == 0)
                    row[curx] = row[curx + 1] = ' ';
                else
                    Sprintf(&row[curx], "%02x", v);
            }
        }
        /* remove trailing spaces */
        for (x = curx - 1; x >= 0; x--)
            if (row[x] != ' ')
                break;
        row[x + 1] = '\0';

        putstr(win, 0, row);
    }
    display_nhwindow(win, TRUE);
    destroy_nhwindow(win);
    return 0;
}

/* #vision command */
STATIC_PTR int
wiz_show_vision(VOID_ARGS)
{
    winid win;
    int x, y, v;
    char row[COLNO + 1];

    win = create_nhwindow(NHW_TEXT);
    Sprintf(row, "Flags: 0x%x could see, 0x%x in sight, 0x%x temp lit",
            COULD_SEE, IN_SIGHT, TEMP_LIT);
    putstr(win, 0, row);
    putstr(win, 0, "");
    for (y = 0; y < ROWNO; y++) {
        for (x = 1; x < COLNO; x++) {
            if (x == u.ux && y == u.uy)
                row[x] = '@';
            else {
                v = viz_array[y][x]; /* data access should be hidden */
                if (v == 0)
                    row[x] = ' ';
                else
                    row[x] = '0' + viz_array[y][x];
            }
        }
        /* remove trailing spaces */
        for (x = COLNO - 1; x >= 1; x--)
            if (row[x] != ' ')
                break;
        row[x + 1] = '\0';

        putstr(win, 0, &row[1]);
    }
    display_nhwindow(win, TRUE);
    destroy_nhwindow(win);
    return 0;
}

/* #wmode command */
STATIC_PTR int
wiz_show_wmodes(VOID_ARGS)
{
    winid win;
    int x, y;
    char row[COLNO + 1];
    struct rm *lev;
    boolean istty = WINDOWPORT("tty");

    win = create_nhwindow(NHW_TEXT);
    if (istty)
        putstr(win, 0, ""); /* tty only: blank top line */
    for (y = 0; y < ROWNO; y++) {
        for (x = 0; x < COLNO; x++) {
            lev = &levl[x][y];
            if (x == u.ux && y == u.uy)
                row[x] = '@';
            else if (IS_WALL(lev->typ) || lev->typ == SDOOR)
                row[x] = '0' + (lev->wall_info & WM_MASK);
            else if (lev->typ == CORR)
                row[x] = '#';
            else if (IS_ROOM(lev->typ) || IS_DOOR(lev->typ))
                row[x] = '.';
            else
                row[x] = 'x';
        }
        row[COLNO] = '\0';
        /* map column 0, levl[0][], is off the left edge of the screen */
        putstr(win, 0, &row[1]);
    }
    display_nhwindow(win, TRUE);
    destroy_nhwindow(win);
    return 0;
}

/* wizard mode variant of #terrain; internal levl[][].typ values in base-36 */
STATIC_OVL void
wiz_map_levltyp(VOID_ARGS)
{
    winid win;
    int x, y, terrain;
    char row[COLNO + 1];
    boolean istty = !strcmp(windowprocs.name, "tty");

    win = create_nhwindow(NHW_TEXT);
    /* map row 0, levl[][0], is drawn on the second line of tty screen */
    if (istty)
        putstr(win, 0, ""); /* tty only: blank top line */
    for (y = 0; y < ROWNO; y++) {
        /* map column 0, levl[0][], is off the left edge of the screen;
           it should always have terrain type "undiggable stone" */
        for (x = 1; x < COLNO; x++) {
            terrain = levl[x][y].typ;
            /* assumes there aren't more than 10+26+26 terrain types */
            row[x - 1] = (char) ((terrain == STONE && !may_dig(x, y))
                                    ? '*'
                                    : (terrain < 10)
                                       ? '0' + terrain
                                       : (terrain < 36)
                                          ? 'a' + terrain - 10
                                          : 'A' + terrain - 36);
        }
        x--;
        if (levl[0][y].typ != STONE || may_dig(0, y))
            row[x++] = '!';
        row[x] = '\0';
        putstr(win, 0, row);
    }

    {
        char dsc[BUFSZ];
        s_level *slev = Is_special(&u.uz);

        Sprintf(dsc, "D:%d,L:%d", u.uz.dnum, u.uz.dlevel);
        /* [dungeon branch features currently omitted] */
        /* special level features */
        if (slev) {
            Sprintf(eos(dsc), " \"%s\"", slev->proto);
            /* special level flags (note: dungeon.def doesn't set `maze'
               or `hell' for any specific levels so those never show up) */
            if (slev->flags.maze_like)
                Strcat(dsc, " mazelike");
            if (slev->flags.hellish)
                Strcat(dsc, " hellish");
            if (slev->flags.town)
                Strcat(dsc, " town");
            if (slev->flags.rogue_like)
                Strcat(dsc, " roguelike");
            /* alignment currently omitted to save space */
        }
        /* level features */
        if (level.flags.nfountains)
            Sprintf(eos(dsc), " %c:%d", defsyms[S_fountain].sym,
                    (int) level.flags.nfountains);
        if (level.flags.nsinks)
            Sprintf(eos(dsc), " %c:%d", defsyms[S_sink].sym,
                    (int) level.flags.nsinks);
        if (level.flags.has_vault)
            Strcat(dsc, " vault");
        if (level.flags.has_shop)
            Strcat(dsc, " shop");
        if (level.flags.has_temple)
            Strcat(dsc, " temple");
        if (level.flags.has_court)
            Strcat(dsc, " throne");
        if (level.flags.has_zoo)
            Strcat(dsc, " zoo");
        if (level.flags.has_morgue)
            Strcat(dsc, " morgue");
        if (level.flags.has_barracks)
            Strcat(dsc, " barracks");
        if (level.flags.has_beehive)
            Strcat(dsc, " hive");
        if (level.flags.has_swamp)
            Strcat(dsc, " swamp");
        /* level flags */
        if (level.flags.noteleport)
            Strcat(dsc, " noTport");
        if (level.flags.hardfloor)
            Strcat(dsc, " noDig");
        if (level.flags.nommap)
            Strcat(dsc, " noMMap");
        if (!level.flags.hero_memory)
            Strcat(dsc, " noMem");
        if (level.flags.shortsighted)
            Strcat(dsc, " shortsight");
        if (level.flags.graveyard)
            Strcat(dsc, " graveyard");
        if (level.flags.is_maze_lev)
            Strcat(dsc, " maze");
        if (level.flags.is_cavernous_lev)
            Strcat(dsc, " cave");
        if (level.flags.arboreal)
            Strcat(dsc, " tree");
        if (Sokoban)
            Strcat(dsc, " sokoban-rules");
        /* non-flag info; probably should include dungeon branching
           checks (extra stairs and magic portals) here */
        if (Invocation_lev(&u.uz))
            Strcat(dsc, " invoke");
        if (On_W_tower_level(&u.uz))
            Strcat(dsc, " tower");
        /* append a branch identifier for completeness' sake */
        if (u.uz.dnum == 0)
            Strcat(dsc, " dungeon");
        else if (u.uz.dnum == mines_dnum)
            Strcat(dsc, " mines");
        else if (In_sokoban(&u.uz))
            Strcat(dsc, " sokoban");
        else if (u.uz.dnum == quest_dnum)
            Strcat(dsc, " quest");
        else if (Is_knox(&u.uz))
            Strcat(dsc, " ludios");
        else if (u.uz.dnum == 1)
            Strcat(dsc, " gehennom");
        else if (u.uz.dnum == tower_dnum)
            Strcat(dsc, " vlad");
        else if (In_endgame(&u.uz))
            Strcat(dsc, " endgame");
        else {
            /* somebody's added a dungeon branch we're not expecting */
            const char *brname = dungeons[u.uz.dnum].dname;

            if (!brname || !*brname)
                brname = "unknown";
            if (!strncmpi(brname, "the ", 4))
                brname += 4;
            Sprintf(eos(dsc), " %s", brname);
        }
        /* limit the line length to map width */
        if (strlen(dsc) >= COLNO)
            dsc[COLNO - 1] = '\0'; /* truncate */
        putstr(win, 0, dsc);
    }

    display_nhwindow(win, TRUE);
    destroy_nhwindow(win);
    return;
}

/* temporary? hack, since level type codes aren't the same as screen
   symbols and only the latter have easily accessible descriptions */
static const char *levltyp[] = {
    "stone", "vertical wall", "horizontal wall", "top-left corner wall",
    "top-right corner wall", "bottom-left corner wall",
    "bottom-right corner wall", "cross wall", "tee-up wall", "tee-down wall",
    "tee-left wall", "tee-right wall", "drawbridge wall", "tree",
    "secret door", "secret corridor", "pool", "moat", "water",
    "drawbridge up", "lava pool", "iron bars", "door", "corridor", "room",
    "stairs", "ladder", "fountain", "throne", "sink", "grave", "altar", "ice",
    "drawbridge down", "air", "cloud",
    /* not a real terrain type, but used for undiggable stone
       by wiz_map_levltyp() */
    "unreachable/undiggable",
    /* padding in case the number of entries above is odd */
    ""
};

/* explanation of base-36 output from wiz_map_levltyp() */
STATIC_OVL void
wiz_levltyp_legend(VOID_ARGS)
{
    winid win;
    int i, j, last, c;
    const char *dsc, *fmt;
    char buf[BUFSZ];

    win = create_nhwindow(NHW_TEXT);
    putstr(win, 0, "#terrain encodings:");
    putstr(win, 0, "");
    fmt = " %c - %-28s"; /* TODO: include tab-separated variant for win32 */
    *buf = '\0';
    /* output in pairs, left hand column holds [0],[1],...,[N/2-1]
       and right hand column holds [N/2],[N/2+1],...,[N-1];
       N ('last') will always be even, and may or may not include
       the empty string entry to pad out the final pair, depending
       upon how many other entries are present in levltyp[] */
    last = SIZE(levltyp) & ~1;
    for (i = 0; i < last / 2; ++i)
        for (j = i; j < last; j += last / 2) {
            dsc = levltyp[j];
            c = !*dsc ? ' '
                   : !strncmp(dsc, "unreachable", 11) ? '*'
                      /* same int-to-char conversion as wiz_map_levltyp() */
                      : (j < 10) ? '0' + j
                         : (j < 36) ? 'a' + j - 10
                            : 'A' + j - 36;
            Sprintf(eos(buf), fmt, c, dsc);
            if (j > i) {
                putstr(win, 0, buf);
                *buf = '\0';
            }
        }
    display_nhwindow(win, TRUE);
    destroy_nhwindow(win);
    return;
}

/* #wizsmell command - test usmellmon(). */
STATIC_PTR int
wiz_smell(VOID_ARGS)
{
    int ans = 0;
    int mndx;  /* monster index */
    coord cc;  /* screen pos of unknown glyph */
    int glyph; /* glyph at selected position */

    cc.x = u.ux;
    cc.y = u.uy;
    mndx = 0; /* gcc -Wall lint */
    if (!olfaction(youmonst.data)) {
        You("are incapable of detecting odors in your present form.");
        return 0;
    }

    pline("You can move the cursor to a monster that you want to smell.");
    do {
        pline("Pick a monster to smell.");
        ans = getpos(&cc, TRUE, "a monster");
        if (ans < 0 || cc.x < 0) {
            return 0; /* done */
        }
        /* Convert the glyph at the selected position to a mndxbol. */
        glyph = glyph_at(cc.x, cc.y);
        if (glyph_is_monster(glyph))
            mndx = glyph_to_mon(glyph);
        else
            mndx = 0;
        /* Is it a monster? */
        if (mndx) {
            if (!usmellmon(&mons[mndx]))
                pline("That monster seems to give off no smell.");
        } else
            pline("That is not a monster.");
    } while (TRUE);
    return 0;
}

/* #wizinstrinsic command to set some intrinsics for testing */
STATIC_PTR int
wiz_intrinsic(VOID_ARGS)
{
    if (wizard) {
        extern const struct propname {
            int prop_num;
            const char *prop_name;
        } propertynames[]; /* timeout.c */
        static const char wizintrinsic[] = "#wizintrinsic";
        static const char fmt[] = "You are%s %s.";
        winid win;
        anything any;
        char buf[BUFSZ];
        int i, j, n, p, amt, typ;
        long oldtimeout, newtimeout;
        const char *propname;
        menu_item *pick_list = (menu_item *) 0;

        any = zeroany;
        win = create_nhwindow(NHW_MENU);
        start_menu(win);
        for (i = 0; (propname = propertynames[i].prop_name) != 0; ++i) {
            p = propertynames[i].prop_num;
            if (p == HALLUC_RES) {
                /* Grayswandir vs hallucination; ought to be redone to
                   use u.uprops[HALLUC].blocked instead of being treated
                   as a separate property; letting in be manually toggled
                   even only in wizard mode would be asking for trouble... */
                continue;
            }
            if (p == FIRE_RES) {
                any.a_int = 0;
                add_menu(win, NO_GLYPH, &any, 0, 0, ATR_NONE, "--", FALSE);
            }
            any.a_int = i + 1; /* +1: avoid 0 */
            oldtimeout = u.uprops[p].intrinsic & TIMEOUT;
            if (oldtimeout)
                Sprintf(buf, "%-27s [%li]", propname, oldtimeout);
            else
                Sprintf(buf, "%s", propname);
            add_menu(win, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);
        }
        end_menu(win, "Which intrinsics?");
        n = select_menu(win, PICK_ANY, &pick_list);
        destroy_nhwindow(win);

        amt = 30; /* TODO: prompt for duration */
        for (j = 0; j < n; ++j) {
            i = pick_list[j].item.a_int - 1; /* -1: reverse +1 above */
            p = propertynames[i].prop_num;
            oldtimeout = u.uprops[p].intrinsic & TIMEOUT;
            newtimeout = oldtimeout + (long) amt;
            switch (p) {
            case SICK:
            case SLIMED:
            case STONED:
                if (oldtimeout > 0L && newtimeout > oldtimeout)
                    newtimeout = oldtimeout;
                break;
            }

            switch (p) {
            case BLINDED:
                make_blinded(newtimeout, TRUE);
                break;
#if 0       /* make_confused() only gives feedback when confusion is
             * ending so use the 'default' case for it instead */
            case CONFUSION:
                make_confused(newtimeout, TRUE);
                break;
#endif /*0*/
            case DEAF:
                make_deaf(newtimeout, TRUE);
                break;
            case HALLUC:
                make_hallucinated(newtimeout, TRUE, 0L);
                break;
            case SICK:
                typ = !rn2(2) ? SICK_VOMITABLE : SICK_NONVOMITABLE;
                make_sick(newtimeout, wizintrinsic, TRUE, typ);
                break;
            case SLIMED:
                Sprintf(buf, fmt,
                        !Slimed ? "" : " still", "turning into slime");
                make_slimed(newtimeout, buf);
                break;
            case STONED:
                Sprintf(buf, fmt,
                        !Stoned ? "" : " still", "turning into stone");
                make_stoned(newtimeout, buf, KILLED_BY, wizintrinsic);
                break;
            case STUNNED:
                make_stunned(newtimeout, TRUE);
                break;
            case VOMITING:
                Sprintf(buf, fmt, !Vomiting ? "" : " still", "vomiting");
                make_vomiting(newtimeout, FALSE);
                pline1(buf);
                break;
            case WARN_OF_MON:
                if (!Warn_of_mon) {
                    context.warntype.speciesidx = PM_GRID_BUG;
                    context.warntype.species
                                         = &mons[context.warntype.speciesidx];
                }
                goto def_feedback;
            case GLIB:
                /* slippery fingers applies to gloves if worn at the time
                   so persistent inventory might need updating */
                make_glib((int) newtimeout);
                goto def_feedback;
            case LEVITATION:
            case FLYING:
                float_vs_flight();
                /*FALLTHRU*/
            default:
 def_feedback:
                pline("Timeout for %s %s %d.", propertynames[i].prop_name,
                      oldtimeout ? "increased by" : "set to", amt);
                if (p != GLIB)
                    incr_itimeout(&u.uprops[p].intrinsic, amt);
                break;
            }
            context.botl = 1; /* probably not necessary... */
        }
        if (n >= 1)
            free((genericptr_t) pick_list);
        doredraw();
    } else
        pline(unavailcmd, visctrl((int) cmd_from_func(wiz_intrinsic)));
    return 0;
}

/* #wizrumorcheck command - verify each rumor access */
STATIC_PTR int
wiz_rumor_check(VOID_ARGS)
{
    rumor_check();
    return 0;
}

/* #terrain command -- show known map, inspired by crawl's '|' command */
STATIC_PTR int
doterrain(VOID_ARGS)
{
    winid men;
    menu_item *sel;
    anything any;
    int n;
    int which;

    /*
     * normal play: choose between known map without mons, obj, and traps
     *  (to see underlying terrain only), or
     *  known map without mons and objs (to see traps under mons and objs), or
     *  known map without mons (to see objects under monsters);
     * explore mode: normal choices plus full map (w/o mons, objs, traps);
     * wizard mode: normal and explore choices plus
     *  a dump of the internal levl[][].typ codes w/ level flags, or
     *  a legend for the levl[][].typ codes dump
     */
    men = create_nhwindow(NHW_MENU);
    start_menu(men);
    any = zeroany;
    any.a_int = 1;
    add_menu(men, NO_GLYPH, &any, 0, 0, ATR_NONE,
             "没有怪物, 物品, 和陷阱的已知地图",
             MENU_SELECTED);
    any.a_int = 2;
    add_menu(men, NO_GLYPH, &any, 0, 0, ATR_NONE,
             "没有怪物和物品的已知地图",
             MENU_UNSELECTED);
    any.a_int = 3;
    add_menu(men, NO_GLYPH, &any, 0, 0, ATR_NONE,
             "没有怪物的已知地图",
             MENU_UNSELECTED);
    if (discover || wizard) {
        any.a_int = 4;
        add_menu(men, NO_GLYPH, &any, 0, 0, ATR_NONE,
                 "没有怪物, 物品, 和陷阱的全地图",
                 MENU_UNSELECTED);
        if (wizard) {
            any.a_int = 5;
            add_menu(men, NO_GLYPH, &any, 0, 0, ATR_NONE,
                     "internal levl[][].typ codes in base-36",
                     MENU_UNSELECTED);
            any.a_int = 6;
            add_menu(men, NO_GLYPH, &any, 0, 0, ATR_NONE,
                     "legend of base-36 levl[][].typ codes",
                     MENU_UNSELECTED);
        }
    }
    end_menu(men, "观看哪个?");

    n = select_menu(men, PICK_ONE, &sel);
    destroy_nhwindow(men);
    /*
     * n <  0: player used ESC to cancel;
     * n == 0: preselected entry was explicitly chosen and got toggled off;
     * n == 1: preselected entry was implicitly chosen via <space>|<enter>;
     * n == 2: another entry was explicitly chosen, so skip preselected one.
     */
    which = (n < 0) ? -1 : (n == 0) ? 1 : sel[0].item.a_int;
    if (n > 1 && which == 1)
        which = sel[1].item.a_int;
    if (n > 0)
        free((genericptr_t) sel);

    switch (which) {
    case 1: /* known map */
        reveal_terrain(0, TER_MAP);
        break;
    case 2: /* known map with known traps */
        reveal_terrain(0, TER_MAP | TER_TRP);
        break;
    case 3: /* known map with known traps and objects */
        reveal_terrain(0, TER_MAP | TER_TRP | TER_OBJ);
        break;
    case 4: /* full map */
        reveal_terrain(1, TER_MAP);
        break;
    case 5: /* map internals */
        wiz_map_levltyp();
        break;
    case 6: /* internal details */
        wiz_levltyp_legend();
        break;
    default:
        break;
    }
    return 0; /* no time elapses */
}

/* -enlightenment and conduct- */
static winid en_win = WIN_ERR;
static boolean en_via_menu = FALSE;
static const char You_[] = "你", are[] = "是", were[] = "是",
                  have[] = "有", had[] = "有", can[] = "能",
                  could[] = "能";
static const char have_been[] = "一直是", have_never[] = "从来没有",
                  never[] = "没有";

#define enl_msg(prefix, present, past, suffix, ps) \
    enlght_line(prefix, final ? past : present, suffix, ps)
#define you_are(attr, ps) enl_msg(You_, "", "", attr, ps)
#define you_have(attr, ps) enl_msg(You_, have, had, attr, ps)
#define you_can(attr, ps) enl_msg(You_, can, could, attr, ps)
#define you_have_been(goodthing) enl_msg(You_, have_been, were, goodthing, "")
#define you_have_never(badthing) \
    enl_msg(You_, have_never, never, badthing, "")
#define you_have_X(something) \
    enl_msg(You_, have, (const char *) "", something, "")

static void
enlght_out(buf)
const char *buf;
{
    if (en_via_menu) {
        anything any;

        any = zeroany;
        add_menu(en_win, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);
    } else
        putstr(en_win, 0, buf);
}

static void
enlght_line(start, middle, end, ps)
const char *start, *middle, *end, *ps;
{
    char buf[BUFSZ];

    Sprintf(buf, "  %s%s%s%s.", start, middle, end, ps);
    enlght_out(buf);
}

/* format increased chance to hit or damage or defense (Protection) */
static char *
enlght_combatinc(inctyp, incamt, final, outbuf)
const char *inctyp;
int incamt, final;
char *outbuf;
{
    const char *modif, *bonus;
    boolean invrt;
    int absamt;

    absamt = abs(incamt);
    /* Protection amount is typically larger than damage or to-hit;
       reduce magnitude by a third in order to stretch modifier ranges
       (small:1..5, moderate:6..10, large:11..19, huge:20+) */
    if (!strcmp(inctyp, "防御"))
        absamt = (absamt * 2) / 3;

    if (absamt <= 3)
        modif = "很少的";
    else if (absamt <= 6)
        modif = "较少的";
    else if (absamt <= 12)
        modif = "较多的";
    else
        modif = "很多的";

    modif = !incamt ? "no" : modif; /* ("no" case shouldn't happen) */
    bonus = (incamt >= 0) ? "加成" : "处罚";
    /* "bonus <foo>" (to hit) vs "<bar> bonus" (damage, defense) */
    invrt = TRUE;

    Sprintf(outbuf, "%s%s%s", modif, invrt ? inctyp : bonus,
            invrt ? bonus : inctyp);
    if (final || wizard)
        Sprintf(eos(outbuf), " (%s%d)", (incamt > 0) ? "+" : "", incamt);

    return outbuf;
}

/* report half physical or half spell damage */
STATIC_OVL void
enlght_halfdmg(category, final)
int category;
int final;
{
    const char *category_name;
    char buf[BUFSZ];

    switch (category) {
    case HALF_PHDAM:
        category_name = "物理";
        break;
    case HALF_SPDAM:
        category_name = "魔法";
        break;
    default:
        category_name = "未知";
        break;
    }
    Sprintf(buf, "%s%s伤害", (final || wizard) ? "一半的" : "减少的",
            category_name);
    enl_msg(You_, "受到", "受到", buf, from_what(category));
}

/* is hero actively using water walking capability on water (or lava)? */
STATIC_OVL boolean
walking_on_water()
{
    if (u.uinwater || Levitation || Flying)
        return FALSE;
    return (boolean) (Wwalking
                      && (is_pool(u.ux, u.uy) || is_lava(u.ux, u.uy)));
}

/* check whether hero is wearing something that player definitely knows
   confers the target property; item must have been seen and its type
   discovered but it doesn't necessarily have to be fully identified */
STATIC_OVL boolean
cause_known(propindx)
int propindx; /* index of a property which can be conveyed by worn item */
{
    register struct obj *o;
    long mask = W_ARMOR | W_AMUL | W_RING | W_TOOL;

    /* simpler than from_what()/what_gives(); we don't attempt to
       handle artifacts and we deliberately ignore wielded items */
    for (o = invent; o; o = o->nobj) {
        if (!(o->owornmask & mask))
            continue;
        if ((int) objects[o->otyp].oc_oprop == propindx
            && objects[o->otyp].oc_name_known && o->dknown)
            return TRUE;
    }
    return FALSE;
}

/* format a characteristic value, accommodating Strength's strangeness */
STATIC_OVL char *
attrval(attrindx, attrvalue, resultbuf)
int attrindx, attrvalue;
char resultbuf[]; /* should be at least [7] to hold "18/100\0" */
{
    if (attrindx != A_STR || attrvalue <= 18)
        Sprintf(resultbuf, "%d", attrvalue);
    else if (attrvalue > STR18(100)) /* 19 to 25 */
        Sprintf(resultbuf, "%d", attrvalue - 100);
    else /* simplify "18/ **" to be "18/100" */
        Sprintf(resultbuf, "18/%02d", attrvalue - 18);
    return resultbuf;
}

void
enlightenment(mode, final)
int mode;  /* BASICENLIGHTENMENT | MAGICENLIGHTENMENT (| both) */
int final; /* ENL_GAMEINPROGRESS:0, ENL_GAMEOVERALIVE, ENL_GAMEOVERDEAD */
{
    char buf[BUFSZ], tmpbuf[BUFSZ];

    en_win = create_nhwindow(NHW_MENU);
    en_via_menu = !final;
    if (en_via_menu)
        start_menu(en_win);

    Strcpy(tmpbuf, plname);
    *tmpbuf = highc(*tmpbuf); /* same adjustment as bottom line */
    /* as in background_enlightenment, when poly'd we need to use the saved
       gender in u.mfemale rather than the current you-as-monster gender */
    Sprintf(buf, "%s  %s的属性:", tmpbuf,
            ((Upolyd ? u.mfemale : flags.female) && urole.name.f)
                ? urole.name.f
                : urole.name.m);

    /* title */
    enlght_out(buf); /* "Conan the Archeologist's attributes:" */
    /* background and characteristics; ^X or end-of-game disclosure */
    if (mode & BASICENLIGHTENMENT) {
        /* role, race, alignment, deities, dungeon level, time, experience */
        background_enlightenment(mode, final);
        /* hit points, energy points, armor class, gold */
        basics_enlightenment(mode, final);
        /* strength, dexterity, &c */
        characteristics_enlightenment(mode, final);
    }
    /* expanded status line information, including things which aren't
       included there due to space considerations--such as obvious
       alternative movement indicators (riding, levitation, &c), and
       various troubles (turning to stone, trapped, confusion, &c);
       shown for both basic and magic enlightenment */
    status_enlightenment(mode, final);
    /* remaining attributes; shown for potion,&c or wizard mode and
       explore mode ^X or end of game disclosure */
    if (mode & MAGICENLIGHTENMENT) {
        /* intrinsics and other traditional enlightenment feedback */
        attributes_enlightenment(mode, final);
    }

    if (!en_via_menu) {
        display_nhwindow(en_win, TRUE);
    } else {
        menu_item *selected = 0;

        end_menu(en_win, (char *) 0);
        if (select_menu(en_win, PICK_NONE, &selected) > 0)
            free((genericptr_t) selected);
        en_via_menu = FALSE;
    }
    destroy_nhwindow(en_win);
    en_win = WIN_ERR;
}

/*ARGSUSED*/
/* display role, race, alignment and such to en_win */
STATIC_OVL void
background_enlightenment(unused_mode, final)
int unused_mode UNUSED;
int final;
{
    const char *role_titl, *rank_titl;
    int innategend, difgend, difalgn;
    char buf[BUFSZ], tmpbuf[BUFSZ];

    /* note that if poly'd, we need to use u.mfemale instead of flags.female
       to access hero's saved gender-as-human/elf/&c rather than current one */
    innategend = (Upolyd ? u.mfemale : flags.female) ? 1 : 0;
    role_titl = (innategend && urole.name.f) ? urole.name.f : urole.name.m;
    rank_titl = rank_of(u.ulevel, Role_switch, innategend);

    enlght_out(""); /* separator after title */
    enlght_out("背景:");

    /* if polymorphed, report current shape before underlying role;
       will be repeated as first status: "you are transformed" and also
       among various attributes: "you are in beast form" (after being
       told about lycanthropy) or "you are polymorphed into <a foo>"
       (with countdown timer appended for wizard mode); we really want
       the player to know he's not a samurai at the moment... */
    if (Upolyd) {
        struct permonst *uasmon = youmonst.data;

        tmpbuf[0] = '\0';
        /* here we always use current gender, not saved role gender */
        if (!is_male(uasmon) && !is_female(uasmon) && !is_neuter(uasmon))
            Sprintf(tmpbuf, "%s ", genders[flags.female ? 1 : 0].adj);
        Sprintf(buf, "%s是%s%s的外貌", !final ? "当前" : "", tmpbuf,
                uasmon->mname);
        you_are(buf, "");
    }

    /* report role; omit gender if it's redundant (eg, "female priestess") */
    tmpbuf[0] = '\0';
    if (!urole.name.f
        && ((urole.allow & ROLE_GENDMASK) == (ROLE_MALE | ROLE_FEMALE)
            || innategend != flags.initgend))
        Sprintf(tmpbuf, "%s", genders[innategend].adj);
    buf[0] = '\0';
    if (Upolyd)
        Strcat(buf, "实际上"); /* "You are actually a ..." */
    if (!strcmpi(rank_titl, role_titl)) {
        /* omit role when rank title matches it */
        Sprintf(eos(buf), "是一位%s, 等级%d %s%s", rank_titl, u.ulevel,
                tmpbuf, urace.noun);
    } else {
        Sprintf(eos(buf), "是一位%s, 等级%d  %s%s%s", rank_titl, u.ulevel,
                tmpbuf, urace.adj, role_titl);
    }
    you_are(buf, "");

    /* report alignment (bypass you_are() in order to omit ending period);
       adverb is used to distinguish between temporary change (helm of opp.
       alignment), permanent change (one-time conversion), and original */
    Sprintf(buf, "  %s%s%s阵营, %s肩负着%s的使命",
            You_, !final ? are : were,
            align_str(u.ualign.type),
            /* helm of opposite alignment (might hide conversion) */
            (u.ualign.type != u.ualignbase[A_CURRENT])
               /* what's the past tense of "currently"? if we used "formerly"
                  it would sound like a reference to the original alignment */
               ? (!final ? "当前" : "临时")
               /* permanent conversion */
               : (u.ualign.type != u.ualignbase[A_ORIGINAL])
                  /* and what's the past tense of "now"? certainly not "then"
                     in a context like this...; "belatedly" == weren't that
                     way sooner (in other words, didn't start that way) */
                  ? (!final ? "现在" : "最近才")
                  /* atheist (ignored in very early game) */
                  : (!u.uconduct.gnostic && moves > 1000L)
                     ? "名义上"
                     /* lastly, normal case */
                     : "",
            u_gname());
    enlght_out(buf);
    /* show the rest of this game's pantheon (finishes previous sentence)
       [appending "also Moloch" at the end would allow for straightforward
       trailing "and" on all three aligned entries but looks too verbose] */
    Sprintf(buf, "  %s敌对的是", !final ? "" : "");
    if (u.ualign.type != A_LAWFUL)
        Sprintf(eos(buf), "%s(%s)和", align_gname(A_LAWFUL),
                align_str(A_LAWFUL));
    if (u.ualign.type != A_NEUTRAL)
        Sprintf(eos(buf), "%s(%s)%s", align_gname(A_NEUTRAL),
                align_str(A_NEUTRAL),
                (u.ualign.type != A_CHAOTIC) ? "和" : "");
    if (u.ualign.type != A_CHAOTIC)
        Sprintf(eos(buf), "%s(%s)", align_gname(A_CHAOTIC),
                align_str(A_CHAOTIC));
    Strcat(buf, "."); /* terminate sentence */
    enlght_out(buf);

    /* show original alignment,gender,race,role if any have been changed;
       giving separate message for temporary alignment change bypasses need
       for tricky phrasing otherwise necessitated by possibility of having
       helm of opposite alignment mask a permanent alignment conversion */
    difgend = (innategend != flags.initgend);
    difalgn = (((u.ualign.type != u.ualignbase[A_CURRENT]) ? 1 : 0)
               + ((u.ualignbase[A_CURRENT] != u.ualignbase[A_ORIGINAL])
                  ? 2 : 0));
    if (difalgn & 1) { /* have temporary alignment so report permanent one */
        Sprintf(buf, "实际上是%s阵营", align_str(u.ualignbase[A_CURRENT]));
        you_are(buf, "");
        difalgn &= ~1; /* suppress helm from "started out <foo>" message */
    }
    if (difgend || difalgn) { /* sex change or perm align change or both */
        Sprintf(buf, "  你最开始是%s%s%s.",
                difgend ? genders[flags.initgend].adj : "",
                (difgend && difalgn) ? "和" : "",
                difalgn ? align_str(u.ualignbase[A_ORIGINAL]) : "");
        enlght_out(buf);
    }

    /* As of 3.6.2: dungeon level, so that ^X really has all status info as
       claimed by the comment below; this reveals more information than
       the basic status display, but that's one of the purposes of ^X;
       similar information is revealed by #overview; the "You died in
       <location>" given by really_done() is more rudimentary than this */
    *buf = *tmpbuf = '\0';
    if (In_endgame(&u.uz)) {
        int egdepth = observable_depth(&u.uz);

        (void) endgamelevelname(tmpbuf, egdepth);
        Sprintf(buf, "在终局, %s%s",
                (egdepth!=-5) ? "元素" : "", tmpbuf);
    } else if (Is_knox(&u.uz)) {
        /* this gives away the fact that the knox branch is only 1 level */
        Sprintf(buf, "在%s层", dungeons[u.uz.dnum].dname);
        /* TODO? maybe phrase it differently when actually inside the fort,
           if we're able to determine that (not trivial) */
    } else {
        char dgnbuf[QBUFSZ];

        Strcpy(dgnbuf, dungeons[u.uz.dnum].dname);
        if (!strncmpi(dgnbuf, "The ", 4))
            *dgnbuf = lowc(*dgnbuf);
        Sprintf(tmpbuf, "%d层",
                In_quest(&u.uz) ? dunlev(&u.uz) : depth(&u.uz));
        /* TODO? maybe extend this bit to include various other automatic
           annotations from the dungeon overview code */
        if (Is_rogue_level(&u.uz))
            Strcat(tmpbuf, ", 一个原始的区域");
        else if (Is_bigroom(&u.uz) && !Blind)
            Strcat(tmpbuf, ", 一个非常大的房间");
        Sprintf(buf, "在%s, %s", dgnbuf, tmpbuf);
    }
    you_are(buf, "");

    /* this is shown even if the 'time' option is off */
    if (moves == 1L) {
        you_are("才刚刚开始了你的冒险", "");
    } else {
        /* 'turns' grates on the nerves in this context... */
        Sprintf(buf, "%ld回合之前进入的地牢", moves);
        /* same phrasing for current and final: "entered" is unconditional */
        enlght_line(You_, "是", buf, "");
    }

    /* for gameover, these have been obtained in really_done() so that they
       won't vary if user leaves a disclosure prompt or --More-- unanswered
       long enough for the dynamic value to change between then and now */
    if (final ? iflags.at_midnight : midnight()) {
        enl_msg("", "现在是", "那是", "午夜时分", "");
    } else if (final ? iflags.at_night : night()) {
        enl_msg("", "现在是", "那是", "晚上", "");
    }
    /* other environmental factors */
    if (flags.moonphase == FULL_MOON || flags.moonphase == NEW_MOON) {
        /* [This had "tonight" but has been changed to "in effect".
           There is a similar issue to Friday the 13th--it's the value
           at the start of the current session but that session might
           have dragged on for an arbitrary amount of time.  We want to
           report the values that currently affect play--or affected
           play when game ended--rather than actual outside situation.] */
        Sprintf(buf, "%s有%s月效果",
                final ? "在你的冒险结束时" : "",
                (flags.moonphase == FULL_MOON) ? "满"
                : (flags.moonphase == NEW_MOON) ? "新"
                  /* showing these would probably just lead to confusion
                     since they have no effect on game play... */
                  : (flags.moonphase < FULL_MOON) ? "上弦"
                    : "下弦"
                /* we don't have access to 'how' here--aside from survived
                   vs died--so settle for general platitude */
                );
        enl_msg("", "现在", "", buf, "");
    }
    if (flags.friday13) {
        /* let player know that friday13 penalty is/was in effect;
           we don't say "it is/was Friday the 13th" because that was at
           the start of the session and it might be past midnight (or
           days later if the game has been paused without save/restore),
           so phrase this similar to the start up message */
        Sprintf(buf, "  坏的事情%s13号星期五.",
                !final ? "会发生在"
                : (final == ENL_GAMEOVERALIVE) ? "会发生在"
                  /* there's no may to tell whether -1 Luck made a
                     difference but hero has died... */
                  : "发生在");
        enlght_out(buf);
    }

    if (!Upolyd) {
        int ulvl = (int) u.ulevel;
        /* [flags.showexp currently does not matter; should it?] */

        /* experience level is already shown above */
        Sprintf(buf, "%-1ld经验值", u.uexp);
        /* TODO?
         *  Remove wizard-mode restriction since patient players can
         *  determine the numbers needed without resorting to spoilers
         *  (even before this started being disclosed for 'final';
         *  just enable 'showexp' and look at normal status lines
         *  after drinking gain level potions or eating wraith corpses
         *  or being level-drained by vampires).
         */
        if (ulvl < 30 && (final || wizard)) {
            long nxtlvl = newuexp(ulvl), delta = nxtlvl - u.uexp;

            Sprintf(eos(buf), ", %s需要%ld点%s%s等级%d",
                    (u.uexp > 0) ? "还" : "", delta,
                    /* present tense=="needed", past tense=="were needed" */
                    !final ? "" : (delta == 1L) ? "" : "",
                    /* "for": grammatically iffy but less likely to wrap */
                    (ulvl < 18) ? "来升到" : "到", (ulvl + 1));
        }
        you_have(buf, "");
    }
#ifdef SCORE_ON_BOTL
    if (flags.showscore) {
        /* describes what's shown on status line, which is an approximation;
           only show it here if player has the 'showscore' option enabled */
        Sprintf(buf, "%ld%s", botl_score(),
                !final ? "" : "在游戏结束调整前");
        enl_msg("你的分数", "是", "是", buf, "");
    }
#endif
}

/* hit points, energy points, armor class -- essential information which
   doesn't fit very well in other categories */
/*ARGSUSED*/
STATIC_OVL void
basics_enlightenment(mode, final)
int mode UNUSED;
int final;
{
    static char Power[] = "Pw";
    char buf[BUFSZ];
    int pw = u.uen, hp = (Upolyd ? u.mh : u.uhp),
        pwmax = u.uenmax, hpmax = (Upolyd ? u.mhmax : u.uhpmax);

    enlght_out(""); /* separator after background */
    enlght_out("基础:");

    if (hp < 0)
        hp = 0;
    /* "1 out of 1" rather than "all" if max is only 1; should never happen */
    if (hp == hpmax && hpmax > 1)
        Sprintf(buf, "%d HP", hpmax);
    else
        Sprintf(buf, "%d HP, 上限%d HP", hp, hpmax);
    you_have(buf, "");

    /* low max energy is feasible, so handle couple of extra special cases */
    if (pwmax == 0 || (pw == pwmax && pwmax == 2)) { /* both: "all 2" is silly */
        Sprintf(buf, "%s %s", !pwmax ? "没有" : "有2", Power);
        you_are(buf, "");
    }
    else {
        if (pw == pwmax && pwmax > 2)
            Sprintf(buf, "%d %s", pwmax, Power);
        else
            Sprintf(buf, "%d %s, 上限%d %s", pw, Power, pwmax, Power);
        you_have(buf, "");
    }

    if (Upolyd) {
        switch (mons[u.umonnum].mlevel) {
        case 0:
            /* status line currently being explained shows "HD:0" */
            Strcpy(buf, "0 生命骰 (实际1/2)");
            break;
        case 1:
            Strcpy(buf, "1 生命骰");
            break;
        default:
            Sprintf(buf, "%d 生命骰", mons[u.umonnum].mlevel);
            break;
        }
        you_have(buf, "");
    }

    Sprintf(buf, "%d", u.uac);
    enl_msg("你的AC", "是", "是", buf, "");

    /* gold; similar to doprgold(#seegold) but without shop billing info;
       same amount as shown on status line which ignores container contents */
    {
        static const char Your_wallet[] = "你的钱包";
        long umoney = money_cnt(invent);

        if (!umoney) {
            enl_msg(Your_wallet, "是", "是", "空的", "");
        } else {
            Sprintf(buf, "%ld %s", umoney, currency(umoney));
            enl_msg(Your_wallet, "有", "有", buf, "");
        }
    }

    if (flags.pickup) {
        char ocl[MAXOCLASSES + 1];

        Strcpy(buf, "开着");
        oc_to_str(flags.pickup_types, ocl);
        Sprintf(eos(buf), ",拾取%s%s%s",
                *ocl ? "'" : "", *ocl ? ocl : "所有类型", *ocl ? "'" : "");
        if (flags.pickup_thrown && *ocl) /* *ocl: don't show if 'all types' */
            Strcat(buf, "以及投掷");
        if (apelist)
            Strcat(buf, ", 除了例外");
    } else
        Strcpy(buf, "关着");
    enl_msg("自动拾取", "是", "是", buf, "");
}

/* characteristics: expanded version of bottom line strength, dexterity, &c */
STATIC_OVL void
characteristics_enlightenment(mode, final)
int mode;
int final;
{
    char buf[BUFSZ];

    enlght_out("");
    Sprintf(buf, "%s品质:", !final ? "当前" : "最终");
    enlght_out(buf);

    /* bottom line order */
    one_characteristic(mode, final, A_STR); /* strength */
    one_characteristic(mode, final, A_DEX); /* dexterity */
    one_characteristic(mode, final, A_CON); /* constitution */
    one_characteristic(mode, final, A_INT); /* intelligence */
    one_characteristic(mode, final, A_WIS); /* wisdom */
    one_characteristic(mode, final, A_CHA); /* charisma */
}

/* display one attribute value for characteristics_enlightenment() */
STATIC_OVL void
one_characteristic(mode, final, attrindx)
int mode, final, attrindx;
{
    extern const char *const attrname[]; /* attrib.c */
    boolean hide_innate_value = FALSE, interesting_alimit;
    int acurrent, abase, apeak, alimit;
    const char *paren_pfx;
    char subjbuf[BUFSZ], valubuf[BUFSZ], valstring[32];

    /* being polymorphed or wearing certain cursed items prevents
       hero from reliably tracking changes to characteristics so
       we don't show base & peak values then; when the items aren't
       cursed, hero could take them off to check underlying values
       and we show those in such case so that player doesn't need
       to actually resort to doing that */
    if (Upolyd) {
        hide_innate_value = TRUE;
    } else if (Fixed_abil) {
        if (stuck_ring(uleft, RIN_SUSTAIN_ABILITY)
            || stuck_ring(uright, RIN_SUSTAIN_ABILITY))
            hide_innate_value = TRUE;
    }
    switch (attrindx) {
    case A_STR:
        if (uarmg && uarmg->otyp == GAUNTLETS_OF_POWER && uarmg->cursed)
            hide_innate_value = TRUE;
        break;
    case A_DEX:
        break;
    case A_CON:
        if (uwep && uwep->oartifact == ART_OGRESMASHER && uwep->cursed)
            hide_innate_value = TRUE;
        break;
    case A_INT:
        if (uarmh && uarmh->otyp == DUNCE_CAP && uarmh->cursed)
            hide_innate_value = TRUE;
        break;
    case A_WIS:
        if (uarmh && uarmh->otyp == DUNCE_CAP && uarmh->cursed)
            hide_innate_value = TRUE;
        break;
    case A_CHA:
        break;
    default:
        return; /* impossible */
    };
    /* note: final disclosure includes MAGICENLIGHTENTMENT */
    if ((mode & MAGICENLIGHTENMENT) && !Upolyd)
        hide_innate_value = FALSE;

    acurrent = ACURR(attrindx);
    (void) attrval(attrindx, acurrent, valubuf); /* Sprintf(valubuf,"%d",) */
    Sprintf(subjbuf, "你的%s", attrname[attrindx]);

    if (!hide_innate_value) {
        /* show abase, amax, and/or attrmax if acurr doesn't match abase
           (a magic bonus or penalty is in effect) or abase doesn't match
           amax (some points have been lost to poison or exercise abuse
           and are restorable) or attrmax is different from normal human
           (while game is in progress; trying to reduce dependency on
           spoilers to keep track of such stuff) or attrmax was different
           from abase (at end of game; this attribute wasn't maxed out) */
        abase = ABASE(attrindx);
        apeak = AMAX(attrindx);
        alimit = ATTRMAX(attrindx);
        /* criterium for whether the limit is interesting varies */
        interesting_alimit =
            final ? TRUE /* was originally `(abase != alimit)' */
                  : (alimit != (attrindx != A_STR ? 18 : STR18(100)));
        paren_pfx = final ? " (" : " (当前; ";
        if (acurrent != abase) {
            Sprintf(eos(valubuf), "%s 基础:%s", paren_pfx,
                    attrval(attrindx, abase, valstring));
            paren_pfx = ", ";
        }
        if (abase != apeak) {
            Sprintf(eos(valubuf), "%s 最高:%s", paren_pfx,
                    attrval(attrindx, apeak, valstring));
            paren_pfx = ", ";
        }
        if (interesting_alimit) {
            Sprintf(eos(valubuf), "%s%s 极限:%s", paren_pfx,
                    /* more verbose if exceeding 'limit' due to magic bonus */
                    (acurrent > alimit) ? "天生" : "",
                    attrval(attrindx, alimit, valstring));
            /* paren_pfx = ", "; */
        }
        if (acurrent != abase || abase != apeak || interesting_alimit)
            Strcat(valubuf, ")");
    }
    enl_msg(subjbuf, "为 ", "为 ", valubuf, "");
}

/* status: selected obvious capabilities, assorted troubles */
STATIC_OVL void
status_enlightenment(mode, final)
int mode;
int final;
{
    boolean magic = (mode & MAGICENLIGHTENMENT) ? TRUE : FALSE;
    int cap, wtype;
    char buf[BUFSZ], youtoo[BUFSZ];
    boolean Riding = (u.usteed
                      /* if hero dies while dismounting, u.usteed will still
                         be set; we want to ignore steed in that situation */
                      && !(final == ENL_GAMEOVERDEAD
                           && !strcmp(killer.name, "乘骑事故")));
    const char *steedname = (!Riding ? (char *) 0
                      : x_monnam(u.usteed,
                                 u.usteed->mtame ? ARTICLE_YOUR : ARTICLE_THE,
                                 (char *) 0,
                                 (SUPPRESS_SADDLE | SUPPRESS_HALLUCINATION),
                                 FALSE));

    /*\
     * Status (many are abbreviated on bottom line; others are or
     *     should be discernible to the hero hence to the player)
    \*/
    enlght_out(""); /* separator after title or characteristics */
    enlght_out(final ? "最终状态:" : "当前状态:");

    Strcpy(youtoo, You_);
    /* not a traditional status but inherently obvious to player; more
       detail given below (attributes section) for magic enlightenment */
    if (Upolyd) {
        Strcpy(buf, "变形了");
        if (ugenocided())
            Sprintf(eos(buf), "且%s内心%s",
                    final ? "感觉" : "感觉", udeadinside());
        you_are(buf, "");
    }
    /* not a trouble, but we want to display riding status before maybe
       reporting steed as trapped or hero stuck to cursed saddle */
    if (Riding) {
        Sprintf(buf, "骑着%s", steedname);
        you_are(buf, "");
        Sprintf(eos(youtoo), "和%s", steedname);
    }
    /* other movement situations that hero should always know */
    if (Levitation) {
        if (Lev_at_will && magic)
            you_are("飘浮着,  随意", "");
        else
            enl_msg(youtoo, "", "", "飘浮着", from_what(LEVITATION));
    } else if (Flying) { /* can only fly when not levitating */
        enl_msg(youtoo, are, were, "飞着", from_what(FLYING));
    }
    if (Underwater) {
        you_are("在水下", "");
    } else if (u.uinwater) {
        you_are(Swimming ? "在游泳" : "在水中", from_what(SWIMMING));
    } else if (walking_on_water()) {
        /* show active Wwalking here, potential Wwalking elsewhere */
        Sprintf(buf, "走在%s上",
                is_pool(u.ux, u.uy) ? "水面"
                : is_lava(u.ux, u.uy) ? "熔岩"
                  : surface(u.ux, u.uy)); /* catchall; shouldn't happen */
        you_are(buf, from_what(WWALKING));
    }
    if (Upolyd && (u.uundetected || U_AP_TYPE != M_AP_NOTHING))
        youhiding(TRUE, final);

    /* internal troubles, mostly in the order that prayer ranks them */
    if (Stoned) {
        if (final && (Stoned & I_SPECIAL))
            enlght_out("  你变成了石头.");
        else
            you_are("正在石化", "");
    }
    if (Slimed) {
        if (final && (Slimed & I_SPECIAL))
            enlght_out("  你变成了黏液.");
        else
            you_are("正在污秽化", "");
    }
    if (Strangled) {
        if (u.uburied) {
            you_are("被埋葬", "");
        } else {
            if (final && (Strangled & I_SPECIAL)) {
                enlght_out("  你死于窒息.");
            } else {
                Strcpy(buf, "被窒息");
                if (wizard)
                    Sprintf(eos(buf), " (%ld)", (Strangled & TIMEOUT));
                you_are(buf, from_what(STRANGLED));
            }
        }
    }
    if (Sick) {
        /* the two types of sickness are lumped together; hero can be
           afflicted by both but there is only one timeout; botl status
           puts TermIll before FoodPois and death due to timeout reports
           terminal illness if both are in effect, so do the same here */
        if (final && (Sick & I_SPECIAL)) {
            Sprintf(buf, "  %s死于%s.", You_, /* has trailing space */
                    (u.usick_type & SICK_NONVOMITABLE)
                    ? "疾病晚期" : "食物中毒");
            enlght_out(buf);
        } else {
            /* unlike death due to sickness, report the two cases separately
               because it is possible to cure one without curing the other */
            if (u.usick_type & SICK_NONVOMITABLE)
                you_are("已经病入膏肓了", "");
            if (u.usick_type & SICK_VOMITABLE)
                you_are("因食物中毒而病入膏肓了", "");
        }
    }
    if (Vomiting)
        you_are("作呕", "");
    if (Stunned)
        you_are("眩晕了", "");
    if (Confusion)
        you_are("混乱了", "");
    if (Hallucination)
        you_are("出现幻觉", "");
    if (Blind) {
        /* from_what() (currently wizard-mode only) checks !haseyes()
           before u.uroleplay.blind, so we should too */
        Sprintf(buf, "%s失明",
                !haseyes(youmonst.data) ? "天生"
                : u.uroleplay.blind ? "永久"
                  /* better phrasing desperately wanted... */
                  : Blindfolded_only ? "故意"
                    : "暂时");
        if (wizard && (Blinded & TIMEOUT) != 0L
            && !u.uroleplay.blind && haseyes(youmonst.data))
            Sprintf(eos(buf), " (%ld)", (Blinded & TIMEOUT));
        /* !haseyes: avoid "you are innately blind innately" */
        you_are(buf, !haseyes(youmonst.data) ? "" : from_what(BLINDED));
    }
    if (Deaf)
        you_are("是聋的", from_what(DEAF));

    /* external troubles, more or less */
    if (Punished) {
        if (uball) {
            Sprintf(buf, "被拴着%s", simpleonames(uball));
        } else {
            impossible("Punished without uball?");
            Strcpy(buf, "punished");
        }
        you_are(buf, "");
    }
    if (u.utrap) {
        char predicament[BUFSZ];
        struct trap *t;
        boolean anchored = (u.utraptype == TT_BURIEDBALL);

        if (anchored) {
            Strcpy(predicament, "拴着埋着的什么东西");
        } else if (u.utraptype == TT_INFLOOR || u.utraptype == TT_LAVA) {
            Sprintf(predicament, "陷在%s", the(surface(u.ux, u.uy)));
        } else {
            Strcpy(predicament, "被困");
            if ((t = t_at(u.ux, u.uy)) != 0)
                Sprintf(eos(predicament), "在%s",
                        defsyms[trap_to_defsym(t->ttyp)].explanation);
        }
        if (u.usteed) { /* not `Riding' here */
            Sprintf(buf, "%s%s ", anchored ? "你和" : "", steedname);
            *buf = highc(*buf);
            enl_msg(buf, (anchored ? "" : ""),
                    (anchored ? "" : ""), predicament, "");
        } else
            you_are(predicament, "");
    } /* (u.utrap) */
    if (u.uswallow) {
        Sprintf(buf, "被%s吞食了", a_monnam(u.ustuck));
        if (wizard)
            Sprintf(eos(buf), " (%u)", u.uswldtim);
        you_are(buf, "");
    } else if (u.ustuck) {
        Sprintf(buf, "%s %s",
                (Upolyd && sticks(youmonst.data)) ? "牵制着" : "受牵制于",
                a_monnam(u.ustuck));
        you_are(buf, "");
    }
    if (Riding) {
        struct obj *saddle = which_armor(u.usteed, W_SADDLE);

        if (saddle && saddle->cursed) {
            Sprintf(buf, "被粘在%s%s", s_suffix(steedname),
                    simpleonames(saddle));
            you_are(buf, "");
        }
    }
    if (Wounded_legs) {
        /* when mounted, Wounded_legs applies to steed rather than to
           hero; we only report steed's wounded legs in wizard mode */
        if (u.usteed) { /* not `Riding' here */
            if (wizard && steedname) {
                Strcpy(buf, steedname);
                *buf = highc(*buf);
                enl_msg(buf, "有", "有", "受伤的腿", "");
            }
        } else {
            Sprintf(buf, "受伤的%s", makeplural(body_part(LEG)));
            you_have(buf, "");
        }
    }
    if (Glib) {
        Sprintf(buf, "光滑的%s", fingers_or_gloves(TRUE));
        if (wizard)
            Sprintf(eos(buf), " (%ld)", (Glib & TIMEOUT));
        you_have(buf, "");
    }
    if (Fumbling) {
        if (magic || cause_known(FUMBLING))
            enl_msg(You_, "很笨拙", "很笨拙", "", from_what(FUMBLING));
    }
    if (Sleepy) {
        if (magic || cause_known(SLEEPY)) {
            Strcpy(buf, from_what(SLEEPY));
            if (wizard)
                Sprintf(eos(buf), " (%ld)", (HSleepy & TIMEOUT));
            enl_msg("你", "陷入了", "陷入了", "沉睡", buf);
        }
    }
    /* hunger/nutrition */
    if (Hunger) {
        if (magic || cause_known(HUNGER))
            enl_msg(You_, "饿", "饿", "得很快",
                    from_what(HUNGER));
    }
    Strcpy(buf, hu_stat[u.uhs]); /* hunger status; omitted if "normal" */
    mungspaces(buf);             /* strip trailing spaces */
    if (*buf) {
        *buf = lowc(*buf); /* override capitalization */
        if (!strcmp(buf, "虚弱"))
            Strcat(buf, "因严重饥饿");
        else if (!cnstrcmp(buf, "昏厥") || !cnstrcmp(buf, "晕厥")) /* fainting, fainted */
            Strcat(buf, "因过度饥饿");
        you_are(buf, "");
    }
    /* encumbrance */
    if ((cap = near_capacity()) > UNENCUMBERED) {
        const char *adj = "?_?"; /* (should always get overridden) */

        Strcpy(buf, enc_stat[cap]);
        *buf = lowc(*buf);
        switch (cap) {
        case SLT_ENCUMBER:
            adj = "轻微";
            break; /* burdened */
        case MOD_ENCUMBER:
            adj = "有些";
            break; /* stressed */
        case HVY_ENCUMBER:
            adj = "非常";
            break; /* strained */
        case EXT_ENCUMBER:
            adj = "极其";
            break; /* overtaxed */
        case OVERLOADED:
            adj = "not possible";
            break;
        }
        Sprintf(eos(buf), "; 移动速度%s%s",  adj,
                (cap < OVERLOADED) ? "慢" : "");
        you_have(buf, "");
    } else {
        /* last resort entry, guarantees Status section is non-empty
           (no longer needed for that purpose since weapon status added;
           still useful though) */
        you_are("无负担", "");
    }

    /* report being weaponless; distinguish whether gloves are worn */
    if (!uwep) {
        you_are(uarmg ? "空手" /* gloves imply hands */
                      : humanoid(youmonst.data)
                         /* hands but no weapon and no gloves */
                         ? "赤着手"
                         /* alternate phrasing for paws or lack of hands */
                         : "没有拿着任何东西",
                "");
    /* two-weaponing implies hands (can't be polymorphed) and
       a weapon or wep-tool (not other odd stuff) in each hand */
    } else if (u.twoweap) {
        you_are("同时使用两把武器", "");
    /* report most weapons by their skill class (so a katana will be
       described as a long sword, for instance; mattock and hook are
       exceptions), or wielded non-weapon item by its object class */
    } else {
        const char *what = weapon_descr(uwep);

        Sprintf(buf, "拿着%s", what);
        you_are(buf, "");
    }
    /*
     * Skill with current weapon.  Might help players who've never
     * noticed #enhance or decided that it was pointless.
     *
     * TODO?  Maybe merge wielding line and skill line into one sentence.
     */
    if ((wtype = uwep_skill_type()) != P_NONE) {
        char sklvlbuf[20];
        int sklvl = P_SKILL(wtype);
        boolean hav = (sklvl != P_UNSKILLED && sklvl != P_SKILLED);

        if (sklvl == P_ISRESTRICTED)
            Strcpy(sklvlbuf, "没有的");
        else
            (void) lcase(skill_level_name(wtype, sklvlbuf));
        /* "you have no/basic/expert/master/grand-master skill with <skill>"
           or "you are unskilled/skilled in <skill>" */
        Sprintf(buf, "在%s方面%s是%s", skill_name(wtype),
                hav ? "的技能" : "", sklvlbuf);
        if (can_advance(wtype, FALSE))
            Sprintf(eos(buf), ", 而且%s",
                    !final ? "可以提升" : "本可以提升");
        if (hav)
            you_are(buf, "");
        else
            you_are(buf, "");
    }
    /* report 'nudity' */
    if (!uarm && !uarmu && !uarmc && !uarms && !uarmg && !uarmf && !uarmh) {
        if (u.uroleplay.nudist)
            enl_msg(You_, "", "", " 没有穿任何防具", "");
        else
            you_are("没有穿任何防具", "");
    }
}

/* attributes: intrinsics and the like, other non-obvious capabilities */
STATIC_OVL void
attributes_enlightenment(unused_mode, final)
int unused_mode UNUSED;
int final;
{
    static NEARDATA const char if_surroundings_permitted[] =
        "如果环境允许的话";
    int ltmp, armpro;
    char buf[BUFSZ];

    /*\
     *  Attributes
    \*/
    enlght_out("");
    enlght_out(final ? "最终属性:" : "当前属性:");

    if (u.uevent.uhand_of_elbereth) {
        static const char *const hofe_titles[3] = { "是伊尔碧绿丝之手",
                                                    "是平衡使者",
                                                    "是亚略的荣耀" };
        you_are(hofe_titles[u.uevent.uhand_of_elbereth - 1], "");
    }

    Sprintf(buf, "%s", piousness(TRUE, "信仰"));
    if (u.ualign.record >= 0)
        you_are(buf, "");
    else
        you_have(buf, "");

    if (wizard) {
        Sprintf(buf, " %d", u.ualign.record);
        enl_msg("你的阵营值", "为", "为", buf, "");
    }

    /*** Resistances to troubles ***/
    if (Invulnerable)
        you_have("绝对防御", from_what(INVULNERABLE));
    if (Antimagic)
        you_are("受魔法的保护", from_what(ANTIMAGIC));
    if (Fire_resistance)
        you_have("火焰抵抗", from_what(FIRE_RES));
    if (Cold_resistance)
        you_have("冰霜抵抗", from_what(COLD_RES));
    if (Sleep_resistance)
        you_have("沉睡抵抗", from_what(SLEEP_RES));
    if (Disint_resistance)
        you_have("分解抵抗", from_what(DISINT_RES));
    if (Shock_resistance)
        you_have("电击抵抗", from_what(SHOCK_RES));
    if (Poison_resistance)
        you_have("毒性抵抗", from_what(POISON_RES));
    if (Acid_resistance)
        you_have("酸性抵抗", from_what(ACID_RES));
    if (Drain_resistance)
        you_have("降级抵抗", from_what(DRAIN_RES));
    if (Sick_resistance)
        you_have("免疫疾病", from_what(SICK_RES));
    if (Stone_resistance)
        you_have("石化抵抗", from_what(STONE_RES));
    if (Halluc_resistance)
        enl_msg(You_, "抵抗", "抵抗", " 幻觉",
                from_what(HALLUC_RES));
    if (u.uedibility)
        you_can("识别出有害的食物", "");

    /*** Vision and senses ***/
    if (!Blind && (Blinded || !haseyes(youmonst.data)))
        you_can("看见", from_what(-BLINDED)); /* Eyes of the Overworld */
    if (See_invisible) {
        if (!Blind)
            enl_msg(You_, "能看见", "能看见", "隐形的怪物", from_what(SEE_INVIS));
        else
            enl_msg(You_, "将看见", "将看见",
                    "隐形的怪物当你没有失明时", from_what(SEE_INVIS));
    }
    if (Blind_telepat)
        you_can("感知怪物", from_what(TELEPAT));
    if (Warning)
        you_are("会警报怪物", from_what(WARNING));
    if (Warn_of_mon && context.warntype.obj) {
        Sprintf(buf, "察觉到%s的存在",
                (context.warntype.obj & M2_ORC) ? "兽人"
                : (context.warntype.obj & M2_ELF) ? "精灵"
                : (context.warntype.obj & M2_DEMON) ? "恶魔" : something);
        you_can(buf, from_what(WARN_OF_MON));
    }
    if (Warn_of_mon && context.warntype.polyd) {
        Sprintf(buf, "察觉到%s的存在",
                ((context.warntype.polyd & (M2_HUMAN | M2_ELF))
                 == (M2_HUMAN | M2_ELF))
                    ? "人类和精灵"
                    : (context.warntype.polyd & M2_HUMAN)
                          ? "人类"
                          : (context.warntype.polyd & M2_ELF)
                                ? "精灵"
                                : (context.warntype.polyd & M2_ORC)
                                      ? "兽人"
                                      : (context.warntype.polyd & M2_DEMON)
                                            ? "恶魔"
                                            : "某些怪物");
        you_can(buf, "");
    }
    if (Warn_of_mon && context.warntype.speciesidx >= LOW_PM) {
        Sprintf(buf, "察觉到%s的存在",
                makeplural(mons[context.warntype.speciesidx].mname));
        you_can(buf, from_what(WARN_OF_MON));
    }
    if (Undead_warning)
        you_are("会警报亡灵", from_what(WARN_UNDEAD));
    if (Searching)
        you_are("会自动搜索", from_what(SEARCHING));
    if (Clairvoyant)
        you_have("千里眼", from_what(CLAIRVOYANT));
    else if ((HClairvoyant || EClairvoyant) && BClairvoyant) {
        Strcpy(buf, from_what(-CLAIRVOYANT));
        if (!cnstrcmp(buf, "是因为"))
            /* overwrite substring; strncpy doesn't add terminator */
            (void) strncpy(buf, "要不是", strlen("要不是"));
        enl_msg(You_, "可以有", "可以有", "千里眼", buf);
    }
    if (Infravision)
        you_have("夜视能力", from_what(INFRAVISION));
    if (Detect_monsters)
        you_can("察觉到怪物的存在", "");
    if (u.umconf)
        you_are("会混乱怪物", "");

    /*** Appearance and behavior ***/
    if (Adornment) {
        int adorn = 0;

        if (uleft && uleft->otyp == RIN_ADORNMENT)
            adorn += uleft->spe;
        if (uright && uright->otyp == RIN_ADORNMENT)
            adorn += uright->spe;
        /* the sum might be 0 (+0 ring or two which negate each other);
           that yields "you are charismatic" (which isn't pointless
           because it potentially impacts seduction attacks) */
        Sprintf(buf, "具有%s魅力",
                (adorn > 0) ? "更多的" : (adorn < 0) ? "更少的" : "");
        you_are(buf, from_what(ADORNED));
    }
    if (Invisible)
        you_are("是隐形的", from_what(INVIS));
    else if (Invis)
        you_are("对别人来说是隐形的", from_what(INVIS));
    /* ordinarily "visible" is redundant; this is a special case for
       the situation when invisibility would be an expected attribute */
    else if ((HInvis || EInvis) && BInvis)
        you_are("是可见的", from_what(-INVIS));
    if (Displaced)
        you_have("位移幻影", from_what(DISPLACED));
    if (Stealth)
        you_have("潜行", from_what(STEALTH));
    if (Aggravate_monster)
        enl_msg("你会触怒", "", "", "怪物",
                from_what(AGGRAVATE_MONSTER));
    if (Conflict)
        enl_msg("你会引起", "", "", "冲突", from_what(CONFLICT));

    /*** Transportation ***/
    if (Jumping)
        you_can("跳", from_what(JUMPING));
    if (Teleportation)
        you_can("传送", from_what(TELEPORT));
    if (Teleport_control)
        you_have("传送控制", from_what(TELEPORT_CONTROL));
    /* actively levitating handled earlier as a status condition */
    if (BLevitation) { /* levitation is blocked */
        long save_BLev = BLevitation;

        BLevitation = 0L;
        if (Levitation) {
            /* either trapped in the floor or inside solid rock
               (or both if chained to buried iron ball and have
               moved one step into solid rock somehow) */
            boolean trapped = (save_BLev & I_SPECIAL) != 0L,
                    terrain = (save_BLev & FROMOUTSIDE) != 0L;

            Sprintf(buf, "%s%s%s的话",
                    trapped ? "如果未被困" : "",
                    (trapped && terrain) ? "且" : "",
                    terrain ? if_surroundings_permitted : "");
            enl_msg(You_, "快飘起来", "快飘起来", buf, "");
        }
        BLevitation = save_BLev;
    }
    /* actively flying handled earlier as a status condition */
    if (BFlying) { /* flight is blocked */
        long save_BFly = BFlying;

        BFlying = 0L;
        if (Flying) {
            enl_msg(You_, "会飞起来", "会飞起来",
                    /* wording quibble: for past tense, "hadn't been"
                       would sound better than "weren't" (and
                       "had permitted" better than "permitted"), but
                       "weren't" and "permitted" are adequate so the
                       extra complexity to handle that isn't worth it */
                    Levitation
                       ? "如果你没飘着的话"
                       : (save_BFly == I_SPECIAL)
                          /* this is an oversimpliction; being trapped
                             might also be blocking levitation so flight
                             would still be blocked after escaping trap */
                          ? "如果你没被困的话"
                          : (save_BFly == FROMOUTSIDE)
                             ? if_surroundings_permitted
                             /* two or more of levitation, surroundings,
                                and being trapped in the floor */
                             : "如果情况允许的话",
                    "");
        }
        BFlying = save_BFly;
    }
    /* actively walking on water handled earlier as a status condition */
    if (Wwalking && !walking_on_water())
        you_can("在水面上走", from_what(WWALKING));
    /* actively swimming (in water but not under it) handled earlier */
    if (Swimming && (Underwater || !u.uinwater))
        you_can("游泳", from_what(SWIMMING));
    if (Breathless)
        you_can("不用呼吸", from_what(MAGICAL_BREATHING));
    else if (Amphibious)
        you_can("在水里呼吸", from_what(MAGICAL_BREATHING));
    if (Passes_walls)
        you_can("穿墙", from_what(PASSES_WALLS));

    /*** Physical attributes ***/
    if (Regeneration)
        enl_msg("你有再生的能力", "", "", "", from_what(REGENERATION));
    if (Slow_digestion)
        you_have("慢消化", from_what(SLOW_DIGESTION));
    if (u.uhitinc)
        you_have(enlght_combatinc("来攻击", u.uhitinc, final, buf), "");
    if (u.udaminc)
        you_have(enlght_combatinc("伤害", u.udaminc, final, buf), "");
    if (u.uspellprot || Protection) {
        int prot = 0;

        if (uleft && uleft->otyp == RIN_PROTECTION)
            prot += uleft->spe;
        if (uright && uright->otyp == RIN_PROTECTION)
            prot += uright->spe;
        if (HProtection & INTRINSIC)
            prot += u.ublessed;
        prot += u.uspellprot;
        if (prot)
            you_have(enlght_combatinc("防御", prot, final, buf), "");
    }
    if ((armpro = magic_negation(&youmonst)) > 0) {
        /* magic cancellation factor, conferred by worn armor */
        static const char *const mc_types[] = {
            "" /*ordinary*/, "受防护", "受保卫", "受保护",
        };
        /* sanity check */
        if (armpro >= SIZE(mc_types))
            armpro = SIZE(mc_types) - 1;
        you_are(mc_types[armpro], "");
    }
    if (Half_physical_damage)
        enlght_halfdmg(HALF_PHDAM, final);
    if (Half_spell_damage)
        enlght_halfdmg(HALF_SPDAM, final);
    /* polymorph and other shape change */
    if (Protection_from_shape_changers)
        you_have("变形保护",
                from_what(PROT_FROM_SHAPE_CHANGERS));
    if (Unchanging) {
        const char *what = 0;

        if (!Upolyd) /* Upolyd handled below after current form */
            you_are("不能改变你当前的外貌",
                    from_what(UNCHANGING));
        /* blocked shape changes */
        if (Polymorph)
            what = !final ? "变形" : "变形";
        else if (u.ulycn >= LOW_PM)
            what = !final ? "改变外形" : "改变外形";
        if (what) {
            Sprintf(buf, "将周期性地%s", what);
            /* omit from_what(UNCHANGING); too verbose */
            enl_msg(You_, buf, buf, " 如果你没有被锁在当前的外貌",
                    "");
        }
    } else if (Polymorph) {
        you_are("会周期性地变形", from_what(POLYMORPH));
    }
    if (Polymorph_control)
        you_have("变形控制", from_what(POLYMORPH_CONTROL));
    if (Upolyd && u.umonnum != u.ulycn
        /* if we've died from turning into slime, we're polymorphed
           right now but don't want to list it as a temporary attribute
           [we need a more reliable way to detect this situation] */
        && !(final == ENL_GAMEOVERDEAD
             && u.umonnum == PM_GREEN_SLIME && !Unchanging)) {
        /* foreign shape (except were-form which is handled below) */
        Sprintf(buf, "已变形为%s", youmonst.data->mname);
        if (wizard)
            Sprintf(eos(buf), " (%d)", u.mtimedone);
        you_are(buf, "");
    }
    if (lays_eggs(youmonst.data) && flags.female) /* Upolyd */
        you_can("下蛋", "");
    if (u.ulycn >= LOW_PM) {
        /* "you are a werecreature [in beast form]" */
        Strcpy(buf, mons[u.ulycn].mname);
        if (u.umonnum == u.ulycn) {
            Strcat(buf, "是野兽形态");
            if (wizard)
                Sprintf(eos(buf), " (%d)", u.mtimedone);
        }
        you_are(buf, "");
    }
    if (Unchanging && Upolyd) /* !Upolyd handled above */
        you_are("不能改变你当前的外貌", from_what(UNCHANGING));
    if (Hate_silver)
        you_are("会受到银制品的伤害", "");
    /* movement and non-armor-based protection */
    if (Fast)
        you_are(Very_fast ? "速度非常快" : "速度快", from_what(FAST));
    if (Reflecting)
        you_have("反射", from_what(REFLECTING));
    if (Free_action)
        you_have("自由行动", from_what(FREE_ACTION));
    if (Fixed_abil)
        you_have("固定的能力", from_what(FIXED_ABIL));
    if (Lifesaved)
        enl_msg("你", "能", "能", "复活", "");

    /*** Miscellany ***/
    if (Luck) {
        ltmp = abs((int) Luck);
        Sprintf(buf, "%s%s幸运",
                ltmp >= 10 ? "极其" : ltmp >= 5 ? "非常" : "",
                Luck < 0 ? "不" : "");
        if (wizard)
            Sprintf(eos(buf), " (%d)", Luck);
        you_are(buf, "");
    } else if (wizard)
        enl_msg("你的幸运值", "为", "为", " 0", "");
    if (u.moreluck > 0)
        you_have("额外的幸运", "");
    else if (u.moreluck < 0)
        you_are("被减少了幸运", "");
    if (carrying(LUCKSTONE) || stone_luck(TRUE)) {
        ltmp = stone_luck(FALSE);
        if (ltmp <= 0)
            enl_msg("坏运气", "不会", "不会", "消失", "");
        if (ltmp >= 0)
            enl_msg("好运气", "不会", "不会", "消失", "");
    }

    if (u.ugangr) {
        Sprintf(buf, "对你%s生气",
                u.ugangr > 6 ? "极其" : u.ugangr > 3 ? "非常" : "");
        if (wizard)
            Sprintf(eos(buf), " (%d)", u.ugangr);
        enl_msg(u_gname(), " ", " ", buf, "");
    } else {
        /*
         * We need to suppress this when the game is over, because death
         * can change the value calculated by can_pray(), potentially
         * resulting in a false claim that you could have prayed safely.
         */
        if (!final) {
#if 0
            /* "can [not] safely pray" vs "could [not] have safely prayed" */
            Sprintf(buf, "%s能%s平安地祈祷%s", can_pray(FALSE) ? "" : "不 ",
                    final ? " " : "", final ? "" : "");
#else
            Sprintf(buf, "%s能平安地祈祷", can_pray(FALSE) ? "" : "不");
#endif
            if (wizard)
                Sprintf(eos(buf), " (%d)", u.ublesscnt);
            you_are(buf, "");
        }
    }

#ifdef DEBUG
    /* named fruit debugging (doesn't really belong here...); to enable,
       include 'fruit' in DEBUGFILES list (even though it isn't a file...) */
    if (wizard && explicitdebug("fruit")) {
        struct fruit *f;

        reorder_fruit(TRUE); /* sort by fruit index, from low to high;
                              * this modifies the ffruit chain, so could
                              * possibly mask or even introduce a problem,
                              * but it does useful sanity checking */
        for (f = ffruit; f; f = f->nextf) {
            Sprintf(buf, "Fruit #%d ", f->fid);
            enl_msg(buf, "is ", "was ", f->fname, "");
        }
        enl_msg("当前的水果 ", "是 ", "是 ", pl_fruit, "");
        Sprintf(buf, "%d", flags.made_fruit);
        enl_msg("制作水果标志 ", "是 ", "是 ", buf, "");
    }
#endif

    {
        const char *p;

        buf[0] = '\0';
        if (final < 2) { /* still in progress, or quit/escaped/ascended */
            p = "活着之前被杀死了";
            switch (u.umortality) {
            case 0:
                p = !final ? (char *) 0 : "活着";
                break;
            default:
                Sprintf(buf, "%d次", u.umortality);
                break;
            }
        } else { /* game ended in character's death */
            p = "死了";
            switch (u.umortality) {
            case 0:
                impossible("dead without dying?");
            case 1:
                break; /* just "are dead" */
            default:
                Sprintf(buf, " (第%d次!)", u.umortality);
                break;
            }
        }
        if (p)
            enl_msg(You_, "被杀死了", p, buf, "");
    }
}

#if 0  /* no longer used */
STATIC_DCL boolean NDECL(minimal_enlightenment);

/*
 * Courtesy function for non-debug, non-explorer mode players
 * to help refresh them about who/what they are.
 * Returns FALSE if menu cancelled (dismissed with ESC), TRUE otherwise.
 */
STATIC_OVL boolean
minimal_enlightenment()
{
    winid tmpwin;
    menu_item *selected;
    anything any;
    int genidx, n;
    char buf[BUFSZ], buf2[BUFSZ];
    static const char untabbed_fmtstr[] = "%-15s: %-12s";
    static const char untabbed_deity_fmtstr[] = "%-17s%s";
    static const char tabbed_fmtstr[] = "%s:\t%-12s";
    static const char tabbed_deity_fmtstr[] = "%s\t%s";
    static const char *fmtstr;
    static const char *deity_fmtstr;

    fmtstr = iflags.menu_tab_sep ? tabbed_fmtstr : untabbed_fmtstr;
    deity_fmtstr = iflags.menu_tab_sep ? tabbed_deity_fmtstr
                                       : untabbed_deity_fmtstr;
    any = zeroany;
    buf[0] = buf2[0] = '\0';
    tmpwin = create_nhwindow(NHW_MENU);
    start_menu(tmpwin);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, iflags.menu_headings,
             "Starting", FALSE);

    /* Starting name, race, role, gender */
    Sprintf(buf, fmtstr, "name", plname);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);
    Sprintf(buf, fmtstr, "race", urace.noun);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);
    Sprintf(buf, fmtstr, "role",
            (flags.initgend && urole.name.f) ? urole.name.f : urole.name.m);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);
    Sprintf(buf, fmtstr, "gender", genders[flags.initgend].adj);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);

    /* Starting alignment */
    Sprintf(buf, fmtstr, "alignment", align_str(u.ualignbase[A_ORIGINAL]));
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);

    /* Current name, race, role, gender */
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, "", FALSE);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, iflags.menu_headings,
             "Current", FALSE);
    Sprintf(buf, fmtstr, "race", Upolyd ? youmonst.data->mname : urace.noun);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);
    if (Upolyd) {
        Sprintf(buf, fmtstr, "role (base)",
                (u.mfemale && urole.name.f) ? urole.name.f
                                            : urole.name.m);
        add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);
    } else {
        Sprintf(buf, fmtstr, "role",
                (flags.female && urole.name.f) ? urole.name.f
                                               : urole.name.m);
        add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);
    }
    /* don't want poly_gender() here; it forces `2' for non-humanoids */
    genidx = is_neuter(youmonst.data) ? 2 : flags.female;
    Sprintf(buf, fmtstr, "gender", genders[genidx].adj);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);
    if (Upolyd && (int) u.mfemale != genidx) {
        Sprintf(buf, fmtstr, "gender (base)", genders[u.mfemale].adj);
        add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);
    }

    /* Current alignment */
    Sprintf(buf, fmtstr, "alignment", align_str(u.ualign.type));
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);

    /* Deity list */
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, "", FALSE);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, iflags.menu_headings,
             "Deities", FALSE);
    Sprintf(buf2, deity_fmtstr, align_gname(A_CHAOTIC),
            (u.ualignbase[A_ORIGINAL] == u.ualign.type
             && u.ualign.type == A_CHAOTIC)               ? " (s,c)"
                : (u.ualignbase[A_ORIGINAL] == A_CHAOTIC) ? " (s)"
                : (u.ualign.type   == A_CHAOTIC)          ? " (c)" : "");
    Sprintf(buf, fmtstr, "Chaotic", buf2);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);

    Sprintf(buf2, deity_fmtstr, align_gname(A_NEUTRAL),
            (u.ualignbase[A_ORIGINAL] == u.ualign.type
             && u.ualign.type == A_NEUTRAL)               ? " (s,c)"
                : (u.ualignbase[A_ORIGINAL] == A_NEUTRAL) ? " (s)"
                : (u.ualign.type   == A_NEUTRAL)          ? " (c)" : "");
    Sprintf(buf, fmtstr, "Neutral", buf2);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);

    Sprintf(buf2, deity_fmtstr, align_gname(A_LAWFUL),
            (u.ualignbase[A_ORIGINAL] == u.ualign.type
             && u.ualign.type == A_LAWFUL)                ? " (s,c)"
                : (u.ualignbase[A_ORIGINAL] == A_LAWFUL)  ? " (s)"
                : (u.ualign.type   == A_LAWFUL)           ? " (c)" : "");
    Sprintf(buf, fmtstr, "Lawful", buf2);
    add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, FALSE);

    end_menu(tmpwin, "Base Attributes");
    n = select_menu(tmpwin, PICK_NONE, &selected);
    destroy_nhwindow(tmpwin);
    return (boolean) (n != -1);
}
#endif /*0*/

/* ^X command */
STATIC_PTR int
doattributes(VOID_ARGS)
{
    int mode = BASICENLIGHTENMENT;

    /* show more--as if final disclosure--for wizard and explore modes */
    if (wizard || discover)
        mode |= MAGICENLIGHTENMENT;

    enlightenment(mode, ENL_GAMEINPROGRESS);
    return 0;
}

void
youhiding(via_enlghtmt, msgflag)
boolean via_enlghtmt; /* englightment line vs topl message */
int msgflag;          /* for variant message phrasing */
{
    char *bp, buf[BUFSZ];

    Strcpy(buf, "隐藏");
    if (U_AP_TYPE != M_AP_NOTHING) {
        /* mimic; hero is only able to mimic a strange object or gold
           or hallucinatory alternative to gold, so we skip the details
           for the hypothetical furniture and monster cases */
        bp = eos(strcpy(buf, "在模拟"));
        if (U_AP_TYPE == M_AP_OBJECT) {
            Sprintf(bp, " %s", simple_typename(youmonst.mappearance));
        } else if (U_AP_TYPE == M_AP_FURNITURE) {
            Strcpy(bp, "什么东西");
        } else if (U_AP_TYPE == M_AP_MONSTER) {
            Strcpy(bp, "什么怪");
        } else {
            ; /* something unexpected; leave 'buf' as-is */
        }
    } else if (u.uundetected) {
        bp = eos(buf); /* points past "hiding" */
        if (youmonst.data->mlet == S_EEL) {
            if (is_pool(u.ux, u.uy))
                Sprintf(bp, "在%s里", waterbody_name(u.ux, u.uy));
        } else if (hides_under(youmonst.data)) {
            struct obj *o = level.objects[u.ux][u.uy];

            if (o)
                Sprintf(bp, "在%s下面", ansimpleoname(o));
        } else if (is_clinger(youmonst.data) || Flying) {
            /* Flying: 'lurker above' hides on ceiling but doesn't cling */
            Sprintf(bp, "在%s上", ceiling(u.ux, u.uy));
        } else {
            /* on floor; is_hider() but otherwise not special: 'trapper' */
            if (u.utrap && u.utraptype == TT_PIT) {
                struct trap *t = t_at(u.ux, u.uy);

                Sprintf(bp, "在一个%s坑里",
                        (t && t->ttyp == SPIKED_PIT) ? "有钉子的" : "");
            } else
                Sprintf(bp, "在%s上", surface(u.ux, u.uy));
        }
    } else {
        ; /* shouldn't happen; will result in generic "you are hiding" */
    }

    if (via_enlghtmt) {
        int final = msgflag; /* 'final' is used by you_are() macro */

        you_are(buf, "");
    } else {
        /* for dohide(), when player uses '#monster' command */
        You("%s%s.", msgflag ? "已经" : "现在", buf);
    }
}

/* KMH, #conduct
 * (shares enlightenment's tense handling)
 */
int
doconduct(VOID_ARGS)
{
    show_conduct(0);
    return 0;
}

void
show_conduct(final)
int final;
{
    char buf[BUFSZ];
    int ngenocided;

    /* Create the conduct window */
    en_win = create_nhwindow(NHW_MENU);
    putstr(en_win, 0, "自愿挑战:");

    if (u.uroleplay.blind)
        you_are("自生来就失明", "");
    if (u.uroleplay.nudist)
        you_have_been("忠实的裸体主义者");

    if (!u.uconduct.food)
        enl_msg(You_, "没有", "没有", "吃过食物", "");
        /* but beverages are okay */
    else if (!u.uconduct.unvegan)
        you_have_X("遵循严格的纯素饮食");
    else if (!u.uconduct.unvegetarian)
        you_have_been("素食者");

    if (!u.uconduct.gnostic)
        you_have_been("一个无神论者");

    if (!u.uconduct.weaphit) {
        you_have_never("用武器攻击");
    } else if (wizard) {
        Sprintf(buf, "用武器%ld次", u.uconduct.weaphit);
        you_have_X(buf);
    }
    if (!u.uconduct.killer)
        you_have_been("和平主义者");

    if (!u.uconduct.literate) {
        you_have_been("文盲");
    } else if (wizard) {
        Sprintf(buf, "阅读物品或雕刻%ld次", u.uconduct.literate);
        you_have_X(buf);
    }

    ngenocided = num_genocides();
    if (ngenocided == 0) {
        you_have_never("灭绝任何怪物");
    } else {
        Sprintf(buf, "灭绝%d种怪", ngenocided);
        you_have_X(buf);
    }

    if (!u.uconduct.polypiles) {
        you_have_never("给一个对象变形");
    } else if (wizard) {
        Sprintf(buf, "给别人变形%ld项", u.uconduct.polypiles);
        you_have_X(buf);
    }

    if (!u.uconduct.polyselfs) {
        you_have_never("改变过外貌");
    } else if (wizard) {
        Sprintf(buf, "改变外貌%ld次", u.uconduct.polyselfs);
        you_have_X(buf);
    }

    if (!u.uconduct.wishes) {
        you_are("未许愿过", "");
    } else {
        Sprintf(buf, "用过%ld次许愿", u.uconduct.wishes);
        if (u.uconduct.wisharti) {
            /* if wisharti == wishes
             *  1 wish (for an artifact)
             *  2 wishes (both for artifacts)
             *  N wishes (all for artifacts)
             * else (N is at least 2 in order to get here; M < N)
             *  N wishes (1 for an artifact)
             *  N wishes (M for artifacts)
             */
            if (u.uconduct.wisharti == u.uconduct.wishes)
                Sprintf(eos(buf), " (%s",
                        (u.uconduct.wisharti > 2L) ? "全是"
                          : (u.uconduct.wisharti == 2L) ? "都是" : "");
            else
                Sprintf(eos(buf), " (%ld ", u.uconduct.wisharti);

            Sprintf(eos(buf), "%s)",
                    (u.uconduct.wisharti == 1L) ? "1 神器"
                                                : "神器");
        }
        you_have_X(buf);

        if (!u.uconduct.wisharti)
            enl_msg(You_, "未许愿", "未许愿",
                    "任何神器", "");
    }

    /* Pop up the window and wait for a key */
    display_nhwindow(en_win, TRUE);
    destroy_nhwindow(en_win);
    en_win = WIN_ERR;
}

/* ordered by command name */
struct ext_func_tab extcmdlist[] = {
    { '#', "#", "#", "执行扩展命令",
            doextcmd, IFBURIED | GENERALCMD },
    { M('?'), "?", "?", "列出所有扩展命令",
            doextlist, IFBURIED | AUTOCOMPLETE | GENERALCMD },
    { M('a'), "adjust", "调整", "调整背包排列字母",
            doorganize, IFBURIED | AUTOCOMPLETE },
    { M('A'), "annotate", "备注", "给这一层备注",
            donamelevel, IFBURIED | AUTOCOMPLETE },
    { 'a', "apply", "使用", "使用工具(鹤嘴锄, 钥匙, 灯...)",
            doapply },
    { C('x'), "attributes", "属性", "显示你的属性",
            doattributes, IFBURIED },
    { '@', "autopickup", "自动拾取", "自动拾取开关",
            dotogglepickup, IFBURIED },
    { 'C', "call", "称作", "称呼什么东西", docallcmd, IFBURIED },
    { 'Z', "cast", "施法", "施展魔法", docast, IFBURIED },
    { M('c'), "chat", "交谈", "和某人交谈", dotalk, IFBURIED | AUTOCOMPLETE },
    { 'c', "close", "关门", "关门", doclose },
    { M('C'), "conduct", "行为", "列出你保持的自愿挑战",
            doconduct, IFBURIED | AUTOCOMPLETE },
    { M('d'), "dip", "浸", "把物品浸到某物", dodip, AUTOCOMPLETE },
    { '>', "down", "下楼", "下楼梯", dodown },
    { 'd', "drop", "扔掉", "扔掉某物", dodrop },
    { 'D', "droptype", "扔掉一类", "扔掉明确的一类物品", doddrop },
    { 'e', "eat", "吃", "吃东西", doeat },
    { 'E', "engrave", "刻写", "在地上写字", doengrave },
    { M('e'), "enhance", "提升能力", "提高或检查武器和魔法技能",
            enhance_weapon_skill, IFBURIED | AUTOCOMPLETE },
    { '\0', "exploremode", "探索模式", "进入探索(发现) 模式",
            enter_explore_mode, IFBURIED },
    { 'f', "fire", "发射", "从箭袋里发射", dofire },
    { M('f'), "force", "开锁", "强行开锁", doforce, AUTOCOMPLETE },
    { ';', "glance", "扫视", "显示地图符号对应的类型",
            doquickwhatis, IFBURIED | GENERALCMD },
    { '?', "help", "帮助", "给出帮助信息", dohelp, IFBURIED | GENERALCMD },
    { '\0', "herecmdmenu", "这里可执行", "显示可在此处执行的命令菜单",
            doherecmdmenu, IFBURIED },
    { 'V', "history", "历史", "显示游戏长版本历史",
            dohistory, IFBURIED | GENERALCMD },
    { 'i', "inventory", "背包", "显示你的背包", ddoinv, IFBURIED },
    { 'I', "inventtype", "背包一类", "背包里明确的一类物品",
            dotypeinv, IFBURIED },
    { M('i'), "invoke", "激活", "激活物品的特殊能力",
            doinvoke, IFBURIED | AUTOCOMPLETE },
    { M('j'), "jump", "跳", "跳到另一个位置", dojump, AUTOCOMPLETE },
    { C('d'), "kick", "踢", "踢什么东西", dokick },
    { '\\', "known", "已知", "显示已发现的物品类型",
            dodiscovered, IFBURIED | GENERALCMD },
    { '`', "knownclass", "已知一类", "显示已发现类型里的一类物品",
            doclassdisco, IFBURIED | GENERALCMD },
    { '\0', "levelchange", "levelchange", "改变等级",
            wiz_level_change, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { '\0', "lightsources", "lightsources", "show mobile light sources",
            wiz_light_sources, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { ':', "look", "查看", "查看这里有什么", dolook, IFBURIED },
    { M('l'), "loot", "搜刮", "搜刮地上的箱子", doloot, AUTOCOMPLETE },
#ifdef DEBUG_MIGRATING_MONS
    { '\0', "migratemons", "migratemons", "migrate N random monsters",
            wiz_migrate_mons, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
#endif
    { M('m'), "monster", "怪物能力", "使用怪物的特殊能力",
            domonability, IFBURIED | AUTOCOMPLETE },
    { 'N', "name", "命名", "给怪物或物品命名",
            docallcmd, IFBURIED | AUTOCOMPLETE },
    { M('o'), "offer", "献祭", "给神献祭",
            dosacrifice, AUTOCOMPLETE },
    { 'o', "open", "开门", "开门", doopen },
    { 'O', "options", "选项", "显示选项设置, 并改变它们",
            doset, IFBURIED | GENERALCMD },
    { C('o'), "overview", "概述", "显示已探索地牢的摘要",
            dooverview, IFBURIED | AUTOCOMPLETE },
    { '\0', "panic", "panic", "test panic routine (fatal to game)",
            wiz_panic, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { 'p', "pay", "付钱", "为你的购物清单付钱", dopay },
    { ',', "pickup", "拾取", "在当前位置拾取东西", dopickup },
    { '\0', "polyself", "polyself", "自我变形",
            wiz_polyself, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { M('p'), "pray", "祈祷", "向神祈祷帮助",
            dopray, IFBURIED | AUTOCOMPLETE },
    { C('p'), "prevmsg", "信息", "显示最近的游戏信息",
            doprev_message, IFBURIED | GENERALCMD },
    { 'P', "puton", "戴上", "戴上首饰(戒指, 护身符, 等等)", doputon },
    { 'q', "quaff", "喝", "喝什么东西", dodrink },
    { M('q'), "quit", "退出游戏", "不保存退出游戏",
            done2, IFBURIED | AUTOCOMPLETE | GENERALCMD },
    { 'Q', "quiver", "准备", "准备好发射物", dowieldquiver },
    { 'r', "read", "阅读", "阅读卷轴或魔法书", doread },
    { C('r'), "redraw", "刷新", "刷新屏幕", doredraw, IFBURIED | GENERALCMD },
    { 'R', "remove", "摘下", "摘下首饰(戒指, 护身符, 等等)", doremring },
    { M('R'), "ride", "乘骑", "乘骑或取消乘骑怪物",
            doride, AUTOCOMPLETE },
    { M('r'), "rub", "擦拭", "擦拭灯或石头", dorub, AUTOCOMPLETE },
    { 'S', "save", "保存退出", "保存游戏并退出", dosave, IFBURIED | GENERALCMD },
    { 's', "search", "搜索", "搜索陷阱和暗门",
            dosearch, IFBURIED, "搜索" },
    { '*', "seeall", "查看装备", "显示所有在使用的装备", doprinuse, IFBURIED },
    { AMULET_SYM, "seeamulet", "查看护身符", "显示穿戴的护身符",
            dopramulet, IFBURIED },
    { ARMOR_SYM, "seearmor", "查看盔甲", "显示穿戴的盔甲",
            doprarm, IFBURIED },
    { GOLD_SYM, "seegold", "查看金币", "你拥有的金币", doprgold, IFBURIED },
    { '\0', "seenv", "seenv", "show seen vectors",
            wiz_show_seenv, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { RING_SYM, "seerings", "查看戒指", "显示穿戴的戒指",
            doprring, IFBURIED },
    { SPBOOK_SYM, "seespells", "查看魔法", "列出并重新排序已知的魔法",
            dovspell, IFBURIED },
    { TOOL_SYM, "seetools", "查看工具", "显示在使用的工具",
            doprtool, IFBURIED },
    { '^', "seetrap", "查看陷阱", "显示邻近的陷阱的类型", doidtrap, IFBURIED },
    { WEAPON_SYM, "seeweapon", "查看武器", "显示装备的武器",
            doprwep, IFBURIED },
    { '!', "shell", "shell", "do a shell escape",
            dosh_core, IFBURIED | GENERALCMD
#ifndef SHELL
                       | CMD_NOT_AVAILABLE
#endif /* SHELL */
    },
    { M('s'), "sit", "坐", "坐下", dosit, AUTOCOMPLETE },
    { '\0', "stats", "stats", "show memory statistics",
            wiz_show_stats, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { C('z'), "suspend", "暂停", "暂停游戏",
            dosuspend_core, IFBURIED | GENERALCMD
#ifndef SUSPEND
                            | CMD_NOT_AVAILABLE
#endif /* SUSPEND */
    },
    { 'x', "swap", "交换武器", "交换主武器和副武器", doswapweapon },
    { 'T', "takeoff", "脱下", "脱下一件盔甲", dotakeoff },
    { 'A', "takeoffall", "全部脱下", "脱下所有盔甲", doddoremarm },
    { C('t'), "teleport", "传送", "在这一层里传送", dotelecmd, IFBURIED },
    { '\0', "terrain", "地图显示", "无障碍显示地图",
            doterrain, IFBURIED | AUTOCOMPLETE },
    { '\0', "therecmdmenu", "那里可执行",
            "显示从这里到相邻点你能执行的命令菜单",
            dotherecmdmenu },
    { 't', "throw", "投掷", "投掷某物", dothrow },
    { '\0', "timeout", "timeout", "look at timeout queue and hero's timed intrinsics",
            wiz_timeout_queue, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { M('T'), "tip", "倒出", "倒空容器", dotip, AUTOCOMPLETE },
    { '_', "travel", "移动", "移动到地图上的具体位置", dotravel },
    { M('t'), "turn", "超度", "超度亡灵", doturn, IFBURIED | AUTOCOMPLETE },
    { 'X', "twoweapon", "双持", "拿两把武器战斗",
            dotwoweapon, AUTOCOMPLETE },
    { M('u'), "untrap", "解除陷阱", "解除陷阱", dountrap, AUTOCOMPLETE },
    { '<', "up", "上楼", "上楼梯", doup },
    { '\0', "vanquished", "消灭怪物", "列出消灭的怪物",
            dovanquished, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { M('v'), "version", "版本",
            "列出NetHack 这个版本的编译时间选项",
            doextversion, IFBURIED | AUTOCOMPLETE | GENERALCMD },
    { 'v', "versionshort", "版本信息", "显示版本信息", doversion, IFBURIED | GENERALCMD },
    { '\0', "vision", "vision", "show vision array",
            wiz_show_vision, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { '.', "wait", "休息", "什么都不做休息一回",
            donull, IFBURIED, "休息" },
    { 'W', "wear", "穿戴", "穿戴一件盔甲", dowear },
    { '&', "whatdoes", "做什么", "告诉你命令是做什么的", dowhatdoes, IFBURIED },
    { '/', "whatis", "是什么", "显示符号对应的类型",
            dowhatis, IFBURIED | GENERALCMD },
    { 'w', "wield", "持握", "装备武器", dowield },
    { M('w'), "wipe", "擦脸", "擦你的脸", dowipe, AUTOCOMPLETE },
#ifdef DEBUG
    { '\0', "wizbury", "wizbury", "bury objs under and around you",
            wiz_debug_cmd_bury, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
#endif
    { C('e'), "wizdetect", "wizdetect", "在小范围内显示隐藏的东西",
            wiz_detect, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { C('g'), "wizgenesis", "wizgenesis", "生成一只怪物",
            wiz_genesis, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { C('i'), "wizidentify", "wizidentify", "鉴定背包里的所有物品",
            wiz_identify, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { '\0', "wizintrinsic", "wizintrinsic", "设置内在",
            wiz_intrinsic, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { C('v'), "wizlevelport", "wizlevelport", "传送到其他层",
            wiz_level_tele, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { '\0', "wizmakemap", "wizmakemap", "重新生成这一层",
            wiz_makemap, IFBURIED | WIZMODECMD },
    { C('f'), "wizmap", "wizmap", "这一层的地图",
            wiz_map, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { '\0', "wizrumorcheck", "wizrumorcheck", "verify rumor boundaries",
            wiz_rumor_check, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { '\0', "wizsmell", "wizsmell", "smell monster",
            wiz_smell, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { '\0', "wizwhere", "wizwhere", "显示特殊层的位置",
            wiz_where, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { C('w'), "wizwish", "wizwish", "许愿东西",
            wiz_wish, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { '\0', "wmode", "wmode", "show wall modes",
            wiz_show_wmodes, IFBURIED | AUTOCOMPLETE | WIZMODECMD },
    { 'z', "zap", "挥动", "挥动魔杖", dozap },
    { '\0', (char *) 0, (char *) 0, (char *) 0, donull, 0, (char *) 0 } /* sentinel */
};

/* for key2extcmddesc() to support dowhatdoes() */
struct movcmd {
    uchar k1, k2, k3, k4; /* 'normal', 'qwertz', 'numpad', 'phone' */
    const char *txt, *alt; /* compass direction, screen direction */
};
static const struct movcmd movtab[] = {
    { 'h', 'h', '4', '4', "west",      "left" },
    { 'j', 'j', '2', '8', "south",     "down" },
    { 'k', 'k', '8', '2', "north",     "up" },
    { 'l', 'l', '6', '6', "east",      "right" },
    { 'b', 'b', '1', '7', "southwest", "lower left" },
    { 'n', 'n', '3', '9', "southeast", "lower right" },
    { 'u', 'u', '9', '3', "northeast", "upper right" },
    { 'y', 'z', '7', '1', "northwest", "upper left" },
    {   0,   0,   0,   0,  (char *) 0, (char *) 0 }
};

int extcmdlist_length = SIZE(extcmdlist) - 1;

const char *
key2extcmddesc(key)
uchar key;
{
    static char key2cmdbuf[48];
    const struct movcmd *mov;
    int k, c;
    uchar M_5 = (uchar) M('5'), M_0 = (uchar) M('0');

    /* need to check for movement commands before checking the extended
       commands table because it contains entries for number_pad commands
       that match !number_pad movement (like 'j' for "jump") */
    key2cmdbuf[0] = '\0';
    if (movecmd(k = key))
        Strcpy(key2cmdbuf, "move"); /* "move or attack"? */
    else if (movecmd(k = unctrl(key)))
        Strcpy(key2cmdbuf, "rush");
    else if (movecmd(k = (Cmd.num_pad ? unmeta(key) : lowc(key))))
        Strcpy(key2cmdbuf, "run");
    if (*key2cmdbuf) {
        for (mov = &movtab[0]; mov->k1; ++mov) {
            c = !Cmd.num_pad ? (!Cmd.swap_yz ? mov->k1 : mov->k2)
                             : (!Cmd.phone_layout ? mov->k3 : mov->k4);
            if (c == k) {
                Sprintf(eos(key2cmdbuf), " %s (screen %s)",
                        mov->txt, mov->alt);
                return key2cmdbuf;
            }
        }
    } else if (digit(key) || (Cmd.num_pad && digit(unmeta(key)))) {
        key2cmdbuf[0] = '\0';
        if (!Cmd.num_pad)
            Strcpy(key2cmdbuf, "start of, or continuation of, a count");
        else if (key == '5' || key == M_5)
            Sprintf(key2cmdbuf, "%s prefix",
                    (!!Cmd.pcHack_compat ^ (key == M_5)) ? "run" : "rush");
        else if (key == '0' || (Cmd.pcHack_compat && key == M_0))
            Strcpy(key2cmdbuf, "synonym for 'i'");
        if (*key2cmdbuf)
            return key2cmdbuf;
    }
    if (Cmd.commands[key]) {
        if (Cmd.commands[key]->ef_txt)
            return Cmd.commands[key]->ef_desc;

    }
    return (char *) 0;
}

boolean
bind_key(key, command)
uchar key;
const char *command;
{
    struct ext_func_tab *extcmd;

    /* special case: "nothing" is reserved for unbinding */
    if (!strcmp(command, "nothing")) {
        Cmd.commands[key] = (struct ext_func_tab *) 0;
        return TRUE;
    }

    for (extcmd = extcmdlist; extcmd->ef_txt; extcmd++) {
        if (strcmp(command, extcmd->ef_txt))
            continue;
        Cmd.commands[key] = extcmd;
#if 0 /* silently accept key binding for unavailable command (!SHELL,&c) */
        if ((extcmd->flags & CMD_NOT_AVAILABLE) != 0) {
            char buf[BUFSZ];

            Sprintf(buf, cmdnotavail, extcmd->ef_txt);
            config_error_add("%s", buf);
        }
#endif
        return TRUE;
    }

    return FALSE;
}

/* initialize all keyboard commands */
void
commands_init()
{
    struct ext_func_tab *extcmd;

    for (extcmd = extcmdlist; extcmd->ef_txt; extcmd++)
        if (extcmd->key)
            Cmd.commands[extcmd->key] = extcmd;

    (void) bind_key(C('l'), "redraw"); /* if number_pad is set */
    /*       'b', 'B' : go sw */
    /*       'F' : fight (one time) */
    /*       'g', 'G' : multiple go */
    /*       'h', 'H' : go west */
    (void) bind_key('h',    "help"); /* if number_pad is set */
    (void) bind_key('j',    "jump"); /* if number_pad is on */
    /*       'j', 'J', 'k', 'K', 'l', 'L', 'm', 'M', 'n', 'N' move commands */
    (void) bind_key('k',    "kick"); /* if number_pad is on */
    (void) bind_key('l',    "loot"); /* if number_pad is on */
    (void) bind_key(C('n'), "annotate"); /* if number_pad is on */
    (void) bind_key(M('n'), "name");
    (void) bind_key(M('N'), "name");
    (void) bind_key('u',    "untrap"); /* if number_pad is on */

    /* alt keys: */
    (void) bind_key(M('O'), "overview");
    (void) bind_key(M('2'), "twoweapon");

    /* wait_on_space */
    (void) bind_key(' ',    "wait");
}

int
dokeylist_putcmds(datawin, docount, cmdflags, exflags, keys_used)
winid datawin;
boolean docount;
int cmdflags, exflags;
boolean *keys_used; /* boolean keys_used[256] */
{
    int i;
    char buf[BUFSZ];
    char buf2[QBUFSZ];
    int count = 0;

    for (i = 0; i < 256; i++) {
        const struct ext_func_tab *extcmd;
        uchar key = (uchar) i;

        if (keys_used[i])
            continue;
        if (key == ' ' && !flags.rest_on_space)
            continue;
        if ((extcmd = Cmd.commands[i]) != (struct ext_func_tab *) 0) {
            if ((cmdflags && !(extcmd->flags & cmdflags))
                || (exflags && (extcmd->flags & exflags)))
                continue;
            if (docount) {
                count++;
                continue;
            }
            Sprintf(buf, "%-8s %-12s %s", key2txt(key, buf2),
                    extcmd->ef_txt,
                    extcmd->ef_desc);
            putstr(datawin, 0, buf);
            keys_used[i] = TRUE;
        }
    }
    return count;
}

/* list all keys and their bindings, like dat/hh but dynamic */
void
dokeylist(VOID_ARGS)
{
    char buf[BUFSZ], buf2[BUFSZ];
    uchar key;
    boolean keys_used[256] = {0};
    winid datawin;
    int i;
    static const char
        run_desc[] = "前缀: 一直跑直到看见有趣的东西",
        forcefight_desc[] =
                     "前缀: 强制战斗即使你没有看见怪物";
    static const struct {
        int nhkf;
        const char *desc;
        boolean numpad;
    } misc_keys[] = {
        { NHKF_ESC, "退出当前查询/操作", FALSE },
        { NHKF_RUSH,
          "前缀: 急速前进直到看见有趣的东西", FALSE },
        { NHKF_RUN, run_desc, FALSE },
        { NHKF_RUN2, run_desc, TRUE },
        { NHKF_FIGHT, forcefight_desc, FALSE },
        { NHKF_FIGHT2, forcefight_desc, TRUE } ,
        { NHKF_NOPICKUP,
          "前缀: 移动时不捡取物品/战斗", FALSE },
        { NHKF_RUN_NOPICKUP,
          "前缀: 跑时不捡取物品/战斗", FALSE },
        { NHKF_DOINV, "查看背包", TRUE },
        { NHKF_REQMENU, "前缀: 请求菜单", FALSE },
#ifdef REDO
        { NHKF_DOAGAIN , "重新执行: 再次执行上一个命令", FALSE },
#endif
        { 0, (const char *) 0, FALSE }
    };

    datawin = create_nhwindow(NHW_TEXT);
    putstr(datawin, 0, "");
    putstr(datawin, 0, "            完整的当前键位绑定列表");

    /* directional keys */
    putstr(datawin, 0, "");
    putstr(datawin, 0, "方向键:");
    show_direction_keys(datawin, '.', FALSE); /* '.'==self in direction grid */

    keys_used[(uchar) Cmd.move_NW] = keys_used[(uchar) Cmd.move_N]
        = keys_used[(uchar) Cmd.move_NE] = keys_used[(uchar) Cmd.move_W]
        = keys_used[(uchar) Cmd.move_E] = keys_used[(uchar) Cmd.move_SW]
        = keys_used[(uchar) Cmd.move_S] = keys_used[(uchar) Cmd.move_SE]
        = TRUE;

    if (!iflags.num_pad) {
        keys_used[(uchar) highc(Cmd.move_NW)]
            = keys_used[(uchar) highc(Cmd.move_N)]
            = keys_used[(uchar) highc(Cmd.move_NE)]
            = keys_used[(uchar) highc(Cmd.move_W)]
            = keys_used[(uchar) highc(Cmd.move_E)]
            = keys_used[(uchar) highc(Cmd.move_SW)]
            = keys_used[(uchar) highc(Cmd.move_S)]
            = keys_used[(uchar) highc(Cmd.move_SE)] = TRUE;
        keys_used[(uchar) C(Cmd.move_NW)]
            = keys_used[(uchar) C(Cmd.move_N)]
            = keys_used[(uchar) C(Cmd.move_NE)]
            = keys_used[(uchar) C(Cmd.move_W)]
            = keys_used[(uchar) C(Cmd.move_E)]
            = keys_used[(uchar) C(Cmd.move_SW)]
            = keys_used[(uchar) C(Cmd.move_S)]
            = keys_used[(uchar) C(Cmd.move_SE)] = TRUE;
        putstr(datawin, 0, "");
        putstr(datawin, 0,
          "Shift-<direction> 将沿指定方向移动直到你");
        putstr(datawin, 0, "        撞到墙或者跑进什么东西.");
        putstr(datawin, 0,
          "Ctrl-<direction> 将沿指定方向跑直到");
        putstr(datawin, 0, "        看见有趣的东西.");
    }

    putstr(datawin, 0, "");
    putstr(datawin, 0, "其他键:");
    for (i = 0; misc_keys[i].desc; i++) {
        key = Cmd.spkeys[misc_keys[i].nhkf];
        if (key && ((misc_keys[i].numpad && iflags.num_pad)
                    || !misc_keys[i].numpad)) {
            keys_used[(uchar) key] = TRUE;
            Sprintf(buf, "%-8s %s", key2txt(key, buf2), misc_keys[i].desc);
            putstr(datawin, 0, buf);
        }
    }
#ifndef NO_SIGNAL
    putstr(datawin, 0, "^c       break out of NetHack (SIGINT)");
    keys_used[(uchar) C('c')] = TRUE;
#endif

    putstr(datawin, 0, "");
    show_menu_controls(datawin, TRUE);

    if (dokeylist_putcmds(datawin, TRUE, GENERALCMD, WIZMODECMD, keys_used)) {
        putstr(datawin, 0, "");
        putstr(datawin, 0, "一般命令:");
        (void) dokeylist_putcmds(datawin, FALSE, GENERALCMD, WIZMODECMD,
                                 keys_used);
    }

    if (dokeylist_putcmds(datawin, TRUE, 0, WIZMODECMD, keys_used)) {
        putstr(datawin, 0, "");
        putstr(datawin, 0, "游戏命令:");
        (void) dokeylist_putcmds(datawin, FALSE, 0, WIZMODECMD, keys_used);
    }

    if (wizard
        && dokeylist_putcmds(datawin, TRUE, WIZMODECMD, 0, keys_used)) {
        putstr(datawin, 0, "");
        putstr(datawin, 0, "向导模式命令:");
        (void) dokeylist_putcmds(datawin, FALSE, WIZMODECMD, 0, keys_used);
    }

    display_nhwindow(datawin, FALSE);
    destroy_nhwindow(datawin);
}

char
cmd_from_func(fn)
int NDECL((*fn));
{
    int i;

    for (i = 0; i < 256; ++i)
        if (Cmd.commands[i] && Cmd.commands[i]->ef_funct == fn)
            return (char) i;
    return '\0';
}

/*
 * wizard mode sanity_check code
 */

static const char template[] = "%-27s  %4ld  %6ld";
static const char stats_hdr[] = "                             count  bytes";
static const char stats_sep[] = "---------------------------  ----- -------";

STATIC_OVL int
size_obj(otmp)
struct obj *otmp;
{
    int sz = (int) sizeof (struct obj);

    if (otmp->oextra) {
        sz += (int) sizeof (struct oextra);
        if (ONAME(otmp))
            sz += (int) strlen(ONAME(otmp)) + 1;
        if (OMONST(otmp))
            sz += size_monst(OMONST(otmp), FALSE);
        if (OMID(otmp))
            sz += (int) sizeof (unsigned);
        if (OLONG(otmp))
            sz += (int) sizeof (long);
        if (OMAILCMD(otmp))
            sz += (int) strlen(OMAILCMD(otmp)) + 1;
    }
    return sz;
}

STATIC_OVL void
count_obj(chain, total_count, total_size, top, recurse)
struct obj *chain;
long *total_count;
long *total_size;
boolean top;
boolean recurse;
{
    long count, size;
    struct obj *obj;

    for (count = size = 0, obj = chain; obj; obj = obj->nobj) {
        if (top) {
            count++;
            size += size_obj(obj);
        }
        if (recurse && obj->cobj)
            count_obj(obj->cobj, total_count, total_size, TRUE, TRUE);
    }
    *total_count += count;
    *total_size += size;
}

STATIC_OVL void
obj_chain(win, src, chain, force, total_count, total_size)
winid win;
const char *src;
struct obj *chain;
boolean force;
long *total_count;
long *total_size;
{
    char buf[BUFSZ];
    long count = 0L, size = 0L;

    count_obj(chain, &count, &size, TRUE, FALSE);

    if (count || size || force) {
        *total_count += count;
        *total_size += size;
        Sprintf(buf, template, src, count, size);
        putstr(win, 0, buf);
    }
}

STATIC_OVL void
mon_invent_chain(win, src, chain, total_count, total_size)
winid win;
const char *src;
struct monst *chain;
long *total_count;
long *total_size;
{
    char buf[BUFSZ];
    long count = 0, size = 0;
    struct monst *mon;

    for (mon = chain; mon; mon = mon->nmon)
        count_obj(mon->minvent, &count, &size, TRUE, FALSE);

    if (count || size) {
        *total_count += count;
        *total_size += size;
        Sprintf(buf, template, src, count, size);
        putstr(win, 0, buf);
    }
}

STATIC_OVL void
contained_stats(win, src, total_count, total_size)
winid win;
const char *src;
long *total_count;
long *total_size;
{
    char buf[BUFSZ];
    long count = 0, size = 0;
    struct monst *mon;

    count_obj(invent, &count, &size, FALSE, TRUE);
    count_obj(fobj, &count, &size, FALSE, TRUE);
    count_obj(level.buriedobjlist, &count, &size, FALSE, TRUE);
    count_obj(migrating_objs, &count, &size, FALSE, TRUE);
    /* DEADMONSTER check not required in this loop since they have no
     * inventory */
    for (mon = fmon; mon; mon = mon->nmon)
        count_obj(mon->minvent, &count, &size, FALSE, TRUE);
    for (mon = migrating_mons; mon; mon = mon->nmon)
        count_obj(mon->minvent, &count, &size, FALSE, TRUE);

    if (count || size) {
        *total_count += count;
        *total_size += size;
        Sprintf(buf, template, src, count, size);
        putstr(win, 0, buf);
    }
}

STATIC_OVL int
size_monst(mtmp, incl_wsegs)
struct monst *mtmp;
boolean incl_wsegs;
{
    int sz = (int) sizeof (struct monst);

    if (mtmp->wormno && incl_wsegs)
        sz += size_wseg(mtmp);

    if (mtmp->mextra) {
        sz += (int) sizeof (struct mextra);
        if (MNAME(mtmp))
            sz += (int) strlen(MNAME(mtmp)) + 1;
        if (EGD(mtmp))
            sz += (int) sizeof (struct egd);
        if (EPRI(mtmp))
            sz += (int) sizeof (struct epri);
        if (ESHK(mtmp))
            sz += (int) sizeof (struct eshk);
        if (EMIN(mtmp))
            sz += (int) sizeof (struct emin);
        if (EDOG(mtmp))
            sz += (int) sizeof (struct edog);
        /* mextra->mcorpsenm doesn't point to more memory */
    }
    return sz;
}

STATIC_OVL void
mon_chain(win, src, chain, force, total_count, total_size)
winid win;
const char *src;
struct monst *chain;
boolean force;
long *total_count;
long *total_size;
{
    char buf[BUFSZ];
    long count, size;
    struct monst *mon;
    /* mon->wormno means something different for migrating_mons and mydogs */
    boolean incl_wsegs = !strcmpi(src, "fmon");

    count = size = 0L;
    for (mon = chain; mon; mon = mon->nmon) {
        count++;
        size += size_monst(mon, incl_wsegs);
    }
    if (count || size || force) {
        *total_count += count;
        *total_size += size;
        Sprintf(buf, template, src, count, size);
        putstr(win, 0, buf);
    }
}

STATIC_OVL void
misc_stats(win, total_count, total_size)
winid win;
long *total_count;
long *total_size;
{
    char buf[BUFSZ], hdrbuf[QBUFSZ];
    long count, size;
    int idx;
    struct trap *tt;
    struct damage *sd; /* shop damage */
    struct kinfo *k; /* delayed killer */
    struct cemetery *bi; /* bones info */

    /* traps and engravings are output unconditionally;
     * others only if nonzero
     */
    count = size = 0L;
    for (tt = ftrap; tt; tt = tt->ntrap) {
        ++count;
        size += (long) sizeof *tt;
    }
    *total_count += count;
    *total_size += size;
    Sprintf(hdrbuf, "traps, size %ld", (long) sizeof (struct trap));
    Sprintf(buf, template, hdrbuf, count, size);
    putstr(win, 0, buf);

    count = size = 0L;
    engr_stats("engravings, size %ld+text", hdrbuf, &count, &size);
    *total_count += count;
    *total_size += size;
    Sprintf(buf, template, hdrbuf, count, size);
    putstr(win, 0, buf);

    count = size = 0L;
    light_stats("light sources, size %ld", hdrbuf, &count, &size);
    if (count || size) {
        *total_count += count;
        *total_size += size;
        Sprintf(buf, template, hdrbuf, count, size);
        putstr(win, 0, buf);
    }

    count = size = 0L;
    timer_stats("timers, size %ld", hdrbuf, &count, &size);
    if (count || size) {
        *total_count += count;
        *total_size += size;
        Sprintf(buf, template, hdrbuf, count, size);
        putstr(win, 0, buf);
    }

    count = size = 0L;
    for (sd = level.damagelist; sd; sd = sd->next) {
        ++count;
        size += (long) sizeof *sd;
    }
    if (count || size) {
        *total_count += count;
        *total_size += size;
        Sprintf(hdrbuf, "shop damage, size %ld",
                (long) sizeof (struct damage));
        Sprintf(buf, template, hdrbuf, count, size);
        putstr(win, 0, buf);
    }

    count = size = 0L;
    region_stats("regions, size %ld+%ld*rect+N", hdrbuf, &count, &size);
    if (count || size) {
        *total_count += count;
        *total_size += size;
        Sprintf(buf, template, hdrbuf, count, size);
        putstr(win, 0, buf);
    }

    count = size = 0L;
    for (k = killer.next; k; k = k->next) {
        ++count;
        size += (long) sizeof *k;
    }
    if (count || size) {
        *total_count += count;
        *total_size += size;
        Sprintf(hdrbuf, "delayed killer%s, size %ld",
                plur(count), (long) sizeof (struct kinfo));
        Sprintf(buf, template, hdrbuf, count, size);
        putstr(win, 0, buf);
    }

    count = size = 0L;
    for (bi = level.bonesinfo; bi; bi = bi->next) {
        ++count;
        size += (long) sizeof *bi;
    }
    if (count || size) {
        *total_count += count;
        *total_size += size;
        Sprintf(hdrbuf, "bones history, size %ld",
                (long) sizeof (struct cemetery));
        Sprintf(buf, template, hdrbuf, count, size);
        putstr(win, 0, buf);
    }

    count = size = 0L;
    for (idx = 0; idx < NUM_OBJECTS; ++idx)
        if (objects[idx].oc_uname) {
            ++count;
            size += (long) (strlen(objects[idx].oc_uname) + 1);
        }
    if (count || size) {
        *total_count += count;
        *total_size += size;
        Strcpy(hdrbuf, "object type names, text");
        Sprintf(buf, template, hdrbuf, count, size);
        putstr(win, 0, buf);
    }
}

/*
 * Display memory usage of all monsters and objects on the level.
 */
static int
wiz_show_stats()
{
    char buf[BUFSZ];
    winid win;
    long total_obj_size, total_obj_count,
         total_mon_size, total_mon_count,
         total_ovr_size, total_ovr_count,
         total_misc_size, total_misc_count;

    win = create_nhwindow(NHW_TEXT);
    putstr(win, 0, "Current memory statistics:");

    total_obj_count = total_obj_size = 0L;
    putstr(win, 0, stats_hdr);
    Sprintf(buf, "  Objects, base size %ld", (long) sizeof (struct obj));
    putstr(win, 0, buf);
    obj_chain(win, "invent", invent, TRUE, &total_obj_count, &total_obj_size);
    obj_chain(win, "fobj", fobj, TRUE, &total_obj_count, &total_obj_size);
    obj_chain(win, "buried", level.buriedobjlist, FALSE,
              &total_obj_count, &total_obj_size);
    obj_chain(win, "migrating obj", migrating_objs, FALSE,
              &total_obj_count, &total_obj_size);
    obj_chain(win, "billobjs", billobjs, FALSE,
              &total_obj_count, &total_obj_size);
    mon_invent_chain(win, "minvent", fmon, &total_obj_count, &total_obj_size);
    mon_invent_chain(win, "migrating minvent", migrating_mons,
                     &total_obj_count, &total_obj_size);
    contained_stats(win, "contained", &total_obj_count, &total_obj_size);
    putstr(win, 0, stats_sep);
    Sprintf(buf, template, "  Obj total", total_obj_count, total_obj_size);
    putstr(win, 0, buf);

    total_mon_count = total_mon_size = 0L;
    putstr(win, 0, "");
    Sprintf(buf, "  Monsters, base size %ld", (long) sizeof (struct monst));
    putstr(win, 0, buf);
    mon_chain(win, "fmon", fmon, TRUE, &total_mon_count, &total_mon_size);
    mon_chain(win, "migrating", migrating_mons, FALSE,
              &total_mon_count, &total_mon_size);
    /* 'mydogs' is only valid during level change or end of game disclosure,
       but conceivably we've been called from within debugger at such time */
    if (mydogs) /* monsters accompanying hero */
        mon_chain(win, "mydogs", mydogs, FALSE,
                  &total_mon_count, &total_mon_size);
    putstr(win, 0, stats_sep);
    Sprintf(buf, template, "  Mon total", total_mon_count, total_mon_size);
    putstr(win, 0, buf);

    total_ovr_count = total_ovr_size = 0L;
    putstr(win, 0, "");
    putstr(win, 0, "  Overview");
    overview_stats(win, template, &total_ovr_count, &total_ovr_size);
    putstr(win, 0, stats_sep);
    Sprintf(buf, template, "  Over total", total_ovr_count, total_ovr_size);
    putstr(win, 0, buf);

    total_misc_count = total_misc_size = 0L;
    putstr(win, 0, "");
    putstr(win, 0, "  Miscellaneous");
    misc_stats(win, &total_misc_count, &total_misc_size);
    putstr(win, 0, stats_sep);
    Sprintf(buf, template, "  Misc total", total_misc_count, total_misc_size);
    putstr(win, 0, buf);

    putstr(win, 0, "");
    putstr(win, 0, stats_sep);
    Sprintf(buf, template, "  Grand total",
            (total_obj_count + total_mon_count
             + total_ovr_count + total_misc_count),
            (total_obj_size + total_mon_size
             + total_ovr_size + total_misc_size));
    putstr(win, 0, buf);

#if defined(__BORLANDC__) && !defined(_WIN32)
    show_borlandc_stats(win);
#endif

    display_nhwindow(win, FALSE);
    destroy_nhwindow(win);
    return 0;
}

void
sanity_check()
{
    obj_sanity_check();
    timer_sanity_check();
    mon_sanity_check();
    light_sources_sanity_check();
    bc_sanity_check();
}

#ifdef DEBUG_MIGRATING_MONS
static int
wiz_migrate_mons()
{
    int mcount = 0;
    char inbuf[BUFSZ] = DUMMY;
    struct permonst *ptr;
    struct monst *mtmp;
    d_level tolevel;

    getlin("How many random monsters to migrate? [0]", inbuf);
    if (*inbuf == '\033')
        return 0;
    mcount = atoi(inbuf);
    if (mcount < 0 || mcount > (COLNO * ROWNO) || Is_botlevel(&u.uz))
        return 0;
    while (mcount > 0) {
        if (Is_stronghold(&u.uz))
            assign_level(&tolevel, &valley_level);
        else
            get_level(&tolevel, depth(&u.uz) + 1);
        ptr = rndmonst();
        mtmp = makemon(ptr, 0, 0, NO_MM_FLAGS);
        if (mtmp)
            migrate_to_level(mtmp, ledger_no(&tolevel), MIGR_RANDOM,
                             (coord *) 0);
        mcount--;
    }
    return 0;
}
#endif

struct {
    int nhkf;
    char key;
    const char *name;
} const spkeys_binds[] = {
    { NHKF_ESC,              '\033', (char *) 0 }, /* no binding */
    { NHKF_DOAGAIN,          DOAGAIN, "repeat" },
    { NHKF_REQMENU,          'm', "reqmenu" },
    { NHKF_RUN,              'G', "run" },
    { NHKF_RUN2,             '5', "run.numpad" },
    { NHKF_RUSH,             'g', "rush" },
    { NHKF_FIGHT,            'F', "fight" },
    { NHKF_FIGHT2,           '-', "fight.numpad" },
    { NHKF_NOPICKUP,         'm', "nopickup" },
    { NHKF_RUN_NOPICKUP,     'M', "run.nopickup" },
    { NHKF_DOINV,            '0', "doinv" },
    { NHKF_TRAVEL,           CMD_TRAVEL, (char *) 0 }, /* no binding */
    { NHKF_CLICKLOOK,        CMD_CLICKLOOK, (char *) 0 }, /* no binding */
    { NHKF_REDRAW,           C('r'), "redraw" },
    { NHKF_REDRAW2,          C('l'), "redraw.numpad" },
    { NHKF_GETDIR_SELF,      '.', "getdir.self" },
    { NHKF_GETDIR_SELF2,     's', "getdir.self2" },
    { NHKF_GETDIR_HELP,      '?', "getdir.help" },
    { NHKF_COUNT,            'n', "count" },
    { NHKF_GETPOS_SELF,      '@', "getpos.self" },
    { NHKF_GETPOS_PICK,      '.', "getpos.pick" },
    { NHKF_GETPOS_PICK_Q,    ',', "getpos.pick.quick" },
    { NHKF_GETPOS_PICK_O,    ';', "getpos.pick.once" },
    { NHKF_GETPOS_PICK_V,    ':', "getpos.pick.verbose" },
    { NHKF_GETPOS_SHOWVALID, '$', "getpos.valid" },
    { NHKF_GETPOS_AUTODESC,  '#', "getpos.autodescribe" },
    { NHKF_GETPOS_MON_NEXT,  'm', "getpos.mon.next" },
    { NHKF_GETPOS_MON_PREV,  'M', "getpos.mon.prev" },
    { NHKF_GETPOS_OBJ_NEXT,  'o', "getpos.obj.next" },
    { NHKF_GETPOS_OBJ_PREV,  'O', "getpos.obj.prev" },
    { NHKF_GETPOS_DOOR_NEXT, 'd', "getpos.door.next" },
    { NHKF_GETPOS_DOOR_PREV, 'D', "getpos.door.prev" },
    { NHKF_GETPOS_UNEX_NEXT, 'x', "getpos.unexplored.next" },
    { NHKF_GETPOS_UNEX_PREV, 'X', "getpos.unexplored.prev" },
    { NHKF_GETPOS_VALID_NEXT, 'z', "getpos.valid.next" },
    { NHKF_GETPOS_VALID_PREV, 'Z', "getpos.valid.prev" },
    { NHKF_GETPOS_INTERESTING_NEXT, 'a', "getpos.all.next" },
    { NHKF_GETPOS_INTERESTING_PREV, 'A', "getpos.all.prev" },
    { NHKF_GETPOS_HELP,      '?', "getpos.help" },
    { NHKF_GETPOS_LIMITVIEW, '"', "getpos.filter" },
    { NHKF_GETPOS_MOVESKIP,  '*', "getpos.moveskip" },
    { NHKF_GETPOS_MENU,      '!', "getpos.menu" }
};

boolean
bind_specialkey(key, command)
uchar key;
const char *command;
{
    int i;
    for (i = 0; i < SIZE(spkeys_binds); i++) {
        if (!spkeys_binds[i].name || strcmp(command, spkeys_binds[i].name))
            continue;
        Cmd.spkeys[spkeys_binds[i].nhkf] = key;
        return TRUE;
    }
    return FALSE;
}

/* returns a one-byte character from the text (it may massacre the txt
 * buffer) */
char
txt2key(txt)
char *txt;
{
    txt = trimspaces(txt);
    if (!*txt)
        return '\0';

    /* simple character */
    if (!txt[1])
        return txt[0];

    /* a few special entries */
    if (!strcmp(txt, "<enter>"))
        return '\n';
    if (!strcmp(txt, "<space>"))
        return ' ';
    if (!strcmp(txt, "<esc>"))
        return '\033';

    /* control and meta keys */
    switch (*txt) {
    case 'm': /* can be mx, Mx, m-x, M-x */
    case 'M':
        txt++;
        if (*txt == '-' && txt[1])
            txt++;
        if (txt[1])
            return '\0';
        return M(*txt);
    case 'c': /* can be cx, Cx, ^x, c-x, C-x, ^-x */
    case 'C':
    case '^':
        txt++;
        if (*txt == '-' && txt[1])
            txt++;
        if (txt[1])
            return '\0';
        return C(*txt);
    }

    /* ascii codes: must be three-digit decimal */
    if (*txt >= '0' && *txt <= '9') {
        uchar key = 0;
        int i;

        for (i = 0; i < 3; i++) {
            if (txt[i] < '0' || txt[i] > '9')
                return '\0';
            key = 10 * key + txt[i] - '0';
        }
        return key;
    }

    return '\0';
}

/* returns the text for a one-byte encoding;
 * must be shorter than a tab for proper formatting */
char *
key2txt(c, txt)
uchar c;
char *txt; /* sufficiently long buffer */
{
    /* should probably switch to "SPC", "ESC", "RET"
       since nethack's documentation uses ESC for <escape> */
    if (c == ' ')
        Sprintf(txt, "<space>");
    else if (c == '\033')
        Sprintf(txt, "<esc>");
    else if (c == '\n')
        Sprintf(txt, "<enter>");
    else if (c == '\177')
        Sprintf(txt, "<del>"); /* "<delete>" won't fit */
    else
        Strcpy(txt, visctrl((char) c));
    return txt;
}


void
parseautocomplete(autocomplete, condition)
char *autocomplete;
boolean condition;
{
    struct ext_func_tab *efp;
    register char *autoc;

    /* break off first autocomplete from the rest; parse the rest */
    if ((autoc = index(autocomplete, ',')) != 0
        || (autoc = index(autocomplete, ':')) != 0) {
        *autoc++ = '\0';
        parseautocomplete(autoc, condition);
    }

    /* strip leading and trailing white space */
    autocomplete = trimspaces(autocomplete);

    if (!*autocomplete)
        return;

    /* take off negation */
    if (*autocomplete == '!') {
        /* unlike most options, a leading "no" might actually be a part of
         * the extended command.  Thus you have to use ! */
        autocomplete++;
        autocomplete = trimspaces(autocomplete);
        condition = !condition;
    }

    /* find and modify the extended command */
    for (efp = extcmdlist; efp->ef_txt; efp++) {
        if (!strcmp(autocomplete, efp->ef_txt)) {
            if (condition)
                efp->flags |= AUTOCOMPLETE;
            else
                efp->flags &= ~AUTOCOMPLETE;
            return;
        }
    }

    /* not a real extended command */
    raw_printf("Bad autocomplete: invalid extended command '%s'.",
               autocomplete);
    wait_synch();
}

/* called at startup and after number_pad is twiddled */
void
reset_commands(initial)
boolean initial;
{
    static const char sdir[] = "hykulnjb><",
                      sdir_swap_yz[] = "hzkulnjb><",
                      ndir[] = "47896321><",
                      ndir_phone_layout[] = "41236987><";
    static const int ylist[] = {
        'y', 'Y', C('y'), M('y'), M('Y'), M(C('y'))
    };
    static struct ext_func_tab *back_dir_cmd[8];
    const struct ext_func_tab *cmdtmp;
    boolean flagtemp;
    int c, i, updated = 0;
    static boolean backed_dir_cmd = FALSE;

    if (initial) {
        updated = 1;
        Cmd.num_pad = FALSE;
        Cmd.pcHack_compat = Cmd.phone_layout = Cmd.swap_yz = FALSE;
        for (i = 0; i < SIZE(spkeys_binds); i++)
            Cmd.spkeys[spkeys_binds[i].nhkf] = spkeys_binds[i].key;
        commands_init();
    } else {

        if (backed_dir_cmd) {
            for (i = 0; i < 8; i++) {
                Cmd.commands[(uchar) Cmd.dirchars[i]] = back_dir_cmd[i];
            }
        }

        /* basic num_pad */
        flagtemp = iflags.num_pad;
        if (flagtemp != Cmd.num_pad) {
            Cmd.num_pad = flagtemp;
            ++updated;
        }
        /* swap_yz mode (only applicable for !num_pad); intended for
           QWERTZ keyboard used in Central Europe, particularly Germany */
        flagtemp = (iflags.num_pad_mode & 1) ? !Cmd.num_pad : FALSE;
        if (flagtemp != Cmd.swap_yz) {
            Cmd.swap_yz = flagtemp;
            ++updated;
            /* Cmd.swap_yz has been toggled;
               perform the swap (or reverse previous one) */
            for (i = 0; i < SIZE(ylist); i++) {
                c = ylist[i] & 0xff;
                cmdtmp = Cmd.commands[c];              /* tmp = [y] */
                Cmd.commands[c] = Cmd.commands[c + 1]; /* [y] = [z] */
                Cmd.commands[c + 1] = cmdtmp;          /* [z] = tmp */
            }
        }
        /* MSDOS compatibility mode (only applicable for num_pad) */
        flagtemp = (iflags.num_pad_mode & 1) ? Cmd.num_pad : FALSE;
        if (flagtemp != Cmd.pcHack_compat) {
            Cmd.pcHack_compat = flagtemp;
            ++updated;
            /* pcHack_compat has been toggled */
            c = M('5') & 0xff;
            cmdtmp = Cmd.commands['5'];
            Cmd.commands['5'] = Cmd.commands[c];
            Cmd.commands[c] = cmdtmp;
            c = M('0') & 0xff;
            Cmd.commands[c] = Cmd.pcHack_compat ? Cmd.commands['I'] : 0;
        }
        /* phone keypad layout (only applicable for num_pad) */
        flagtemp = (iflags.num_pad_mode & 2) ? Cmd.num_pad : FALSE;
        if (flagtemp != Cmd.phone_layout) {
            Cmd.phone_layout = flagtemp;
            ++updated;
            /* phone_layout has been toggled */
            for (i = 0; i < 3; i++) {
                c = '1' + i;             /* 1,2,3 <-> 7,8,9 */
                cmdtmp = Cmd.commands[c];              /* tmp = [1] */
                Cmd.commands[c] = Cmd.commands[c + 6]; /* [1] = [7] */
                Cmd.commands[c + 6] = cmdtmp;          /* [7] = tmp */
                c = (M('1') & 0xff) + i; /* M-1,M-2,M-3 <-> M-7,M-8,M-9 */
                cmdtmp = Cmd.commands[c];              /* tmp = [M-1] */
                Cmd.commands[c] = Cmd.commands[c + 6]; /* [M-1] = [M-7] */
                Cmd.commands[c + 6] = cmdtmp;          /* [M-7] = tmp */
            }
        }
    } /*?initial*/

    if (updated)
        Cmd.serialno++;
    Cmd.dirchars = !Cmd.num_pad
                       ? (!Cmd.swap_yz ? sdir : sdir_swap_yz)
                       : (!Cmd.phone_layout ? ndir : ndir_phone_layout);
    Cmd.alphadirchars = !Cmd.num_pad ? Cmd.dirchars : sdir;

    Cmd.move_W = Cmd.dirchars[0];
    Cmd.move_NW = Cmd.dirchars[1];
    Cmd.move_N = Cmd.dirchars[2];
    Cmd.move_NE = Cmd.dirchars[3];
    Cmd.move_E = Cmd.dirchars[4];
    Cmd.move_SE = Cmd.dirchars[5];
    Cmd.move_S = Cmd.dirchars[6];
    Cmd.move_SW = Cmd.dirchars[7];

    if (!initial) {
        for (i = 0; i < 8; i++) {
            back_dir_cmd[i] =
                (struct ext_func_tab *) Cmd.commands[(uchar) Cmd.dirchars[i]];
            Cmd.commands[(uchar) Cmd.dirchars[i]] = (struct ext_func_tab *) 0;
        }
        backed_dir_cmd = TRUE;
        for (i = 0; i < 8; i++)
            (void) bind_key(Cmd.dirchars[i], "nothing");
    }
}

/* non-movement commands which accept 'm' prefix to request menu operation */
STATIC_OVL boolean
accept_menu_prefix(cmd_func)
int NDECL((*cmd_func));
{
    if (cmd_func == dopickup || cmd_func == dotip
        /* eat, #offer, and apply tinning-kit all use floorfood() to pick
           an item on floor or in invent; 'm' skips picking from floor
           (ie, inventory only) rather than request use of menu operation */
        || cmd_func == doeat || cmd_func == dosacrifice || cmd_func == doapply
        /* 'm' for removing saddle from adjacent monster without checking
           for containers at <u.ux,u.uy> */
        || cmd_func == doloot
        /* travel: pop up a menu of interesting targets in view */
        || cmd_func == dotravel
        /* wizard mode ^V and ^T */
        || cmd_func == wiz_level_tele || cmd_func == dotelecmd
        /* 'm' prefix allowed for some extended commands */
        || cmd_func == doextcmd || cmd_func == doextlist)
        return TRUE;
    return FALSE;
}

char
randomkey()
{
    static unsigned i = 0;
    char c;

    switch (rn2(16)) {
    default:
        c = '\033';
        break;
    case 0:
        c = '\n';
        break;
    case 1:
    case 2:
    case 3:
    case 4:
        c = (char) rn1('~' - ' ' + 1, ' ');
        break;
    case 5:
        c = (char) (rn2(2) ? '\t' : ' ');
        break;
    case 6:
        c = (char) rn1('z' - 'a' + 1, 'a');
        break;
    case 7:
        c = (char) rn1('Z' - 'A' + 1, 'A');
        break;
    case 8:
        c = extcmdlist[i++ % SIZE(extcmdlist)].key;
        break;
    case 9:
        c = '#';
        break;
    case 10:
    case 11:
    case 12:
        c = Cmd.dirchars[rn2(8)];
        if (!rn2(7))
            c = !Cmd.num_pad ? (!rn2(3) ? C(c) : (c + 'A' - 'a')) : M(c);
        break;
    case 13:
        c = (char) rn1('9' - '0' + 1, '0');
        break;
    case 14:
        /* any char, but avoid '\0' because it's used for mouse click */
        c = (char) rnd(iflags.wc_eight_bit_input ? 255 : 127);
        break;
    }

    return c;
}

void
random_response(buf, sz)
char *buf;
int sz;
{
    char c;
    int count = 0;

    for (;;) {
        c = randomkey();
        if (c == '\n')
            break;
        if (c == '\033') {
            count = 0;
            break;
        }
        if (count < sz - 1)
            buf[count++] = c;
    }
    buf[count] = '\0';
}

int
rnd_extcmd_idx(VOID_ARGS)
{
    return rn2(extcmdlist_length + 1) - 1;
}

int
ch2spkeys(c, start, end)
char c;
int start,end;
{
    int i;

    for (i = start; i <= end; i++)
        if (Cmd.spkeys[i] == c)
            return i;
    return NHKF_ESC;
}

void
rhack(cmd)
register char *cmd;
{
    int spkey;
    boolean prefix_seen, bad_command,
        firsttime = (cmd == 0);

    iflags.menu_requested = FALSE;
#ifdef SAFERHANGUP
    if (program_state.done_hup)
        end_of_input();
#endif
    if (firsttime) {
        context.nopick = 0;
        cmd = parse();
    }
    if (*cmd == Cmd.spkeys[NHKF_ESC]) {
        context.move = FALSE;
        return;
    }
    if (*cmd == DOAGAIN && !in_doagain && saveq[0]) {
        in_doagain = TRUE;
        stail = 0;
        rhack((char *) 0); /* read and execute command */
        in_doagain = FALSE;
        return;
    }
    /* Special case of *cmd == ' ' handled better below */
    if (!*cmd || *cmd == (char) 0377) {
        nhbell();
        context.move = FALSE;
        return; /* probably we just had an interrupt */
    }

    /* handle most movement commands */
    prefix_seen = FALSE;
    context.travel = context.travel1 = 0;
    spkey = ch2spkeys(*cmd, NHKF_RUN, NHKF_CLICKLOOK);

    switch (spkey) {
    case NHKF_RUSH:
        if (movecmd(cmd[1])) {
            context.run = 2;
            domove_attempting |= DOMOVE_RUSH;
        } else
            prefix_seen = TRUE;
        break;
    case NHKF_RUN2:
        if (!Cmd.num_pad)
            break;
        /*FALLTHRU*/
    case NHKF_RUN:
        if (movecmd(lowc(cmd[1]))) {
            context.run = 3;
            domove_attempting |= DOMOVE_RUSH;
        } else
            prefix_seen = TRUE;
        break;
    case NHKF_FIGHT2:
        if (!Cmd.num_pad)
            break;
        /*FALLTHRU*/
    /* Effects of movement commands and invisible monsters:
     * m: always move onto space (even if 'I' remembered)
     * F: always attack space (even if 'I' not remembered)
     * normal movement: attack if 'I', move otherwise.
     */
    case NHKF_FIGHT:
        if (movecmd(cmd[1])) {
            context.forcefight = 1;
            domove_attempting |= DOMOVE_WALK;
        } else
            prefix_seen = TRUE;
        break;
    case NHKF_NOPICKUP:
        if (movecmd(cmd[1]) || u.dz) {
            context.run = 0;
            context.nopick = 1;
            if (!u.dz)
                domove_attempting |= DOMOVE_WALK;
            else
                cmd[0] = cmd[1]; /* "m<" or "m>" */
        } else
            prefix_seen = TRUE;
        break;
    case NHKF_RUN_NOPICKUP:
        if (movecmd(lowc(cmd[1]))) {
            context.run = 1;
            context.nopick = 1;
            domove_attempting |= DOMOVE_RUSH;
        } else
            prefix_seen = TRUE;
        break;
    case NHKF_DOINV:
        if (!Cmd.num_pad)
            break;
        (void) ddoinv(); /* a convenience borrowed from the PC */
        context.move = FALSE;
        multi = 0;
        return;
    case NHKF_CLICKLOOK:
        if (iflags.clicklook) {
            context.move = FALSE;
            do_look(2, &clicklook_cc);
        }
        return;
    case NHKF_TRAVEL:
        if (flags.travelcmd) {
            context.travel = 1;
            context.travel1 = 1;
            context.run = 8;
            context.nopick = 1;
            domove_attempting |= DOMOVE_RUSH;
            break;
        }
        /*FALLTHRU*/
    default:
        if (movecmd(*cmd)) { /* ordinary movement */
            context.run = 0; /* only matters here if it was 8 */
            domove_attempting |= DOMOVE_WALK;
        } else if (movecmd(Cmd.num_pad ? unmeta(*cmd) : lowc(*cmd))) {
            context.run = 1;
            domove_attempting |= DOMOVE_RUSH;
        } else if (movecmd(unctrl(*cmd))) {
            context.run = 3;
            domove_attempting |= DOMOVE_RUSH;
        }
        break;
    }

    /* some special prefix handling */
    /* overload 'm' prefix to mean "request a menu" */
    if (prefix_seen && cmd[0] == Cmd.spkeys[NHKF_REQMENU]) {
        /* (for func_tab cast, see below) */
        const struct ext_func_tab *ft = Cmd.commands[cmd[1] & 0xff];
        int NDECL((*func)) = ft ? ((struct ext_func_tab *) ft)->ef_funct : 0;

        if (func && accept_menu_prefix(func)) {
            iflags.menu_requested = TRUE;
            ++cmd;
        }
    }

    if (((domove_attempting & (DOMOVE_RUSH | DOMOVE_WALK)) != 0L)
                            && !context.travel && !dxdy_moveok()) {
        /* trying to move diagonally as a grid bug;
           this used to be treated by movecmd() as not being
           a movement attempt, but that didn't provide for any
           feedback and led to strangeness if the key pressed
           ('u' in particular) was overloaded for num_pad use */
        You_cant("从这里到那里...");
        context.run = 0;
        context.nopick = context.forcefight = FALSE;
        context.move = context.mv = FALSE;
        multi = 0;
        return;
    }

    if ((domove_attempting & DOMOVE_WALK) != 0L) {
        if (multi)
            context.mv = TRUE;
        domove();
        context.forcefight = 0;
        return;
    } else if ((domove_attempting & DOMOVE_RUSH) != 0L) {
        if (firsttime) {
            if (!multi)
                multi = max(COLNO, ROWNO);
            u.last_str_turn = 0;
        }
        context.mv = TRUE;
        domove();
        return;
    } else if (prefix_seen && cmd[1] == Cmd.spkeys[NHKF_ESC]) {
        /* <prefix><escape> */
        /* don't report "unknown command" for change of heart... */
        bad_command = FALSE;
    } else if (*cmd == ' ' && !flags.rest_on_space) {
        bad_command = TRUE; /* skip cmdlist[] loop */

    /* handle all other commands */
    } else {
        register const struct ext_func_tab *tlist;
        int res, NDECL((*func));

        /* current - use *cmd to directly index cmdlist array */
        if ((tlist = Cmd.commands[*cmd & 0xff]) != 0) {
            if (!wizard && (tlist->flags & WIZMODECMD)) {
                You_cant("做那个!");
                res = 0;
            } else if (u.uburied && !(tlist->flags & IFBURIED)) {
                You_cant("在你被埋葬的时候做那个!");
                res = 0;
            } else {
                /* we discard 'const' because some compilers seem to have
                   trouble with the pointer passed to set_occupation() */
                func = ((struct ext_func_tab *) tlist)->ef_funct;
                if (tlist->f_text && !occupation && multi)
                    set_occupation(func, tlist->f_text, multi);
                res = (*func)(); /* perform the command */
            }
            if (!res) {
                context.move = FALSE;
                multi = 0;
            }
            return;
        }
        /* if we reach here, cmd wasn't found in cmdlist[] */
        bad_command = TRUE;
    }

    if (bad_command) {
        char expcmd[20]; /* we expect 'cmd' to point to 1 or 2 chars */
        char c, c1 = cmd[1];

        expcmd[0] = '\0';
        while ((c = *cmd++) != '\0')
            Strcat(expcmd, visctrl(c)); /* add 1..4 chars plus terminator */

        if (!prefix_seen || !help_dir(c1, spkey, "无效的方向键!"))
            Norep("未知命令 '%s'.", expcmd);
    }
    /* didn't move */
    context.move = FALSE;
    multi = 0;
    return;
}

/* convert an x,y pair into a direction code */
int
xytod(x, y)
schar x, y;
{
    register int dd;

    for (dd = 0; dd < 8; dd++)
        if (x == xdir[dd] && y == ydir[dd])
            return dd;
    return -1;
}

/* convert a direction code into an x,y pair */
void
dtoxy(cc, dd)
coord *cc;
register int dd;
{
    cc->x = xdir[dd];
    cc->y = ydir[dd];
    return;
}

/* also sets u.dz, but returns false for <> */
int
movecmd(sym)
char sym;
{
    register const char *dp = index(Cmd.dirchars, sym);

    u.dz = 0;
    if (!dp || !*dp)
        return 0;
    u.dx = xdir[dp - Cmd.dirchars];
    u.dy = ydir[dp - Cmd.dirchars];
    u.dz = zdir[dp - Cmd.dirchars];
#if 0 /* now handled elsewhere */
    if (u.dx && u.dy && NODIAG(u.umonnum)) {
        u.dx = u.dy = 0;
        return 0;
    }
#endif
    return !u.dz;
}

/* grid bug handling which used to be in movecmd() */
int
dxdy_moveok()
{
    if (u.dx && u.dy && NODIAG(u.umonnum))
        u.dx = u.dy = 0;
    return u.dx || u.dy;
}

/* decide whether a character (user input keystroke) requests screen repaint */
boolean
redraw_cmd(c)
char c;
{
    return (boolean) (c == Cmd.spkeys[NHKF_REDRAW]
                      || (Cmd.num_pad && c == Cmd.spkeys[NHKF_REDRAW2]));
}

boolean
prefix_cmd(c)
char c;
{
    return (c == Cmd.spkeys[NHKF_RUSH]
            || c == Cmd.spkeys[NHKF_RUN]
            || c == Cmd.spkeys[NHKF_NOPICKUP]
            || c == Cmd.spkeys[NHKF_RUN_NOPICKUP]
            || c == Cmd.spkeys[NHKF_FIGHT]
            || (Cmd.num_pad && (c == Cmd.spkeys[NHKF_RUN2]
                                || c == Cmd.spkeys[NHKF_FIGHT2])));
}

/*
 * uses getdir() but unlike getdir() it specifically
 * produces coordinates using the direction from getdir()
 * and verifies that those coordinates are ok.
 *
 * If the call to getdir() returns 0, Never_mind is displayed.
 * If the resulting coordinates are not okay, emsg is displayed.
 *
 * Returns non-zero if coordinates in cc are valid.
 */
int
get_adjacent_loc(prompt, emsg, x, y, cc)
const char *prompt, *emsg;
xchar x, y;
coord *cc;
{
    xchar new_x, new_y;
    if (!getdir(prompt)) {
        pline1(Never_mind);
        return 0;
    }
    new_x = x + u.dx;
    new_y = y + u.dy;
    if (cc && isok(new_x, new_y)) {
        cc->x = new_x;
        cc->y = new_y;
    } else {
        if (emsg)
            pline1(emsg);
        return 0;
    }
    return 1;
}

int
getdir(s)
const char *s;
{
    char dirsym;
    int is_mov;

 retry:
    if (in_doagain || *readchar_queue)
        dirsym = readchar();
    else
        dirsym = yn_function((s && *s != '^') ? s : "哪个方向?",
                             (char *) 0, '\0');
    /* remove the prompt string so caller won't have to */
    clear_nhwindow(WIN_MESSAGE);

    if (redraw_cmd(dirsym)) { /* ^R */
        docrt();              /* redraw */
        goto retry;
    }
    savech(dirsym);

    if (dirsym == Cmd.spkeys[NHKF_GETDIR_SELF]
        || dirsym == Cmd.spkeys[NHKF_GETDIR_SELF2]) {
        u.dx = u.dy = u.dz = 0;
    } else if (!(is_mov = movecmd(dirsym)) && !u.dz) {
        boolean did_help = FALSE, help_requested;

        if (!index(quitchars, dirsym)) {
            help_requested = (dirsym == Cmd.spkeys[NHKF_GETDIR_HELP]);
            if (help_requested || iflags.cmdassist) {
                did_help = help_dir((s && *s == '^') ? dirsym : '\0',
                                    NHKF_ESC,
                                    help_requested ? (const char *) 0
                                                  : "无效的方向键!");
                if (help_requested)
                    goto retry;
            }
            if (!did_help)
                pline("好奇怪的方向!");
        }
        return 0;
    } else if (is_mov && !dxdy_moveok()) {
        You_cant("使自己朝向那个方向.");
        return 0;
    }
    if (!u.dz && (Stunned || (Confusion && !rn2(5))))
        confdir();
    return 1;
}

STATIC_OVL void
show_direction_keys(win, centerchar, nodiag)
winid win; /* should specify a window which is using a fixed-width font... */
char centerchar; /* '.' or '@' or ' ' */
boolean nodiag;
{
    char buf[BUFSZ];

    if (!centerchar)
        centerchar = ' ';

    if (nodiag) {
        Sprintf(buf, "             %c   ", Cmd.move_N);
        putstr(win, 0, buf);
        putstr(win, 0, "             |   ");
        Sprintf(buf, "          %c- %c -%c",
                Cmd.move_W, centerchar, Cmd.move_E);
        putstr(win, 0, buf);
        putstr(win, 0, "             |   ");
        Sprintf(buf, "             %c   ", Cmd.move_S);
        putstr(win, 0, buf);
    } else {
        Sprintf(buf, "          %c  %c  %c",
                Cmd.move_NW, Cmd.move_N, Cmd.move_NE);
        putstr(win, 0, buf);
        putstr(win, 0, "           \\ | / ");
        Sprintf(buf, "          %c- %c -%c",
                Cmd.move_W, centerchar, Cmd.move_E);
        putstr(win, 0, buf);
        putstr(win, 0, "           / | \\ ");
        Sprintf(buf, "          %c  %c  %c",
                Cmd.move_SW, Cmd.move_S, Cmd.move_SE);
        putstr(win, 0, buf);
    };
}

/* explain choices if player has asked for getdir() help or has given
   an invalid direction after a prefix key ('F', 'g', 'm', &c), which
   might be bogus but could be up, down, or self when not applicable */
STATIC_OVL boolean
help_dir(sym, spkey, msg)
char sym;
int spkey; /* NHKF_ code for prefix key, if one was used, or for ESC */
const char *msg;
{
    static const char wiz_only_list[] = "EFGIVW";
    char ctrl;
    winid win;
    char buf[BUFSZ], buf2[BUFSZ], *explain;
    const char *dothat, *how;
    boolean prefixhandling, viawindow;

    /* NHKF_ESC indicates that player asked for help at getdir prompt */
    viawindow = (spkey == NHKF_ESC || iflags.cmdassist);
    prefixhandling = (spkey != NHKF_ESC);
    /*
     * Handling for prefix keys that don't want special directions.
     * Delivered via pline if 'cmdassist' is off, or instead of the
     * general message if it's on.
     */
    dothat = "做那个";
    how = " at"; /* for "<action> at yourself"; not used for up/down */
    switch (spkey) {
    case NHKF_NOPICKUP:
        dothat = "移动";
        break;
    case NHKF_RUSH:
        dothat = "迅速移动";
        break;
    case NHKF_RUN2:
        if (!Cmd.num_pad)
            break;
        /*FALLTHRU*/
    case NHKF_RUN:
    case NHKF_RUN_NOPICKUP:
        dothat = "跑";
        break;
    case NHKF_FIGHT2:
        if (!Cmd.num_pad)
            break;
        /*FALLTHRU*/
    case NHKF_FIGHT:
        dothat = "战斗";
        how = ""; /* avoid "fight at yourself" */
        break;
    default:
        prefixhandling = FALSE;
        break;
    }

    buf[0] = '\0';
    /* for movement prefix followed by '.' or (numpad && 's') to mean 'self';
       note: '-' for hands (inventory form of 'self') is not handled here */
    if (prefixhandling
        && (sym == Cmd.spkeys[NHKF_GETDIR_SELF]
            || (Cmd.num_pad && sym == Cmd.spkeys[NHKF_GETDIR_SELF2]))) {
        Sprintf(buf, "你不能向自己%s%s.", dothat, how);
    /* for movement prefix followed by up or down */
    } else if (prefixhandling && (sym == '<' || sym == '>')) {
        Sprintf(buf, "你不能%s %s.",
                /* was "upwards" and "downwards", but they're considered
                   to be variants of canonical "upward" and "downward" */
                (sym == '<') ? "往上" : "往下", dothat);
    }

    /* if '!cmdassist', display via pline() and we're done (note: asking
       for help at getdir() prompt forces cmdassist for this operation) */
    if (!viawindow) {
        if (prefixhandling) {
            if (!*buf)
                Sprintf(buf, "无效的方向前缀'%s'.",
                        visctrl(Cmd.spkeys[spkey]));
            pline("%s", buf);
            return TRUE;
        }
        /* when 'cmdassist' is off and caller doesn't insist, do nothing */
        return FALSE;
    }

    win = create_nhwindow(NHW_TEXT);
    if (!win)
        return FALSE;

    if (*buf) {
        /* show bad-prefix message instead of general invalid-direction one */
        putstr(win, 0, buf);
        putstr(win, 0, "");
    } else if (msg) {
        Sprintf(buf, "命令助手: %s", msg);
        putstr(win, 0, buf);
        putstr(win, 0, "");
    }

    if (!prefixhandling && (letter(sym) || sym == '[')) {
        /* '[': old 'cmdhelp' showed ESC as ^[ */
        sym = highc(sym); /* @A-Z[ (note: letter() accepts '@') */
        ctrl = (sym - 'A') + 1; /* 0-27 (note: 28-31 aren't applicable) */
        if ((explain = dowhatdoes_core(ctrl, buf2)) != 0
            && (!index(wiz_only_list, sym) || wizard)) {
            Sprintf(buf, "你是想%s使用^%c 吗?",
                    index(wiz_only_list, sym) ? ""
                        : "按Guidebook 上的规定", sym);
            putstr(win, 0, buf);
            putstr(win, 0, "");
            putstr(win, 0, explain);
            putstr(win, 0, "");
            putstr(win, 0,
                  "要使用那个命令, 按住<Ctrl> 键当做shift");
            Sprintf(buf, "并按<%c> 键.", sym);
            putstr(win, 0, buf);
            putstr(win, 0, "");
        }
    }

    Sprintf(buf, "%s%s%s的有效的方向键是:",
            NODIAG(u.umonnum) ? "在你当前的外貌下" : "",
            prefixhandling ? "来" : "", prefixhandling ? dothat : "");
    putstr(win, 0, buf);
    show_direction_keys(win, !prefixhandling ? '.' : ' ', NODIAG(u.umonnum));

    if (!prefixhandling || spkey == NHKF_NOPICKUP) {
        /* NOPICKUP: unlike the other prefix keys, 'm' allows up/down for
           stair traversal; we won't get here when "m<" or "m>" has been
           given but we include up and down for 'm'+invalid_direction;
           self is excluded as a viable direction for every prefix */
        putstr(win, 0, "");
        putstr(win, 0, "          <  上");
        putstr(win, 0, "          >  下");
        if (!prefixhandling) {
            int selfi = Cmd.num_pad ? NHKF_GETDIR_SELF2 : NHKF_GETDIR_SELF;

            Sprintf(buf,   "       %4s  你自己的位置",
                    visctrl(Cmd.spkeys[selfi]));
            putstr(win, 0, buf);
        }
    }

    if (msg) {
        /* non-null msg means that this wasn't an explicit user request */
        putstr(win, 0, "");
        putstr(win, 0,
               "(在配置文件中用!cmdassist来禁止此消息.)");
    }
    display_nhwindow(win, FALSE);
    destroy_nhwindow(win);
    return TRUE;
}

void
confdir()
{
    register int x = NODIAG(u.umonnum) ? 2 * rn2(4) : rn2(8);

    u.dx = xdir[x];
    u.dy = ydir[x];
    return;
}

const char *
directionname(dir)
int dir;
{
    static NEARDATA const char *const dirnames[] = {
        "西",      "西北", "北",     "东北", "东",
        "东南", "南",     "西南", "下",      "上",
    };

    if (dir < 0 || dir >= SIZE(dirnames))
        return "invalid";
    return dirnames[dir];
}

int
isok(x, y)
register int x, y;
{
    /* x corresponds to curx, so x==1 is the first column. Ach. %% */
    return x >= 1 && x <= COLNO - 1 && y >= 0 && y <= ROWNO - 1;
}

/* #herecmdmenu command */
STATIC_PTR int
doherecmdmenu(VOID_ARGS)
{
    char ch = here_cmd_menu(TRUE);

    return ch ? 1 : 0;
}

/* #therecmdmenu command, a way to test there_cmd_menu without mouse */
STATIC_PTR int
dotherecmdmenu(VOID_ARGS)
{
    char ch;

    if (!getdir((const char *) 0) || !isok(u.ux + u.dx, u.uy + u.dy))
        return 0;

    if (u.dx || u.dy)
        ch = there_cmd_menu(TRUE, u.ux + u.dx, u.uy + u.dy);
    else
        ch = here_cmd_menu(TRUE);

    return ch ? 1 : 0;
}

STATIC_OVL void
add_herecmd_menuitem(win, func, text)
winid win;
int NDECL((*func));
const char *text;
{
    char ch;
    anything any;

    if ((ch = cmd_from_func(func)) != '\0') {
        any = zeroany;
        any.a_nfunc = func;
        add_menu(win, NO_GLYPH, &any, 0, 0, ATR_NONE, text, MENU_UNSELECTED);
    }
}

STATIC_OVL char
there_cmd_menu(doit, x, y)
boolean doit;
int x, y;
{
    winid win;
    char ch;
    char buf[BUFSZ];
    schar typ = levl[x][y].typ;
    int npick, K = 0;
    menu_item *picks = (menu_item *) 0;
    struct trap *ttmp;
    struct monst *mtmp;

    win = create_nhwindow(NHW_MENU);
    start_menu(win);

    if (IS_DOOR(typ)) {
        boolean key_or_pick, card;
        int dm = levl[x][y].doormask;

        if ((dm & (D_CLOSED | D_LOCKED))) {
            add_herecmd_menuitem(win, doopen, "开门"), ++K;
            /* unfortunately there's no lknown flag for doors to
               remember the locked/unlocked state */
            key_or_pick = (carrying(SKELETON_KEY) || carrying(LOCK_PICK));
            card = (carrying(CREDIT_CARD) != 0);
            if (key_or_pick || card) {
                Sprintf(buf, "%s解锁门",
                        key_or_pick ? "上锁或者" : "");
                add_herecmd_menuitem(win, doapply, upstart(buf)), ++K;
            }
            /* unfortunately there's no tknown flag for doors (or chests)
               to remember whether a trap had been found */
            add_herecmd_menuitem(win, dountrap,
                                 "搜索门是否有陷阱"), ++K;
            /* [what about #force?] */
            add_herecmd_menuitem(win, dokick, "踢门"), ++K;
        } else if ((dm & D_ISOPEN)) {
            add_herecmd_menuitem(win, doclose, "关门"), ++K;
        }
    }

    if (typ <= SCORR)
        add_herecmd_menuitem(win, dosearch, "搜索暗门"), ++K;

    if ((ttmp = t_at(x, y)) != 0 && ttmp->tseen) {
        add_herecmd_menuitem(win, doidtrap, "检查陷阱"), ++K;
        if (ttmp->ttyp != VIBRATING_SQUARE)
            add_herecmd_menuitem(win, dountrap, "试图解除陷阱"), ++K;
    }

    mtmp = m_at(x, y);
    if (mtmp && !canspotmon(mtmp))
        mtmp = 0;
    if (mtmp && which_armor(mtmp, W_SADDLE)) {
        char *mnam = x_monnam(mtmp, ARTICLE_THE, (char *) 0,
                              SUPPRESS_SADDLE, FALSE);

        if (!u.usteed) {
            Sprintf(buf, "乘骑%s", mnam);
            add_herecmd_menuitem(win, doride, buf), ++K;
        }
        Sprintf(buf, "拿下%s 的鞍", mnam);
        add_herecmd_menuitem(win, doloot, buf), ++K;
    }
    if (mtmp && can_saddle(mtmp) && !which_armor(mtmp, W_SADDLE)
        && carrying(SADDLE)) {
        Sprintf(buf, "把鞍放上%s", mon_nam(mtmp)), ++K;
        add_herecmd_menuitem(win, doapply, buf);
    }
#if 0
    if (mtmp || glyph_is_invisible(glyph_at(x, y))) {
        /* "Attack %s", mtmp ? mon_nam(mtmp) : "unseen creature" */
    } else {
        /* "Move %s", direction */
    }
#endif

    if (K) {
        end_menu(win, "你想做什么?");
        npick = select_menu(win, PICK_ONE, &picks);
    } else {
        pline("没有适用的行为.");
        npick = 0;
    }
    destroy_nhwindow(win);
    ch = '\0';
    if (npick > 0) {
        int NDECL((*func)) = picks->item.a_nfunc;
        free((genericptr_t) picks);

        if (doit) {
            int ret = (*func)();

            ch = (char) ret;
        } else {
            ch = cmd_from_func(func);
        }
    }
    return ch;
}

STATIC_OVL char
here_cmd_menu(doit)
boolean doit;
{
    winid win;
    char ch;
    char buf[BUFSZ];
    schar typ = levl[u.ux][u.uy].typ;
    int npick;
    menu_item *picks = (menu_item *) 0;

    win = create_nhwindow(NHW_MENU);
    start_menu(win);

    if (IS_FOUNTAIN(typ) || IS_SINK(typ)) {
        Sprintf(buf, "喝%s",
                defsyms[IS_FOUNTAIN(typ) ? S_fountain : S_sink].explanation);
        add_herecmd_menuitem(win, dodrink, buf);
    }
    if (IS_FOUNTAIN(typ))
        add_herecmd_menuitem(win, dodip,
                             "把某物浸到喷泉里");
    if (IS_THRONE(typ))
        add_herecmd_menuitem(win, dosit,
                             "坐到王座上");

    if ((u.ux == xupstair && u.uy == yupstair)
        || (u.ux == sstairs.sx && u.uy == sstairs.sy && sstairs.up)
        || (u.ux == xupladder && u.uy == yupladder)) {
        Sprintf(buf, "上%s",
                (u.ux == xupladder && u.uy == yupladder)
                ? "梯子" : "楼梯");
        add_herecmd_menuitem(win, doup, buf);
    }
    if ((u.ux == xdnstair && u.uy == ydnstair)
        || (u.ux == sstairs.sx && u.uy == sstairs.sy && !sstairs.up)
        || (u.ux == xdnladder && u.uy == ydnladder)) {
        Sprintf(buf, "下%s",
                (u.ux == xupladder && u.uy == yupladder)
                ? "梯子" : "楼梯");
        add_herecmd_menuitem(win, dodown, buf);
    }
    if (u.usteed) { /* another movement choice */
        Sprintf(buf, "下乘骑的%s",
                x_monnam(u.usteed, ARTICLE_THE, (char *) 0,
                         SUPPRESS_SADDLE, FALSE));
        add_herecmd_menuitem(win, doride, buf);
    }

#if 0
    if (Upolyd) { /* before objects */
        Sprintf(buf, "Use %s special ability",
                s_suffix(mons[u.umonnum].mname));
        add_herecmd_menuitem(win, domonability, buf);
    }
#endif

    if (OBJ_AT(u.ux, u.uy)) {
        struct obj *otmp = level.objects[u.ux][u.uy];

        Sprintf(buf, "捡取%s", otmp->nexthere ? "物品" : doname(otmp));
        add_herecmd_menuitem(win, dopickup, buf);

        if (Is_container(otmp)) {
            Sprintf(buf, "搜刮%s", doname(otmp));
            add_herecmd_menuitem(win, doloot, buf);
        }
        if (otmp->oclass == FOOD_CLASS) {
            Sprintf(buf, "吃%s", doname(otmp));
            add_herecmd_menuitem(win, doeat, buf);
        }
    }

    if (invent)
        add_herecmd_menuitem(win, dodrop, "扔物品");

    add_herecmd_menuitem(win, donull, "休息一回");
    add_herecmd_menuitem(win, dosearch, "搜索你周围");
    add_herecmd_menuitem(win, dolook, "查看这里有什么");

    end_menu(win, "你想做什么?");
    npick = select_menu(win, PICK_ONE, &picks);
    destroy_nhwindow(win);
    ch = '\0';
    if (npick > 0) {
        int NDECL((*func)) = picks->item.a_nfunc;
        free((genericptr_t) picks);

        if (doit) {
            int ret = (*func)();

            ch = (char) ret;
        } else {
            ch = cmd_from_func(func);
        }
    }
    return ch;
}


static NEARDATA int last_multi;

/*
 * convert a MAP window position into a movecmd
 */
const char *
click_to_cmd(x, y, mod)
int x, y, mod;
{
    int dir;
    static char cmd[4];
    cmd[1] = 0;

    if (iflags.clicklook && mod == CLICK_2) {
        clicklook_cc.x = x;
        clicklook_cc.y = y;
        cmd[0] = Cmd.spkeys[NHKF_CLICKLOOK];
        return cmd;
    }

    x -= u.ux;
    y -= u.uy;

    if (flags.travelcmd) {
        if (abs(x) <= 1 && abs(y) <= 1) {
            x = sgn(x), y = sgn(y);
        } else {
            u.tx = u.ux + x;
            u.ty = u.uy + y;
            cmd[0] = Cmd.spkeys[NHKF_TRAVEL];
            return cmd;
        }

        if (x == 0 && y == 0) {
            if (iflags.herecmd_menu) {
                cmd[0] = here_cmd_menu(FALSE);
                return cmd;
            }

            /* here */
            if (IS_FOUNTAIN(levl[u.ux][u.uy].typ)
                || IS_SINK(levl[u.ux][u.uy].typ)) {
                cmd[0] = cmd_from_func(mod == CLICK_1 ? dodrink : dodip);
                return cmd;
            } else if (IS_THRONE(levl[u.ux][u.uy].typ)) {
                cmd[0] = cmd_from_func(dosit);
                return cmd;
            } else if ((u.ux == xupstair && u.uy == yupstair)
                       || (u.ux == sstairs.sx && u.uy == sstairs.sy
                           && sstairs.up)
                       || (u.ux == xupladder && u.uy == yupladder)) {
                cmd[0] = cmd_from_func(doup);
                return cmd;
            } else if ((u.ux == xdnstair && u.uy == ydnstair)
                       || (u.ux == sstairs.sx && u.uy == sstairs.sy
                           && !sstairs.up)
                       || (u.ux == xdnladder && u.uy == ydnladder)) {
                cmd[0] = cmd_from_func(dodown);
                return cmd;
            } else if (OBJ_AT(u.ux, u.uy)) {
                cmd[0] = cmd_from_func(Is_container(level.objects[u.ux][u.uy])
                                       ? doloot : dopickup);
                return cmd;
            } else {
                cmd[0] = cmd_from_func(donull); /* just rest */
                return cmd;
            }
        }

        /* directional commands */

        dir = xytod(x, y);

        if (!m_at(u.ux + x, u.uy + y)
            && !test_move(u.ux, u.uy, x, y, TEST_MOVE)) {
            cmd[1] = Cmd.dirchars[dir];
            cmd[2] = '\0';
            if (iflags.herecmd_menu) {
                cmd[0] = there_cmd_menu(FALSE, u.ux + x, u.uy + y);
                if (cmd[0] == '\0')
                    cmd[1] = '\0';
                return cmd;
            }

            if (IS_DOOR(levl[u.ux + x][u.uy + y].typ)) {
                /* slight assistance to the player: choose kick/open for them
                 */
                if (levl[u.ux + x][u.uy + y].doormask & D_LOCKED) {
                    cmd[0] = cmd_from_func(dokick);
                    return cmd;
                }
                if (levl[u.ux + x][u.uy + y].doormask & D_CLOSED) {
                    cmd[0] = cmd_from_func(doopen);
                    return cmd;
                }
            }
            if (levl[u.ux + x][u.uy + y].typ <= SCORR) {
                cmd[0] = cmd_from_func(dosearch);
                cmd[1] = 0;
                return cmd;
            }
        }
    } else {
        /* convert without using floating point, allowing sloppy clicking */
        if (x > 2 * abs(y))
            x = 1, y = 0;
        else if (y > 2 * abs(x))
            x = 0, y = 1;
        else if (x < -2 * abs(y))
            x = -1, y = 0;
        else if (y < -2 * abs(x))
            x = 0, y = -1;
        else
            x = sgn(x), y = sgn(y);

        if (x == 0 && y == 0) {
            /* map click on player to "rest" command */
            cmd[0] = cmd_from_func(donull);
            return cmd;
        }
        dir = xytod(x, y);
    }

    /* move, attack, etc. */
    cmd[1] = 0;
    if (mod == CLICK_1) {
        cmd[0] = Cmd.dirchars[dir];
    } else {
        cmd[0] = (Cmd.num_pad
                     ? M(Cmd.dirchars[dir])
                     : (Cmd.dirchars[dir] - 'a' + 'A')); /* run command */
    }

    return cmd;
}

char
get_count(allowchars, inkey, maxcount, count, historical)
char *allowchars;
char inkey;
long maxcount;
long *count;
boolean historical; /* whether to include in message history: True => yes */
{
    char qbuf[QBUFSZ];
    int key;
    long cnt = 0L;
    boolean backspaced = FALSE;
    /* this should be done in port code so that we have erase_char
       and kill_char available; we can at least fake erase_char */
#define STANDBY_erase_char '\177'

    for (;;) {
        if (inkey) {
            key = inkey;
            inkey = '\0';
        } else
            key = readchar();

        if (digit(key)) {
            cnt = 10L * cnt + (long) (key - '0');
            if (cnt < 0)
                cnt = 0;
            else if (maxcount > 0 && cnt > maxcount)
                cnt = maxcount;
        } else if (cnt && (key == '\b' || key == STANDBY_erase_char)) {
            cnt = cnt / 10;
            backspaced = TRUE;
        } else if (key == Cmd.spkeys[NHKF_ESC]) {
            break;
        } else if (!allowchars || index(allowchars, key)) {
            *count = cnt;
            break;
        }

        if (cnt > 9 || backspaced) {
            clear_nhwindow(WIN_MESSAGE);
            if (backspaced && !cnt) {
                Sprintf(qbuf, "Count: ");
            } else {
                Sprintf(qbuf, "Count: %ld", cnt);
                backspaced = FALSE;
            }
            custompline(SUPPRESS_HISTORY, "%s", qbuf);
            mark_synch();
        }
    }

    if (historical) {
        Sprintf(qbuf, "Count: %ld ", *count);
        (void) key2txt((uchar) key, eos(qbuf));
        putmsghistory(qbuf, FALSE);
    }

    return key;
}


STATIC_OVL char *
parse()
{
#ifdef LINT /* static char in_line[COLNO]; */
    char in_line[COLNO];
#else
    static char in_line[COLNO];
#endif
    register int foo;

    iflags.in_parse = TRUE;
    multi = 0;
    context.move = 1;
    flush_screen(1); /* Flush screen buffer. Put the cursor on the hero. */

#ifdef ALTMETA
    alt_esc = iflags.altmeta; /* readchar() hack */
#endif
    if (!Cmd.num_pad || (foo = readchar()) == Cmd.spkeys[NHKF_COUNT]) {
        long tmpmulti = multi;

        foo = get_count((char *) 0, '\0', LARGEST_INT, &tmpmulti, FALSE);
        last_multi = multi = tmpmulti;
    }
#ifdef ALTMETA
    alt_esc = FALSE; /* readchar() reset */
#endif

    if (iflags.debug_fuzzer /* if fuzzing, override '!' and ^Z */
        && (Cmd.commands[foo & 0x0ff]
            && (Cmd.commands[foo & 0x0ff]->ef_funct == dosuspend_core
                || Cmd.commands[foo & 0x0ff]->ef_funct == dosh_core)))
        foo = Cmd.spkeys[NHKF_ESC];

    if (foo == Cmd.spkeys[NHKF_ESC]) { /* esc cancels count (TH) */
        clear_nhwindow(WIN_MESSAGE);
        multi = last_multi = 0;
    } else if (foo == Cmd.spkeys[NHKF_DOAGAIN] || in_doagain) {
        multi = last_multi;
    } else {
        last_multi = multi;
        savech(0); /* reset input queue */
        savech((char) foo);
    }

    if (multi) {
        multi--;
        save_cm = in_line;
    } else {
        save_cm = (char *) 0;
    }
    /* in 3.4.3 this was in rhack(), where it was too late to handle M-5 */
    if (Cmd.pcHack_compat) {
        /* This handles very old inconsistent DOS/Windows behaviour
           in a different way: earlier, the keyboard handler mapped
           these, which caused counts to be strange when entered
           from the number pad. Now do not map them until here. */
        switch (foo) {
        case '5':
            foo = Cmd.spkeys[NHKF_RUSH];
            break;
        case M('5'):
            foo = Cmd.spkeys[NHKF_RUN];
            break;
        case M('0'):
            foo = Cmd.spkeys[NHKF_DOINV];
            break;
        default:
            break; /* as is */
        }
    }

    in_line[0] = foo;
    in_line[1] = '\0';
    if (prefix_cmd(foo)) {
        foo = readchar();
        savech((char) foo);
        in_line[1] = foo;
        in_line[2] = 0;
    }
    clear_nhwindow(WIN_MESSAGE);

    iflags.in_parse = FALSE;
    return in_line;
}

#ifdef HANGUPHANDLING
/* some very old systems, or descendents of such systems, expect signal
   handlers to have return type `int', but they don't actually inspect
   the return value so we should be safe using `void' unconditionally */
/*ARGUSED*/
void
hangup(sig_unused) /* called as signal() handler, so sent at least one arg */
int sig_unused UNUSED;
{
    if (program_state.exiting)
        program_state.in_moveloop = 0;
    nhwindows_hangup();
#ifdef SAFERHANGUP
    /* When using SAFERHANGUP, the done_hup flag it tested in rhack
       and a couple of other places; actual hangup handling occurs then.
       This is 'safer' because it disallows certain cheats and also
       protects against losing objects in the process of being thrown,
       but also potentially riskier because the disconnected program
       must continue running longer before attempting a hangup save. */
    program_state.done_hup++;
    /* defer hangup iff game appears to be in progress */
    if (program_state.in_moveloop && program_state.something_worth_saving)
        return;
#endif /* SAFERHANGUP */
    end_of_input();
}

void
end_of_input()
{
#ifdef NOSAVEONHANGUP
#ifdef INSURANCE
    if (flags.ins_chkpt && program_state.something_worth_saving)
        program_state.preserve_locks = 1; /* keep files for recovery */
#endif
    program_state.something_worth_saving = 0; /* don't save */
#endif

#ifndef SAFERHANGUP
    if (!program_state.done_hup++)
#endif
        if (program_state.something_worth_saving)
            (void) dosave0();
    if (iflags.window_inited)
        exit_nhwindows((char *) 0);
    clearlocks();
    nh_terminate(EXIT_SUCCESS);
    /*NOTREACHED*/ /* not necessarily true for vms... */
    return;
}
#endif /* HANGUPHANDLING */

char
readchar()
{
    register int sym;
    int x = u.ux, y = u.uy, mod = 0;

    if (iflags.debug_fuzzer)
        return randomkey();
    if (*readchar_queue)
        sym = *readchar_queue++;
    else
        sym = in_doagain ? pgetchar() : nh_poskey(&x, &y, &mod);

#ifdef NR_OF_EOFS
    if (sym == EOF) {
        register int cnt = NR_OF_EOFS;
        /*
         * Some SYSV systems seem to return EOFs for various reasons
         * (?like when one hits break or for interrupted systemcalls?),
         * and we must see several before we quit.
         */
        do {
            clearerr(stdin); /* omit if clearerr is undefined */
            sym = pgetchar();
        } while (--cnt && sym == EOF);
    }
#endif /* NR_OF_EOFS */

    if (sym == EOF) {
#ifdef HANGUPHANDLING
        hangup(0); /* call end_of_input() or set program_state.done_hup */
#endif
        sym = '\033';
#ifdef ALTMETA
    } else if (sym == '\033' && alt_esc) {
        /* iflags.altmeta: treat two character ``ESC c'' as single `M-c' */
        sym = *readchar_queue ? *readchar_queue++ : pgetchar();
        if (sym == EOF || sym == 0)
            sym = '\033';
        else if (sym != '\033')
            sym |= 0200; /* force 8th bit on */
#endif /*ALTMETA*/
    } else if (sym == 0) {
        /* click event */
        readchar_queue = click_to_cmd(x, y, mod);
        sym = *readchar_queue++;
    }
    return (char) sym;
}

/* '_' command, #travel, via keyboard rather than mouse click */
STATIC_PTR int
dotravel(VOID_ARGS)
{
    static char cmd[2];
    coord cc;

    /* [FIXME?  Supporting the ability to disable traveling via mouse
       click makes some sense, depending upon overall mouse usage.
       Disabling '_' on a user by user basis makes no sense at all since
       even if it is typed by accident, aborting when picking a target
       destination is trivial.  Travel via mouse predates travel via '_',
       and this use of OPTION=!travel is probably just a mistake....] */
    if (!flags.travelcmd)
        return 0;

    cmd[1] = 0;
    cc.x = iflags.travelcc.x;
    cc.y = iflags.travelcc.y;
    if (cc.x == 0 && cc.y == 0) {
        /* No cached destination, start attempt from current position */
        cc.x = u.ux;
        cc.y = u.uy;
    }
    iflags.getloc_travelmode = TRUE;
    if (iflags.menu_requested) {
        int gf = iflags.getloc_filter;
        iflags.getloc_filter = GFILTER_VIEW;
        if (!getpos_menu(&cc, GLOC_INTERESTING)) {
            iflags.getloc_filter = gf;
            iflags.getloc_travelmode = FALSE;
            return 0;
        }
        iflags.getloc_filter = gf;
    } else {
        pline("你想走到哪里?");
        if (getpos(&cc, TRUE, "期望的目的地") < 0) {
            /* user pressed ESC */
            iflags.getloc_travelmode = FALSE;
            return 0;
        }
    }
    iflags.getloc_travelmode = FALSE;
    iflags.travelcc.x = u.tx = cc.x;
    iflags.travelcc.y = u.ty = cc.y;
    cmd[0] = Cmd.spkeys[NHKF_TRAVEL];
    readchar_queue = cmd;
    return 0;
}

/*
 *   Parameter validator for generic yes/no function to prevent
 *   the core from sending too long a prompt string to the
 *   window port causing a buffer overflow there.
 */
char
yn_function(query, resp, def)
const char *query, *resp;
char def;
{
    char res, qbuf[QBUFSZ];
#ifdef DUMPLOG
    extern unsigned saved_pline_index; /* pline.c */
    unsigned idx = saved_pline_index;
    /* buffer to hold query+space+formatted_single_char_response */
    char dumplog_buf[QBUFSZ + 1 + 15]; /* [QBUFSZ+1+7] should suffice */
#endif

    iflags.last_msg = PLNMSG_UNKNOWN; /* most recent pline is clobbered */

    /* maximum acceptable length is QBUFSZ-1 */
    if (strlen(query) >= QBUFSZ) {
        /* caller shouldn't have passed anything this long */
        paniclog("Query truncated: ", query);
        (void) strncpy(qbuf, query, QBUFSZ - 1 - 3);
        Strcpy(&qbuf[QBUFSZ - 1 - 3], "...");
        query = qbuf;
    }
    res = (*windowprocs.win_yn_function)(query, resp, def);
#ifdef DUMPLOG
    if (idx == saved_pline_index) {
        /* when idx is still the same as saved_pline_index, the interface
           didn't put the prompt into saved_plines[]; we put a simplified
           version in there now (without response choices or default) */
        Sprintf(dumplog_buf, "%s ", query);
        (void) key2txt((uchar) res, eos(dumplog_buf));
        dumplogmsg(dumplog_buf);
    }
#endif
    return res;
}

/* for paranoid_confirm:quit,die,attack prompting */
boolean
paranoid_query(be_paranoid, prompt)
boolean be_paranoid;
const char *prompt;
{
    boolean confirmed_ok;

    /* when paranoid, player must respond with "yes" rather than just 'y'
       to give the go-ahead for this query; default is "no" unless the
       ParanoidConfirm flag is set in which case there's no default */
    if (be_paranoid) {
        char pbuf[BUFSZ], qbuf[QBUFSZ], ans[BUFSZ];
        const char *promptprefix = "",
                *responsetype = ParanoidConfirm ? "(yes|no)" : "(yes) [no]";
        int k, trylimit = 6; /* 1 normal, 5 more with "Yes or No:" prefix */

        copynchars(pbuf, prompt, BUFSZ - 1);
        /* in addition to being paranoid about this particular
           query, we might be even more paranoid about all paranoia
           responses (ie, ParanoidConfirm is set) in which case we
           require "no" to reject in addition to "yes" to confirm
           (except we won't loop if response is ESC; it means no) */
        do {
            /* make sure we won't overflow a QBUFSZ sized buffer */
            k = (int) (strlen(promptprefix) + 1 + strlen(responsetype));
            if ((int) strlen(pbuf) + k > QBUFSZ - 1) {
                /* chop off some at the end */
                Strcpy(pbuf + (QBUFSZ - 1) - k - 4, "...?"); /* -4: "...?" */
            }

            Sprintf(qbuf, "%s%s %s", promptprefix, pbuf, responsetype);
            *ans = '\0';
            getlin(qbuf, ans);
            (void) mungspaces(ans);
            confirmed_ok = !strcmpi(ans, "yes");
            if (confirmed_ok || *ans == '\033')
                break;
            promptprefix = "\"Yes\" or \"No\": ";
        } while (ParanoidConfirm && strcmpi(ans, "no") && --trylimit);
    } else
        confirmed_ok = (yn(prompt) == 'y');

    return confirmed_ok;
}

/* ^Z command, #suspend */
STATIC_PTR int
dosuspend_core(VOID_ARGS)
{
#ifdef SUSPEND
    /* Does current window system support suspend? */
    if ((*windowprocs.win_can_suspend)()) {
        /* NB: SYSCF SHELLERS handled in port code. */
        dosuspend();
    } else
#endif
        Norep(cmdnotavail, "#suspend");
    return 0;
}

/* '!' command, #shell */
STATIC_PTR int
dosh_core(VOID_ARGS)
{
#ifdef SHELL
    /* access restrictions, if any, are handled in port code */
    dosh();
#else
    Norep(cmdnotavail, "#shell");
#endif
    return 0;
}

/*cmd.c*/
