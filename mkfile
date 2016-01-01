</$objtype/mkfile

P=gc

LIB=lib$P.$O.a
OFILES=$P.$O
HFILES=/sys/include/$P.h

</sys/src/cmd/mksyslib

install:V:	$LIB
	cp $LIB /$objtype/lib/lib$P.a
	cp $P.h /sys/include/$P.h

uninstall:V:
	rm -f /$objtype/lib/lib$P.a /sys/include/$P.h

nuke:V:
	mk uninstall

clean:V:
	rm -f *.[$OS] [$OS].out [$OS].test lib$P.[$OS].a

$O.test:	test.$O $LIB
	$LD -o $target $prereq

test:V:
	mk clean
	mk $O.test
	$O.test
