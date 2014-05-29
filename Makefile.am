SUBDIRS=      \
 src          \
 src/pppoe    \
 src/ctrl     \
 conf         \
 scripts

ACLOCAL_AMFLAGS="-Im4"

edit = sed \
       -e 's|@VERSION[@]|$(VERSION)|g'

dist-hook: pppoe-client.spec

cleantar:
	@(rm -f pppoe-client*.tar.gz)

RPMARCH=`rpm --showrc | awk '/^build arch/ {print $$4}'`

rpm:	dist-gzip
	mkdir -p .rpmbuild/BUILD \
	.rpmbuild/SPECS \
	.rpmbuild/SOURCES \
	.rpmbuild/SRPMS \
	.rpmbuild/RPMS/$(RPMARCH) \
	.rpmbuild/tmp
	cp -f $(PACKAGE)-$(VERSION).tar.gz .rpmbuild/SOURCES
	rpmbuild --clean -ba $(top_srcdir)/pppoe-client.spec \
   -D "_topdir $$PWD/.rpmbuild" -D "_tmppath $$PWD/.rpmbuild/tmp"
	@echo "============================================================"
	@echo "RPMs located in .rpmbuild/RPMS and .rpmbuild/SRPMS"

clean-local:	cleantar cleandeb
	-rm -rf .rpmbuild
	-rm -rf .debbuild
	-rm -f debian/changelog

if HAVE_DEB

deb: dist-gzip 
	mkdir -p .debbuild
	cp -f $(PACKAGE)-$(VERSION).tar.gz .debbuild
	cd .debbuild && tar xzf $(PACKAGE)-$(VERSION).tar.gz
	$(edit) .debbuild/$(PACKAGE)-$(VERSION)/debian/changelog.in > \
          .debbuild/$(PACKAGE)-$(VERSION)/debian/changelog
	cd .debbuild/$(PACKAGE)-$(VERSION) && dpkg-buildpackage -F -us -uc
	@echo "============================================================"
	@echo "debs located in .debbuild"
cleandeb:
	dh_clean
else

cleandeb:

endif

EXTRA_DIST =            \
 AUTHORS                \
 COPYING                \
 ChangeLog              \
 NEWS                   \
 README                 \
 README.md              \
 pppoe-client.spec.in   \
 license.txt            \
 debian


