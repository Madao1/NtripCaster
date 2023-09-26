/* relay.c
 * - Relay functions
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
#include <errno.h>
# ifndef __USE_BSD
#  define __USE_BSD
# endif
#ifndef __EXTENSIONS__
#define __EXTENSIONS__
#endif

#include <string.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <winsock.h>
#endif

#include "avl.h"
#include "threads.h"
#include "ntripcastertypes.h"
#include "ntripcaster.h"
#include "ntrip.h"
#include "utility.h"
#include "ntripcaster_string.h"
#include "connection.h"
#include "relay.h"
#include "threads.h"
#include "client.h"
#include "sock.h"
#include "ntripcaster_resolv.h"
#include "source.h"
#include "log.h"
#include "memory.h"
#include "commands.h"
#include "vars.h"
#include "logtime.h"
#include "pool.h"

extern server_info_t info;
extern mutex_t library_mutex;
extern mutex_t authentication_mutex;

/* 
 * Add a pulling relay to the list of relays automagically connected now and then.
 * Possible error codes:
 * ICE_ERROR_INVALID_SYNTAX    - Invalid syntax or too few arguments 
 * ICE_ERROR_NO_SUCH_MOUNT     - Trying to push a nonexistant mount
 * ICE_ERROR_ARGUMENT_REQUIRED - The given option requires an argument
 * Assert Class: 3
 */
int
relay_add_pull_to_list (char *arg) {
	relay_t *new;
	char localmount[BUFSIZE];
	char id[BUFSIZE]; //added. ajd

	if (!arg || !arg[0]) return ICE_ERROR_INVALID_SYNTAX;
	
	xa_debug (2, "DEBUG: Adding [%s] to list of pulling relays", arg);

	new = relay_create();

	localmount[0] = '\0';
	id[0] = '\0';

	while ((arg[0] == '-') || (arg[0] == ' ')) { // loop to parse options. ajd
		if (arg[0] == ' ')
			arg++;
		else if (arg[1] == 'm') {
			splitc (NULL, arg, ' ');
			if (splitc (localmount, arg, ' ') == NULL) {
				relay_dispose(new);
				return ICE_ERROR_ARGUMENT_REQUIRED;
			}
		} else if (arg[1] == 'i') {
			splitc (NULL, arg, ' ');
			if (splitc (id, arg, ' ') == NULL) {
				relay_dispose(new);
				return ICE_ERROR_ARGUMENT_REQUIRED;
			}
		} else if (arg[1] == 'n') {
			splitc (NULL, arg, ' ');
			new->type = relay_nontrip_e;
		} else
			splitc (NULL, arg, ' ');
	}

	generate_http_request (arg, &(new->req));

	if (!new->req.path[0]) {
		relay_dispose(new);
		return ICE_ERROR_INVALID_SYNTAX;
	}

	if (is_empty_request(&(new->req))) new->type = relay_nontrip_e;

	if (localmount[0] != '\0') new->localmount = slashalize(localmount, 0);
	if (id[0] != '\0') new->userID = util_base64_encode(id);

	if (relay_insert(new) != OK) {
		xa_debug (3, "DEBUG: Insertion of pulling relay failed");
		relay_dispose(new);
		return 0;
	}

	xa_debug (3, "DEBUG: Insertion of pulling relay succeeded");

	return OK;
}

/*
 * Remove the relay from list of relays
 * Possible error codes:
 * ICE_ERROR_NOT_FOUND - No such relay in list
 * ICE_ERROR_NULL - Argument was NULL
 * Assert Class: 1
 */
int
relay_remove (relay_t *relay)
{
	relay_t *out;

	if (!relay) {
		write_log (LOG_DEFAULT, "WARNING: Trying to remove a NULL relay from tree");
		return ICE_ERROR_NULL;
	}

	thread_mutex_lock (&info.relay_mutex);
	out = avl_delete (info.relays, relay);
	thread_mutex_unlock (&info.relay_mutex);

	if (out) {
		xa_debug (2, "DEBUG: Removed relay [%s:%d%s]", out->req.host, out->req.port, out->req.path);
		relay_dispose (out);
		return OK;
	}
	
	return ICE_ERROR_NOT_FOUND;
}

