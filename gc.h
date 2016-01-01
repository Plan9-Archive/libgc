typedef void (destructor)(void *ptr, void *ctx);

void	ginit(void);
void	gcollect(void);
void*	galloc(ulong size);
void	gfree(void *ptr);

void*	gallocd(ulong size, destructor *d, void *ctx);
