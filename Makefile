# Makefile for TinyMUX 2.6
#
# Search for the text 'CONFIGURATION SECTION' and make any changes needed
# there.

SHELL=/bin/sh
srcdir = .

BIN = game/bin
CC = gcc
CXX = g++
#CXXCPP = g++ -E	# This is broken in autoconf.  Sigh.
CXXCPP = $(CXX) -E -fpermissive
LIBS = -lm -lnsl -lresolv -lcrypt  

.SUFFIXES: .cpp

# After we finish beta testing, we'll take away the debugging -g flag.
# If you don't use GCC, don't use -g. Add -pg for profiling (gprof netmux
# gmon.out)
#
#OPTIM = -O
#OPTIM = -g -pg -O
OPTIM = -g -O -fpermissive

# By default, REALITY_LVLS is not enabled.  If you wish to use REALITY_LVLS,
# run configure with --enable-realitylvls.  Please read the docs/REALITY.
#
# NOTE: To ensure a clean build, please 'make clean' first.
#
REALITY_LVLS = 
REALITY_SRC = 
REALITY_OBJ = 

# By default, WOD_REALMS is not enabled.  If you wish to use WOD_REALMS,
# run configure with --enable-wodrealms.  Please read the docs/REALMS.
#
# NOTE: To ensure a clean build, please 'make clean' first.
#
WOD_REALMS = 

# Disk-based caching is the default.  If you wish to use memory-based, run
# configure with --enable-memorybased.  Disk-based usually uses less memory,
# but more disk space.  Memory-based usually uses more memory, less
# disk-space, and some gain in performance for a very narrow set of
# operations.
#
# NOTE: To ensure a clean build, please 'make clean' first.
#
MEMORY_BASED = 

# Query Slave. By default, this is off. This can be turned on by running
# configure with --enable-sqlquery. The Query Slave is how to support
# queries to foriegn data sources such as SQL servers.
#
QUERY_SLAVE = 
QUERYLIBS   = -ldbi
SQLSLAVE    = 

# Firan MUX. By default, this is off. This can be turned on by running
# configure with --enable-firanmux. Firan MUX modifications are necessary
# in order to run Firan's database.  Typically, --enable-memorybased is also
# necessary.
#
FIRANMUX    = 

# Firan MUX Conversion. By default, this is off. This can be turned on by
# running configure with --enable-firanmuxconvert. Firan MUX modifications
# are necessary in order convert Firan's old database.  This should only
# be done once. Typically, --enable-memorybased and --enable-firanmux are
# also necessary.
#
FIRANMUX_CONVERT = 

# Deprecated Features. By default, this is off. This can be turned on by
# running configure with --enable-deprecated. Features which are infrequently
# used become deprecated and are eventually removed from the source.
#
DEPRECATED = 

# Distribution source files
D_SRC	= _build.cpp alloc.cpp attrcache.cpp boolexp.cpp bsd.cpp command.cpp \
	comsys.cpp conf.cpp cque.cpp create.cpp db.cpp db_rw.cpp eval.cpp \
	file_c.cpp flags.cpp funceval.cpp functions.cpp funmath.cpp game.cpp \
	help.cpp htab.cpp local.cpp log.cpp look.cpp mail.cpp match.cpp \
	mguests.cpp move.cpp muxcli.cpp netcommon.cpp object.cpp \
	predicates.cpp player.cpp player_c.cpp plusemail.cpp powers.cpp \
	quota.cpp rob.cpp pcre.cpp set.cpp sha1.cpp speech.cpp sqlshared.cpp \
	stringutil.cpp strtod.cpp svdrand.cpp svdhash.cpp svdreport.cpp \
	timer.cpp timeutil.cpp unparse.cpp vattr.cpp walkdb.cpp wild.cpp \
	wiz.cpp
D_OBJ	= _build.o alloc.o attrcache.o boolexp.o bsd.o command.o comsys.o \
	conf.o cque.o create.o db.o db_rw.o eval.o file_c.o flags.o \
	funceval.o functions.o funmath.o game.o help.o htab.o local.o log.o \
	look.o mail.o match.o mguests.o move.o muxcli.o netcommon.o object.o \
	predicates.o player.o player_c.o plusemail.o powers.o quota.o rob.o \
	pcre.o set.o sha1.o speech.o sqlshared.o stringutil.o strtod.o \
	svdrand.o svdhash.o svdreport.o timer.o timeutil.o unparse.o vattr.o \
	walkdb.o wild.o wiz.o

