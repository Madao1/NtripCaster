/* http.c
 * - Everything HTTP related
 *
 * Copyright (c) 2003
 * German Federal Agency for Cartography and Geodesy (BKG)
 *
 * Developed for Networked Transport of RTCM via Internet Protocol (NTRIP)
 * for streaming GNSS data over the Internet.
 *
 * Designed by Informatik Centrum Dortmund http://www.icd.de
 *
 * NTRIP is currently an experimental technology.
 * The BKG disclaims any liability nor responsibility to any person or entity
 * with respect to any loss or damage caused, or alleged to be caused,
 * directly or indirectly by the use and application of the NTRIP technology.
 *
 * For latest information and updates, access:
 * http://igs.ifag.de/index_ntrip.htm
 *
 * Georg Weber
 * BKG, Frankfurt, Germany, June 2003-06-13
 * E-mail: euref-ip@bkg.bund.de
 *
 * Based on the GNU General Public License published Icecast 1.3.12
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#ifdef _WIN32
#include <win32config.h>
#else
#include <config.h>
#endif
#endif

#include "definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef __USE_BSD
#define __USE_BSD
#endif
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>

#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define read _read
#else
#include <dirent.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#endif

#include <sys/stat.h>

#include "avl.h"
#include "avl_functions.h"
#include "threads.h"
#include "ntripcastertypes.h"
#include "ntripcaster.h"
#include "admin.h"
#include "utility.h"
#include "ntripcaster_string.h"
#include "log.h"
#include "commands.h"
#include "sock.h"
#include "ntripcaster_resolv.h"
#include "connection.h"
#include "logtime.h"
#include "restrict.h"
#include "memory.h"
#include "source.h"
#include "http.h"
#include "sourcetable.h"
#include "match.h"
#include "vars.h"
#include "authenticate/basic.h"
#include "http.h"

#include "authenticate/user.h"
#include "authenticate/group.h"
#include "authenticate/mount.h"

extern server_info_t info;
extern comp_element commands[];

http_command_t http_commands[] = 
{
	/* command, function to call, whether to wrap, extra argument */
	{ "alias",     com_alias,       0, NULL },
	{ "allow",     com_allow,       0, NULL },
	{ "deny",      com_deny,        0, NULL },
	{ "acl",       com_acl, 	0, NULL },
	{ "admins",    com_admins,      0, NULL },
//	{ "dir",       com_dir,         0, NULL },
//	{ "dump",      com_dump,        0, NULL },
//	{ "help",      http_help,        1, NULL }, // not needed. ajd
	{ "kick",      com_kick,        0, NULL },
	{ "listeners", com_listeners,   0, NULL },
	{ "modify",    com_modify,      0, NULL },
//	{ "pause",     com_pause,       0, NULL },
	{ "rehash",    com_rehash,      0, NULL },
//	{ "select",    com_sel,         0, NULL },
	{ "sources",   com_sources,     0, NULL },
  { "sourcetable", com_sourcetable, 0, NULL }, // Added. ajd
	{ "resync",  com_resync,    0, NULL },
	{ "set",       com_set,         0, NULL },
	{ "stats",     com_stats,       0, NULL },
	{ "tail",      com_tail,        0, NULL }, 
	{ "tell",      com_tell,        0, NULL },
//	{ "touch",     com_touch,       0, NULL }, // not needed. ajd
//	{ "unpause",   com_unpause,     0, NULL },
	{ "untail",    com_untail,      0, NULL },
	{ "uptime",    com_uptime,      0, NULL },
	{ "list",      com_list,        0, NULL },
	{ "relay",     com_relay,       0, NULL },
	{ "threads",   com_threads,     0, NULL },
	{ "locks",     com_locks,       0, NULL },   
	{ "mem",       com_mem,         0, NULL },
//	{ "streamtitle", com_streamtitle, 0, NULL },
//	{ "streamurl", com_streamurl,   0, NULL },
	{ "describe",  com_describe,    0, NULL },
	{ "auth",      com_auth,        0, NULL },
	{ "server_info", com_runtime,   0, NULL },
	{ "display",   http_display,    0, NULL },
	{ "change",    http_change,     0, NULL },
	{ (char *) NULL, (ntripcaster_int_function *) NULL, 0, (char *) NULL },
};


http_variable_t http_variables[] = 
{
  { "PORT", integer_e, &info.port[0] },
  { "SOURCES", integer_e, &info.num_sources },
  { "LISTENERS", integer_e, &info.num_clients },
  { "MAXSOURCES", integer_e, &info.max_sources },
  { "MAXLISTENERS", integer_e, &info.max_clients },
  { "MAXLISTENERSPERSOURCE", integer_e, &info.max_clients_per_source},
  { "ADMINS", integer_e, &info.num_admins },
  { "MAXADMINS", integer_e, &info.max_admins },
//  { "STATICDIR", string_e, &info.staticdir },
  { "CONFIGFILE", string_e, &info.configfile },
  { "LOGFILE", string_e, &info.logfile },
//  { "STATSFILE", string_e, &info.statsfile },
  { "SERVERNAME", string_e, &info.server_name },
  { "VERSION", string_e, &info.version }, // added. ajd
  { "NTRIPPROTOCOLVERSION", string_e, &info.ntripversion }, // added. ajd
  { "NTRIPINFOURL", string_e, &info.ntripinfourl }, // added. ajd
  { "NAME", string_e, &info.name }, // added. ajd  
  { "OPERATOR", string_e, &info.operator }, // added. ajd  
  { "OPERATORURL", string_e, &info.operatorurl }, // added. ajd   
  { "THROTTLE", real_e, &info.throttle},
  { "BANDWIDTH", real_e, &info.bandwidth_usage},
  { "LOCATION", string_e, &info.location },
  { "RPEMAIL", string_e, &info.rp_email },
  { "URL", string_e, &info.url },
  { "HOSTNAME", string_e, &info.myhostname },
  { "CLIENT_TIMEOUT", integer_e, &info.client_timeout },
  { "UPTIME", function_e, (void *)ntripcaster_uptime },
  { "STARTTIME", function_e, (void *)ntripcaster_starttime },
  { (char *) NULL, 0, NULL }
};

http_link_t http_links[] =
{
	{ "/admin", "<b>home</b>", " | " },
	{ "/admin?mode=stats", "statistics", " | " },
	{ "/admin?mode=sourcetable", "sourcetable", " | " },
	{ "/admin?mode=listeners", "listeners", " | " },
	{ "/admin?mode=sources", "sources", " | " },
	{ "/admin?mode=admins", "admins", " || " },
//	{ "/admin?mode=auth", "authentication", " | " },
	{ "/admin?mode=set", "settings", "" },
  { (char *) NULL, (char *) NULL }
};

/* Functions to call for the server-parsed language */
const char * http_foreach ();
const char * http_include ();
htf http_even (), http_odd();
void wrap_http_function (const http_command_t *http_command, com_request_t *req, const comp_element *command);


http_parsable_t http_parsables[] = 
{
  { "FOREACH", http_foreach },
  { "INCLUDE", http_include },
  { "EVEN", http_even },
  { "ODD", http_odd },
  { (char *) NULL, (HttpFunction *) NULL}
};

