/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef _PURPLE_SMILEY_PARSER_H_
#define _PURPLE_SMILEY_PARSER_H_

#include "purple.h"

typedef void (*PurpleSmileyParseCb)(GString *out, PurpleSmiley *smiley,
	PurpleConversation *conv, gpointer ui_data);

/* XXX: this shouldn't be a conv for incoming messages - see
 * PurpleConversationPrivate#remote_smileys.
 * For outgoing, we could pass conv in ui_data (or something).
 *
 * @ui_data is passed to @cb and #purple_smiley_theme_get_smileys.
 *
 * XXX: we might not want to display remote smileys for
 * outgoing messages. To be considered.
 */
gchar *
purple_smiley_parse(PurpleConversation *conv, const gchar *html_message,
	PurpleSmileyParseCb cb, gpointer ui_data);

GList *
purple_smiley_find(PurpleSmileyList *smileys, const gchar *html_message);

#endif /* _PURPLE_SMILEY_PARSER_H_ */