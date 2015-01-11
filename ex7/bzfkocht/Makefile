.PHONY:		depend clean lint doxygen check speed valgrind coverage

VERSION		=	4.0
OPT		=	opt
CC		=	gcc
DCC		=	gcc
LINT		=	flexelint
DOXY		=	doxygen
VALGRIND	=	valgrind --tool=memcheck --leak-check=full \
			--leak-resolution=high --show-reachable=yes 

SRCDIR		=	src
BINDIR		=	bin

CPPFLAGS	=	-DVERSION=\"$(VERSION)\" 

GCCWARN		=	-Wall -Wextra -W -Wpedantic -Wpointer-arith -Wwrite-strings \
			-Wstrict-prototypes -Wmissing-prototypes -Wcast-align \
			-Wmissing-declarations -Wshadow -Waggregate-return \
			-Wno-unknown-pragmas -Wuninitialized -Wbad-function-cast \
			-Wno-format-security -Wconversion 
# 
CPPFLAGS	+=	-I$(SRCDIR) 
CFLAGS		=	-O -msse2
LDFLAGS		=	-lm	
DFLAGS		=	-MM
CFLAGS		=	-std=gnu99 $(GCCWARN)

ifeq ($(OPT),dbg)
CFLAGS		+=	-g 
else ifeq ($(OPT),gcov)
CFLAGS		+=	-g -fprofile-arcs -ftest-coverage -DNDEBUG
else ifeq ($(OPT),valgrind)
CFLAGS		+=	-g -O
else
CFLAGS		+=	-g -O3 -m32 -DNDEBUG -DNO_MSHELL
endif

BASE		=	$(CC).$(OPT)
OBJDIR		=	obj/O.$(BASE)
NAME		=	ex7
TARGET		=	$(NAME)-$(VERSION).$(BASE)
BINARY		=	$(BINDIR)/$(TARGET)
DEPEND		=	$(SRCDIR)/depend


#-----------------------------------------------------------------------------

OBJECT		=	$(NAME).o bip.o splitline.o mshell.o

OBJXXX		=	$(addprefix $(OBJDIR)/,$(OBJECT))
OBJSRC		=	$(addprefix $(SRCDIR)/,$(OBJECT:.o=.c))

#-----------------------------------------------------------------------------

$(BINARY):	$(OBJDIR) $(BINDIR) $(OBJXXX)
		$(CC) $(CFLAGS) $(OBJXXX) $(LDFLAGS) -o $@

clean:
		-rm -rf $(OBJDIR)/* $(BINARY) 

$(OBJDIR):	
		@echo "** creating directory \"$@\""
		@-mkdir -p $(OBJDIR)

$(BINDIR):
		@echo "** creating directory \"$@\""
		@-mkdir -p $(BINDIR)

lint:		$(OBJSRC) 
		$(LINT) $(SRCDIR)/project.lnt -os\(lint.out\) \
		$(CPPFLAGS) -UNDEBUG -DNO_MSHELL $^

doxygen:
		$(DOXY) ex.dxy

check:
		cd check; \
		/bin/sh ./check.sh ../$(BINARY) 

speed:
		cd check; \
		/bin/sh ./speed.sh ../$(BINARY) 

valgrind:
		cd check; \
		/bin/sh ./check.sh "$(VALGRIND) ../$(BINARY)" 

coverage:	$(BINARY)
		-ln -s ../../src $(OBJDIR)
		-mkdir -p gcov
		lcov -d $(OBJDIR) -z
		make OPT=gcov check
		lcov -d $(OBJDIR) -c >gcov/z.capture
		lcov -d $(OBJDIR) -r gcov/z.capture "mshell.c" >gcov/zz.capture
		genhtml -o gcov gcov/zz.capture
		-rm gcov/z.capture gcov/zz.capture

depend:
		$(SHELL) -ec '$(DCC) $(DFLAGS) $(CPPFLAGS) $(OBJSRC) \
		| sed '\''s|^\([0-9A-Za-z\_]\{1,\}\)\.o|$$\(OBJDIR\)/\1.o|g'\'' \
		>$(DEPEND)'

-include	$(DEPEND)

$(OBJDIR)/%.o:	$(SRCDIR)/%.c
		$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# --- EOF ---------------------------------------------------------------------