void
write_http_code_page (connection_t *con, int code, const char *msg)
{
	char template_file[BUFSIZE];
	char filename[BUFSIZE];
	
	snprintf (template_file, BUFSIZE, "%d.html", code);

	write_http_header (con->sock, code, msg);
	sock_write_line (con->sock, "Connection: close");
	sock_write_line (con->sock, "Content-Type: text/html\r\n");

	if (get_ntripcaster_file (template_file, template_file_e, R_OK, filename) != NULL)
	{
		write_template_parsed_html_page (con, NULL, filename, -2, NULL);
//		nfree (filename);
		return;
	}
	
	sock_write_line (con->sock, "<html><head><title>%d %s</title></head>%s", code, msg, DEFAULT_BODY_TAG);
	sock_write_line (con->sock, "%d %s", code, msg);
	sock_write_line (con->sock, "</body></html>");
}

void
write_401 (connection_t *con, char *realm)
{
	char filename[BUFSIZE];
	write_http_header (con->sock, 401, "Unauthorized");
	sock_write_line (con->sock, "WWW-Authenticate: Basic realm=\"%s\"", realm);
	sock_write_line (con->sock, "Content-Type: text/html");
	sock_write_line (con->sock, "Connection: close\r\n");
	
	if (get_ntripcaster_file ("401.html", template_file_e, R_OK, filename) != NULL)
	{
		write_template_parsed_html_page (con, NULL, filename, -2, NULL);
//		nfree (filename);
		return;
	}
	
	sock_write_line (con->sock, "<html><head><title>%d %s</title></head>%s", 401, "Unauthorized", DEFAULT_BODY_TAG);
	sock_write_line (con->sock, "<h1><center>The server does not recognize your privileges to the requested entity/stream</center></h1>\r\n");
	sock_write_line (con->sock, "</body></html>");
}


http_parsable_t *
find_http_element (char *name, http_parsable_t *el)
{
	register int i;
	for (i = 0; el[i].name; i++)
	  {
	    if (ntripcaster_strncmp (name, el[i].name, ntripcaster_strlen (el[i].name)) == 0)
	      return (&el[i]);
	  }
	return ((http_parsable_t *)NULL);
}

const http_variable_t *
find_http_variable (const char *name, const http_variable_t *el)
{
	register int i;
	for (i = 0; el[i].name; i++)
	  {
	    if (ntripcaster_strcmp (name, el[i].name) == 0)
	      return (&el[i]);
	  }
	return ((http_variable_t *)NULL);
}

const http_command_t *
find_http_command (const char *name, const http_command_t *el)
{
  register int i;
  for (i = 0; el[i].name; i++)
    if (ntripcaster_strcmp (name, el[i].name) == 0)
      return (&el[i]);
  return ((http_command_t *)NULL);
}


#define HEX_ESCAPE '%'
#define ACCEPTABLE(a)  (a >= 32 && a < 128 && ((isAcceptable[a - 32]) & mask))

unsigned char isAcceptable[96] =
{
  0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xF,0xE,0x0,0xF,0xF,0xC, 
  0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0x8,0x0,0x0,0x0,0x0,0x0, 
  0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF, 
  0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0x0,0x0,0x0,0x0,0xF, 
  0x0,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF, 
  0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0x0,0x0,0x0,0x0,0x0  
};
char *hex = "0123456789ABCDEF";
int mask = 0x8; 

char *
url_decode (const char *intarget)
{
    char *temp, *target;
    char c1, c2;
    int len, run, j;

    if (!intarget)
    {
	    xa_debug (1, "WARNING: url_decode called with NULL pointer");
	    return NULL;
    }

    target = nstrdup (intarget);

    len = ntripcaster_strlen (target);
    temp = (char *) nmalloc (len + 1);
    strncpy(temp, target, len+1);

    for (j = 0, run = 0; j < len; j++, run++) 
    {
	    switch (temp[j]) 
	    {
		    case '+':
			    target[run] = ' ';
			    break;
		    case '%':
			    c1 = tolower (temp[++j]);
			    if (isdigit ((int)c1)) 
			    {
				    c1 -= '0';
			    } else {
				    c1 = c1 - 'a' + 10;
			    }
			    c2 = tolower (temp[++j]);
			    if (isdigit ((int)c2)) 
			    {
				    c2 -= '0';
			    } else {
				    c2 = c2 - 'a' + 10;
			    }
			    target[run] = c1 * 16 + c2;
			    break;
		    default:
			    target[run] = temp[j];
			    break;
	    }
    }
    target[run] = '\0';
    
    nfree (temp);
    return (target);
}

char * 
url_encode (const char *str, char **result_p)
{
  const char *p;
  char *q;
  char *result;
  int unacceptable = 0;

  if (!str) 
  {
	  xa_debug (1, "WARNING: url_encode() called with NULL string");
	  return NULL;
  }

  for (p = str; *p; p++)
    if (!ACCEPTABLE((unsigned char) (*p)))
      unacceptable++;

  result = (char  *) nmalloc (p - str + unacceptable + unacceptable + 1);

  *result_p = result;

  for (q = result, p = str; *p; p++) 
    {
      unsigned char a = *p;
      if (!ACCEPTABLE(a)) 
	{
	  *q++ = HEX_ESCAPE;  /* Means hex commming */
	  *q++ = hex[a >> 4];
	  *q++ = hex[a & 15];
	}
      else *q++ = *p;
    }
  *q++ = 0;                   /* Terminate */
  return result;
}

/*
int
updinfo (connection_t *con, vartree_t *request_vars)
{
//	const char *song = get_variable (request_vars, "song");
//	const char *url = get_variable (request_vars, "url");
	const char *mount = get_variable (request_vars, "mount");
//	const char *msg = get_variable (request_vars, "msg");
//	const char *length = get_variable (request_vars, "length");
	char *ptr;
	connection_t *sourcecon;

	if (!con || !request_vars)
	{
		xa_debug (2, "WARNING: updinfo called with NULL pointers");
		return 0;
	}

	if (!mount)
		sourcecon = get_source_from_host (con);
	else
		sourcecon = get_source_with_mount (mount);

	if (!sourcecon)
	{
		sock_write_line (con->sock, "Invalid request");
		return 0;
	}

	sourcecon->food.source->info.udpseqnr++;

	if (length)
	{
		ptr = url_decode (length);
		sourcecon->food.source->info.streamlength = atol (ptr);
		nfree (ptr);
		xa_debug (2, "DEBUG: updinfo() changed song length to %ld bytes", sourcecon->food.source->info.streamlength);
	}

	if (msg)
	{
		ptr = sourcecon->food.source->info.streammsg;
		sourcecon->food.source->info.streammsg = url_decode (msg);
		nfree (ptr);
		xa_debug (2, "DEBUG: updinfo() changed msg info to %s", sourcecon->food.source->info.streammsg);
	}

	if (song)
	{
		char *ptr2 = url_decode (song);

		ptr = sourcecon->food.source->info.streamtitle;
		sourcecon->food.source->info.streamtitle = ntripcaster_sprintf (info.streamtitletemplate, ptr2);
		nfree (ptr);

		ptr = info.streamtitle;
		info.streamtitle = ntripcaster_sprintf (info.streamtitletemplate, ptr2);
		nfree (ptr);

		nfree (ptr2);
//		info.directorylasttime = 0; // force an update
		// change_thread_id_on_mem (sourcecon->food.source->streamtitle, sourcecon->food.source->thread_id);

		xa_debug (2, "DEBUG: updinfo() changed song info to %s", sourcecon->food.source->info.streamtitle);
	}

	if (url)
	{
		if (info.streamurllock)
		{
			sock_write_line (con->sock, "Error: StreamUrl is locked by admin and cannot be changed");
			return 0;
		}
		ptr = sourcecon->food.source->info.streamurl;
		sourcecon->food.source->info.streamurl = url_decode (url);
		// change_thread_id_on_mem (sourcecon->food.source->url, sourcecon->food.source->thread_id);

		nfree (ptr);
//		xa_debug (2, "DEBUG: updinfo() changed url info to %s", url);
	}

	update_metadata_on_relays (sourcecon, mount, song, msg, length, url);
	return 1;
}
*/

