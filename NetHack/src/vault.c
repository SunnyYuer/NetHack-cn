/* NetHack 3.6	vault.c	$NHDT-Date: 1549921171 2019/02/11 21:39:31 $  $NHDT-Branch: NetHack-3.6.2-beta01 $:$NHDT-Revision: 1.62 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2011. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

STATIC_DCL boolean FDECL(clear_fcorr, (struct monst *, BOOLEAN_P));
STATIC_DCL void FDECL(blackout, (int, int));
STATIC_DCL void FDECL(restfakecorr, (struct monst *));
STATIC_DCL void FDECL(parkguard, (struct monst *));
STATIC_DCL boolean FDECL(in_fcorridor, (struct monst *, int, int));
STATIC_DCL boolean FDECL(find_guard_dest, (struct monst *, xchar *, xchar *));
STATIC_DCL void FDECL(move_gold, (struct obj *, int));
STATIC_DCL void FDECL(wallify_vault, (struct monst *));
STATIC_DCL void FDECL(gd_mv_monaway, (struct monst *, int, int));
STATIC_OVL void FDECL(gd_pick_corridor_gold, (struct monst *, int, int));

void
newegd(mtmp)
struct monst *mtmp;
{
    if (!mtmp->mextra)
        mtmp->mextra = newmextra();
    if (!EGD(mtmp)) {
        EGD(mtmp) = (struct egd *) alloc(sizeof (struct egd));
        (void) memset((genericptr_t) EGD(mtmp), 0, sizeof (struct egd));
    }
}

void
free_egd(mtmp)
struct monst *mtmp;
{
    if (mtmp->mextra && EGD(mtmp)) {
        free((genericptr_t) EGD(mtmp));
        EGD(mtmp) = (struct egd *) 0;
    }
    mtmp->isgd = 0;
}

/* try to remove the temporary corridor (from vault to rest of map) being
   maintained by guard 'grd'; if guard is still in it, removal will fail,
   to be tried again later */
STATIC_OVL boolean
clear_fcorr(grd, forceshow)
struct monst *grd;
boolean forceshow;
{
    register int fcx, fcy, fcbeg;
    struct monst *mtmp;
    boolean sawcorridor = FALSE,
            silently = program_state.stopprint ? TRUE : FALSE;
    struct egd *egrd = EGD(grd);
    struct trap *trap;
    struct rm *lev;

    if (!on_level(&egrd->gdlevel, &u.uz))
        return TRUE;

    /* note: guard remains on 'fmons' list (alive or dead, at off-map
       coordinate <0,0>), until temporary corridor from vault back to
       civilization has been removed */
    while ((fcbeg = egrd->fcbeg) < egrd->fcend) {
        fcx = egrd->fakecorr[fcbeg].fx;
        fcy = egrd->fakecorr[fcbeg].fy;
        if ((DEADMONSTER(grd) || !in_fcorridor(grd, u.ux, u.uy))
            && egrd->gddone)
            forceshow = TRUE;
        if ((u.ux == fcx && u.uy == fcy && !DEADMONSTER(grd))
            || (!forceshow && couldsee(fcx, fcy))
            || (Punished && !carried(uball) && uball->ox == fcx
                && uball->oy == fcy))
            return FALSE;

        if ((mtmp = m_at(fcx, fcy)) != 0) {
            if (mtmp->isgd) {
                return FALSE;
            } else if (!in_fcorridor(grd, u.ux, u.uy)) {
                if (mtmp->mtame)
                    yelp(mtmp);
                (void) rloc(mtmp, FALSE);
            }
        }
        lev = &levl[fcx][fcy];
        if (lev->typ == CORR && cansee(fcx, fcy))
            sawcorridor = TRUE;
        lev->typ = egrd->fakecorr[fcbeg].ftyp;
        if (IS_STWALL(lev->typ)) {
            /* destroy any trap here (pit dug by you, hole dug via
               wand while levitating or by monster, bear trap or land
               mine via object, spun web) when spot reverts to stone */
            if ((trap = t_at(fcx, fcy)) != 0)
                deltrap(trap);
            /* undo scroll/wand/spell of light affecting this spot */
            if (lev->typ == STONE)
                blackout(fcx, fcy);
        }
        map_location(fcx, fcy, 1); /* bypass vision */
        if (!ACCESSIBLE(lev->typ))
            block_point(fcx, fcy);
        vision_full_recalc = 1;
        egrd->fcbeg++;
    }
    if (sawcorridor && !silently)
        pline_The("通道消失了.");
    /* only give encased message if hero is still alive (might get here
       via paygd() -> mongone() -> grddead() when game is over;
       died: no message, quit: message) */
    if (IS_ROCK(levl[u.ux][u.uy].typ) && (Upolyd ? u.mh : u.uhp) > 0
        && !silently)
        You("被围在岩石里.");
    return TRUE;
}

/* as a temporary corridor is removed, set stone locations and adjacent
   spots to unlit; if player used scroll/wand/spell of light while inside
   the corridor, we don't want the light to reappear if/when a new tunnel
   goes through the same area */
