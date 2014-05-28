SUBDIRS=      \
 src          \
 src/pppoe    \
 src/ctrl     \
 conf         \
 scripts

ACLOCAL_AMFLAGS="-Im4"

dist-hook: pppoe-client.spec

cleantar:
	@(rm -f pppoe-client*.tar.gz)

if HAVE_DEB
cleandeb:
	dh_clean
else
cleandeb:
endif

RPMARCH=`rpm --showrc | awk '/^build arch/ {print $$4}'`

rpm:	clean-local dist-gzip
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

clean-local:	cleantar
	-rm -rf .rpmbuild
	-rm -fr debian/tmp
	-rm -fr debian/pppoe-client-rfc4938

if HAVE_DEB
deb: debian/changelog
	dpkg-buildpackage -b -us -uc

debian/%:	debian/%.in
	if test -f $@; then chmod u+w $@; fi
	$(edit) $< > $@
	chmod g-w,u-w $@
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

DISTCLEANFILES = .rpmbuild debian/changelog build-stamp

edit = sed \
       -e 's|@VERSION[@]|$(VERSION)|g'