/*
void
update_metadata_on_relays (connection_t *con, const char *mount, const char *song, const char *msg, const char *length, const char *url)
{
	connection_t *relaycon;
	avl_traverser trav = {0};

	thread_mutex_lock (&con->food.source->mutex);
	
	while ((relaycon = avl_traverse (con->food.source->clients, &trav)))
	{
		if ((relaycon->food.client->type == pusher_e) || (relaycon->food.client->type == pulling_client_e))
			update_metadata_on_relay (relaycon, mount, song, msg, length, url);
	}

	thread_mutex_unlock (&con->food.source->mutex);
}

void
update_metadata_on_relay (connection_t *con, const char *mount, const char *song, const char *msg, const char *length, const char *url)
{
	const char *ports = get_con_variable (con, "x-audiocast-port");
	const char *password = get_con_variable (con, "x-audiocast-source-password");
	const char *smount = get_con_variable (con, "x-audiocast-mount");
	int port;
	SOCKET sockfd;

	if (!password)
	{
		xa_debug (2, "DEBUG: Cannot update metainfo on %s, cause I don't know the password", con_host (con));
		return;
	}

	if (ports)
		port = atoi (ports);
	else
		port = info.port[0];

	if (!smount)
		smount = mount;

	if ((sockfd = sock_connect_wto (con->host, port, 15)) != -1)
	{
		sock_write (sockfd, "GET /admin.cgi?pass=%s&mode=updinfo&song=%s&mount=%s&length=%ld HTTP/1.0\r\nHost:%s:%d\rUser-Agent: NTRIP Caster/%s\r\n\r\n",
			    password, song, smount, length, con->host, port, info.version);
		sock_close (sockfd);
	}
}
*/
/*
void
handle_http_admin_command (connection_t *con, vartree_t *request_vars)
{
	const char *inmode = get_variable (request_vars, "mode");

	xa_debug (2, "DEBUG: handle_http_admin_command() doing command [%s]", inmode ? inmode : "null");

	if (inmode && ntripcaster_strcmp (inmode, "updinfo") == 0)
		updinfo (con, request_vars);
	else
	{
		sock_write_line (con->sock, "Unknown command %s", inmode ? inmode : "null");
	}
}
*/

int
http_admin_command (connection_t *con, ntrip_request_t *req)
{
	vartree_t *request_vars = avl_create (compare_vars, &info);
	
//	const char *inpass, *inmode;

	if (!req || !req->path)
	{
		xa_debug (1, "WARNING: http_admin_command called with NULL request");
		return 0;
	}
	
/*	if (strstr (req->path, "updinfo") == NULL)
	{
*/
#ifdef HAVE_LIBWRAP
		if (!sock_check_libwrap (con->sock, admin_e)) {
			write_http_code_page (con, 403, "Forbidden");
			kick_not_connected(con, "Access denied (libwrap (admin connection))");
			free_variables (request_vars);  // added. ajd
			thread_exit(0);
		}
#endif
		if (!allowed(con, admin_e) || !info.allow_http_admin) {
			write_http_code_page (con, 403, "Forbidden");
			kick_not_connected(con, "Access denied (internal acl list (admin connection))");
			free_variables (request_vars);  // added. ajd
			thread_exit(0);
		}
			
		if ((ntripcaster_strncmp (req->path, "admin", 5) == 0) || (ntripcaster_strncmp (req->path, "/admin", 6) == 0) 
		    || (ntripcaster_strncmp (req->path, "admin/", 6) == 0))
		{
			thread_rename ("HTTP Admin Thread");
			
			put_http_admin (con);
			
			display_admin_page (con, req);

			free_variables (request_vars);  // added. ajd
			
			return 1;
		}

//	}
/*
	extract_vars (request_vars, req->path);

	inpass = get_variable (request_vars, "pass");
	inmode = get_variable (request_vars, "mode");

	xa_debug (3, "DEBUG: http_admin_command() adding variables from [%s]", req->path);
	
	xa_debug (3, "DEBUG: http_admin_command() checking password [%s] vs [%s]", info.remote_admin_pass, inpass ? inpass : "null");
	
	if (inpass && password_match (info.remote_admin_pass, inpass))
	{
		handle_http_admin_command (con, request_vars);
		free_variables (request_vars);
		return 1;
	} else if (inpass && inmode && (ntripcaster_strcmp (inmode, "updinfo") == 0) && (password_match (info.encoder_pass, inpass)))
		
	{
		handle_http_admin_command (con, request_vars);
		free_variables (request_vars);
		return 1;
	} else {
		//If a relay source is updating its metadata it will be
		//using the crypted encoder_password 
		const char* inmount;
		connection_t* source;

		if (inpass &&
		    (inmount = get_variable (request_vars, "mount")) &&
		    (source = get_source_with_mount (inmount)) &&
		    (source->type != encoder_e) && 
		    (ntripcaster_strcmp (inmode, "updinfo") == 0) &&
		    (ntripcaster_strcmp (info.encoder_pass, inpass) == 0))
		{
		  handle_http_admin_command (con, request_vars);
		  free_variables (request_vars);
		  return 1;
		}
		
		free_variables (request_vars);
		return 0;
	}
	*/
	return 0;
}