STATIC_OVL void
blackout(x, y)
int x, y;
{
    struct rm *lev;
    int i, j;

    for (i = x - 1; i <= x + 1; ++i)
        for (j = y - 1; j <= y + 1; ++j) {
            if (!isok(i, j))
                continue;
            lev = &levl[i][j];
            /* [possible bug: when (i != x || j != y), perhaps we ought
               to check whether the spot on the far side is lit instead
               of doing a blanket blackout of adjacent locations] */
            if (lev->typ == STONE)
                lev->lit = lev->waslit = 0;
            /* mark <i,j> as not having been seen from <x,y> */
            unset_seenv(lev, x, y, i, j);
        }
}

STATIC_OVL void
restfakecorr(grd)
struct monst *grd;
{
    /* it seems you left the corridor - let the guard disappear */
    if (clear_fcorr(grd, FALSE)) {
        grd->isgd = 0; /* dmonsfree() should delete this mon */
        mongone(grd);
    }
}

/* move guard--dead to alive--to <0,0> until temporary corridor is removed */
STATIC_OVL void
parkguard(grd)
struct monst *grd;
{
    /* either guard is dead or will now be treated as if so;
       monster traversal loops should skip it */
    if (grd == context.polearm.hitmon)
        context.polearm.hitmon = 0;
    if (grd->mx) {
        remove_monster(grd->mx, grd->my);
        newsym(grd->mx, grd->my);
        place_monster(grd, 0, 0);
        /* [grd->mx,my just got set to 0,0 by place_monster(), so this
           just sets EGD(grd)->ogx,ogy to 0,0 too; is that what we want?] */
        EGD(grd)->ogx = grd->mx;
        EGD(grd)->ogy = grd->my;
    }
}

/* called in mon.c */
boolean
grddead(grd)
struct monst *grd;
{
    boolean dispose = clear_fcorr(grd, TRUE);

    if (!dispose) {
        /* destroy guard's gold; drop any other inventory */
        relobj(grd, 0, FALSE);
        grd->mhp = 0;
        parkguard(grd);
        dispose = clear_fcorr(grd, TRUE);
    }
    if (dispose)
        grd->isgd = 0; /* for dmonsfree() */
    return dispose;
}

STATIC_OVL boolean
in_fcorridor(grd, x, y)
struct monst *grd;
int x, y;
{
    register int fci;
    struct egd *egrd = EGD(grd);

    for (fci = egrd->fcbeg; fci < egrd->fcend; fci++)
        if (x == egrd->fakecorr[fci].fx && y == egrd->fakecorr[fci].fy)
            return TRUE;
    return FALSE;
}

struct monst *
findgd()
{
    register struct monst *mtmp;

    for (mtmp = fmon; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        if (mtmp->isgd && on_level(&(EGD(mtmp)->gdlevel), &u.uz))
            return mtmp;
    }
    return (struct monst *) 0;
}

void
vault_summon_gd()
{
    if (vault_occupied(u.urooms) && !findgd())
        u.uinvault = (VAULT_GUARD_TIME - 1);
}

char
vault_occupied(array)
char *array;
{
    register char *ptr;

    for (ptr = array; *ptr; ptr++)
        if (rooms[*ptr - ROOMOFFSET].rtype == VAULT)
            return *ptr;
    return '\0';
}

/* hero has teleported out of vault while a guard is active */
void
uleftvault(grd)
struct monst *grd;
{
    /* only called if caller has checked vault_occupied() and findgd() */
    if (!grd || !grd->isgd || DEADMONSTER(grd)) {
        impossible("escaping vault without guard?");
        return;
    }
    /* if carrying gold and arriving anywhere other than next to the guard,
       set the guard loose */
    if ((money_cnt(invent) || hidden_gold())
        && um_dist(grd->mx, grd->my, 1)) {
        if (grd->mpeaceful) {
            if (canspotmon(grd)) /* see or sense via telepathy */
                pline("%s发怒了.", Monnam(grd));
            grd->mpeaceful = 0; /* bypass setmangry() */
        }
        /* if arriving outside guard's temporary corridor, give the
           guard an extra move to deliver message(s) and to teleport
           out of and remove that corridor */
        if (!in_fcorridor(grd, u.ux, u.uy))
            (void) gd_move(grd);
    }
}

STATIC_OVL boolean
find_guard_dest(guard, rx, ry)
struct monst *guard;
xchar *rx, *ry;
{
    register int x, y, dd, lx = 0, ly = 0;

    for (dd = 2; (dd < ROWNO || dd < COLNO); dd++) {
        for (y = u.uy - dd; y <= u.uy + dd; ly = y, y++) {
            if (y < 0 || y > ROWNO - 1)
                continue;
            for (x = u.ux - dd; x <= u.ux + dd; lx = x, x++) {
                if (y != u.uy - dd && y != u.uy + dd && x != u.ux - dd)
                    x = u.ux + dd;
                if (x < 1 || x > COLNO - 1)
                    continue;
                if (guard && ((x == guard->mx && y == guard->my)
                              || (guard->isgd && in_fcorridor(guard, x, y))))
                    continue;
                if (levl[x][y].typ == CORR) {
                    lx = (x < u.ux) ? x + 1 : (x > u.ux) ? x - 1 : x;
                    ly = (y < u.uy) ? y + 1 : (y > u.uy) ? y - 1 : y;
                    if (levl[lx][ly].typ != STONE && levl[lx][ly].typ != CORR)
                        goto incr_radius;
                    *rx = x;
                    *ry = y;
                    return TRUE;
                }
            }
        }
 incr_radius:
        ;
    }
    impossible("Not a single corridor on this level?");
    tele();
    return FALSE;
}

