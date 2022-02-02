%{
/*
 * Configuration file parser for querierd
 *
 * Written originally for mrouted by Bill Fenner, NRL, 1994
 * Adapted to querierd by Joachim Wiberg, Westermo, 2022
 *
 * cfparse.y,v 3.8.4.30 1998/03/01 01:48:58 fenner Exp
 */
#include <stdio.h>
#include <stdarg.h>
#include "defs.h"
#include <netdb.h>
#include <ifaddrs.h>

/*
 * Local function declarations
 */
static void        fatal(const char *fmt, ...);
static void        warn(const char *fmt, ...);
static void        yyerror(char *s);
static char       *next_word(void);
static int         yylex(void);
int                yyparse(void);

static FILE *fp;

static int lineno;
static int state;

static struct uvif *v;
static struct uvif scrap;

%}

%union
{
    int       num;
    char     *ptr;
    uint32_t  addr, group;
};

%token QUERY_INTERVAL QUERY_LAST_MEMBER_INTERVAL QUERY_RESPONSE_INTERVAL
%token IGMP_ROBUSTNESS ROUTER_TIMEOUT ROUTER_ALERT
%token NO PHYINT
%token DISABLE ENABLE IGMPV1 IGMPV2 IGMPV3 STATIC_GROUP
%token <num> BOOLEAN
%token <num> NUMBER
%token <ptr> STRING
%token <addr> ADDR GROUP
%type  <addr> interface

%start conf

%%

conf	: stmts
	;

stmts	: /* Empty */
	| stmts stmt
	;

stmt	: error
	| NO PHYINT		{ config_set_ifflag(VIFF_DISABLED); }
	| PHYINT interface
	{
	    state++;

	    v = config_find_ifaddr($2);
	    if (!v) {
		if ($2 != 0)
		    warn("phyint %s not available, continuing ...", inet_fmt($2, s1, sizeof(s1)));
		v = &scrap;
	    }
	}
	ifmods
	| NO ROUTER_ALERT
	{
	    router_alert = 0;
	}
	| ROUTER_ALERT BOOLEAN
	{
	    router_alert = $2;
	}
	| ROUTER_TIMEOUT NUMBER
	{
	    if ($2 < 1 || $2 > 1024)
		fatal("Invalid multicast router timeout [1,1024]: %d", $2);
	    router_timeout = $2;
	}
	| QUERY_INTERVAL NUMBER
	{
	    if ($2 < 1 || $2 > 1024)
		fatal("Invalid multicast query interval [1,1024]: %d", $2);
	    igmp_query_interval = $2;
	}
	| QUERY_RESPONSE_INTERVAL NUMBER
	{
	    if ($2 < 1 || $2 > 1024)
		fatal("Invalid multicast query response interval [1,1024]: %d", $2);
	    igmp_response_interval = $2;
	}
	| QUERY_LAST_MEMBER_INTERVAL NUMBER
	{
	    if ($2 < 1 || $2 > 1024)
		fatal("Invalid multicast query interval [1,1024]: %d", $2);
	    igmp_last_member_interval = $2;
	}
	| IGMP_ROBUSTNESS NUMBER
	{
	    if ($2 < 2 || $2 > 10)
		fatal("Invalid multicast robustness value [2,10]: %d", $2);
	    igmp_robustness = $2;
	}
	;

ifmods	: /* empty */
	| ifmods ifmod
	;

ifmod	: DISABLE		{ v->uv_flags |= VIFF_DISABLED; }
	| ENABLE		{ v->uv_flags &= ~VIFF_DISABLED; }
	| IGMPV1		{ v->uv_flags &= ~VIFF_IGMPV2; v->uv_flags |= VIFF_IGMPV1; }
	| IGMPV2		{ v->uv_flags &= ~VIFF_IGMPV1; v->uv_flags |= VIFF_IGMPV2; }
	| IGMPV3		{ v->uv_flags &= ~VIFF_IGMPV1; v->uv_flags &= ~VIFF_IGMPV2; }
	| STATIC_GROUP GROUP
	{
	    struct listaddr *a;

	    a = calloc(1, sizeof(struct listaddr));
	    if (!a) {
		fatal("Failed allocating memory for 'struct listaddr'");
		return 0;
	    }

	    a->al_addr  = $2;
	    a->al_pv    = 2;	/* IGMPv2 only, no SSM */
	    a->al_flags = NBRF_STATIC_GROUP;
	    time(&a->al_ctime);

	    TAILQ_INSERT_TAIL(&v->uv_static, a, al_link);
	}
	;