void
display_admin_page (connection_t *con, ntrip_request_t *req)
{
	vartree_t *request_vars = avl_create (compare_vars, &info);
	const char *basic_command;
	const char *commandptr;
	char commandstr[BUFSIZE];
	ntrip_request_t checkreq;
	const comp_element *com;
	const http_command_t *http_command;
	com_request_t comreq;
	int argcount = 1;
	char argstring[BUFSIZE], buff[BUFSIZE];

	extract_vars (request_vars, req->path);
	
	comreq.con = con;
	comreq.wid = -1;

	//snprintf(comreq.url, BUFSIZE, "http://%s:%d", req->host, req->port); // added to show links on generated http pages. ajd

	commandptr = get_variable (request_vars, "argument");

	if (commandptr)
	{
		strncpy(commandstr, commandptr, BUFSIZE);
		comreq.arg = commandstr;

		//decode_url_string(comreq.arg); // added. ajd

		do
		{
			snprintf(argstring, BUFSIZE, "argument%d", argcount);
			commandptr = get_variable (request_vars, argstring);
			if (commandptr)
			{
				snprintf(buff, BUFSIZE, "%s %s", comreq.arg, commandptr);
				strncpy(comreq.arg, buff, BUFSIZE);
			}
			argcount++;
		} while (argcount < 10);
	} else
		comreq.arg = NULL;

	basic_command = get_variable (request_vars, "mode");

	zero_request (&checkreq); // von weiter unten eingesetzt. ajd

	if (!basic_command)
	{
		strncpy (checkreq.path, "/admin", BUFSIZE); // von weiter unten kopiert. ajd

    if (info.allow_http_admin == 1 && authenticate_user_request (con, &checkreq, client_e)) // Abfrage hinzugefgt. ajd
			display_generic_admin_page (con);
		else
			write_401 (con, checkreq.path);

		free_variables (request_vars);
		return;
	}

	/* Find information about the http command */
	http_command = find_http_command (basic_command, http_commands);
	if (!http_command)
	{
		write_no_such_command (con, basic_command);
		free_variables (request_vars);
		return;
	}

	/* Find information about the command */
	com = find_comp_element (basic_command, commands);

	if (com && com->oper)
		strncpy(checkreq.path, "/oper", BUFSIZE);
	else
		strncpy(checkreq.path, "/admin", BUFSIZE);

	if (info.allow_http_admin == 0 || (need_authentication (&checkreq, get_client_mounttree()) != NULL)) {
		if (info.allow_http_admin == 0 || !authenticate_user_request (con, &checkreq, client_e))
		{
			write_401 (con, checkreq.path);
			free_variables (request_vars);
			return;
		}
	}

/* For web based authentication administration. Building strings of users, groups,
	mounts,.... out of variables. ajd */

//	typecast_arguments(request_vars, &comreq); // typearg[] targets must be nfreed below!!  ajd

/*********************************************************************************/

	log_command (basic_command, &comreq);

	if (!http_command->wrap) {
		char path[BUFSIZE];
		
		if (get_ntripcaster_file ("header.html", template_file_e, R_OK, path) != NULL) {
			write_template_parsed_html_page (comreq.con, NULL, path, -1, NULL);
//			nfree (header);
		}

		http_write_links(&comreq);
		
		((*(http_command->func)) (&comreq));
		
		if (get_ntripcaster_file ("footer.html", template_file_e, R_OK, path) != NULL) {
			write_template_parsed_html_page (comreq.con, NULL, path, -2, NULL);
//			nfree (footer);
		}
	}
	else
		((*(http_command->func)) (&comreq));

	free_variables (request_vars);

//	nfree_typecasted_arguments(&comreq); // added. IMPORTANT!. ajd

}

void
http_display_home_page (connection_t *con)
{
	char file[BUFSIZE];

	if (get_ntripcaster_file ("home.html", template_file_e, R_OK, file) != NULL)
	{
		write_template_parsed_html_page (con, NULL, file, -1, NULL);
//		nfree (file);

	} else {
		write_http_header (con->sock, 200, "OK");
		sock_write_line (con->sock, "Connection: close");
		sock_write_line (con->sock, "Content-Type: text/html\r\n");
		sock_write_line (con->sock, "<html>No home page template found.<br></html>");
	}

}

void
display_generic_admin_page (connection_t *con)
{	
	char file[BUFSIZE];
	
	if (get_ntripcaster_file ("admin.html", template_file_e, R_OK, file) != NULL)
	{
		write_template_parsed_html_page (con, NULL, file, -1, NULL);
//		nfree (file);

	} else {
		write_http_header (con->sock, 200, "OK");
		sock_write_line (con->sock, "Connection: close");
		sock_write_line (con->sock, "Content-Type: text/html\r\n");
		sock_write_line (con->sock, "<html>No admin page template found.<br></html>");
	}
}

void 
write_http_header(sock_t sockfd, int error, const char *msg)
{
	sock_write_line (sockfd, "HTTP/1.1 %i %s", error, msg);
	sock_write_line (sockfd, "Ntrip-Version: Ntrip/%s", NTRIP_VERSION);
	sock_write_line (sockfd, "Server: NTRIP Caster/%s", VERSION); //not needed. ajd
}
	
int
print_http_variable (vartree_t *request_vars, const char *name, connection_t *clicon, int fd)
{
  const char *out = get_variable (request_vars, name);

  if (out)
  {
	  if (fd < 0)
		  sock_write (clicon->sock, "%s", out);
	  else
		  fd_write (fd, out);
	  
	  return 1;
  } else {
	  const http_variable_t *var = find_http_variable (name, http_variables);
	  if (!var)
		  return 0;
	  if (var->type == integer_e)
	  {
		  if (fd < 0)
			  sock_write (clicon->sock, "%d", *(int *)var->valueptr);
		  else
			  fd_write (fd, "%d", *(int *)var->valueptr);
	  }
	  else if (var->type == real_e)
	  {
		  if (fd < 0)
			  sock_write (clicon->sock, "%d", (int)*(double *)var->valueptr); // added cast to integer. ajd
		  else
			  fd_write (fd, "%f", *(double *)var->valueptr);
	  }
	  else if (var->type == function_e)
	  {
		  /* Nice.. isn't it? ;) */
		  char *ptr = (char *)((*((HttpFunction *) (var->valueptr))) ());
		  if (fd < 0)
			  sock_write (clicon->sock, "%s", ptr);
		  else
			  fd_write (fd, "%s", ptr);
		  nfree (ptr);
	  }
	  else 
	  {
		  char *ptr;
		  if (!var->valueptr)
			  ptr = "(null)";
		  else
			  ptr = *(char **)var->valueptr;

		  if (fd < 0)
			  sock_write (clicon->sock, "%s", ptr);
		  else
			  fd_write (fd, "%s", ptr);
	  }
	  return 1;
  }
}
      
int
write_template_parsed_html_page (connection_t *clicon, connection_t *sourcecon, const char *template_file, int fd,
				 vartree_t *variables)
{
  int len, ffd = open_for_reading (template_file);
  struct stat st;
  char *fileptr = NULL;
  const char *res;
  int count = 0, readlen = 0;
   
  xa_debug (2, "DEBUG: wtphp(): Entering function with file=%s fd=%d", template_file, fd);

  if (fd == -1 && clicon)
  {
	  write_http_header (clicon->sock, 200, "OK");
	  sock_write_line (clicon->sock, "Connection: close");
	  sock_write_line (clicon->sock, "Content-Type: text/html\r\n");
  }

  if (ffd < 0)
  {
	  xa_debug (1, "ERROR: Cannot open file [%s]", template_file);
	  if (fd < 0)
		  sock_write_line (clicon->sock, "ERROR: Cannot open file [%s]", template_file);
	  return 0;
  }
  
  if (fstat (ffd, &st) == -1)
  {
	  xa_debug (1, "ERROR: Cannot stat file [%s]", template_file);
	  if (fd < 0)
		  sock_write_line (clicon->sock, "ERROR: Cannot stat file [%s]", template_file);
	  return 0;
  }
  
  xa_debug (3, "DEBUG: wtphp(): Parsing page %s", template_file);
  
  /* Don't use nmalloc here, we want our own error handling */
  fileptr = (char *) malloc (st.st_size + 2);
  if (fileptr == NULL)
  {
	  if (fd < 0)
		  sock_write_line (clicon->sock, "ERROR: Cannot allocate enough memory, try again later");
	  return 0;
  }
  
  count = readlen = 0;
  while (count < st.st_size) {
	  len = min(2048, st.st_size - count);
	  readlen = read(ffd, &fileptr[count], len);
	  if (readlen > 0) {
		  count += readlen;
	  } else {
		  xa_debug (1, "Read error while parsing %s", template_file);

			free (fileptr);  // added. ajd

		  return 0;
	  }
  }
  
  fileptr[count] = '\0';
  
  if (!variables)
	  variables = avl_create (compare_vars, &info);
  
  xa_debug (3, "DEBUG: wtphp(): Done reading template file into memory, read %d bytes, starting parsing", count);
  
  res = parse_template_file (clicon, sourcecon, fileptr, fd, variables);

  xa_debug (3, "DEBUG: wtphp(): Done parsing, freeing variables and saying goodbye");
  
  if (fileptr != NULL)
	  free (fileptr);

  fd_close (ffd);
  free_variables (variables);
  
  return res ? 1 : 0;
}

