/* ntripcastertypes.h
 * - General Type Declarations
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

#ifndef __NTRIPCASTER_TYPES_H
#define __NTRIPCASTER_TYPES_H

/* rtsp. ajd */
#include <limits.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif
#if (defined (WORDS_BIGENDIAN)) || (defined (__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN))
 #define RTP_BIG_ENDIAN 1
 #undef RTP_LITTLE_ENDIAN
#else
 #define RTP_LITTLE_ENDIAN 1
 #undef RTP_BIG_ENDIAN
#endif
#ifdef __STDC__
#define MAX32U 0xFFFFFFFFU
#else
#define MAX32U 0xFFFFFFFF
#endif
#define MAX32  0x8FFFFFFF

/* 32 bit machines. rtsp. ajd */
#if ULONG_MAX == MAX32U
typedef unsigned long  u_int32;
/* 16 bit machines */
#elif ULONG_MAX == 0xFFFF
typedef unsigned long  u_int32;
/* 64 bit machines */
#else
typedef unsigned int u_int32;
#endif

/* rtsp. ajd */
typedef enum {unknown_client_e = -1, http_client_e = 0, rtsp_client_e = 1, pulling_client_e = 2, rtsp_client_listener_e = 3 } client_type_t;
typedef enum {unknown_source_e = -1, http_source_e = 0, rtsp_client_source_e = 1, pulling_source_e = 2, nontrip_source_e = 3 } source_type_t;
typedef enum {unknown_protocol_e = -1, tcp_e = 0, udp_e = 1, rtp_e = 2, http_e = 3, rtsp_e = 4, ntrip1_0_e = 5, ntrip2_0_e = 6} protocol_t;
typedef enum {gnss_data_e = 0, gnss_sourcetable_e = 1 } content_type_t;
typedef enum {not_chunked_e = 0, chunked_e = 1 } transfer_encoding_t;

typedef enum contype_e {client_e = 0, source_e = 1, admin_e = 2, unknown_connection_e = 3} contype_t;
typedef enum { deny = 0, allow = 1, all = 2 } acltype_t;
typedef enum scheme_e {html_scheme_e = 0, default_scheme_e = 1, tagged_scheme_e = 2} scheme_t;
typedef enum { conf_file_e = 1, log_file_e = 2, template_file_e = 3, var_file_e = 4} filetype_t;
typedef enum { linux_gethostbyname_r_e = 1, solaris_gethostbyname_r_e = 2, standard_gethostbyname_e = 3 } resolv_type_t;
typedef enum { relay_pull_e = 1, relay_nontrip_e = 2 } relay_type_t;

typedef void ntripcaster_function();
typedef int ntripcaster_int_function();

typedef const char *htf;
typedef htf HttpFunction();
typedef avl_tree vartree_t;
typedef int wid_t;

typedef enum type_e {integer_e = 1, real_e = 2, string_e = 3, function_e = 4, unknown_type_e = -1 } type_t;
// original #define BUFSIZE 8192
#define BUFSIZE 1000
#define FILE_LINE_BUFSIZE 100000
//#define CHUNKLEN 64 /*(undefined influence) original 64*/
// original #define MAXMETADATALENGTH (16 * 256)
//#define MAXMETADATALENGTH 100
// original #define SOURCE_BUFFSIZE 8192
//#define SOURCE_BUFFSIZE 1000
#define SOURCE_READSIZE 100 /* packet size which will be send to client */
/* #define SOURCE_READSIZE (SOURCE_BUFFSIZE - MAXMETADATALENGTH) // Please be careful here... */
#define MAXLISTEN 5 /* max number of listening ports */

/* rtsp. ajd */
#define MAXUDPSIZE 1600
#define DATAGRAMBUFSIZE 8
#define CHUNKLEN 32 // was 64. ajd

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

#ifndef _WIN32
typedef int SOCKET;
#define DIR_DELIMITER '/'
#else
#define DIR_DELIMITER '\\'
#define W_OK 2
#define R_OK 3
#endif

#ifndef HAVE_SOCK_T
typedef int sock_t;
#else
typedef sock_t SOCKET;
#endif

typedef struct http_parsable_commandsSt
{
  char *name;         /* What will be substituted */
  HttpFunction *func; /* Function to call for the substitution
		       * This function should return a pointer
		       * to where the parser should jump */
} http_parsable_t;

typedef struct http_variableSt
{
  char *name;
  type_t type;
  void *valueptr;
}http_variable_t;

typedef struct varpair_St
{
	char *name;
	char *value;
} varpair_t;

