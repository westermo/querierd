EXTRA_DIST         = lib.sh basic.sh sleepy.sh two.sh ipc.sh late.sh
CLEANFILES         = *~ *.trs *.log

TEST_EXTENSIONS    = .sh
TESTS_ENVIRONMENT  = unshare -mrun

TESTS              = sleepy.sh
TESTS             += basic.sh
TESTS             += ipc.sh
TESTS             += late.sh
TESTS             += two.sh