/*
 * This is rather ugly.. but it will have to do for now
 */
const char *
parse_template_file (connection_t *clicon, connection_t *sourcecon, const char *ptr, int fd, vartree_t *variables)
{
  const char *start = ptr;
  char line[BUFSIZE];
  const char *tmpptr, *nextptr;
  
  http_parsable_t *ht;
  
  xa_debug (6, "DEBUG: parsing template file starting at [%s], fd: %d", ptr, fd);
  
  while (ptr && *ptr)
  {
	  if (ptr[0] == '@')
	  {
		  memset (line, 0, BUFSIZE);
		  if ((nextptr = strchr (ptr + 1, '@')))
		  {
			  if (nextptr - ptr < BUFSIZE)
				  memcpy (line, ptr + 1, nextptr - (ptr + 1));
			  else
			  {
				  if (fd < 0)
					  sock_write_bytes (clicon->sock, ptr, 1);
				  else
					  fd_write_bytes (fd, ptr, 1);
				  ptr++;
				  continue;
			  }
			  
			  ht = find_http_element (line, http_parsables);
			
			  if (ht)
			  {
				  xa_debug (4, "DEBUG: parse_template_file(): Calling function for [%s]", line);
				  tmpptr = ((*(ht->func)) (line, clicon, sourcecon, nextptr + 1, fd, variables));
				  
				  if (tmpptr)
				  {
				    ptr = tmpptr;
				  }
				  else
				    {
				      ptr = nextptr + 1;
				    }
			  }
			  else if (print_http_variable (variables, line, clicon, fd))
			  {
				  xa_debug (4, "DEBUG: parse_template_file(): Evaluated variable [%s]", line);
				  ptr = nextptr + 1;
			  } 
			  else if (ntripcaster_strncmp (line, "ENDFOR", 6) == 0)
			  {
				  xa_debug (4, "DEBUG: parse_tempate_file(): Returning from For-loop [%s:%s]", line, nextptr + 1);
				  return nextptr + 1;
			  } else { /* @something but we should output it asis */
				  if (fd < 0)
					  sock_write (clicon->sock, "@%s@", line);
				  else
					  fd_write (fd, "@%s@", line);
				  ptr = nextptr + 1;
			  }
		  }
		  else /* @something with no ending @ */
		  {
			  if (fd < 0)
				  sock_write_bytes (clicon->sock, ptr, 1);
			  else 
				  fd_write_bytes (fd, ptr, 1);
			  ptr++;
		  }
	  } 
	  else /* Any other character */ 
	  {
		  if (fd < 0)
			  sock_write_bytes (clicon->sock, ptr, 1);
		  else
			  fd_write_bytes (fd, ptr, 1);
		  ptr++;
	  }
  }
  return start;
}

/* @ODD ident@ */
const char *
http_odd (char *command, connection_t *clicon, connection_t *sourcecon, const char *ptr, int fd, vartree_t *variables)
{
	const void *varptr;
	char endat[BUFSIZE];
	const char *nextline;

	if (!command || !clicon || !ptr || !variables)
	{
		xa_debug (1, "ERROR: http_odd() called with NULL arguments");
		return NULL;
	}

	splitc (NULL, command, ' ');
	
	varptr = get_http_variable (variables, command);

	if (!varptr)
	{
		xa_debug (3, "ERROR: http_odd() called with invalid counter variable");
		if (fd < 0)
			sock_write_line (clicon->sock, "No such variable [%s]", command);
		else
			fd_write_line (fd, "No such variable [%s]", command);
		return NULL;
	}

	snprintf(endat, BUFSIZE, "@ENDFOR %s@", command);

	nextline = strchr (ptr, '\n');

	xa_debug (3, "DEBUG: http_odd(): Checking %d", atoi ((char *)varptr));
	
	if (nextline && atoi ((char *)varptr) % 2 != 0)
	{
		parse_template_file (clicon, sourcecon, ptr, fd, variables);
		return skip_before (ptr, endat);
	}

	if (nextline)
	  {
	    nextline = strchr (nextline + 1, '\n');
	  }
	return nextline ? nextline + 1 : NULL;
}

const char *
http_even (char *command, connection_t *clicon, connection_t *sourcecon, const char *ptr, int fd, vartree_t *variables)
{
	const void *varptr;
	char endat[BUFSIZE];
	const char *nextline;
	
	if (!command || !clicon || !ptr || !variables)
	{
		xa_debug (1, "ERROR: http_even() called with NULL arguments");
		return NULL;
	}

	splitc (NULL, command, ' ');

	varptr = get_http_variable (variables, command);

	if (!varptr)
	{
		xa_debug (3, "ERROR: http_even() called with invalid counter variable");
		if (fd < 0)
			sock_write_line (clicon->sock, "No such variable [%s]", command);
		else
			fd_write_line (fd, "No such variable [%s]", command);
		return NULL;
	}
	
	snprintf(endat, BUFSIZE, "@ENDFOR %s@", command);

	nextline = strchr (ptr, '\n');
	
	xa_debug (3, "DEBUG: http_even(): Checking %d", atoi ((char *)varptr));

	if (nextline && (atoi ((char *)varptr) % 2 == 0))
	{
		parse_template_file (clicon, sourcecon, ptr, fd, variables);
		return skip_before (ptr, endat);
	}

	if (nextline)
	  {
	    nextline = strchr (nextline + 1, '\n');
	  }
	return nextline ? nextline + 1 : NULL;
}

const char *
http_include (char *command, connection_t *clicon, connection_t *sourcecon, const char *ptr, int fd, vartree_t *variables)
{
	int ffd, len;
	struct stat st;
	char *fileptr;
	char filename[BUFSIZE];
	int count = 0, readlen = 0;
	
	splitc (NULL, command, ' ');
	
	if (get_ntripcaster_file (command, template_file_e, R_OK, filename) == NULL)
	{
		xa_debug (1, "ERROR: Cannot find template file [%s]", command);
		if (fd < 0)
			sock_write_line (clicon->sock, "ERROR: Cannot find template file %s", command);
		else
			fd_write_line (fd, "ERROR: Cannot find template file %s", command);
		return NULL;
	}

  xa_debug (5, "DEBUG: http_include(): including file [%s]", command);
  ffd = open_for_reading (filename);

  if (ffd < 0)
    {
      xa_debug (1, "ERROR: Cannot open file [%s]", filename);
      if (fd < 0)
	      sock_write_line (clicon->sock, "ERROR: Cannot open file [%s]", filename);
      else
	      fd_write_line (fd, "ERROR: Cannot open file [%s]", filename);
//      nfree (filename);
      return NULL;
    }
  
  if (fstat (ffd, &st) == -1)
    {
      xa_debug (1, "ERROR: Cannot stat file [%s]", filename);
      if (fd < 0)
	      sock_write_line (clicon->sock, "ERROR: Cannot stat file [%s]", filename);
      else
	      fd_write_line (fd, "ERROR: Cannot stat file [%s]", filename);
//      nfree (filename);
      return NULL;
    }
  
//  nfree (filename);
  
  /* Don't use nmalloc here, we want our own error handling */
  fileptr = (char *) malloc (st.st_size + 2);
  if (fileptr == NULL)
  {
	  if (fd < 0)
		  sock_write_line (clicon->sock, "ERROR: Cannot allocate enough memory, try again later");
	  else
		  fd_write_line (fd, "ERROR: Cannot allocate enough memory, try again later");
	  return NULL;
  }
  
  count = readlen = 0;
  while (count < st.st_size)
  {
	  len = min (2048, st.st_size - count);
	  readlen = read (ffd, &fileptr[count], len);
	  if (readlen >= 0)
		  count += readlen;
	  else if (!is_recoverable (errno))
	  {
		  xa_debug (1, "Read error while parsing %s", filename);

		  free (fileptr); // added. ajd

		  return NULL;
	  }
  }
  
  fileptr[count] = '\0';
  
  parse_template_file (clicon, sourcecon, fileptr, fd, variables);
  
  if (fileptr != NULL)
	  free (fileptr);

  fd_close (ffd);  
  
  return NULL;
}

