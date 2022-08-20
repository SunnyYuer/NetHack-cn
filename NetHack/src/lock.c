/* NetHack 3.6	lock.c	$NHDT-Date: 1548978605 2019/01/31 23:50:05 $  $NHDT-Branch: NetHack-3.6.2-beta01 $:$NHDT-Revision: 1.84 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2011. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* at most one of `door' and `box' should be non-null at any given time */
STATIC_VAR NEARDATA struct xlock_s {
    struct rm *door;
    struct obj *box;
    int picktyp, /* key|pick|card for unlock, sharp vs blunt for #force */
        chance, usedtime;
    boolean magic_key;
} xlock;

/* occupation callbacks */
STATIC_PTR int NDECL(picklock);
STATIC_PTR int NDECL(forcelock);

STATIC_DCL const char *NDECL(lock_action);
STATIC_DCL boolean FDECL(obstructed, (int, int, BOOLEAN_P));
STATIC_DCL void FDECL(chest_shatter_msg, (struct obj *));

boolean
picking_lock(x, y)
int *x, *y;
{
    if (occupation == picklock) {
        *x = u.ux + u.dx;
        *y = u.uy + u.dy;
        return TRUE;
    } else {
        *x = *y = 0;
        return FALSE;
    }
}

boolean
picking_at(x, y)
int x, y;
{
    return (boolean) (occupation == picklock && xlock.door == &levl[x][y]);
}

/* produce an occupation string appropriate for the current activity */
STATIC_OVL const char *
lock_action()
{
    /* "unlocking"+2 == "locking" */
    static const char *actions[] = {
        "解锁门",   /* [0] */
        "解锁箱子",  /* [1] */
        "解锁盒子",    /* [2] */
        "撬锁"      /* [3] */
    };
    int cn = strlen("解");

    /* if the target is currently unlocked, we're trying to lock it now */
    if (xlock.door && !(xlock.door->doormask & D_LOCKED))
        return actions[0] + cn; /* "locking the door" */
    else if (xlock.box && !xlock.box->olocked)
        return xlock.box->otyp == CHEST ? actions[1] + cn : actions[2] + cn;
    /* otherwise we're trying to unlock it */
    else if (xlock.picktyp == LOCK_PICK)
        return actions[3]; /* "picking the lock" */
    else if (xlock.picktyp == CREDIT_CARD)
        return actions[3]; /* same as lock_pick */
    else if (xlock.door)
        return actions[0]; /* "unlocking the door" */
    else if (xlock.box)
        return xlock.box->otyp == CHEST ? actions[1] : actions[2];
    else
        return actions[3];
}

/* try to open/close a lock */
STATIC_PTR int
picklock(VOID_ARGS)
{
    if (xlock.box) {
        if (xlock.box->where != OBJ_FLOOR
            || xlock.box->ox != u.ux || xlock.box->oy != u.uy) {
            return ((xlock.usedtime = 0)); /* you or it moved */
        }
    } else { /* door */
        if (xlock.door != &(levl[u.ux + u.dx][u.uy + u.dy])) {
            return ((xlock.usedtime = 0)); /* you moved */
        }
        switch (xlock.door->doormask) {
        case D_NODOOR:
            pline("这个门口没有门.");
            return ((xlock.usedtime = 0));
        case D_ISOPEN:
            You("不能锁打开的门.");
            return ((xlock.usedtime = 0));
        case D_BROKEN:
            pline("这个门被破坏了.");
            return ((xlock.usedtime = 0));
        }
    }

    if (xlock.usedtime++ >= 50 || nohands(youmonst.data)) {
        You("放弃了尝试%s.", lock_action());
        exercise(A_DEX, TRUE); /* even if you don't succeed */
        return ((xlock.usedtime = 0));
    }

    if (rn2(100) >= xlock.chance)
        return 1; /* still busy */

    /* using the Master Key of Thievery finds traps if its bless/curse
       state is adequate (non-cursed for rogues, blessed for others;
       checked when setting up 'xlock') */
    if ((!xlock.door ? (int) xlock.box->otrapped
                     : (xlock.door->doormask & D_TRAPPED) != 0)
        && xlock.magic_key) {
        xlock.chance += 20; /* less effort needed next time */
        /* unfortunately we don't have a 'tknown' flag to record
           "known to be trapped" so declining to disarm and then
           retrying lock manipulation will find it all over again */
        if (yn("你发现了一个陷阱!  你想试着解除它吗?") == 'y') {
            const char *what;
            boolean alreadyunlocked;

            /* disarming while using magic key always succeeds */
            if (xlock.door) {
                xlock.door->doormask &= ~D_TRAPPED;
                what = "门";
                alreadyunlocked = !(xlock.door->doormask & D_LOCKED);
            } else {
                xlock.box->otrapped = 0;
                what = (xlock.box->otyp == CHEST) ? "箱子" : "盒子";
                alreadyunlocked = !xlock.box->olocked;
            }
            You("成功解除了陷阱.  %s仍然%s锁着.",
                what, alreadyunlocked ? "未" : "");
            exercise(A_WIS, TRUE);
        } else {
            You("停止了%s.", lock_action());
            exercise(A_WIS, FALSE);
        }
        return ((xlock.usedtime = 0));
    }

    You("成功地%s.", lock_action());
    if (xlock.door) {
        if (xlock.door->doormask & D_TRAPPED) {
            b_trapped("门", FINGER);
            xlock.door->doormask = D_NODOOR;
            unblock_point(u.ux + u.dx, u.uy + u.dy);
            if (*in_rooms(u.ux + u.dx, u.uy + u.dy, SHOPBASE))
                add_damage(u.ux + u.dx, u.uy + u.dy, SHOP_DOOR_COST);
            newsym(u.ux + u.dx, u.uy + u.dy);
        } else if (xlock.door->doormask & D_LOCKED)
            xlock.door->doormask = D_CLOSED;
        else
            xlock.door->doormask = D_LOCKED;
    } else {
        xlock.box->olocked = !xlock.box->olocked;
        xlock.box->lknown = 1;
        if (xlock.box->otrapped)
            (void) chest_trap(xlock.box, FINGER, FALSE);
    }
    exercise(A_DEX, TRUE);
    return ((xlock.usedtime = 0));
}

