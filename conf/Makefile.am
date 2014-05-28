CONF=             \
 rfc4938.conf

EXTRA_DIST=       \
 rfc4938.conf.in

edit = sed        \
       -e 's|@sbindir[@]|$(sbindir)|g'  

all-local:	$(CONF)

rfc4938.conf:	rfc4938.conf.in
	$(edit) rfc4938.conf.in > rfc4938.conf

clean-local:
	-rm -f $(CONF)


install-exec-hook:      
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/$(PACKAGE)
	cp -f  $(CONF) $(DESTDIR)$(sysconfdir)/$(PACKAGE)

uninstall-local:
	rm -f  $(DESTDIR)$(sysconfdir)/$(PACKAGE)$(CONF)