/* FOREACH ident SOURCES|LISTENERS|ADMINS|FILES */
const char *
http_foreach (char *command, connection_t *clicon, connection_t *sourcecon, const char *ptr, int fd, vartree_t *variables)
{
  char ident[BUFSIZE];

  if (splitc (NULL, command, ' ') == NULL)
    return NULL;
  
  if (splitc (ident, command, ' ') == NULL)
    return NULL;
  
  xa_debug (6, "DEBUG: http_foreach: called with [%s] [%s]", command, ptr);
  
  if (ntripcaster_strcasecmp (command, "SOURCES") == 0)
	  return http_loop_sources (ident, clicon, ptr, fd, variables);
  else if (ntripcaster_strcasecmp (command, "LISTENERS") == 0)
	  return http_loop_listeners (ident, clicon, ptr, fd, variables);
  else if (ntripcaster_strcasecmp (command, "ADMINS") == 0)
	  return http_loop_admins (ident, clicon, ptr, fd, variables);
//  else if (ntripcaster_strcasecmp (command, "FILES") == 0)
//	  return http_loop_directory (ident, clicon, ptr, fd, variables);
  else {
	  xa_debug (1, "WARNING: Unknown Traverse type for FOREACH [%s]", command);
	  if (fd < 0)
		  sock_write (clicon->sock, "Unknown Traverse type [%s]", command);
	  else
		  fd_write (fd, "Unknown Traverse type [%s]", command);
	  return NULL;
  }
}

const char *
http_loop_sources (char *ident, connection_t *clicon, const char *ptr, int fd, vartree_t *variables)
{
  const char *runptr = ptr, *endptr = NULL;
  char buf[BUFSIZE];
  int i = 0;
  connection_t *travcon;

  avl_traverser trav = {0};
  
  xa_debug (3, "DEBUG: http_loop_sources(): Traversing sources");
  
  if (info.num_sources <= 0)
    {
      xa_debug (3, "DEBUG: http_loop_sources(): Empty tree");
      if (fd < 0)
	      sock_write (clicon->sock, "No sources available<br>");
      else
	      fd_write (fd, "No sources available<br>");
      snprintf(buf, BUFSIZE, "@ENDFOR %s@", ident);
      return skip_after (ptr, buf);
    }
  
  thread_mutex_lock (&info.source_mutex);
  
  while ((travcon = avl_traverse (info.sources, &trav)))
    {
      xa_debug (3, "DEBUG: http_loop_sources(): Running for source [%s]", con_host (travcon));
      runptr = ptr;
      
      add_varpair2 (variables, nstrdup (ident), ntripcaster_itoa (i));
      add_varpair2 (variables, ntripcaster_cat (ident, ".host"), nstrdup (con_host (travcon)));
      add_varpair2 (variables, ntripcaster_cat (ident, ".clients"), ntripcaster_utoa (travcon->food.source->num_clients));
//      add_varpair2 (variables, ntripcaster_cat (ident, ".dumpfile"), nstrdup (travcon->food.source->dumpfile));
//      add_varpair2 (variables, ntripcaster_cat (ident, ".streamurl"), nstrdup (travcon->food.source->info.streamurl));
//      add_varpair2 (variables, ntripcaster_cat (ident, ".streamtitle"), nstrdup (travcon->food.source->info.streamtitle));
//      add_varpair2 (variables, ntripcaster_cat (ident, ".name"), nstrdup (travcon->food.source->audiocast.name));
//      add_varpair2 (variables, ntripcaster_cat (ident, ".genre"), nstrdup (travcon->food.source->audiocast.genre));
//      add_varpair2 (variables, ntripcaster_cat (ident, ".url"), nstrdup (travcon->food.source->audiocast.url));
      add_varpair2 (variables, ntripcaster_cat (ident, ".mount"), nstrdup (travcon->food.source->audiocast.mount));
//      add_varpair2 (variables, ntripcaster_cat (ident, ".description"), nstrdup (travcon->food.source->audiocast.description));
//      add_varpair2 (variables, ntripcaster_cat (ident, ".public"), ntripcaster_itoa (travcon->food.source->audiocast.public));
//      add_varpair2 (variables, ntripcaster_cat (ident, ".bitrate"), ntripcaster_itoa (travcon->food.source->audiocast.bitrate));
      add_varpair2 (variables, ntripcaster_cat (ident, ".connecttime"), nstrdup (nntripcaster_time (get_time() - travcon->connect_time, buf)));
      endptr = parse_template_file (clicon, NULL, runptr, fd, variables);
      i++;
      
    }
  
  thread_mutex_unlock (&info.source_mutex);
  
  return endptr;
}

const char *
http_loop_admins (char *ident, connection_t *clicon, const char *ptr, int fd, vartree_t *variables)
{
  const char *runptr = ptr, *endptr = NULL;
  char buf[BUFSIZE];
  int i = 0;
  connection_t *travcon;

  avl_traverser trav = {0};
  
  xa_debug (3, "DEBUG: http_loop_admins(): Traversing admins");
  
  if (info.num_admins <= 0)
    {
      xa_debug (3, "DEBUG: http_loop_admins(): Empty tree");
      if (fd < 0)
	      sock_write (clicon->sock, "No admins available<br>");
      else
	      fd_write (fd, "No admins available<br>");
      snprintf(buf, BUFSIZE, "@ENDFOR %s@", ident);
      return skip_after (ptr, buf);
    }
  
  thread_mutex_lock (&info.admin_mutex);
  
  while ((travcon = avl_traverse (info.admins, &trav)))
  {
	  xa_debug (3, "DEBUG: http_loop_admins(): Running for admins [%s]", con_host (travcon));
	  runptr = ptr;
	  
	  add_varpair2 (variables, nstrdup (ident), ntripcaster_itoa (i));
	  add_varpair2 (variables, ntripcaster_cat (ident, ".host"), nstrdup (con_host (travcon)));
	  add_varpair2 (variables, ntripcaster_cat (ident, ".connecttime"), nstrdup (nntripcaster_time (get_time() - travcon->connect_time, buf)));
	  endptr = parse_template_file (clicon, NULL, runptr, fd, variables);
	  i++;
	  
  }
  
  thread_mutex_unlock (&info.admin_mutex);
  
  return endptr;
      
}

