#
# Makefile for libphoenix/string
#
# Copyright 2017, 2020 Phoenix Systems
# Author: Pawel Pisarczyk
#

INCS := errno.str.inc errno.tab.inc gaierr.str.inc gaierr.tab.inc


ERR_EXTRACT := $(SED) -En -e '/^\s*\#\s*define\s+E\w+\s+[[:digit:]]+/{s/^[^d]*define\s+(\w+)\s+([[:digit:]]+).*/\2\t\1/;p;d}' \
	-e '/^\s*E\w+\s*=\s*-?[[:digit:]]+/{s/^\s*(\w+)\s*=\s*-?([[:digit:]]+).*/\2\t\1/;p;d}'


# errno.list is num<TAB>DESCRIPTION: the numeric errno definitions extracted from
# the headers, joined by name against errno.desc (name<TAB>POSIX message) so that
# strerror() returns a human-readable message ("No such file or directory") rather
# than the bare macro name ("ENOENT"). An errno with no entry in errno.desc falls
# back to its name, so strerror() is never empty for a newly added errno. The
# gaierr.list rule below has no .desc and keeps names, so gai_strerror() is
# unchanged.
%/errno.list: $(SYSROOT)/usr/include/phoenix/errno.h include/errno.h string/errno.desc
	@(printf "GEN %-24s  \n" "$(@F)")
	$(SIL)$(ERR_EXTRACT) $(filter %errno.h,$^) | \
		awk -F'\t' 'NR==FNR { if ($$0 ~ /^[[:space:]]*#/ || NF < 2) next; desc[$$1] = $$2; next } { print $$1 "\t" ($$2 in desc ? desc[$$2] : $$2) }' string/errno.desc - | \
		sort -n > $@


%/gaierr.list: include/netdb.h Makefile
	@(printf "GEN %-24s  \n" "$(@F)")
	$(SIL)$(ERR_EXTRACT) $^ | sort -n > $@


%.str.inc: %.list Makefile
	@(printf "GEN %-24s  \n" "$(@F)")
	$(SIL)$(SED) -e 's/^.*\t\(.*\)$$/"\1\\0"/' $< > $@


%.tab.inc: %.list Makefile
	@(printf "GEN %-24s  \n" "$(@F)")
	$(SIL)bash -c 'o=0; while read num name; do echo "{ $$num, $$o },"; o=$$((o+$${#name}+1)); done' < $< > $@


$(PREFIX_O)string/strerror.o: $(patsubst %,$(CURDIR)/string/%,$(INCS))


OBJS += $(addprefix $(PREFIX_O)string/, string.o strdup.o strerror.o strsignal.o)