int
relay_remove_with_con (connection_t *con)
{
	relay_t *rel;

	thread_mutex_lock (&info.relay_mutex); 
	rel = relay_find_with_con (con);
	thread_mutex_unlock (&info.relay_mutex);

	if (rel) return relay_remove (rel);
	return ICE_ERROR_NOT_FOUND;
}

int
relay_remove_with_req (ntrip_request_t *req)
{
	relay_t *rel;
	
	thread_mutex_lock (&info.relay_mutex);
	 
	rel = relay_find_with_req (req);
	
	thread_mutex_unlock (&info.relay_mutex);
	
	if (rel) return relay_remove (rel);
	return ICE_ERROR_NOT_FOUND;
}

/*
 * Find a relay given a request struct 
 * Assert Class: 2
 */
relay_t *relay_find_with_req (ntrip_request_t *req) {
	relay_t rel, *out = NULL;

	strncpy(rel.req.host, req->host, BUFSIZE);
	rel.req.port = req->port;
	strncpy(rel.req.path, req->path, BUFSIZE);
	rel.localmount = NULL;

//	thread_mutex_lock (&info.relay_mutex);
	out = avl_find (info.relays, &rel);
//	thread_mutex_unlock (&info.relay_mutex);
	
	return out;
}

/*
 * Find a relay given a connection struct
 * Assert Class: 2
 */
relay_t *
relay_find_with_con (connection_t *con)
{
	avl_traverser trav = {0};
	relay_t *rel;

	if (!con) return NULL;

	while ((rel = avl_traverse (info.relays, &trav))) {
		if (rel->con == con)
			break;
	}

	if (rel && rel->con == con) return rel;
	return NULL;
}


/*
 * Insert a relay struct into the list of relays
 * Assert Class: 0
 */
int relay_insert (relay_t *relay) {
	relay_t *out = NULL;

	if (!relay) {
		xa_debug (2, "DEBUG: Trying to insert NULL relay into relay tree");
		return 0;
	}

	relay->reconnect_now = 1;

	thread_mutex_lock (&info.relay_mutex);
	out = avl_replace (info.relays, relay);

	if (out != NULL) {
		xa_debug (2, "DEBUG: Replaced already existing relay %s:%d%s with %s:%d%s", out->req.host, out->req.port, out->req.path, relay->req.host, relay->req.port, relay->req.path);

		relay->con = out->con;
		relay->reconnects = out->reconnects;
		relay->last_reconnect = out->last_reconnect;
		relay->reconnect_now = out->reconnect_now;
		relay->pending = out->pending;

		relay_dispose(out);
	}

	thread_mutex_unlock (&info.relay_mutex);

	return OK;
}

/*
 * Free dynamic memory in a relay struct
 * Assert Class: 1
 */
void
relay_dispose (relay_t *relay)
{
	if (relay->con) relay->con = NULL;
	if (relay->localmount != NULL) nfree (relay->localmount);
	if (relay->userID != NULL) nfree (relay->userID);
	nfree (relay);
}

/* 
 * Dynamically create a relay struct
 * Assert Class: 1
 */
relay_t *
relay_create ()
{
	relay_t *relay = (relay_t *) nmalloc (sizeof (relay_t));
	relay->con = NULL;
	relay->localmount = NULL;
	relay->userID = NULL;
	relay->reconnects = 0;
	relay->last_reconnect = (time_t) 0;
	relay->type = relay_pull_e;
	relay->reconnect_now = 0;
	relay->pending = 0;

	zero_request(&relay->req);

	return relay;
}