/*
const char *
http_loop_directory (char *ident, connection_t *clicon, const char *ptr, int fd, vartree_t *variables)
{
  const char *runptr = ptr, *endptr = NULL;
  char buf[BUFSIZE];
  char fullfilename[BUFSIZE*2];
  struct stat st;
  int i = 0;
  dir_t *od = NULL;
  const char *directoryname = get_variable (variables, "LISTED_DIRECTORY");

  if (directoryname)
	  od = dir_findfirst (directoryname);
  
  if (!directoryname || !od)
  { 
	  xa_debug (2, "DEBUG: http_loop_directory(): Could not open directory");
	  sock_write (clicon->sock, "No such directory<br>");
	  snprintf(buf, BUFSIZE, "@ENDFOR %s@", ident);
	  return skip_after (ptr, buf);	  
  }

  xa_debug (3, "DEBUG: http_loop_directory(): Traversing directory");
  
  while (dir_findnext (od))
  {
	  snprintf(fullfilename, BUFSIZE, "%s/%s", directoryname, od->filename);
	  if ((stat (fullfilename, &st) == 0) && wild_match ((unsigned char *)"*.mp3", (unsigned char *)od->filename))
	  {
		  char *escaped;

		  add_varpair2 (variables, nstrdup (ident), ntripcaster_itoa (i));
		  url_encode(od->filename, &escaped);
		  add_varpair2 (variables, ntripcaster_cat (ident, ".urlname"), escaped);
		  add_varpair2 (variables, ntripcaster_cat (ident, ".name"), nstrdup (od->filename));
		  add_varpair2 (variables, ntripcaster_cat (ident, ".size"), ntripcaster_itoa (st.st_size));
		  endptr = parse_template_file (clicon, NULL, runptr, fd, variables);
		  i++;
	  }
  }

  dir_findclose (od);
  
  return endptr;
}
*/


const char *
http_loop_listeners (char *ident, connection_t *clicon, const char *ptr, int fd, vartree_t *variables)
{
  const char *runptr = ptr, *endptr = NULL;
  char buf[BUFSIZE];
  int i = 0;
  connection_t *travclients;

  avl_traverser travc = {0};
  
  xa_debug (3, "DEBUG: http_loop_listeners(): Traversing clients");
  
  if (info.num_clients <= 0)
    {
      xa_debug (3, "DEBUG: http_loop_listeners(): Empty tree");
      if (fd < 0)
	      sock_write (clicon->sock, "No listeners available<br>");
      else
	      fd_write (fd, "No listeners available<br>");
      snprintf(buf, BUFSIZE, "@ENDFOR %s@", ident);
      return skip_after (ptr, buf);
    }
  
	thread_mutex_lock (&info.client_mutex);

	while ((travclients = avl_traverse (info.clients, &travc)))
	{
	  add_varpair2 (variables, nstrdup (ident), ntripcaster_itoa (i));
	  add_varpair2 (variables, ntripcaster_cat (ident, ".id"), ntripcaster_itoa (travclients->id));
	  add_varpair2 (variables, ntripcaster_cat (ident, ".host"), nstrdup (con_host (travclients)));
	  add_varpair2 (variables, ntripcaster_cat (ident, ".user_agent"), nstrdup (get_user_agent (travclients)));
	  add_varpair2 (variables, ntripcaster_cat (ident, ".writebytes"), ntripcaster_utoa (travclients->food.client->write_bytes));
	  add_varpair2 (variables, ntripcaster_cat (ident, ".connecttime"), nstrdup (nntripcaster_time (get_time() - travclients->connect_time, buf)));
	  endptr = parse_template_file (clicon, NULL, runptr, fd, variables);
	  i++;
	}
      
	thread_mutex_unlock (&info.client_mutex);
  
  return endptr;
}

char *
ntripcaster_uptime ()
{
  char buf[BUFSIZE];
  long filetime = read_starttime(); // added. ajd
  
  if (filetime > 0)
  	return nstrdup (nntripcaster_time (get_time () - filetime, buf));
  else
	return nstrdup (nntripcaster_time (get_time () - info.server_start_time, buf));
}

char *
ntripcaster_starttime ()
{
  char ptr[100];
  long filetime = read_starttime(); // added. ajd
  
  if (filetime > 0)
  	get_string_time (ptr, filetime, REGULAR_DATETIME);
  else
	get_string_time (ptr, info.server_start_time, REGULAR_DATETIME);

  return nstrdup(ptr);
}

const void *
get_http_variable (vartree_t *request_vars, const char *name)
{
  const char *out = get_variable (request_vars, name);

  if (out)
    return out;
  else 
    {
      const http_variable_t *var = find_http_variable (name, http_variables);
      if (!var)
	return NULL;
      return var->valueptr;
    }
}

void 
write_didnt_find_html_page (connection_t *con, char *file)
{
	write_http_header (con->sock, 200, "OK");
	sock_write_line (con->sock, "Connection: close");
	sock_write_line (con->sock, "Content-Type: text/html\r\n");
	sock_write_line (con->sock, "<html>Template file %s not found.<br></html>", file);
}

html_wrapper_t html_wrappers[] =
{
	{ ADMIN_SHOW_ADMIN_START, "<h2>Connected Admins</h2><ul>\n" },
	{ ADMIN_SHOW_ADMIN_ENTRY, "<li>%s</li>\n" },
	{ ADMIN_SHOW_ADMIN_END, "</ul>End of admin listing<br>\n" },
	{ ADMIN_SHOW_LISTENERS_START, "<h2>Connected Listeners</h2><ul>\n" },
	{ ADMIN_SHOW_LISTENERS_ENTRY, "<li>%s</li>\n" },
	{ ADMIN_SHOW_LISTENERS_END, "</ul>End of client listing<br>\n" },

/* added. ajd */
	{ ADMIN_SHOW_SOURCETABLE, "<h2>Sourcetable</h2><ul>\n" },
	{ ADMIN_SHOW_SOURCETABLE_LINE, "<li>%s</li>\n" },
	{ ADMIN_SHOW_SOURCETABLE_RED_LINE, "<li><font color=\"#FF0000\">%s</font></li>\n" },
	{ ADMIN_SHOW_SOURCETABLE_GREY_LINE, "<li><font color=\"#999999\">%s</font></li>\n" },
	{ ADMIN_SHOW_SOURCETABLE_END, "</ul>\n" },
	{ ADMIN_SHOW_SOURCETABLE_NEW_NET, "</ul><b>%s</b><ul>\n" },

	{ ADMIN_SHOW_SETTINGS_INVALID, "<h2>%s</h2>\n" },
	{ ADMIN_SHOW_SETTINGS_CHANGED_INT, "<h2>%s</h2>\n" },
	{ ADMIN_SHOW_SETTINGS_CHANGED_REAL, "<h2>%s</h2>\n" },
	{ ADMIN_SHOW_SETTINGS_CHANGED_STRING, "<h2>%s</h2>\n" },

	{ 0, NULL }
};






/* added. ajd*/
html_wrapper_t html_wrappers_form[] =
{
	{ TABLE_START, "<table border=\"0\" cellpadding=\"5\" cellspacing=\"0\">\r\n<tr><td>" },
	{ TABLE_D, "</td><td>" },
	{ TABLE_R, "</td></tr>\n<tr><td>" },
	{ TABLE_END, "</td></tr></table>\n" },

	{ FORM_START, "<form action=\"%s\" method=\"get\">\n" },
	{ FORM_END, "</form>" },

	{ SELECT_START, "<select name=\"%s\" size=\"1\">\n" },
	{ SELECT_OPTION, "<option>%s</option>" },
	{ SELECT_END, "</option></select>" },

	{ TEXT_FIELD, "<input type=\"text\" name=\"%s\" size=\"40\" maxlength=\"150\">%s" },

	{ SUBMIT_BUTTON, "<input type=\"submit\" name=\"%s\" value=\"%s\">" },
		
	{ 0, NULL }
};






