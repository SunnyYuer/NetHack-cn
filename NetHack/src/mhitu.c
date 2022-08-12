/* NetHack 3.6	mhitu.c	$NHDT-Date: 1575245065 2019/12/02 00:04:25 $  $NHDT-Branch: NetHack-3.6 $:$NHDT-Revision: 1.168 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2012. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artifact.h"

STATIC_VAR NEARDATA struct obj *mon_currwep = (struct obj *) 0;

STATIC_DCL boolean FDECL(u_slip_free, (struct monst *, struct attack *));
STATIC_DCL int FDECL(passiveum, (struct permonst *, struct monst *,
                                 struct attack *));
STATIC_DCL void FDECL(mayberem, (struct monst *, const char *,
                                 struct obj *, const char *));
STATIC_DCL boolean FDECL(diseasemu, (struct permonst *));
STATIC_DCL int FDECL(hitmu, (struct monst *, struct attack *));
STATIC_DCL int FDECL(gulpmu, (struct monst *, struct attack *));
STATIC_DCL int FDECL(explmu, (struct monst *, struct attack *, BOOLEAN_P));
STATIC_DCL void FDECL(missmu, (struct monst *, BOOLEAN_P, struct attack *));
STATIC_DCL void FDECL(mswings, (struct monst *, struct obj *));
STATIC_DCL void FDECL(wildmiss, (struct monst *, struct attack *));
STATIC_DCL void FDECL(hitmsg, (struct monst *, struct attack *));

/* See comment in mhitm.c.  If we use this a lot it probably should be */
/* changed to a parameter to mhitu. */
static int dieroll;

STATIC_OVL void
hitmsg(mtmp, mattk)
struct monst *mtmp;
struct attack *mattk;
{
    int compat;
    const char *pfmt = 0;
    char *Monst_name = Monnam(mtmp);

    /* Note: if opposite gender, "seductively" */
    /* If same gender, "engagingly" for nymph, normal msg for others */
    if ((compat = could_seduce(mtmp, &youmonst, mattk)) != 0
        && !mtmp->mcan && !mtmp->mspec_used) {
        pline("%s%s%s.", Monst_name,
              (compat == 2) ? "动人地" : "诱惑地",
              !Blind ? "向你微笑" : !Deaf ? "和你说话" : "触摸你");
    } else {
        switch (mattk->aatyp) {
        case AT_BITE:
            pfmt = "%s 咬了一口!";
            break;
        case AT_KICK:
            pline("%s 踢了一下%c", Monst_name,
                  thick_skinned(youmonst.data) ? '.' : '!');
            break;
        case AT_STNG:
            pfmt = "%s 叮了一口!";
            break;
        case AT_BUTT:
            pfmt = "%s 撞了一下!";
            break;
        case AT_TUCH:
            pfmt = "%s 碰了你一下!";
            break;
        case AT_TENT:
            pfmt = "%s 触手吸食你!";
            Monst_name = s_suffix(Monst_name);
            break;
        case AT_EXPL:
        case AT_BOOM:
            pfmt = "%s 爆炸了!";
            break;
        default:
            pfmt = "%s 打了一下!";
        }
        if (pfmt)
            pline(pfmt, Monst_name);
    }
}

/* monster missed you */
STATIC_OVL void
missmu(mtmp, nearmiss, mattk)
struct monst *mtmp;
boolean nearmiss;
struct attack *mattk;
{
    if (!canspotmon(mtmp))
        map_invisible(mtmp->mx, mtmp->my);

    if (could_seduce(mtmp, &youmonst, mattk) && !mtmp->mcan)
        pline("%s 假装友好.", Monnam(mtmp));
    else
        pline("%s %s没打中!", Monnam(mtmp),
              (nearmiss && flags.verbose) ? "恰好 " : "");

    stop_occupation();
}

/* monster swings obj */
STATIC_OVL void
mswings(mtmp, otemp)
struct monst *mtmp;
struct obj *otemp;
{
    if (flags.verbose && !Blind && mon_visible(mtmp)) {
        pline("%s %s %s%s %s.", Monnam(mtmp),
              (objects[otemp->otyp].oc_dir & PIERCE) ? "戳刺着" : "挥舞着",
              (otemp->quan > 1L) ? "一个" : "", mhis(mtmp), xname(otemp));
    }
}

/* return how a poison attack was delivered */
const char *
mpoisons_subj(mtmp, mattk)
struct monst *mtmp;
struct attack *mattk;
{
    if (mattk->aatyp == AT_WEAP) {
        struct obj *mwep = (mtmp == &youmonst) ? uwep : MON_WEP(mtmp);
        /* "Foo's attack was poisoned." is pretty lame, but at least
           it's better than "sting" when not a stinging attack... */
        return (!mwep || !mwep->opoisoned) ? "攻击" : "武器";
    } else {
        return (mattk->aatyp == AT_TUCH) ? "接触"
                  : (mattk->aatyp == AT_GAZE) ? "凝视"
                       : (mattk->aatyp == AT_BITE) ? "咬" : "叮";
    }
}

/* called when your intrinsic speed is taken away */
void
u_slow_down()
{
    HFast = 0L;
    if (!Fast)
        You("慢了下来.");
    else /* speed boots */
        Your("迅速感觉不自然.");
    exercise(A_DEX, FALSE);
}

/* monster attacked your displaced image */
STATIC_OVL void
wildmiss(mtmp, mattk)
struct monst *mtmp;
struct attack *mattk;
{
    int compat;
    const char *Monst_name; /* Monnam(mtmp) */

    /* no map_invisible() -- no way to tell where _this_ is coming from */

    if (!flags.verbose)
        return;
    if (!cansee(mtmp->mx, mtmp->my))
        return;
    /* maybe it's attacking an image around the corner? */

    compat = ((mattk->adtyp == AD_SEDU || mattk->adtyp == AD_SSEX)
              ? could_seduce(mtmp, &youmonst, mattk) : 0);
    Monst_name = Monnam(mtmp);

    if (!mtmp->mcansee || (Invis && !perceives(mtmp->data))) {
        const char *swings = (mattk->aatyp == AT_BITE) ? "咬"
                             : (mattk->aatyp == AT_KICK) ? "踢"
                               : (mattk->aatyp == AT_STNG
                                  || mattk->aatyp == AT_BUTT
                                  || nolimbs(mtmp->data)) ? "戳刺"
                                 : "挥舞";

        if (compat)
            pline("%s 试图碰你但没碰到!", Monst_name);
        else
            switch (rn2(3)) {
            case 0:
                pline("%s 疯狂地%s但没打中!", Monst_name, swings);
                break;
            case 1:
                pline("%s 攻击了你的一旁.", Monst_name);
                break;
            case 2:
                pline("%s 攻击了%s!", Monst_name,
                      (levl[mtmp->mux][mtmp->muy].typ == WATER)
                        ? "空水"
                        : "空气");
                break;
            default:
                pline("%s 疯狂地%s!", Monst_name, swings);
                break;
            }

    } else if (Displaced) {
        /* give 'displaced' message even if hero is Blind */
        if (compat)
            pline("%s %s向你的%s位移幻影微笑...", Monst_name,
                  (compat == 2) ? "动人地" : "诱惑地",
                  Invis ? "隐形的" : "");
        else
            pline("%s 击打你的%s位移幻影而没打中你!",
                  /* Note:  if you're both invisible and displaced, only
                   * monsters which see invisible will attack your displaced
                   * image, since the displaced image is also invisible. */
                  Monst_name, Invis ? "隐形的" : "");

    } else if (Underwater) {
        /* monsters may miss especially on water level where
           bubbles shake the player here and there */
        if (compat)
            pline("%s 直接够到你的扭曲的幻影.", Monst_name);
        else
            pline("%s 被水的反射所愚弄并没打中!",
                  Monst_name);

    } else
        impossible("%s attacks you without knowing your location?",
                   Monst_name);
}

void
expels(mtmp, mdat, message)
struct monst *mtmp;
struct permonst *mdat; /* if mtmp is polymorphed, mdat != mtmp->data */
boolean message;
{
    if (message) {
        if (is_animal(mdat)) {
            You("反胃了!");
        } else {
            char blast[40];
            struct attack *attk = attacktype_fordmg(mdat, AT_ENGL, AD_ANY);

            blast[0] = '\0';
            if (!attk) {
                impossible("Swallower has no engulfing attack?");
            } else {
                if (is_whirly(mdat)) {
                    switch (attk->adtyp) {
                    case AD_ELEC:
                        Strcpy(blast, "在一阵火花中");
                        break;
                    case AD_COLD:
                        Strcpy(blast, "在一阵霜冻中");
                        break;
                    }
                } else {
                    Strcpy(blast, "嘎吱一声");
                }
                You("被%s %s喷了出去!", mon_nam(mtmp), blast);
            }
        }
    }
    unstuck(mtmp); /* ball&chain returned in unstuck() */
    mnexto(mtmp);
    newsym(u.ux, u.uy);
    spoteffects(TRUE);
    /* to cover for a case where mtmp is not in a next square */
    if (um_dist(mtmp->mx, mtmp->my, 1))
        pline("嘣...  你艰难地跌落到一定的距离.");
}

/* select a monster's next attack, possibly substituting for its usual one */
struct attack *
getmattk(magr, mdef, indx, prev_result, alt_attk_buf)
struct monst *magr, *mdef;
int indx, prev_result[];
struct attack *alt_attk_buf;
{
    struct permonst *mptr = magr->data;
    struct attack *attk = &mptr->mattk[indx];
    struct obj *weap = (magr == &youmonst) ? uwep : MON_WEP(magr);

    /* honor SEDUCE=0 */
    if (!SYSOPT_SEDUCE) {
        extern const struct attack sa_no[NATTK];

        /* if the first attack is for SSEX damage, all six attacks will be
           substituted (expected succubus/incubus handling); if it isn't
           but another one is, only that other one will be substituted */
        if (mptr->mattk[0].adtyp == AD_SSEX) {
            *alt_attk_buf = sa_no[indx];
            attk = alt_attk_buf;
        } else if (attk->adtyp == AD_SSEX) {
            *alt_attk_buf = *attk;
            attk = alt_attk_buf;
            attk->adtyp = AD_DRLI;
        }
    }

    /* prevent a monster with two consecutive disease or hunger attacks
       from hitting with both of them on the same turn; if the first has
       already hit, switch to a stun attack for the second */
    if (indx > 0 && prev_result[indx - 1] > 0
        && (attk->adtyp == AD_DISE || attk->adtyp == AD_PEST
            || attk->adtyp == AD_FAMN)
        && attk->adtyp == mptr->mattk[indx - 1].adtyp) {
        *alt_attk_buf = *attk;
        attk = alt_attk_buf;
        attk->adtyp = AD_STUN;

    /* make drain-energy damage be somewhat in proportion to energy */
    } else if (attk->adtyp == AD_DREN && mdef == &youmonst) {
        int ulev = max(u.ulevel, 6);

        *alt_attk_buf = *attk;
        attk = alt_attk_buf;
        /* 3.6.0 used 4d6 but since energy drain came out of max energy
           once current energy was gone, that tended to have a severe
           effect on low energy characters; it's now 2d6 with ajustments */
        if (u.uen <= 5 * ulev && attk->damn > 1) {
            attk->damn -= 1; /* low energy: 2d6 -> 1d6 */
            if (u.uenmax <= 2 * ulev && attk->damd > 3)
                attk->damd -= 3; /* very low energy: 1d6 -> 1d3 */
        } else if (u.uen > 12 * ulev) {
            attk->damn += 1; /* high energy: 2d6 -> 3d6 */
            if (u.uenmax > 20 * ulev)
                attk->damd += 3; /* very high energy: 3d6 -> 3d9 */
            /* note: 3d9 is slightly higher than previous 4d6 */
        }

    } else if (attk->aatyp == AT_ENGL && magr->mspec_used) {
        /* can't re-engulf yet; switch to simpler attack */
        *alt_attk_buf = *attk;
        attk = alt_attk_buf;
        if (attk->adtyp == AD_ACID || attk->adtyp == AD_ELEC
            || attk->adtyp == AD_COLD || attk->adtyp == AD_FIRE) {
            attk->aatyp = AT_TUCH;
        } else {
            attk->aatyp = AT_CLAW; /* attack message will be "<foo> hits" */
            attk->adtyp = AD_PHYS;
        }
        attk->damn = 1; /* relatively weak: 1d6 */
        attk->damd = 6;

    /* barrow wight, Nazgul, erinys have weapon attack for non-physical
       damage; force physical damage if attacker has been cancelled or
       if weapon is sufficiently interesting; a few unique creatures
       have two weapon attacks where one does physical damage and other
       doesn't--avoid forcing physical damage for those */
    } else if (indx == 0 && magr != &youmonst
               && attk->aatyp == AT_WEAP && attk->adtyp != AD_PHYS
               && !(mptr->mattk[1].aatyp == AT_WEAP
                    && mptr->mattk[1].adtyp == AD_PHYS)
               && (magr->mcan
                   || (weap && ((weap->otyp == CORPSE
                                 && touch_petrifies(&mons[weap->corpsenm]))
                                || weap->oartifact == ART_STORMBRINGER
                                || weap->oartifact == ART_VORPAL_BLADE)))) {
        *alt_attk_buf = *attk;
        attk = alt_attk_buf;
        attk->adtyp = AD_PHYS;
    }
    return attk;
}