relay_t *relay_copy (relay_t *other) {
	relay_t *relay = relay_create();

	relay->con = other->con;

	if (other->localmount != NULL) relay->localmount = strndup(other->localmount, BUFSIZE);
	if (other->userID != NULL) relay->userID = strndup(other->userID, BUFSIZE);

	relay->reconnects = other->reconnects;
	relay->last_reconnect = other->last_reconnect;
	relay->type = other->type;
	relay->pending = other->pending;

	strcpy(relay->req.path, other->req.path);
	strcpy(relay->req.host, other->req.host);
	relay->req.port = other->req.port;

	return relay;
}

/*
 * Run through the list of relays, and connect the not connected ones
 * according to relay type.
 * Assert Class: 2
 */
void 
relay_connect_all_relays ()
{
	avl_traverser trav = {0};
	relay_t *rel = NULL;
	time_t now;

	xa_debug (4, "DEBUG: Reconnecting unconnected relays....");
	
/* that's why the server hanged from time to time. it takes some time to connect all
 * relays and during that time the double_mutex was locked.
 * Locking (and unlocking) of double_mutex no longer needed because of unlocking and
 * locking relay_mutex in function 'relay_connect_pull()'. ajd
 */
//	thread_mutex_lock (&info.double_mutex);

	if (!info.relays) {
		write_log (LOG_DEFAULT, "WARNING: info.relays is NULL, weeird!");
	}
	
	thread_mutex_lock (&info.relay_mutex);

	while ((rel = avl_traverse (info.relays, &trav)))
	{
		now = get_time ();
		if (!relay_connected_or_pending (rel)) {
			if (rel->reconnect_now) {
				xa_debug (3, "DEBUG: Immediately connecting relay");
				rel->reconnects++;
				rel->last_reconnect = now;
				rel->reconnect_now = 0;
				rel->pending = 1;
				thread_create("Relay Connect List Item", relay_connect_list_item, (void *)relay_copy(rel));
				
			} else if ((((rel->last_reconnect + info.relay_reconnect_time) < now) || (rel->last_reconnect == 0)) && ((rel->reconnects < info.relay_reconnect_tries) || (info.relay_reconnect_tries == -1)))
			{
				rel->reconnects++;
				rel->last_reconnect = now;
				rel->reconnect_now = 0;
				rel->pending = 1;
				thread_create("Relay Connect List Item", relay_connect_list_item, (void *)relay_copy(rel));
			}
		}
	}

	thread_mutex_unlock (&info.relay_mutex);
//	thread_mutex_unlock (&info.double_mutex); // not recommendable (see above). ajd
	
	xa_debug (4, "DEBUG: Done reconnecting relays.");
}

/*
 * Is relay connected?
 * Assert Class: 0
 */
int
relay_connected_or_pending (relay_t *rel)
{
	if (!rel) {
		write_log (LOG_DEFAULT, "WARNING: relay_connected(): Relay is NULL!");
		return 0;
	}
	
	if ((rel->con != NULL) || (rel->pending == 1))
		return 1;
	else
		return 0;
}

/* 
 * Connect a single relay from the list, according to type
 * Assert Class: 1
 *
 */

void *relay_connect_list_item (void *arg) // relay (arg) must be freed. ajd
{
	relay_t *rel = (relay_t *)arg;
	relay_t *orginal;
	ntrip_request_t *relreq = (ntrip_request_t *) &rel->req;

	thread_init();
	
	xa_debug (2, "DEBUG: Reconnecting relay [%s:%d%s]", relreq->host, relreq->port, relreq->path);

	/* Does not return unless an error happens or the source dies. */
	relay_connect_pull (rel);

	thread_mutex_lock (&info.relay_mutex);

	orginal = relay_find_with_req (relreq);
	if (orginal != NULL) {
		orginal->con = NULL;
		orginal->pending = 0;
	}

	thread_mutex_unlock (&info.relay_mutex);

	relay_dispose (rel);

	thread_exit(0);
	return NULL;
}

