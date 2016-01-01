#include <u.h>
#include <libc.h>
#include <tos.h>

#include "gc.h"

enum
{
	DEBUG = 1,
};

#define dprint if(DEBUG) print
#define UNTAG(p) ((cell*)(((uintptr) (p)) & 0xfffffffc))

typedef struct cell cell;
struct cell
{
	uintptr size;
	cell *next;
	destructor *d;
	uintptr magic;
};

long gcmax = 5000;
long gcmin = 50;
static long allocs = 0;

static cell base;
static cell *usedp, *freep;

#pragma varargck        type    "G"     cell*

static int
cellfmt(Fmt *f)
{
	cell *c;

	c = va_arg(f->args, cell*);

	if(c == nil)
		return fmtprint(f, "cell <nil>");
	return fmtprint(f, "cell %#p size %lud next %#p mark %lud",
		c, c->size, c->next, (uintptr)(c->next) & 1 == 1);
}

void
ginit(void)
{
	fmtinstall('G', cellfmt);
	usedp = nil;
	base.next = freep = &base;
	base.size = 0;
	memset(&base.magic, 0x35, sizeof(base.magic));
}

void
gdump(void)
{
	int i;
	cell *p;

	dprint("gdump> base %G\n", &base);

	dprint("gdump> usedp %G\n", usedp);

	i = 0;
	p = usedp;
	if(p != nil)
		do {
			dprint("gdump> %G\n", p);
			p = UNTAG(p->next);
			i++;
		} while(p != usedp);

	dprint("gdump> %d total usedp\n", i);

	dprint("gdump> freep %G\n", freep);

	i = 0;
	p = freep;
	if(p != nil)
		do {
			dprint("gdump> %G\n", p);
			p = UNTAG(p->next);
			i++;
		} while(p != freep);

	dprint("gdump> %d total freep\n", i);
}

static void
addfree(cell *bp)
{
	cell *p;

	dprint("addfree> %#p size %ld next %#p destructor %#p magic %#lux\n", bp, bp->size, bp->next, bp->d, bp->magic);

	assert((bp->magic & 0x35353535) == 0x35353535);

	for(p = freep; !(bp > p && bp < p->next); p = p->next)
		if(p >= p->next && (bp > p || bp < p->next))
			break;

	if (bp + bp->size == p->next) {
		bp->size += p->next->size;
		bp->next = p->next->next;
	} else
		bp->next = p->next;

	if (p + p->size == bp) {
		p->size += bp->size;
		p->next = bp->next;
	} else
		p->next = bp;

	freep = p;
}

static void
markregion(uintptr *sp, uintptr *ep)
{
	uintptr v;
	cell *bp;

	dprint("markregion> start %#p - %#p\n", sp, ep);

	for(; sp < ep; sp++){
		v = *sp;
		bp = usedp;
		do {
			//dprint("markregion> sp %#p v %#lux bp %#p size %ld next %#p destructor %#p magic %#lux\n", sp, v, bp, bp->size, bp->next, bp->d, bp->magic);
			if(bp + 1 <= (cell*)v && bp + 1 + bp->size > (cell*)v){
				dprint("markregion> %#p in use at %#p\n", bp, sp);
				bp->next = (cell*)((uintptr)bp->next | 1);
			}
		} while((bp = UNTAG(bp->next)) != usedp);		
	}

	dprint("markregion> end\n");
}

static void
markheap(void)
{
	uintptr *vp, v;
	cell *bp, *up;

	dprint("markheap> start\n");

	for(bp = UNTAG(usedp->next); bp != usedp; bp = UNTAG(bp->next)){
		if(!((uintptr)bp->next & 1))
			continue;
		for(vp = (uintptr*)(bp + 1); (cell*)vp < (bp + bp->size + 1); vp++){
			v = *vp;
			up = UNTAG(bp->next);
			do {
				if(up != bp && up + 1 <= (cell*)v && up + 1 + up->size > (cell*)v){
					dprint("markheap> %#p in use at %#p\n", up, vp);
					up->next = (cell*)((uintptr)up->next | 1);
					break;
				}
			} while((up = UNTAG(up->next)) != bp);
		}
	}

	dprint("markheap> end\n");
}

/* bottom of stack */
void*
bos(void)
{
	void *p = nil;
	USED(p);
	return &p;
}

void
gcollect(void)
{
	cell *p, *prevp, *tp;
	extern char etext[];

	dprint("gcollect> enter\n");

	gdump();

	if(usedp == nil)
		return;

	/* mark bss/data */
	dprint("gcollect> scan data\n");
	markregion((uintptr*)etext, (uintptr*)end);

	/* mark stack */
	dprint("gcollect> scan stack\n");
	markregion(bos(), (uintptr*)_tos);

	/* mark heap */
	dprint("gcollect> scan heap\n");
	markheap();

	gdump();

	/* collect */
	dprint("gcollect> collect\n");
	for(prevp = usedp, p = UNTAG(usedp->next);; prevp = p, p = UNTAG(p->next)){
next_chunk:
		if(!((uintptr)p->next & 1)){
			/* not marked, free it */
			dprint("gcollect> freeing %#p %ld\n", p, p->size);
			tp = p;
			p = UNTAG(p->next);
			addfree(tp);

			if(usedp == tp){ 
				usedp = nil;
				break;
			}

			prevp->next = (cell*)((uintptr)p | ((uintptr)(prevp->next) & 1));
			goto next_chunk;
		}

		/* unmark */
		p->next = (cell*)(((uintptr)p->next) & ~1);
		if(p == usedp)
			break;
	}

	gdump();

	dprint("gcollect> end\n");
}

static cell*
getcell(ulong num_units)
{
	void *v;
	cell *up;

	if(num_units/sizeof(cell) < 4096)
		num_units = 4096/sizeof(cell);

	if((v = malloc(num_units * sizeof(cell))) == nil)
		return nil;

	memset(v, 0, num_units * sizeof(cell));

	up = (cell*)v;
	up->size = num_units;
	memset(&up->magic, 0x35, sizeof(up->magic));
	addfree(up);
	return freep;
}

void*
galloc(ulong size)
{
	ulong num_units;
	cell *p, *prevp;

	num_units = (size + sizeof(cell) - 1) / sizeof(cell) + 1;  
	prevp = freep;

	dprint("galloc> %lud bytes %lud * sizeof(cell)\n", size, num_units);

	gdump();

	for(p = prevp->next;; prevp = p, p = p->next){
		if(p->size >= num_units){ /* big enough. */
			if(p->size == num_units) /* exact size. */
				prevp->next = p->next;
			else {
				/* pointer is adjusted here, so also need to fix up magic. */
				p->size -= num_units;
				p += p->size;
				p->size = num_units;
				memset(&p->magic, 0x35, sizeof(p->magic));
			}

			freep = prevp;
			
			/* add to p to the used list. */
			if(usedp == nil)	
				usedp = p->next = p;
			else {
				p->next = usedp->next;
				usedp->next = p;
			}

			dprint("galloc> cell %#p mem %#p\n", p, (void*)(p+1));

			return (void*)(p+1);
		}

		if(p == freep){ /* no free blocks */
			p = getcell(num_units);
			if(p == nil) /* oom */
				return nil;
		}
	}
}

void
gfree(void *ptr)
{
	USED(ptr);
}
