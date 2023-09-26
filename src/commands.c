/* commands.c
 * - Admin Commands Handling
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
#include "definitions.h"

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "avl.h"
#include "threads.h"
#include "ntripcaster.h"
#include "ntripcastertypes.h"
#include "ntrip.h"
#include "commands.h"
#include "sock.h"
#include "ntripcaster_resolv.h"
#include "log.h"
#include "main.h"
#include "utility.h"
#include "ntripcaster_string.h"
#include "relay.h"
#include "logtime.h"
#include "source.h"
#include "client.h"
#include "avl_functions.h"
#include "timer.h"
#include "alias.h"
#include "restrict.h"
#include "memory.h"
#include "admin.h"
#include "sourcetable.h"
#include "match.h"
#include "connection.h"
#include "item.h"
#include "authenticate/basic.h"
#include "authenticate/user.h"
#include "authenticate/group.h"
#include "authenticate/mount.h"
#include "pool.h"
#include "interpreter.h"
#include "http.h"
#include "vars.h"
#include "sourcetable.h"

#include <time.h>
#include <errno.h>

extern int errno;
extern server_info_t info;
extern mutex_t library_mutex;
extern mutex_t authentication_mutex;
extern mutex_t sock_mutex;
extern avl_tree *sock_sockets;

typedef struct
{
	char opt;
	char *longopt;
	int arindex;
} conopt_t;

static void zero_opts (int *opt, int maxelements);
static void set_opts (int *opt, const char *arg, conopt_t *opts, int maxelements);

/* All the remote admin commands */
/* unused commands removed. ajd*/
comp_element commands[] = 
{
	{ "alias",   com_alias,    "Add an alias for a mount point", 1, 1,
	  "alias <add|del|list> [newmountpoint] [mountpoint] or [newmountpoint] [http://server:port/mountpoint]\r\n\tMakes mountpoint available on 2 places\r\n" },
	{ "allow",   com_allow,    "Add hostmask for allowed host", 1, 1,
	  "allow <client|source|admin|all> <add|del|list> [hostmask]\r\n\tAdd, delete or list allowed hostmasks\r\n" },
	{ "deny",    com_deny,     "Add hostmask for denied host", 1, 1,
	  "deny <client|source|admin|all> <add|del|list> [hostmask]\r\n\tAdd, delete or list denied hostmasks\r\n" },
	{ "acl",     com_acl, 	   "List all access controls", 1, 1,
	  "acl\r\n\tList all access controls\r\n" },
	{ "admins",  com_admins,   "Show all connected admins", 0, 1,
	  "admins [pattern]\r\n\tDisplays all admins matching pattern, or all just all admins if no pattern is specified\r\n"},
//	{ "dir",     com_dir,      "Add, delete or remove directory servers", 1, 1,
//	  "dir [add|delete|list]\r\n\tadd host[:port[/path] [icy]], del <host>, or list all directory servers\r\n"},
//	{ "dump",    com_dump,     "Dump a source to a specified file", 1, 1,
//	  "dump <sourceid> <filename|close>\r\n\tDump source with id <sourceid> to a file specified with filename, or stop dumping if the filename is \"close\".\r\n"},
	{ "help",    com_help,     "Display this text", 0, 1,
	  "help <command>\r\n\tDisplay general help, or help for a specific command.\r\n"},
	{ "kick",    com_kick,     "Kick client, admin, or source", 1, 1,
	  "kick <options> <pattern|id>\r\n\t-c for clients, -a for admins, -s for sources, kicks specified pattern or id.\r\n"},
	{ "listeners", com_listeners, "Show connected listeners", 0, 1,
	  "listeners <pattern>\r\n\tDisplay all listeners matching pattern, or all listeners if no pattern is specified\r\n" },
//	{ "modify",  com_modify,   "Change the audiocast data for a source", 1, 1,
//	  "modify <source_id> <-mungdPpb> <argument>\r\n\tModify a audiocast field for the specified source\r\n" },
	{ "oper",    com_oper,     "Obtain operator privileges", 0, 1,
	  "oper <password>\r\n\tIf correct password is specified, makes you a NtripCaster operator and grants access to the operator commands\r\n" },
//	{ "pause",   com_pause,    "Put a client or source on hold", 1, 1,
//	  "pause <client_id|source_id>\r\n\tEither pause a client (cutting the data), or a source (no data to its clients)\r\n"},
	{ "quit",    com_quit,     "Leave remote admin console", 0 , 1,
	  "quit <message>\r\n\tLeaves the remote admin console with message displayed for all other admins.\r\n"},
	{ "rehash",  com_rehash,   "Parse the config file and update sourcetable.dat.upd", 1, 1,
	  "rehash <configfile>\r\n\tOpen and parse the configfile, or the default configfile if not specified, and update sourcetable.dat.upd.\r\n"},
//	{ "select",  com_sel,      "Select encoder for clients", 1, 1,
//	  "select <options> <sid> [pattern] <tid>\r\n\tEither select -a <sourceid> <targetid>, which moves all clients from sourceid to targetid. Or select -p <sourceid> <pattern> <targetid>, which moves all clients matching pattern from sourceid to targetid. Or select <clientid> <targetsourceid>, which moves client with id clientid to source with id targetsourceid.\r\n"},
	{ "sources", com_sources,  "Show connected encoders", 0, 1,
	  "sources [pattern] [options]\r\n\tShow connected sources matching pattern, or all sources if no pattern is specified.\r\n"},
	{ "resync", com_resync, "Resync the server", 1, 1,
	  "resync <message>\r\n\tResync the server, display message to all connected admins.\r\n"},
	{ "set",     com_set,      "Set <var> to <arg>", 1, 1,
	  "set [variable] [argument]\r\n\tIf both variable and argument, set variable to argument. If just variable, show current setting. If just set, then show all current settings.\r\n"},
	{ "stats",   com_stats,    "Show server statistics", 0, 1,
	  "stats <daily|hourly|clear>\r\n\tShow current server statistics, averages, uptime, etc.  Argument is optional - either gives daily stats, hourly stats, or clears all stats\r\n"},
	{ "tail",    com_tail,     "Start tailing the logfile", 0, 1,
	  "tail\r\n\tIntercept NtripCaster server log messages.\r\n"},
	{ "tell",    com_tell,     "Tell other admins", 0, 1,
	  "tell <message>\r\n\tDisplay message for all connected admins.\r\n"},
//	{ "touch",   com_touch,    "Touch the directory server", 1, 1,
//	  "touch\r\n\tForce all directory servers to be touched now.\r\n"}, 1,
//	{ "unpause", com_unpause,  "Unpause a client or source", 1, 1,
//	  "unpause <client_id|source_id>\r\n\tRemove a lock on a client or source, making it run like normal again\r\n."},
	{ "untail",  com_untail,   "Stop tailing the logfile", 0, 1,
	  "untail\r\n\tDon't display logfile messages.\r\n"},
	{ "uptime",  com_uptime,   "Show server uptime", 0, 1,
	  "uptime\r\n\tShow NtripCaster server version and uptime.\r\n"},
	{ "list",    com_list,     "List all current connections", 0, 1,
	  "list <pattern>\r\n\tList all connections matching pattern, or all connections if no pattern is specified.\r\n"},
	{ "relay",   com_relay,    "Pull a server relay", 1, 1,
	  "relay pull [-i userID] [-m local mountpoint] <url>>\r\n\t(pull) relay a source from a remote server.\r\n"},
	{ "threads", com_threads,  "Show all program threads", 0, 0,
	  "threads [kill <id>]\r\n\tDisplay all the currently running threads in the program, and where and when they were created [kill thread with id].\r\n"},
	{ "locks",   com_locks,    "Show status of all locks", 0, 0,
	  "locks [unlock source]\r\n\tDisplay all mutexes, and their status [force unlocking of source_mutex].\r\n"},
	{ "status",  com_status,   "Turn status info on/off (for you)", 0, 1,
	  "status [on|off|show]\r\n\tTurn on or off the periodic status information line.\r\n"},
	{ "debug",   com_debug,     "Set debugging output level for this connection.", 0, 0,
	  "debug <integer>\r\n\tSets debugging level. Higher value is more debugging output.\r\n"},
	{ "mem", com_mem,          "Another debug function, shows mem usage.", 0, 0,
	  "mem\r\n\tShows all alloced addresses.\r\n"},
//	{ "streamtitle", com_streamtitle, "Set and/or lock streamtitle for a source.", 0, 1,
//	  "streamtitle <-a|sourceid> <message>\r\n\tSets and/or locks the stream title for source or [-a] all sources.\r\n"},
//	{ "streamurl", com_streamurl, "Set and/or lock streamurl for a source.", 0, 1,
//	  "streamurl [-a] [sourceid] <message>\r\n\tSets the stream url for source or [-a] all sources.\r\n"},
	{ "describe", com_describe, "Show detailed view on a connection.", 0, 1,
	  "describe <id>\r\n\tDisplay all information available for the specified connection id.\r\n"},
	{ "auth", com_auth, "Show authorization groups, users, or mountpoints.", 1, 1,
	  "auth <groups|users|mounts>\r\n\tList all authentication entries for users, groups or mounts.\r\n"},
	{ "scheme", com_scheme, "Change output format.", 0, 1, "scheme <default|html|tagged>\r\n\tChange the output format for all admin commands.\r\n"},
	{ "server_info", com_runtime, "Display runtime information.", 0, 0, "server_info\r\n\tDisplay information about server only available at runtime.\r\n"},
//	{ "resolv", com_resolv, "Resolv given ip or hostname.", 0, 1, "resolv <hostname|ip>\r\n\tResolv hostname/ip.\r\n"},
//	{ "ping", com_ping, "Connect to given NtripCaster server and check if it's alive.", 0, 1, "ping <server[:port]>\r\n\tConnect to NtripCaster server and check if it is up.\r\n"},
//	{ "sock", com_sock, "List or close sockets.", 1, 1, "sock <list|close> [sock]\r\n\tList all sockets or close a given socket descriptor.\r\n"},
//	{ "run", com_run, "Let interpreter run a file.", 1, 1, "run <filename>\r\n\tEvaluate the file with the interpreter.\r\n"},
	{ (char *) NULL, (ntripcaster_int_function *)NULL, (char *)NULL, 0 , 1}
};

/* All the settings for the remote admin */
/* unused settings removed. ajd */
set_element admin_settings[] = 
{
//  { "encoder_password", string_e,  "The encoder password", NULL },
//  { "client_password", string_e, "<unimplemented> Client password", NULL },
  { "admin_password", string_e, "The remote admin password", NULL },
  { "oper_password", string_e, "Operator Password", NULL },
//  { "touch_freq", integer_e, "Touch frequency", NULL },
  { "client_timeout", integer_e, "When to kick out clients", NULL },
  { "max_clients", integer_e, "How many listeners to let in", NULL },
  { "max_clients_per_source", integer_e, "Max number of clients per source", NULL },
  { "max_sources", integer_e, "How many sources to let in", NULL },
  { "max_admins", integer_e, "How many admins to let in", NULL },
//  { "reverse", integer_e, "Whether to reverse ip:s to hostnames", NULL },
  { "location", string_e, "NtripCaster servers geographical location", NULL },
  { "rp_email", string_e, "Resposible person email", NULL },
//  { "transparent_proxy", integer_e, "Whether to act as transparent proxy", NULL },
//  { "statsfilename", string_e, "Filename for dumping periodic stats", NULL },
//  { "statshtmlfilename", string_e, "Filename for dumping periodic stats in html format", NULL },
//  { "stats_time", integer_e,       "How often to write to the stats logfile, in seconds", NULL},
// 'acl_policy' was here. ajd
  { "throttle", real_e,      "At what bandwidth usage (KB/s) do we deny access?", NULL},
//  { "kick_relays", integer_e, "Kick relays after how many seconds without listeners? (0 means keep forever)", NULL},
//  { "kick_clients", integer_e, "0 means move clients to default source when their source dies means kick them out", NULL},
//  { "status_time", integer_e, "Seconds between status line updates", NULL},
//  { "logfiledebuglevel", integer_e, "Debug level for logfile output", NULL},
//  { "consoledebuglevel", integer_e, "Debug level for console output", NULL},
  { "server_url", string_e, "URL for this Ntrip Caster server", NULL},
/*  { "use_meta_data", integer_e, "Whether to include metadata in the stream", NULL},
  { "streamurllock", integer_e, "Whether or not to allow sources to change the stream url", NULL},
  { "streamurl", string_e, "Server global stream url", NULL},
  { "streamtitletemplate", string_e, "Template string for stream title string", NULL},
  { "nametemplate", string_e, "Template string for stream name", NULL},
  { "desctemplate", string_e, "Template string for description template", NULL},
*/
//  { "logfile", string_e, "Main logfile", NULL},
//  { "accessfilename", string_e, "Access logfilename", NULL},
//  { "usagefilename", string_e, "Usage logfilename", NULL},
//  { "staticdir", string_e, "Directory for static files", NULL},
//  { "default_source_options", string_e, "Default options to 'sources'", NULL},
//  { "mount_fallback", integer_e, "Fallback to default mount (1 yes, 0 no)", NULL},
//  { "force_servername", integer_e, "Force use of server_name on directory server", NULL},
//  { "resolv_type", integer_e, "Type of resolv function to use. 1 is linux reentrant, 2 is solaris reentrant, 3 is standard nonreentrant", NULL},
//  { "http_admin", integer_e, "Whether to allow admins on the WWW interface. 1 is yes, 0 means no", NULL},
//  { "relay_reconnect_max", integer_e, "How many times to try reconnecting a relay, -1 means forever", NULL},
//  { "relay_reconnect_time", integer_e, "Seconds to wait between reconnects", NULL},
//  { "sleep_ratio", real_e, "Ratio that affects source sleep time. Larger value means sleep more (normal values (0.0 - 1.0)", NULL },
//  { "acl_policy", integer_e, "Whether to allow (1), or deny (0) by default", NULL}, // moved here from above. ajd
  { (char *) NULL, 0, (char *) NULL, NULL }
};

/* Make sure the array of admin settings point to the right data */
/* unused setings removed!!!. ajd */
void
setup_admin_settings ()
{
	int x = 0;
	/* The order of these are IMPORTANT, get it? */
//	admin_settings[x++].setting = &info.encoder_pass;
//	admin_settings[x++].setting = &info.client_pass;
	admin_settings[x++].setting = &info.remote_admin_pass;
	admin_settings[x++].setting = &info.oper_pass;
//	admin_settings[x++].setting = &info.touch_freq;
	admin_settings[x++].setting = &info.client_timeout;
	admin_settings[x++].setting = &info.max_clients;
	admin_settings[x++].setting = &info.max_clients_per_source;
	admin_settings[x++].setting = &info.max_sources;
	admin_settings[x++].setting = &info.max_admins;
//	admin_settings[x++].setting = &info.reverse_lookups;
	admin_settings[x++].setting = &info.location;
	admin_settings[x++].setting = &info.rp_email;
//	admin_settings[x++].setting = &info.transparent_proxy;
//	admin_settings[x++].setting = &info.statsfilename;
//	admin_settings[x++].setting = &info.statshtmlfilename;
//	admin_settings[x++].setting = &info.statstime;
// 'info.policy' was here. ajd
	admin_settings[x++].setting = &info.throttle;
//	admin_settings[x++].setting = &info.kick_relays;
//	admin_settings[x++].setting = &info.kick_clients;
//	admin_settings[x++].setting = &info.statustime;
//	admin_settings[x++].setting = &info.logfiledebuglevel;
//	admin_settings[x++].setting = &info.consoledebuglevel;
	admin_settings[x++].setting = &info.url;
/*
	admin_settings[x++].setting = &info.use_meta_data;
	admin_settings[x++].setting = &info.streamurllock;
	admin_settings[x++].setting = &info.streamurl;
	admin_settings[x++].setting = &info.streamtitletemplate;
	admin_settings[x++].setting = &info.nametemplate;
	admin_settings[x++].setting = &info.descriptiontemplate;
*/
//	admin_settings[x++].setting = &info.logfilename;
//	admin_settings[x++].setting = &info.accessfilename;
//	admin_settings[x++].setting = &info.usagefilename;
//	admin_settings[x++].setting = &info.staticdir;
//	admin_settings[x++].setting = &info.default_sourceopts;
//	admin_settings[x++].setting = &info.mount_fallback;
//	admin_settings[x++].setting = &info.force_servername;
//	admin_settings[x++].setting = &info.resolv_type;
//	admin_settings[x++].setting = &info.allow_http_admin;
//	admin_settings[x++].setting = &info.relay_reconnect_tries;
//	admin_settings[x++].setting = &info.relay_reconnect_time;
//	admin_settings[x++].setting = &info.sleep_ratio;
//	admin_settings[x++].setting = &info.policy; // moved to here from above. ajd
}