void relay_source_setmp(connection_t *con, relay_t *rel) {
	source_t *source = con->food.source;
	source->type = pulling_source_e;

	if (rel->localmount != NULL)
		source->audiocast.mount = nstrdup (rel->localmount);
	else {
		if (is_empty_request(&(rel->req)))
			source->audiocast.mount = create_random_mountpoint(8);
		else
			source->audiocast.mount = nstrdup (rel->req.path);
	}
}

/* 
 * Initiate a pulling relay connection, login and add to list of sources
 * Errors in err, one of following:
 * ICE_ERROR_INIT_FAILED - Failed to initialize relay resources
 * ICE_ERROR_INSERT_FAILED - Failed to insert new source in sourcetree (login failure...)
 * Assert Class: 4
 */
int relay_connect_pull (relay_t *rel) {
	connection_t *newcon;
	int res;
	relay_t *relay = NULL;
	ntrip_request_t *relreq = &rel->req;
	
/* Setup the connection with sockets and stuff.
 * connection_t newcon must be closed and freed
 * (in login_as_[nontrip_]client_on_server(...) or relay_source_login(...))
 */
	newcon = relay_setup_connection (relreq);
        relay_source_setmp(newcon, rel);

//	if (newcon == NULL) {
//		xa_debug (4, "WARNING: relay_setup_connection() failed");
//		return ICE_ERROR_INIT_FAILED;
//	}

	if (rel->type == relay_nontrip_e)
		res = login_as_nontrip_client_on_server (newcon, rel);
	else
		res = login_as_client_on_server (newcon, rel);

	if (res < 0) {
		xa_debug (4, "WARNING: relay login as client failed (%d)", res);
		return ICE_ERROR_CONNECT;
	}

	thread_mutex_lock (&info.relay_mutex);

	relay = relay_find_with_req (relreq);
	if (relay) {
		relay->con = newcon;
		relay->pending = 0;
	}

	thread_mutex_unlock (&info.relay_mutex);

/* Does not return unless an error happens or the source dies. ajd */
	relay_source_login (newcon, rel);

	xa_debug (4, "Relay connection ended.");

	return OK;
}

void relay_source_login(connection_t *con, relay_t *rel) {
	source_t *source = con->food.source;

	xa_debug (2, "DEBUG: Relay encoder logging in on mount [%s]", source->audiocast.mount);

	thread_mutex_lock(&info.source_mutex);
		
	if (mount_exists (source->audiocast.mount)) {
		thread_mutex_unlock(&info.source_mutex);
		kick_connection (con, "Relay source with existing Mountpoint");
		return;
	}

	if ((info.num_sources + 1) > info.max_sources) {
		thread_mutex_unlock(&info.source_mutex);
		kick_connection (con, "Relay source: server Full (too many streams)");
		return;
	}

	add_source();
	source->connected = SOURCE_CONNECTED;
	avl_insert(info.sources, con);

	thread_mutex_unlock(&info.source_mutex);

	write_log (LOG_DEFAULT, "Accepted relay encoder on mountpoint %s from %s. %d sources connected", source->audiocast.mount, con_host(con), info.num_sources);

	thread_rename("Source Thread");

	source_func(con);
}


/*
 * Login as a client on a given server
 * Returns OK or one of the following errors:
 * ICE_ERROR_CONNECT - Connection failed 
 * ICE_ERROR_TRANSMISSION - Connect worked, but failed to read data
 * ICE_ERROR_HEADER - Invalid headers received from server
 * Assert Class: 3
 */
