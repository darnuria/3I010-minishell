# Mettre dans SCHEDPATH le repertoire ou se trouve libsched.a
# et sched.h
NAME = minishell
SRCDIR = ./
OBJDIR = ./obj
INCLUDEDIR = ./
#LDFLAGS =

SRC = slice.c \
	  process.c \
      main.c

CFLAGS = --std='c11'

ifeq ($(RELEASE),yes)
	CC = gcc
	CFLAGS += -O3
else
	CC = clang
	CFLAGS += -ggdb3 -Wshadow -Wunreachable-code \
			  -pedantic-errors -O0 -W -Wundef \
			  -Wfatal-errors -Wstrict-prototypes -Wmissing-prototypes \
			  -Wwrite-strings -Wunknown-pragmas \
			  -Wold-style-definition -Wfloat-equal \
			  -Wpointer-arith -Wnested-externs -Wstrict-overflow=5 \
			  -Wno-missing-field-initializers -Wswitch-default -Wswitch-enum \
			  -Wbad-function-cast -Wredundant-decls
endif

OBJS = $(SRC:.c=.o)
OBJS_PREF = $(addprefix $(OBJDIR)/, $(OBJS))
LD = $(CC)

SCHEDPATH = /Infos/lmd/2005/licence/ue/li324-2006fev/libsched/src

all: objdir $(NAME)

objdir:
	@mkdir -p $(OBJDIR)

#build_dep:
#	make re ${ARGS}-C my_stdext

$(NAME): $(OBJS_PREF)
	$(LD) $(LDFLAGS) -I$(INCLUDEDIR) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -I$(INCLUDEDIR) -o $@ -c $<

#	gcc $(CFLAGS) -I. -o run_test test.c libdl_list.a

clean:
	rm -f $(OBJS_PREF)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: clean fclean re all