void
breakchestlock(box, destroyit)
struct obj *box;
boolean destroyit;
{
    if (!destroyit) { /* bill for the box but not for its contents */
        struct obj *hide_contents = box->cobj;

        box->cobj = 0;
        costly_alteration(box, COST_BRKLCK);
        box->cobj = hide_contents;
        box->olocked = 0;
        box->obroken = 1;
        box->lknown = 1;
    } else { /* #force has destroyed this box (at <u.ux,u.uy>) */
        struct obj *otmp;
        struct monst *shkp = (*u.ushops && costly_spot(u.ux, u.uy))
                                 ? shop_keeper(*u.ushops)
                                 : 0;
        boolean costly = (boolean) (shkp != 0),
                peaceful_shk = costly && (boolean) shkp->mpeaceful;
        long loss = 0L;

        pline("事实上, 你完全破坏了%s.", the(xname(box)));
        /* Put the contents on ground at the hero's feet. */
        while ((otmp = box->cobj) != 0) {
            obj_extract_self(otmp);
            if (!rn2(3) || otmp->oclass == POTION_CLASS) {
                chest_shatter_msg(otmp);
                if (costly)
                    loss += stolen_value(otmp, u.ux, u.uy, peaceful_shk, TRUE);
                if (otmp->quan == 1L) {
                    obfree(otmp, (struct obj *) 0);
                    continue;
                }
                /* this works because we're sure to have at least 1 left;
                   otherwise it would fail since otmp is not in inventory */
                useup(otmp);
            }
            if (box->otyp == ICE_BOX && otmp->otyp == CORPSE) {
                otmp->age = monstermoves - otmp->age; /* actual age */
                start_corpse_timeout(otmp);
            }
            place_object(otmp, u.ux, u.uy);
            stackobj(otmp);
        }
        if (costly)
            loss += stolen_value(box, u.ux, u.uy, peaceful_shk, TRUE);
        if (loss)
            You("破坏了价值%ld %s的物品.", loss, currency(loss));
        delobj(box);
    }
}

/* try to force a locked chest */
STATIC_PTR int
forcelock(VOID_ARGS)
{
    if ((xlock.box->ox != u.ux) || (xlock.box->oy != u.uy))
        return ((xlock.usedtime = 0)); /* you or it moved */

    if (xlock.usedtime++ >= 50 || !uwep || nohands(youmonst.data)) {
        You("放弃了尝试撬锁.");
        if (xlock.usedtime >= 50) /* you made the effort */
            exercise((xlock.picktyp) ? A_DEX : A_STR, TRUE);
        return ((xlock.usedtime = 0));
    }

    if (xlock.picktyp) { /* blade */
        if (rn2(1000 - (int) uwep->spe) > (992 - greatest_erosion(uwep) * 10)
            && !uwep->cursed && !obj_resists(uwep, 0, 99)) {
            /* for a +0 weapon, probability that it survives an unsuccessful
             * attempt to force the lock is (.992)^50 = .67
             */
            pline("%s %s 打破了!", (uwep->quan > 1L) ? "你的一个" : "你的",
                  xname(uwep));
            useup(uwep);
            You("放弃了尝试撬锁.");
            exercise(A_DEX, TRUE);
            return ((xlock.usedtime = 0));
        }
    } else             /* blunt */
        wake_nearby(); /* due to hammering on the container */

    if (rn2(100) >= xlock.chance)
        return 1; /* still busy */

    You("成功地撬开了锁.");
    exercise(xlock.picktyp ? A_DEX : A_STR, TRUE);
    /* breakchestlock() might destroy xlock.box; if so, xlock context will
       be cleared (delobj -> obfree -> maybe_reset_pick); but it might not,
       so explicitly clear that manually */
    breakchestlock(xlock.box, (boolean) (!xlock.picktyp && !rn2(3)));
    reset_pick(); /* lock-picking context is no longer valid */

    return 0;
}

