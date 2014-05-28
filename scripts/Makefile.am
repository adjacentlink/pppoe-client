
EXTRA_DIST = \
	rfc4938clientd.in

edit = sed \
	-e 's|@configdir[@]|$(sysconfdir)/pppoe-client|g' \
	-e 's|@execdir[@]|$(sbindir)|g'

rfc4938clientd: rfc4938clientd.in
	$(edit) $< > $@
	chmod +x  rfc4938clientd

all-local: rfc4938clientd


clean-local:
	-rm -f rfc4938clientd


install-exec-hook: rfc4938clientd
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/init.d
	cp -f rfc4938clientd $(DESTDIR)$(sysconfdir)/init.d/rfc4938clientd
	chmod +x  $(DESTDIR)$(sysconfdir)/init.d/rfc4938clientd

uninstall-local:
	rm -f $(DESTDIR)$(sysconfdir)/init.d/rfc4938clientd
