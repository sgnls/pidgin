/*
 * purple - Jabber Protocol Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"
#include "core.h"
#include "debug.h"
#include "prefs.h"
#include "util.h"

#include "buddy.h"
#include "disco.h"
#include "google.h"
#include "iq.h"
#include "jingle/jingle.h"
#include "oob.h"
#include "roster.h"
#include "si.h"
#include "ping.h"
#include "adhoccommands.h"
#include "data.h"
#include "ibb.h"

#ifdef _WIN32
#include "utsname.h"
#endif

GHashTable *iq_handlers = NULL;


JabberIq *jabber_iq_new(JabberStream *js, JabberIqType type)
{
	JabberIq *iq;

	iq = g_new0(JabberIq, 1);

	iq->type = type;

	iq->node = xmlnode_new("iq");
	switch(iq->type) {
		case JABBER_IQ_SET:
			xmlnode_set_attrib(iq->node, "type", "set");
			break;
		case JABBER_IQ_GET:
			xmlnode_set_attrib(iq->node, "type", "get");
			break;
		case JABBER_IQ_ERROR:
			xmlnode_set_attrib(iq->node, "type", "error");
			break;
		case JABBER_IQ_RESULT:
			xmlnode_set_attrib(iq->node, "type", "result");
			break;
		case JABBER_IQ_NONE:
			/* this shouldn't ever happen */
			break;
	}

	iq->js = js;

	if(type == JABBER_IQ_GET || type == JABBER_IQ_SET) {
		iq->id = jabber_get_next_id(js);
		xmlnode_set_attrib(iq->node, "id", iq->id);
	}

	return iq;
}

JabberIq *jabber_iq_new_query(JabberStream *js, JabberIqType type,
		const char *xmlns)
{
	JabberIq *iq = jabber_iq_new(js, type);
	xmlnode *query;

	query = xmlnode_new_child(iq->node, "query");
	xmlnode_set_namespace(query, xmlns);

	return iq;
}

typedef struct _JabberCallbackData {
	JabberIqCallback *callback;
	gpointer data;
} JabberCallbackData;

void
jabber_iq_set_callback(JabberIq *iq, JabberIqCallback *callback, gpointer data)
{
	iq->callback = callback;
	iq->callback_data = data;
}

void jabber_iq_set_id(JabberIq *iq, const char *id)
{
	g_free(iq->id);

	if(id) {
		xmlnode_set_attrib(iq->node, "id", id);
		iq->id = g_strdup(id);
	} else {
		xmlnode_remove_attrib(iq->node, "id");
		iq->id = NULL;
	}
}

void jabber_iq_send(JabberIq *iq)
{
	JabberCallbackData *jcd;
	g_return_if_fail(iq != NULL);

	jabber_send(iq->js, iq->node);

	if(iq->id && iq->callback) {
		jcd = g_new0(JabberCallbackData, 1);
		jcd->callback = iq->callback;
		jcd->data = iq->callback_data;
		g_hash_table_insert(iq->js->iq_callbacks, g_strdup(iq->id), jcd);
	}

	jabber_iq_free(iq);
}

void jabber_iq_free(JabberIq *iq)
{
	g_return_if_fail(iq != NULL);

	g_free(iq->id);
	xmlnode_free(iq->node);
	g_free(iq);
}

static void jabber_iq_last_parse(JabberStream *js, const char *from,
                                 JabberIqType type, const char *id,
                                 xmlnode *packet)
{
	JabberIq *iq;
	xmlnode *query;
	char *idle_time;

	if(type == JABBER_IQ_GET) {
		iq = jabber_iq_new_query(js, JABBER_IQ_RESULT, "jabber:iq:last");
		jabber_iq_set_id(iq, id);
		if (from)
			xmlnode_set_attrib(iq->node, "to", from);

		query = xmlnode_get_child(iq->node, "query");

		idle_time = g_strdup_printf("%ld", js->idle ? time(NULL) - js->idle : 0);
		xmlnode_set_attrib(query, "seconds", idle_time);
		g_free(idle_time);

		jabber_iq_send(iq);
	}
}