void
reset_pick()
{
    xlock.usedtime = xlock.chance = xlock.picktyp = 0;
    xlock.magic_key = FALSE;
    xlock.door = (struct rm *) 0;
    xlock.box = (struct obj *) 0;
}

/* level change or object deletion; context may no longer be valid */
void
maybe_reset_pick(container)
struct obj *container; /* passed from obfree() */
{
    /*
     * If a specific container, only clear context if it is for that
     * particular container (which is being deleted).  Other stuff on
     * the current dungeon level remains valid.
     * However if 'container' is Null, clear context if not carrying
     * xlock.box (which might be Null if context is for a door).
     * Used for changing levels, where a floor container or a door is
     * being left behind and won't be valid on the new level but a
     * carried container will still be.  There might not be any context,
     * in which case redundantly clearing it is harmless.
     */
    if (container ? (container == xlock.box)
                  : (!xlock.box || !carried(xlock.box)))
        reset_pick();
}

/* for doapply(); if player gives a direction or resumes an interrupted
   previous attempt then it costs hero a move even if nothing ultimately
   happens; when told "can't do that" before being asked for direction
   or player cancels with ESC while giving direction, it doesn't */
#define PICKLOCK_LEARNED_SOMETHING (-1) /* time passes */
#define PICKLOCK_DID_NOTHING 0          /* no time passes */
#define PICKLOCK_DID_SOMETHING 1

/* player is applying a key, lock pick, or credit card */
int
pick_lock(pick)
struct obj *pick;
{
    int picktyp, c, ch;
    coord cc;
    struct rm *door;
    struct obj *otmp;
    char qbuf[QBUFSZ];

    picktyp = pick->otyp;

    /* check whether we're resuming an interrupted previous attempt */
    if (xlock.usedtime && picktyp == xlock.picktyp) {
        static char no_longer[] = "不幸的是, 你不再能%s %s.";

        if (nohands(youmonst.data)) {
            const char *what = (picktyp == LOCK_PICK) ? "开锁器" : "钥匙";

            if (picktyp == CREDIT_CARD)
                what = "卡";
            pline(no_longer, "拿着", what);
            reset_pick();
            return PICKLOCK_LEARNED_SOMETHING;
        } else if (u.uswallow || (xlock.box && !can_reach_floor(TRUE))) {
            pline(no_longer, "够到", "锁");
            reset_pick();
            return PICKLOCK_LEARNED_SOMETHING;
        } else {
            const char *action = lock_action();

            You("继续尝试%s.", action);
            xlock.magic_key = is_magic_key(&youmonst, pick);
            set_occupation(picklock, action, 0);
            return PICKLOCK_DID_SOMETHING;
        }
    }

    if (nohands(youmonst.data)) {
        You_cant("拿着%s --  你没有手!", doname(pick));
        return PICKLOCK_DID_NOTHING;
    } else if (u.uswallow) {
        You_cant("%s开锁 %s.", (picktyp == CREDIT_CARD) ? "" : "上锁或",
                 mon_nam(u.ustuck));
        return PICKLOCK_DID_NOTHING;
    }

    if (picktyp != LOCK_PICK
        && picktyp != CREDIT_CARD
        && picktyp != SKELETON_KEY) {
        impossible("picking lock with object %d?", picktyp);
        return PICKLOCK_DID_NOTHING;
    }
    ch = 0; /* lint suppression */

    if (!get_adjacent_loc((char *) 0, "Invalid location!", u.ux, u.uy, &cc))
        return PICKLOCK_DID_NOTHING;