/* All the settings for the config file */
set_element configfile_settings[] = 
{
	{ "encoder_password", string_e,  "The encoder password", NULL },
//	{ "client_password", string_e,   "<unimplemented> Client password", NULL },
	{ "admin_password", string_e,    "The remote admin password", NULL },
	{ "oper_password", string_e,     "Operator Password", NULL },
//	{ "touch_freq", integer_e,       "Touch frequency", NULL },
	{ "max_clients", integer_e,      "Highest number of client connections",  NULL },
	{ "max_ip_connections", integer_e,      "Highest number of client connections per IP",  NULL },
	{ "max_sources", integer_e,      "Highest number of source connections", NULL },
	{ "max_admins", integer_e,       "Highest number of admin connections", NULL },
	{ "logfilename", string_e,           "Logfile to write to", NULL },
	{ "watchfilename", string_e,     "Name of runtime watchfile", NULL },
	{ "pidfilename", string_e,       "Name of file containing the pid", NULL },
	{ "hostname", string_e,          "What host/ip to bind to", NULL},
	{ "server_name", string_e,	 "Server's hostname", NULL},
	{ "console_mode", integer_e,     "How to use the NtripCaster console", NULL },
	{ "client_timeout", integer_e,   "When to kick out clients", NULL },
//	{ "statsfilename", string_e,         "Stats logfilename to write to", NULL},
//	{ "statshtmlfilename", string_e, "Filename for dumping periodic stats in html format", NULL},
//	{ "stats_time", integer_e,       "How often to write to the stats logfile, in seconds", NULL},
	{ "max_clients_per_source", integer_e, "Max number of clients listening on one source", NULL},
	{ "reverse_lookups", integer_e,  "Whether to reverse lookup hostnames", NULL},
	{ "location", string_e, "NtripCaster server geographical location", NULL},
	{ "rp_email", string_e, "Resposible person email", NULL},
	{ "transparent_proxy", integer_e, "Whether to act as transparent proxy", NULL},
	{ "acl_policy", integer_e,        "Whether to allow (1) or deny (0) by default", NULL},
	{ "throttle", real_e,      "At what bandwidth usage (KB/s) do we deny access?", NULL},
	{ "kick_relays", integer_e, "Kick relays after how many seconds without listeners? (0 means keep forever)", NULL},
	{ "kick_clients", integer_e, "0 means move clients to default source when their source dies, 1 means kick them out", NULL},
	{ "status_time", integer_e, "Seconds between status line updates", NULL},
//	{ "staticdir", string_e, "Directory for static files", NULL},
	{ "logfiledebuglevel", integer_e,    "Debug level for logfile output", NULL},
	{ "consoledebuglevel", integer_e, "Debug level for console output", NULL},
  { "url", string_e, "URL for this NtripCaster server", NULL},
	{ "use_meta_data", integer_e, "Whether to include metadata in the stream", NULL},
	{ "streamurllock", integer_e, "Whether or not to allow sources to change the stream url", NULL},
	{ "streamurl", string_e, "Server global stream url", NULL},
	{ "streamtitletemplate", string_e, "Template string for stream title string", NULL},
	{ "nametemplate", string_e, "Template string for stream name", NULL},
	{ "desctemplate", string_e, "Template string for description template", NULL},
	{ "accessfilename", string_e, "Access logfile", NULL},
	{ "usagefilename", string_e, "Usage logfile", NULL},
	{ "default_source_options", string_e, "Default options to 'sources'", NULL},
	{ "mount_fallback", integer_e, "Fallback to default mount (1 yes, 0 no)", NULL},
	{ "force_servername", integer_e, "Force use of server_name on directory server", NULL},
	{ "logdir", string_e, "Directory for log files", NULL},
	{ "templatedir", string_e, "Directory for template files", NULL},
	{ "resolv_type", integer_e, "Type of resolv function to use. 1 is linux reentrant, 2 is solaris reentrant, 3 is standard nonreentrant", NULL},
	{ "http_admin", integer_e, "Whether to allow admins on the WWW interface. 1 is yes, 0 means no", NULL},
	{ "relay_reconnect_max", integer_e, "How many times to try reconnecting a relay, -1 means forever", NULL},
	{ "relay_reconnect_time", integer_e, "Seconds to wait between reconnects", NULL},
	{ "sleep_ratio", real_e, "Ratio that affects source sleep time. Larger value means sleep more (normal values (0.0 - 1.0)", NULL },
	{ "ntrip_info_url", string_e, "Where you find informations about the NTRIP protocol", NULL},
	{ "name", string_e, "Name of the NTRIP server", NULL},
	{ "operator", string_e, "Who operates the server", NULL},
	{ "operator_url", string_e, "Where you find informations about the NTRIP operator", NULL},
	{ "sourcetablefile", string_e, "Sourcetable file", NULL},
#ifdef HAVE_LIBLDAP
	{ "ldap_server", string_e, "LDAP server name for authentication", NULL},
	{ "ldap_uid_prefix", string_e, "LDAP user ID prefix for authentication", NULL},
	{ "ldap_people_context", string_e, "LDAP people context for authentication", NULL},
#endif /* HAVE_LIBLDAP */
#ifdef USE_CRYPT
  { "encrypt_passwords", string_e, "Encrypt base parameter for password encryption", NULL },
#endif /* USE_CRYPT */
	{ (char *) NULL, 0, (char *) NULL, NULL }
};

#ifdef HAVE_LIBREADLINE
char *
commands_generator (char *text, int state)
{
  static int list_index, len;
  char *name;
  if (!state)
    {
      list_index = 0;
      len = ntripcaster_strlen (text);
    }
  while ((name = commands[list_index].name))
    {
      list_index++;
      if (ntripcaster_strncmp (name, text, len) == 0)
        return (nstrdup (name));
    }
  /* If no names matched, then return NULL. */
  return ((char *)NULL);
}

char *
settings_generator (char *text, int state)
{
  static int list_index, len;
  char *name;
  if (!state)
    {
      list_index = 0;
      len = ntripcaster_strlen (text);
    }
  while ((name = admin_settings[list_index].name))
    {
      list_index++;
      if (ntripcaster_strncmp (name, text, len) == 0)
        return (nstrdup (name));
    }
  /* If no names matched, then return NULL. */
  return ((char *)NULL);
}
#endif

/* make sure the array of settings for the config file points to the right data */
void 
setup_config_file_settings()
{
	int x = 0;
	/* Don't change the order of these */
	configfile_settings[x++].setting = &info.encoder_pass;
//	configfile_settings[x++].setting = &info.client_pass;
	configfile_settings[x++].setting = &info.remote_admin_pass;
	configfile_settings[x++].setting = &info.oper_pass;
//	configfile_settings[x++].setting = &info.touch_freq;
	configfile_settings[x++].setting = &info.max_clients;
	configfile_settings[x++].setting = &info.max_ip_connections;
	configfile_settings[x++].setting = &info.max_sources;
	configfile_settings[x++].setting = &info.max_admins;
	configfile_settings[x++].setting = &info.logfilename;
	configfile_settings[x++].setting = &info.watchfile;
	configfile_settings[x++].setting = &info.pidfile;
	configfile_settings[x++].setting = &info.myhostname;
	configfile_settings[x++].setting = &info.server_name;
	configfile_settings[x++].setting = &info.console_mode;
	configfile_settings[x++].setting = &info.client_timeout;
//	configfile_settings[x++].setting = &info.statsfilename;
//	configfile_settings[x++].setting = &info.statshtmlfilename;
//	configfile_settings[x++].setting = &info.statstime;
	configfile_settings[x++].setting = &info.max_clients_per_source;
	configfile_settings[x++].setting = &info.reverse_lookups;
	configfile_settings[x++].setting = &info.location;
	configfile_settings[x++].setting = &info.rp_email;
	configfile_settings[x++].setting = &info.transparent_proxy;
	configfile_settings[x++].setting = &info.policy;
	configfile_settings[x++].setting = &info.throttle;
	configfile_settings[x++].setting = &info.kick_relays;
	configfile_settings[x++].setting = &info.kick_clients;
	configfile_settings[x++].setting = &info.statustime;
//	configfile_settings[x++].setting = &info.staticdir;
	configfile_settings[x++].setting = &info.logfiledebuglevel;
	configfile_settings[x++].setting = &info.consoledebuglevel;
	configfile_settings[x++].setting = &info.url;
	configfile_settings[x++].setting = &info.use_meta_data;
	configfile_settings[x++].setting = &info.streamurllock;
	configfile_settings[x++].setting = &info.streamurl;
	configfile_settings[x++].setting = &info.streamtitletemplate;
	configfile_settings[x++].setting = &info.nametemplate;
	configfile_settings[x++].setting = &info.descriptiontemplate;
	configfile_settings[x++].setting = &info.accessfilename;
	configfile_settings[x++].setting = &info.usagefilename;
	configfile_settings[x++].setting = &info.default_sourceopts;
	configfile_settings[x++].setting = &info.mount_fallback;
	configfile_settings[x++].setting = &info.force_servername;
	configfile_settings[x++].setting = &info.logdir;
	configfile_settings[x++].setting = &info.templatedir;
	configfile_settings[x++].setting = &info.resolv_type;
	configfile_settings[x++].setting = &info.allow_http_admin;
	configfile_settings[x++].setting = &info.relay_reconnect_tries;
	configfile_settings[x++].setting = &info.relay_reconnect_time;
	configfile_settings[x++].setting = &info.sleep_ratio;
	configfile_settings[x++].setting = &info.ntripinfourl;
	configfile_settings[x++].setting = &info.name;
	configfile_settings[x++].setting = &info.operator;
	configfile_settings[x++].setting = &info.operatorurl;
	configfile_settings[x++].setting = &info.sourcetablefile;
#ifdef HAVE_LIBLDAP
	configfile_settings[x++].setting = &info.ldap_server;
	configfile_settings[x++].setting = &info.ldap_uid_prefix;
	configfile_settings[x++].setting = &info.ldap_people_context;
#endif /* HAVE_LIBLDAP */
#ifdef USE_CRYPT
	configfile_settings[x++].setting = &info.encrypt_passwords;
#endif /* USE_CRYPT */
}

set_element *
find_set_element (char *name, set_element *el)
{
  register int i;
  for (i = 0; el[i].name; i++)
    if (ntripcaster_strcmp (name, el[i].name) == 0)
      return (&el[i]);
  return ((set_element *)NULL);
}

const comp_element *
find_comp_element (const char *name, const comp_element *el)
{
  register int i;
  for (i = 0; el[i].name; i++)
    if (ntripcaster_strcmp (name, el[i].name) == 0)
      return (&el[i]);
  return ((comp_element *)NULL);
}

int 
parse_default_config_file()
{
	char file[BUFSIZE];

	if (get_ntripcaster_file(info.configfile, conf_file_e, R_OK, file) != NULL) {
		parse_config_file(file);
//		nfree(file);
		return 1;
	}
	
	parse_config_file(info.configfile);
	return 1;
}
		
/* Parse the configfile pointed to by file.
   If it doesn't exist, no matter, just go on */
int 
parse_config_file(char *file)
{
	set_element *se;
	char word[BUFSIZE], line[BUFSIZE];
	int cf;
	int i;
	int lineno = 0;

	xa_debug (1, "DEBUG: Parsing configuration file %s", file ? file : "(null)");

	if (!file)
		return 0;

	if ((cf = open_for_reading (file)) == -1)
	{
		write_log(LOG_DEFAULT, "No configfile found, using defaults.");
		return 1;
	}

	/* Clean up aliases, directories, and acl lists  and ports*/

	if (!info.client_acl || !info.source_acl || !info.admin_acl || !info.all_acl) 
		write_log (LOG_DEFAULT, "WARNING: parse_config_file(): NULL acl tree pointers, this is weird!");
	else
		free_acl_lists ();

	if (!info.client_acl || !info.source_acl || !info.admin_acl || !info.all_acl) 
		write_log (LOG_DEFAULT, "WARNING: parse_config_file(after): NULL acl tree pointers, this is weird!");

//	free_directory_servers (&info.d_servers);
	free_aliases ();

	for (i = 0; i < MAXLISTEN; i++) {
		info.port[i] = 0;
	}

	while (fd_read_line (cf, line, BUFSIZE) > 0)
	{
		lineno++;
		if ((ntripcaster_strlen (line) < 2) || (line[0] == '#') || (line[0] == ' '))
			continue;
		
		if (line[ntripcaster_strlen(line) - 1] == '\n')
			line[ntripcaster_strlen(line) - 1] = '\0';
		if (splitc(word, line, ' ') == NULL) {
			write_log(LOG_DEFAULT, "ERROR: No argument given to setting %s on line %d", 
				   line, lineno);
			continue; 
		}
		
		se = find_set_element(word, configfile_settings);
		
		/* K, even though this looks wrong, it's not :) */
		
/*
		if ((ntripcaster_strncmp(word, "icydir", 6) == 0) || (ntripcaster_strncmp (word, "dir", 3) == 0))
		{
			char host[BUFSIZE], port[BUFSIZE];
			directory_server_t *dir = create_directory();
			
			dir->type = (ntripcaster_strncmp (word, "icydir", 6) == 0) ? icy_e : xaudiocast_e;
			dir->counter = 0;
			dir->id = -1;

			if (strchr (line, '/') != NULL && strchr (line, ':') == NULL)
			{
				write_log (LOG_DEFAULT, "ERROR: Path but no port specified in icy directory on line %d", lineno);
				continue;
			}

			if (splitc (host, line, ':') == NULL) // No port, no path
			{
				dir->port = 80;
				dir->path = nstrdup (DEFAULT_YP_TOUCH);
				strncpy(host, line, BUFSIZE);
			} else if (splitc (port, line, '/') == NULL) // port, but no path
			{
				dir->port = atoi (line);
				dir->path = nstrdup ("DEFAULT_YP_TOUCH");
			} else {
				dir->port = atoi (port);
				dir->path = nstrdup (line);
			}

			dir->host = nstrdup (host);

			thread_mutex_lock (&info.directory_mutex);
			if (avl_find (info.d_servers, dir) == NULL)
				avl_insert (info.d_servers, dir);
			thread_mutex_unlock (&info.directory_mutex);

			if (dir->type == icy_e)
				write_log (LOG_DEFAULT, "Using icy directory server [%s:%d]", dir->host, dir->port);
			else
				write_log (LOG_DEFAULT, "Using xaudiocast directory server [%s:%d/%s]", dir->host, dir->port, dir->path);
			continue;

		} else
*/
		if (se) {
			if (se->type == integer_e) {
				*(int *)(se->setting) = atoi(line);
			} else if (se->type == real_e) {
				*(double *)(se->setting) = atof(line);
			} else {
				/* string, might have to free it first */
				if (*(char **)(se->setting) != NULL)
					nfree(*(char **)(se->setting));
				*((char **)(se->setting)) = clean_string(nstrdup(line));
			}
			continue;
		} else if (ntripcaster_strncmp(word, "alias", 5) == 0)
		{
			char name[BUFSIZE];
		// added. ajd
			char real[BUFSIZE];
			char *idenc = NULL;

			if (splitc(name, line, ' ') == NULL) {
				write_log(LOG_DEFAULT, "ERROR: Missing argument for alias on line %d", lineno);
			} else {
				ntrip_request_t nameline, realline;

				zero_request(&nameline);
				zero_request(&realline);
// added. ajd
				if (splitc(real, line, ' ') != NULL) {
					idenc = util_base64_encode(clean_string(line));
				} else strncpy(real, line, BUFSIZE);

				generate_request(name, &nameline);
				generate_request(real, &realline);

				if (!nameline.path[0] || !realline.path[0]) {
					write_log(LOG_DEFAULT, "ERROR: Invalid syntax for alias on line %d", lineno);
					if (idenc != NULL) nfree(idenc);
					continue;
				}

				add_alias(&nameline, &realline, idenc, NULL);

				if (idenc != NULL) nfree(idenc);
			}
			continue;
		} else if (ntripcaster_strncmp(word, "relay", 5) == 0) {

			char command[BUFSIZE];

			if (line[0] != 'p')
				 relay_add_pull_to_list (line);
			else {
				if (splitc(command, line, ' ') != NULL)
					if (ntripcaster_strncmp(command, "pull", 4) == 0) {
						relay_add_pull_to_list (line);
					}
			}

			continue;
		} else if (ntripcaster_strncmp(word, "port", 4) == 0) {
			int p = atoi(line);
			for (i = 0; i < MAXLISTEN; i++)
				if (info.port[i] == 0) break;
			if (i < MAXLISTEN)
				info.port[i] = p;
			continue;
		} else if (ntripcaster_strncmp(word, "nontripsource", 13) == 0) { // nontrip. ajd
			add_nontrip_source(line);
			continue;
		} else if (ntripcaster_strncmp(word, "include", 7) == 0) {
			parse_config_file(line);
			continue;
		} else if (ntripcaster_strncmp(word, "allow", 5) == 0) {
			char type[BUFSIZE];
			restrict_t *res;
			if (splitc(type, line, ' ') == NULL) {
				write_log(LOG_DEFAULT, "ERROR: Invalid syntax for acl allow on line %d", lineno);
				continue;
			}
			res = add_restrict(get_acl_tree(type), line, allow);
			if (!res)
				write_log(LOG_DEFAULT, "ERROR: Acl addition for [%s] failed", line);
			continue;
		} else if (ntripcaster_strncmp(word, "deny", 4) == 0) {
			char type[BUFSIZE];
			restrict_t *res;
			if (splitc(type, line, ' ') == NULL) {
				write_log(LOG_DEFAULT, "ERROR: Invalid syntax for acl deny on line %d", lineno);
				continue;
			}
			res = add_restrict(get_acl_tree(type), line, deny);
			if (!res)
				write_log(LOG_DEFAULT, "ERROR: Acl addition for [%s] failed", line);
			continue;
		}

		write_log(LOG_DEFAULT, "Unknown setting %s on line %d", word, lineno);
	}
	fd_close(cf);
	return 0;
}