/*
 * mattacku: monster attacks you
 *      returns 1 if monster dies (e.g. "yellow light"), 0 otherwise
 *      Note: if you're displaced or invisible the monster might attack the
 *              wrong position...
 *      Assumption: it's attacking you or an empty square; if there's another
 *              monster which it attacks by mistake, the caller had better
 *              take care of it...
 */
int
mattacku(mtmp)
register struct monst *mtmp;
{
    struct attack *mattk, alt_attk;
    int i, j = 0, tmp, sum[NATTK];
    struct permonst *mdat = mtmp->data;
    boolean ranged = (distu(mtmp->mx, mtmp->my) > 3);
    /* Is it near you?  Affects your actions */
    boolean range2 = !monnear(mtmp, mtmp->mux, mtmp->muy);
    /* Does it think it's near you?  Affects its actions */
    boolean foundyou = (mtmp->mux == u.ux && mtmp->muy == u.uy);
    /* Is it attacking you or your image? */
    boolean youseeit = canseemon(mtmp);
    /* Might be attacking your image around the corner, or
     * invisible, or you might be blind....
     */
    boolean skipnonmagc = FALSE;
    /* Are further physical attack attempts useless? */

    if (!ranged)
        nomul(0);
    if (DEADMONSTER(mtmp) || (Underwater && !is_swimmer(mtmp->data)))
        return 0;

    /* If swallowed, can only be affected by u.ustuck */
    if (u.uswallow) {
        if (mtmp != u.ustuck)
            return 0;
        u.ustuck->mux = u.ux;
        u.ustuck->muy = u.uy;
        range2 = 0;
        foundyou = 1;
        if (u.uinvulnerable)
            return 0; /* stomachs can't hurt you! */

    } else if (u.usteed) {
        if (mtmp == u.usteed)
            /* Your steed won't attack you */
            return 0;
        /* Orcs like to steal and eat horses and the like */
        if (!rn2(is_orc(mtmp->data) ? 2 : 4)
            && distu(mtmp->mx, mtmp->my) <= 2) {
            /* Attack your steed instead */
            i = mattackm(mtmp, u.usteed);
            if ((i & MM_AGR_DIED))
                return 1;
            /* make sure steed is still alive and within range */
            if ((i & MM_DEF_DIED) || !u.usteed
                || distu(mtmp->mx, mtmp->my) > 2)
                return 0;
            /* Let your steed retaliate */
            return !!(mattackm(u.usteed, mtmp) & MM_DEF_DIED);
        }
    }

    if (u.uundetected && !range2 && foundyou && !u.uswallow) {
        if (!canspotmon(mtmp))
            map_invisible(mtmp->mx, mtmp->my);
        u.uundetected = 0;
        if (is_hider(youmonst.data) && u.umonnum != PM_TRAPPER) {
            /* ceiling hider */
            coord cc; /* maybe we need a unexto() function? */
            struct obj *obj;

            You("从%s掉落!", ceiling(u.ux, u.uy));
            /* take monster off map now so that its location
               is eligible for placing hero; we assume that a
               removed monster remembers its old spot <mx,my> */
            remove_monster(mtmp->mx, mtmp->my);
            if (!enexto(&cc, u.ux, u.uy, youmonst.data)
                /* a fish won't voluntarily swap positions
                   when it's in water and hero is over land */
                || (mtmp->data->mlet == S_EEL
                    && is_pool(mtmp->mx, mtmp->my)
                    && !is_pool(u.ux, u.uy))) {
                /* couldn't find any spot for hero; this used to
                   kill off attacker, but now we just give a "miss"
                   message and keep both mtmp and hero at their
                   original positions; hero has become unconcealed
                   so mtmp's next move will be a regular attack */
                place_monster(mtmp, mtmp->mx, mtmp->my); /* put back */
                newsym(u.ux, u.uy); /* u.uundetected was toggled */
                pline("%s 当你掉落便退回了!", Monnam(mtmp));
                return 0;
            }

            /* put mtmp at hero's spot and move hero to <cc.x,.y> */
            newsym(mtmp->mx, mtmp->my); /* finish removal */
            place_monster(mtmp, u.ux, u.uy);
            if (mtmp->wormno) {
                worm_move(mtmp);
                /* tail hasn't grown, so if it now occupies <cc.x,.y>
                   then one of its original spots must be free */
                if (m_at(cc.x, cc.y))
                    (void) enexto(&cc, u.ux, u.uy, youmonst.data);
            }
            teleds(cc.x, cc.y, TRUE); /* move hero */
            set_apparxy(mtmp);
            newsym(u.ux, u.uy);

            if (youmonst.data->mlet != S_PIERCER)
                return 0; /* lurkers don't attack */

            obj = which_armor(mtmp, WORN_HELMET);
            if (obj && is_metallic(obj)) {
                Your("打击擦过了 %s %s.", s_suffix(mon_nam(mtmp)),
                     helm_simple_name(obj));
            } else {
                if (3 + find_mac(mtmp) <= rnd(20)) {
                    pline("%s 被一个掉落的锥子( 你)打到!",
                          Monnam(mtmp));
                    if ((mtmp->mhp -= d(3, 6)) < 1)
                        killed(mtmp);
                } else
                    pline("%s 几乎被一个掉落的锥子( 你)打到!",
                          Monnam(mtmp));
            }

        } else {
            /* surface hider */
            if (!youseeit) {
                pline("它试图移动到你所隐藏的地方.");
            } else {
                /* Ugly kludge for eggs.  The message is phrased so as
                 * to be directed at the monster, not the player,
                 * which makes "laid by you" wrong.  For the
                 * parallelism to work, we can't rephrase it, so we
                 * zap the "laid by you" momentarily instead.
                 */
                struct obj *obj = level.objects[u.ux][u.uy];

                if (obj || u.umonnum == PM_TRAPPER
                    || (youmonst.data->mlet == S_EEL
                        && is_pool(u.ux, u.uy))) {
                    int save_spe = 0; /* suppress warning */

                    if (obj) {
                        save_spe = obj->spe;
                        if (obj->otyp == EGG)
                            obj->spe = 0;
                    }
                    /* note that m_monnam() overrides hallucination, which is
                       what we want when message is from mtmp's perspective */
                    if (youmonst.data->mlet == S_EEL
                        || u.umonnum == PM_TRAPPER)
                        pline(
                             "等等, %s!  那里有一个隐藏的%s 叫做%s!",
                              m_monnam(mtmp), youmonst.data->mname, plname);
                    else
                        pline(
                          "等等, %s!  那里有一个%s 叫做%s 隐藏在%s下面!",
                              m_monnam(mtmp), youmonst.data->mname, plname,
                              doname(level.objects[u.ux][u.uy]));
                    if (obj)
                        obj->spe = save_spe;
                } else
                    impossible("hiding under nothing?");
            }
            newsym(u.ux, u.uy);
        }
        return 0;
    }

    /* hero might be a mimic, concealed via #monster */
    if (youmonst.data->mlet == S_MIMIC && U_AP_TYPE && !range2
        && foundyou && !u.uswallow) {
        boolean sticky = sticks(youmonst.data);

        if (!canspotmon(mtmp))
            map_invisible(mtmp->mx, mtmp->my);
        if (sticky && !youseeit)
            pline("它被你困住了.");
        else /* see note about m_monnam() above */
            pline("等等, %s!  那是一个%s 叫做%s!", m_monnam(mtmp),
                  youmonst.data->mname, plname);
        if (sticky)
            u.ustuck = mtmp;
        youmonst.m_ap_type = M_AP_NOTHING;
        youmonst.mappearance = 0;
        newsym(u.ux, u.uy);
        return 0;
    }

    /* non-mimic hero might be mimicking an object after eating m corpse */
    if (U_AP_TYPE == M_AP_OBJECT && !range2 && foundyou && !u.uswallow) {
        if (!canspotmon(mtmp))
            map_invisible(mtmp->mx, mtmp->my);
        if (!youseeit)
            pline("%s %s!", Something, (likes_gold(mtmp->data)
                                        && youmonst.mappearance == GOLD_PIECE)
                                           ? "试图拾起你"
                                           : "打扰你");
        else /* see note about m_monnam() above */
            pline("等等, %s!  那个%s 实际上是%s 叫做%s!", m_monnam(mtmp),
                  mimic_obj_name(&youmonst), an(mons[u.umonnum].mname),
                  plname);
        if (multi < 0) { /* this should always be the case */
            char buf[BUFSZ];

            Sprintf(buf, "你再次显现为%s.",
                    Upolyd ? (const char *) an(youmonst.data->mname)
                           : (const char *) "你自己");
            unmul(buf); /* immediately stop mimicking */
        }
        return 0;
    }

    /*  Work out the armor class differential   */
    tmp = AC_VALUE(u.uac) + 10; /* tmp ~= 0 - 20 */
    tmp += mtmp->m_lev;
    if (multi < 0)
        tmp += 4;
    if ((Invis && !perceives(mdat)) || !mtmp->mcansee)
        tmp -= 2;
    if (mtmp->mtrapped)
        tmp -= 2;
    if (tmp <= 0)
        tmp = 1;

    /* make eels visible the moment they hit/miss us */
    if (mdat->mlet == S_EEL && mtmp->minvis && cansee(mtmp->mx, mtmp->my)) {
        mtmp->minvis = 0;
        newsym(mtmp->mx, mtmp->my);
    }

    /*  Special demon handling code */
    if ((mtmp->cham == NON_PM) && is_demon(mdat) && !range2
        && mtmp->data != &mons[PM_BALROG] && mtmp->data != &mons[PM_SUCCUBUS]
        && mtmp->data != &mons[PM_INCUBUS])
        if (!mtmp->mcan && !rn2(13))
            (void) msummon(mtmp);

    /*  Special lycanthrope handling code */
    if ((mtmp->cham == NON_PM) && is_were(mdat) && !range2) {
        if (is_human(mdat)) {
            if (!rn2(5 - (night() * 2)) && !mtmp->mcan)
                new_were(mtmp);
        } else if (!rn2(30) && !mtmp->mcan)
            new_were(mtmp);
        mdat = mtmp->data;

        if (!rn2(10) && !mtmp->mcan) {
            int numseen, numhelp;
            char buf[BUFSZ], genericwere[BUFSZ];

            Strcpy(genericwere, "creature");
            numhelp = were_summon(mdat, FALSE, &numseen, genericwere);
            if (youseeit) {
                pline("%s召唤帮助!", Monnam(mtmp));
                if (numhelp > 0) {
                    if (numseen == 0)
                        You_feel("被包围了.");
                } else
                    pline("但是什么都没来.");
            } else {
                const char *from_nowhere;

                if (!Deaf) {
                    pline("%s %s!", Something, makeplural(growl_sound(mtmp)));
                    from_nowhere = "";
                } else
                    from_nowhere = " 不知从哪儿冒出来";
                if (numhelp > 0) {
                    if (numseen < 1)
                        You_feel("被包围了.");
                    else {
                        if (numseen == 1)
                            Sprintf(buf, "一只%s出现了", genericwere);
                        else
                            Sprintf(buf, "%s出现了",
                                    makeplural(genericwere));
                        pline("%s%s!", upstart(buf), from_nowhere);
                    }
                } /* else no help came; but you didn't know it tried */
            }
        }
    }

    if (u.uinvulnerable) {
        /* monsters won't attack you */
        if (mtmp == u.ustuck) {
            pline("%s轻微地放松了控制.", Monnam(mtmp));
        } else if (!range2) {
            if (youseeit || sensemon(mtmp))
                pline("%s开始攻击你, 但又撤回去了.",
                      Monnam(mtmp));
            else
                You_feel("%s在附近移动.", something);
        }
        return 0;
    }

    /* Unlike defensive stuff, don't let them use item _and_ attack. */
    if (find_offensive(mtmp)) {
        int foo = use_offensive(mtmp);

        if (foo != 0)
            return (foo == 1);
    }

    for (i = 0; i < NATTK; i++) {
        sum[i] = 0;
        mon_currwep = (struct obj *)0;
        mattk = getmattk(mtmp, &youmonst, i, sum, &alt_attk);
        if ((u.uswallow && mattk->aatyp != AT_ENGL)
            || (skipnonmagc && mattk->aatyp != AT_MAGC))
            continue;

        switch (mattk->aatyp) {
        case AT_CLAW: /* "hand to hand" attacks */
        case AT_KICK:
        case AT_BITE:
        case AT_STNG:
        case AT_TUCH:
        case AT_BUTT:
        case AT_TENT:
            if (!range2 && (!MON_WEP(mtmp) || mtmp->mconf || Conflict
                            || !touch_petrifies(youmonst.data))) {
                if (foundyou) {
                    if (tmp > (j = rnd(20 + i))) {
                        if (mattk->aatyp != AT_KICK
                            || !thick_skinned(youmonst.data))
                            sum[i] = hitmu(mtmp, mattk);
                    } else
                        missmu(mtmp, (tmp == j), mattk);
                } else {
                    wildmiss(mtmp, mattk);
                    /* skip any remaining non-spell attacks */
                    skipnonmagc = TRUE;
                }
            }
            break;

        case AT_HUGS: /* automatic if prev two attacks succeed */
            /* Note: if displaced, prev attacks never succeeded */
            if ((!range2 && i >= 2 && sum[i - 1] && sum[i - 2])
                || mtmp == u.ustuck)
                sum[i] = hitmu(mtmp, mattk);
            break;

        case AT_GAZE: /* can affect you either ranged or not */
            /* Medusa gaze already operated through m_respond in
               dochug(); don't gaze more than once per round. */
            if (mdat != &mons[PM_MEDUSA])
                sum[i] = gazemu(mtmp, mattk);
            break;

        case AT_EXPL: /* automatic hit if next to, and aimed at you */
            if (!range2)
                sum[i] = explmu(mtmp, mattk, foundyou);
            break;

        case AT_ENGL:
            if (!range2) {
                if (foundyou) {
                    if (u.uswallow
                        || (!mtmp->mspec_used && tmp > (j = rnd(20 + i)))) {
                        /* force swallowing monster to be displayed
                           even when hero is moving away */
                        flush_screen(1);
                        sum[i] = gulpmu(mtmp, mattk);
                    } else {
                        missmu(mtmp, (tmp == j), mattk);
                    }
                } else if (is_animal(mtmp->data)) {
                    pline("%s 大口的吸入一些空气!", Monnam(mtmp));
                } else {
                    if (youseeit)
                        pline("%s 向前冲然后弹回了!", Monnam(mtmp));
                    else
                        You_hear("附近有%s.",
                                 is_whirly(mtmp->data) ? "哗哗的噪声"
                                                       : "啪嗒声");
                }
            }
            break;
        case AT_BREA:
            if (range2)
                sum[i] = breamu(mtmp, mattk);
            /* Note: breamu takes care of displacement */
            break;
        case AT_SPIT:
            if (range2)
                sum[i] = spitmu(mtmp, mattk);
            /* Note: spitmu takes care of displacement */
            break;
        case AT_WEAP:
            if (range2) {
                if (!Is_rogue_level(&u.uz))
                    thrwmu(mtmp);
            } else {
                int hittmp = 0;

                /* Rare but not impossible.  Normally the monster
                 * wields when 2 spaces away, but it can be
                 * teleported or whatever....
                 */
                if (mtmp->weapon_check == NEED_WEAPON || !MON_WEP(mtmp)) {
                    mtmp->weapon_check = NEED_HTH_WEAPON;
                    /* mon_wield_item resets weapon_check as appropriate */
                    if (mon_wield_item(mtmp) != 0)
                        break;
                }
                if (foundyou) {
                    mon_currwep = MON_WEP(mtmp);
                    if (mon_currwep) {
                        hittmp = hitval(mon_currwep, &youmonst);
                        tmp += hittmp;
                        mswings(mtmp, mon_currwep);
                    }
                    if (tmp > (j = dieroll = rnd(20 + i)))
                        sum[i] = hitmu(mtmp, mattk);
                    else
                        missmu(mtmp, (tmp == j), mattk);
                    /* KMH -- Don't accumulate to-hit bonuses */
                    if (mon_currwep)
                        tmp -= hittmp;
                } else {
                    wildmiss(mtmp, mattk);
                    /* skip any remaining non-spell attacks */
                    skipnonmagc = TRUE;
                }
            }
            break;
        case AT_MAGC:
            if (range2)
                sum[i] = buzzmu(mtmp, mattk);
            else
                sum[i] = castmu(mtmp, mattk, TRUE, foundyou);
            break;

        default: /* no attack */
            break;
        }
        if (context.botl)
            bot();
        /* give player a chance of waking up before dying -kaa */
        if (sum[i] == 1) { /* successful attack */
            if (u.usleep && u.usleep < monstermoves && !rn2(10)) {
                multi = -1;
                nomovemsg = "战斗突然唤醒了你.";
            }
        }
        if (sum[i] == 2)
            return 1; /* attacker dead */
        if (sum[i] == 3)
            break; /* attacker teleported, no more attacks */
        /* sum[i] == 0: unsuccessful attack */
    }
    return 0;
}

