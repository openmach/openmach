
#include "trap.h"

void trap_dump_die(struct trap_state *st)
{
	about_to_die(1);

	trap_dump(st);

	die("terminated due to trap\n");
}