char *
find_html_wrapper (int message_type)
{
  register int i;
  for (i = 0; html_wrappers[i].message_type; i++)
    if (message_type == html_wrappers[i].message_type)
      return (html_wrappers[i].html);
  return ((char *)NULL);
}

int
http_write_string (const com_request_t *req, const int message_type, const char *buff)
{
	char *wrappertext = find_html_wrapper (message_type);

	if (wrappertext)
	{
		if (strstr (wrappertext, "%s") != NULL)
			return sock_write (req->con->sock, wrappertext, buff);
		return sock_write_string (req->con->sock, wrappertext);
	}
	return sock_write_string (req->con->sock, buff);
}

/* Function called when needed to do extra parsing of http variables before we call
   the ordinary command function */
void
wrap_http_function (const http_command_t *http_command, com_request_t *req, const comp_element *command)
{
	return;
}

int
http_display (com_request_t *req)
{
	char *arg = com_arg (req);
	char command[BUFSIZE+4];
	char file[BUFSIZE];

	if (!arg || !arg[0])
	{
		write_no_such_command (req->con, "NULL");
		return 0;
	}

	snprintf(command, BUFSIZE+4, "%s.html", arg);

	if (get_ntripcaster_file (command, template_file_e, R_OK, file) != NULL)
	{
		write_template_parsed_html_page (req->con, NULL, file, -1, NULL);
//		nfree (file);
		return 1;
	} else {
		write_http_header (req->con->sock, 200, "OK");
		sock_write_line (req->con->sock, "Connection: close");
		sock_write_line (req->con->sock, "Content-Type: text/html\r\n");
		sock_write_line (req->con->sock, "<html>No admin template page found for command %s. Create one and try again<br>", arg);
		sock_write_line (req->con->sock, "Or, you could try the raw command output here:<a href=\"/admin?mode=%s\">%s<br></html>", arg, arg);
		return 0;
	}
}

/*
int
http_help (com_request_t *req)
{
	char *file = get_template ("manual.html");

	if (!file)
	{
		write_didnt_find_html_page (req->con, file);
		return 1;
	}

	http_puke_file_to_socket (req->con->sock, file);
	return 1;
}
*/

int
http_change (com_request_t *req)
{
	char *arg = com_arg (req);
	char helpfile[BUFSIZE];
	char file[BUFSIZE];
	vartree_t *extravars;

	if (!arg || !arg[0]) {
		sock_write_line (req->con->sock, "You must supply a variable to be changed<br>\r\n");
		return 0;
	}

	snprintf(helpfile, BUFSIZE, "%s.html", arg);

	if (get_ntripcaster_file (helpfile, template_file_e, R_OK, file) != NULL) {
		write_template_parsed_html_page (req->con, NULL, file, -2, NULL);
//		nfree (file);
	}

	if (get_ntripcaster_file ("admin_change.html", template_file_e, R_OK, file) == NULL) {
		write_didnt_find_html_page (req->con, "admin_change.html");
		return 1;
	}
	
	extravars = avl_create (compare_vars, &info);
	
	add_varpair2 (extravars, nstrdup ("varname"), nstrdup (arg));
	add_varpair2 (extravars, nstrdup ("currentvalue"), variable_to_string (arg));
	
	write_template_parsed_html_page (req->con, NULL, file, -2, extravars);
	
//	nfree (file);
	
	return 1;
}
	

void 
write_no_such_command (connection_t *con, const char *name)
{
	write_http_header (con->sock, 200, "OK");
	sock_write_line (con->sock, "Connection: close");
	sock_write_line (con->sock, "Content-Type: text/html\r\n");
	sock_write_line (con->sock, "<html><h1>ERROR: Nonexistant command [%s]<br></html>", name);
}

/*
void http_puke_file_to_socket (SOCKET s, const char *template_file)
{
	struct stat st;
	int ffd = open_for_reading(template_file);

	write_http_header(s, 200, "OK");
	sock_write_line(s, "Connection: close");
	sock_write_line(s, "Content-Type: text/html\r\n");


	if (ffd == -1) {
		sock_write_line(s, "ERROR: Cannot open file [%s]<br>",
				template_file);
		return;
	}

	if (fstat(ffd, &st) == -1) {
	  sock_write_line(s, "ERROR: Cannot stat file [%s]",
			  template_file);
	  return;
	}
	sock_dump_fd (s, ffd, st.st_size);
}
*/

/* added to show links on generated pages. ajd */
int
http_write_links(const com_request_t * req)
{
	int i=0;

	admin_write (req, ADMIN_SHOW_LINKS, "<nobr>");

	while (http_links[i].path != NULL) {
		admin_write (req, ADMIN_SHOW_LINKS, "<a href=\"%s\">%s</a>%s", http_links[i].path, http_links[i].link, http_links[i].space);
		i++;
	}

	admin_write (req, ADMIN_SHOW_LINKS, "</nobr><hr>");

	return 1;

}

/*
void
http_display_auth_page (com_request_t *req) {

	admin_write (req, ADMIN_SHOW_AUTH_USER_START, "<h2>Authentication</h2><form action=\"/admin\" method=\"get\">\r\n<input type=\"hidden\" name=\"mode\" value=\"auth\">\r\n");

	admin_write (req, ADMIN_SHOW_AUTH_USER_START, "<table border=\"0\" cellpadding=\"5\" cellspacing=\"0\">\r\n");

	admin_write (req, ADMIN_SHOW_AUTH_USER_START, "<tr><td colspan=\"3\"><input type=\"text\" name=\"tf\" size=\"100\" maxlength=\"100\"></td></tr>");
	admin_write (req, ADMIN_SHOW_AUTH_USER_START, "<tr><td valign=\"top\"><input type=\"submit\" name=\"argument\" value=\"add user\"><br><input type=\"submit\" name=\"argument\" value=\"del user\"></td>");
	admin_write (req, ADMIN_SHOW_AUTH_USER_START, "<td valign=\"top\"><input type=\"submit\" name=\"argument\" value=\"add group\"><br><input type=\"submit\" name=\"argument\" value=\"del group\"><br><input type=\"submit\" name=\"argument\" value=\"set max_num_con\"></td>");
	admin_write (req, ADMIN_SHOW_AUTH_USER_START, "<td valign=\"top\"><input type=\"submit\" name=\"argument\" value=\"add mount\"><br><input type=\"submit\" name=\"argument\" value=\"del mount\"></td></tr>");

	admin_write (req, ADMIN_SHOW_AUTH_USER_START, "<tr><td valign=\"top\">");

	html_display_users (req);

	admin_write (req, ADMIN_SHOW_AUTH_USER_START, "</td><td valign=\"top\">");

	html_display_groups (req);

	admin_write (req, ADMIN_SHOW_AUTH_USER_START, "</td><td valign=\"top\">");

 	html_display_mounts (req);

	admin_write (req, ADMIN_SHOW_AUTH_USER_START, "</td></tr></table>");

  admin_write (req, ADMIN_SHOW_AUTH_USER_END, "</form>");
}
*/
