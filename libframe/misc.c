/* Copyright (c) 1992 AT&T - All rights reserved. */
#include	<u.h>
#include	<libc.h>
#include	<pwd.h>

#ifdef	HAVE_STDARG_H
#include	<stdarg.h>
#else
#include	<varargs.h>
#endif

#ifdef HAVE_STDARG_H
void
fprint(int fd, char *z, ...)
{
	va_list args;
	char buf[2048];			/* pick reasonable blocksize */

	va_start(args, z);
	vsprintf(buf, z, args);
	write(fd, buf, strlen(buf));
	va_end(args);
}
#else	/* !HAVE_STDARG_H */
void
fprint(va_alist)
va_dcl
{
	int fd;
	char *fmt;
	va_list args;
	char buf[2048];			/* pick reasonable blocksize */

	va_start(args);
	fd = va_arg(args, int);
	fmt = va_arg(args, char *);
	vsprintf(buf, fmt, args);
	write(fd, buf, strlen(buf));
	va_end(args);
}
#endif	/* HAVE_STDARG_H */

#ifndef HAVE_STRERROR
char *
strerror(int n)
{
	extern char *sys_errlist[];
	return sys_errlist[n];
}
#endif /* HAVE_STRERROR */

int errstr(char *buf)
{
	extern int errno;

	strncpy(buf, strerror(errno), ERRLEN);
	return 1;
}

char*
getuser(void)
{
	struct passwd *p;

	static char *user = 0;

	if (!user) {
		p = getpwuid(getuid());
		if (p && p->pw_name) {
			user = malloc(strlen(p->pw_name)+1);
			if (user)
				strcpy(user, p->pw_name);
		}
	}
	if(!user)
		user = "unknown";
	return user;
}

#ifndef HAVE_MEMMOVE
/*
 * memcpy is probably fast, but may not work with overlap
 */
void*
memmove(void *a1, const void *a2, size_t n)
{
	char *s1;
	const char *s2;

	s1 = a1;
	s2 = a2;
	if(s1 > s2)
		goto back;
	if(s1 + n <= s2)
		return memcpy(a1, a2, n);
	while(n > 0) {
		*s1++ = *s2++;
		n--;
	}
	return a1;

back:
	s2 += n;
	if(s2 <= s1)
		return memcpy(a1, a2, n);
	s1 += n;
	while(n > 0) {
		*--s1 = *--s2;
		n--;
	}
	return a1;
}
#endif /* HAVE_MEMMOVE */