    if (cc.x == u.ux && cc.y == u.uy) { /* pick lock on a container */
        const char *verb;
        char qsfx[QBUFSZ];
        boolean it;
        int count;

        if (u.dz < 0) {
            There("没有任何种类的锁%s.",
                  Levitation ? "" : "");
            return PICKLOCK_LEARNED_SOMETHING;
        } else if (is_lava(u.ux, u.uy)) {
            pline("做那个可能使%s熔化.", yname(pick));
            return PICKLOCK_LEARNED_SOMETHING;
        } else if (is_pool(u.ux, u.uy) && !Underwater) {
            pline_The("%s没有锁.", hliquid("水"));
            return PICKLOCK_LEARNED_SOMETHING;
        }

        count = 0;
        c = 'n'; /* in case there are no boxes here */
        for (otmp = level.objects[cc.x][cc.y]; otmp; otmp = otmp->nexthere)
            if (Is_box(otmp)) {
                ++count;
                if (!can_reach_floor(TRUE)) {
                    You_cant("从这里够到%s.", the(xname(otmp)));
                    return PICKLOCK_LEARNED_SOMETHING;
                }
                it = 0;
                if (otmp->obroken)
                    verb = "修理";
                else if (!otmp->olocked)
                    verb = "锁住", it = 1;
                else if (picktyp != LOCK_PICK)
                    verb = "解锁", it = 1;
                else
                    verb = "撬";

                /* "There is <a box> here; <verb> <it|its lock>?" */
                Sprintf(qsfx, "; %s %s?", verb, it ? "它" : "它的锁");
                (void) safe_qbuf(qbuf, "这里有", qsfx, otmp, doname,
                                 ansimpleoname, "a box");
                otmp->lknown = 1;

                c = ynq(qbuf);
                if (c == 'q')
                    return 0;
                if (c == 'n')
                    continue;

                if (otmp->obroken) {
                    You_cant("用%s来修理坏掉的锁.", doname(pick));
                    return PICKLOCK_LEARNED_SOMETHING;
                } else if (picktyp == CREDIT_CARD && !otmp->olocked) {
                    /* credit cards are only good for unlocking */
                    You_cant("用%s做那个.",
                             simple_typename(picktyp));
                    return PICKLOCK_LEARNED_SOMETHING;
                }
                switch (picktyp) {
                case CREDIT_CARD:
                    ch = ACURR(A_DEX) + 20 * Role_if(PM_ROGUE);
                    break;
                case LOCK_PICK:
                    ch = 4 * ACURR(A_DEX) + 25 * Role_if(PM_ROGUE);
                    break;
                case SKELETON_KEY:
                    ch = 75 + ACURR(A_DEX);
                    break;
                default:
                    ch = 0;
                }
                if (otmp->cursed)
                    ch /= 2;

                xlock.box = otmp;
                xlock.door = 0;
                break;
            }
        if (c != 'y') {
            if (!count)
                There("似乎没有任何种类的锁.");
            return PICKLOCK_LEARNED_SOMETHING; /* decided against all boxes */
        }
    } else { /* pick the lock in a door */
        struct monst *mtmp;

        if (u.utrap && u.utraptype == TT_PIT) {
            You_cant("够到坑的边缘.");
            return PICKLOCK_LEARNED_SOMETHING;
        }

        door = &levl[cc.x][cc.y];
        mtmp = m_at(cc.x, cc.y);
        if (mtmp && canseemon(mtmp) && M_AP_TYPE(mtmp) != M_AP_FURNITURE
            && M_AP_TYPE(mtmp) != M_AP_OBJECT) {
            if (picktyp == CREDIT_CARD
                && (mtmp->isshk || mtmp->data == &mons[PM_ORACLE]))
                verbalize("No checks, no credit, no problem.");
            else
                pline("我不认为%s会理解那个.",
                      mon_nam(mtmp));
            return PICKLOCK_LEARNED_SOMETHING;
        } else if (mtmp && is_door_mappear(mtmp)) {
            /* "The door actually was a <mimic>!" */
            stumble_onto_mimic(mtmp);
            /* mimic might keep the key (50% chance, 10% for PYEC or MKoT) */
            maybe_absorb_item(mtmp, pick, 50, 10);
            return PICKLOCK_LEARNED_SOMETHING;
        }
        if (!IS_DOOR(door->typ)) {
            if (is_drawbridge_wall(cc.x, cc.y) >= 0)
                You("%s吊桥上没有锁.", Blind ? "感觉" : "看见");
            else
                You("%s那里没有门.", Blind ? "感觉" : "看见");
            return PICKLOCK_LEARNED_SOMETHING;
        }
        switch (door->doormask) {
        case D_NODOOR:
            pline("这个门口没有门.");
            return PICKLOCK_LEARNED_SOMETHING;
        case D_ISOPEN:
            You("不能锁住打开的门.");
            return PICKLOCK_LEARNED_SOMETHING;
        case D_BROKEN:
            pline("这个门被破坏了.");
            return PICKLOCK_LEARNED_SOMETHING;
        default:
            /* credit cards are only good for unlocking */
            if (picktyp == CREDIT_CARD && !(door->doormask & D_LOCKED)) {
                You_cant("用信用卡锁门.");
                return PICKLOCK_LEARNED_SOMETHING;
            }

            Sprintf(qbuf, "%s?",
                    (door->doormask & D_LOCKED) ? "解锁" : "上锁");

            c = yn(qbuf);
            if (c == 'n')
                return 0;

            switch (picktyp) {
            case CREDIT_CARD:
                ch = 2 * ACURR(A_DEX) + 20 * Role_if(PM_ROGUE);
                break;
            case LOCK_PICK:
                ch = 3 * ACURR(A_DEX) + 30 * Role_if(PM_ROGUE);
                break;
            case SKELETON_KEY:
                ch = 70 + ACURR(A_DEX);
                break;
            default:
                ch = 0;
            }
            xlock.door = door;
            xlock.box = 0;
        }
    }
    context.move = 0;
    xlock.chance = ch;
    xlock.picktyp = picktyp;
    xlock.magic_key = is_magic_key(&youmonst, pick);
    xlock.usedtime = 0;
    set_occupation(picklock, lock_action(), 0);
    return PICKLOCK_DID_SOMETHING;
}

