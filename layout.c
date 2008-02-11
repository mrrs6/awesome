/*
 * layout.c - layout management
 *
 * Copyright © 2007 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "tag.h"
#include "xutil.h"
#include "focus.h"
#include "widget.h"
#include "window.h"
#include "client.h"
#include "screen.h"
#include "layouts/tile.h"
#include "layouts/max.h"
#include "layouts/fibonacci.h"
#include "layouts/floating.h"

extern AwesomeConf globalconf;

#include "layoutgen.h"

/** Arrange windows following current selected layout
 * \param screen the screen to arrange
 */
static void
arrange(int screen)
{
    Client *c;
    Layout *curlay = get_current_layout(screen);
    unsigned int dui;
    int di, x, y;
    Window rootwin, childwin;

    for(c = globalconf.clients; c; c = c->next)
    {
        if(client_isvisible(c, screen) && !c->newcomer)
            client_unban(c);
        /* we don't touch other screens windows */
        else if(c->screen == screen || c->newcomer)
            client_ban(c);
    }

    curlay->arrange(screen);

    for(c = globalconf.clients; c; c = c->next)
        if(c->newcomer && client_isvisible(c, screen))
        {
            c->newcomer = False;
            client_unban(c);
            if(globalconf.screens[screen].new_get_focus)
                client_focus(c, screen, True);
        }

    /* if we have a valid client that could be focused but currently no window
     * are focused, then set the focus on this window */
    if((c = focus_get_current_client(screen)) && !globalconf.focus->client)
        client_focus(c, screen, True);

    /* check that the mouse is on a window or not */
    if(XQueryPointer(globalconf.display, RootWindow(globalconf.display,
                                                    get_phys_screen(screen)),
                     &rootwin, &childwin, &x, &y, &di, &di, &dui)
       && (rootwin == None || childwin == None || childwin == rootwin))
        window_root_grabbuttons(get_phys_screen(screen));

    /* reset status */
    globalconf.screens[screen].need_arrange = False;
}

int
layout_refresh(void)
{
    int screen;
    int arranged = 0;

    for(screen = 0; screen < globalconf.nscreen; screen++)
        if(globalconf.screens[screen].need_arrange)
        {
            arrange(screen);
            arranged++;
        }

    return arranged;
}

Layout *
get_current_layout(int screen)
{
    Tag **curtags = tags_get_current(screen);
    Layout *l = curtags[0]->layout;
    p_delete(&curtags);
    return l;
}

void
loadawesomeprops(int screen)
{
    int i, ntags = 0;
    char *prop;
    Tag *tag;

    for(tag = globalconf.screens[screen].tags; tag; tag = tag->next)
        ntags++;

    prop = p_new(char, ntags + 1);

    if(xgettextprop(RootWindow(globalconf.display, get_phys_screen(screen)),
                    XInternAtom(globalconf.display, "_AWESOME_PROPERTIES", False),
                    prop, ntags + 1))
        for(i = 0, tag = globalconf.screens[screen].tags; tag && prop[i]; i++, tag = tag->next)
            tag_view_byindex(screen, i, prop[i] == '1');

    p_delete(&prop);
}

void
saveawesomeprops(int screen)
{
    int i, ntags = 0;
    char *prop;
    Tag *tag;

    for(tag = globalconf.screens[screen].tags; tag; tag = tag->next)
        ntags++;

    prop = p_new(char, ntags + 1);

    for(i = 0, tag = globalconf.screens[screen].tags; tag; tag = tag->next, i++)
        prop[i] = tag->selected ? '1' : '0';

    prop[i] = '\0';
    XChangeProperty(globalconf.display,
                    RootWindow(globalconf.display, get_phys_screen(screen)),
                    XInternAtom(globalconf.display, "_AWESOME_PROPERTIES", False),
                    XA_STRING, 8, PropModeReplace, (unsigned char *) prop, i);
    p_delete(&prop);
}

/** Set layout for tag
 * \param screen Screen ID
 * \param arg Layout specifier
 * \ingroup ui_callback
 */
void
uicb_tag_setlayout(int screen, char *arg)
{
    Layout *l = globalconf.screens[screen].layouts;
    Tag *tag, **curtags;
    int i;

    if(arg)
    {
        curtags = tags_get_current(screen);
        for(i = 0; l && l != curtags[0]->layout; i++, l = l->next);
        p_delete(&curtags);

        if(!l)
            i = 0;

        i = compute_new_value_from_arg(arg, (double) i);

        if(i >= 0)
            for(l = globalconf.screens[screen].layouts; l && i > 0; i--)
                 l = l->next;
        else
            for(l = globalconf.screens[screen].layouts; l && i < 0; i++)
                 l = layout_list_prev_cycle(&globalconf.screens[screen].layouts, l);

        if(!l)
            l = globalconf.screens[screen].layouts;
    }

    for(tag = globalconf.screens[screen].tags; tag; tag = tag->next)
        if(tag->selected)
            tag->layout = l;

    if(globalconf.focus->client)
        arrange(screen);

    widget_invalidate_cache(screen, WIDGET_CACHE_LAYOUTS);

    saveawesomeprops(screen);
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