void
invault()
{
#ifdef BSD_43_BUG
    int dummy; /* hack to avoid schain botch */
#endif
    struct monst *guard;
    boolean gsensed;
    int trycount, vaultroom = (int) vault_occupied(u.urooms);

    if (!vaultroom) {
        u.uinvault = 0;
        return;
    }
    vaultroom -= ROOMOFFSET;

    guard = findgd();
    if (++u.uinvault % VAULT_GUARD_TIME == 0 && !guard) {
        /* if time ok and no guard now. */
        char buf[BUFSZ];
        register int x, y, gx, gy;
        xchar rx, ry;
        long umoney;

        /* first find the goal for the guard */
        if (!find_guard_dest((struct monst *)0, &rx, &ry))
            return;
        gx = rx, gy = ry;

        /* next find a good place for a door in the wall */
        x = u.ux;
        y = u.uy;
        if (levl[x][y].typ != ROOM) { /* player dug a door and is in it */
            if (levl[x + 1][y].typ == ROOM)
                x = x + 1;
            else if (levl[x][y + 1].typ == ROOM)
                y = y + 1;
            else if (levl[x - 1][y].typ == ROOM)
                x = x - 1;
            else if (levl[x][y - 1].typ == ROOM)
                y = y - 1;
            else if (levl[x + 1][y + 1].typ == ROOM) {
                x = x + 1;
                y = y + 1;
            } else if (levl[x - 1][y - 1].typ == ROOM) {
                x = x - 1;
                y = y - 1;
            } else if (levl[x + 1][y - 1].typ == ROOM) {
                x = x + 1;
                y = y - 1;
            } else if (levl[x - 1][y + 1].typ == ROOM) {
                x = x - 1;
                y = y + 1;
            }
        }
        while (levl[x][y].typ == ROOM) {
            register int dx, dy;

            dx = (gx > x) ? 1 : (gx < x) ? -1 : 0;
            dy = (gy > y) ? 1 : (gy < y) ? -1 : 0;
            if (abs(gx - x) >= abs(gy - y))
                x += dx;
            else
                y += dy;
        }
        if (x == u.ux && y == u.uy) {
            if (levl[x + 1][y].typ == HWALL || levl[x + 1][y].typ == DOOR)
                x = x + 1;
            else if (levl[x - 1][y].typ == HWALL
                     || levl[x - 1][y].typ == DOOR)
                x = x - 1;
            else if (levl[x][y + 1].typ == VWALL
                     || levl[x][y + 1].typ == DOOR)
                y = y + 1;
            else if (levl[x][y - 1].typ == VWALL
                     || levl[x][y - 1].typ == DOOR)
                y = y - 1;
            else
                return;
        }

        /* make something interesting happen */
        if (!(guard = makemon(&mons[PM_GUARD], x, y, MM_EGD)))
            return;
        guard->isgd = 1;
        guard->mpeaceful = 1;
        set_malign(guard);
        EGD(guard)->gddone = 0;
        EGD(guard)->ogx = x;
        EGD(guard)->ogy = y;
        assign_level(&(EGD(guard)->gdlevel), &u.uz);
        EGD(guard)->vroom = vaultroom;
        EGD(guard)->warncnt = 0;

        reset_faint(); /* if fainted - wake up */
        gsensed = !canspotmon(guard);
        if (!gsensed)
            pline("突然一个金库的%s进来了!",
                  makeplural(guard->data->mname));
        else
            pline("有人已经进入了金库.");
        newsym(guard->mx, guard->my);
        if (u.uswallow) {
            /* can't interrogate hero, don't interrogate engulfer */
            if (!Deaf)
                verbalize("这里发生了什么事?");
            if (gsensed)
                pline_The("其他人不见了踪影.");
            mongone(guard);
            return;
        }
        if (U_AP_TYPE == M_AP_OBJECT || u.uundetected) {
            if (U_AP_TYPE == M_AP_OBJECT
                && youmonst.mappearance != GOLD_PIECE)
                if (!Deaf)
                    verbalize("嘿! 谁把%s放在这儿的?",
                              mimic_obj_name(&youmonst));
            /* You're mimicking some object or you're hidden. */
            pline("很困惑, %s转身离开.", mhe(guard));
            mongone(guard);
            return;
        }
        if (Strangled || is_silent(youmonst.data) || multi < 0) {
            /* [we ought to record whether this this message has already
               been given in order to vary it upon repeat visits, but
               discarding the monster and its egd data renders that hard] */
            if (Deaf)
                pline("%s气喘吁吁地转身离开了.", noit_Monnam(guard));
            else
                verbalize("当你准备好跟我说话时, 我会回来的!");
            mongone(guard);
            return;
        }

        stop_occupation(); /* if occupied, stop it *now* */
        if (multi > 0) {
            nomul(0);
            unmul((char *) 0);
        }
        buf[0] = '\0';
        trycount = 5;
        do {
            getlin(Deaf ? "你被要求提供你的名字. -"
                        : "\" 你好陌生人, 你是谁?\" -", buf);
            (void) mungspaces(buf);
        } while (!buf[0] && --trycount > 0);

        if (u.ualign.type == A_LAWFUL
            /* ignore trailing text, in case player includes rank */
            && strncmpi(buf, plname, (int) strlen(plname)) != 0) {
            adjalign(-1); /* Liar! */
        }

        if (!strcmpi(buf, "Croesus") || !strcmpi(buf, "Kroisos")
            || !strcmpi(buf, "Creosote") || !strcmpi(buf, "克罗伊斯")) { /* Discworld */
            if (!mvitals[PM_CROESUS].died) {
                if (Deaf) {
                    if (!Blind)
                        pline("%s 挥手再见.", noit_Monnam(guard));
                } else {
                    verbalize(
                         "哦, 是的, 当然.  抱歉打扰你了.");
                }
                mongone(guard);
            } else {
                setmangry(guard, FALSE);
                if (Deaf) {
                   if (!Blind)
                        pline("%s 在说什么而且看起来非常生气!",
                              noit_Monnam(guard));
                } else {
                   verbalize(
                           "你死而复生?  我会纠正你!");
                }
                /* don't want guard to waste next turn wielding a weapon */
                if (!MON_WEP(guard)) {
                    guard->weapon_check = NEED_HTH_WEAPON;
                    (void) mon_wield_item(guard);
                }
            }
            return;
        }
        if (Deaf)
            pline("%s%s没有认出你来.", noit_Monnam(guard),
                    (Blind) ? "" : "好像");
        else
            verbalize("我不认识你.");
        umoney = money_cnt(invent);
        if (!umoney && !hidden_gold()) {
            if (Deaf)
                pline("%s 跺着脚%s.", noit_Monnam(guard),
                        (Blind) ? "" : "并招着手");
            else
                verbalize("请跟我来.");
        } else {
            if (!umoney) {
                if (Deaf) {
                    if (!Blind)
                        pline("%s 怒视着你%s.", noit_Monnam(guard),
                            invent ? "的东西" : "");
                } else {
                   verbalize("你藏了金币.");
		        }
            }
            if (Deaf) {
                if (!Blind)
                    pline(
                       "%s伸出%s手掌, 并用%s另一只手招手示意.",
                          noit_Monnam(guard), noit_mhis(guard),
                          noit_mhis(guard));
            } else {
                verbalize(
                    "很可能你所有的金币都是从这个的金库偷的.");
                verbalize("请扔掉金币然后跟着我.");
            }
        }
        EGD(guard)->gdx = gx;
        EGD(guard)->gdy = gy;
        EGD(guard)->fcbeg = 0;
        EGD(guard)->fakecorr[0].fx = x;
        EGD(guard)->fakecorr[0].fy = y;
        if (IS_WALL(levl[x][y].typ)) {
            EGD(guard)->fakecorr[0].ftyp = levl[x][y].typ;
        } else { /* the initial guard location is a dug door */
            int vlt = EGD(guard)->vroom;
            xchar lowx = rooms[vlt].lx, hix = rooms[vlt].hx;
            xchar lowy = rooms[vlt].ly, hiy = rooms[vlt].hy;

            if (x == lowx - 1 && y == lowy - 1)
                EGD(guard)->fakecorr[0].ftyp = TLCORNER;
            else if (x == hix + 1 && y == lowy - 1)
                EGD(guard)->fakecorr[0].ftyp = TRCORNER;
            else if (x == lowx - 1 && y == hiy + 1)
                EGD(guard)->fakecorr[0].ftyp = BLCORNER;
            else if (x == hix + 1 && y == hiy + 1)
                EGD(guard)->fakecorr[0].ftyp = BRCORNER;
            else if (y == lowy - 1 || y == hiy + 1)
                EGD(guard)->fakecorr[0].ftyp = HWALL;
            else if (x == lowx - 1 || x == hix + 1)
                EGD(guard)->fakecorr[0].ftyp = VWALL;
        }
        levl[x][y].typ = DOOR;
        levl[x][y].doormask = D_NODOOR;
        unblock_point(x, y); /* doesn't block light */
        EGD(guard)->fcend = 1;
        EGD(guard)->warncnt = 1;
    }
}