static void jabber_iq_time_parse(JabberStream *js, const char *from,
                                 JabberIqType type, const char *id,
                                 xmlnode *child)
{
	const char *xmlns;
	JabberIq *iq;
	time_t now_t;
	struct tm now_local;
	struct tm now_utc;
	struct tm *now;

	time(&now_t);
	now = localtime(&now_t);
	memcpy(&now_local, now, sizeof(struct tm));
	now = gmtime(&now_t);
	memcpy(&now_utc, now, sizeof(struct tm));

	xmlns = xmlnode_get_namespace(child);

	if(type == JABBER_IQ_GET) {
		xmlnode *utc;
		const char *date, *tz, *display;

		iq = jabber_iq_new(js, JABBER_IQ_RESULT);
		jabber_iq_set_id(iq, id);
		if (from)
			xmlnode_set_attrib(iq->node, "to", from);

		child = xmlnode_new_child(iq->node, child->name);
		xmlnode_set_namespace(child, xmlns);
		utc = xmlnode_new_child(child, "utc");

		if(!strcmp("urn:xmpp:time", xmlns)) {
			tz = purple_get_tzoff_str(&now_local, TRUE);
			xmlnode_insert_data(xmlnode_new_child(child, "tzo"), tz, -1);

			date = purple_utf8_strftime("%FT%TZ", &now_utc);
			xmlnode_insert_data(utc, date, -1);
		} else { /* jabber:iq:time */
			tz = purple_utf8_strftime("%Z", &now_local);
			xmlnode_insert_data(xmlnode_new_child(child, "tz"), tz, -1);

			date = purple_utf8_strftime("%Y%m%dT%T", &now_utc);
			xmlnode_insert_data(utc, date, -1);

			display = purple_utf8_strftime("%d %b %Y %T", &now_local);
			xmlnode_insert_data(xmlnode_new_child(child, "display"), display, -1);
		}

		jabber_iq_send(iq);
	}
}

static void jabber_iq_version_parse(JabberStream *js, const char *from,
                                    JabberIqType type, const char *id,
                                    xmlnode *packet)
{
	JabberIq *iq;
	xmlnode *query;

	if(type == JABBER_IQ_GET) {
		GHashTable *ui_info;
		const char *ui_name = NULL, *ui_version = NULL;
#if 0
		char *os = NULL;
		if(!purple_prefs_get_bool("/plugins/prpl/jabber/hide_os")) {
			struct utsname osinfo;

			uname(&osinfo);
			os = g_strdup_printf("%s %s %s", osinfo.sysname, osinfo.release,
					osinfo.machine);
		}
#endif

		iq = jabber_iq_new_query(js, JABBER_IQ_RESULT, "jabber:iq:version");
		if (from)
			xmlnode_set_attrib(iq->node, "to", from);
		jabber_iq_set_id(iq, id);

		query = xmlnode_get_child(iq->node, "query");

		ui_info = purple_core_get_ui_info();

		if(NULL != ui_info) {
			ui_name = g_hash_table_lookup(ui_info, "name");
			ui_version = g_hash_table_lookup(ui_info, "version");
		}

		if(NULL != ui_name && NULL != ui_version) {
			char *version_complete = g_strdup_printf("%s (libpurple " VERSION ")", ui_version);
			xmlnode_insert_data(xmlnode_new_child(query, "name"), ui_name, -1);
			xmlnode_insert_data(xmlnode_new_child(query, "version"), version_complete, -1);
			g_free(version_complete);
		} else {
			xmlnode_insert_data(xmlnode_new_child(query, "name"), "libpurple", -1);
			xmlnode_insert_data(xmlnode_new_child(query, "version"), VERSION, -1);
		}

#if 0
		if(os) {
			xmlnode_insert_data(xmlnode_new_child(query, "os"), os, -1);
			g_free(os);
		}
#endif

		jabber_iq_send(iq);
	}
}

void jabber_iq_remove_callback_by_id(JabberStream *js, const char *id)
{
	g_hash_table_remove(js->iq_callbacks, id);
}