/* Top level remote admin console command parser */
void 
handle_admin_command (connection_t *con, char *command, int command_len)
{
  int flag = 0;
  register int i;
  const comp_element *com;
  com_request_t req;
  char comstr[BUFSIZE], widstr[BUFSIZE];
  admin_t *adm = con->food.admin;
  
  widstr[0] = '\0';

  /* Initialize the request */
  req.con = con;
  req.wid = -1;
  req.arg = NULL;

  if (command_len < 2)
  {
	  write_admin_prompt (con);
	  return;
  }
  
  /* sanity check */
  for (i = 0; i <= command_len; i++)
  {
	  if (command[i] == '\n' || command[i] == '\0')
	  {
		  flag = 1;
		  command[i] = '\0';
		  break;
	  }
  }

  if (command[strlen (command) - 1] == ' ')
	  command[strlen (command) - 1] = '\0';
  
  if (!flag)
  {
	  write_admin_prompt (con);
	  return;
  }

  splitc (comstr, command, ' ');
  
  if (!comstr[0] || !command[0] || command[0] == ' ')
  {
	  if (command[0] && command[0] != ' ')
		  strncpy(comstr, command, BUFSIZE);
	  req.arg = NULL;
  } else
	  req.arg = command;
  
  splitc (widstr, comstr, '.');
  
  if (is_number(widstr))
  {
	  req.wid = atoi(widstr);
  } 

  /* Functions are now responsible for their own logging */
  com = find_comp_element (comstr, commands);

  if (!com)
  {
	  com_help (&req);
	  write_admin_prompt (con);
	  return;
  }
  if (com->oper && (!adm->oper))
	  admin_write_line (&req, -1, "Only operators can use the %s command", com->name);
  else
  {
	  log_command (comstr, &req);
	  ((*(com->func)) (&req));
  }
  
  write_admin_prompt (con);
}

void
log_command (const char *command, const com_request_t *req)
{
	if (req->arg && strcmp (command, "oper") != 0)
		write_log_not_me (LOG_DEFAULT, req->con, "Admin [%s] said: %s %s", con_host (req->con), command, req->arg);
	else
		write_log_not_me (LOG_DEFAULT, req->con, "Admin [%s] said: %s", con_host (req->con), command);
}

char *
com_arg (const com_request_t *req)
{
	if (req)
		return req->arg;
	return NULL;
}

int
com_admins (com_request_t *req)
{
	char *arg = com_arg (req);
	connection_t *admcon;
	avl_traverser trav = {0};
	char pattern[BUFSIZE], buf[BUFSIZE];
	int listed = 0;
	time_t t = get_time (); /* Might save a few calls to time() */

/* added. ajd. (remove lines in threads.c when removing this!)
	thread_mutex_lock(&info.ajd_mutex);

	admin_write_line(req, ADMIN_SHOW_LINKS, "Sourcelock: %s", &info.ajd_mutex_lock);
 	admin_write_line(req, ADMIN_SHOW_LINKS, "Src-unlock: %s", &info.ajd_mutex_unlock);

	thread_mutex_unlock(&info.ajd_mutex);
*/


	pattern[0] = '\0';
	
	if (arg && arg[0])
	{
		strncpy(pattern, arg, BUFSIZE);
		item_write_formatted_line (req, ADMIN_SHOW_ADMIN_START, list_start, 1, 
					   item_create ("Listing admins matching [%s]", "%s", arg));
	} else {
		item_write_formatted_line (req, ADMIN_SHOW_ADMIN_START, list_start, 1,
					   item_create ("Listing admins", "%s", NULL));
	}

	item_write_formatted_line (req, ADMIN_SHOW_ADMIN_CAPTIONS, list_caption, 4, 
				   item_create ("Connection id", "%s", NULL),
				   item_create ("Host", "%s", NULL),
				   item_create ("Connected for", "%s", NULL),
				   item_create ("Commands issued", "%s", NULL));

	thread_mutex_lock (&info.admin_mutex);
	while ((admcon = avl_traverse (info.admins, &trav)))
	{
		if (pattern[0])
			if (!hostmatch (admcon, pattern))
				continue;
		listed++;
		item_write_formatted_line (req, ADMIN_SHOW_ADMIN_ENTRY, list_item, 4,
					    item_create ("Id", "%d", &admcon->id),
					    item_create ("Host", "%s", con_host (admcon)),
					    item_create ("Connected for", "%s", nntripcaster_time (t - admcon->connect_time, buf)),
					    item_create ("Commands issued", "%d", &admcon->food.admin->commands));
		
/*		admin_write (req, ADMIN_SHOW_ADMIN_ENTRY, "[Id: %d] [Host: %s] [Connected for: %s] [Commands issued: %d]",
		admcon->id, con_host (admcon), nntripcaster_time (t - admcon->connect_time, buf), admcon->food.admin->commands); 
		admin_write_line (req, -1, " [Flags: %s%s]", admin2string (admcon->food.admin, buf), 
		(thread_equal (thread_self(), admcon->food.admin->thread)) ? "M":""); */
	}
	
	thread_mutex_unlock (&info.admin_mutex);
	
	item_write_formatted_line (req, ADMIN_SHOW_ADMIN_END, list_end, 1, item_create ("End of admin listing", "%d", &listed));
	return 1;
}

void
show_settings (com_request_t *req)
{
	register int i;
	set_element *se = admin_settings;

	item_write_formatted_line (req, ADMIN_SHOW_SETTINGS_START, list_start, 1,
				    item_create ("Current settings", "%s", NULL));
	
	item_write_formatted_line (req, ADMIN_SHOW_SETTINGS_CAPTION, list_caption, 2,
				   item_create ("Setting", "%s", NULL),
				   item_create ("Value", "%s", NULL));

	
	for (i = 0; se[i].name; i++)
	{
		if (se[i].type == integer_e)
			item_write_formatted_line (req, ADMIN_SHOW_SETTINGS_ENTRY_INT, list_set_item, 2,
						   item_create ("", "%25s:\t", se[i].name),
						   item_create ("", "%d", se[i].setting));
		else if (se[i].type == real_e)
			item_write_formatted_line (req, ADMIN_SHOW_SETTINGS_ENTRY_REAL, list_set_item, 2,
						   item_create ("", "%25s:\t", se[i].name),
						   item_create ("", "%f", se[i].setting));
		else {
			item_write_formatted_line (req, ADMIN_SHOW_SETTINGS_ENTRY_STRING, list_set_item, 2,
						   item_create ("", "%25s:\t", se[i].name),
						   item_create ("", "%s", *(char **)(se[i].setting)));
}
						   

	}

	item_write_formatted_line (req, ADMIN_SHOW_SETTINGS_END, 1, list_end, 
				   item_create ("End of settings", "%s", NULL));
}

int
com_help (com_request_t *req)
{
	register int i;
	const comp_element *ce;
	set_element *se;
	char *arg = com_arg (req);

	if (!arg)
	{
		ce = commands;
		admin_write_line (req, ADMIN_SHOW_COMMANDS_START,  "Available commands:");
		for (i = 0; ce[i].name; i++)
			if (ce[i].show == 1) // added. ajd
				admin_write_line (req, ADMIN_SHOW_COMMANDS_ENTRY, "%20s\t: %s", ce[i].name, ce[i].doc);
		admin_write_line (req, ADMIN_SHOW_COMMANDS_END, "For slighly more detailed help on each command, try 'help <command>'");
		return 1;
	}
	
	if (ntripcaster_strncmp (arg, "setti", 5) == 0)
	{
		se = admin_settings;
		admin_write_line (req, ADMIN_SHOW_SETTINGS_START, "Available settings:");
		for (i = 0; se[i].name; i++)
			admin_write_line (req, ADMIN_SHOW_SETTINGS_ENTRY,  "%25s\t: %s", se[i].name, se[i].doc);
		admin_write_line (req, ADMIN_SHOW_SETTINGS_END, "End of settings list (%d listed)", i);
		return 1;
	}
	
	ce = find_comp_element (arg, commands);
	
	if (!ce)
	{
		admin_write_line (req, ADMIN_SHOW_COMMANDS_INVALID, "No such command [%s]", arg);
		return 1;
	}
	
	admin_write_line (req, ADMIN_SHOW_COMMANDS_SPECIFIC, "Command: [%s]\tDescription: [%s]%s", ce->name, 
			  ce->doc, ce->doclong);
	return 1;
}

int
com_set (com_request_t *req)
{
  char argument[BUFSIZE];
  set_element *s;
  char *arg = com_arg (req);

  argument[0] = '\0';

  if (!arg || !arg[0])
  {
	  show_settings (req); /* No setting, show all settings */
	  return 1;
  }

  if (!arg[0] || arg[0] == ' ' || arg[0] == '\0' || splitc (argument, arg, ' ') == NULL || argument[0] == ' '
      || argument[0] == '\0' || arg[0] == ' ' || arg[0] == '\0')
	  /* No argument, show setting */
  {
	  s = find_set_element (arg, admin_settings);
	  if (!s)
	  {
		  admin_write_line (req, ADMIN_SHOW_SETTINGS_INVALID, "No such setting [%s]", arg);
		  return 1;
	  }
	  if (s->type == integer_e)
		  admin_write_line (req, ADMIN_SHOW_SETTINGS_ENTRY_INT, "%s: %d", s->name, *(int *)(s->setting));
	  else if (s->type == real_e)
		  admin_write_line (req, ADMIN_SHOW_SETTINGS_ENTRY_REAL, "%s: %f", s->name, *(double *)(s->setting));
	  else 
		  admin_write_line (req, ADMIN_SHOW_SETTINGS_ENTRY_STRING, "%s: %s", s->name, *(char **)(s->setting));
	  return 1;
  }
  
  s = find_set_element (argument, admin_settings);
  if (!s)
  {
	  admin_write_line (req, ADMIN_SHOW_SETTINGS_INVALID,  "No such setting [%s]", argument);
	  return 1;
  }
  
  /* Some variables take special care */
  if (is_special_variable (argument))
  {
	  change_special_variable (req, s, argument, arg);
	  return 1;
  }

  change_variable (req, s, argument, arg);
  
  
  return 1;
}

int
com_tail (com_request_t *req)
{
	admin_write_line (req, ADMIN_SHOW_TAILING_ON, "Now tailing logfile");
	req->con->food.admin->tailing = 1;
	return 1;
}

int
com_untail (com_request_t *req)
{
	admin_write_line (req, ADMIN_SHOW_TAILING_OFF, "No longer tailing logfile");
	req->con->food.admin->tailing = 0;
	return 1;
}

conopt_t sourceopts[] = 
{
	{ 'i', "Show id", SOURCE_SHOW_ID }, 
	{ 's', "Show socket", SOURCE_SHOW_SOCKET }, 
	{ 't', "Show time of connect", SOURCE_SHOW_CTIME }, 
	{ 'p', "Show ip", SOURCE_SHOW_IP },
	{ 'h', "Show host", SOURCE_SHOW_HOST },

	{ 'x', "Show source agent", SOURCE_SHOW_AGENT }, // added. ajd

	{ 'e', "Show all headers", SOURCE_SHOW_HEADERS },
	{ 'z', "Show state (connected/pending)", SOURCE_SHOW_STATE }, // changed. was 'c'. ajd
	{ 'y', "Show source type", SOURCE_SHOW_TYPE },
	{ 'o', "Show protocol (xaudiocast/icy)", SOURCE_SHOW_PROTO },
	{ 'c', "Show number of clients", SOURCE_SHOW_CLIENTS },
//	{ 'd', "Show dumpfile", SOURCE_SHOW_DUMPFILE },
	{ 'r', "Show mount priority", SOURCE_SHOW_PRIO },
	{ 'l', "Show song title", SOURCE_SHOW_SONG_TITLE },
	{ 'u', "Show song url", SOURCE_SHOW_SONG_URL },
	{ 'm', "Show stream msg", SOURCE_SHOW_STREAM_MSG },
	{ 'n', "Show song length", SOURCE_SHOW_SONG_LENGTH },
	{ 'a', "Show source name", SOURCE_SHOW_NAME },
	{ 'g', "Show source genre", SOURCE_SHOW_GENRE },
	{ 'b', "Show source bitrate", SOURCE_SHOW_BITRATE },
	{ 'U', "Show source URL", SOURCE_SHOW_URL },
	{ 'M', "Show source mountpoint", SOURCE_SHOW_MOUNT },
	{ 'D', "Show source description", SOURCE_SHOW_DESC},
	{ 'R', "Show bytes read", SOURCE_SHOW_READ},
	{ 'W', "Show bytes written", SOURCE_SHOW_WRITTEN },
	{ 'C', "Show total connects", SOURCE_SHOW_CONNECTS },
	{ 'T', "Show connected time", SOURCE_SHOW_TIME },
	{ '\0', NULL, -1 }
};

int
com_sources (com_request_t *req)
{
	int opt[SOURCE_OPTS], listed = 0;
	char pattern[BUFSIZE], line[BUFSIZE], *arg = com_arg (req);
	connection_t *source;
	avl_traverser trav = {0};

	zero_opts (opt, SOURCE_OPTS);
	
	pattern[0] = '\0';
	
	if (arg && arg[0])
	{
		if (arg[0] == '-')
			set_opts (opt, arg, sourceopts, SOURCE_OPTS);
		else {
			if (splitc (pattern, arg, ' ') == NULL)
				strncpy(pattern, arg, BUFSIZE);
			set_opts (opt, arg, sourceopts, SOURCE_OPTS);
		}
	} else {
		set_opts (opt, info.default_sourceopts, sourceopts, SOURCE_OPTS);
	}
	
	if (pattern[0])
		item_write_formatted_line (req, ADMIN_SHOW_SOURCE_START, list_start, 1, 
					   item_create ("Listing sources matching [%s]", "%s", pattern));
	else
		item_write_formatted_line (req, ADMIN_SHOW_SOURCE_START, list_start, 1,
					   item_create ("Listing sources", "%s", NULL));
	
	item_write_formatted_line (req, ADMIN_SHOW_SOURCE_CAPTIONS, list_caption, 11,
				   item_create ("Mountpoint", "%s", NULL),
				   item_create ("Id", "%s", NULL),
				   item_create ("Host", "%s", NULL),
				   item_create ("Source Agent", "%s", NULL), // added. ajd
				   item_create ("Time of connect", "%s", NULL),
				   item_create ("IP", "%s", NULL),
				   item_create ("Clients", "%s", NULL),
				   item_create ("KBytes read", "%s", NULL),
				   item_create ("KBytes written", "%s", NULL),
				   item_create ("Client connections", "%s", NULL),
				   item_create ("Connected for", "%s", NULL));

	thread_mutex_lock(&info.source_mutex);

	while ((source = avl_traverse (info.sources, &trav)))
	{
		if (pattern[0])
			if (!hostmatch (source, pattern))
				continue;
		
		if (admin_scheme (req) == default_scheme_e) {
			build_source_con_line_with_opts (source, line, opt, BUFSIZE);

			admin_write_line (req, ADMIN_SHOW_SOURCE_ENTRY, "%s", line);
		} else {
			char ct[100];
			char buf[BUFSIZE];

			buf[0] = '\0';
			
			get_string_time (ct, source->connect_time, REGULAR_DATETIME);

			item_write_formatted_line (req, ADMIN_SHOW_SOURCE_ENTRY, list_item, 11,
						   item_create ("Mountpoint", "%s", nullcheck_string (source->food.source->audiocast.mount)),
						   item_create ("Id", "%d", &source->id),
						   item_create ("Host", "%s", nullcheck_string ((source->hostname ? source->hostname : source->host))),
						item_create ("Source Agent", "%s", get_source_agent(source)), // added. ajd
						   item_create ("Time of connect", "%s", ct),
						   item_create ("IP", "%s", nullcheck_string (source->host)),
						   item_create ("Clients", "%d", &source->food.source->num_clients),

/* read/write_megs durch read/write_kilos ersetzt */
						   item_create ("KBytes read", "%d", &source->food.source->stats.read_kilos),
						   item_create ("KBytes written", "%d", &source->food.source->stats.write_kilos),

						   item_create ("Client connections", "%d", &source->food.source->stats.client_connections),
						   item_create ("Connected for", "%s", nntripcaster_time (get_time () - source->connect_time, buf)));
				
		}
		
		
		listed++;
	}
	
	thread_mutex_unlock(&info.source_mutex);	
	
	item_write_formatted_line (req, ADMIN_SHOW_SOURCE_END, list_end, 1, item_create ("End of source listing", "%d", &listed));

	return 1;
	}



