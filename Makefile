-include .config

#########################################
# VARIABLES - overridable by make flags #
#########################################
CFLAGS         = 
cflags-y       = $(CFLAGS) $(EXTRA_CFLAGS)
cflags-y      += -Isrc
cflags-y      += -std=c11 -Wall -Wextra -Wno-implicit-fallthrough -Wno-unused-const-variable
cflags-y      += -Werror=format-security -Werror=implicit-function-declaration 
cflags-y      += -Wshadow -Wdouble-promotion -Wformat=2 -Wformat-truncation -Wvla 
cflags-y      += -Wno-unused-parameter -fno-common
cflags-y      += -pipe
LDFLAGS        = 
ldflags-y      = $(LDFLAGS) $(EXTRA_LDFLAGS)
ldflags-y     += -fno-common
LDLIBS         = $(EXTRA_LDLIBS)
ldlibs-y       = $(LDLIBS) $(EXTRA_LDLIBS)
DESTDIR        = /
PREFIX         = /usr/local
RAGEL          = ragel
RAGELFLAGS     = -G2 $(EXTRA_RAGELFLAGS)
INSTALL        = install
BEAR           = bear
COMPLEXITY     = complexity
CFLOW          = cflow
SED            = sed
NEATO          = neato
CTAGS          = ctags
UPX            = upx
STRIP          = strip
SCAN_BUILD     = scan-build
KCONFIG_MCONF  = kconfig-mconf
KCONFIG_NCONF  = kconfig-nconf
KCONFIG_CONF   = kconfig-conf
Q              = @
CC_COLOR       = \033[0;34m
LD_COLOR       = \033[0;33m
TEST_COLOR     = \033[0;35m
INSTALL_COLOR  = \033[0;32m
NO_COLOR       = \033[m


# position independent code (address space layout randomization)
cflags-$(CONFIG_PIE) += -pie -fpie -Wl,-pie 
ldflags-$(CONFIG_PIE) += -pie -fpie -Wl,-pie

cflags-$(CONFIG_STATIC) += -static
ldflags-$(CONFIG_STATIC) += -static

# runtime buffer overflow detection
cflags-$(CONFIG_RBUD) += -D_FORTIFY_SOURCE=2

# detect and reject underlinking
cflags-$(CONFIG_DETECT_UNDERLINKING) += -Wl,-z,defs
ldflags-$(CONFIG_DETECT_UNDERLINKING) += -Wl,-z,defs

# disable lazy binding
cflags-$(CONFIG_DISABLE_LAZY_BINDING) += -Wl,-z,now
ldflags-$(CONFIG_DISABLE_LAZY_BINDING) += -Wl,-z,now

# read-only segments after relocation
cflags-$(CONFIG_RELRO) += -Wl,-z,relro
ldflags-$(CONFIG_RELRO) += -Wl,-z,relro

# increased reliability of stack overflow detection
cflags-$(CONFIG_STACK_CLASH_PROTECTION) += -fstack-clash-protection
ldflags-$(CONFIG_STACK_CLASH_PROTECTION) += -fstack-clash-protection

# stack smashing protection levels
cflags-$(CONFIG_STACK_PROTECTOR) += -fstack-protector
ldflags-$(CONFIG_STACK_PROTECTOR) += -fstack-protector
cflags-$(CONFIG_STACK_PROTECTOR_ALL) += -fstack-protector-all
ldflags-$(CONFIG_STACK_PROTECTOR_ALL) += -fstack-protector-all
cflags-$(CONFIG_STACK_PROTECTOR_STRONG) += -fstack-protector-strong
ldflags-$(CONFIG_STACK_PROTECTOR_STRONG) += -fstack-protector-strong

# AddressSanitizer
cflags-$(CONFIG_ASAN) += -fsanitize=address
ldflags-$(CONFIG_ASAN) += -fsanitize=address
ldlibs-$(CONFIG_ASAN) += -lasan

# UndefinedBehaviourSanitizer
cflags-$(CONFIG_UBSAN) += -fsanitize=undefined
ldflags-$(CONFIG_UBSAN) += -fsanitize=undefined

# build with debug info
cflags-$(CONFIG_OPTIMIZE_DEBUG) += -O0 -g3
ldflags-$(CONFIG_OPTIMIZE_DEBUG) += -O0 -g3

# build optimize for smallness
cflags-$(CONFIG_OPTIMIZE_SMALL) += -Os
ldflags-$(CONFIG_OPTIMIZE_SMALL) += -Os

# build with debug outputs
cflags-$(CONFIG_DEBUG) += -DDEBUG

ifdef CONFIG_NATS_HOST
cflags-y += -DCONFIG_NATS_HOST='$(CONFIG_NATS_HOST)' 
endif

ifdef CONFIG_NATS_PORT
cflags-y += -DCONFIG_NATS_PORT='$(CONFIG_NATS_PORT)'
endif

ifdef CONFIG_MAX_MSG_LEN
cflags-y += -DCONFIG_MAX_MSG_LEN='$(CONFIG_MAX_MSG_LEN)'
endif

ifdef CONFIG_MAX_HOSTNAME_LEN
cflags-y += -DCONFIG_MAX_HOSTNAME_LEN='$(CONFIG_MAX_HOSTNAME_LEN)'
endif

# }}}


###############
# MAIN TARGET #
###############
# {{{

default: all

all: ljsyslog ljsyslog.stripped

ljsyslog: ljsyslog.o ljsyslog_journald.o ljsyslog_nats.o ljsyslog_nats_parser.o ljsyslog_journald_parser.o

# }}}


#########################
# DEVELOPMENT UTILITIES #
#########################
# {{{

.PHONY: complexity
complexity:
	$(COMPLEXITY) --scores --threshold=1 src/*.c

.PHONY: ci
ci: | cscope.files
	cat cscope.files | entr sh -c "clear; make -B"

.PHONY: ci-test
ci-test: | cscope.files
	cat cscope.files | entr sh -c "clear; make -B test"

.PHONY: cscope
cscope: | cscope.files
	cscope -b -q -k

.PHONY: compile_commands.json
compile_commands.json:
	$(BEAR) -- $(MAKE) -B all

.PHONY: ctags
ctags: | cscope.files
	$(CTAGS) -L cscope.files

.PHONY: scan-build
scan-build:
	$(SCAN_BUILD) $(MAKE) -B all

nconfig:
	$(Q)$(KCONFIG_NCONF) Kconfig

silentoldconfig:
	$(Q)$(KCONFIG_CONF) --silentoldconfig Kconfig

oldconfig:
	$(Q)$(KCONFIG_CONF) --oldconfig Kconfig

menuconfig:
	$(Q)$(KCONFIG_MCONF) Kconfig

config:
	$(Q)$(KCONFIG_CONF) Kconfig

allnoconfig:
	$(Q)$(KCONFIG_CONF) --allnoconfig Kconfig

allyesconfig:
	$(Q)$(KCONFIG_CONF) --allyesconfig Kconfig

savedefconfig:
	$(Q)$(KCONFIG_CONF) --savedefconfig=defconfig Kconfig

%_defconfig:
	$(Q)$(KCONFIG_CONF) --defconfig=configs/$@ Kconfig

defconfig:
	$(Q)$(KCONFIG_CONF) --defconfig=configs/defconfig Kconfig

# }}}


################
# TEST TARGETS #
################
# {{{

# The 'test' target is primarily for running a separate test suite, usually
# for unit tests and property based testing. It differs from the 'check'
# target in that it does not necessarily need the compiled target (the
# library or binary that this Makefile builds) - it only needs some
# of the object files. Most users will make a project by naively running
# 'make' in the directory, and then run 'make test' -  but in 'make test',
# we'd like to enable code coverage and other neat stuff using CFLAGS and
# LDLIBS. I've taken the liberty to assume that this Makefile will be used
# in projects where a full recompile isn't a big deal, and we just recompile
# the entire project with the correct compile flags. Then we have the opposite
# problem, that the user might run 'make install' after a 'make test'; that
# won't be *as much* of an issue - at least the target binary will not be
# linked with the '--coverage' flag, and it won't generate gcov files when
# executed.
test: cflags-y += -fprofile-arcs -ftest-coverage
test: ldlibs-y += -lgcov --coverage
test: test_driver
	@printf "$(TEST_COLOR)TEST$(NO_COLOR) $@\n"
	$(Q)./test_driver


# The 'check' target is primarily for testing *the compiled target*; i.e. if
# you're building a shared library, the 'check' target would compile a binary
# which links to that shared library and runs tests. If you're building a
# binary, then this target would in some useful way execute that file and test
# it's behaviour.
check:
	@echo "No checks available."

test_driver: test_driver.o ljsyslog_journald_parser.o

# }}}


###################
# INSTALL TARGETS #
###################
# {{{

install: $(DESTDIR)$(PREFIX)/bin/ljsyslog

# }}}


#################
# CLEAN TARGETS #
#################
# {{{

clean:
	rm -f *.o test_driver *.gcda *.gcno *.gcov *.cflow *deps *parser.c *.dot .ccls-cache

distclean: clean
	rm -f *.so ljsyslog ljsyslog.stripped

# }}}


########
# DOCS #
########
# {{{

.PHONY: docs
docs:
	$(MAKE) -C docs $@

.PHONY: latexpdf
latexpdf:
	$(MAKE) -C docs $@

# }}}


################
# SOURCE PATHS #
################
# {{{

vpath %.c src/
vpath %.c.rst src/
vpath %.c.md src/
vpath %.c.rl src/
vpath %.c.rl.md src/
vpath %.c.rl.rst src/
vpath %.h include/
vpath %.h inc/
vpath test_%.c tests/

# }}}


##################
# IMPLICIT RULES #
##################
# {{{

$(DESTDIR)$(PREFIX)/bin:
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0755 -d $@

$(DESTDIR)$(PREFIX)/lib:
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0755 -d $@

$(DESTDIR)$(PREFIX)/include:
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0755 -d $@

$(DESTDIR)$(PREFIX)/lib/%.so: %.so | $(DESTDIR)$(PREFIX)/lib
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0644 $< $@

$(DESTDIR)$(PREFIX)/lib/%.a: %.a | $(DESTDIR)$(PREFIX)/lib
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0644 $< $@

$(DESTDIR)$(PREFIX)/include/%.h: %.h | $(DESTDIR)$(PREFIX)/include
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0644 $< $@

$(DESTDIR)$(PREFIX)/bin/%: % | $(DESTDIR)$(PREFIX)/bin
	@printf "$(INSTALL_COLOR)INSTALL$(NO_COLOR) $@\n"
	$(Q)$(INSTALL) -m 0755 $< $@

%.deps: %
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(CC) -c $(cflags-y) $(CPPFLAGS) -M $^ | $(SED) -e 's/[\\ ]/\n/g' | $(SED) -e '/^$$/d' -e '/\.o:[ \t]*$$/d' | sort | uniq > $@

%: %.o
	@printf "$(LD_COLOR)LD$(NO_COLOR) $@\n"
	$(Q)$(CROSS_COMPILE)$(CC) $(ldflags-y) -o $@ $^ $(LOADLIBES) $(ldlibs-y)

%.a:
	@printf "$(LD_COLOR)LD$(NO_COLOR) $@\n"
	$(Q)$(AR) rcs $@ $^

%.so: cflags-y += -fPIC
%.so:
	@printf "$(LD_COLOR)LD$(NO_COLOR) $@\n"
	$(Q)$(CROSS_COMPILE)$(CC) $(ldflags-y) -shared -o $@ $^ $(LOADLIBES) $(ldlibs-y)

%.o: %.c
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(CROSS_COMPILE)$(CC) -c $(cflags-y) $(CPPFLAGS) -o $@ $^

# UPX-minified binaries
%.upx: %
	@printf "$(LD_COLOR)UPX$(NO_COLOR) $@\n"
	$(Q)$(UPX) -o $@ $^

%.stripped: %
	@printf "$(LD_COLOR)STRIP$(NO_COLOR) $@\n"
	$(Q)$(STRIP) -o $@ $^

# for each c file, it's possible to generate a cflow flow graph.
%.c.cflow: %.c
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(CFLOW) -o $@ $<

%.png: %.dot
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(NEATO) -Tpng -Ln100 -o $@ $<

%.dot: %.rl
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(RAGEL) $(RAGELFLAGS) -V -p $< -o $@

%.c: %.c.rl
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)$(RAGEL) -Iinclude $(RAGELFLAGS) -o $@ $<

%.c: %.c.rst
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)cat $< | rst_tangle > $@

# build c files from markdown files - literate programming style
%.c: %.c.md
	@printf "$(CC_COLOR)CC$(NO_COLOR) $@\n"
	$(Q)cat $< | sed -n '/^```c/,/^```/ p' | sed '/^```/ d' > $@

# }}}


#vim: set foldmethod=marker
