#include <u.h>
#include <libc.h>

#include "gc.h"

/* stress */
int sflag = 0;

void
stress(void)
{
	int i;
	void *ptrs[1];

	for(;;){
		for(i = 0; i < nelem(ptrs); i++){
			ptrs[i] = galloc(256);
			print("---- p[%d] = %#p %#p\n", i, &ptrs[i], ptrs[i]);
		}

		gcollect();

		print("----------\n");

		sleep(100);
	}
}

void *data = 0;

void
main(int argc, char *argv[])
{
	void *p, *p2;

	ARGBEGIN{
	case 's':
		sflag = 1;
	}ARGEND

	ginit();

	if(sflag){
		stress();
		exits(nil);
	}

	p2 = galloc(512);

	print("---- p2 = %#p %#p\n", p2, &p2);

	p = galloc(256);
	print("---- p = %#p %#p\n", p, &p);
	gcollect();

	p = galloc(256);
	print("---- p = %#p %#p\n", p, &p);
	gcollect();

	p = galloc(256);
	print("---- p = %#p %#p\n", p, &p);
	gcollect();

	p = galloc(256);
	print("---- p = %#p %#p\n", p, &p);
	gcollect();

	exits(nil);
}
