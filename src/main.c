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

static int sighandled = 0;
#define	GOT_SIGINT	0x01
#define	GOT_SIGHUP	0x02
#define	GOT_SIGUSR1	0x04
#define	GOT_SIGUSR2	0x08

int running = 1;
int use_syslog = 1;
time_t bridged_init_time;

char *config_file = _PATH_BRIDGED_CONF;
char *pid_file    = NULL;
char *sock_file   = NULL;

char *ident       = PACKAGE_NAME;
char *prognm      = NULL;
const char *versionstring = "bridged version " PACKAGE_VERSION;

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
static void handle_signals(int);
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

static int usage(int code)
{
    printf("Usage: %s [-himnpsv] [-f FILE] [-i NAME] [-p FILE]\n"
	   "\n"
	   "  -f, --config=FILE        Configuration file to use, default /etc/%s.conf\n"
	   "  -h, --help               Show this help text\n"
	   "  -i, --ident=NAME         Identity for syslog, .cfg & .pid file, default: %s\n"
	   "  -l, --loglevel=LEVEL     Set log level: none, err, notice (default), info, debug\n"
	   "  -n, --foreground         Run in foreground, do not detach from controlling terminal\n"
	   "  -p, --pidfile=FILE       File to store process ID for signaling daemon\n"
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
    FILE *fp;
    int foreground = 0;
    int vers, n = -1, i, ch;
    struct pollfd *pfd;
    struct sigaction sa;
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

    pid_file = prognm = ident = progname(argv[0]);
    while ((ch = getopt_long(argc, argv, "f:hi:l:np:su:v", long_options, NULL)) != EOF) {
	const char *errstr = NULL;

	switch (ch) {
	case 'f':
	    config_file = optarg;
	    break;

	case 'h':
	    return usage(0);

	case 'i':	/* --ident=NAME */
	    pid_file = prognm = ident = optarg;
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

    timer_init();
    igmp_init();

    init_vifs();

    sa.sa_handler = handle_signals;
    sa.sa_flags = 0;	/* Interrupt system calls */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    pfd = calloc(NHANDLERS, sizeof(struct pollfd));
    if (!pfd) {
	logit(LOG_ERR, errno, "Failed allocating struct pollfd");
	return 1;	/* NOTREACHED */
    }

    /* schedule first timer interrupt */
    timer_set(TIMER_INTERVAL, timer, NULL);

    /* Signal world we are now ready to start taking calls */
    if (pidfile(pid_file))
	logit(LOG_WARNING, errno, "Cannot create pidfile");

    /*
     * Main receive loop.
     */
    while (running) {
	for (i = 0; i < nhandlers; i++) {
	    pfd[i].fd = ihandlers[i].fd;
	    pfd[i].events = POLLIN;
	}

	if (check_signals())
	    break;

	n = poll(pfd, nhandlers, timeout(n) * 1000);
	if (n < 0) {
	    if (errno != EINTR)
		logit(LOG_WARNING, errno, "poll failed");
	    continue;
	}

	if (n > 0) {
	    for (i = 0; i < nhandlers; i++) {
		if (pfd[i].revents & POLLIN)
		    (*ihandlers[i].func)(ihandlers[i].fd);
	    }
	}
    }

    logit(LOG_NOTICE, 0, "%s exiting", versionstring);
    free(pfd);
    cleanup();

    return 0;
}

/*
 * The 'virtual_time' variable is initialized to a value that will cause the
 * first invocation of timer() to send a probe or route report to all vifs
 * and send group membership queries to all subnets for which this router is
 * querier.  This first invocation occurs approximately TIMER_INTERVAL seconds
 * after the router starts up.   Note that probes for neighbors and queries
 * for group memberships are also sent at start-up time, as part of initial-
 * ization.  This repetition after a short interval is desirable for quickly
 * building up topology and membership information in the presence of possible
 * packet loss.
 *
 * 'virtual_time' advances at a rate that is only a crude approximation of
 * real time, because it does not take into account any time spent processing,
 * and because the timer intervals are sometimes shrunk by a random amount to
 * avoid unwanted synchronization with other routers.
 */
uint32_t virtual_time = 0;


/*
 * Timer routine.  Performs periodic neighbor probing, route reporting, and
 * group querying duties, and drives various timers in routing entries and
 * virtual interface data structures.
 */
static void timer(void *arg)
{
    timer_set(TIMER_INTERVAL, timer, NULL);

    age_vifs();		/* Advance the timers for neighbors */

    /*
     * Advance virtual time
     */
    virtual_time += TIMER_INTERVAL;
}

/*
 * Handle timeout queue.
 *
 * If poll() + packet processing took more than 1 second, or if there is
 * a timeout pending, age the timeout queue.  If not, collect usec in
 * difftime to make sure that the time doesn't drift too badly.
 *
 * XXX: If the timeout handlers took more than 1 second, age the timeout
 * queue again.  Note, this introduces the potential for infinite loops!
 */
static int timeout(int n)
{
    static struct timespec difftime, curtime, lasttime;
    static int init = 1, secs = 0;

    /* Age queue */
    do {
	/*
	 * If poll() timed out, then there's no other activity to
	 * account for and we don't need to call clock_gettime().
	 */
	if (n == 0) {
	    curtime.tv_sec = lasttime.tv_sec + secs;
	    curtime.tv_nsec = lasttime.tv_nsec;
	    n = -1; /* don't do this next time through the loop */
	} else {
	    clock_gettime(CLOCK_MONOTONIC, &curtime);
	    if (init) {
		init = 0;	/* First time only */
		lasttime = curtime;
		difftime.tv_nsec = 0;
	    }
	}

	difftime.tv_sec = curtime.tv_sec - lasttime.tv_sec;
	difftime.tv_nsec += curtime.tv_nsec - lasttime.tv_nsec;
	while (difftime.tv_nsec > 1000000000) {
	    difftime.tv_sec++;
	    difftime.tv_nsec -= 1000000000;
	}

	if (difftime.tv_nsec < 0) {
	    difftime.tv_sec--;
	    difftime.tv_nsec += 1000000000;
	}
	lasttime = curtime;

	if (secs == 0 || difftime.tv_sec > 0)
	    timer_age_queue(difftime.tv_sec);

	secs = -1;
    } while (difftime.tv_sec > 0);

    /* Next timer to wait for */
    secs = timer_next_delay();

    return secs;
}

static void cleanup(void)
{
    static int in_cleanup = 0;

    if (!in_cleanup) {
	in_cleanup++;

	stop_all_vifs();
	close(udp_socket);

	timer_exit();
	igmp_exit();
    }
}

/*
 * Signal handler.  Take note of the fact that the signal arrived
 * so that the main loop can take care of it.
 */
static void handle_signals(int sig)
{
    switch (sig) {
	case SIGINT:
	case SIGTERM:
	    sighandled |= GOT_SIGINT;
	    break;

	case SIGHUP:
	    sighandled |= GOT_SIGHUP;
	    break;

	case SIGUSR1:
	    sighandled |= GOT_SIGUSR1;
	    break;

	case SIGUSR2:
	    sighandled |= GOT_SIGUSR2;
	    break;
    }
}

static int check_signals(void)
{
    if (!sighandled)
	return 0;

    if (sighandled & GOT_SIGINT) {
	sighandled &= ~GOT_SIGINT;
	return 1;
    }

    if (sighandled & GOT_SIGHUP) {
	sighandled &= ~GOT_SIGHUP;
	restart();
    }

    if (sighandled & GOT_SIGUSR1) {
	sighandled &= ~GOT_SIGUSR1;
	logit(LOG_INFO, 0, "SIGUSR1 is no longer supported, use mroutectl instead.");
    }

    if (sighandled & GOT_SIGUSR2) {
	sighandled &= ~GOT_SIGUSR2;
	logit(LOG_INFO, 0, "SIGUSR2 is no longer supported, use mroutectl instead.");
    }

    return 0;
}

void restart(void)
{
    char *s;

    s = strdup (" restart");
    if (s == NULL)
	logit(LOG_ERR, 0, "out of memory");

    /*
     * reset all the entries
     */
    timer_stop_all();
    stop_all_vifs();
    igmp_exit();
#ifndef IOCTL_OK_ON_RAW_SOCKET
    close(udp_socket);
#endif

    igmp_init();
    init_vifs();

    /* Touch PID file to acknowledge SIGHUP */
    pidfile(pid_file);

    /* schedule timer interrupts */
    timer_set(TIMER_INTERVAL, timer, NULL);
}

#define SCALETIMEBUFLEN 27
char *scaletime(time_t t)
{
    static char buf1[SCALETIMEBUFLEN];
    static char buf2[SCALETIMEBUFLEN];
    static char *buf = buf1;
    char *p;

    p = buf;
    if (buf == buf1)
	buf = buf2;
    else
	buf = buf1;

    snprintf(p, SCALETIMEBUFLEN, "%2ld:%02ld:%02ld", t / 3600, (t % 3600) / 60, t % 60);

    return p;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "cc-mode"
 * End:
 */