/* to show sourcetable through admin web interface. ajd */
int com_sourcetable (com_request_t *req) {
	avl_traverser trav = {0};
	sourcetable_entry_t *se;

	admin_write_line (req, ADMIN_SHOW_SOURCETABLE, "Sourcetable");

	thread_mutex_lock(&info.sourcetable_mutex);

	while ((se = avl_traverse (info.sourcetable.tree, &trav))) {
		if (se->show == 1)
			admin_write_line (req, ADMIN_SHOW_SOURCETABLE_LINE, se->line);
		else
			admin_write_line (req, ADMIN_SHOW_SOURCETABLE_RED_LINE, se->line);
	}

	thread_mutex_unlock(&info.sourcetable_mutex);

	admin_write_line (req, ADMIN_SHOW_SOURCETABLE_END, "End of Sourcetable");

        return 1;
}

int
com_users(com_request_t *req) {


	return 1;
}

int
com_listeners (com_request_t *req)
{
	char pattern[BUFSIZE], buf[BUFSIZE], *arg = com_arg (req);
	connection_t *clicon;
	avl_traverser trav = {0};
	int listed = 0;
	time_t t = get_time ();
/* Added code to show the user's names. ajd */
	ntripcaster_user_t *user;

	pattern[0] = '\0';

	if (arg && arg[0])
	{
		strncpy(pattern, arg, BUFSIZE);
		admin_write_line (req, ADMIN_SHOW_LISTENERS_START, "Listing listeners matching [%s]:", pattern);
	} else {
		admin_write_line (req, ADMIN_SHOW_LISTENERS_START, "Listing listeners");
	}
	
	thread_mutex_lock (&info.client_mutex);

	while ((clicon = avl_traverse (info.clients, &trav)))
	{
		if (pattern[0]) if (!hostmatch (clicon, pattern)) continue;
		listed++;

/* Added code to show the user's names */
		user = con_get_user(clicon);

		admin_write_line (req, ADMIN_SHOW_LISTENERS_ENTRY, "[Host: %s] [IP: %s] [User: %s] [Mountpoint %s] [Id: %ld] [Connected for: %s] [Bytes written: %ld] [Errors: %d] [User agent: %s] [Type: %s]", con_host (clicon), nullcheck_string (clicon->host), (user != NULL)?nullcheck_string (user->name):"(null)", clicon->food.client->source->audiocast.mount, clicon->id, nntripcaster_time (t - clicon->connect_time, buf), clicon->food.client->write_bytes, client_errors (clicon->food.client), get_user_agent (clicon), client_type (clicon));

/* Added code to show the user's names */
		if (user != NULL) {
			nfree(user->name);
			nfree(user->pass);
			nfree(user);
		}
	}

	thread_mutex_unlock (&info.client_mutex);
	
	admin_write_line (req, ADMIN_SHOW_LISTENERS_END, "End of listener listing");
	return 1;
}

int
com_rehash (com_request_t *req) {
	char oldlogfile[BUFSIZE];
	char oldaccessfile[BUFSIZE];
	char oldusagefile[BUFSIZE];
	char *arg = com_arg (req);

	strncpy(oldlogfile, info.logfilename, BUFSIZE);
	strncpy(oldaccessfile, info.accessfilename, BUFSIZE);
	strncpy(oldusagefile, info.usagefilename, BUFSIZE);

	admin_write_line (req, ADMIN_SHOW_REHASH, "Reading configfile %s", arg ? arg : info.configfile);
	write_log_not_me (LOG_DEFAULT, req->con, "Reading configfile %s", arg ? arg : info.configfile);

	if (arg && arg[0])
		parse_config_file (arg);
	else
		parse_default_config_file ();
	
	if ((strncmp(oldlogfile, info.logfilename, BUFSIZE) != 0) || (strncmp(oldusagefile, info.usagefilename, BUFSIZE) != 0) || (strncmp(oldaccessfile, info.accessfilename, BUFSIZE) != 0)) open_log_files ();

/* Updating sourcetable. ajd */
	admin_write_line (req, ADMIN_SHOW_REHASH, "Rehashing sourcetable");
	write_log_not_me (LOG_DEFAULT, req->con, "Rehashing sourcetable");
//	update_sourcetable();
	read_sourcetable();

/* Rehash authentication scheme. ajd */
	admin_write_line (req, ADMIN_SHOW_REHASH, "Rehashing authentication scheme");
	write_log_not_me (LOG_DEFAULT, req->con, "Rehashing authentication scheme");
	rehash_authentication_scheme();

	return 1;
}

int
com_uptime (com_request_t *req)
{
	char timebuf[BUFSIZE];
#ifdef CHANGE4
	long filetime;
	filetime = read_starttime();
	if (filetime > 0)
		nntripcaster_time (get_time () - filetime, timebuf);
	else
		nntripcaster_time (get_time () - info.server_start_time, timebuf);

	admin_write_line (req, ADMIN_SHOW_UPTIME, "NtripCaster %s server running on %s default port %d, uptime: %s", info.version, nullcheck_string (info.server_name), info.port[0], timebuf);
#else
	int i = get_time () - info.server_start_time;
	admin_write_line (req, ADMIN_SHOW_UPTIME, "NtripCaster %s server running on %s default port %d, uptime: %s", info.version, nullcheck_string (info.server_name), info.port[0], nntripcaster_time (i, timebuf));
#endif
	return 1;
}

int
com_tell (com_request_t *req)
{
	char buf[BUFSIZE];
	avl_traverser trav = {0};
	connection_t *con2;
	char *arg = com_arg (req);

	if (!arg || !arg[0])
	{
		admin_write_line (req, ADMIN_SHOW_TELL_INVALID, "Don't be so modest, spit it out :)");
		return 1;
	}
	
	snprintf(buf, BUFSIZE, "Admin %ld said: [%s]\r\n-> ", req->con->id, arg);
	
	thread_mutex_lock (&info.admin_mutex);
	
	while ((con2 = avl_traverse (info.admins, &trav)))
	{
		if (con2->id != req->con->id)
			sock_write_line (con2->sock, "M%d %s", ADMIN_SHOW_TELL, buf);
	}
	
	thread_mutex_unlock(&info.admin_mutex);
	return 1;
}

/* If things goes as planned, this should do no traversing of
   avl trees, just printing stuff put in the statistics struct by
   the stats thread */
int
com_stats (com_request_t *req)
{
	statistics_t stat;
	time_t t;
	unsigned long int extra_clientconnections = 0;
	unsigned long int extra_sourceconnections = 0;
	char *arg = com_arg (req);
	char timebuf[BUFSIZE];
#ifdef CHANGE4
	time_t filetime;
	char realstarttime[100];
#endif
	
	admin_write (req, ADMIN_SHOW_STATS_DAILY, "<h2>Statistics</h2>");
	
	zero_stats (&stat);
	
	if (arg && arg[0])
	{
		if (ntripcaster_strcmp (arg, "daily") == 0)
		{
			statistics_t cs;
			get_current_stats (&cs);
			add_stats (&stat, &cs, 0);
			
			get_hourly_stats (&cs);
			add_stats (&stat, &cs, 0);
			
			get_daily_stats (&cs);
			add_stats (&stat, &cs, 1000);
			
			extra_sourceconnections = info.num_sources;
			extra_clientconnections = info.num_clients;
			admin_write_line (req, ADMIN_SHOW_STATS_DAILY, "Following info is for the last day:");
		} else if (ntripcaster_strcmp (arg, "hourly") == 0)
		{
			statistics_t cs;
			get_current_stats (&cs);
			add_stats (&stat, &cs, 0);
			
			get_hourly_stats (&cs);
			add_stats (&stat, &cs, 0);
			extra_sourceconnections = info.num_sources;
			extra_clientconnections = info.num_clients;
			admin_write_line (req, ADMIN_SHOW_STATS_HOURLY,  "Following info is for the last hour:");
/*		} else if (ntripcaster_strcmp (arg, "clear") == 0)
		{
			admin_write_line (req, ADMIN_SHOW_STATS_CLEAR,  "Clearing stats...");
			write_log (LOG_DEFAULT, "Clearing stats...");
			thread_mutex_lock (&info.misc_mutex);
			zero_stats (&info.daily_stats);
			zero_stats (&info.hourly_stats);
			zero_stats (&info.total_stats);
			thread_mutex_unlock (&info.misc_mutex);
			// clear source stats - do we need a mutex? Yeah, we do :)
			thread_mutex_lock (&info.double_mutex);
			thread_mutex_lock (&info.source_mutex);
			avl_walk (info.sources, clear_source_stats, &info);
			thread_mutex_unlock (&info.source_mutex);
			thread_mutex_unlock (&info.double_mutex);
			// clear admin stats
			thread_mutex_lock (&info.admin_mutex);
			avl_walk (info.admins, clear_admin_stats, &info);
			thread_mutex_unlock (&info.admin_mutex);
*/
		} else {
			admin_write_line (req, ADMIN_SHOW_STATS_INVALID, "Unknown stats type %s", arg);
			return 1;
		}
	} else {
		admin_write_line (req, ADMIN_SHOW_STATS_TOTAL, "Following info is server total:");
		get_running_stats (&stat);
	}
	
	t = get_time ();
#ifdef CHANGE4
	filetime = read_starttime();
	
	if (filetime > 0)
		nntripcaster_time (t - filetime, timebuf);
	else
		nntripcaster_time (t - info.server_start_time, timebuf);
		
	get_string_time (realstarttime, info.server_start_time, REGULAR_DATETIME);

	admin_write_line (req, ADMIN_SHOW_UPTIME, "NtripCaster %s server running on %s default port %d, uptime: %s", info.version, nullcheck_string (info.server_name), info.port[0], timebuf);
	
	admin_write_line (req, ADMIN_SHOW_STATS_NUMADMINS, "Admins (%d):", info.num_admins);
	admin_write_line (req, ADMIN_SHOW_STATS_NUMSOURCES, "Sources (%d):", info.num_sources);
	admin_write_line (req, ADMIN_SHOW_STATS_NUMLISTENERS, "Listeners (%d):", info.num_clients);
	
	admin_write_line (req, ADMIN_SHOW_STATS_MISC, "Displaying server statistics since last resync at: %s", realstarttime); // added realstarttime. ajd
	admin_write_line (req, ADMIN_SHOW_STATS_READ, "Total KBytes read: %lu", stat.read_kilos);
	admin_write_line (req, ADMIN_SHOW_STATS_WRITTEN, "Total KBytes written: %lu", stat.write_kilos);
	admin_write_line (req, ADMIN_SHOW_STATS_SOURCE_CONNECTS, "Number of source connects: %lu", stat.source_connections);
	admin_write_line (req, ADMIN_SHOW_STATS_CLIENT_CONNECTS, "Number of client connects: %lu", stat.client_connections);
	
	if (stat.client_connections > 0)
	{
		admin_write_line (req, ADMIN_SHOW_STATS_LISTENER_ALT, "Average listener time: %s", connect_average (stat.client_connect_time, stat.client_connections + extra_clientconnections, timebuf));
		admin_write_line (req, ADMIN_SHOW_STATS_LISTENER_ALTF, "Average listener transfer: %lu KBytes", transfer_average (stat.write_kilos, stat.client_connections + extra_clientconnections));
	}
#else
	admin_write_line (req, ADMIN_SHOW_UPTIME, "NtripCaster %s server running on %s default port %d, uptime: %s", info.version, nullcheck_string (info.server_name), info.port[0],
			  nntripcaster_time (t - info.server_start_time, timebuf));
	
	admin_write_line (req, ADMIN_SHOW_STATS_NUMADMINS, "Admins (%d):", info.num_admins);
	admin_write_line (req, ADMIN_SHOW_STATS_NUMSOURCES, "Sources (%d):", info.num_sources);
	admin_write_line (req, ADMIN_SHOW_STATS_NUMLISTENERS, "Listeners (%d):", info.num_clients);
	
	admin_write_line (req, ADMIN_SHOW_STATS_MISC, "Displaying server statistics:");
	admin_write_line (req, ADMIN_SHOW_STATS_READ, "Total KBytes read: %lu", stat.read_kilos);
	admin_write_line (req, ADMIN_SHOW_STATS_WRITTEN, "Total KBytes written: %lu", stat.write_kilos);
	admin_write_line (req, ADMIN_SHOW_STATS_SOURCE_CONNECTS, "Number of source connects: %lu", stat.source_connections);
	admin_write_line (req, ADMIN_SHOW_STATS_CLIENT_CONNECTS, "Number of client connects: %lu", stat.client_connections);
	
	if (stat.client_connections > 0)
	{
		admin_write_line (req, ADMIN_SHOW_STATS_LISTENER_ALT, "Average listener time: %s", connect_average (stat.client_connect_time, stat.client_connections + extra_clientconnections, timebuf));
		admin_write_line (req, ADMIN_SHOW_STATS_LISTENER_ALTF, "Average listener transfer: %lu KBytes", transfer_average (stat.write_kilos, stat.client_connections + extra_clientconnections));
	}
#endif
	
	if (stat.source_connections > 0)
	{
		admin_write_line (req, ADMIN_SHOW_STATS_SOURCE_ALT,  "Average source connect time: %s", connect_average (stat.source_connect_time, stat.source_connections + extra_sourceconnections, timebuf));
		admin_write_line (req, ADMIN_SHOW_STATS_SOURCE_ALTF, "Average source transfer: %lu KBytes", transfer_average (stat.read_kilos, stat.source_connections + extra_sourceconnections));
	}

	admin_write_line (req, ADMIN_SHOW_STATS_END, "End of statistics");
	
	return 1;
}

int com_resync(com_request_t *req)
{
	char *arg = com_arg(req);
	
	admin_write_line (req, ADMIN_SHOW_RESYNC, "Resyncing server");
	
	if (arg && arg[0]) 
		com_tell(req);

	set_server_running(0);

	return 1;
}

int
com_oper (com_request_t *req)
{
	admin_t *adm;
	char *arg = com_arg (req);
	
	if (!arg)
	{
		admin_write_line (req, ADMIN_SHOW_OPER_INVALID, "*hint* The password is longer than that :)");
		return 1;
	}

	adm = req->con->food.admin;
	
	if (password_match (info.oper_pass, arg))
	{
		adm->oper = 1;
		admin_write_line (req, ADMIN_SHOW_OPER_OK, "You are now an NtripCaster operator");
		write_log (LOG_DEFAULT, "Admin %s[%i] is now an NtripCaster operator", con_host (req->con), req->con->id);
	} else
		admin_write_line (req, ADMIN_SHOW_OPER_INVALID, "Invalid password");
	return 1;
}

int
com_quit (com_request_t *req)
{
	char *arg = com_arg (req);
	
	if (req->con->host && ntripcaster_strcmp (req->con->host, "NtripCaster console") == 0)
	{
		fd_write_line (1, "Sorry, you can't quit from the local admin console");
	} else {
		char message[BUFSIZE+15];
		snprintf(message, BUFSIZE+15, "Admin quit: %s", arg ? arg : "No message");
		admin_write_line (req, ADMIN_SHOW_QUIT_BYE, "Okay, bye!");
		kick_connection (req->con, message);
	}
	return 1;
}  

/*
int
com_touch (com_request_t *req)
{
	admin_write_line (req, ADMIN_SHOW_TOUCH, "Touching directory servers");

	info.directorylasttime = get_time ();
	
	thread_mutex_lock(&info.directory_mutex);
	avl_walk (info.d_servers, avl_touch_directory, &info);
	thread_mutex_unlock(&info.directory_mutex);

	return 1;
}
*/