STATIC_OVL boolean
diseasemu(mdat)
struct permonst *mdat;
{
    if (Sick_resistance) {
        You_feel("有轻微的疾病.");
        return FALSE;
    } else {
        make_sick(Sick ? Sick / 3L + 1L : (long) rn1(ACURR(A_CON), 20),
                  mdat->mname, TRUE, SICK_NONVOMITABLE);
        return TRUE;
    }
}

/* check whether slippery clothing protects from hug or wrap attack */
STATIC_OVL boolean
u_slip_free(mtmp, mattk)
struct monst *mtmp;
struct attack *mattk;
{
    struct obj *obj = (uarmc ? uarmc : uarm);

    if (!obj)
        obj = uarmu;
    if (mattk->adtyp == AD_DRIN)
        obj = uarmh;

    /* if your cloak/armor is greased, monster slips off; this
       protection might fail (33% chance) when the armor is cursed */
    if (obj && (obj->greased || obj->otyp == OILSKIN_CLOAK)
        && (!obj->cursed || rn2(3))) {
        pline("%s %s 你的 %s %s!", Monnam(mtmp),
              (mattk->adtyp == AD_WRAP) ? "滑过"
                                        : "抓住了你, 但是不能持续在",
              obj->greased ? "上油的" : "滑的",
              /* avoid "slippery slippery cloak"
                 for undiscovered oilskin cloak */
              (obj->greased || objects[obj->otyp].oc_name_known)
                  ? xname(obj)
                  : cloak_simple_name(obj));

        if (obj->greased && !rn2(2)) {
            pline_The("油脂消退了.");
            obj->greased = 0;
            update_inventory();
        }
        return TRUE;
    }
    return FALSE;
}

/* armor that sufficiently covers the body might be able to block magic */
int
magic_negation(mon)
struct monst *mon;
{
    struct obj *o;
    long wearmask;
    int armpro, mc = 0;
    boolean is_you = (mon == &youmonst),
            gotprot = is_you ? (EProtection != 0L)
                             /* high priests have innate protection */
                             : (mon->data == &mons[PM_HIGH_PRIEST]);

    for (o = is_you ? invent : mon->minvent; o; o = o->nobj) {
        /* a_can field is only applicable for armor (which must be worn) */
        if ((o->owornmask & W_ARMOR) != 0L) {
            armpro = objects[o->otyp].a_can;
            if (armpro > mc)
                mc = armpro;
        }
        /* if we've already confirmed Protection, skip additional checks */
        if (is_you || gotprot)
            continue;

        /* omit W_SWAPWEP+W_QUIVER; W_ART+W_ARTI handled by protects() */
        wearmask = W_ARMOR | W_ACCESSORY;
        if (o->oclass == WEAPON_CLASS || is_weptool(o))
            wearmask |= W_WEP;
        if (protects(o, ((o->owornmask & wearmask) != 0L) ? TRUE : FALSE))
            gotprot = TRUE;
    }

    if (gotprot) {
        /* extrinsic Protection increases mc by 1 */
        if (mc < 3)
            mc += 1;
    } else if (mc < 1) {
        /* intrinsic Protection is weaker (play balance; obtaining divine
           protection is too easy); it confers minimum mc 1 instead of 0 */
        if ((is_you && ((HProtection && u.ublessed > 0) || u.uspellprot))
            /* aligned priests and angels have innate intrinsic Protection */
            || (mon->data == &mons[PM_ALIGNED_PRIEST] || is_minion(mon->data)))
            mc = 1;
    }
    return mc;
}

/*
 * hitmu: monster hits you
 *        returns 2 if monster dies (e.g. "yellow light"), 1 otherwise
 *        3 if the monster lives but teleported/paralyzed, so it can't keep
 *             attacking you
 */