/* try to force a chest with your weapon */
int
doforce()
{
    register struct obj *otmp;
    register int c, picktyp;
    char qbuf[QBUFSZ];

    if (u.uswallow) {
        You_cant("在里面强行做任何事情.");
        return 0;
    }
    if (!uwep /* proper type test */
        || ((uwep->oclass == WEAPON_CLASS || is_weptool(uwep))
               ? (objects[uwep->otyp].oc_skill < P_DAGGER
                  || objects[uwep->otyp].oc_skill == P_FLAIL
                  || objects[uwep->otyp].oc_skill > P_LANCE)
               : uwep->oclass != ROCK_CLASS)) {
        You_cant("强行做任何事情 %s 武器.",
                 !uwep ? "当你没有装备一个"
                       : (uwep->oclass != WEAPON_CLASS && !is_weptool(uwep))
                             ? "不用一个合适的"
                             : "用那个");
        return 0;
    }
    if (!can_reach_floor(TRUE)) {
        cant_reach_floor(u.ux, u.uy, FALSE, TRUE);
        return 0;
    }

    picktyp = is_blade(uwep) && !is_pick(uwep);
    if (xlock.usedtime && xlock.box && picktyp == xlock.picktyp) {
        You("重新开始尝试撬锁.");
        set_occupation(forcelock, "撬锁", 0);
        return 1;
    }

    /* A lock is made only for the honest man, the thief will break it. */
    xlock.box = (struct obj *) 0;
    for (otmp = level.objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere)
        if (Is_box(otmp)) {
            if (otmp->obroken || !otmp->olocked) {
                /* force doname() to omit known "broken" or "unlocked"
                   prefix so that the message isn't worded redundantly;
                   since we're about to set lknown, there's no need to
                   remember and then reset its current value */
                otmp->lknown = 0;
                There("有%s, 但是它的锁已经%s.",
                      doname(otmp), otmp->obroken ? "坏了" : "开了");
                otmp->lknown = 1;
                continue;
            }
            (void) safe_qbuf(qbuf, "这里有", " ; 要撬开锁吗?",
                             otmp, doname, ansimpleoname, "a box");
            otmp->lknown = 1;

            c = ynq(qbuf);
            if (c == 'q')
                return 0;
            if (c == 'n')
                continue;

            if (picktyp)
                You("把%s 伸进裂缝中然后使劲地撬动.", yname(uwep));
            else
                You("用%s 使劲地砸.", yname(uwep));
            xlock.box = otmp;
            xlock.chance = objects[uwep->otyp].oc_wldam * 2;
            xlock.picktyp = picktyp;
            xlock.magic_key = FALSE;
            xlock.usedtime = 0;
            break;
        }

    if (xlock.box)
        set_occupation(forcelock, "撬锁", 0);
    else
        You("决定不硬来.");
    return 1;
}

boolean
stumble_on_door_mimic(x, y)
int x, y;
{
    struct monst *mtmp;

    if ((mtmp = m_at(x, y)) && is_door_mappear(mtmp)
        && !Protection_from_shape_changers) {
        stumble_onto_mimic(mtmp);
        return TRUE;
    }
    return FALSE;
}

/* the 'O' command - try to open a door */
int
doopen()
{
    return doopen_indir(0, 0);
}