/*
#define DIRHELP "dir <add|del|list> [options|number]\r\n"
int
com_dir (com_request_t *req)
{
	char *out, *arg = com_arg (req);
	char type[BUFSIZE];
	
	if (!arg || !arg[0])
	{
		admin_write (req, ADMIN_SHOW_DIR_INVALID_SYNTAX, DIRHELP);
		return 1;
	}
	
	out = splitc (type, arg, ' ');
	
	if (!out)
		strncpy(type, arg, BUFSIZE);
	
	if (type[0] == 'a')
	{
		char host[BUFSIZE], port[BUFSIZE];
		directory_server_t *dir;
		
		if (!out)
		{
			admin_write (req, ADMIN_SHOW_DIR_INVALID_SYNTAX, DIRHELP);
			return 1;
		}
		
		dir = create_directory ();
		dir->counter = 0;
		dir->id = -1;
		dir->type = xaudiocast_e;
		dir->host = NULL;
		dir->path = NULL;
		
		if (strchr (arg, '/') != NULL && strchr (arg, ':') == NULL)
		{
			admin_write_line (req, ADMIN_SHOW_DIR_INVALID_SYNTAX, "ERROR in directory syntax. Path but no port specified");
			return 1;
		}
		
		if (strstr (arg, " icy") != NULL)
		{
			dir->type = icy_e;
			*(strchr (arg, ' ')) = '\0';
		}
		
		if (splitc (host, arg, ':') == NULL) // No port, no path
		{
			dir->port = 80;
			dir->path = nstrdup (DEFAULT_YP_TOUCH);
			strncpy(host, arg, BUFSIZE);
		} else if (splitc (port, arg, '/') == NULL) // port, but no path
		{
			dir->port = atoi (arg);
			dir->path = nstrdup (DEFAULT_YP_TOUCH);
		} else {
			dir->port = atoi (port);
			dir->path = nstrdup (arg);
		}
		
		dir->host = nstrdup (host);
		
		if (dir->type == icy_e)
			admin_write_line (req, ADMIN_SHOW_DIR_ADD_SERVER_ICY, "Adding icy directory server [%s:%d]", dir->host, dir->port);
		else
			admin_write_line (req, ADMIN_SHOW_DIR_ADD_SERVER_XA,  "Adding xaudiocast directory server [%s:%d/%s]", dir->host, dir->port, dir->path);
		
		thread_mutex_lock (&info.directory_mutex);
		avl_insert (info.d_servers, dir);
		thread_mutex_unlock (&info.directory_mutex);
		
		return 1;
	}
	
	if (type[0] == 'd')
	{
		directory_server_t *out, ds;
		char *ptr;
		
		if ((ptr = strchr (arg, ':')))
		{
			*ptr = '\0';
			ds.port = atoi (ptr + 1);
			ds.host = &arg[0];
		} else {
			ds.port = 80;
			ds.host = &arg[0];
		}
		
		thread_mutex_lock (&info.directory_mutex);
		out = avl_delete (info.d_servers, &ds);
		thread_mutex_unlock (&info.directory_mutex);
		
		if (out && out->id >= 0 && out->type == icy_e)
			directory_remove (out);
		
		if (out)
		{
			admin_write_line (req, ADMIN_SHOW_DIR_SERVER_REMOVED, "Directory server [%s:%d] removed", out->host, out->port);
			nfree (out->host);
			nfree (out->path);
			nfree (out);
		} else {
			admin_write_line (req, ADMIN_SHOW_DIR_SERVER_REMOVE_FAILED, "Directory removal failed.");
		}
	}

	if (type[0] == 'l')
	{
		int listed = 0;
		avl_traverser trav = {0};
		directory_server_t *dir;

		admin_write_line (req, ADMIN_SHOW_DIR_START, "Listing directories: ");
		
		thread_mutex_lock(&info.directory_mutex);
		
		while ((dir = avl_traverse (info.d_servers, &trav)))
		{
			admin_write_line (req, ADMIN_SHOW_DIR_ENTRY, "[Host: %s] [Id: %d] [Touches: %d]", dir->host, dir->id, dir->counter);
			listed++;
		}
		
		thread_mutex_unlock(&info.directory_mutex);

		admin_write_line (req, ADMIN_SHOW_DIR_END, "End of directory listing (%d listed)", listed);
	}
	return 1;
}
*/

int
com_kick (com_request_t *req)
{
	char buf[BUFSIZE], *arg = com_arg (req);
	
	if (!arg || !arg[0])
	{
		admin_write_line (req, ADMIN_SHOW_KICK_INVALID_SYNTAX, "kick <id>");
		return 1;
	}
	
	if (splitc (buf, arg, ' ') == NULL)
		strncpy(buf, arg, BUFSIZE);
	
	if (buf[0] == '-')
	{
		connection_t *clicon;
		avl_traverser trav = {0};
		char reason[BUFSIZE];
		snprintf(reason, BUFSIZE, "Matching (%s)", arg);

		switch (buf[1])
		{
			case 'a':
				admin_write_line (req, ADMIN_SHOW_KICKING_ADMINS_MATCHING, "Kicking all admins matching (%s)", arg);
				thread_mutex_lock (&info.admin_mutex);
				do_if_match_tree (info.admins, arg, kick_connection_not_me, reason, 1);
				thread_mutex_unlock (&info.admin_mutex);
				return 1;
				break;
			case 's':
				admin_write_line (req, ADMIN_SHOW_KICKING_SOURCES_MATCHING, "Kicking all sources matching (%s)", arg);
				thread_mutex_lock (&info.source_mutex);
				do_if_match_tree (info.sources, arg, kick_connection, reason, 0);
				thread_mutex_unlock (&info.source_mutex);
				return 1;
				break;
			case 'c':
				admin_write_line (req, ADMIN_SHOW_KICKING_CLIENTS_MATCHING, "Kicking all clients matching (%s)", arg);

				thread_mutex_lock (&info.client_mutex);
				while ((clicon = avl_traverse (info.clients, &trav)))
				{
					if (wild_match ((unsigned const char *)arg, (unsigned const char *) clicon->host)
					|| (clicon->hostname && wild_match ((unsigned const char *)arg, (unsigned const char *)clicon->hostname)))
						kick_connection(clicon, reason);
				}
				thread_mutex_unlock (&info.client_mutex);
				return 1;
				break;
			default:
				admin_write_line (req, ADMIN_SHOW_KICK_INVALID_SYNTAX, "Try 'help kick'");
				return 1;
				break;
		}
	}
				
	if (is_pattern (buf))
	{
		admin_write_line (req, ADMIN_SHOW_KICKING_ALL_MATCHING, "Kicking all connections matching (%s)", buf);
		kick_if_match (buf);
		return 1;
	} else {
		connection_t *kick = find_id (atoi (arg));
		
		if (!kick)
		{
			admin_write_line (req, ADMIN_SHOW_KICK_INVALID_ID, "No such id %d", atoi (arg));
			return 1;
		}
		
		if (kick->id == req->con->id)
		{
			admin_write_line (req, ADMIN_SHOW_KICK_YOURSELF, "Some people kick theirselves, but please do it somewhere less public ;)");
			return 1;
		}
		kick_connection (kick, "Kicked by admin");
	}
	return 1;
}

 
/* Used either like:
   select -p <sourceid> <pattern> <newsourceid>
   select -a <sourceid> <newsourceid>
   select <clientid> <newsourceid> */
/*
int
com_sel (com_request_t *req)
{
	char first[BUFSIZE], second[BUFSIZE], *arg = com_arg (req);
	
	if (!arg || !arg[0] || (splitc (first, arg, ' ') == NULL))
	{
		admin_write_line (req, ADMIN_SHOW_SELECT_INVALID_SYNTAX, "Try 'help select'");
		return 1;
	}
	
	if (first[0] == '-')
	{
		if (splitc (second, arg, ' ') != NULL)
		{
			if (first[1] == 'a')
			{
				move_all_from (req, source_get_id (second), source_get_id (arg));
				return 1;
			} else if (first[1] == 'p')
			{
				move_all_matching (req, source_get_id (second), arg);
				return 1;
			}
		}
		admin_write_line (req, ADMIN_SHOW_SELECT_INVALID_SYNTAX, "Try 'help select'");
		return 1;
	}
	
	if (!isdigit ((int)first[0]))
	{
		admin_write_line (req, ADMIN_SHOW_SELECT_INVALID_SYNTAX, "Try 'help select'");
		return 1;
	}
	
	move_id_to (req, atoi (first), source_get_id (arg));
	return 1;
}
*/

void
move_to (void *clientarg, void *sourcetargetarg)
{
	connection_t *client = (connection_t *)clientarg;
	connection_t *sourcetarget = (connection_t *)sourcetargetarg;
	
	avl_delete (client->food.client->source->clients, client);
	del_client (client, client->food.client->source);
	client->food.client->virgin = 1;
	client->food.client->alive = CLIENT_ALIVE;
	client->food.client->source = sourcetarget->food.source;
	pool_add (client);
	util_increase_total_clients ();
}


/* Move all clients from sourceid, matching pattern, to new id */
/*
void
move_all_matching (com_request_t *req, int sourceid, char *arg)
{
	char pattern[BUFSIZE];
	connection_t *sourcecon, *targetcon;

	sourcecon = find_source_with_id (sourceid);
	if (!sourcecon)
	{
		admin_write_line (req, ADMIN_SHOW_SELECT_INVALID_SOURCE_ID, "Error in source id [%d]", sourceid);
		return;
	}

	if (splitc (pattern, arg, ' ') == NULL)
	{
		admin_write_line (req, ADMIN_SHOW_SELECT_INVALID_SYNTAX, "Try 'help select'");
		return;
	}

	targetcon = find_source_with_id (atoi (arg));
	if (!targetcon)
	{
		admin_write_line (req, ADMIN_SHOW_SELECT_INVALID_TARGET_ID, "Error in target id [%d]", atoi (arg));
		return;
	}
	
	admin_write_line (req, ADMIN_SHOW_SELECT_MOVING_ALL_MATCHING, "Moving all clients matching pattern [%s] from source %s to source %s",
			  pattern, sourcecon->food.source->audiocast.mount, targetcon->food.source->audiocast.mount);
	
	thread_mutex_lock (&info.double_mutex);
	
	thread_mutex_lock (&sourcecon->food.source->mutex);
	thread_mutex_lock (&targetcon->food.source->mutex);
	
	do_if_match_tree (sourcecon->food.source->clients, pattern, move_to, targetcon, 1);
	
	thread_mutex_unlock (&sourcecon->food.source->mutex);
	thread_mutex_unlock (&targetcon->food.source->mutex);

	thread_mutex_unlock (&info.double_mutex);

	admin_write_line (req, ADMIN_SHOW_SELECT_MOVE_OK, "Done, source now has %d clients, and target source has %d",
			  sourcecon->food.source->num_clients, targetcon->food.source->num_clients);
	return;
}
*/	

/* Move all client from source with id sourceid, to source with id targetid */
/*
void 
move_all_from (com_request_t *req, int sourceid, int targetid)
{
  connection_t *sourcecon, *targetcon, *clicon;

  sourcecon = find_source_with_id (sourceid);
  if (!sourcecon)
  {
	  admin_write_line (req, ADMIN_SHOW_SELECT_INVALID_SOURCE_ID, "Error in source id [%d]", sourceid);
	  return;
  }
  
  targetcon = find_source_with_id (targetid);
  if (!targetcon)
  {
	  admin_write_line (req, ADMIN_SHOW_SELECT_INVALID_TARGET_ID, "Error in target id [%d]", targetid);
	  return;
  }
  
  admin_write_line (req, ADMIN_SHOW_SELECT_MOVING_ALL, "Moving all clients from source %s to source %s",
		    sourcecon->food.source->audiocast.mount, targetcon->food.source->audiocast.mount);
  
  thread_mutex_lock (&info.double_mutex);
  
  thread_mutex_lock (&sourcecon->food.source->mutex);
  thread_mutex_lock (&targetcon->food.source->mutex);
  
  while ((clicon = avl_get_any_node (sourcecon->food.source->clients)))
  {
	  avl_delete (sourcecon->food.source->clients, clicon);
	  del_client (clicon, sourcecon->food.source);
	  clicon->food.client->virgin = 1;
	  clicon->food.client->source = targetcon->food.source;
	  pool_add (clicon);
	  util_increase_total_clients ();
  }
  
  thread_mutex_unlock (&targetcon->food.source->mutex);
  thread_mutex_unlock (&sourcecon->food.source->mutex);
  
  thread_mutex_unlock (&info.double_mutex);
  
  admin_write_line (req, ADMIN_SHOW_SELECT_MOVE_OK, "Done, old source has %d clients, new source has %d clients",
		    sourcecon->food.source->num_clients, targetcon->food.source->num_clients);
}
*/

/*
void
move_id_to (com_request_t *req, int id, int newsourceid)
{
	connection_t *con = find_client_with_id (id);
	connection_t *newsourcecon = find_source_with_id (newsourceid);
	
	if (!con || con->type != client_e)
	{
		admin_write_line (req, ADMIN_SHOW_SELECT_INVALID_CLIENT_ID, "Error in client id [%d]", id);
		return;
	}
	
	if (!newsourcecon || newsourcecon->type != source_e)
	{
		admin_write_line (req, ADMIN_SHOW_SELECT_INVALID_SOURCE_ID, "Error in new source id [%d]", newsourceid);
		return;
	}
	
	admin_write_line (req, ADMIN_SHOW_SELECT_MOVING_CLIENT, "Moving client %d from source %s to source %s", 
			  con->id, con->food.client->source->audiocast.mount,
			  newsourcecon->food.source->audiocast.mount);
	
	thread_mutex_lock (&con->food.client->source->mutex);
	avl_delete (con->food.client->source->clients, con);
	thread_mutex_unlock (&con->food.client->source->mutex);
	del_client (con, con->food.client->source);

	con->food.client->virgin = 1;
	con->food.client->source = newsourcecon->food.source;
	pool_add (con);

	util_increase_total_clients ();
	admin_write_line (req, ADMIN_SHOW_SELECT_MOVE_OK, "Moved client %d to client tree of source %d", 
			  con->id, newsourcecon->id);
	return;
}
*/

int
com_list (com_request_t *req)
{
	char pattern[BUFSIZE], *arg = com_arg (req), buf[BUFSIZE];
	connection_t *con, *sourcecon;
	avl_traverser trav = {0}, sourcetrav = {0};
	int listed = 0;
	time_t t = get_time ();

	pattern[0] = '\0';

	if (arg && arg[0])
	{
		strncpy(pattern, arg, BUFSIZE);
		admin_write_line (req, ADMIN_SHOW_LIST_START, "Listing connections matching [%s]:", pattern);
	} else {
		admin_write_line (req, ADMIN_SHOW_LIST_START, "Listing connections:", pattern);
	}
	
	thread_mutex_lock(&info.admin_mutex);

	while ((con = avl_traverse (info.admins, &trav)))
	{
		if (pattern[0])
			if (!hostmatch (con, pattern))
				continue;
		listed++;
		admin_write_line (req, ADMIN_SHOW_LIST_ENTRY, "[Id: %lu] [Host: %s] [Type: admin] [Connected for: %s]", 
				  con->id, con_host (con), nntripcaster_time (t - con->connect_time, buf));
	}
	
	thread_mutex_unlock (&info.admin_mutex);

	zero_trav (&trav);
	
	thread_mutex_lock (&info.client_mutex);

	while ((con = avl_traverse (info.clients, &trav)))
	{
		if (pattern[0]) if (!hostmatch (con, pattern)) continue;

		admin_write_line (req, ADMIN_SHOW_LIST_ENTRY, "[Id: %lu] [Host: %s] [Type: client] [Connected for: %s]", con->id, con_host (con), nntripcaster_time (t - con->connect_time, buf));
		listed++;
	}
	
	thread_mutex_unlock (&info.client_mutex);

	thread_mutex_lock (&info.source_mutex);

	while ((sourcecon = avl_traverse (info.sources, &sourcetrav)))
	{
		if (pattern[0]) if (!hostmatch (sourcecon, pattern)) continue;
		
		admin_write_line (req, ADMIN_SHOW_LIST_ENTRY, "[Id: %lu] [Host: %s] [Type: source] [Connected for: %s]", sourcecon->id, con_host (sourcecon), nntripcaster_time (t - sourcecon->connect_time, buf));
		listed++;
	}
	
	thread_mutex_unlock(&info.source_mutex);
	
	admin_write_line (req, ADMIN_SHOW_LIST_END, "End of list listing (%d listed)", listed);
	return 1;
}


