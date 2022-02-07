/*
 * Parts of this program has been derived from mrouted.  It is covered
 * by the license in the accompanying file named "LICENSE".
 */

#include "defs.h"
#include <err.h>
#include <getopt.h>
#include <paths.h>
#include <fcntl.h>
#include <poll.h>

int haveterminal = 1;
int running = 1;
int use_syslog = 1;
time_t querierd_init_time;

char *config_file = NULL;
char *pid_file    = NULL;
char *sock_file   = NULL;

char *ident       = PACKAGE_NAME;
char *prognm      = NULL;
const char *versionstring = "querierd version " PACKAGE_VERSION;

#define NHANDLERS	5
static struct ihandler {
    int fd;			/* File descriptor	*/
    ihfunc_t func;		/* Function to call	*/
} ihandlers[NHANDLERS];
static int nhandlers = 0;

/*
 * Forward declarations.
 */
static void timer(void*);
static void handle_signals(int, void *);
static int  check_signals(void);
static int  timeout(int);
static void cleanup(void);

int register_input_handler(int fd, ihfunc_t func)
{
    int i;

    if (nhandlers >= NHANDLERS)
	return -1;

    for (i = 0; i < NHANDLERS; i++) {
	if (ihandlers[i].func)
	    continue;

	ihandlers[i].fd   = fd;
	ihandlers[i].func = func;
	nhandlers++;

	return 0;
    }

    return -1;
}

void deregister_input_handler(int fd)
{
    int i;

    for (i = 0; i < NHANDLERS; i++) {
	if (ihandlers[i].fd != fd)
	    continue;

	ihandlers[i].fd   = 0;
	ihandlers[i].func = NULL;
	nhandlers--;

	return;
    }
}

static int compose_paths(void)
{
    /* Default .conf file path: "/etc" + '/' + "pimd" + ".conf" */
    if (!config_file) {
	size_t len = strlen(SYSCONFDIR) + strlen(ident) + 7;

	config_file = malloc(len);
	if (!config_file) {
	    logit(LOG_ERR, errno, "Failed allocating memory, exiting.");
	    exit(1);
	}

	snprintf(config_file, len, _PATH_QUERIERD_CONF, ident);
    }

    /* Default is to let pidfile() API construct PID file from ident */
    if (!pid_file)
	pid_file = strdup(ident);

    return 0;
}

static int usage(int code)
{
    printf("Usage: %s [-himnpsv] [-f FILE] [-i NAME] [-p FILE]\n"
	   "\n"
	   "  -f, --config=FILE        Configuration file to use, default ident: /etc/%s.conf\n"
	   "  -h, --help               Show this help text\n"
	   "  -i, --ident=NAME         Identity for syslog, .cfg & .pid file, default: %s\n"
	   "  -l, --loglevel=LEVEL     Set log level: none, err, notice (default), info, debug\n"
	   "  -n, --foreground         Run in foreground, do not detach from controlling terminal\n"
	   "  -p, --pidfile=FILE       File to store process ID for signaling daemon, default ident\n"
	   "  -s, --syslog             Log to syslog, default unless running in --foreground\n"
	   "  -v, --version            Show %s version\n", prognm, ident, PACKAGE_NAME, prognm);

    printf("\nBug report address: %-40s\n", PACKAGE_BUGREPORT);
#ifdef PACKAGE_URL
    printf("Project homepage: %s\n", PACKAGE_URL);
#endif

    return code;
}

static char *progname(char *arg0)
{
       char *nm;

       nm = strrchr(arg0, '/');
       if (nm)
	       nm++;
       else
	       nm = arg0;

       return nm;
}