STATIC_OVL void
move_gold(gold, vroom)
struct obj *gold;
int vroom;
{
    xchar nx, ny;

    remove_object(gold);
    newsym(gold->ox, gold->oy);
    nx = rooms[vroom].lx + rn2(2);
    ny = rooms[vroom].ly + rn2(2);
    place_object(gold, nx, ny);
    stackobj(gold);
    newsym(nx, ny);
}

STATIC_OVL void
wallify_vault(grd)
struct monst *grd;
{
    int x, y, typ;
    int vlt = EGD(grd)->vroom;
    char tmp_viz;
    xchar lox = rooms[vlt].lx - 1, hix = rooms[vlt].hx + 1,
          loy = rooms[vlt].ly - 1, hiy = rooms[vlt].hy + 1;
    struct monst *mon;
    struct obj *gold;
    struct trap *trap;
    boolean fixed = FALSE;
    boolean movedgold = FALSE;

    for (x = lox; x <= hix; x++)
        for (y = loy; y <= hiy; y++) {
            /* if not on the room boundary, skip ahead */
            if (x != lox && x != hix && y != loy && y != hiy)
                continue;

            if (!IS_WALL(levl[x][y].typ) && !in_fcorridor(grd, x, y)) {
                if ((mon = m_at(x, y)) != 0 && mon != grd) {
                    if (mon->mtame)
                        yelp(mon);
                    (void) rloc(mon, FALSE);
                }
                if ((gold = g_at(x, y)) != 0) {
                    move_gold(gold, EGD(grd)->vroom);
                    movedgold = TRUE;
                }
                if ((trap = t_at(x, y)) != 0)
                    deltrap(trap);
                if (x == lox)
                    typ =
                        (y == loy) ? TLCORNER : (y == hiy) ? BLCORNER : VWALL;
                else if (x == hix)
                    typ =
                        (y == loy) ? TRCORNER : (y == hiy) ? BRCORNER : VWALL;
                else /* not left or right side, must be top or bottom */
                    typ = HWALL;
                levl[x][y].typ = typ;
                levl[x][y].doormask = 0;
                /*
                 * hack: player knows walls are restored because of the
                 * message, below, so show this on the screen.
                 */
                tmp_viz = viz_array[y][x];
                viz_array[y][x] = IN_SIGHT | COULD_SEE;
                newsym(x, y);
                viz_array[y][x] = tmp_viz;
                block_point(x, y);
                fixed = TRUE;
            }
        }