typedef struct acl_controlSt
{
	avl_tree *all;  /* tree of restrict_t's */
} acl_control_t;

typedef struct ntrip_method_St
{
	char *method;
	protocol_t protocol;
	ntripcaster_function *login_func;
	ntripcaster_int_function *execute_func;
} ntrip_method_t;

typedef struct ntrip_request_St {
	ntrip_method_t *method;
	char path[BUFSIZE];
	char host[BUFSIZE];
	int port;
	int cseq;
	long int sessid;
} ntrip_request_t;

/*
typedef struct request_St
{
	char path[BUFSIZE];
	char host[BUFSIZE];
	char userID[BUFSIZE];		// user:password (base64 encoded); for mountpoints with need for authentication. ajd
	int port;
} request_t;
*/

/*
typedef struct relay_request_St
{
	char path[BUFSIZE];
	char host[BUFSIZE];
	char userID[BUFSIZE];		// user:password (base64 encoded); for mountpoints with need for authentication. ajd
	int port;
	char localmount[BUFSIZE];
} relay_request_t;
*/

typedef struct alias_St
{
	ntrip_request_t *name;
	ntrip_request_t *real;
	char localmount[BUFSIZE];       /* Name of the local mount (both for push and pull) */
	char userID[BUFSIZE];
	int pending; // added. ajd
} alias_t;

typedef struct restrict_St
{
	unsigned long int id;
	char *mask;
	acltype_t type;	/* is this restriction an allow or a deny? */
	int num_short_connections; // added. ajd
} restrict_t;

typedef struct chunkSt
{
	char data[SOURCE_READSIZE];
	int len;
	int clients_left;
} chunk_t;

typedef struct http_chunkSt {
	char buf[20]; // to store hex length. ajd
	int off; // store offset in buf.
	int left;
	int finish;
} http_chunk_t;

typedef struct statistics_St
{
	unsigned long int read_bytes;   /* Bytes read from encoder(s) */
  unsigned long int read_kilos;   /* Kilos read from encoder(s) */
	//unsigned long int read_megs;    /* Megs read from encoder(s) */
	unsigned long int write_bytes;  /* Bytes written to client(s) */
  unsigned long int write_kilos;  /* Kilos written to client(s) */
	//unsigned long int write_megs;   /* Megs written to clients */
	unsigned long int client_connections; /* Number of connects from clients */
	unsigned long int source_connections; /* Number of connects from sources */
	unsigned long int client_connect_time; /* Total sum of the time each client has been connected (minutes) */
	unsigned long int source_connect_time; /* Total sum of the time each source has been connected (minutes) */
} statistics_t;

/* audiocast stuff */
typedef struct audiocast_St {
	char *name;		/* Name of Server */
//	char *genre;		/* Genre of music */
//	int bitrate;		/* Bitrate of server */
//	char *url;		/* Url for server */
	char *mount;		/* Name of this particular channel */
//	char *description;	/* Description of this stream */
//	char *streammimetype;   /* MIMEtype of stream (defaults to something smart) */
//	char *contentid;
//	int public;		/* List on the directory? */
} audiocast_t;

/*
typedef struct relay_idSt
{
	char *host;
	int id;
} relay_id_t;
*/

typedef struct sourcetable_St {
	int length;
//	int show_length;
	int lines;
	avl_tree *tree;
//	avl_tree *net_tree;
} sourcetable_t;
/*
typedef struct sourcetable_entry_St
{
	char *type;
	char *mount;
	char *net;
	char *line;
	int is_connected;
} sourcetable_entry_t;
*/
/*
typedef struct songinfo_St
{
	char *streamurl;
	char *streamtitle;
	char *streammsg;
	long int streamlength;
	unsigned long int udpseqnr;
} songinfo_t;
*/

typedef struct source_St {
	int connected;                 /* Is connected? */
	source_type_t type;            /* Encoder, or pulling redirect */
//	protocol_t protocol;
//	mutex_t mutex;
	audiocast_t audiocast;
	avl_tree *clients;             /* Tree of clients */
	icethread_t thread;              /* Pointer to running thread */
	statistics_t stats;
	unsigned long int num_clients; /* Take a wild guess */
	chunk_t chunk[CHUNKLEN];
	int cid;
//	char *dumpfile;
//	int dumpfd;
//	avl_tree *relay_tree;
	int priority;                  /* order for getting the default mount in the sourcetree */
//	songinfo_t info;

//	char *source_agent; // added. ajd
//	sourcetable_entry_t *ste;
//	int pending_clients; // number of clients for that source not in pool yet. ajd

} source_t;

