CFLAGS  = -ansi -fPIC -Wall -Wextra -Wpedantic -I../include
LDFLAGS = -shared
LDLIBS  = -lcjson -lcurl -lsqlite3

depdir = .dep
objdir = .obj

src := $(wildcard *.c)
dep := $(src:%.c=$(depdir)/%.d)
obj := $(src:%.c=$(objdir)/%.o)
lib  = libnhl.so

this := $(lastword $(MAKEFILE_LIST))
cache = $(this)Target
clean = rm -f $(lib) $(objdir)/*.o $(depdir)/*.d $(cache).*

.PHONY: release
release: CFLAGS += -O2 -DNDEBUG
release: $(cache).release $(lib)

.PHONY: debug
debug: CFLAGS += -O0 -g3
debug: $(cache).debug $(lib)

$(lib): $(obj) $(this)
	$(CC) $(obj) -o $@ $(LDFLAGS) $(LDLIBS) $(CFLAGS)

$(objdir)/%.o: %.c $(depdir)/%.d $(this) | $(objdir) $(depdir)
	$(CC) $< -c -o $@ -MMD -MP -MF $(depdir)/$*.d $(CFLAGS)

$(objdir) $(depdir):
	mkdir $@

$(cache).%:
	$(clean)
	@touch $@

.PHONY: clean
clean:
	$(clean)
	-rmdir $(objdir) $(depdir)

.DELETE_ON_ERROR:

$(dep):
include $(wildcard $(dep))