# Version number routine
VER_SRC	= version.cpp
VER_OBJ	= version.o
VER_FLG	= -DMUX_BUILD_DATE="\"`date`\"" \
	  -DMUX_BUILD_NUM="\"`sh ./buildnum.sh`\""

# ===================== CONFIGURATION SECTION ====================
#
# Select the correct C compiler.  Whatever you choose, it must be able
# to grok ANSI C (function prototypes)
#
#-----CC or GCC (must be able to grok function prototypes)
#
DEFS =
#
#-----CC on a NeXT system, really weird derivative of GCC
#
#DEFS = -DNEXT -DNEED_STRDUP
#
#-----GCC if the libraries were built for a pcc-derived cc compiler
#     (most systems)
#
#DEFS = -fpcc-struct-return -Wall
#
#-----GCC with GCC-compatible libraries if you want verbose error messages
#
#DEFS = -Wall
#
#-----HP-UX C compiler
#
#DEFS = -w +Obb800 -Aa -D_INCLUDE_POSIX_SOURCE -D_INCLUDE_HPUX_SOURCE -D_INCLUDE_XOPEN_SOURCE
#
#-----MIPS C compiler (also DEC 3xxx, Prime EXL7xxx)
#
#DEFS = -signed
#

# Libraries.  Use the second line if you want to use the resolver to get
# hostnames and your libc doesn't use it already.  If you use it, you'd
# better have your nameserver working or things may hang for a while when
# people try to login from distant sites.  Use the third line if you're running
# on a SysV-ish system and BSD support isn't built in to the standard libc.
#
MORELIBS	= -lm
#MORELIBS	= -lm -lnsl -lsocket -L/usr/ucblib -lucb	# Mips

# ================== END OF CONFIGURATION SECTION =================

# Auxiliary source files: only used by offline utilities.
#
AUX_SRC	= unsplit.cpp
ALLCXXFLAGS = $(CXXFLAGS) $(OPTIM) $(DEFS) $(MEMORY_BASED) $(WOD_REALMS) $(REALITY_LVLS) $(QUERY_SLAVE) $(FIRANMUX) $(FIRANMUX_CONVERT) $(DEPRECATED)

# Compiliation source files.
#
ALLSRC	= $(D_SRC) $(REALITY_SRC) $(VER_SRC) $(AUX_SRC)
SRC	= $(D_SRC) $(REALITY_SRC)
OBJ	= $(D_OBJ) $(REALITY_OBJ)

.cpp.o:
	$(CXX) $(ALLCXXFLAGS) -c $<

all: netmux slave $(SQLSLAVE) links

links: netmux slave $(SQLSLAVE)
	cd game/bin ; rm -f dbconvert ; ln -s ../../netmux dbconvert
	cd game/bin ; rm -f netmux ; ln -s ../../netmux netmux
	cd game/bin ; rm -f slave ; ln -s ../../slave slave
	( if [ -f sqlslave ]; then cd game/bin ; rm -f sqlslave; ln -s ../../sqlslave sqlslave ; fi )

sqlslave: sqlslave.o sqlshared.o
	$(CXX) $(ALLCXXFLAGS) $(LIBS) $(QUERYLIBS) -o sqlslave sqlslave.o sqlshared.o

slave: slave.o
	$(CXX) $(ALLCXXFLAGS) $(LIBS) -o slave slave.o

unsplit: unsplit.o
	$(CXX) $(ALLCXXFLAGS) -o unsplit unsplit.o

netmux: $(OBJ) $(VER_SRC)
	$(CXX) $(ALLCXXFLAGS) $(VER_FLG) -c $(VER_SRC)
	( if [ -f netmux ]; then mv -f netmux netmux~ ; fi )
	$(CXX) $(ALLCXXFLAGS) -o netmux $(OBJ) $(VER_OBJ) $(LIBS) $(MORELIBS)

depend: $(ALLSRC) unsplit
	for i in $(D_SRC) $(AUX_SRC) slave.cpp sqlslave.cpp sqlshared.cpp ; do $(CXXCPP) $(ALLCXXFLAGS) -M $$i; done | sed -e 's:/usr[^ ]*[ ]*::g' | ./unsplit > .depend~
	mv .depend~ .depend

realclean:
	-rm -f *.o a.out core gmon.out mux.*log mux.*sum netmux netmux~

clean:
	-rm -f *.o a.out core gmon.out mux.*log mux.*sum slave sqlslave netmux