typedef struct client_St {
//	unsigned int use_udp:1;
//	unsigned int use_icy:1;

	int errors;             /* Used at first to mark position in buf, later to mark error */
	int offset;
	int cid;
	int alive;
	client_type_t type;
	unsigned long int write_bytes;	/* Number of bytes written to client */
	int virgin;			/* Need sync? */
	source_t *source;        /* Pointer back to the source (to avoid having to find it) */
//	int use_icy_metadata;
//	unsigned long int metadataoffset;
//	unsigned long int metadatalen;
//	unsigned long int metadatawritten;
//	unsigned long int udpseqnr;
} client_t;

typedef struct admin_St {
	unsigned int status:1; /* Show status information ? */ 
	unsigned int oper:1;
	unsigned int tailing:1;
	unsigned int alive:1;
	int commands;          /* Number of issued commands */
	icethread_t thread;       /* Pointer to running thread */
	int debuglevel;
	scheme_t scheme;
} admin_t;

/*
typedef struct dirSt {
	int counter;     // Number of touches (or number of errors if id < 0)
	int touches;
	int id;          // our id on this server
	char *host;      // Hostname of the directory server
	int port;        // What port for the directory server?
	protocol_t type; // Standard xaudiocast or icy?
	char *path;      // Path to script
	time_t touchtime;
} directory_server_t;
*/

typedef struct nontripsource_St { // nontrip. ajd
	int port;
	char *mount;
	SOCKET listen_sock;
} nontripsource_t;


typedef struct rtp_datagram_St { // rtsp. ajd
#ifdef RTP_BIG_ENDIAN
	unsigned int version:2;   /* protocol version */
	unsigned int p:1;         /* padding flag */
	unsigned int x:1;         /* header extension flag */
	unsigned int cc:4;        /* CSRC count */
	unsigned int m:1;         /* marker bit */
	unsigned int pt:7;        /* payload type */
#else
	unsigned int cc:4;        /* CSRC count */
	unsigned int x:1;         /* header extension flag */
	unsigned int p:1;         /* padding flag */
	unsigned int version:2;   /* protocol version */
	unsigned int pt:7;        /* payload type */
	unsigned int m:1;         /* marker bit */
#endif
	unsigned int seq:16;      /* sequence number */
	u_int32 ts;               /* timestamp */
	u_int32 ssrc;             /* synchronization source */
//    u_int32 csrc[1];          /* optional CSRC list */

/****** memory alignment??? rtsp. ajd ******/
	char data[MAXUDPSIZE];
	int data_len;
} rtp_datagram_t;

typedef struct rtp_St {
	struct timeval sendtime;
	rtp_datagram_t *datagram;

// in host byte order to save computations. rtsp. ajd
	unsigned int host_seq:16;
	unsigned int last_host_seq:16;
	u_int32 host_ts;

	int virgin;
	rtp_datagram_t *datagrambuf[DATAGRAMBUFSIZE];
	int offset;
} rtp_t;

typedef struct udpbuffersSt {
	mutex_t       buffer_mutex;
	unsigned char buffer[1024];
	int           len;
	SOCKET        sock;
	time_t        lastactive;
	time_t        lastsend;
	unsigned int  seq;   /* remote sequence number */
	unsigned int  ssrc;  /* remote ssrc number */
} udpbuffers_t;

typedef struct connectionSt {
	contype_t type;
	union {
		client_t *client;
		source_t *source;
		admin_t *admin;
	} food;
	unsigned long int id;  /* Session unique connection id */
	struct sockaddr_in *sin;
	socklen_t sinlen;
	SOCKET sock;
	time_t connect_time;
	char *host;
	char *hostname;
	udpbuffers_t *udpbuffers;
	vartree_t *headervars;
	char *group; // added to identifiy group of connecting user. ajd
	restrict_t *res; // added. ajd
	int ghost; // if it's a ghost connection (no log output to access file). ajd
	nontripsource_t *nontripsrc; // nontrip. ajd

	/* rtsp. ajd */
	protocol_t com_protocol;
	protocol_t data_protocol;
	transfer_encoding_t trans_encoding;
	long int session_id;
	http_chunk_t *http_chunk; // rtsp. used for chunked transfer encoding. ajd
	rtp_t *rtp;
} connection_t;

typedef struct rtsp_session_St {
	long int id;
	int state;
	long int creation_time;
	long int timeout_time;
	int server_port;
	int client_port;
	int transport_ttl;
	connection_t *con;
	SOCKET udp_sockfd;
	char *mount;
	char *transport_ip;
} rtsp_session_t;