    if (movedgold || fixed) {
        if (in_fcorridor(grd, grd->mx, grd->my) || cansee(grd->mx, grd->my))
            pline("%s 低声念了一句咒语.", noit_Monnam(grd));
        else
            You_hear("远处的咏唱声.");
        if (movedgold)
            pline("一种神秘的力量把金币移进了金库.");
        if (fixed)
            pline_The("被破坏的金库的墙壁魔法般的恢复了!");
    }
}

STATIC_OVL void
gd_mv_monaway(grd, nx, ny)
register struct monst *grd;
int nx, ny;
{
    if (MON_AT(nx, ny) && !(nx == grd->mx && ny == grd->my)) {
        if (!Deaf)
            verbalize("别挡道, 人渣!");
        if (!rloc(m_at(nx, ny), FALSE) || MON_AT(nx, ny))
            m_into_limbo(m_at(nx, ny));
    }
}

/* have guard pick gold off the floor, possibly moving to the gold's
   position before message and back to his current spot after */
STATIC_OVL void
gd_pick_corridor_gold(grd, goldx, goldy)
struct monst *grd;
int goldx, goldy; /* <gold->ox, gold->oy> */
{
    struct obj *gold;
    coord newcc, bestcc;
    int gdelta, newdelta, bestdelta, tryct,
        guardx = grd->mx, guardy = grd->my;
    boolean under_u = (goldx == u.ux && goldy == u.uy),
            see_it = cansee(goldx, goldy);

    if (under_u) {
        /* Grab the gold from between the hero's feet.
           If guard is two or more steps away; bring him closer first. */
        gold = g_at(goldx, goldy);
        if (!gold) {
            impossible("vault guard: no gold at hero's feet?");
            return;
        }
        gdelta = distu(guardx, guardy);
        if (gdelta > 2 && see_it) { /* skip if player won't see it */
            bestdelta = gdelta;
            bestcc.x = (xchar) guardx, bestcc.y = (xchar) guardy;
            tryct = 9;
            do {
                /* pick an available spot nearest the hero and also try
                   to find the one meeting that criterium which is nearest
                   the guard's current location */
                if (enexto(&newcc, goldx, goldy, grd->data)) {
                    if ((newdelta = distu(newcc.x, newcc.y)) < bestdelta
                        || (newdelta == bestdelta
                            && dist2(newcc.x, newcc.y, guardx, guardy)
                               < dist2(bestcc.x, bestcc.y, guardx, guardy))) {
                        bestdelta = newdelta;
                        bestcc = newcc;
                    }
                }
            } while (--tryct >= 0);

            if (bestdelta < gdelta) {
                remove_monster(guardx, guardy);
                newsym(guardx, guardy);
                place_monster(grd, (int) bestcc.x, (int) bestcc.y);
                newsym(grd->mx, grd->my);
            }
        }
        obj_extract_self(gold);
        add_to_minv(grd, gold);
        newsym(goldx, goldy);

    /* guard is already at gold's location */
    } else if (goldx == guardx && goldy == guardy) {
        mpickgold(grd); /* does a newsym */

    /* gold is at some third spot, neither guard's nor hero's */
    } else {
        /* just for insurance... */
        gd_mv_monaway(grd, goldx, goldy); /* make room for guard */
        if (see_it) { /* skip if player won't see the message */
            remove_monster(grd->mx, grd->my);
            newsym(grd->mx, grd->my);
            place_monster(grd, goldx, goldy); /* sets <grd->mx, grd->my> */
        }
        mpickgold(grd); /* does a newsym */
    }

    if (see_it) { /* cansee(goldx, goldy) */
        char monnambuf[BUFSZ];

        Strcpy(monnambuf, Monnam(grd));
        if (!strcmpi(monnambuf, "它"))
            Strcpy(monnambuf, "某人");
        pline("%s%s捡起了%s金币.", monnambuf,
              (grd->mpeaceful && EGD(grd)->warncnt > 5)
                 ? "平静下来然后" : "",
              under_u ? "在你下方的" : "");
    }

    /* if guard was moved to get the gold, move him back */
    if (grd->mx != guardx || grd->my != guardy) {
        remove_monster(grd->mx, grd->my);
        newsym(grd->mx, grd->my);
        place_monster(grd, guardx, guardy);
        newsym(guardx, guardy);
    }
    return;
}