STATIC_OVL int
hitmu(mtmp, mattk)
register struct monst *mtmp;
register struct attack *mattk;
{
    struct permonst *mdat = mtmp->data;
    int uncancelled, ptmp;
    int dmg, armpro, permdmg, tmphp;
    char buf[BUFSZ];
    struct permonst *olduasmon = youmonst.data;
    int res;

    if (!canspotmon(mtmp))
        map_invisible(mtmp->mx, mtmp->my);

    /*  If the monster is undetected & hits you, you should know where
     *  the attack came from.
     */
    if (mtmp->mundetected && (hides_under(mdat) || mdat->mlet == S_EEL)) {
        mtmp->mundetected = 0;
        if (!(Blind ? Blind_telepat : Unblind_telepat)) {
            struct obj *obj;
            const char *what;

            if ((obj = level.objects[mtmp->mx][mtmp->my]) != 0) {
                if (Blind && !obj->dknown)
                    what = something;
                else if (is_pool(mtmp->mx, mtmp->my) && !Underwater)
                    what = "水";
                else
                    what = doname(obj);

                pline("%s 隐藏在%s下面!", Amonnam(mtmp), what);
            }
            newsym(mtmp->mx, mtmp->my);
        }
    }

    /*  First determine the base damage done */
    dmg = d((int) mattk->damn, (int) mattk->damd);
    if ((is_undead(mdat) || is_vampshifter(mtmp)) && midnight())
        dmg += d((int) mattk->damn, (int) mattk->damd); /* extra damage */

    /*  Next a cancellation factor.
     *  Use uncancelled when cancellation factor takes into account certain
     *  armor's special magic protection.  Otherwise just use !mtmp->mcan.
     */
    armpro = magic_negation(&youmonst);
    uncancelled = !mtmp->mcan && (rn2(10) >= 3 * armpro);

    permdmg = 0;
    /*  Now, adjust damages via resistances or specific attacks */
    switch (mattk->adtyp) {
    case AD_PHYS:
        if (mattk->aatyp == AT_HUGS && !sticks(youmonst.data)) {
            if (!u.ustuck && rn2(2)) {
                if (u_slip_free(mtmp, mattk)) {
                    dmg = 0;
                } else {
                    u.ustuck = mtmp;
                    pline("%s 抓住了你!", Monnam(mtmp));
                }
            } else if (u.ustuck == mtmp) {
                exercise(A_STR, FALSE);
                You("被%s.", (mtmp->data == &mons[PM_ROPE_GOLEM])
                                         ? "阻塞"
                                         : "压垮");
            }
        } else { /* hand to hand weapon */
            struct obj *otmp = mon_currwep;

            if (mattk->aatyp == AT_WEAP && otmp) {
                struct obj *marmg;
                int tmp;

                if (otmp->otyp == CORPSE
                    && touch_petrifies(&mons[otmp->corpsenm])) {
                    dmg = 1;
                    pline("%s 用 %s 尸体打你.", Monnam(mtmp),
                          mons[otmp->corpsenm].mname);
                    if (!Stoned)
                        goto do_stone;
                }
                dmg += dmgval(otmp, &youmonst);
                if ((marmg = which_armor(mtmp, W_ARMG)) != 0
                    && marmg->otyp == GAUNTLETS_OF_POWER)
                    dmg += rn1(4, 3); /* 3..6 */
                if (dmg <= 0)
                    dmg = 1;
                if (!(otmp->oartifact
                      && artifact_hit(mtmp, &youmonst, otmp, &dmg, dieroll)))
                    hitmsg(mtmp, mattk);
                if (!dmg)
                    break;
                if (objects[otmp->otyp].oc_material == SILVER
                    && Hate_silver) {
                    pline_The("银器灼伤了你的身体!");
                    exercise(A_CON, FALSE);
                }
                /* this redundancy necessary because you have
                   to take the damage _before_ being cloned;
                   need to have at least 2 hp left to split */
                tmp = dmg;
                if (u.uac < 0)
                    tmp -= rnd(-u.uac);
                if (tmp < 1)
                    tmp = 1;
                if (u.mh - tmp > 1
                    && (objects[otmp->otyp].oc_material == IRON
                        /* relevant 'metal' objects are scalpel and tsurugi */
                        || objects[otmp->otyp].oc_material == METAL)
                    && (u.umonnum == PM_BLACK_PUDDING
                        || u.umonnum == PM_BROWN_PUDDING)) {
                    if (tmp > 1)
                        exercise(A_STR, FALSE);
                    /* inflict damage now; we know it can't be fatal */
                    u.mh -= tmp;
                    context.botl = 1;
                    dmg = 0; /* don't inflict more damage below */
                    if (cloneu())
                        You("在%s 打你的时候分开了!", mon_nam(mtmp));
                }
                rustm(&youmonst, otmp);
            } else if (mattk->aatyp != AT_TUCH || dmg != 0
                       || mtmp != u.ustuck)
                hitmsg(mtmp, mattk);
        }
        break;
    case AD_DISE:
        hitmsg(mtmp, mattk);
        if (!diseasemu(mdat))
            dmg = 0;
        break;
    case AD_FIRE:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            pline("你%s!", on_fire(youmonst.data, mattk));
            if (completelyburns(youmonst.data)) { /* paper or straw golem */
                You("在大火中结束!");
                /* KMH -- this is okay with unchanging */
                rehumanize();
                break;
            } else if (Fire_resistance) {
                pline_The("火感觉不热!");
                dmg = 0;
            }
            if ((int) mtmp->m_lev > rn2(20))
                destroy_item(SCROLL_CLASS, AD_FIRE);
            if ((int) mtmp->m_lev > rn2(20))
                destroy_item(POTION_CLASS, AD_FIRE);
            if ((int) mtmp->m_lev > rn2(25))
                destroy_item(SPBOOK_CLASS, AD_FIRE);
            burn_away_slime();
        } else
            dmg = 0;
        break;
    case AD_COLD:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            pline("你被冰霜覆盖了!");
            if (Cold_resistance) {
                pline_The("冰霜似乎不冷!");
                dmg = 0;
            }
            if ((int) mtmp->m_lev > rn2(20))
                destroy_item(POTION_CLASS, AD_COLD);
        } else
            dmg = 0;
        break;
    case AD_ELEC:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            You("被电了下!");
            if (Shock_resistance) {
                pline_The("电没有冲击到你!");
                dmg = 0;
            }
            if ((int) mtmp->m_lev > rn2(20))
                destroy_item(WAND_CLASS, AD_ELEC);
            if ((int) mtmp->m_lev > rn2(20))
                destroy_item(RING_CLASS, AD_ELEC);
        } else
            dmg = 0;
        break;
    case AD_SLEE:
        hitmsg(mtmp, mattk);
        if (uncancelled && multi >= 0 && !rn2(5)) {
            if (Sleep_resistance)
                break;
            fall_asleep(-rnd(10), TRUE);
            if (Blind)
                You("陷入了沉睡!");
            else
                You("陷入了沉睡因 %s!", mon_nam(mtmp));
        }
        break;
    case AD_BLND:
        if (can_blnd(mtmp, &youmonst, mattk->aatyp, (struct obj *) 0)) {
            if (!Blind)
                pline("%s 使你失明!", Monnam(mtmp));
            make_blinded(Blinded + (long) dmg, FALSE);
            if (!Blind)
                Your1(vision_clears);
        }
        dmg = 0;
        break;
    case AD_DRST:
        ptmp = A_STR;
        goto dopois;
    case AD_DRDX:
        ptmp = A_DEX;
        goto dopois;
    case AD_DRCO:
        ptmp = A_CON;
 dopois:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(8)) {
            Sprintf(buf, "%s %s", s_suffix(Monnam(mtmp)),
                    mpoisons_subj(mtmp, mattk));
            poisoned(buf, ptmp, mdat->mname, 30, FALSE);
        }
        break;
    case AD_DRIN:
        hitmsg(mtmp, mattk);
        if (defends(AD_DRIN, uwep) || !has_head(youmonst.data)) {
            You("似乎没有受到伤害.");
            /* Not clear what to do for green slimes */
            break;
        }
        if (u_slip_free(mtmp, mattk))
            break;

        if (uarmh && rn2(8)) {
            /* not body_part(HEAD) */
            Your("%s 阻碍了对你的头的攻击.",
                 helm_simple_name(uarmh));
            break;
        }
        /* negative armor class doesn't reduce this damage */
        if (Half_physical_damage)
            dmg = (dmg + 1) / 2;
        mdamageu(mtmp, dmg);
        dmg = 0; /* don't inflict a second dose below */

        if (!uarmh || uarmh->otyp != DUNCE_CAP) {
            /* eat_brains() will miss if target is mindless (won't
               happen here; hero is considered to retain his mind
               regardless of current shape) or is noncorporeal
               (can't happen here; no one can poly into a ghost
               or shade) so this check for missing is academic */
            if (eat_brains(mtmp, &youmonst, TRUE, (int *) 0) == MM_MISS)
                break;
        }
        /* adjattrib gives dunce cap message when appropriate */
        (void) adjattrib(A_INT, -rnd(2), FALSE);
        forget_levels(25);  /* lose memory of 25% of levels */
        forget_objects(25); /* lose memory of 25% of objects */
        break;
    case AD_PLYS:
        hitmsg(mtmp, mattk);
        if (uncancelled && multi >= 0 && !rn2(3)) {
            if (Free_action) {
                You("立即僵硬了.");
            } else {
                if (Blind)
                    You("被僵住了!");
                else
                    You("被%s僵住!", mon_nam(mtmp));
                nomovemsg = You_can_move_again;
                nomul(-rnd(10));
                multi_reason = "被怪物麻痹";
                exercise(A_DEX, FALSE);
            }
        }
        break;
    case AD_DRLI:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(3) && !Drain_resistance) {
            losexp("生命流逝");
        }
        break;
    case AD_LEGS: {
        long side = rn2(2) ? RIGHT_SIDE : LEFT_SIDE;
        const char *sidestr = (side == RIGHT_SIDE) ? "右" : "左",
                   *Monst_name = Monnam(mtmp), *leg = body_part(LEG);

        /* This case is too obvious to ignore, but Nethack is not in
         * general very good at considering height--most short monsters
         * still _can_ attack you when you're flying or mounted.
         */
        if ((u.usteed || Levitation || Flying) && !is_flyer(mtmp->data)) {
            pline("%s试图够到你的%s%s!", Monst_name, sidestr, leg);
            dmg = 0;
        } else if (mtmp->mcan) {
            pline("%s 紧贴着你的%s %s!", Monnam(mtmp),
                  sidestr, leg);
            dmg = 0;
        } else {
            if (uarmf) {
                if (rn2(2) && (uarmf->otyp == LOW_BOOTS
                               || uarmf->otyp == IRON_SHOES)) {
                    pline("%s 刺你的部分暴露的%s %s!",
                          Monst_name, sidestr, leg);
                } else if (!rn2(5)) {
                    pline("%s 刺穿了你的%s鞋!", Monst_name,
                          sidestr);
                } else {
                    pline("%s 刮花了你的%s鞋!", Monst_name,
                          sidestr);
                    dmg = 0;
                    break;
                }
            } else
                pline("%s 刺你的%s%s!", Monst_name, sidestr, leg);

            set_wounded_legs(side, rnd(60 - ACURR(A_DEX)));
            exercise(A_STR, FALSE);
            exercise(A_DEX, FALSE);
        }
        break;
    }
    case AD_STON: /* cockatrice */
        hitmsg(mtmp, mattk);
        if (!rn2(3)) {
            if (mtmp->mcan) {
                if (!Deaf)
                    You_hear("%s的咳嗽!", mon_nam(mtmp));
            } else {
                if (!Deaf)
                    You_hear("%s 嘶嘶声!", s_suffix(mon_nam(mtmp)));
                if (!rn2(10)
                    || (flags.moonphase == NEW_MOON && !have_lizard())) {
 do_stone:
                    if (!Stoned && !Stone_resistance
                        && !(poly_when_stoned(youmonst.data)
                             && polymon(PM_STONE_GOLEM))) {
                        int kformat = KILLED_BY_AN;
                        const char *kname = mtmp->data->mname;

                        if (mtmp->data->geno & G_UNIQ) {
                            if (!type_is_pname(mtmp->data))
                                kname = the(kname);
                            kformat = KILLED_BY;
                        }
                        make_stoned(5L, (char *) 0, kformat, kname);
                        return 1;
                        /* done_in_by(mtmp, STONING); */
                    }
                }
            }
        }
        break;
    case AD_STCK:
        hitmsg(mtmp, mattk);
        if (uncancelled && !u.ustuck && !sticks(youmonst.data))
            u.ustuck = mtmp;
        break;
    case AD_WRAP:
        if ((!mtmp->mcan || u.ustuck == mtmp) && !sticks(youmonst.data)) {
            if (!u.ustuck && !rn2(10)) {
                if (u_slip_free(mtmp, mattk)) {
                    dmg = 0;
                } else {
                    pline("%s 在你的一旁摇动自己!", Monnam(mtmp));
                    u.ustuck = mtmp;
                }
            } else if (u.ustuck == mtmp) {
                if (is_pool(mtmp->mx, mtmp->my) && !Swimming && !Amphibious) {
                    boolean moat = (levl[mtmp->mx][mtmp->my].typ != POOL)
                                   && (levl[mtmp->mx][mtmp->my].typ != WATER)
                                   && !Is_medusa_level(&u.uz)
                                   && !Is_waterlevel(&u.uz);

                    pline("%s淹死了你...", Monnam(mtmp));
                    killer.format = KILLED_BY_AN;
                    Sprintf(killer.name, "%s的%s",
                            mtmp->data->mname,
                            moat ? "护城河" : "水池");
                    done(DROWNING);
                } else if (mattk->aatyp == AT_HUGS)
                    You("被压垮了.");
            } else {
                dmg = 0;
                if (flags.verbose)
                    pline("%s 轻触了你的 %s.", Monnam(mtmp),
                          body_part(LEG));
            }
        } else
            dmg = 0;
        break;
    case AD_WERE:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(4) && u.ulycn == NON_PM
            && !Protection_from_shape_changers && !defends(AD_WERE, uwep)) {
            You_feel("发烧了.");
            exercise(A_CON, FALSE);
            set_ulycn(monsndx(mdat));
            retouch_equipment(2);
        }
        break;
    case AD_SGLD:
        hitmsg(mtmp, mattk);
        if (youmonst.data->mlet == mdat->mlet)
            break;
        if (!mtmp->mcan)
            stealgold(mtmp);
        break;

    case AD_SSEX:
        if (SYSOPT_SEDUCE) {
            if (could_seduce(mtmp, &youmonst, mattk) == 1 && !mtmp->mcan)
                if (doseduce(mtmp))
                    return 3;
            break;
        }
        /*FALLTHRU*/
    case AD_SITM: /* for now these are the same */
    case AD_SEDU:
        if (is_animal(mtmp->data)) {
            hitmsg(mtmp, mattk);
            if (mtmp->mcan)
                break;
            /* Continue below */
        } else if (dmgtype(youmonst.data, AD_SEDU)
                   /* !SYSOPT_SEDUCE: when hero is attacking and AD_SSEX
                      is disabled, it would be changed to another damage
                      type, but when defending, it remains as-is */
                   || dmgtype(youmonst.data, AD_SSEX)) {
            pline("%s %s.", Monnam(mtmp),
                  Deaf ? "说了些什么但你听不见"
                       : mtmp->minvent
                      ? "吹嘘着一些地牢探索者提供的物品"
                  : "对最近盗窃有多么困难发表了一些评论");
            if (!tele_restrict(mtmp))
                (void) rloc(mtmp, TRUE);
            return 3;
        } else if (mtmp->mcan) {
            if (!Blind)
                pline("%s 试图%s你, 但你似乎%s.",
                      Adjmonnam(mtmp, "平淡的"),
                      flags.female ? "诱惑" : "引诱",
                      flags.female ? "不受影响" : "不感兴趣");
            if (rn2(3)) {
                if (!tele_restrict(mtmp))
                    (void) rloc(mtmp, TRUE);
                return 3;
            }
            break;
        }
        buf[0] = '\0';
        switch (steal(mtmp, buf)) {
        case -1:
            return 2;
        case 0:
            break;
        default:
            if (!is_animal(mtmp->data) && !tele_restrict(mtmp))
                (void) rloc(mtmp, TRUE);
            if (is_animal(mtmp->data) && *buf) {
                if (canseemon(mtmp))
                    pline("%s 试图 %s 走带着 %s.", Monnam(mtmp),
                          locomotion(mtmp->data, "跑"), buf);
            }
            monflee(mtmp, 0, FALSE, FALSE);
            return 3;
        }
        break;

    case AD_SAMU:
        hitmsg(mtmp, mattk);
        /* when the Wizard or quest nemesis hits, there's a 1/20 chance
           to steal a quest artifact (any, not just the one for the hero's
           own role) or the Amulet or one of the invocation tools */
        if (!rn2(20))
            stealamulet(mtmp);
        break;

    case AD_TLPT:
        hitmsg(mtmp, mattk);
        if (uncancelled) {
            if (flags.verbose)
                Your("位置似乎突然%s不确定!",
                     (Teleport_control && !Stunned && !unconscious()) ? ""
                     : "很");
            tele();
            /* As of 3.6.2:  make sure damage isn't fatal; previously, it
               was possible to be teleported and then drop dead at
               the destination when QM's 1d4 damage gets applied below;
               even though that wasn't "wrong", it seemed strange,
               particularly if the teleportation had been controlled
               [applying the damage first and not teleporting if fatal
               is another alternative but it has its own complications] */
            if ((Half_physical_damage ? (dmg - 1) / 2 : dmg)
                >= (tmphp = (Upolyd ? u.mh : u.uhp))) {
                dmg = tmphp - 1;
                if (Half_physical_damage)
                    dmg *= 2; /* doesn't actually increase damage; we only
                               * get here if half the original damage would
                               * would have been fatal, so double reduced
                               * damage will be less than original damage */
                if (dmg < 1) { /* implies (tmphp <= 1) */
                    dmg = 1;
                    /* this might increase current HP beyond maximum HP but
                       it will be immediately reduced below, so that should
                       be indistinguishable from zero damage; we don't drop
                       damage all the way to zero because that inhibits any
                       passive counterattack if poly'd hero has one */
                    if (Upolyd && u.mh == 1)
                        ++u.mh;
                    else if (!Upolyd && u.uhp == 1)
                        ++u.uhp;
                    /* [don't set context.botl here] */
                }
            }
        }
        break;
    case AD_RUST:
        hitmsg(mtmp, mattk);
        if (mtmp->mcan)
            break;
        if (u.umonnum == PM_IRON_GOLEM) {
            You("生锈了!");
            /* KMH -- this is okay with unchanging */
            rehumanize();
            break;
        }
        erode_armor(&youmonst, ERODE_RUST);
        break;
    case AD_CORR:
        hitmsg(mtmp, mattk);
        if (mtmp->mcan)
            break;
        erode_armor(&youmonst, ERODE_CORRODE);
        break;
    case AD_DCAY:
        hitmsg(mtmp, mattk);
        if (mtmp->mcan)
            break;
        if (u.umonnum == PM_WOOD_GOLEM || u.umonnum == PM_LEATHER_GOLEM) {
            You("腐烂了!");
            /* KMH -- this is okay with unchanging */
            rehumanize();
            break;
        }
        erode_armor(&youmonst, ERODE_ROT);
        break;
    case AD_HEAL:
        /* a cancelled nurse is just an ordinary monster,
         * nurses don't heal those that cause petrification */
        if (mtmp->mcan || (Upolyd && touch_petrifies(youmonst.data))) {
            hitmsg(mtmp, mattk);
            break;
        }
        if (!uwep && !uarmu && !uarm && !uarmc
            && !uarms && !uarmg && !uarmf && !uarmh) {
            boolean goaway = FALSE;

            pline("%s 打了一下!  ( 希望你不介意.)", Monnam(mtmp));
            if (Upolyd) {
                u.mh += rnd(7);
                if (!rn2(7)) {
                    /* no upper limit necessary; effect is temporary */
                    u.mhmax++;
                    if (!rn2(13))
                        goaway = TRUE;
                }
                if (u.mh > u.mhmax)
                    u.mh = u.mhmax;
            } else {
                u.uhp += rnd(7);
                if (!rn2(7)) {
                    /* hard upper limit via nurse care: 25 * ulevel */
                    if (u.uhpmax < 5 * u.ulevel + d(2 * u.ulevel, 10))
                        u.uhpmax++;
                    if (!rn2(13))
                        goaway = TRUE;
                }
                if (u.uhp > u.uhpmax)
                    u.uhp = u.uhpmax;
            }
            if (!rn2(3))
                exercise(A_STR, TRUE);
            if (!rn2(3))
                exercise(A_CON, TRUE);
            if (Sick)
                make_sick(0L, (char *) 0, FALSE, SICK_ALL);
            context.botl = 1;
            if (goaway) {
                mongone(mtmp);
                return 2;
            } else if (!rn2(33)) {
                if (!tele_restrict(mtmp))
                    (void) rloc(mtmp, TRUE);
                monflee(mtmp, d(3, 6), TRUE, FALSE);
                return 3;
            }
            dmg = 0;
        } else {
            if (Role_if(PM_HEALER)) {
                if (!Deaf && !(moves % 5))
                    verbalize("伙计, 除非你合作否则我不能帮助你.");
                dmg = 0;
            } else
                hitmsg(mtmp, mattk);
        }
        break;
    case AD_CURS:
        hitmsg(mtmp, mattk);
        if (!night() && mdat == &mons[PM_GREMLIN])
            break;
        if (!mtmp->mcan && !rn2(10)) {
            if (!Deaf) {
                if (Blind)
                    You_hear("笑声.");
                else
                    pline("%s 咯咯笑.", Monnam(mtmp));
            }
            if (u.umonnum == PM_CLAY_GOLEM) {
                pline("有些文字从你的头上消失了!");
                /* KMH -- this is okay with unchanging */
                rehumanize();
                break;
            }
            attrcurse();
        }
        break;
    case AD_STUN:
        hitmsg(mtmp, mattk);
        if (!mtmp->mcan && !rn2(4)) {
            make_stunned((HStun & TIMEOUT) + (long) dmg, TRUE);
            dmg /= 2;
        }
        break;
    case AD_ACID:
        hitmsg(mtmp, mattk);
        if (!mtmp->mcan && !rn2(3))
            if (Acid_resistance) {
                pline("你满身都是%s, 但它似乎无害.",
                      hliquid("酸"));
                dmg = 0;
            } else {
                pline("你满身都是%s!  它灼伤了你!", hliquid("酸"));
                exercise(A_STR, FALSE);
            }
        else
            dmg = 0;
        break;
    case AD_SLOW:
        hitmsg(mtmp, mattk);
        if (uncancelled && HFast && !defends(AD_SLOW, uwep) && !rn2(4))
            u_slow_down();
        break;
    case AD_DREN:
        hitmsg(mtmp, mattk);
        if (uncancelled && !rn2(4)) /* 25% chance */
            drain_en(dmg);
        dmg = 0;
        break;
    case AD_CONF:
        hitmsg(mtmp, mattk);
        if (!mtmp->mcan && !rn2(4) && !mtmp->mspec_used) {
            mtmp->mspec_used = mtmp->mspec_used + (dmg + rn2(6));
            if (Confusion)
                You("变得更加混乱.");
            else
                You("变得混乱.");
            make_confused(HConfusion + dmg, FALSE);
        }
        dmg = 0;
        break;
    case AD_DETH:
        pline("%s 伸出其致命的碰触.", Monnam(mtmp));
        if (is_undead(youmonst.data)) {
            /* Still does normal damage */
            pline("那个是死亡之触?");
            break;
        }
        switch (rn2(20)) {
        case 19:
        case 18:
        case 17:
            if (!Antimagic) {
                killer.format = KILLED_BY_AN;
                Strcpy(killer.name, "死亡之触");
                done(DIED);
                dmg = 0;
                break;
            }
            /*FALLTHRU*/
        default: /* case 16: ... case 5: */
            You_feel("你的生命力在流逝...");
            permdmg = 1; /* actual damage done below */
            break;
        case 4:
        case 3:
        case 2:
        case 1:
        case 0:
            if (Antimagic)
                shieldeff(u.ux, u.uy);
            pline("你很幸运, 它没起作用!");
            dmg = 0;
            break;
        }
        break;
    case AD_PEST:
        pline("%s 伸出手, 你觉得发烧和发冷.", Monnam(mtmp));
        (void) diseasemu(mdat); /* plus the normal damage */
        break;
    case AD_FAMN:
        pline("%s 伸出手, 你的身体枯萎了.", Monnam(mtmp));
        exercise(A_CON, FALSE);
        if (!is_fainted())
            morehungry(rn1(40, 40));
        /* plus the normal damage */
        break;
    case AD_SLIM:
        hitmsg(mtmp, mattk);
        if (!uncancelled)
            break;
        if (flaming(youmonst.data)) {
            pline_The("黏液被烧掉了!");
            dmg = 0;
        } else if (Unchanging || noncorporeal(youmonst.data)
                   || youmonst.data == &mons[PM_GREEN_SLIME]) {
            You("不受影响.");
            dmg = 0;
        } else if (!Slimed) {
            You("感觉不是特别好.");
            make_slimed(10L, (char *) 0);
            delayed_killer(SLIMED, KILLED_BY_AN, mtmp->data->mname);
        } else
            pline("呸!");
        break;
    case AD_ENCH: /* KMH -- remove enchantment (disenchanter) */
        hitmsg(mtmp, mattk);
        /* uncancelled is sufficient enough; please
           don't make this attack less frequent */
        if (uncancelled) {
            struct obj *obj = some_armor(&youmonst);

            if (!obj) {
                /* some rings are susceptible;
                   amulets and blindfolds aren't (at present) */
                switch (rn2(5)) {
                case 0:
                    break;
                case 1:
                    obj = uright;
                    break;
                case 2:
                    obj = uleft;
                    break;
                case 3:
                    obj = uamul;
                    break;
                case 4:
                    obj = ublindf;
                    break;
                }
            }
            if (drain_item(obj, FALSE)) {
                pline("%s不太有效.", Yobjnam2(obj, "似乎"));
            }
        }
        break;
    default:
        dmg = 0;
        break;
    }
    if ((Upolyd ? u.mh : u.uhp) < 1) {
        /* already dead? call rehumanize() or done_in_by() as appropriate */
        mdamageu(mtmp, 1);
        dmg = 0;
    }

    /*  Negative armor class reduces damage done instead of fully protecting
     *  against hits.
     */
    if (dmg && u.uac < 0) {
        dmg -= rnd(-u.uac);
        if (dmg < 1)
            dmg = 1;
    }

    if (dmg) {
        if (Half_physical_damage
            /* Mitre of Holiness */
            || (Role_if(PM_PRIEST) && uarmh && is_quest_artifact(uarmh)
                && (is_undead(mtmp->data) || is_demon(mtmp->data)
                    || is_vampshifter(mtmp))))
            dmg = (dmg + 1) / 2;

        if (permdmg) { /* Death's life force drain */
            int lowerlimit, *hpmax_p;
            /*
             * Apply some of the damage to permanent hit points:
             *  polymorphed         100% against poly'd hpmax
             *  hpmax > 25*lvl      100% against normal hpmax
             *  hpmax > 10*lvl  50..100%
             *  hpmax >  5*lvl  25..75%
             *  otherwise        0..50%
             * Never reduces hpmax below 1 hit point per level.
             */
            permdmg = rn2(dmg / 2 + 1);
            if (Upolyd || u.uhpmax > 25 * u.ulevel)
                permdmg = dmg;
            else if (u.uhpmax > 10 * u.ulevel)
                permdmg += dmg / 2;
            else if (u.uhpmax > 5 * u.ulevel)
                permdmg += dmg / 4;

            if (Upolyd) {
                hpmax_p = &u.mhmax;
                /* [can't use youmonst.m_lev] */
                lowerlimit = min((int) youmonst.data->mlevel, u.ulevel);
            } else {
                hpmax_p = &u.uhpmax;
                lowerlimit = u.ulevel;
            }
            if (*hpmax_p - permdmg > lowerlimit)
                *hpmax_p -= permdmg;
            else if (*hpmax_p > lowerlimit)
                *hpmax_p = lowerlimit;
            /* else unlikely...
             * already at or below minimum threshold; do nothing */
            context.botl = 1;
        }

        mdamageu(mtmp, dmg);
    }

    if (dmg)
        res = passiveum(olduasmon, mtmp, mattk);
    else
        res = 1;
    stop_occupation();
    return res;
}