int
login_as_client_on_server (connection_t *con, relay_t *rel)
{
	SOCKET sockfd;
	const char *relaystr = "Relay refused entrance: ";
	char buffer[BUFSIZE];
	int res = 0, i;
	char *recvbuf, *text=0;
	ntrip_request_t *req = &rel->req;

	if ((sockfd = sock_connect_wto (req->host, req->port, 15)) == -1) {
		xa_debug (4, "WARNING: sock_connect_wto() to [%s:%d] failed", req->host, req->port);
		con->connect_time = get_time ();
		kick_connection (con, "Relay: Could not connect");
		return ICE_ERROR_CONNECT;
	}

	con->sock = sockfd;
	con->connect_time = get_time ();

	snprintf(buffer, BUFSIZE, "GET %s HTTP/1.0\r\nHost: %s\r\n", req->path, req->host);
	catsnprintf(buffer, BUFSIZE, "User-Agent: NTRIP Caster/%s\r\nReferer: RELAY\r\n", info.version);
	if (rel->userID != NULL) catsnprintf(buffer, BUFSIZE, "Authorization: Basic %s\r\n", rel->userID);

	sock_write_line (sockfd, buffer);

	i = strlen(relaystr);
	recvbuf = buffer+i;
	memcpy(buffer, relaystr, i);
	//res = sock_read_lines_with_timeout(sockfd, recvbuf, sizeof(buffer)-i);
	res = sock_read_line_with_timeout(sockfd, recvbuf, sizeof(buffer)-i);

	if (res < 0) {
		kick_connection (con, "Relay: Error in read");
		return ICE_ERROR_TRANSMISSION;
	}

	if (ntripcaster_strncmp (recvbuf, "ICY 200 OK", 10) != 0) {
		for(i = 0; i < 100 && recvbuf[i] && recvbuf[i] >= 0x20 && recvbuf[i] < 0x7F; ++i)
			;
		recvbuf[i] = 0;
		/* we already appended the error text before, so send as one string */
		kick_connection (con, buffer);
		return ICE_ERROR_HEADER;
	}

	/* disabled, would require Ntrip 2.0 access
	for(i = 10; i < res; ++i)
	{
		if(!strncmp("Server: ", recvbuf+i, 8))
		{
			i += 8;
			text = recvbuf+i;
			for(; recvbuf[i] && recvbuf[i] >= 0x20 && recvbuf[i] < 0x7F; ++i)
				;
			recvbuf[i] = 0;
			break;
		}
	}
	*/

	if(!text)
	{
		snprintf(buffer, sizeof(buffer), "NTRIP Caster/%s (relay)", info.version);
		text = buffer;
	}
	add_varpair2(con->headervars, nstrdup("Source-Agent"), nstrdup(text));

	return OK;
}

int login_as_nontrip_client_on_server (connection_t *con, relay_t *rel) {
	SOCKET sockfd;
	char buffer[50];
	ntrip_request_t *req = &rel->req;

	snprintf(buffer, sizeof(buffer), "NTRIP Caster/%s (direct access)", info.version);
	add_varpair2(con->headervars, nstrdup("Source-Agent"), nstrdup(buffer));

	if ((sockfd = sock_connect_wto (req->host, req->port, 15)) == -1) {
		xa_debug (4, "WARNING: sock_connect_wto() to [%s:%d] failed", req->host, req->port);
		con->connect_time = get_time ();
		kick_connection (con, "Relay: Could not connect");
		return ICE_ERROR_CONNECT;
	}

	con->sock = sockfd;
	con->connect_time = get_time ();

	return OK;
}

/*
 * Allocate and innitiate a relay connection and request struct
 * Assert Class: 2
 */
connection_t *relay_setup_connection (ntrip_request_t *req) {
	connection_t *con;
	char c[20];

	con = create_connection();
	con->type = source_e;
	con->headervars = create_header_vars();

	con->com_protocol = ntrip1_0_e;
	con->data_protocol = tcp_e;
	con->trans_encoding = not_chunked_e;

	if (forward (req->host, c) != NULL)
		con->host = nstrdup(c);
	else
		con->host = nstrdup(req->host);

	con->id = new_id ();
	if (info.reverse_lookups) con->hostname = reverse (con->host);

	put_source(con);
	con->food.source->type = pulling_source_e;

	return con;
}