#define RELAY_SYNTAX "Syntax: relay <pull|list> <arguments>"
int com_relay(com_request_t *req)
{
	char command[BUFSIZE], *arg = com_arg(req);
	int err = OK;

	if (!arg || !arg[0]) {
  		admin_write_line(req, ADMIN_SHOW_RELAY_INVALID_SYNTAX, RELAY_SYNTAX);
		return 1;
	}

	if (splitc(command, arg, ' ') == NULL) {
		strncpy(command, arg, BUFSIZE);
		arg[0] = '\0';
	}

/* command "relay push" disabled. ajd */
		if (ntripcaster_strncmp(command, "pull", 4) == 0)
			err = relay_add_pull_to_list (arg);
		else if (ntripcaster_strncmp(command, "del", 3) == 0) {
			ntrip_request_t rreq;
			generate_http_request (arg, &rreq);

			if (!is_valid_http_request (&rreq)) {
				admin_write_line (req, ADMIN_SHOW_RELAY_INVALID_SYNTAX, RELAY_SYNTAX);
				return 0;
			}
		
			if (relay_remove_with_req (&rreq) > 0)
				admin_write_line (req, ADMIN_SHOW_RELAY_REMOVED, "Relay [%s:%d%s] removed",
						  rreq.host, rreq.port, rreq.path);
			else
				admin_write_line (req, ADMIN_SHOW_RELAY_REMOVED, "Could not remove [%s:%d%s]",
						  rreq.host, rreq.port, rreq.path);
			return 0;
		} else if (ntripcaster_strncmp(command, "list", 4) == 0) {
			avl_traverser trav = {0};
			relay_t *rel;

			admin_write_line (req, ADMIN_SHOW_RELAY_LIST_START, "Listing relays:");
			thread_mutex_lock (&info.relay_mutex);

			while ((rel = avl_traverse (info.relays, &trav)))
			{
#ifdef CHANGE1
				admin_write_line (req, ADMIN_SHOW_RELAY_ITEM,
						  "Remote: [%s:%d%s] Local: [%s] Connected: [%s] Type: [%s] Reconnects: [%d]",
						  rel->req.host, rel->req.port, rel->req.path, rel->localmount,
						  relay_connected_or_pending (rel) ? "yes" : "no", "pull",
						  rel->reconnects);
#else
				admin_write_line (req, ADMIN_SHOW_RELAY_ITEM,
						  "Remote: [%s:%d%s] Local: [%s] Connected: [%s] Type: [%s] Reconnects: [%d]",
						  rel->req.host, rel->req.port, rel->req.path, rel->localmount,
						  relay_connected_or_pending (rel) ? "yes" : "no", "pull",
						  rel->reconnects);
#endif
			}
  	
			thread_mutex_unlock (&info.relay_mutex);
			admin_write_line (req, ADMIN_SHOW_RELAY_LIST_END, "End of relay listing");
			return 1;
		} else {
 	 		admin_write_line(req, ADMIN_SHOW_RELAY_INVALID_SYNTAX, RELAY_SYNTAX);
			return 0;
		}


	if (err != OK)
	{
		switch (err)
		{
			case ICE_ERROR_INVALID_SYNTAX:
				admin_write_line (req, ADMIN_SHOW_RELAY_INVALID_SYNTAX, RELAY_SYNTAX);
				break;
			case ICE_ERROR_NO_SUCH_MOUNT:
				admin_write_line (req, ADMIN_SHOW_RELAY_INVALID_SOURCE, "No such source");
				break;
			case ICE_ERROR_ARGUMENT_REQUIRED:
				admin_write_line (req, ADMIN_SHOW_RELAY_ARGUMENT_REQUIRED, "Argument required for option");
				break;
			default:
				admin_write_line (req, ADMIN_SHOW_RELAY_INVALID_SYNTAX, "Unspecific error");
				break;
		}
		return 0;
	} else {
		admin_write_line (req, ADMIN_SHOW_RELAY_OK, "Relay added");
	}
	
	return 1;
}

#define ALIAS_USAGE "alias <add|del|list> [mountpoint] [real source]\r\n"
int
com_alias (com_request_t *req)
{
	char type[BUFSIZE], *arg = com_arg (req);
	
	if (!arg || !arg[0])
	{
		admin_write (req, ADMIN_SHOW_ALIAS_INVALID_SYNTAX, ALIAS_USAGE);
		return 0;
	}

	if (splitc (type, arg, ' ') == NULL)
		strncpy(type, arg, BUFSIZE);
	
	if (ntripcaster_strcmp (type, "list") == 0)
	{
		list_aliases (req);
		return 1;
	} else if (ntripcaster_strcmp (type, "add") == 0)
	{
		char mountpoint[BUFSIZE];
		ntrip_request_t nameline, realline;
		alias_t *res = NULL;
		
		zero_request (&nameline);
		zero_request (&realline);
		
		if (splitc (mountpoint, arg, ' ') == NULL)
		{
			admin_write (req, ADMIN_SHOW_ALIAS_INVALID_SYNTAX, ALIAS_USAGE);
			return 0;
		}

		generate_request (mountpoint, &nameline);
		generate_request (arg, &realline);

		if (!nameline.path[0] || !realline.path[0])
		{
			admin_write (req, ADMIN_SHOW_ALIAS_INVALID_SYNTAX, ALIAS_USAGE);
			return 0;
		}
			
		res = add_alias (&nameline, &realline, NULL, NULL);
		if (res)
		{
			admin_write_line (req, ADMIN_SHOW_ALIAS_ADD_OK,  "Added alias [%s] for [%s]", res->name, res->real);
			return 1;
		} else {
			admin_write_line (req, ADMIN_SHOW_ALIAS_ADD_FAILED, "Alias add failed, alias not added");
			return 0;
		}
	} else if (ntripcaster_strcmp (type, "del") == 0)
	{
		if (del_alias (arg))
		{
			admin_write_line (req, ADMIN_SHOW_ALIAS_REMOVE_OK, "Removed alias [%s]", arg);
			return 1;
		} else {
			admin_write_line (req, ADMIN_SHOW_ALIAS_REMOVE_FAILED, "Alias remove failed", arg);
			return 0;
		}
	} else {
		admin_write_line (req, ADMIN_SHOW_ALIAS_UNKNOWN_SUBCOMMAND, "Unknown command alias [%s] [%s]", type, arg);
		return 0;
	}
}

int 
com_threads(com_request_t *req)
{
	avl_traverser trav = {0};
	mythread_t *mt;
	char time[100];
	int listed = 0;
	char *arg = com_arg (req);
	char command[BUFSIZE];

	if ((arg != NULL) && (splitc (command, arg, ' ') != NULL)) {
		if (ntripcaster_strncmp (command, "kill", 4) == 0) {
			thread_kill(atoi(arg));
		}
		return 1;		
	}

	admin_write_line (req, ADMIN_SHOW_THREADS_START, "Listing threads (might take a short while)");

	internal_lock_mutex (&info.thread_mutex); /* can't use thread_mutex_lock() cause it calls thread_get_mythread() which in turn locks thread_mutex */
	
	while ((mt = avl_traverse (info.threads, &trav)))
	{
		mt->ping = 1;
	}
	
	my_sleep (1000000); /* Wait for the threads to unlock it */

	zero_trav (&trav);

	while ((mt = avl_traverse (info.threads, &trav)))
	{
		get_string_time (time, mt->created, REGULAR_DATETIME);

		admin_write_line (req, ADMIN_SHOW_THREADS_ENTRY, "%d\tType: [%23s]\tStarted [File: %10s Line: %d] Stuck: %s Started: %s", mt->id, mt->name, mt->file, mt->line, mt->ping == 0 ? "no" : "yes", time);
				  
		listed++;
	}
	
	internal_unlock_mutex (&info.thread_mutex);

	admin_write_line (req, ADMIN_SHOW_THREADS_END, "End of threads listing (%d listed)", listed);
	
	return 1;
}

int
com_status (com_request_t *req)
{
	char *arg = com_arg (req);
	char timebuf[BUFSIZE];
	long filetime;

	filetime = read_starttime();
	if (filetime > 0)
		nntripcaster_time (get_time () - filetime, timebuf);
	else
		nntripcaster_time (get_time () - info.server_start_time, timebuf);

	if (arg && arg[0])
	{
		if (ntripcaster_strncmp (arg, "off", 3) == 0)
			req->con->food.admin->status = 0;
		else if (ntripcaster_strncmp (arg, "on", 2) == 0)
			req->con->food.admin->status = 1;
		else if (ntripcaster_strncmp (arg, "show", 4) == 0)
		{
			char lt[100];

			get_log_time (lt);
			admin_write_line (req, ADMIN_SHOW_STATUS, "[%s] [Bandwidth: %fKB/s] [Sources: %ld] [Clients: %ld] [Admins: %ld] [Uptime: %s] ",
					 lt, info.bandwidth_usage, info.num_sources, info.num_clients, info.num_admins,  timebuf);
			return 1;
		} else {
			admin_write_line (req, ADMIN_SHOW_STATUS_INVALID_SYNTAX,  "Usage: status [off|on]");
			return 1;
		}
	}
	admin_write_line (req, ADMIN_SHOW_STATUS_NEW, "Status information is [%s]", req->con->food.admin->status ? "on" : "off");
	return 1;
}

/*
#define DUMP_USAGE "dump <sourceid> <filename|close>\r\n"
int
com_dump (com_request_t *req)
{
	char cid[BUFSIZE], *arg = com_arg (req);
	connection_t *sourcecon;
	
	if (!arg || !arg[0])
	{
		admin_write (req, ADMIN_SHOW_DUMP_INVALID_SYNTAX, DUMP_USAGE);
		return 0;
	}
	
	if (splitc (cid, arg, ' ') == NULL)
	{
		admin_write (req, ADMIN_SHOW_DUMP_INVALID_SYNTAX, DUMP_USAGE);
		return 0;
	}
	
	sourcecon = find_source_with_id (atoi (cid));
	if (!sourcecon)
	{
		admin_write_line (req, ADMIN_SHOW_DUMP_INVALID_SOURCE_ID, "No source with id [%s] found.", cid);
		return 0;
	}
	
	thread_mutex_lock (&sourcecon->food.source->mutex);
	
	if (ntripcaster_strncmp(arg, "close", 5) == 0) {
		fd_close(sourcecon->food.source->dumpfd);
		nfree(sourcecon->food.source->dumpfile);
		sourcecon->food.source->dumpfile = NULL;
		sourcecon->food.source->dumpfd = -1;

		thread_mutex_unlock(&sourcecon->food.source->mutex);
		
		admin_write_line (req, ADMIN_SHOW_DUMP_FILE_CLOSED,  "Ok, stopped and closed dumping for source %d", sourcecon->id);
		return 1;
	}

	sourcecon->food.source->dumpfile = my_strdup (arg);
	// Don't use fopen().. it can only play with fd < 255
	sourcecon->food.source->dumpfd = open_for_writing (sourcecon->food.source->dumpfile);

	if (sourcecon->food.source->dumpfd < 0)	{
		admin_write_line (req, ADMIN_SHOW_DUMP_OPEN_FAILED, "Failed opening dumpfile [%s] for source %d [%d]", sourcecon->food.source->dumpfile, sourcecon->id, errno);
		nfree(sourcecon->food.source->dumpfile);
		sourcecon->food.source->dumpfile = NULL;
		fd_close(sourcecon->food.source->dumpfd);

		thread_mutex_unlock(&sourcecon->food.source->mutex);
		return 0;
	}

	thread_mutex_unlock(&sourcecon->food.source->mutex);

	admin_write_line (req, ADMIN_SHOW_DUMP_OK,  "Ok, dumping source %d to %s", sourcecon->id, sourcecon->food.source->dumpfile);

	return 1;
}
*/

/*
#define PAUSE_USAGE "pause <client_id|source_id>\r\n"
int
com_pause (com_request_t *req)
{
	char cid[BUFSIZE], *arg = com_arg (req);
	connection_t *pausecon;

	if (!arg || !arg[0])
	{
		admin_write (req, ADMIN_SHOW_PAUSE_INVALID_SYNTAX, PAUSE_USAGE);
		return 0;
	}

	if (splitc (cid, arg, ' ') == NULL)
		strncpy(cid, arg, BUFSIZE);

	pausecon = find_source_with_id (atoi (cid));
	if (!pausecon)
		pausecon = find_client_with_id (atoi (cid));

	if (!pausecon)
	{
		admin_write_line (req, ADMIN_SHOW_PAUSE_INVALID_ID, "No source or client with id [%s] found.", cid);
		return 0;
	}
	
	if (pausecon->type == source_e)
		pausecon->food.source->connected = SOURCE_PAUSED;
	else if (pausecon->type == client_e)
		pausecon->food.client->virgin = CLIENT_PAUSED;
	else {
		admin_write_line (req, ADMIN_SHOW_PAUSE_INVALID_TYPE, "Invalid type of paused id [%s].", cid);
		return 0;
	}

	return 1;
}
*/

/*
int
com_unpause (com_request_t *req)
{
	char cid[BUFSIZE], *arg = com_arg (req);

	connection_t *pausecon;
	
	if (!arg || !arg[0])
	{
		admin_write_line (req, ADMIN_SHOW_UNPAUSE_INVALID_SYNTAX, PAUSE_USAGE);
		return 0;
	}

	if (splitc (cid, arg, ' ') == NULL)
		strncpy(cid, arg, BUFSIZE);
	
	pausecon = find_source_with_id (atoi (cid));
	if (!pausecon)
		pausecon = find_client_with_id (atoi (cid));
	
	if (!pausecon)
	{
		admin_write_line (req, ADMIN_SHOW_UNPAUSE_INVALID_ID, "No source or client with id [%s] found.", cid);
		return 0;
	}
	
	if (pausecon->type == source_e)
	{
		avl_traverser trav = {0};
		connection_t *client;
		
		thread_mutex_lock (&pausecon->food.source->mutex);
		
		while ((client = avl_traverse (pausecon->food.source->clients, &trav)))
			client->food.client->virgin = CLIENT_UNPAUSED;
		
		thread_mutex_unlock (&pausecon->food.source->mutex);
		
		pausecon->food.source->connected = SOURCE_CONNECTED;
	} else if (pausecon->type == client_e)
		pausecon->food.client->virgin = CLIENT_UNPAUSED;
	else {
		admin_write_line (req, ADMIN_SHOW_UNPAUSE_INVALID_TYPE, "Invalid type of unpaused id [%s].", cid);
		return 0;
	}
	
	return 1;
}
*/

#define RESTRICT_USAGE "<deny|allow> <add|del|list> [mask]\r\n"
int
com_restrict (com_request_t *req, avl_tree *tree, acltype_t acl_type)
{
	char type[BUFSIZE], *arg = com_arg (req);
	
	if (!arg || !arg[0])
	{
		admin_write (req, ADMIN_SHOW_RESTRICT_INVALID_SYNTAX, RESTRICT_USAGE);
		return 0;
	}

	if (splitc (type, arg, ' ') == NULL)
		strncpy(type, arg, BUFSIZE);

	if (ntripcaster_strcmp (type, "list") == 0)
	{
		list_restrict (req, tree, acl_type);
		return 1;
	} else if (ntripcaster_strcmp (type, "add") == 0)
	{
		restrict_t *res = add_restrict (tree, arg, acl_type);

		if (res)
		{
			admin_write_line (req, ADMIN_SHOW_RESTRICT_ADD_OK, "Added acl rule [%s]", res->mask);
			return 1;
		} else {
			admin_write_line (req, ADMIN_SHOW_RESTRICT_ADD_FAILED, "Acl addition for [%s] failed", arg);
			return 0;
		}
	} else if (ntripcaster_strcmp (type, "del") == 0)
	{
		if (del_restrict (tree, arg, acl_type))
		{
			admin_write_line (req, ADMIN_SHOW_RESTRICT_REMOVE_OK, "Acl rule [%s] removed", arg);
			return 1;
		} else {
			admin_write_line (req, ADMIN_SHOW_RESTRICT_REMOVE_FAILED, "Acl removal of [%s] failed", arg);
		}
	} else {
		admin_write_line (req, ADMIN_SHOW_RESTRICT_UNKNOWN_SUBCOMMAND, "Unknown restrict command [%s] [%s]", type, arg);
		return 0;
	} 
	return 0;
}

avl_tree *
get_acl_tree (char *type)
{
	if (ntripcaster_strncmp (type, "client", 5) == 0)
		return info.client_acl;
	if (ntripcaster_strncmp (type, "source", 5) == 0)
		return info.source_acl;
	if (ntripcaster_strncmp (type, "admin", 5) == 0)
		return info.admin_acl;
	if (ntripcaster_strncmp (type, "all", 3) == 0)
		return info.all_acl;

	return NULL;
}

#define DENY_USAGE "deny <client|source|admin|all> <add|del|list> [hostmask]\r\n"
int
com_deny (com_request_t *req)
{
	char type[BUFSIZE], *arg = com_arg (req);
	avl_tree *treetype; 

	if (!arg || splitc (type, arg, ' ') == NULL)
	{
		admin_write (req, ADMIN_SHOW_RESTRICT_INVALID_SYNTAX, DENY_USAGE);
		return 0;
	}
	
	if ((treetype = get_acl_tree (type)) == NULL)
	{
		admin_write (req, ADMIN_SHOW_RESTRICT_INVALID_SYNTAX, DENY_USAGE);
		return 0;
	}
	return com_restrict (req, treetype, deny);
}

#define ALLOW_USAGE "allow <client|source|admin|all> <add|del|list> [hostmask]\r\n"
int
com_allow (com_request_t *req)
{
	char type[BUFSIZE], *arg = com_arg (req);
	avl_tree *treetype; 

	if (!arg || splitc (type, arg, ' ') == NULL)
	{
		admin_write (req, ADMIN_SHOW_RESTRICT_INVALID_SYNTAX, ALLOW_USAGE);
		return 0;
	}
	
	if ((treetype = get_acl_tree (type)) == NULL)
	{
		admin_write (req, ADMIN_SHOW_RESTRICT_INVALID_SYNTAX, ALLOW_USAGE);
		return 0;
	}

	return com_restrict (req, treetype, allow);
}