int main(int argc, char *argv[])
{
    struct option long_options[] = {
	{ "config",        1, 0, 'f' },
	{ "help",          0, 0, 'h' },
	{ "ident",         1, 0, 'i' },
	{ "loglevel",      1, 0, 'l' },
	{ "foreground",    0, 0, 'n' },
	{ "pidfile",       1, 0, 'p' },
	{ "syslog",        0, 0, 's' },
	{ "version",       0, 0, 'v' },
	{ NULL, 0, 0, 0 }
    };
    int foreground = 0;
    int i, ch;
    FILE *fp;

    prognm = ident = progname(argv[0]);
    while ((ch = getopt_long(argc, argv, "f:hi:l:np:su:v", long_options, NULL)) != EOF) {
	const char *errstr = NULL;

	switch (ch) {
	case 'f':
	    config_file = strdup(optarg);
	    break;

	case 'h':
	    return usage(0);

	case 'i':	/* --ident=NAME */
	    ident = optarg;
	    break;

	case 'l':
	    if (!strcmp(optarg, "?")) {
		char buf[128];

		log_list(buf, sizeof(buf));
		return !puts(buf);
	    }

	    loglevel = log_str2lvl(optarg);
	    if (-1 == loglevel)
		return usage(1);
	    break;

	case 'n':
	    foreground = 1;
	    use_syslog--;
	    break;

	case 'p':	/* --pidfile=NAME */
	    pid_file = strdup(optarg);
	    break;

	case 's':	/* --syslog */
	    use_syslog++;
	    break;

	case 'v':
	    printf("%s\n", versionstring);
	    return 0;

	default:
	    return usage(1);
	}
    }

    /* Check for unsupported command line arguments */
    argc -= optind;
    if (argc > 0)
	return usage(1);

    if (geteuid() != 0) {
	fprintf(stderr, "%s: must be root\n", ident);
	exit(1);
    }

    if (!foreground) {
#ifdef TIOCNOTTY
	int fd;
#endif

	/* Detach from the terminal */
	haveterminal = 0;
	if (fork())
	    exit(0);

	(void)close(0);
	(void)close(1);
	(void)close(2);
	(void)open("/dev/null", O_RDONLY);
	(void)dup2(0, 1);
	(void)dup2(0, 2);
#ifdef TIOCNOTTY
	fd = open("/dev/tty", O_RDWR);
	if (fd >= 0) {
	    (void)ioctl(fd, TIOCNOTTY, NULL);
	    (void)close(fd);
	}
#else
	if (setsid() < 0)
	    perror("setsid");
#endif
    } else
	setlinebuf(stderr);

    /*
     * Setup logging
     */
    log_init(ident);
    logit(LOG_DEBUG, 0, "%s starting", versionstring);

    compose_paths();

    pev_init();
    igmp_init();
    init_vifs();

    pev_sig_add(SIGHUP,  handle_signals, NULL);
    pev_sig_add(SIGINT,  handle_signals, NULL);
    pev_sig_add(SIGTERM, handle_signals, NULL);
    pev_sig_add(SIGUSR1, handle_signals, NULL);
    pev_sig_add(SIGUSR2, handle_signals, NULL);

    /* Signal world we are now ready to start taking calls */
    if (pidfile(pid_file))
	logit(LOG_WARNING, errno, "Cannot create pidfile");

    return pev_run();
}

static void cleanup(void)
{
    static int in_cleanup = 0;

    if (!in_cleanup) {
	in_cleanup++;

	stop_all_vifs();
	igmp_exit();
    }
}

/*
 * Signal handler.  Take note of the fact that the signal arrived
 * so that the main loop can take care of it.
 */
static void handle_signals(int signo, void *arg)
{
    switch (signo) {
	case SIGINT:
	case SIGTERM:
	    logit(LOG_NOTICE, 0, "%s exiting", versionstring);
	    cleanup();
	    free(pid_file);
	    free(config_file);
	    pev_exit(0);
	    break;

	case SIGHUP:
	    restart();
	    break;

	case SIGUSR1:
	case SIGUSR2:
	    /* ignored for now */
	    break;
    }
}

void restart(void)
{
    /*
     * reset all the entries
     */
    stop_all_vifs();
    igmp_exit();

    igmp_init();
    init_vifs();

    /* Touch PID file to acknowledge SIGHUP */
    pidfile(pid_file);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "cc-mode"
 * End:
 */
