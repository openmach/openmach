
#include <ctype.h>
#include <stdlib.h>

long
atol(const char *str)
{
	long n = 0;
	while (isdigit(*str))
	{
		n = (n * 10) + *str - '0';
		str++;
	}
	return n;
}