#define ACL_USAGE "acl []\r\n"
int
com_acl (com_request_t *req)
{
	avl_tree *treetype;
	char *arg = com_arg (req);

	if (!arg) 
	{
		admin_write (req, ADMIN_SHOW_ACL_INVALID_SYNTAX, ACL_USAGE);
		return 0;
	}
	
	if ((treetype = get_acl_tree ("all")) == NULL)
	{
		admin_write (req, ADMIN_SHOW_ACL_INVALID_SYNTAX, ACL_USAGE);
		return 0;
	}
	
	req->arg = "list";
	
	return com_restrict (req, treetype, all);
}

#define MODIFY_USAGE "modify <source_id> <-mungPdpb> <argument>\r\n"
int
com_modify (com_request_t *req)
{
	char cid[BUFSIZE], carg[BUFSIZE], *arg = com_arg (req);
	
	connection_t *sourcecon;
	
	if (!arg || !arg[0] || (splitc (cid, arg, ' ') == NULL))
	{
		admin_write (req, ADMIN_SHOW_MODIFY_INVALID_SYNTAX, MODIFY_USAGE);
		return 0;
	}
	
	sourcecon = find_source_with_id (atoi (cid));
	if (!sourcecon)
	{
		admin_write_line (req, ADMIN_SHOW_MODIFY_INVALID_SOURCE_ID, "No source found with id [%s]", cid);
		return 0;
	}
	
	if (arg[0] != '-')
	{
		admin_write (req, ADMIN_SHOW_MODIFY_INVALID_SYNTAX, MODIFY_USAGE);
		return 0;
	}

	if (splitc (carg, arg, ' ') == NULL)
		strncpy(carg, arg, BUFSIZE);

	switch (carg[1])
	{
//		case 'b':
//			sourcecon->food.source->audiocast.bitrate = atoi (arg);
//			break;
//		case 'p':
//			sourcecon->food.source->audiocast.public = atoi (arg);
//			break;
		case 'P':
			sourcecon->food.source->priority = atoi (arg);
			break;
//		case 'd':
//			nfree (sourcecon->food.source->audiocast.description);
//			sourcecon->food.source->audiocast.description = nstrdup (arg);
//			break;
//		case 'g':
//			nfree (sourcecon->food.source->audiocast.genre);
//			sourcecon->food.source->audiocast.genre = nstrdup (arg);
//			break;
		case 'n':
			nfree (sourcecon->food.source->audiocast.name);
			sourcecon->food.source->audiocast.name = nstrdup (arg);
			break;
//		case 'u':
//			nfree (sourcecon->food.source->audiocast.url);
//			sourcecon->food.source->audiocast.url = nstrdup (arg);
//			break;
		case 'm':
			nfree (sourcecon->food.source->audiocast.mount);
			sourcecon->food.source->audiocast.mount = nstrdup (arg);
			break;
		default:
			admin_write (req, ADMIN_SHOW_MODIFY_INVALID_SYNTAX, MODIFY_USAGE);
			return 0;
	}
	admin_write_line (req, ADMIN_SHOW_MODIFY_VALUE_CHANGED, "Value changed to [%s]", arg);
	return 1;
}

void 
show_lock_status (com_request_t *req, char *name, mutex_t *mutex)
{
	if (mutex->lineno >= 0)
		admin_write_line (req, ADMIN_SHOW_LOCKS_ENTRY, "Mutex: [%21s] Status: [%8s] Thread: [%d] Locked on line: [%d]", name, mutex->thread_id >= 0 ? "locked" : "unlocked", mutex->thread_id, mutex->lineno);
	else if (mutex->thread_id >= 0)
		admin_write_line (req, ADMIN_SHOW_LOCKS_ENTRY, "Mutex: [%21s] Status: [%8s] Thread: [%d]", name, mutex->thread_id >= 0 ? "locked" : "unlocked", mutex->thread_id);
	else
		admin_write_line (req, ADMIN_SHOW_LOCKS_ENTRY, "Mutex: [%21s] Status: [%8s]", name, mutex->thread_id >= 0 ? "locked" : "unlocked");
}

int 
com_locks (com_request_t *req)
{
	char *arg = com_arg (req);
	char command[BUFSIZE];

	if ((arg != NULL) && (splitc (command, arg, ' ') != NULL)) {
		if (ntripcaster_strncmp (command, "unlock", 6) == 0) {
			thread_force_mutex_unlock(arg);
		}
		return 1;		
	}

#ifdef NTRIP_NUMBER
  admin_write_line (req, ADMIN_SHOW_LOCKS_NOT_AVAIL, "Server is compiled with optimization, no mutex information available");
  return 1;
#endif

  show_lock_status (req, "source mutex", &info.source_mutex);
  show_lock_status (req, "admin mutex", &info.admin_mutex);
//  show_lock_status (req, "directory mutex", &info.directory_mutex);
  show_lock_status (req, "alias mutex", &info.alias_mutex);
  show_lock_status (req, "misc mutex", &info.misc_mutex);
//  show_lock_status (req, "mount mutex", &info.mount_mutex);
  show_lock_status (req, "hostname mutex", &info.hostname_mutex);
  show_lock_status (req, "acl mutex", &info.acl_mutex);
  show_lock_status (req, "double mutex", &info.double_mutex);
  show_lock_status (req, "thread mutex", &info.thread_mutex);
  show_lock_status (req, "dns mutex", &info.resolvmutex);
  show_lock_status (req, "relay mutex", &info.relay_mutex);
#ifdef DEBUG_MEMORY
  show_lock_status (req, "memory mutex", &info.memory_mutex);
#endif
#ifdef DEBUG_SOCKETS
  show_lock_status (req, "socket mutex", &sock_mutex);
#endif
  show_lock_status (req, "library mutex", &library_mutex);
  show_lock_status (req, "authentication mutex", &authentication_mutex);
//  show_lock_status (req, "sourcetable mutex", &info.sourcetable_mutex);

  return 1;
}

#define DEBUGSYNTAX "debug <level>\r\n"

int
com_debug (com_request_t *req)
{
	char *arg = com_arg (req);

	if (!arg)
	{
		admin_write_line (req, ADMIN_SHOW_DEBUG_CURRENT, "Your debugging level is [%d]", req->con->food.admin->debuglevel);
		return 1;
	}

	req->con->food.admin->debuglevel = atoi (arg);
	
	admin_write_line (req, ADMIN_SHOW_DEBUG_CHANGED_TO, "Your debugging level is now [%d]", req->con->food.admin->debuglevel);
	return 0;
}
	       
#define AUTHSYNTAX "auth <add|del|list> <group|user|mount> [user password|group [user user2...]|mount [group group2...]\r\n"

int
com_auth (com_request_t *req)
{
	char *arg = com_arg (req);
	char command[BUFSIZE]; /* add/del/list/set. ajd */

	if (!arg || !arg[0])
	{

/*		if (req->con->food.admin->scheme == html_scheme_e) { // added. ajd
			rehash_authentication_scheme ();
			http_display_auth_page(req);
			return 1;

		} else {
*/
			admin_write (req, ADMIN_SHOW_AUTH_INVALID_SYNTAX, AUTHSYNTAX);
			return 0;
//		}
	}
	
//	rehash_authentication_scheme ();

	if (splitc (command, arg, ' ') == NULL)
	{
		admin_write (req, ADMIN_SHOW_AUTH_INVALID_SYNTAX, AUTHSYNTAX);
		return 0;
	}

	if (ntripcaster_strcmp (command, "add") == 0)
		return com_auth_add (req, arg);
	else if (ntripcaster_strcmp (command, "del") == 0)
		return com_auth_del (req, arg);
	else if (ntripcaster_strcmp (command, "list") == 0)
 			return com_auth_list (req, arg);
	else if (ntripcaster_strcmp (command, "set") == 0)
 			return com_auth_set (req, arg);
	else
		admin_write (req, ADMIN_SHOW_AUTH_INVALID_SYNTAX, AUTHSYNTAX);
	return 0;
}

int
com_auth_add (com_request_t *req, char *arg)
{
	char type[BUFSIZE], firstarg[BUFSIZE];
	int count = 0;

	firstarg[0] = type[0] = '\0';

	if ((!splitc (type, arg, ' ')) && !(req->con->food.admin->scheme == html_scheme_e))
	{
		admin_write (req, ADMIN_SHOW_AUTH_INVALID_SYNTAX, AUTHSYNTAX);
		return 0;
	}

	if (!splitc (firstarg, arg, ' '))
	{
		strncpy(firstarg, arg, BUFSIZE); /* Just adding a group or mount with no users / groups */
		count = 1;
	} else
		count = 2; /* arg is second arg */
	
	if (ntripcaster_strncmp (type, "user", 4) == 0)
	{
		if (count == 2)
			return runtime_add_user (firstarg, arg);
		admin_write (req, ADMIN_SHOW_AUTH_INVALID_SYNTAX, AUTHSYNTAX);
		return 0;
	} else if (ntripcaster_strncmp (type, "group", 5) == 0)
	{
		if (count == 2)
			return runtime_add_group_with_user (firstarg, arg);
		return runtime_add_group (firstarg);
	} else if (ntripcaster_strncmp (type, "clientmount", 11) == 0)
	{
		if (count == 2)
			return runtime_add_mount_with_group (firstarg, arg, info.client_mountfile, get_client_mounttree());
		return runtime_add_mount (firstarg, info.client_mountfile, get_client_mounttree());
	} else if (ntripcaster_strncmp (type, "sourcemount", 11) == 0)
	{
		if (count == 2)
			return runtime_add_mount_with_group (firstarg, arg, info.source_mountfile, get_source_mounttree());
		return runtime_add_mount (firstarg, info.source_mountfile, get_source_mounttree());
	}

	admin_write (req, ADMIN_SHOW_AUTH_INVALID_SYNTAX, AUTHSYNTAX);
	return 0;
}
			
		
int
com_auth_del (com_request_t *req, char *arg)
{
	return 1;
}

int
com_auth_list (com_request_t *req, char *arg)
{
	if (ntripcaster_strncmp (arg, "user", 4) == 0)
		con_display_users (req);
	else if (ntripcaster_strncmp (arg, "group", 5) == 0)
		con_display_groups (req);
	else if (ntripcaster_strncmp (arg, "clientmount", 5) == 0)
		con_display_mounts (req, get_client_mounttree());
	else if (ntripcaster_strncmp (arg, "sourcemount", 5) == 0)
		con_display_mounts (req, get_source_mounttree());
	return 1;
}

int
com_auth_set (com_request_t *req, char *arg) {

	return 1;
}

int
com_mem (com_request_t *req)
{
#if defined (DEBUG_MEMORY_MCHECK) && defined (HAVE_MTRACE)
	char *arg = com_arg (req);
	
	if (arg && arg[0])
	{
		if (ntripcaster_strcmp (arg, "trace") == 0)
			mtrace();
		else if (ntripcaster_strcmp (arg, "untrace") == 0)
			muntrace ();
		return 1;
	} 
#endif
		
#ifndef DEBUG_MEMORY
		admin_write_line (req, ADMIN_SHOW_MEM_NOT_AVAIL, "No internal memory debug information available in this binary");
#else
	{
		avl_traverser trav = {0};
		meminfo_t *go;
		char time[100];
		
		unsigned long int totalbytes = 0;
		
		get_string_time (time, go->time, REGULAR_DATETIME);
		
		admin_write_line (req, ADMIN_SHOW_MEM_START, "Listing memory allocation chunks:");
		
		thread_mutex_lock (&info.memory_mutex);
		
		while ((go = avl_traverse (info.mem, &trav)))
		{
			admin_write_line (req, ADMIN_SHOW_MEM_ENTRY, "[%d bytes]\t[%p]\t[%d:%10s]\tthread: [%d]\ttime: %s", go->size, go->ptr, go->line, go->file, go->thread_id, time);
			totalbytes += go->size;
		}
		
		thread_mutex_unlock (&info.memory_mutex);
		
		admin_write_line (req, ADMIN_SHOW_MEM_END, "Total bytes allocated: %u", totalbytes);
	}
		
#endif
		
#if defined(HAVE_MALLINFO)
	{
		struct mallinfo malli = mallinfo ();
		admin_write_line (req, ADMIN_SHOW_MEM_MCHECK_START, "Information as reported by libc malloc():");
		admin_write_line (req, ADMIN_SHOW_MEM_MCHECK_TOTAL, "Total bytes allocated by malloc: %d", malli.arena);
		admin_write_line (req, ADMIN_SHOW_MEM_MCHECK_UNUSED, "Chunks not in use %d", malli.ordblks);
		admin_write_line (req, ADMIN_SHOW_MEM_MCHECK_CHUNK_MMAP, "Chunks allocated by mmap: %d", malli.hblks);
		admin_write_line (req, ADMIN_SHOW_MEM_MCHECK_MMAP, "Total bytes allocated by mmap: %d", malli.hblkhd);
		admin_write_line (req, ADMIN_SHOW_MEM_MCHECK_OCCUPIED, "Total memory occupied by malloc chunks: %d", malli.uordblks);
		admin_write_line (req, ADMIN_SHOW_MEM_MCHECK_FREE, "Total memory occupied by free (not in use): %d", malli.fordblks);
		admin_write_line (req, ADMIN_SHOW_MEM_MCHECK_KEEPCOST, "Top-most releaseable chunksize: %d", malli.keepcost);
	}
#endif
	return 1;
}

/*
#define STREAMTITLE_SYNTAX "streamtitle <-a|source_id> <message>\r\n" 

int
com_streamtitle (com_request_t *req)
{
	int all = 0;
	char id[BUFSIZE];
	char *ptr, *arg = com_arg (req);
	avl_traverser trav = {0};
	connection_t *sourcecon = NULL;
	
	if (!arg || !arg[0])
	{
		admin_write (req, ADMIN_SHOW_STREAMTITLE_INVALID_SYNTAX, STREAMTITLE_SYNTAX);
		return 0;
	}
	
	if (arg[0] == '-')
	{
		if (arg[1] == 'a')
			all = 1;
		splitc (NULL, arg, ' ');
	}

	if ((all == 0) && (splitc (id, arg, ' ') == NULL))
	{
		admin_write (req, ADMIN_SHOW_STREAMTITLE_INVALID_SYNTAX, STREAMTITLE_SYNTAX);
		return 0;
	}

	if (all)
	{
		thread_mutex_lock (&info.source_mutex);
		
		while ((sourcecon = avl_traverse (info.sources, &trav)))
		{
			ptr = sourcecon->food.source->info.streamtitle;
			sourcecon->food.source->info.streamtitle = nstrdup (arg);
			sourcecon->food.source->info.udpseqnr++;
			nfree (ptr);
		}

		thread_mutex_unlock (&info.source_mutex);
		admin_write_line (req, ADMIN_SHOW_STREAMTITLE_ALL_SET, "Set all source titles to %s", arg);

	} else {
		sourcecon = find_source_with_id (atoi (id));

		if (sourcecon)
		{
			ptr = sourcecon->food.source->info.streamtitle;
			sourcecon->food.source->info.streamtitle = nstrdup (arg);
			sourcecon->food.source->info.udpseqnr++;
			nfree (ptr);
		} else {
			admin_write_line (req, ADMIN_SHOW_STREAMTITLE_INVALID_SOURCE_ID, "No source with id %d", atoi (id));
			return 0;
		}
		admin_write_line (req, ADMIN_SHOW_STREAMTITLE_SET, "Set source title for source %d to %s", atoi (id), arg);
	}
	return 1;
}
*/

/*
#define STREAMURL_SYNTAX "streamurl [-a] <url>\r\n"
int
com_streamurl (com_request_t *req)
{
	int all = 0;
	char id[BUFSIZE], *ptr, *arg = com_arg (req);
	avl_traverser trav = {0};
	connection_t *sourcecon;

	if (!arg || !arg[0])
	{
		admin_write (req, ADMIN_SHOW_STREAMURL_INVALID_SYNTAX, STREAMURL_SYNTAX);
		return 0;
	}
	
	if (arg[0] == '-')
	{
		if (arg[1] == 'a')
			all = 1;
		splitc (NULL, arg, ' ');
	}

	if ((all == 0) && (splitc (id, arg, ' ') == NULL))
	{
		admin_write (req, ADMIN_SHOW_STREAMURL_INVALID_SYNTAX, STREAMURL_SYNTAX);
		return 0;
	}
	
	if (all)
	{
		thread_mutex_lock (&info.source_mutex);
		
		while ((sourcecon = avl_traverse (info.sources, &trav)))
		{
			ptr = sourcecon->food.source->info.streamurl;
			sourcecon->food.source->info.streamurl = nstrdup (arg);
			sourcecon->food.source->info.udpseqnr++;
			nfree (ptr);
		}
		
		thread_mutex_unlock (&info.source_mutex);
		admin_write_line (req, ADMIN_SHOW_STREAMURL_ALL_SET, "Set all source urls to %s", arg);

	} else {
		sourcecon = find_source_with_id (atoi (id));

		if (sourcecon)
		{
			ptr = sourcecon->food.source->info.streamurl;
			sourcecon->food.source->info.streamurl = nstrdup (arg);
			sourcecon->food.source->info.udpseqnr++;
			nfree (ptr);
		} else {
			admin_write_line (req, ADMIN_SHOW_STREAMURL_INVALID_SOURCE_ID, "No source with id %d", atoi (id));
			return 0;
		}
		admin_write_line (req, ADMIN_SHOW_STREAMURL_SET, "Set source url for source %d to %s", atoi (id), arg);
	}
	return 1;
}
*/