/* try to open a door in direction u.dx/u.dy */
int
doopen_indir(x, y)
int x, y;
{
    coord cc;
    register struct rm *door;
    boolean portcullis;
    int res = 0;

    if (nohands(youmonst.data)) {
        You_cant("打开任何东西 --  你没有手!");
        return 0;
    }

    if (u.utrap && u.utraptype == TT_PIT) {
        You_cant("够到坑的边缘.");
        return 0;
    }

    if (x > 0 && y > 0) {
        cc.x = x;
        cc.y = y;
    } else if (!get_adjacent_loc((char *) 0, (char *) 0, u.ux, u.uy, &cc))
        return 0;

    /* open at yourself/up/down */
    if ((cc.x == u.ux) && (cc.y == u.uy))
        return doloot();

    if (stumble_on_door_mimic(cc.x, cc.y))
        return 1;

    /* when choosing a direction is impaired, use a turn
       regardless of whether a door is successfully targetted */
    if (Confusion || Stunned)
        res = 1;

    door = &levl[cc.x][cc.y];
    portcullis = (is_drawbridge_wall(cc.x, cc.y) >= 0);
    if (Blind) {
        int oldglyph = door->glyph;
        schar oldlastseentyp = lastseentyp[cc.x][cc.y];

        feel_location(cc.x, cc.y);
        if (door->glyph != oldglyph
            || lastseentyp[cc.x][cc.y] != oldlastseentyp)
            res = 1; /* learned something */
    }

    if (portcullis || !IS_DOOR(door->typ)) {
        /* closed portcullis or spot that opened bridge would span */
        if (is_db_wall(cc.x, cc.y) || door->typ == DRAWBRIDGE_UP)
            There("没有明显的方式来打开吊桥.");
        else if (portcullis || door->typ == DRAWBRIDGE_DOWN)
            pline_The("吊桥已经开了.");
        else if (container_at(cc.x, cc.y, TRUE))
            pline("%s好像那里有什么东西可以搜刮.",
                  Blind ? "感觉" : "看来");
        else
            You("%s那里没有门.", Blind ? "感觉" : "看见");
        return res;
    }

    if (!(door->doormask & D_CLOSED)) {
        const char *mesg;

        switch (door->doormask) {
        case D_BROKEN:
            mesg = " 被破坏了";
            break;
        case D_NODOOR:
            mesg = "口没有门";
            break;
        case D_ISOPEN:
            mesg = " 已经是开的";
            break;
        default:
            mesg = " 是锁着的";
            break;
        }
        pline("这个门%s.", mesg);
        return res;
    }

    if (verysmall(youmonst.data)) {
        pline("你太小了不能拉开门.");
        return res;
    }

    /* door is known to be CLOSED */
    if (rnl(20) < (ACURRSTR + ACURR(A_DEX) + ACURR(A_CON)) / 3) {
        pline_The("门开了.");
        if (door->doormask & D_TRAPPED) {
            b_trapped("门", FINGER);
            door->doormask = D_NODOOR;
            if (*in_rooms(cc.x, cc.y, SHOPBASE))
                add_damage(cc.x, cc.y, SHOP_DOOR_COST);
        } else
            door->doormask = D_ISOPEN;
        feel_newsym(cc.x, cc.y); /* the hero knows she opened it */
        unblock_point(cc.x, cc.y); /* vision: new see through there */
    } else {
        exercise(A_STR, TRUE);
        pline_The("门在抵抗!");
    }

    return 1;
}

STATIC_OVL boolean
obstructed(x, y, quietly)
register int x, y;
boolean quietly;
{
    register struct monst *mtmp = m_at(x, y);

    if (mtmp && M_AP_TYPE(mtmp) != M_AP_FURNITURE) {
        if (M_AP_TYPE(mtmp) == M_AP_OBJECT)
            goto objhere;
        if (!quietly) {
            if ((mtmp->mx != x) || (mtmp->my != y)) {
                /* worm tail */
                pline("%s%s 挡住了路!",
                      !canspotmon(mtmp) ? Something : s_suffix(Monnam(mtmp)),
                      !canspotmon(mtmp) ? "" : "尾巴");
            } else {
                pline("%s 挡住了路!",
                      !canspotmon(mtmp) ? "某个生物" : Monnam(mtmp));
            }
        }
        if (!canspotmon(mtmp))
            map_invisible(x, y);
        return TRUE;
    }
    if (OBJ_AT(x, y)) {
 objhere:
        if (!quietly)
            pline("%s's 挡在路上.", Something);
        return TRUE;
    }
    return FALSE;
}