/* An interface for use when taking a blindfold off, for example,
 * to see if an engulfing attack should immediately take affect, like
 * a passive attack. TRUE if engulfing blindness occurred */
boolean
gulp_blnd_check()
{
    struct attack *mattk;

    if (!Blinded && u.uswallow
        && (mattk = attacktype_fordmg(u.ustuck->data, AT_ENGL, AD_BLND))
        && can_blnd(u.ustuck, &youmonst, mattk->aatyp, (struct obj *) 0)) {
        ++u.uswldtim; /* compensate for gulpmu change */
        (void) gulpmu(u.ustuck, mattk);
        return TRUE;
    }
    return FALSE;
}

/* monster swallows you, or damage if u.uswallow */
STATIC_OVL int
gulpmu(mtmp, mattk)
struct monst *mtmp;
struct attack *mattk;
{
    struct trap *t = t_at(u.ux, u.uy);
    int tmp = d((int) mattk->damn, (int) mattk->damd);
    int tim_tmp;
    struct obj *otmp2;
    int i;
    boolean physical_damage = FALSE;

    if (!u.uswallow) { /* swallows you */
        int omx = mtmp->mx, omy = mtmp->my;

        if (!engulf_target(mtmp, &youmonst))
            return 0;
        if ((t && is_pit(t->ttyp)) && sobj_at(BOULDER, u.ux, u.uy))
            return 0;

        if (Punished)
            unplacebc(); /* ball&chain go away */
        remove_monster(omx, omy);
        mtmp->mtrapped = 0; /* no longer on old trap */
        place_monster(mtmp, u.ux, u.uy);
        u.ustuck = mtmp;
        newsym(mtmp->mx, mtmp->my);
        if (is_animal(mtmp->data) && u.usteed) {
            char buf[BUFSZ];

            /* Too many quirks presently if hero and steed
             * are swallowed. Pretend purple worms don't
             * like horses for now :-)
             */
            Strcpy(buf, mon_nam(u.usteed));
            pline("%s猛向前冲把你从%s 拉了下来!", Monnam(mtmp),
                  buf);
            dismount_steed(DISMOUNT_ENGULFED);
        } else
            pline("%s 吞食了你!", Monnam(mtmp));
        stop_occupation();
        reset_occupations(); /* behave as if you had moved */

        if (u.utrap) {
            You("从%s中被释放出来!",
                u.utraptype == TT_WEB ? "网" : "陷阱");
            reset_utrap(FALSE);
        }

        i = number_leashed();
        if (i > 0) {
            const char *s = (i > 1) ? "狗链" : "狗链";

            pline_The("%s %s变松了.", s, vtense(s, "突然"));
            unleash_all();
        }

        if (touch_petrifies(youmonst.data) && !resists_ston(mtmp)) {
            /* put the attacker back where it started;
               the resulting statue will end up there */
            remove_monster(mtmp->mx, mtmp->my); /* u.ux,u.uy */
            place_monster(mtmp, omx, omy);
            minstapetrify(mtmp, TRUE);
            /* normally unstuck() would do this, but we're not
               fully swallowed yet so that won't work here */
            if (Punished)
                placebc();
            u.ustuck = 0;
            return (!DEADMONSTER(mtmp)) ? 0 : 2;
        }

        display_nhwindow(WIN_MESSAGE, FALSE);
        vision_recalc(2); /* hero can't see anything */
        u.uswallow = 1;
        /* for digestion, shorter time is more dangerous;
           for other swallowings, longer time means more
           chances for the swallower to attack */
        if (mattk->adtyp == AD_DGST) {
            tim_tmp = 25 - (int) mtmp->m_lev;
            if (tim_tmp > 0)
                tim_tmp = rnd(tim_tmp) / 2;
            else if (tim_tmp < 0)
                tim_tmp = -(rnd(-tim_tmp) / 2);
            /* having good armor & high constitution makes
               it take longer for you to be digested, but
               you'll end up trapped inside for longer too */
            tim_tmp += -u.uac + 10 + (ACURR(A_CON) / 3 - 1);
        } else {
            /* higher level attacker takes longer to eject hero */
            tim_tmp = rnd((int) mtmp->m_lev + 10 / 2);
        }
        /* u.uswldtim always set > 1 */
        u.uswldtim = (unsigned) ((tim_tmp < 2) ? 2 : tim_tmp);
        swallowed(1);
        for (otmp2 = invent; otmp2; otmp2 = otmp2->nobj)
            (void) snuff_lit(otmp2);
    }

