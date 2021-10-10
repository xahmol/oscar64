#include "string.h"

#if 1

char * strcpy(char * dst, const char * src)
{
	__asm 
	{
		lda dst
		sta accu
		lda dst + 1
		sta accu + 1

		ldy #0
	L1:	lda (src), y
		sta (dst), y
		beq W1
		iny
		lda (src), y
		sta (dst), y
		beq W1
		iny
		bne L1
		inc src + 1
		inc dst + 1
		bne L1
	W1:

	}
}

#else
char * strcpy(char * dst, const char * src)
{
	char * d = dst;
	const char * s = src;

	do {} while (*d++ = *s++);
	
	return dst;
}
#endif

#if 1

int strcmp(const char * ptr1, const char * ptr2)
{
	__asm 
	{
		ldy #0
	L1: lda (ptr1), y
		beq W1
		cmp (ptr2), y
		bne W2
		iny
		bne L1
		inc ptr1 + 1
		inc ptr2 + 1
		bne L1
	W2:	bcs gt
		lda #$ff
		sta accu
		bmi E

	gt: lda #$01
		sta accu
		lda #$00
		beq	E

	W1:	cmp (ptr2), y
		bne W2
		lda #$00
		sta accu
	E:
		sta accu + 1
	}
}

#else
int strcmp(const char * ptr1, const char * ptr2)
{
	const char	*	p = ptr1, * q = ptr2;
	char		c, d;
	while ((c = *p++) == (d = *q++))
	{
		if (!c)
			return 0;
	}
	if (c < d)
		return -1;
	else
		return 1;
}
#endif


int strlen(const char * str)
{
	const char	*	s = str;
	
	int i = 0;
	while (s[i])
		i++;
	return i;
}

char * strcat(char * dst, const char * src)
{
	char * d = dst;
	const char * s = src;

	while (*d)
		d++;
	
	do {} while (*d++ = *s++);
	
	return dst;
}	

void * memset(void * dst, int value, int size)
{
	__asm
	{
			lda	value

			ldx	size + 1
			beq	_w1
			ldy	#0
	_loop1:
			sta (dst), y
			iny
			bne	_loop1
			inc dst + 1
			dex
			bne	_loop1
	_w1:
			ldy	size
			beq	_w2
	_loop2:
			dey
			sta (dst), y
			bne _loop2
	_w2:
	}
	return dst;
}
	

void * memclr(void * dst, int size)
{
	char	*	d = dst;
	while (size--)
		*d++ = 0;
	return dst;
}	

void * memcpy(void * dst, const void * src, int size)
{
	char	*	d = dst, * s = src;
	while (size--)
		*d++ = *s++;
	return dst;
}

void * memmove(void * dst, const void * src, int size)
{
	char	*	d = dst, * s = src;
	if (d < s)
	{
		while (size--)
			*d++ = *s++;
	}
	else if (d > s)
	{
		d += size;
		s += size;
		while (size--)
			*--d = *--s;
	}	
	return dst;
}

int memcmp(const void * ptr1, const void * ptr2, int size)
{
	char	*	p = ptr1, * q = ptr2;
	char		c, d;

	while (size--)
	{
		c = *p++;
		d = *q++;
		if (c < d)
			return -1;
		else if (c > d)
			return 1;
	}
	
	return 0;
}
	