/* the 'C' command - try to close a door */
int
doclose()
{
    register int x, y;
    register struct rm *door;
    boolean portcullis;
    int res = 0;

    if (nohands(youmonst.data)) {
        You_cant("关闭任何东西 --  你没有手!");
        return 0;
    }

    if (u.utrap && u.utraptype == TT_PIT) {
        You_cant("够到坑的边缘.");
        return 0;
    }

    if (!getdir((char *) 0))
        return 0;

    x = u.ux + u.dx;
    y = u.uy + u.dy;
    if ((x == u.ux) && (y == u.uy)) {
        You("挡在路上!");
        return 1;
    }

    if (!isok(x, y))
        goto nodoor;

    if (stumble_on_door_mimic(x, y))
        return 1;

    /* when choosing a direction is impaired, use a turn
       regardless of whether a door is successfully targetted */
    if (Confusion || Stunned)
        res = 1;

    door = &levl[x][y];
    portcullis = (is_drawbridge_wall(x, y) >= 0);
    if (Blind) {
        int oldglyph = door->glyph;
        schar oldlastseentyp = lastseentyp[x][y];

        feel_location(x, y);
        if (door->glyph != oldglyph || lastseentyp[x][y] != oldlastseentyp)
            res = 1; /* learned something */
    }

    if (portcullis || !IS_DOOR(door->typ)) {
        /* is_db_wall: closed portcullis */
        if (is_db_wall(x, y) || door->typ == DRAWBRIDGE_UP)
            pline_The("吊桥已经关闭了.");
        else if (portcullis || door->typ == DRAWBRIDGE_DOWN)
            There("没有明显的方式来关闭吊桥.");
        else {
 nodoor:
            You("%s那里没有门.", Blind ? "感觉" : "看见");
        }
        return res;
    }

    if (door->doormask == D_NODOOR) {
        pline("这个门口没有门.");
        return res;
    } else if (obstructed(x, y, FALSE)) {
        return res;
    } else if (door->doormask == D_BROKEN) {
        pline("这个门被破坏了.");
        return res;
    } else if (door->doormask & (D_CLOSED | D_LOCKED)) {
        pline("这个门已经是关的.");
        return res;
    }

    if (door->doormask == D_ISOPEN) {
        if (verysmall(youmonst.data) && !u.usteed) {
            pline("你太小了不能拉上门.");
            return res;
        }
        if (u.usteed
            || rn2(25) < (ACURRSTR + ACURR(A_DEX) + ACURR(A_CON)) / 3) {
            pline_The("门关了.");
            door->doormask = D_CLOSED;
            feel_newsym(x, y); /* the hero knows she closed it */
            block_point(x, y); /* vision:  no longer see there */
        } else {
            exercise(A_STR, TRUE);
            pline_The("门在抵抗!");
        }
    }

    return 1;
}

/* box obj was hit with spell or wand effect otmp;
   returns true if something happened */
boolean
boxlock(obj, otmp)
struct obj *obj, *otmp; /* obj *is* a box */
{
    boolean res = 0;

    switch (otmp->otyp) {
    case WAN_LOCKING:
    case SPE_WIZARD_LOCK:
        if (!obj->olocked) { /* lock it; fix if broken */
            pline("哐啷!");
            obj->olocked = 1;
            obj->obroken = 0;
            if (Role_if(PM_WIZARD))
                obj->lknown = 1;
            else
                obj->lknown = 0;
            res = 1;
        } /* else already closed and locked */
        break;
    case WAN_OPENING:
    case SPE_KNOCK:
        if (obj->olocked) { /* unlock; couldn't be broken */
            pline("咔玲!");
            obj->olocked = 0;
            res = 1;
            if (Role_if(PM_WIZARD))
                obj->lknown = 1;
            else
                obj->lknown = 0;
        } else /* silently fix if broken */
            obj->obroken = 0;
        break;
    case WAN_POLYMORPH:
    case SPE_POLYMORPH:
        /* maybe start unlocking chest, get interrupted, then zap it;
           we must avoid any attempt to resume unlocking it */
        if (xlock.box == obj)
            reset_pick();
        break;
    }
    return res;
}

/* Door/secret door was hit with spell or wand effect otmp;
   returns true if something happened */