/*
 * return  1: guard moved,  0: guard didn't,  -1: let m_move do it,  -2: died
 */
int
gd_move(grd)
register struct monst *grd;
{
    int x, y, nx, ny, m, n;
    int dx, dy, gx = 0, gy = 0, fci;
    uchar typ;
    struct rm *crm;
    struct fakecorridor *fcp;
    register struct egd *egrd = EGD(grd);
    long umoney = 0L;
    boolean goldincorridor = FALSE, u_in_vault = FALSE, grd_in_vault = FALSE,
            disappear_msg_seen = FALSE, semi_dead = DEADMONSTER(grd),
            u_carry_gold = FALSE, newspot = FALSE, see_guard;

    if (!on_level(&(egrd->gdlevel), &u.uz))
        return -1;
    nx = ny = m = n = 0;
    if (semi_dead || !grd->mx || egrd->gddone) {
        egrd->gddone = 1;
        goto cleanup;
    }
    debugpline1("gd_move: %s guard", grd->mpeaceful ? "peaceful" : "hostile");

    u_in_vault = vault_occupied(u.urooms) ? TRUE : FALSE;
    grd_in_vault = *in_rooms(grd->mx, grd->my, VAULT) ? TRUE : FALSE;
    if (!u_in_vault && !grd_in_vault)
        wallify_vault(grd);

    if (!grd->mpeaceful) {
        if (!u_in_vault
            && (grd_in_vault || (in_fcorridor(grd, grd->mx, grd->my)
                                 && !in_fcorridor(grd, u.ux, u.uy)))) {
            (void) rloc(grd, FALSE);
            wallify_vault(grd);
            (void) clear_fcorr(grd, TRUE);
            goto letknow;
        }
        if (!in_fcorridor(grd, grd->mx, grd->my))
            (void) clear_fcorr(grd, TRUE);
        return -1;
    }
    if (abs(egrd->ogx - grd->mx) > 1 || abs(egrd->ogy - grd->my) > 1)
        return -1; /* teleported guard - treat as monster */

    if (egrd->witness) {
        if (!Deaf)
            verbalize("你竟敢%s金币, 恶棍!",
                      (egrd->witness & GD_EATGOLD) ? "吃掉" : "毁坏");
        egrd->witness = 0;
        grd->mpeaceful = 0;
        return -1;
    }

    umoney = money_cnt(invent);
    u_carry_gold = umoney > 0L || hidden_gold() > 0L;
    if (egrd->fcend == 1) {
        if (u_in_vault && (u_carry_gold || um_dist(grd->mx, grd->my, 1))) {
            if (egrd->warncnt == 3 && !Deaf)
                verbalize("我重复一遍, %s跟我来!",
                          u_carry_gold
                              ? (!umoney ? "扔掉那个隐藏的金币并"
                                         : "扔掉那个金币并")
                              : "");
            if (egrd->warncnt == 7) {
                m = grd->mx;
                n = grd->my;
                if (!Deaf)
                    verbalize("已经警告过你了, 无赖!");
                mnexto(grd);
                levl[m][n].typ = egrd->fakecorr[0].ftyp;
                newsym(m, n);
                grd->mpeaceful = 0;
                return -1;
            }
            /* not fair to get mad when (s)he's fainted or paralyzed */
            if (!is_fainted() && multi >= 0)
                egrd->warncnt++;
            return 0;
        }

        if (!u_in_vault) {
            if (u_carry_gold) { /* player teleported */
                m = grd->mx;
                n = grd->my;
                (void) rloc(grd, TRUE);
                levl[m][n].typ = egrd->fakecorr[0].ftyp;
                newsym(m, n);
                grd->mpeaceful = 0;
 letknow:
                if (!cansee(grd->mx, grd->my) || !mon_visible(grd))
                    You_hear("%s.",
                             m_carrying(grd, TIN_WHISTLE)
                                 ? "警卫尖锐的哨声"
                                 : "怒吼声");
                else
                    You(um_dist(grd->mx, grd->my, 2)
                            ? "看见%s接近."
                            : "面对着%s.",
                        /* "an angry guard" */
                        x_monnam(grd, ARTICLE_A, "生气的", 0, FALSE));
                return -1;
            } else {
                if (!Deaf)
                    verbalize("好, 去吧.");
                egrd->gddone = 1;
                goto cleanup;
            }
        }
    }

    if (egrd->fcend > 1) {
        if (egrd->fcend > 2 && in_fcorridor(grd, grd->mx, grd->my)
            && !egrd->gddone && !in_fcorridor(grd, u.ux, u.uy)
            && levl[egrd->fakecorr[0].fx][egrd->fakecorr[0].fy].typ
                   == egrd->fakecorr[0].ftyp) {
            pline("%s, 混乱的, 消失了.", noit_Monnam(grd));
            disappear_msg_seen = TRUE;
            goto cleanup;
        }
        if (u_carry_gold && (in_fcorridor(grd, u.ux, u.uy)
                             /* cover a 'blind' spot */
                             || (egrd->fcend > 1 && u_in_vault))) {
            if (!grd->mx) {
                restfakecorr(grd);
                return -2;
            }
            if (egrd->warncnt < 6) {
                egrd->warncnt = 6;
                if (Deaf) {
                    if (!Blind)
                        pline("%s强硬地伸出%s手掌!",
                              noit_Monnam(grd), noit_mhis(grd));
                } else {
                    verbalize("扔掉你所有的金币, 恶棍!");
		        }
                return 0;
            } else {
                if (Deaf) {
                    if (!Blind)
                        pline("%s既愤怒又喜悦地搓%s手!",
                              noit_Monnam(grd), noit_mhis(grd));
                } else {
                    verbalize("就这样, 无赖!");
		        }
                grd->mpeaceful = 0;
                return -1;
            }
        }
    }
    for (fci = egrd->fcbeg; fci < egrd->fcend; fci++)
        if (g_at(egrd->fakecorr[fci].fx, egrd->fakecorr[fci].fy)) {
            m = egrd->fakecorr[fci].fx;
            n = egrd->fakecorr[fci].fy;
            goldincorridor = TRUE;
            break;
        }
    /* new gold can appear if it was embedded in stone and hero kicks it
       (on even via wish and drop) so don't assume hero has been warned */
    if (goldincorridor && !egrd->gddone) {
        gd_pick_corridor_gold(grd, m, n);
        if (!grd->mpeaceful)
            return -1;
        egrd->warncnt = 5;
        return 0;
    }
    if (um_dist(grd->mx, grd->my, 1) || egrd->gddone) {
        if (!egrd->gddone && !rn2(10) && !Deaf && !u.uswallow
            && !(u.ustuck && !sticks(youmonst.data)))
            verbalize("往前走!");
        restfakecorr(grd);
        return 0; /* didn't move */
    }
    x = grd->mx;
    y = grd->my;

    if (u_in_vault)
        goto nextpos;

    /* look around (hor & vert only) for accessible places */
    for (nx = x - 1; nx <= x + 1; nx++)
        for (ny = y - 1; ny <= y + 1; ny++) {
            if ((nx == x || ny == y) && (nx != x || ny != y)
                && isok(nx, ny)) {
                typ = (crm = &levl[nx][ny])->typ;
                if (!IS_STWALL(typ) && !IS_POOL(typ)) {
                    if (in_fcorridor(grd, nx, ny))
                        goto nextnxy;

                    if (*in_rooms(nx, ny, VAULT))
                        continue;

                    /* seems we found a good place to leave him alone */
                    egrd->gddone = 1;
                    if (ACCESSIBLE(typ))
                        goto newpos;
#ifdef STUPID
                    if (typ == SCORR)
                        crm->typ = CORR;
                    else
                        crm->typ = DOOR;
#else
                    crm->typ = (typ == SCORR) ? CORR : DOOR;
#endif
                    if (crm->typ == DOOR)
                        crm->doormask = D_NODOOR;
                    goto proceed;
                }
            }
 nextnxy:
            ;
        }
 nextpos:
    nx = x;
    ny = y;
    gx = egrd->gdx;
    gy = egrd->gdy;
    dx = (gx > x) ? 1 : (gx < x) ? -1 : 0;
    dy = (gy > y) ? 1 : (gy < y) ? -1 : 0;
    if (abs(gx - x) >= abs(gy - y))
        nx += dx;
    else
        ny += dy;

    while ((typ = (crm = &levl[nx][ny])->typ) != STONE) {
        /* in view of the above we must have IS_WALL(typ) or typ == POOL */
        /* must be a wall here */
        if (isok(nx + nx - x, ny + ny - y) && !IS_POOL(typ)
            && IS_ROOM(levl[nx + nx - x][ny + ny - y].typ)) {
            crm->typ = DOOR;
            crm->doormask = D_NODOOR;
            goto proceed;
        }
        if (dy && nx != x) {
            nx = x;
            ny = y + dy;
            continue;
        }
        if (dx && ny != y) {
            ny = y;
            nx = x + dx;
            dy = 0;
            continue;
        }
        /* I don't like this, but ... */
        if (IS_ROOM(typ)) {
            crm->typ = DOOR;
            crm->doormask = D_NODOOR;
            goto proceed;
        }
        break;
    }
    crm->typ = CORR;
 proceed:
    newspot = TRUE;
    unblock_point(nx, ny); /* doesn't block light */
    if (cansee(nx, ny))
        newsym(nx, ny);

    if ((nx != gx || ny != gy) || (grd->mx != gx || grd->my != gy)) {
        fcp = &(egrd->fakecorr[egrd->fcend]);
        if (egrd->fcend++ == FCSIZ)
            panic("fakecorr overflow");
        fcp->fx = nx;
        fcp->fy = ny;
        fcp->ftyp = typ;
    } else if (!egrd->gddone) {
        /* We're stuck, so try to find a new destination. */
        if (!find_guard_dest(grd, &egrd->gdx, &egrd->gdy)
            || (egrd->gdx == gx && egrd->gdy == gy)) {
            pline("%s, 混乱的, 消失了.", Monnam(grd));
            disappear_msg_seen = TRUE;
            goto cleanup;
        } else
            goto nextpos;
    }
 newpos:
    gd_mv_monaway(grd, nx, ny);
    if (egrd->gddone) {
        /* The following is a kludge.  We need to keep    */
        /* the guard around in order to be able to make   */
        /* the fake corridor disappear as the player      */
        /* moves out of it, but we also need the guard    */
        /* out of the way.  We send the guard to never-   */
        /* never land.  We set ogx ogy to mx my in order  */
        /* to avoid a check at the top of this function.  */
        /* At the end of the process, the guard is killed */
        /* in restfakecorr().                             */
 cleanup:
        x = grd->mx, y = grd->my;
        see_guard = canspotmon(grd);
        parkguard(grd); /* move to <0,0> */
        wallify_vault(grd);
        restfakecorr(grd);
        debugpline2("gd_move: %scleanup%s",
                    grd->isgd ? "" : "final ",
                    grd->isgd ? " attempt" : "");
        if (!semi_dead && (in_fcorridor(grd, u.ux, u.uy) || cansee(x, y))) {
            if (!disappear_msg_seen && see_guard)
                pline("突然, %s消失了.", noit_mon_nam(grd));
            return 1;
        }
        return -2;
    }
    egrd->ogx = grd->mx; /* update old positions */
    egrd->ogy = grd->my;
    remove_monster(grd->mx, grd->my);
    place_monster(grd, nx, ny);
    if (newspot && g_at(nx, ny)) {
        /* if there's gold already here (most likely from mineralize()),
           pick it up now so that guard doesn't later think hero dropped
           it and give an inappropriate message */
        mpickgold(grd);
        if (canspotmon(grd))
            pline("%s捡起了一些金币.", Monnam(grd));
    } else
        newsym(grd->mx, grd->my);
    restfakecorr(grd);
    return 1;
}