typedef struct relay_St {
	ntrip_request_t req;            /* Host, port and path of remote machine */
	relay_type_t type;              /* Type of relay (pushing or pulling) */
	char *localmount;       /* Name of the local mount (both for push and pull) */
	char *userID;
	connection_t *con;              /* Connection struct of a connected relay (or NULL if not connected) */
	int reconnects;                 /* How many times have we tried reconnecting? */
	time_t last_reconnect;          /* When was the last reconnect? */
//	char *password;                 /* Only for push, encoder password */
//	protocol_t protocol;            /* Protocol in/to use */
//	audiocast_t audiocast;          /* Audiocast headers (only push) */
	int reconnect_now;		/* Tell reconnector to reconnect this now */
	int pending;	// connection in progress ?. ajd
} relay_t;

typedef struct {
  
	/* Global stuff */
	char *runpath;			/* the argv[0] */
	int port[MAXLISTEN];		/* Listen to what port(s)? */
	SOCKET listen_sock[MAXLISTEN];	/* Socket to listen to */
	SOCKET listen_sock_udp[MAXLISTEN];	/* Socket to listen to */

	/* Where ntripcaster lives */
	char *etcdir;		/* Name of config file directory */
	char *logdir;
	char *vardir;   // added. ajd
	char *templatedir;

	/* Encoder stuff */
	avl_tree *sources;	/* Source array */
	unsigned long int num_sources;	/* Encoders connected */
	unsigned long int max_sources;	/* Maximal number of encoders */
	char *encoder_pass;	/* Password to verify NTRIP 1.0 encoder */
	char *default_sourceopts; /* Default output for 'sources' */

#ifdef USE_CRYPT
	char *encrypt_passwords; /* Passwords should be encrypted */
#endif /* USE_CRYPT */

	/* Admin stuff */
	char *oper_pass;	/* Operator password (this one can do it all) */
	avl_tree *admins;	/* Admin array */
	unsigned long int num_admins;		/* Number of connected admins */
	unsigned long int max_admins;		/* Maximal number of admins */
	char *remote_admin_pass;	/* Password for remote administration */

	/* Directory server stuff
	avl_tree *d_servers;	// Directory servers to rat on
	int touch_freq;	        // Frequency to update directory
				 //  (same touch_freq for all directories)
	unsigned long int max_directories;	// Maximal number of directory servers
	time_t directorylasttime;		// Time of last touch()
*/

	/* Udp info stuff */
	time_t udpupdatelasttime;
	int udpupdatetime;

	/* Misc stuff */
	char *staticdir;	/* Name of static content directory */
	char *configfile;	/* Name of configuration file */
	char *userfile;
	char *groupfile;
	char *watchfile;
	char *pidfile;
	char *client_mountfile;
	char *source_mountfile; // added. ajd
	char *prompt;

/* added. ajd*/
	char *sourcetablefile; // name of sourcetable file. ajf
//	char *sourcetableutdfile; // path and name. ajd
//	char *sourcetablexxxfile; // path and name. ajd
	
	/* Logfiles */
	char *logfilename;	/* Name of default log file */
	int logfile;		/* File descriptor for that */
	char *usagefilename;    /* Name of usage log file   */
	int usagefile;          /* File descriptor for that */
	char *accessfilename;   /* Access file name */
	int accessfile;         /* File descriptor for that */
//	char *statsfilename;	/* Name of stats logfile */
//	char *statshtmlfilename; /* Like previous, but in html format */
//	int statsfile;	        /* File to dump stats to */
//	int statstime;		/* How long between dumps to the stats file */

//	char *clisrcfilename;		/* name for client/source count logfile. ajd */
//	int clisrcfile;		/* file descriptor for client/source count logfile. ajd */

//	unsigned long int statscount;		/* counts invocation of function stats_write(...). ajd */

	time_t statslasttime;	/* Time since last dump */
	long server_start_time; /* The time the server started */
	time_t statuslasttime;
	int statustime;
	char *myhostname;	/* NULL unless we want to bind to specific ip */
	char *server_name;	/* Server name */

	char *version; // added. ajd
	char *ntripversion; // added. ajd
	char *ntripinfourl; // added. ajd
	char *name; // added. ajd
	char *operator; // added. ajd
	char *operatorurl; // added. ajd
	
	char timezone[50]; // added. ajd

	int console_mode;	/* CONSOLE_BACKGROUND, CONSOLE_LOG or CONSOLE_ADMIN */

	int detach;
	int throttle_on;
	double throttle;
	double bandwidth_usage;
	double sleep_ratio;    /* 0.0 - 1.0, how much of the time between reads should a source sleep  */
	
	int reverse_lookups;
	int force_servername;
	int mount_fallback;

//	icethread_t main_thread;
	mythread_t *main_thread;

#ifndef _WIN32
	pthread_attr_t defaultattr;
#endif
	mutex_t source_mutex;
	mutex_t client_mutex; // added. ajd
	mutex_t admin_mutex;
//	mutex_t directory_mutex;
	mutex_t alias_mutex;
	mutex_t misc_mutex;
//	mutex_t mount_mutex;
	mutex_t hostname_mutex;
	mutex_t acl_mutex;
	mutex_t double_mutex;
	mutex_t thread_mutex;
	mutex_t mutex_mutex;
	mutex_t relay_mutex;

	/* rtsp. ajd */
	mutex_t session_mutex;
	mutex_t header_mutex;

	mutex_t sourcetable_mutex;
	mutex_t logfile_mutex;

#ifdef DEBUG_MEMORY
	mutex_t memory_mutex;
	avl_tree *mem;
#endif

	mutex_t resolvmutex;
	resolv_type_t resolv_type;

	/* How to deal with clients when no encoder is connected
	 * Negative value means keep them forever.
	 * 0 means kick them out instantly
	 * Other values are keep them X seconds */
	int client_timeout; 

	avl_tree *clients;
//	char *client_pass;
	unsigned long int num_clients;
	unsigned long int max_clients;
	unsigned long int max_ip_connections;
	unsigned long int max_clients_per_source;

	avl_tree *threads;
	avl_tree *mutexes;
	avl_tree *relays; /* Connected and not connected relays */
//	avl_tree *pending_alias; // pending alias connection requests. ajd

	long int threadid;
	long int mutexid;
	unsigned long int id;

	avl_tree *aliases;
	int transparent_proxy;

	int kick_relays;   /* Kick relays when they are not relaying to any client (recommended) */
	int relay_reconnect_time; /* Seconds to wait before reconnecting dead relay */
	int relay_reconnect_tries; /* Number of tries before giving up reconnect (-1 means go on forever) */
	int kick_clients;  /* Kick clients when their source dies, instead of moving them to the default */

	avl_tree *my_hostnames;

	/* ACL controll */
	avl_tree *all_acl;
	avl_tree *admin_acl;
	avl_tree *source_acl;
	avl_tree *client_acl;
	
	int policy;
	int allow_http_admin;

	/* Statistics */
	statistics_t hourly_stats;
	statistics_t daily_stats;
	statistics_t total_stats;

	/* Server meta info */
	char *location;
	char *rp_email;
	char *url;

	int streamurllock;

	char *streamtitle;
	char *streamurl;

	/* Templates man */
	char *streamtitletemplate;
	char *nametemplate;
	char *descriptiontemplate;

#ifdef HAVE_LIBLDAP
  /* LDAP */
	char * ldap_server;
	char * ldap_uid_prefix;
	char * ldap_people_context;
#endif /* HAVE_LIBLDAP */

	int use_meta_data;

	unsigned long int metainterval;

	int consoledebuglevel;
	int logfiledebuglevel;

	sourcetable_t sourcetable;
	avl_tree *rtsp_sessions;
	int session_timeout; // seconds

	char date[20];

	avl_tree *nontripsources; // nontrip. ajd

} server_info_t;

#define COMREQUEST_NUMARGS 10

typedef struct com_requestSt
{
	wid_t wid;           /* Request includes a window id, or -1 for none */
	connection_t *con;   /* Pointer to requester connection */
	char *arg;           /* Pointer to requester argument, might be NULL */

/* Char pointer array to strings each of that contain a list of arguments of a
	certain type. (type users, mounts, groups,...). ajd */
	char *typearg[COMREQUEST_NUMARGS];

	//char url[BUFSIZE];  // added to keep server url with port. not used. ajd

} com_request_t;

typedef struct list_element_St {
	void *data;
	struct list_element_St *next;
} list_element_t;

typedef struct list_St {
	int size;
	list_element_t *head;
	list_element_t *tail;
} list_t;

typedef struct list_enum_St {
	list_t *list;
	list_element_t *next;
} list_enum_t;

typedef struct string_buffer_St {
	int size;
	int pos;
	char *buf;
} string_buffer_t;

#endif