boolean
doorlock(otmp, x, y)
struct obj *otmp;
int x, y;
{
    register struct rm *door = &levl[x][y];
    boolean res = TRUE;
    int loudness = 0;
    const char *msg = (const char *) 0;
    const char *dustcloud = "一阵灰尘";
    const char *quickly_dissipates = "很快消散";
    boolean mysterywand = (otmp->oclass == WAND_CLASS && !otmp->dknown);

    if (door->typ == SDOOR) {
        switch (otmp->otyp) {
        case WAN_OPENING:
        case SPE_KNOCK:
        case WAN_STRIKING:
        case SPE_FORCE_BOLT:
            door->typ = DOOR;
            door->doormask = D_CLOSED | (door->doormask & D_TRAPPED);
            newsym(x, y);
            if (cansee(x, y))
                pline("墙里出现了一道门!");
            if (otmp->otyp == WAN_OPENING || otmp->otyp == SPE_KNOCK)
                return TRUE;
            break; /* striking: continue door handling below */
        case WAN_LOCKING:
        case SPE_WIZARD_LOCK:
        default:
            return FALSE;
        }
    }

    switch (otmp->otyp) {
    case WAN_LOCKING:
    case SPE_WIZARD_LOCK:
        if (Is_rogue_level(&u.uz)) {
            boolean vis = cansee(x, y);
            /* Can't have real locking in Rogue, so just hide doorway */
            if (vis)
                pline("%s 涌现在更古老, 更原始的门口.",
                      dustcloud);
            else
                You_hear("哗哗响.");
            if (obstructed(x, y, mysterywand)) {
                if (vis)
                    pline_The("烟尘 %s.", quickly_dissipates);
                return FALSE;
            }
            block_point(x, y);
            door->typ = SDOOR, door->doormask = D_NODOOR;
            if (vis)
                pline_The("门口消失了!");
            newsym(x, y);
            return TRUE;
        }
        if (obstructed(x, y, mysterywand))
            return FALSE;
        /* Don't allow doors to close over traps.  This is for pits */
        /* & trap doors, but is it ever OK for anything else? */
        if (t_at(x, y)) {
            /* maketrap() clears doormask, so it should be NODOOR */
            pline("%s 涌现在门口, 但是 %s.", dustcloud,
                  quickly_dissipates);
            return FALSE;
        }

        switch (door->doormask & ~D_TRAPPED) {
        case D_CLOSED:
            msg = "门锁上了!";
            break;
        case D_ISOPEN:
            msg = "门摇晃地关上并锁上了!";
            break;
        case D_BROKEN:
            msg = "被破坏的门重新组装并锁上了!";
            break;
        case D_NODOOR:
            msg =
               "一阵灰尘涌现并组装成了一道门!";
            break;
        default:
            res = FALSE;
            break;
        }
        block_point(x, y);
        door->doormask = D_LOCKED | (door->doormask & D_TRAPPED);
        newsym(x, y);
        break;
    case WAN_OPENING:
    case SPE_KNOCK:
        if (door->doormask & D_LOCKED) {
            msg = "门解锁了!";
            door->doormask = D_CLOSED | (door->doormask & D_TRAPPED);
        } else
            res = FALSE;
        break;
    case WAN_STRIKING:
    case SPE_FORCE_BOLT:
        if (door->doormask & (D_LOCKED | D_CLOSED)) {
            if (door->doormask & D_TRAPPED) {
                if (MON_AT(x, y))
                    (void) mb_trapped(m_at(x, y));
                else if (flags.verbose) {
                    if (cansee(x, y))
                        pline("嘣!!  你看见门爆炸了.");
                    else
                        You_hear("远处的爆炸声.");
                }
                door->doormask = D_NODOOR;
                unblock_point(x, y);
                newsym(x, y);
                loudness = 40;
                break;
            }
            door->doormask = D_BROKEN;
            if (flags.verbose) {
                if (cansee(x, y))
                    pline_The("门崩溃打开了!");
                else
                    You_hear("崩溃声.");
            }
            unblock_point(x, y);
            newsym(x, y);
            /* force vision recalc before printing more messages */
            if (vision_full_recalc)
                vision_recalc(0);
            loudness = 20;
        } else
            res = FALSE;
        break;
    default:
        impossible("magic (%d) attempted on door.", otmp->otyp);
        break;
    }
    if (msg && cansee(x, y))
        pline1(msg);
    if (loudness > 0) {
        /* door was destroyed */
        wake_nearto(x, y, loudness);
        if (*in_rooms(x, y, SHOPBASE))
            add_damage(x, y, 0L);
    }

    if (res && picking_at(x, y)) {
        /* maybe unseen monster zaps door you're unlocking */
        stop_occupation();
        reset_pick();
    }
    return res;
}

STATIC_OVL void
chest_shatter_msg(otmp)
struct obj *otmp;
{
    const char *disposition;
    const char *thing;
    long save_Blinded;

    if (otmp->oclass == POTION_CLASS) {
        You("%s %s 破碎了!", Blind ? "听见" : "看见", bottlename());
        if (!breathless(youmonst.data) || haseyes(youmonst.data))
            potionbreathe(otmp);
        return;
    }
    /* We have functions for distant and singular names, but not one */
    /* which does _both_... */
    save_Blinded = Blinded;
    Blinded = 1;
    thing = singular(otmp, xname);
    Blinded = save_Blinded;
    switch (objects[otmp->otyp].oc_material) {
    case PAPER:
        disposition = "被撕成碎片";
        break;
    case WAX:
        disposition = "被压碎了";
        break;
    case VEGGY:
        disposition = "是浆状的";
        break;
    case FLESH:
        disposition = "被捣碎";
        break;
    case GLASS:
        disposition = "破碎了";
        break;
    case WOOD:
        disposition = "分裂成碎片";
        break;
    default:
        disposition = "被破坏了";
        break;
    }
    pline("%s %s!", thing, disposition);
}

/*lock.c*/
