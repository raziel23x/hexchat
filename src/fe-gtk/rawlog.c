/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "fe-gtk.h"

#include <gtk/gtkbutton.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtkstock.h>
#include <gdk/gdkkeysyms.h>

#include "../common/hexchat.h"
#include "../common/hexchatc.h"
#include "../common/cfgfiles.h"
#include "../common/server.h"
#include "gtkutil.h"
#include "palette.h"
#include "maingui.h"
#include "rawlog.h"
#include "xtext.h"
#include "fkeys.h"

static void
close_rawlog (GtkWidget *wid, server *serv)
{
	if (is_server (serv))
		serv->gui->rawlog_window = 0;
}

static void
rawlog_save (server *serv, char *file)
{
	int fh = -1;

	if (file)
	{
		if (serv->gui->rawlog_window)
			fh = hexchat_open_file (file, O_TRUNC | O_WRONLY | O_CREAT,
										 0600, XOF_DOMODE | XOF_FULLPATH);
		if (fh != -1)
		{
			gtk_xtext_save (GTK_XTEXT (serv->gui->rawlog_textlist), fh);
			close (fh);
		}
	}
}

static int
rawlog_clearbutton (GtkWidget * wid, server *serv)
{
	gtk_xtext_clear (GTK_XTEXT (serv->gui->rawlog_textlist)->buffer, 0);
	return FALSE;
}

static int
rawlog_savebutton (GtkWidget * wid, server *serv)
{
	gtkutil_file_req (_("Save As..."), rawlog_save, serv, NULL, NULL, FRF_WRITE);
	return FALSE;
}

static void
rawlog_key_cb (GtkWidget * wid, GdkEventKey * key, gpointer userdata)
{
	/* Copy rawlog selection to clipboard when Ctrl+Shift+C is pressed,
	 * but make sure not to copy twice, i.e. when auto-copy is enabled.
	 */
	if (!prefs.hex_text_autocopy_text &&
		(key->keyval == GDK_c || key->keyval == GDK_C) &&
		key->state & STATE_SHIFT &&
		key->state & STATE_CTRL)
	{
		gtk_xtext_copy_selection (userdata);
	}
	/* close_rawlog is given to mg_create_generic_tab as
	 * close_callback, it should take care of the rest.
	 */
	else if (key->keyval == GDK_Escape)
	{
		gtk_widget_destroy (wid);
	}
}

void
open_rawlog (struct server *serv)
{
	GtkWidget *hbox, *vscrollbar, *vbox;
	char tbuf[256];

	if (serv->gui->rawlog_window)
	{
		mg_bring_tofront (serv->gui->rawlog_window);
		return;
	}

	snprintf (tbuf, sizeof tbuf, _(DISPLAY_NAME": Raw Log (%s)"), serv->servername);
	serv->gui->rawlog_window =
		mg_create_generic_tab ("RawLog", tbuf, FALSE, TRUE, close_rawlog, serv,
							 640, 320, &vbox, serv);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
	gtk_widget_show (hbox);

	serv->gui->rawlog_textlist = gtk_xtext_new (colors, 0);
	gtk_xtext_set_tint (GTK_XTEXT (serv->gui->rawlog_textlist), prefs.hex_text_tint_red, prefs.hex_text_tint_green, prefs.hex_text_tint_blue);
	gtk_xtext_set_background (GTK_XTEXT (serv->gui->rawlog_textlist),
									  channelwin_pix, prefs.hex_text_transparent);

	gtk_container_add (GTK_CONTAINER (hbox), serv->gui->rawlog_textlist);
	gtk_xtext_set_font (GTK_XTEXT (serv->gui->rawlog_textlist), prefs.hex_text_font);
	GTK_XTEXT (serv->gui->rawlog_textlist)->ignore_hidden = 1;
	gtk_widget_show (serv->gui->rawlog_textlist);

	vscrollbar = gtk_vscrollbar_new (GTK_XTEXT (serv->gui->rawlog_textlist)->adj);
	gtk_box_pack_start (GTK_BOX (hbox), vscrollbar, FALSE, FALSE, 0);
	show_and_unfocus (vscrollbar);

	hbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_SPREAD);
	gtk_box_pack_end (GTK_BOX (vbox), hbox, 0, 0, 0);
	gtk_widget_show (hbox);

	gtkutil_button (hbox, GTK_STOCK_CLEAR, NULL, rawlog_clearbutton,
						 serv, _("Clear Raw Log"));

	gtkutil_button (hbox, GTK_STOCK_SAVE_AS, NULL, rawlog_savebutton,
						 serv, _("Save As..."));

	/* Copy selection to clipboard when Ctrl+Shift+C is pressed AND text auto-copy is disabled */
	g_signal_connect (G_OBJECT (serv->gui->rawlog_window), "key_press_event", G_CALLBACK (rawlog_key_cb), serv->gui->rawlog_textlist);

	gtk_widget_show (serv->gui->rawlog_window);
}

void
fe_add_rawlog (server *serv, char *text, int len, int outbound)
{
	char *new_text;

	if (!serv->gui->rawlog_window)
		return;

	new_text = malloc (len + 7);

	len = sprintf (new_text, "\0033>>\017 %s", text);
	if (outbound)
	{
		new_text[1] = '4';
		new_text[2] = '<';
		new_text[3] = '<';
	}
	gtk_xtext_append (GTK_XTEXT (serv->gui->rawlog_textlist)->buffer, new_text, len);
	free (new_text);
}