interface: ADDR
	{
	    $$ = $1;
	}
	| STRING
	{
	    struct uvif *v;

	    /*
	     * Looks a little weird, but the orig. code was based around
	     * the addresses being used to identify interfaces.
	     */
	    v = config_find_ifname($1);
	    if (!v) {
		warn("No such interface %s, skipping ...", $1);
		$$ = 0;
	    } else
		$$ = v->uv_lcl_addr;
	}
	;

%%

static void fatal(const char *fmt, ...)
{
    va_list ap;
    char buf[MAXHOSTNAMELEN + 100];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    logit(LOG_ERR, 0, "%s:%d: %s", config_file, lineno, buf);
}

static void warn(const char *fmt, ...)
{
    va_list ap;
    char buf[200];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    logit(LOG_WARNING, 0, "%s:%d: %s", config_file, lineno, buf);
}

static void yyerror(char *msg)
{
    logit(LOG_ERR, 0, "%s:%d: %s", config_file, lineno, msg);
}

static char *next_word(void)
{
    static char buf[1024];
    static char *p = NULL;
    char *q;

    while (1) {
        if (!p || !*p) {
            lineno++;
            if (fgets(buf, sizeof(buf), fp) == NULL)
                return NULL;
            p = buf;
        }

        while (*p && (*p == ' ' || *p == '\t'))	/* skip whitespace */
            p++;

        if (*p == '#') {
            p = NULL;		/* skip comments */
            continue;
        }

        q = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n')
            p++;		/* find next whitespace */
        *p++ = '\0';	/* null-terminate string */

        if (!*q) {
            p = NULL;
            continue;	/* if 0-length string, read another line */
        }

        return q;
    }
}

/*
 * List of keywords.  Must have an empty record at the end to terminate
 * list.  If a second value is specified, the first is used at the beginning
 * of the file and the second is used while parsing interfaces (e.g. after
 * the first "phyint" or "tunnel" keyword).
 */
static struct keyword {
	char	*word;
	int	val1;
	int	val2;
} words[] = {
	{ "multicast-query-interval", QUERY_INTERVAL, 0 },
	{ "multicast-query-response-interval", QUERY_RESPONSE_INTERVAL, 0 },
	{ "multicast-query-last-member-interval", QUERY_LAST_MEMBER_INTERVAL, 0 },
	{ "multicast-robustness",    IGMP_ROBUSTNESS, 0 },
	{ "multicast-router-timeout", ROUTER_TIMEOUT, 0 },
	{ "no",                 NO, 0 },
	{ "phyint",		PHYINT, 0 },
	{ "iface",		PHYINT, 0 },
	{ "disable",		DISABLE, 0 },
	{ "enable",		ENABLE, 0 },
	{ "router-alert",	ROUTER_ALERT, 0 },
	{ "igmpv1",		IGMPV1, 0 },
	{ "igmpv2",		IGMPV2, 0 },
	{ "igmpv3",		IGMPV3, 0 },
	{ "static-group",	STATIC_GROUP, 0 },
	{ NULL,			0, 0 }
};


static int yylex(void)
{
    struct keyword *w;
    uint32_t addr, n;
    char *q;

    q = next_word();
    if (!q)
        return 0;

    for (w = words; w->word; w++) {
        if (!strcmp(q, w->word))
            return (state && w->val2) ? w->val2 : w->val1;
    }

    if (!strcmp(q,"on") || !strcmp(q,"yes")) {
        yylval.num = 1;
        return BOOLEAN;
    }

    if (!strcmp(q,"off") || !strcmp(q,"no")) {
        yylval.num = 0;
        return BOOLEAN;
    }

    if (sscanf(q,"%[.0-9]/%u%c",s1,&n,s2) == 2) {
	addr = inet_parse(s1,1);
        /* fall through to returning STRING */
    }

    if (sscanf(q,"%[.0-9]%c",s1,s2) == 1) {
	addr = inet_parse(s1, 4);
        if (addr != 0xffffffff) {
	    if (inet_valid_host(addr)) {
		yylval.addr = addr;
		return ADDR;
	    }
	    if (inet_valid_group(addr)) {
		yylval.addr = addr;
		return GROUP;
	    }
        }
    }

    if (sscanf(q,"0x%8x%c", &n, s1) == 1) {
        yylval.addr = n;
        return ADDR;
    }

    if (sscanf(q,"%u%c",&n,s1) == 1) {
        yylval.num = n;
        return NUMBER;
    }

    yylval.ptr = q;

    return STRING;
}

void config_vifs_from_file(void)
{
    TAILQ_INIT(&scrap.uv_static);
    TAILQ_INIT(&scrap.uv_groups);

    state = 0;
    lineno = 0;

    fp = fopen(config_file, "r");
    if (!fp) {
        if (errno != ENOENT)
            logit(LOG_ERR, errno, "Cannot open %s", config_file);
        return;
    }

    yyparse();

    fclose(fp);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "cc-mode"
 * End:
 */
