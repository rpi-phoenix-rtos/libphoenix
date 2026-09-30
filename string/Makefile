#
# Makefile for libphoenix/string
#
# Copyright 2017, 2020 Phoenix Systems
# Author: Pawel Pisarczyk
#

INCS := errno.str.inc errno.tab.inc gaierr.str.inc gaierr.tab.inc


# Prints num<TAB>NAME for every error code defined in the given headers, either as
# `#define ENAME N` or as an enumerator `ENAME = N`. A negative code (the EAI_*
# codes of <netdb.h> are `#define EAI_NONAME -2`) is listed by its absolute value,
# as strerror_() looks codes up after negating a negative one.
ERR_EXTRACT := $(SED) -En -e '/^\s*\#\s*define\s+E\w+\s+-?[[:digit:]]+/{s/^[^d]*define\s+(\w+)\s+-?([[:digit:]]+).*/\2\t\1/;p;d}' \
	-e '/^\s*E\w+\s*=\s*-?[[:digit:]]+/{s/^\s*(\w+)\s*=\s*-?([[:digit:]]+).*/\2\t\1/;p;d}'

# Joins ERR_EXTRACT's num<TAB>NAME lines (stdin) by name against a .desc file
# ($(1): NAME<TAB>message, `#` comments allowed) and prints num<TAB>message sorted
# by number. A code with no .desc entry falls back to its name, so a message is
# never empty for a newly added code.
ERR_DESCRIBE = awk -F'\t' 'NR==FNR { if ($$0 ~ /^[[:space:]]*\#/ || NF < 2) next; desc[$$1] = $$2; next } { print $$1 "\t" ($$2 in desc ? desc[$$2] : $$2) }' $(1) - | sort -n


# errno.list and gaierr.list are num<TAB>DESCRIPTION, so that strerror() and
# gai_strerror() return a human-readable message ("No such file or directory",
# "Name or service not known") rather than the bare macro name or, as
# gai_strerror() did while ERR_EXTRACT ignored negative codes, "Unknown error N".
# Messages follow glibc's wording (POSIX specifies the conditions, not the text).
# A .desc message must not contain a tab, a backslash or a double quote, and
# should be ASCII: the .inc rules below copy it verbatim into a C string and
# count its length in characters.
%/errno.list: $(SYSROOT)/usr/include/phoenix/errno.h include/errno.h string/errno.desc string/Makefile
	@(printf "GEN %-24s  \n" "$(@F)")
	$(SIL)$(ERR_EXTRACT) $(filter %errno.h,$^) | $(call ERR_DESCRIBE,string/errno.desc) > $@


%/gaierr.list: include/netdb.h string/gaierr.desc string/Makefile
	@(printf "GEN %-24s  \n" "$(@F)")
	$(SIL)$(ERR_EXTRACT) $(filter %netdb.h,$^) | $(call ERR_DESCRIBE,string/gaierr.desc) > $@


%.str.inc: %.list Makefile
	@(printf "GEN %-24s  \n" "$(@F)")
	$(SIL)$(SED) -e 's/^.*\t\(.*\)$$/"\1\\0"/' $< > $@


%.tab.inc: %.list Makefile
	@(printf "GEN %-24s  \n" "$(@F)")
	$(SIL)bash -c 'o=0; while read num name; do echo "{ $$num, $$o },"; o=$$((o+$${#name}+1)); done' < $< > $@


$(PREFIX_O)string/strerror.o: $(patsubst %,$(CURDIR)/string/%,$(INCS))


OBJS += $(addprefix $(PREFIX_O)string/, string.o strdup.o strerror.o strsignal.o)