void jabber_iq_parse(JabberStream *js, xmlnode *packet)
{
	JabberCallbackData *jcd;
	xmlnode *child, *error, *x;
	const char *xmlns;
	const char *iq_type, *id, *from;
	JabberIqType type = JABBER_IQ_NONE;

	/*
	 * child will be either the first tag child or NULL if there is no child.
	 * Historically, we used just the 'query' subchild, but newer XEPs use
	 * differently named children. Grabbing the first child is (for the time
	 * being) sufficient.
	 */
	for (child = packet->child; child; child = child->next) {
		if (child->type == XMLNODE_TYPE_TAG)
			break;
	}

	iq_type = xmlnode_get_attrib(packet, "type");
	from = xmlnode_get_attrib(packet, "from");
	id = xmlnode_get_attrib(packet, "id");

	if (iq_type) {
		if (!strcmp(iq_type, "get"))
			type = JABBER_IQ_GET;
		else if (!strcmp(iq_type, "set"))
			type = JABBER_IQ_SET;
		else if (!strcmp(iq_type, "result"))
			type = JABBER_IQ_RESULT;
		else if (!strcmp(iq_type, "error"))
			type = JABBER_IQ_ERROR;
	}

	if (type == JABBER_IQ_NONE) {
		purple_debug_error("jabber", "IQ with invalid type ('%s') - ignoring.\n",
						   iq_type ? iq_type : "(null)");
		return;
	}

	/* All IQs must have an ID, so send an error for a set/get that doesn't */
	if(!id || !*id) {

		if(type == JABBER_IQ_SET || type == JABBER_IQ_GET) {
			JabberIq *iq = jabber_iq_new(js, JABBER_IQ_ERROR);

			xmlnode_free(iq->node);
			iq->node = xmlnode_copy(packet);
			if (from) {
				xmlnode_set_attrib(iq->node, "to", from);
				xmlnode_remove_attrib(iq->node, "from");
			}

			xmlnode_set_attrib(iq->node, "type", "error");
			/* This id is clearly not useful, but we must put something there for a valid stanza */
			iq->id = jabber_get_next_id(js);
			xmlnode_set_attrib(iq->node, "id", iq->id);
			error = xmlnode_new_child(iq->node, "error");
			xmlnode_set_attrib(error, "type", "modify");
			x = xmlnode_new_child(error, "bad-request");
			xmlnode_set_namespace(x, "urn:ietf:params:xml:ns:xmpp-stanzas");

			jabber_iq_send(iq);
		} else
			purple_debug_error("jabber", "IQ of type '%s' missing id - ignoring.\n",
			                   iq_type);

		return;
	}

	/* First, lets see if a special callback got registered */
	if(type == JABBER_IQ_RESULT || type == JABBER_IQ_ERROR) {
		if((jcd = g_hash_table_lookup(js->iq_callbacks, id))) {
			jcd->callback(js, from, type, id, packet, jcd->data);
			jabber_iq_remove_callback_by_id(js, id);
			return;
		}
	}

	/* Apparently not, so lets see if we have a pre-defined handler */
	if(child && (xmlns = xmlnode_get_namespace(child))) {
		char *key = g_strdup_printf("%s %s", child->name, xmlns);
		JabberIqHandler *jih = g_hash_table_lookup(iq_handlers, key);
		g_free(key);

		if(jih) {
			jih(js, from, type, id, child);
			return;
		}
	}

	purple_debug_info("jabber", "jabber_iq_parse\n");

	/* If we get here, send the default error reply mandated by XMPP-CORE */
	if(type == JABBER_IQ_SET || type == JABBER_IQ_GET) {
		JabberIq *iq = jabber_iq_new(js, JABBER_IQ_ERROR);

		xmlnode_free(iq->node);
		iq->node = xmlnode_copy(packet);
		if (from) {
			xmlnode_set_attrib(iq->node, "to", from);
			xmlnode_remove_attrib(iq->node, "from");
		}

		xmlnode_set_attrib(iq->node, "type", "error");
		error = xmlnode_new_child(iq->node, "error");
		xmlnode_set_attrib(error, "type", "cancel");
		xmlnode_set_attrib(error, "code", "501");
		x = xmlnode_new_child(error, "feature-not-implemented");
		xmlnode_set_namespace(x, "urn:ietf:params:xml:ns:xmpp-stanzas");

		jabber_iq_send(iq);
	}
}

void jabber_iq_register_handler(const char *node, const char *xmlns, JabberIqHandler *handlerfunc)
{
	/*
	 * This is valid because nodes nor namespaces cannot have spaces in them
	 * (see http://www.w3.org/TR/2006/REC-xml-20060816/ and
	 * http://www.w3.org/TR/REC-xml-names/)
	 */
	char *key = g_strdup_printf("%s %s", node, xmlns);
	g_hash_table_replace(iq_handlers, key, handlerfunc);
}

void jabber_iq_init(void)
{
	iq_handlers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	jabber_iq_register_handler("mailbox", "google:mail:notify", jabber_gmail_poke);
	jabber_iq_register_handler("new-mail", "google:mail:notify", jabber_gmail_poke);
	jabber_iq_register_handler("query", "http://jabber.org/protocol/bytestreams", jabber_bytestreams_parse);
	jabber_iq_register_handler("query", "http://jabber.org/protocol/disco#info", jabber_disco_info_parse);
	jabber_iq_register_handler("query", "http://jabber.org/protocol/disco#items", jabber_disco_items_parse);
	jabber_iq_register_handler("si", "http://jabber.org/protocol/si", jabber_si_parse);
	jabber_iq_register_handler("query", "jabber:iq:last", jabber_iq_last_parse);
	jabber_iq_register_handler("query", "jabber:iq:oob", jabber_oob_parse);
	jabber_iq_register_handler("query", "jabber:iq:register", jabber_register_parse);
	jabber_iq_register_handler("query", "jabber:iq:roster", jabber_roster_parse);
	jabber_iq_register_handler("query", "jabber:iq:time", jabber_iq_time_parse);
	jabber_iq_register_handler("query", "jabber:iq:version", jabber_iq_version_parse);
	jabber_iq_register_handler("data", XEP_0231_NAMESPACE, jabber_data_parse);
	jabber_iq_register_handler("ping", "urn:xmpp:ping", jabber_ping_parse);
	jabber_iq_register_handler("time", "urn:xmpp:time", jabber_iq_time_parse);

	jabber_iq_register_handler("jingle", JINGLE, jingle_parse);
	jabber_iq_register_handler("query", GOOGLE_JINGLE_INFO_NAMESPACE,
		jabber_google_handle_jingle_info);
}

void jabber_iq_uninit(void)
{
	g_hash_table_destroy(iq_handlers);
	iq_handlers = NULL;
}