/* Routine when dying or quitting with a vault guard around */
void
paygd(silently)
boolean silently;
{
    register struct monst *grd = findgd();
    long umoney = money_cnt(invent);
    struct obj *coins, *nextcoins;
    int gx, gy;
    char buf[BUFSZ];

    if (!umoney || !grd)
        return;

    if (u.uinvault) {
        if (!silently)
            Your("%ld %s进入了魔力的记忆金库.",
                 umoney, currency(umoney));
        gx = u.ux;
        gy = u.uy;
    } else {
        if (grd->mpeaceful) /* peaceful guard has no "right" to your gold */
            goto remove_guard;

        mnexto(grd);
        if (!silently)
            pline("%s把你的金币汇寄回了金库.", Monnam(grd));
        gx = rooms[EGD(grd)->vroom].lx + rn2(2);
        gy = rooms[EGD(grd)->vroom].ly + rn2(2);
        Sprintf(buf, "致克罗伊斯: 这里是从%s %s获得的金币.",
                plname, mons[u.umonster].mname);
        make_grave(gx, gy, buf);
    }
    for (coins = invent; coins; coins = nextcoins) {
        nextcoins = coins->nobj;
        if (objects[coins->otyp].oc_class == COIN_CLASS) {
            freeinv(coins);
            place_object(coins, gx, gy);
            stackobj(coins);
        }
    }
 remove_guard:
    mongone(grd);
    return;
}

long
hidden_gold()
{
    long value = 0L;
    struct obj *obj;

    for (obj = invent; obj; obj = obj->nobj)
        if (Has_contents(obj))
            value += contained_gold(obj);
    /* unknown gold stuck inside statues may cause some consternation... */

    return value;
}

/* prevent "You hear footsteps.." when inappropriate */
boolean
gd_sound()
{
    struct monst *grd = findgd();

    if (vault_occupied(u.urooms))
        return FALSE;
    else
        return (boolean) (grd == (struct monst *) 0);
}

void
vault_gd_watching(activity)
unsigned int activity;
{
    struct monst *guard = findgd();

    if (guard && guard->mcansee && m_canseeu(guard)) {
        if (activity == GD_EATGOLD || activity == GD_DESTROYGOLD)
            EGD(guard)->witness = activity;
    }
}

/*vault.c*/