#define DESCRIBE_SYNTAX "describe <id>\r\n"
int
com_describe (com_request_t *req)
{
	int id;
	connection_t *idcon;
	char *arg = com_arg (req);
	
	if (!req)
		return 0;
	else if (!arg || !arg[0])
	{
		admin_write_line (req, ADMIN_SHOW_DESCRIBE_INVALID_SYNTAX, DESCRIBE_SYNTAX);
		return 0;
	}
	
	id = atoi (arg);
	
	idcon = find_id (id);
	
	if (!idcon)
	{
		admin_write_line (req, ADMIN_SHOW_DESCRIBE_INVALID_ID, "No connection found with id [%d]", id);
		return 0;
	}

	switch (idcon->type)
	{
		case client_e:
			describe_client (req, idcon);
			break;
		case source_e:
			describe_source (req, idcon);
			break;
		case admin_e:
			describe_admin (req, idcon);
			break;
		default:
			admin_write_line (req, ADMIN_SHOW_DESCRIBE_INVALID_TYPE, "Unknown connection type on connection id [%d]", id);
			break;
	}
	return 1;
}

int
com_scheme (com_request_t *req)
{
	char *arg = com_arg (req);

	if (!req)
		return 0;

	if (!arg)
	{
		switch (req->con->food.admin->scheme)
		{
			case default_scheme_e:
				admin_write_line (req, ADMIN_SHOW_SCHEME_TYPE, "Using default output scheme");
				break;
			case html_scheme_e:
				admin_write_line (req, ADMIN_SHOW_SCHEME_TYPE, "Using html output scheme");
				break;
			case tagged_scheme_e:
				admin_write_line (req, ADMIN_SHOW_SCHEME_TYPE, "Using tagged output scheme");
				break;
		}
		return 1;

	} else {
		if (ntripcaster_strcasecmp (arg, "html") == 0)
			req->con->food.admin->scheme = html_scheme_e;
		else if (ntripcaster_strcasecmp (arg, "default") == 0)
			req->con->food.admin->scheme = default_scheme_e;
		else if (ntripcaster_strcasecmp (arg, "tagged") == 0)
			req->con->food.admin->scheme = tagged_scheme_e;
		else
		{
			admin_write_line (req, ADMIN_SHOW_SCHEME_UNKNOWN_SCHEME, "No such scheme [%s]", arg);
			return 0;
		}
	}

	admin_write_line (req, ADMIN_SHOW_SCHEME_CHANGED_TO, "Changed scheme to %s", arg);
	return 1;
}

#define PING_SYNTAX "ping <host[:port]>"
int
com_ping (com_request_t *req)
{
	char *arg = com_arg (req);
	char host[BUFSIZE], timebuf[BUFSIZE];
	int s, port = info.port[0];
	time_t before = 0;

	if (!arg || !arg[0])
		admin_write_line (req, ADMIN_SHOW_PING_INVALID_SYNTAX, PING_SYNTAX);
	
	host[0] = '\0';

	if (splitc (host, arg, ':') != NULL)
		port = atoi (arg);
	else
		strncpy(host, arg, BUFSIZE);
	
	admin_write_line (req, ADMIN_SHOW_PING_BEFORE_CONNECT, "Connecting to [%s] on port [%d] (will timeout after 30 seconds)", host, port);

	before = get_time ();

	if (!sock_valid (s = sock_connect_wto (host, port, 30)))
	{
		admin_write_line (req, ADMIN_SHOW_PING_CONNECT_FAILED, "Connect to [%s:%d] failed. Connect took %s", host, port, nntripcaster_time (get_time () - before, timebuf));
		return 0;
	}
	
	admin_write_line (req, ADMIN_SHOW_PING_AFTER_CONNECT, "Connected to [%s:%d] after %s...", host, port, nntripcaster_time (get_time () - before, timebuf));
	sock_write_line (s, "PING\r\n");

	admin_write_line (req, ADMIN_SHOW_PING_AFTER_RESPONSE, "Pong response from [%s:%d] after %s, terminating connection.", host, port, nntripcaster_time (get_time () - before, timebuf));
			  
	return 1;
}

int
com_resolv (com_request_t *req)
{
	char *arg = com_arg (req);
	char *out, outtarget[20];

	if (!arg || !arg[0])
	{
		admin_write_line (req, ADMIN_SHOW_RESOLV_NO_HOST, "Resolv what?");
		return 0;
	}

	if (isdigit ((int)(arg[0])) && isdigit ((int)(arg[strlen (arg) - 1])))
	{
		out = reverse (arg);
		if (!out)
			admin_write_line (req, ADMIN_SHOW_RESOLV_NO_SUCH_HOST, "No such host [%s]", arg);
		else
		{
			admin_write_line (req, ADMIN_SHOW_RESOLV_RESOLVED, "[%s] is [%s]", arg, out);
			nfree (out);
		}
	} else {
		if (!forward (arg, outtarget))
			admin_write_line (req, ADMIN_SHOW_RESOLV_NO_SUCH_HOST, "No such host [%s]", arg);
		else
			admin_write_line (req, ADMIN_SHOW_RESOLV_RESOLVED, "[%s] is [%s]", arg, outtarget);
	}
	return 1;
}

int
com_runtime (com_request_t *req)
{
  	admin_write_line (req, ADMIN_SHOW_RUNTIME_START, "Runtime Configuration:");
#if defined(HAVE_NANOSLEEP)
	admin_write_line (req, ADMIN_SHOW_RUNTIME_SLEEP_METHOD, "Using nanosleep() as sleep method");
#elif defined(HAVE_SELECT)
	admin_write_line (req, ADMIN_SHOW_RUNTIME_SLEEP_METHOD, "Using select() as sleep method");
#else
#ifdef _WIN32
	admin_write_line (req, ADMIN_SHOW_RUNTIME_SLEEP_METHOD, "Using Sleep() as sleep method");
#else
	admin_write_line (req, ADMIN_SHOW_RUNTIME_SLEEP_METHOD, "Using usleep() as sleep method - THIS MAY BE UNSAFE");
#endif
#endif
	admin_write_line (req, ADMIN_SHOW_RUNTIME_BACKLOG, "Using %d chunks of %d bytes for client backlog", CHUNKLEN, SOURCE_READSIZE);

	switch (info.resolv_type)
	{
		case solaris_gethostbyname_r_e:
			admin_write_line (req, ADMIN_SHOW_RUNTIME_RESOLV, "Using solaris own gethostbyname_r() and getaddrbyname_r(), which is good.");
			break;
		case linux_gethostbyname_r_e:
			admin_write_line (req, ADMIN_SHOW_RUNTIME_RESOLV, "Using linux own gethostbyname_r() and getaddrbyname_r(), which is good.");
			break;
		case standard_gethostbyname_e:
			admin_write_line (req, ADMIN_SHOW_RUNTIME_RESOLV, "Using standard gethostbyname() and getaddrbyname(), which might be dangerous cause it's not threadsafe!");
			break;
	}

#ifdef HAVE_SIGACTION
	admin_write_line (req, ADMIN_SHOW_RUNTIME_POSIX_SIGNALS, 
			  "Using posix signal interface to block all signals in threads that don't want them");
#endif

#ifdef PTHREAD_THREADS_MAX
	admin_write_line (req, ADMIN_SHOW_RUNTIME_THREADS, "System can create max %d threads", PTHREAD_THREADS_MAX);
#endif

#ifdef DEBUG_MEMORY
	admin_write_line (req, ADMIN_SHOW_RUNTIME_MEMORY_DEBUG, "Compiled with support for memory debugging");
#endif
#ifdef DEBUG_MUTEXES
	admin_write_line (req, ADMIN_SHOW_RUNTIME_MUTEX_DEBUG, "Compiled with support for mutex debugging");
#endif
#ifdef USE_CRYPT
	if(info.encrypt_passwords && strcmp(info.encrypt_passwords, "0"))
		admin_write_line (req, ADMIN_SHOW_RUNTIME_USE_CRYPT, "Using crypted passwords.");
#endif
#ifdef HAVE_LIBLDAP
	if(info.ldap_server)
		admin_write_line (req, ADMIN_SHOW_RUNTIME_HAVE_LIBLDAP, "Using LDAP for authentication.");
#endif
#ifdef HAVE_LIBWRAP
	admin_write_line (req, ADMIN_SHOW_RUNTIME_HAVE_LIBWRAP, "Using tcp wrapper support for incoming connections.");
#endif
	return 1;
}

#define RUN_SYNTAX "run <filename>"
int
com_run (com_request_t *req)
{
#ifdef HAVE_LIBPYTHON
	char *arg = com_arg (req);
	int fd;

	if (!arg || !arg[0]) {
		admin_write_line (req, ADMIN_SHOW_RUN_INVALID_SYNTAX, RUN_SYNTAX);
		return 0;
	}

	if ((fd = open_for_reading (arg)) == -1) {
		admin_write_line (req, ADMIN_SHOW_RUN_INVALID_FILE, "Could not open file [%s]", arg);
		return 0;
	}

	fd_close (fd);
	
	interpreter_python_eval_file (arg);
#else
	write_log (LOG_DEFAULT, "This binary has not been compiled with python interpreter support. You can't do that!");
#endif
	return 1;
}

#define SOCK_SYNTAX "sock <list|close>"
int
com_sock (com_request_t *req)
{
#ifndef DEBUG_SOCKETS
	write_log (LOG_DEFAULT, "This binary has not been compiled with DEBUG_SOCKETS, no socket information available");
	return 1;
#else
	char *arg = com_arg (req);
	char subcom[BUFSIZE];
	avl_traverser trav = {0};
	ntripcaster_socket_t *is;
	int num = 0;

	if (!arg || !arg[0]) {
		admin_write_line (req, ADMIN_SHOW_SOCK_INVALID_SYNTAX, SOCK_SYNTAX);	
		return 0;
	}
	
	if (splitc (subcom, arg, ' ') == NULL)
		strncpy(subcom, arg, BUFSIZE);

	if (ntripcaster_strcmp (subcom, "list") == 0) {
		
		item_write_formatted_line (req, ADMIN_SHOW_SOCK_START, list_start, 1, item_create ("Listing sockets", "%s", NULL));
		item_write_formatted_line (req, ADMIN_SHOW_SOCK_CAPTIONS, list_caption, 8, 
					   item_create ("socket descriptor", "%s", NULL),
					   item_create ("socket domain", "%s", NULL),
					   item_create ("socket type", "%s", NULL),
					   item_create ("socket protocol", "%s", NULL),
					   item_create ("blocking", "%s", NULL),
					   item_create ("keepalive", "%s", NULL),
					   item_create ("linger", "%s", NULL),
					   item_create ("busy", "%s", NULL));
		thread_mutex_lock (&sock_mutex);

		while ((is = avl_traverse (sock_sockets, &trav))) {
			item_write_formatted_line (req, ADMIN_SHOW_SOCK_ENTRY, list_item, 8,
						   item_create ("socket descriptor", "%d", &is->sock),
						   item_create ("socket domain", "%d", &is->domain),
						   item_create ("socket type", "%d", &is->type),
						   item_create ("socket protocol", "%d", &is->protocol),
						   item_create ("blocking", "%s", (is->blocking == SOCK_BLOCK) ? "yes" : "no"),
						   item_create ("keepalive", "%s", is->keepalive ? "yes" : "no"),
						   item_create ("linger", "%s", is->linger ? "yes" : "no"),
						   item_create ("busy", "%s", is->busy ? "yes" : "no"));
			num++;
		}

		thread_mutex_unlock (&sock_mutex);
		item_write_formatted_line (req, ADMIN_SHOW_SOCK_END, list_end, 1, item_create ("End of socket listing (%d listed)", "%d", &num));
	} else if (ntripcaster_strcmp (subcom, "close") == 0) {
		int socketnum = atoi (arg);

		is = sock_find (socketnum);

		if (!is) {
			admin_write_line (req, ADMIN_SHOW_SOCK_INVALID_SOCKET, "No such socket %d", socketnum);
			return 1;
		}

		if (sock_close (socketnum) == 0)
			admin_write_line (req, ADMIN_SHOW_SOCK_CLOSED, "Socket %d is closed", socketnum);
		else
			admin_write_line (req, ADMIN_SHOW_SOCK_FAILED, "Failed to close socket %d", socketnum);
	}
	return 1;
#endif
}

/* Opt stuff */
void 
zero_opts (int *opt, int maxelements)
{
	int i;

	if (!opt)
		return;

	for (i = 0; i < maxelements; i++)
		opt[i] = 0;
}

void 
set_opts (int *opt, const char *arg, conopt_t *opts, int maxelements)
{
	int i, k;

	if (!opt || !arg || !opts || maxelements < 0)
		return;

	for (i = 0; arg[i]; i++)
	{
		if (arg[i] == '-')
			continue;

		for (k = 0; k < maxelements; k++)
		{
			if (opts[k].opt == arg[i])
				opt[opts[k].arindex] = 1;
		}
	}
}

int
is_special_variable (char *variable_name)
{
	if (!variable_name)
		return 0;

	if ((strcmp (variable_name, "accessfilename") == 0) || (strcmp (variable_name, "usagefilename") == 0) ||
	    (strcmp (variable_name, "logfilename") == 0) || (strcmp (variable_name, "resolv_type") == 0))
		return 1;
	return 0;
}

void 
change_special_variable (com_request_t *req, set_element *s, char *variable_name, char *argument)
{
	if (!variable_name || !argument)
		return;

	if ((strcmp (variable_name, "accessfilename") == 0) || (strcmp (variable_name, "usagefilename") == 0) ||
	    (strcmp (variable_name, "logfilename") == 0))
	{
		change_variable (req, s, variable_name, argument);
		open_log_files ();
	} else if (strcmp (variable_name, "resolv_type") == 0)
	{
		int val = atoi (argument);
		switch (val)
		{
			case 1: 
#ifdef SOLARIS_RESOLV_OK
				admin_write_line (req, ADMIN_SHOW_SETTINGS_CHANGED_STRING, "Resolv style changed to solaris reentrant");
				change_variable (req, s, variable_name, argument);
#else
				admin_write_line (req, ADMIN_SHOW_SETTINGS_INVALID, "Solaris style reentrant resolv functions not available");
#endif
				break;
			case 2: 
#ifdef LINUX_RESOLV_OK
				info.resolv_type = 2;
				admin_write_line (req, ADMIN_SHOW_SETTINGS_CHANGED_STRING, "Resolv style changed to linux reentrant");
				change_variable (req, s, variable_name, argument);
#else
				admin_write_line (req, ADMIN_SHOW_SETTINGS_INVALID, "Linux style reentrant resolv functions not available");
#endif
				break;
			case 3:
				info.resolv_type = 3;
				admin_write_line (req, ADMIN_SHOW_SETTINGS_CHANGED_STRING, "Resolv style changed to standard (nonreentrant)");
				change_variable (req, s, variable_name, argument);
				break;
			default:
				admin_write_line (req, ADMIN_SHOW_SETTINGS_INVALID, "Only values 1,2 and 3 are valid for resolv_type");
				break;
		}
	}
}

void
change_variable (com_request_t *req, set_element *s, char *argument, char *arg)
{

	if (s->type == integer_e)
	{
		int oldval = *(int *)(s->setting);
		*(int *)(s->setting) = atoi (arg);
		admin_write_line (req, ADMIN_SHOW_SETTINGS_CHANGED_INT, "%s changed from %d to %d", argument, oldval, *(int *)(s->setting));
	} else if (s->type == real_e)
	{
		double oldval = *(double *)(s->setting);
		*(double *)(s->setting) = atof (arg);
		admin_write_line (req, ADMIN_SHOW_SETTINGS_CHANGED_REAL, "%s changed from %f to %f", argument, oldval, *(double *)(s->setting));
	} else {
		char *oldval = *(char **)(s->setting);
		*((char **)(s->setting)) = nstrdup (arg);
		admin_write_line (req, ADMIN_SHOW_SETTINGS_CHANGED_STRING, "%s changed from %s to [%s]", argument, nullcheck_string (oldval), *((char **)(s->setting)));
		nfree (oldval);
	}
}

char *
variable_to_string (char *varname)
{
	set_element *s;

	if (!varname)
		return nstrdup ("(null)");
	
	s = find_set_element (varname, admin_settings);

	if (!s) 
		return nstrdup ("(not found)");

	if (s->type == integer_e)
		return ntripcaster_itoa (*(int *)(s->setting));
	else if (s->type == real_e)
		return ntripcaster_itoa (*(double *)(s->setting));
	else
		return nstrdup (*(char **)(s->setting));
}

	