    if (mtmp != u.ustuck)
        return 0;
    if (Punished) {
        /* ball&chain are in limbo while swallowed; update their internal
           location to be at swallower's spot */
        if (uchain->where == OBJ_FREE)
            uchain->ox = mtmp->mx, uchain->oy = mtmp->my;
        if (uball->where == OBJ_FREE)
            uball->ox = mtmp->mx, uball->oy = mtmp->my;
    }
    if (u.uswldtim > 0)
        u.uswldtim -= 1;

    switch (mattk->adtyp) {
    case AD_DGST:
        physical_damage = TRUE;
        if (Slow_digestion) {
            /* Messages are handled below */
            u.uswldtim = 0;
            tmp = 0;
        } else if (u.uswldtim == 0) {
            pline("%s 整个地消化你!", Monnam(mtmp));
            tmp = u.uhp;
            if (Half_physical_damage)
                tmp *= 2; /* sorry */
        } else {
            pline("%s%s 消化你!", Monnam(mtmp),
                  (u.uswldtim == 2) ? " 彻底地"
                                    : (u.uswldtim == 1) ? " 完全地" : "");
            exercise(A_STR, FALSE);
        }
        break;
    case AD_PHYS:
        physical_damage = TRUE;
        if (mtmp->data == &mons[PM_FOG_CLOUD]) {
            You("承载着水分并 %s",
                flaming(youmonst.data)
                    ? "烧完了!"
                    : Breathless ? "发现它有点不舒服."
                                 : amphibious(youmonst.data)
                                       ? "感到舒适."
                                       : "几乎不能呼吸!");
            /* NB: Amphibious includes Breathless */
            if (Amphibious && !flaming(youmonst.data))
                tmp = 0;
        } else {
            You("连续被碎渣打中!");
            exercise(A_STR, FALSE);
        }
        break;
    case AD_ACID:
        if (Acid_resistance) {
            You("被看似无害的黏性物质所覆盖.");
            tmp = 0;
        } else {
            if (Hallucination)
                pline("哎哟!  你已经黏液化了!");
            else
                You("被黏液覆盖了!  它灼烧了你!");
            exercise(A_STR, FALSE);
        }
        break;
    case AD_BLND:
        if (can_blnd(mtmp, &youmonst, mattk->aatyp, (struct obj *) 0)) {
            if (!Blind) {
                long was_blinded = Blinded;

                if (!Blinded)
                    You_cant("看见在这里!");
                make_blinded((long) tmp, FALSE);
                if (!was_blinded && !Blind)
                    Your1(vision_clears);
            } else
                /* keep him blind until disgorged */
                make_blinded(Blinded + 1, FALSE);
        }
        tmp = 0;
        break;
    case AD_ELEC:
        if (!mtmp->mcan && rn2(2)) {
            pline_The("你周围的空气充满了电.");
            if (Shock_resistance) {
                shieldeff(u.ux, u.uy);
                You("似乎没有受伤.");
                ugolemeffects(AD_ELEC, tmp);
                tmp = 0;
            }
        } else
            tmp = 0;
        break;
    case AD_COLD:
        if (!mtmp->mcan && rn2(2)) {
            if (Cold_resistance) {
                shieldeff(u.ux, u.uy);
                You_feel("轻微的寒冷.");
                ugolemeffects(AD_COLD, tmp);
                tmp = 0;
            } else
                You("快被冻死了!");
        } else
            tmp = 0;
        break;
    case AD_FIRE:
        if (!mtmp->mcan && rn2(2)) {
            if (Fire_resistance) {
                shieldeff(u.ux, u.uy);
                You_feel("轻微的热.");
                ugolemeffects(AD_FIRE, tmp);
                tmp = 0;
            } else
                You("快被烧脆了!");
            burn_away_slime();
        } else
            tmp = 0;
        break;
    case AD_DISE:
        if (!diseasemu(mtmp->data))
            tmp = 0;
        break;
    case AD_DREN:
        /* AC magic cancellation doesn't help when engulfed */
        if (!mtmp->mcan && rn2(4)) /* 75% chance */
            drain_en(tmp);
        tmp = 0;
        break;
    default:
        physical_damage = TRUE;
        tmp = 0;
        break;
    }

    if (physical_damage)
        tmp = Maybe_Half_Phys(tmp);

    mdamageu(mtmp, tmp);
    if (tmp)
        stop_occupation();

    if (!u.uswallow) {
        ; /* life-saving has already expelled swallowed hero */
    } else if (touch_petrifies(youmonst.data) && !resists_ston(mtmp)) {
        pline("%s非常匆忙地%s你!", Monnam(mtmp),
              is_animal(mtmp->data) ? "反胃出" : "喷出");
        expels(mtmp, mtmp->data, FALSE);
    } else if (!u.uswldtim || youmonst.data->msize >= MZ_HUGE) {
        /* As of 3.6.2: u.uswldtim used to be set to 0 by life-saving but it
           expels now so the !u.uswldtim case is no longer possible;
           however, polymorphing into a huge form while already
           swallowed is still possible */
        You("被%s出来!", is_animal(mtmp->data) ? "反胃" : "喷射");
        if (flags.verbose
            && (is_animal(mtmp->data)
                || (dmgtype(mtmp->data, AD_DGST) && Slow_digestion)))
            pline("%s明显不喜欢你的味道.", mon_nam(mtmp));
        expels(mtmp, mtmp->data, FALSE);
    }
    return 1;
}

/* monster explodes in your face */
STATIC_OVL int
explmu(mtmp, mattk, ufound)
struct monst *mtmp;
struct attack *mattk;
boolean ufound;
{
    boolean physical_damage = TRUE, kill_agr = TRUE;

    if (mtmp->mcan)
        return 0;

    if (!ufound) {
        pline("%s在%s中的某个地方爆炸了!",
              canseemon(mtmp) ? Monnam(mtmp) : "它",
              levl[mtmp->mux][mtmp->muy].typ == WATER ? "空空的水"
                                                      : "稀薄的空气");
    } else {
        int tmp = d((int) mattk->damn, (int) mattk->damd);
        boolean not_affected = defends((int) mattk->adtyp, uwep);

        hitmsg(mtmp, mattk);

        switch (mattk->adtyp) {
        case AD_COLD:
            physical_damage = FALSE;
            not_affected |= Cold_resistance;
            goto common;
        case AD_FIRE:
            physical_damage = FALSE;
            not_affected |= Fire_resistance;
            goto common;
        case AD_ELEC:
            physical_damage = FALSE;
            not_affected |= Shock_resistance;
            goto common;
        case AD_PHYS:
            /* there aren't any exploding creatures with AT_EXPL attack
               for AD_PHYS damage but there might be someday; without this,
               static analysis complains that 'physical_damage' is always
               False when tested below; it's right, but having that in
               place means one less thing to update if AD_PHYS gets added */
 common:

            if (!not_affected) {
                if (ACURR(A_DEX) > rnd(20)) {
                    You("躲避了一些爆炸.");
                    tmp = (tmp + 1) / 2;
                } else {
                    if (flags.verbose)
                        You("被炸到!");
                }
                if (mattk->adtyp == AD_FIRE)
                    burn_away_slime();
                if (physical_damage)
                    tmp = Maybe_Half_Phys(tmp);
                mdamageu(mtmp, tmp);
            }
            break;

        case AD_BLND:
            not_affected = resists_blnd(&youmonst);
            if (!not_affected) {
                /* sometimes you're affected even if it's invisible */
                if (mon_visible(mtmp) || (rnd(tmp /= 2) > u.ulevel)) {
                    You("被光的爆炸致失明!");
                    make_blinded((long) tmp, FALSE);
                    if (!Blind)
                        Your1(vision_clears);
                } else if (flags.verbose)
                    You("得到印象它不是可怕的光.");
            }
            break;

        case AD_HALU:
            not_affected |= Blind || (u.umonnum == PM_BLACK_LIGHT
                                      || u.umonnum == PM_VIOLET_FUNGUS
                                      || dmgtype(youmonst.data, AD_STUN));
            if (!not_affected) {
                boolean chg;
                if (!Hallucination)
                    You("被卷入了千变万化的光的爆炸!");
                /* avoid hallucinating the black light as it dies */
                mondead(mtmp);    /* remove it from map now */
                kill_agr = FALSE; /* already killed (maybe lifesaved) */
                chg =
                    make_hallucinated(HHallucination + (long) tmp, FALSE, 0L);
                You("%s.", chg ? "吓坏了" : "似乎不受影响");
            }
            break;

        default:
            break;
        }
        if (not_affected) {
            You("似乎不受它的影响.");
            ugolemeffects((int) mattk->adtyp, tmp);
        }
    }
    if (kill_agr)
        mondead(mtmp);
    wake_nearto(mtmp->mx, mtmp->my, 7 * 7);
    return (!DEADMONSTER(mtmp)) ? 0 : 2;
}

/* monster gazes at you */
int
gazemu(mtmp, mattk)
struct monst *mtmp;
struct attack *mattk;
{
    static const char *const reactions[] = {
        "混乱的",              /* [0] */
        "眩晕的",               /* [1] */
        "困惑的",   "目眩的",  /* [2,3] */
        "恼怒的", "激动的", /* [4,5] */
        "疲倦的",                 /* [6] */
        "无趣的",                /* [7] */
    };
    int react = -1;
    boolean cancelled = (mtmp->mcan != 0), already = FALSE;

    /* assumes that hero has to see monster's gaze in order to be
       affected, rather than monster just having to look at hero;
       when hallucinating, hero's brain doesn't register what
       it's seeing correctly so the gaze is usually ineffective
       [this could be taken a lot farther and select a gaze effect
       appropriate to what's currently being displayed, giving
       ordinary monsters a gaze attack when hero thinks he or she
       is facing a gazing creature, but let's not go that far...] */
    if (Hallucination && rn2(4))
        cancelled = TRUE;

    switch (mattk->adtyp) {
    case AD_STON:
        if (cancelled || !mtmp->mcansee) {
            if (!canseemon(mtmp))
                break; /* silently */
            pline("%s %s.", Monnam(mtmp),
                  (mtmp->data == &mons[PM_MEDUSA] && mtmp->mcan)
                      ? "看起来不那么难看"
                      : "凝视无效");
            break;
        }
        if (Reflecting && couldsee(mtmp->mx, mtmp->my)
            && mtmp->data == &mons[PM_MEDUSA]) {
            /* hero has line of sight to Medusa and she's not blind */
            boolean useeit = canseemon(mtmp);

            if (useeit)
                (void) ureflects("%s 凝视被你的 %s反射.",
                                 s_suffix(Monnam(mtmp)));
            if (mon_reflects(
                    mtmp, !useeit ? (char *) 0
                                  : "凝视被%s %s反射开!"))
                break;
            if (!m_canseeu(mtmp)) { /* probably you're invisible */
                if (useeit)
                    pline(
                      "%s 似乎没有注意%s 凝视被反射了.",
                          Monnam(mtmp), mhis(mtmp));
                break;
            }
            if (useeit)
                pline("%s 变成了石头!", Monnam(mtmp));
            stoned = TRUE;
            killed(mtmp);

            if (!DEADMONSTER(mtmp))
                break;
            return 2;
        }
        if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my)
            && !Stone_resistance) {
            You("接触到%s 凝视的目光.", s_suffix(mon_nam(mtmp)));
            stop_occupation();
            if (poly_when_stoned(youmonst.data) && polymon(PM_STONE_GOLEM))
                break;
            You("变成了石头...");
            killer.format = KILLED_BY;
            Strcpy(killer.name, mtmp->data->mname);
            done(STONING);
        }
        break;
    case AD_CONF:
        if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) && mtmp->mcansee
            && !mtmp->mspec_used && rn2(5)) {
            if (cancelled) {
                react = 0; /* "confused" */
                already = (mtmp->mconf != 0);
            } else {
                int conf = d(3, 4);

                mtmp->mspec_used = mtmp->mspec_used + (conf + rn2(6));
                if (!Confusion)
                    pline("%s 凝视混乱了你!", s_suffix(Monnam(mtmp)));
                else
                    You("变得越来越混乱.");
                make_confused(HConfusion + conf, FALSE);
                stop_occupation();
            }
        }
        break;
    case AD_STUN:
        if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) && mtmp->mcansee
            && !mtmp->mspec_used && rn2(5)) {
            if (cancelled) {
                react = 1; /* "stunned" */
                already = (mtmp->mstun != 0);
            } else {
                int stun = d(2, 6);

                mtmp->mspec_used = mtmp->mspec_used + (stun + rn2(6));
                pline("%s 尖锐地盯着你!", Monnam(mtmp));
                make_stunned((HStun & TIMEOUT) + (long) stun, TRUE);
                stop_occupation();
            }
        }
        break;
    case AD_BLND:
        if (canseemon(mtmp) && !resists_blnd(&youmonst)
            && distu(mtmp->mx, mtmp->my) <= BOLT_LIM * BOLT_LIM) {
            if (cancelled) {
                react = rn1(2, 2); /* "puzzled" || "dazzled" */
                already = (mtmp->mcansee == 0);
                /* Archons gaze every round; we don't want cancelled ones
                   giving the "seems puzzled/dazzled" message that often */
                if (mtmp->mcan && mtmp->data == &mons[PM_ARCHON] && rn2(5))
                    react = -1;
            } else {
                int blnd = d((int) mattk->damn, (int) mattk->damd);

                You("被%s 光芒致失明!", s_suffix(mon_nam(mtmp)));
                make_blinded((long) blnd, FALSE);
                stop_occupation();
                /* not blind at this point implies you're wearing
                   the Eyes of the Overworld; make them block this
                   particular stun attack too */
                if (!Blind) {
                    Your1(vision_clears);
                } else {
                    long oldstun = (HStun & TIMEOUT), newstun = (long) rnd(3);

                    /* we don't want to increment stun duration every time
                       or sighted hero will become incapacitated */
                    make_stunned(max(oldstun, newstun), TRUE);
                }
            }
        }
        break;
    case AD_FIRE:
        if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) && mtmp->mcansee
            && !mtmp->mspec_used && rn2(5)) {
            if (cancelled) {
                react = rn1(2, 4); /* "irritated" || "inflamed" */
            } else {
                int dmg = d(2, 6), lev = (int) mtmp->m_lev;

                pline("%s 用炽热的凝视攻击你!", Monnam(mtmp));
                stop_occupation();
                if (Fire_resistance) {
                    pline_The("火感觉不是很热!");
                    dmg = 0;
                }
                burn_away_slime();
                if (lev > rn2(20))
                    destroy_item(SCROLL_CLASS, AD_FIRE);
                if (lev > rn2(20))
                    destroy_item(POTION_CLASS, AD_FIRE);
                if (lev > rn2(25))
                    destroy_item(SPBOOK_CLASS, AD_FIRE);
                if (dmg)
                    mdamageu(mtmp, dmg);
            }
        }
        break;
#ifdef PM_BEHOLDER /* work in progress */
    case AD_SLEE:
        if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) && mtmp->mcansee
            && multi >= 0 && !rn2(5) && !Sleep_resistance) {
            if (cancelled) {
                react = 6;                      /* "tired" */
                already = (mtmp->mfrozen != 0); /* can't happen... */
            } else {
                fall_asleep(-rnd(10), TRUE);
                pline("%s gaze makes you very sleepy...",
                      s_suffix(Monnam(mtmp)));
            }
        }
        break;
    case AD_SLOW:
        if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my) && mtmp->mcansee
            && (HFast & (INTRINSIC | TIMEOUT)) && !defends(AD_SLOW, uwep)
            && !rn2(4)) {
            if (cancelled) {
                react = 7; /* "dulled" */
                already = (mtmp->mspeed == MSLOW);
            } else {
                u_slow_down();
                stop_occupation();
            }
        }
        break;
#endif /* BEHOLDER */
    default:
        impossible("Gaze attack %d?", mattk->adtyp);
        break;
    }
    if (react >= 0) {
        if (Hallucination && rn2(3))
            react = rn2(SIZE(reactions));
        /* cancelled/hallucinatory feedback; monster might look "confused",
           "stunned",&c but we don't actually set corresponding attribute */
        pline("%s 看起来 %s%s.", Monnam(mtmp),
              !rn2(3) ? "" : already ? "相当 "
                                     : (!rn2(2) ? "有一点儿 " : "稍微 "),
              reactions[react]);
    }
    return 0;
}

/* mtmp hits you for n points damage */
void
mdamageu(mtmp, n)
struct monst *mtmp;
int n;
{
    context.botl = 1;
    if (Upolyd) {
        u.mh -= n;
        if (u.mh < 1)
            rehumanize();
    } else {
        u.uhp -= n;
        if (u.uhp < 1)
            done_in_by(mtmp, DIED);
    }
}

/* returns 0 if seduction impossible,
 *         1 if fine,
 *         2 if wrong gender for nymph
 */
int
could_seduce(magr, mdef, mattk)
struct monst *magr, *mdef;
struct attack *mattk; /* non-Null: current attack; Null: general capability */
{
    struct permonst *pagr;
    boolean agrinvis, defperc;
    xchar genagr, gendef;
    int adtyp;

    if (is_animal(magr->data))
        return 0;
    if (magr == &youmonst) {
        pagr = youmonst.data;
        agrinvis = (Invis != 0);
        genagr = poly_gender();
    } else {
        pagr = magr->data;
        agrinvis = magr->minvis;
        genagr = gender(magr);
    }
    if (mdef == &youmonst) {
        defperc = (See_invisible != 0);
        gendef = poly_gender();
    } else {
        defperc = perceives(mdef->data);
        gendef = gender(mdef);
    }

    adtyp = mattk ? mattk->adtyp
            : dmgtype(pagr, AD_SSEX) ? AD_SSEX
              : dmgtype(pagr, AD_SEDU) ? AD_SEDU
                : AD_PHYS;
    if (adtyp == AD_SSEX && !SYSOPT_SEDUCE)
        adtyp = AD_SEDU;

    if (agrinvis && !defperc && adtyp == AD_SEDU)
        return 0;

    /* nymphs have two attacks, one for steal-item damage and the other
       for seduction, both pass the could_seduce() test;
       incubi/succubi have three attacks, their claw attacks for damage
       don't pass the test */
    if ((pagr->mlet != S_NYMPH
         && pagr != &mons[PM_INCUBUS] && pagr != &mons[PM_SUCCUBUS])
        || (adtyp != AD_SEDU && adtyp != AD_SSEX && adtyp != AD_SITM))
        return 0;

    return (genagr == 1 - gendef) ? 1 : (pagr->mlet == S_NYMPH) ? 2 : 0;
}

/* returns 1 if monster teleported (or hero leaves monster's vicinity) */
int
doseduce(mon)
struct monst *mon;
{
    struct obj *ring, *nring;
    boolean fem = (mon->data == &mons[PM_SUCCUBUS]); /* otherwise incubus */
    boolean seewho, naked; /* True iff no armor */
    int attr_tot, tried_gloves = 0;
    char qbuf[QBUFSZ], Who[QBUFSZ];

    if (mon->mcan || mon->mspec_used) {
        pline("%s 的行为好像%s得了%s头痛.", Monnam(mon),
              mhe(mon), mon->mcan ? "严重的 " : "");
        return 0;
    }
    if (unconscious()) {
        pline("%s 似乎对你的没有反应感到沮丧.", Monnam(mon));
        return 0;
    }
    seewho = canseemon(mon);
    if (!seewho)
        pline("有人爱抚你...");
    else
        You_feel("很受%s的吸引.", mon_nam(mon));
    /* cache the seducer's name in a local buffer */
    Strcpy(Who, (!seewho ? (fem ? "她" : "他") : Monnam(mon)));

    /* if in the process of putting armor on or taking armor off,
       interrupt that activity now */
    (void) stop_donning((struct obj *) 0);
    /* don't try to take off gloves if cursed weapon blocks them */
    if (welded(uwep))
        tried_gloves = 1;

    for (ring = invent; ring; ring = nring) {
        nring = ring->nobj;
        if (ring->otyp != RIN_ADORNMENT)
            continue;
        if (fem) {
            if (ring->owornmask && uarmg) {
                /* don't take off worn ring if gloves are in the way */
                if (!tried_gloves++)
                    mayberem(mon, Who, uarmg, "手套");
                if (uarmg)
                    continue; /* next ring might not be worn */
            }
            /* confirmation prompt when charisma is high bypassed if deaf */
            if (!Deaf && rn2(20) < ACURR(A_CHA)) {
                (void) safe_qbuf(qbuf, "\"那个 ",
                                 "看起来很漂亮.  我可以拥有它吗?\"", ring,
                                 xname, simpleonames, "ring");
                makeknown(RIN_ADORNMENT);
                if (yn(qbuf) == 'n')
                    continue;
            } else
                pline("%s决定要%s, 然后拿走了它.",
                      Who, yname(ring));
            makeknown(RIN_ADORNMENT);
            /* might be in left or right ring slot or weapon/alt-wep/quiver */
            if (ring->owornmask)
                remove_worn_item(ring, FALSE);
            freeinv(ring);
            (void) mpickobj(mon, ring);
        } else {
            if (uleft && uright && uleft->otyp == RIN_ADORNMENT
                && uright->otyp == RIN_ADORNMENT)
                break;
            if (ring == uleft || ring == uright)
                continue;
            if (uarmg) {
                /* don't put on ring if gloves are in the way */
                if (!tried_gloves++)
                    mayberem(mon, Who, uarmg, "手套");
                if (uarmg)
                    break; /* no point trying further rings */
            }
            /* confirmation prompt when charisma is high bypassed if deaf */
            if (!Deaf && rn2(20) < ACURR(A_CHA)) {
                (void) safe_qbuf(qbuf, "\"那个",
                                 "看起来很漂亮.  你会为我戴上它吗?\"",
                                 ring, xname, simpleonames, "ring");
                makeknown(RIN_ADORNMENT);
                if (yn(qbuf) == 'n')
                    continue;
            } else {
                pline("%s决定你戴上%s会更漂亮,",
                      Who, yname(ring));
                pline("并把它戴到你的手指上.");
            }
            makeknown(RIN_ADORNMENT);
            if (!uright) {
                pline("%s把%s戴到你的右%s.",
                      Who, the(xname(ring)), body_part(HAND));
                setworn(ring, RIGHT_RING);
            } else if (!uleft) {
                pline("%s把%s戴到你的左%s.",
                      Who, the(xname(ring)), body_part(HAND));
                setworn(ring, LEFT_RING);
            } else if (uright && uright->otyp != RIN_ADORNMENT) {
                /* note: the "replaces" message might be inaccurate if
                   hero's location changes and the process gets interrupted,
                   but trying to figure that out in advance in order to use
                   alternate wording is not worth the effort */
                pline("%s把%s替换为%s.",
                      Who, yname(uright), yname(ring));
                Ring_gone(uright);
                /* ring removal might cause loss of levitation which could
                   drop hero onto trap that transports hero somewhere else */
                if (u.utotype || distu(mon->mx, mon->my) > 2)
                    return 1;
                setworn(ring, RIGHT_RING);
            } else if (uleft && uleft->otyp != RIN_ADORNMENT) {
                /* see "replaces" note above */
                pline("%s把%s替换为%s.",
                      Who, yname(uleft), yname(ring));
                Ring_gone(uleft);
                if (u.utotype || distu(mon->mx, mon->my) > 2)
                    return 1;
                setworn(ring, LEFT_RING);
            } else
                impossible("ring replacement");
            Ring_on(ring);
            prinv((char *) 0, ring, 0L);
        }
    }

    naked = (!uarmc && !uarmf && !uarmg && !uarms && !uarmh && !uarmu);
    pline("%s %s%s.", Who,
          Deaf ? "似乎在你耳边低语"
               : naked ? "在你耳边低语情话"
                       : "在你耳边低语",
          naked ? "" : ", 并帮你脱衣服");
    mayberem(mon, Who, uarmc, cloak_simple_name(uarmc));
    if (!uarmc)
        mayberem(mon, Who, uarm, suit_simple_name(uarm));
    mayberem(mon, Who, uarmf, "靴子");
    if (!tried_gloves)
        mayberem(mon, Who, uarmg, "手套");
    mayberem(mon, Who, uarms, "盾牌");
    mayberem(mon, Who, uarmh, helm_simple_name(uarmh));
    if (!uarmc && !uarm)
        mayberem(mon, Who, uarmu, "衬衫");

    /* removing armor (levitation boots, or levitation ring to make
       room for adornment ring with incubus case) might result in the
       hero falling through a trap door or landing on a teleport trap
       and changing location, so hero might not be adjacent to seducer
       any more (mayberem() has its own adjacency test so we don't need
       to check after each potential removal) */
    if (u.utotype || distu(mon->mx, mon->my) > 2)
        return 1;

    if (uarm || uarmc) {
        if (!Deaf)
            verbalize("你真是个%s; 我希望...",
                      flags.female ? "甜美女孩" : "好男孩");
        else if (seewho)
            pline("%s似乎在叹息.", Monnam(mon));
        /* else no regret message if can't see or hear seducer */

        if (!tele_restrict(mon))
            (void) rloc(mon, TRUE);
        return 1;
    }
    if (u.ualign.type == A_CHAOTIC)
        adjalign(1);

    /* by this point you have discovered mon's identity, blind or not... */
    pline("时间静止在你和 %s躺在彼此的怀里...",
          noit_mon_nam(mon));
    /* 3.6.1: a combined total for charisma plus intelligence of 35-1
       used to guarantee successful outcome; now total maxes out at 32
       as far as deciding what will happen; chance for bad outcome when
       Cha+Int is 32 or more is 2/35, a bit over 5.7% */
    attr_tot = ACURR(A_CHA) + ACURR(A_INT);
    if (rn2(35) > min(attr_tot, 32)) {
        /* Don't bother with mspec_used here... it didn't get tired! */
        pline("%s 似乎比你更为享受...",
              noit_Monnam(mon));
        switch (rn2(5)) {
        case 0:
            You_feel("耗尽了能量.");
            u.uen = 0;
            u.uenmax -= rnd(Half_physical_damage ? 5 : 10);
            exercise(A_CON, FALSE);
            if (u.uenmax < 0)
                u.uenmax = 0;
            break;
        case 1:
            You("情绪低落.");
            (void) adjattrib(A_CON, -1, TRUE);
            exercise(A_CON, FALSE);
            context.botl = 1;
            break;
        case 2:
            Your("感官迟钝.");
            (void) adjattrib(A_WIS, -1, TRUE);
            exercise(A_WIS, FALSE);
            context.botl = 1;
            break;
        case 3:
            if (!resists_drli(&youmonst)) {
                You_feel("不成样子.");
                losexp("过度费力");
            } else {
                You("有一种古怪的感觉...");
            }
            exercise(A_CON, FALSE);
            exercise(A_DEX, FALSE);
            exercise(A_WIS, FALSE);
            break;
        case 4: {
            int tmp;

            You_feel("疲惫的.");
            exercise(A_STR, FALSE);
            tmp = rn1(10, 6);
            losehp(Maybe_Half_Phys(tmp), "精疲力竭", DIE_OF);
            break;
        } /* case 4 */
        } /* switch */
    } else {
        mon->mspec_used = rnd(100); /* monster is worn out */
        You("似乎比%s更为享受...", noit_mon_nam(mon));
        switch (rn2(5)) {
        case 0:
            You_feel("唤起了你的全部潜能.");
            exercise(A_CON, TRUE);
            u.uen = (u.uenmax += rnd(5));
            break;
        case 1:
            You_feel("足够的好来再做一次.");
            (void) adjattrib(A_CON, 1, TRUE);
            exercise(A_CON, TRUE);
            context.botl = 1;
            break;
        case 2:
            You("会永远记住%s...", noit_mon_nam(mon));
            (void) adjattrib(A_WIS, 1, TRUE);
            exercise(A_WIS, TRUE);
            context.botl = 1;
            break;
        case 3:
            pline("这是一个非常有教育性的经验.");
            pluslvl(FALSE);
            exercise(A_WIS, TRUE);
            break;
        case 4:
            You_feel("恢复了健康!");
            u.uhp = u.uhpmax;
            if (Upolyd)
                u.mh = u.mhmax;
            exercise(A_STR, TRUE);
            context.botl = 1;
            break;
        }
    }

    if (mon->mtame) { /* don't charge */
        ;
    } else if (rn2(20) < ACURR(A_CHA)) {
        pline("%s要求你付钱给%s, 但你拒绝了...",
              noit_Monnam(mon), noit_mhim(mon));
    } else if (u.umonnum == PM_LEPRECHAUN) {
        pline("%s试图拿走你的钱, 但失败了...", noit_Monnam(mon));
    } else {
        long cost;
        long umoney = money_cnt(invent);

        if (umoney > (long) LARGEST_INT - 10L)
            cost = (long) rnd(LARGEST_INT) + 500L;
        else
            cost = (long) rnd((int) umoney + 10) + 500L;
        if (mon->mpeaceful) {
            cost /= 5L;
            if (!cost)
                cost = 1L;
        }
        if (cost > umoney)
            cost = umoney;
        if (!cost) {
            verbalize("本次免费!");
        } else {
            pline("%s拿走%ld %s作为服务报答!", noit_Monnam(mon),
                  cost, currency(cost));
            money2mon(mon, cost);
            context.botl = 1;
        }
    }
    if (!rn2(25))
        mon->mcan = 1; /* monster is worn out */
    if (!tele_restrict(mon))
        (void) rloc(mon, TRUE);
    return 1;
}

STATIC_OVL void
mayberem(mon, seducer, obj, str)
struct monst *mon;
const char *seducer; /* only used for alternate message */
struct obj *obj;
const char *str;
{
    char qbuf[QBUFSZ];

    if (!obj || !obj->owornmask)
        return;
    /* removal of a previous item might have sent the hero elsewhere
       (loss of levitation that leads to landing on a transport trap) */
    if (u.utotype || distu(mon->mx, mon->my) > 2)
        return;

    /* being deaf overrides confirmation prompt for high charisma */
    if (Deaf) {
        pline("%s脱掉了你的%s.", seducer, str);
    } else if (rn2(20) < ACURR(A_CHA)) {
        Sprintf(qbuf, "\"要我脱掉你的%s, %s?\"", str,
                (!rn2(2) ? "情人" : !rn2(2) ? "亲爱的" : "甜心"));
        if (yn(qbuf) == 'n')
            return;
    } else {
        char hairbuf[BUFSZ];

        Sprintf(hairbuf, "让我把我的手指穿过你的 %s",
                body_part(HAIR));
        verbalize("脱掉你的 %s; %s.", str,
                  (obj == uarm)
                     ? "让我们离得更近一点"
                     : (obj == uarmc || obj == uarms)
                        ? "就是这样"
                        : (obj == uarmf)
                           ? "让我擦你的脚"
                           : (obj == uarmg)
                              ? "它们太笨拙了"
                              : (obj == uarmu)
                                 ? "让我来给你按摩"
                                 /* obj == uarmh */
                                 : hairbuf);
    }
    remove_worn_item(obj, TRUE);
}

/* FIXME:
 *  sequencing issue:  a monster's attack might cause poly'd hero
 *  to revert to normal form.  The messages for passive counterattack
 *  would look better if they came before reverting form, but we need
 *  to know whether hero reverted in order to decide whether passive
 *  damage applies.
 */
STATIC_OVL int
passiveum(olduasmon, mtmp, mattk)
struct permonst *olduasmon;
struct monst *mtmp;
struct attack *mattk;
{
    int i, tmp;
    struct attack *oldu_mattk = 0;

    /*
     * mattk      == mtmp's attack that hit you;
     * oldu_mattk == your passive counterattack (even if mtmp's attack
     *               has already caused you to revert to normal form).
     */
    for (i = 0; !oldu_mattk; i++) {
        if (i >= NATTK)
            return 1;
        if (olduasmon->mattk[i].aatyp == AT_NONE
            || olduasmon->mattk[i].aatyp == AT_BOOM)
            oldu_mattk = &olduasmon->mattk[i];
    }
    if (oldu_mattk->damn)
        tmp = d((int) oldu_mattk->damn, (int) oldu_mattk->damd);
    else if (oldu_mattk->damd)
        tmp = d((int) olduasmon->mlevel + 1, (int) oldu_mattk->damd);
    else
        tmp = 0;

    /* These affect the enemy even if you were "killed" (rehumanized) */
    switch (oldu_mattk->adtyp) {
    case AD_ACID:
        if (!rn2(2)) {
            pline("%s 被%s%s溅到了!", Monnam(mtmp),
                  /* temporary? hack for sequencing issue:  "your acid"
                     looks strange coming immediately after player has
                     been told that hero has reverted to normal form */
                  !Upolyd ? "" : "你的", hliquid("酸"));
            if (resists_acid(mtmp)) {
                pline("%s 不受影响.", Monnam(mtmp));
                tmp = 0;
            }
        } else
            tmp = 0;
        if (!rn2(30))
            erode_armor(mtmp, ERODE_CORRODE);
        if (!rn2(6))
            acid_damage(MON_WEP(mtmp));
        goto assess_dmg;
    case AD_STON: /* cockatrice */
    {
        long protector = attk_protection((int) mattk->aatyp),
             wornitems = mtmp->misc_worn_check;

        /* wielded weapon gives same protection as gloves here */
        if (MON_WEP(mtmp) != 0)
            wornitems |= W_ARMG;

        if (!resists_ston(mtmp)
            && (protector == 0L
                || (protector != ~0L
                    && (wornitems & protector) != protector))) {
            if (poly_when_stoned(mtmp->data)) {
                mon_to_stone(mtmp);
                return 1;
            }
            pline("%s 变成了石头!", Monnam(mtmp));
            stoned = 1;
            xkilled(mtmp, XKILL_NOMSG);
            if (!DEADMONSTER(mtmp))
                return 1;
            return 2;
        }
        return 1;
    }
    case AD_ENCH: /* KMH -- remove enchantment (disenchanter) */
        if (mon_currwep) {
            /* by_you==True: passive counterattack to hero's action
               is hero's fault */
            (void) drain_item(mon_currwep, TRUE);
            /* No message */
        }
        return 1;
    default:
        break;
    }
    if (!Upolyd)
        return 1;

    /* These affect the enemy only if you are still a monster */
    if (rn2(3))
        switch (oldu_mattk->adtyp) {
        case AD_PHYS:
            if (oldu_mattk->aatyp == AT_BOOM) {
                You("爆炸了!");
                /* KMH, balance patch -- this is okay with unchanging */
                rehumanize();
                goto assess_dmg;
            }
            break;
        case AD_PLYS: /* Floating eye */
            if (tmp > 127)
                tmp = 127;
            if (u.umonnum == PM_FLOATING_EYE) {
                if (!rn2(4))
                    tmp = 127;
                if (mtmp->mcansee && haseyes(mtmp->data) && rn2(3)
                    && (perceives(mtmp->data) || !Invis)) {
                    if (Blind)
                        pline("作为失明的%s, 你不能保护自己.",
                              youmonst.data->mname);
                    else {
                        if (mon_reflects(mtmp,
                                         "你的凝视被%s%s反射."))
                            return 1;
                        pline("%s被你的凝视僵住!", Monnam(mtmp));
                        paralyze_monst(mtmp, tmp);
                        return 3;
                    }
                }
            } else { /* gelatinous cube */
                pline("%s被你僵住了.", Monnam(mtmp));
                paralyze_monst(mtmp, tmp);
                return 3;
            }
            return 1;
        case AD_COLD: /* Brown mold or blue jelly */
            if (resists_cold(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline("%s轻微的寒冷.", Monnam(mtmp));
                golemeffects(mtmp, AD_COLD, tmp);
                tmp = 0;
                break;
            }
            pline("%s突然非常热!", Monnam(mtmp));
            u.mh += tmp / 2;
            if (u.mhmax < u.mh)
                u.mhmax = u.mh;
            if (u.mhmax > ((youmonst.data->mlevel + 1) * 8))
                (void) split_mon(&youmonst, mtmp);
            break;
        case AD_STUN: /* Yellow mold */
            if (!mtmp->mstun) {
                mtmp->mstun = 1;
                pline("%s %s.", Monnam(mtmp),
                      makeplural(stagger(mtmp->data, "蹒跚")));
            }
            tmp = 0;
            break;
        case AD_FIRE: /* Red mold */
            if (resists_fire(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline("%s轻微的温暖.", Monnam(mtmp));
                golemeffects(mtmp, AD_FIRE, tmp);
                tmp = 0;
                break;
            }
            pline("%s突然非常热!", Monnam(mtmp));
            break;
        case AD_ELEC:
            if (resists_elec(mtmp)) {
                shieldeff(mtmp->mx, mtmp->my);
                pline("%s轻微的刺痛.", Monnam(mtmp));
                golemeffects(mtmp, AD_ELEC, tmp);
                tmp = 0;
                break;
            }
            pline("%s被你电击到!", Monnam(mtmp));
            break;
        default:
            tmp = 0;
            break;
        }
    else
        tmp = 0;

 assess_dmg:
    if ((mtmp->mhp -= tmp) <= 0) {
        pline("%s 死了!", Monnam(mtmp));
        xkilled(mtmp, XKILL_NOMSG);
        if (!DEADMONSTER(mtmp))
            return 1;
        return 2;
    }
    return 1;
}

struct monst *
cloneu()
{
    struct monst *mon;
    int mndx = monsndx(youmonst.data);

    if (u.mh <= 1)
        return (struct monst *) 0;
    if (mvitals[mndx].mvflags & G_EXTINCT)
        return (struct monst *) 0;
    mon = makemon(youmonst.data, u.ux, u.uy, NO_MINVENT | MM_EDOG);
    if (!mon)
        return NULL;
    mon->mcloned = 1;
    mon = christen_monst(mon, plname);
    initedog(mon);
    mon->m_lev = youmonst.data->mlevel;
    mon->mhpmax = u.mhmax;
    mon->mhp = u.mh / 2;
    u.mh -= mon->mhp;
    context.botl = 1;
    return mon;
}

/*mhitu.c*/
