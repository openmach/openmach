/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
/*
 *	File:	ipc/ipc_entry.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Primitive functions to manipulate translation entries.
 */

#include <mach/kern_return.h>
#include <mach/port.h>
#include <kern/assert.h>
#include <kern/sched_prim.h>
#include <kern/zalloc.h>
#include <ipc/port.h>
#include <ipc/ipc_types.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_splay.h>
#include <ipc/ipc_hash.h>
#include <ipc/ipc_table.h>
#include <ipc/ipc_object.h>

zone_t ipc_tree_entry_zone;

/*
 *	Routine:	ipc_entry_tree_collision
 *	Purpose:
 *		Checks if "name" collides with an allocated name
 *		in the space's tree.  That is, returns TRUE
 *		if the splay tree contains a name with the same
 *		index as "name".
 *	Conditions:
 *		The space is locked (read or write) and active.
 */

boolean_t
ipc_entry_tree_collision(
	ipc_space_t	space,
	mach_port_t	name)
{
	mach_port_index_t index;
	mach_port_t lower, upper;

	assert(space->is_active);

	/*
	 *	Check if we collide with the next smaller name
	 *	or the next larger name.
	 */

	ipc_splay_tree_bounds(&space->is_tree, name, &lower, &upper);

	index = MACH_PORT_INDEX(name);
	return (((lower != ~0) && (MACH_PORT_INDEX(lower) == index)) ||
		((upper != 0) && (MACH_PORT_INDEX(upper) == index)));
}

/*
 *	Routine:	ipc_entry_lookup
 *	Purpose:
 *		Searches for an entry, given its name.
 *	Conditions:
 *		The space must be read or write locked throughout.
 *		The space must be active.
 */

ipc_entry_t
ipc_entry_lookup(space, name)
	ipc_space_t space;
	mach_port_t name;
{
	mach_port_index_t index;
	ipc_entry_t entry;

	assert(space->is_active);

	index = MACH_PORT_INDEX(name);
	if (index < space->is_table_size) {
		entry = &space->is_table[index];
		if (IE_BITS_GEN(entry->ie_bits) != MACH_PORT_GEN(name))
			if (entry->ie_bits & IE_BITS_COLLISION) {
				assert(space->is_tree_total > 0);
				goto tree_lookup;
			} else
				entry = IE_NULL;
		else if (IE_BITS_TYPE(entry->ie_bits) == MACH_PORT_TYPE_NONE)
			entry = IE_NULL;
	} else if (space->is_tree_total == 0)
		entry = IE_NULL;
	else
	    tree_lookup:
		entry = (ipc_entry_t)
				ipc_splay_tree_lookup(&space->is_tree, name);

	assert((entry == IE_NULL) || IE_BITS_TYPE(entry->ie_bits));
	return entry;
}

/*
 *	Routine:	ipc_entry_get
 *	Purpose:
 *		Tries to allocate an entry out of the space.
 *	Conditions:
 *		The space is write-locked and active throughout.
 *		An object may be locked.  Will not allocate memory.
 *	Returns:
 *		KERN_SUCCESS		A free entry was found.
 *		KERN_NO_SPACE		No entry allocated.
 */

kern_return_t
ipc_entry_get(space, namep, entryp)
	ipc_space_t space;
	mach_port_t *namep;
	ipc_entry_t *entryp;
{
	ipc_entry_t table;
	mach_port_index_t first_free;
	mach_port_t new_name;
	ipc_entry_t free_entry;

	assert(space->is_active);

	table = space->is_table;
	first_free = table->ie_next;

	if (first_free == 0)
		return KERN_NO_SPACE;

	free_entry = &table[first_free];
	table->ie_next = free_entry->ie_next;

	/*
	 *	Initialize the new entry.  We need only
	 *	increment the generation number and clear ie_request.
	 */

    {
	mach_port_gen_t gen;

	assert((free_entry->ie_bits &~ IE_BITS_GEN_MASK) == 0);
	gen = free_entry->ie_bits + IE_BITS_GEN_ONE;
	free_entry->ie_bits = gen;
	free_entry->ie_request = 0;
	new_name = MACH_PORT_MAKE(first_free, gen);
    }

	/*
	 *	The new name can't be MACH_PORT_NULL because index
	 *	is non-zero.  It can't be MACH_PORT_DEAD because
	 *	the table isn't allowed to grow big enough.
	 *	(See comment in ipc/ipc_table.h.)
	 */

	assert(MACH_PORT_VALID(new_name));
	assert(free_entry->ie_object == IO_NULL);

	*namep = new_name;
	*entryp = free_entry;
	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_entry_alloc
 *	Purpose:
 *		Allocate an entry out of the space.
 *	Conditions:
 *		The space is not locked before, but it is write-locked after
 *		if the call is successful.  May allocate memory.
 *	Returns:
 *		KERN_SUCCESS		An entry was allocated.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_NO_SPACE		No room for an entry in the space.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory for an entry.
 */

kern_return_t
ipc_entry_alloc(
	ipc_space_t	space,
	mach_port_t	*namep,
	ipc_entry_t	*entryp)
{
	kern_return_t kr;

	is_write_lock(space);

	for (;;) {
		if (!space->is_active) {
			is_write_unlock(space);
			return KERN_INVALID_TASK;
		}

		kr = ipc_entry_get(space, namep, entryp);
		if (kr == KERN_SUCCESS)
			return kr;

		kr = ipc_entry_grow_table(space);
		if (kr != KERN_SUCCESS)
			return kr; /* space is unlocked */
	}
}

/*
 *	Routine:	ipc_entry_alloc_name
 *	Purpose:
 *		Allocates/finds an entry with a specific name.
 *		If an existing entry is returned, its type will be nonzero.
 *	Conditions:
 *		The space is not locked before, but it is write-locked after
 *		if the call is successful.  May allocate memory.
 *	Returns:
 *		KERN_SUCCESS		Found existing entry with same name.
 *		KERN_SUCCESS		Allocated a new entry.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
ipc_entry_alloc_name(
	ipc_space_t	space,
	mach_port_t	name,
	ipc_entry_t	*entryp)
{
	mach_port_index_t index = MACH_PORT_INDEX(name);
	mach_port_gen_t gen = MACH_PORT_GEN(name);
	ipc_tree_entry_t tree_entry = ITE_NULL;

	assert(MACH_PORT_VALID(name));


	is_write_lock(space);

	for (;;) {
		ipc_entry_t entry;
		ipc_tree_entry_t tentry;
		ipc_table_size_t its;

		if (!space->is_active) {
			is_write_unlock(space);
			if (tree_entry) ite_free(tree_entry);
			return KERN_INVALID_TASK;
		}

		/*
		 *	If we are under the table cutoff,
		 *	there are three cases:
		 *		1) The entry is inuse, for the same name
		 *		2) The entry is inuse, for a different name
		 *		3) The entry is free
		 */

		if ((0 < index) && (index < space->is_table_size)) {
			ipc_entry_t table = space->is_table;

			entry = &table[index];

			if (IE_BITS_TYPE(entry->ie_bits)) {
				if (IE_BITS_GEN(entry->ie_bits) == gen) {
					*entryp = entry;
					if (tree_entry) ite_free(tree_entry);
					return KERN_SUCCESS;
				}
			} else {
				mach_port_index_t free_index, next_index;

				/*
				 *	Rip the entry out of the free list.
				 */

				for (free_index = 0;
				     (next_index = table[free_index].ie_next)
							!= index;
				     free_index = next_index)
					continue;

				table[free_index].ie_next =
					table[next_index].ie_next;

				entry->ie_bits = gen;
				assert(entry->ie_object == IO_NULL);
				entry->ie_request = 0;

				*entryp = entry;
				if (tree_entry) ite_free(tree_entry);
				return KERN_SUCCESS;
			}
		}

		/*
		 *	Before trying to allocate any memory,
		 *	check if the entry already exists in the tree.
		 *	This avoids spurious resource errors.
		 *	The splay tree makes a subsequent lookup/insert
		 *	of the same name cheap, so this costs little.
		 */

		if ((space->is_tree_total > 0) &&
		    ((tentry = ipc_splay_tree_lookup(&space->is_tree, name))
							!= ITE_NULL)) {
			assert(tentry->ite_space == space);
			assert(IE_BITS_TYPE(tentry->ite_bits));

			*entryp = &tentry->ite_entry;
			if (tree_entry) ite_free(tree_entry);
			return KERN_SUCCESS;
		}

		its = space->is_table_next;

		/*
		 *	Check if the table should be grown.
		 *
		 *	Note that if space->is_table_size == its->its_size,
		 *	then we won't ever try to grow the table.
		 *
		 *	Note that we are optimistically assuming that name
		 *	doesn't collide with any existing names.  (So if
		 *	it were entered into the tree, is_tree_small would
		 *	be incremented.)  This is OK, because even in that
		 *	case, we don't lose memory by growing the table.
		 */

		if ((space->is_table_size <= index) &&
		    (index < its->its_size) &&
		    (((its->its_size - space->is_table_size) *
		      sizeof(struct ipc_entry)) <
		     ((space->is_tree_small + 1) *
		      sizeof(struct ipc_tree_entry)))) {
			kern_return_t kr;

			/*
			 *	Can save space by growing the table.
			 *	Because the space will be unlocked,
			 *	we must restart.
			 */

			kr = ipc_entry_grow_table(space);
			assert(kr != KERN_NO_SPACE);
			if (kr != KERN_SUCCESS) {
				/* space is unlocked */
				if (tree_entry) ite_free(tree_entry);
				return kr;
			}

			continue;
		}

		/*
		 *	If a splay-tree entry was allocated previously,
		 *	go ahead and insert it into the tree.
		 */

		if (tree_entry != ITE_NULL) {
			space->is_tree_total++;

			if (index < space->is_table_size)
				space->is_table[index].ie_bits |=
					IE_BITS_COLLISION;
			else if ((index < its->its_size) &&
				 !ipc_entry_tree_collision(space, name))
				space->is_tree_small++;

			ipc_splay_tree_insert(&space->is_tree,
					      name, tree_entry);

			tree_entry->ite_bits = 0;
			tree_entry->ite_object = IO_NULL;
			tree_entry->ite_request = 0;
			tree_entry->ite_space = space;
			*entryp = &tree_entry->ite_entry;
			return KERN_SUCCESS;
		}

		/*
		 *	Allocate a tree entry and try again.
		 */

		is_write_unlock(space);
		tree_entry = ite_alloc();
		if (tree_entry == ITE_NULL)
			return KERN_RESOURCE_SHORTAGE;
		is_write_lock(space);
	}
}

/*
 *	Routine:	ipc_entry_dealloc
 *	Purpose:
 *		Deallocates an entry from a space.
 *	Conditions:
 *		The space must be write-locked throughout.
 *		The space must be active.
 */

void
ipc_entry_dealloc(
	ipc_space_t	space,
	mach_port_t	name,
	ipc_entry_t	entry)
{
	ipc_entry_t table;
	ipc_entry_num_t size;
	mach_port_index_t index;

	assert(space->is_active);
	assert(entry->ie_object == IO_NULL);
	assert(entry->ie_request == 0);

	index = MACH_PORT_INDEX(name);
	table = space->is_table;
	size = space->is_table_size;

	if ((index < size) && (entry == &table[index])) {
		assert(IE_BITS_GEN(entry->ie_bits) == MACH_PORT_GEN(name));

		if (entry->ie_bits & IE_BITS_COLLISION) {
			struct ipc_splay_tree small, collisions;
			ipc_tree_entry_t tentry;
			mach_port_t tname;
			boolean_t pick;
			ipc_entry_bits_t bits;
			ipc_object_t obj;

			/* must move an entry from tree to table */

			ipc_splay_tree_split(&space->is_tree,
					     MACH_PORT_MAKE(index+1, 0),
					     &collisions);
			ipc_splay_tree_split(&collisions,
					     MACH_PORT_MAKE(index, 0),
					     &small);

			pick = ipc_splay_tree_pick(&collisions,
						   &tname, &tentry);
			assert(pick);
			assert(MACH_PORT_INDEX(tname) == index);

			bits = tentry->ite_bits;
			entry->ie_bits = bits | MACH_PORT_GEN(tname);
			entry->ie_object = obj = tentry->ite_object;
			entry->ie_request = tentry->ite_request;
			assert(tentry->ite_space == space);

			if (IE_BITS_TYPE(bits) == MACH_PORT_TYPE_SEND) {
				ipc_hash_global_delete(space, obj,
						       tname, tentry);
				ipc_hash_local_insert(space, obj,
						      index, entry);
			}

			ipc_splay_tree_delete(&collisions, tname, tentry);

			assert(space->is_tree_total > 0);
			space->is_tree_total--;

			/* check if collision bit should still be on */

			pick = ipc_splay_tree_pick(&collisions,
						   &tname, &tentry);
			if (pick) {
				entry->ie_bits |= IE_BITS_COLLISION;
				ipc_splay_tree_join(&space->is_tree,
						    &collisions);
			}

			ipc_splay_tree_join(&space->is_tree, &small);
		} else {
			entry->ie_bits &= IE_BITS_GEN_MASK;
			entry->ie_next = table->ie_next;
			table->ie_next = index;
		}
	} else {
		ipc_tree_entry_t tentry = (ipc_tree_entry_t) entry;

		assert(tentry->ite_space == space);

		ipc_splay_tree_delete(&space->is_tree, name, tentry);

		assert(space->is_tree_total > 0);
		space->is_tree_total--;

		if (index < size) {
			ipc_entry_t ientry = &table[index];

			assert(ientry->ie_bits & IE_BITS_COLLISION);

			if (!ipc_entry_tree_collision(space, name))
				ientry->ie_bits &= ~IE_BITS_COLLISION;
		} else if ((index < space->is_table_next->its_size) &&
			   !ipc_entry_tree_collision(space, name)) {
			assert(space->is_tree_small > 0);
			space->is_tree_small--;
		}
	}
}

/*
 *	Routine:	ipc_entry_grow_table
 *	Purpose:
 *		Grows the table in a space.
 *	Conditions:
 *		The space must be write-locked and active before.
 *		If successful, it is also returned locked.
 *		Allocates memory.
 *	Returns:
 *		KERN_SUCCESS		Grew the table.
 *		KERN_SUCCESS		Somebody else grew the table.
 *		KERN_SUCCESS		The space died.
 *		KERN_NO_SPACE		Table has maximum size already.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate a new table.
 */

kern_return_t
ipc_entry_grow_table(space)
	ipc_space_t space;
{
	ipc_entry_num_t osize, size, nsize;

	do {
		ipc_entry_t otable, table;
		ipc_table_size_t oits, its, nits;
		mach_port_index_t i, free_index;

		assert(space->is_active);

		if (space->is_growing) {
			/*
			 *	Somebody else is growing the table.
			 *	We just wait for them to finish.
			 */

			assert_wait((event_t) space, FALSE);
			is_write_unlock(space);
			thread_block((void (*)()) 0);
			is_write_lock(space);
			return KERN_SUCCESS;
		}

		otable = space->is_table;
		its = space->is_table_next;
		size = its->its_size;
		oits = its - 1;
		osize = oits->its_size;
		nits = its + 1;
		nsize = nits->its_size;

		if (osize == size) {
			is_write_unlock(space);
			return KERN_NO_SPACE;
		}

		assert((osize < size) && (size <= nsize));

		/*
		 *	OK, we'll attempt to grow the table.
		 *	The realloc requires that the old table
		 *	remain in existence.
		 */

		space->is_growing = TRUE;
		is_write_unlock(space);
		if (it_entries_reallocable(oits))
			table = it_entries_realloc(oits, otable, its);
		else
			table = it_entries_alloc(its);
		is_write_lock(space);
		space->is_growing = FALSE;

		/*
		 *	We need to do a wakeup on the space,
		 *	to rouse waiting threads.  We defer
		 *	this until the space is unlocked,
		 *	because we don't want them to spin.
		 */

		if (table == IE_NULL) {
			is_write_unlock(space);
			thread_wakeup((event_t) space);
			return KERN_RESOURCE_SHORTAGE;
		}

		if (!space->is_active) {
			/*
			 *	The space died while it was unlocked.
			 */

			is_write_unlock(space);
			thread_wakeup((event_t) space);
			it_entries_free(its, table);
			is_write_lock(space);
			return KERN_SUCCESS;
		}

		assert(space->is_table == otable);
		assert(space->is_table_next == its);
		assert(space->is_table_size == osize);

		space->is_table = table;
		space->is_table_size = size;
		space->is_table_next = nits;

		/*
		 *	If we did a realloc, it remapped the data.
		 *	Otherwise we copy by hand first.  Then we have
		 *	to clear the index fields in the old part and
		 *	zero the new part.
		 */

		if (!it_entries_reallocable(oits))
			(void) memcpy((void *) table, (const void *) otable,
			      osize * sizeof(struct ipc_entry));

		for (i = 0; i < osize; i++)
			table[i].ie_index = 0;

		(void) memset((void *) (table + osize), 0,
		      (size - osize) * sizeof(struct ipc_entry));

		/*
		 *	Put old entries into the reverse hash table.
		 */

		for (i = 0; i < osize; i++) {
			ipc_entry_t entry = &table[i];

			if (IE_BITS_TYPE(entry->ie_bits) ==
						MACH_PORT_TYPE_SEND)
				ipc_hash_local_insert(space, entry->ie_object,
						      i, entry);
		}

		/*
		 *	If there are entries in the splay tree,
		 *	then we have work to do:
		 *		1) transfer entries to the table
		 *		2) update is_tree_small
		 */

		if (space->is_tree_total > 0) {
			mach_port_index_t index;
			boolean_t delete;
			struct ipc_splay_tree ignore;
			struct ipc_splay_tree move;
			struct ipc_splay_tree small;
			ipc_entry_num_t nosmall;
			ipc_tree_entry_t tentry;

			/*
			 *	The splay tree divides into four regions,
			 *	based on the index of the entries:
			 *		1) 0 <= index < osize
			 *		2) osize <= index < size
			 *		3) size <= index < nsize
			 *		4) nsize <= index
			 *
			 *	Entries in the first part are ignored.
			 *	Entries in the second part, that don't
			 *	collide, are moved into the table.
			 *	Entries in the third part, that don't
			 *	collide, are counted for is_tree_small.
			 *	Entries in the fourth part are ignored.
			 */

			ipc_splay_tree_split(&space->is_tree,
					     MACH_PORT_MAKE(nsize, 0),
					     &small);
			ipc_splay_tree_split(&small,
					     MACH_PORT_MAKE(size, 0),
					     &move);
			ipc_splay_tree_split(&move,
					     MACH_PORT_MAKE(osize, 0),
					     &ignore);

			/* move entries into the table */

			for (tentry = ipc_splay_traverse_start(&move);
			     tentry != ITE_NULL;
			     tentry = ipc_splay_traverse_next(&move, delete)) {
				mach_port_t name;
				mach_port_gen_t gen;
				mach_port_type_t type;
				ipc_entry_bits_t bits;
				ipc_object_t obj;
				ipc_entry_t entry;

				name = tentry->ite_name;
				gen = MACH_PORT_GEN(name);
				index = MACH_PORT_INDEX(name);

				assert(tentry->ite_space == space);
				assert((osize <= index) && (index < size));

				entry = &table[index];

				/* collision with previously moved entry? */

				bits = entry->ie_bits;
				if (bits != 0) {
					assert(IE_BITS_TYPE(bits));
					assert(IE_BITS_GEN(bits) != gen);

					entry->ie_bits =
						bits | IE_BITS_COLLISION;
					delete = FALSE;
					continue;
				}

				bits = tentry->ite_bits;
				type = IE_BITS_TYPE(bits);
				assert(type != MACH_PORT_TYPE_NONE);

				entry->ie_bits = bits | gen;
				entry->ie_object = obj = tentry->ite_object;
				entry->ie_request = tentry->ite_request;

				if (type == MACH_PORT_TYPE_SEND) {
					ipc_hash_global_delete(space, obj,
							       name, tentry);
					ipc_hash_local_insert(space, obj,
							      index, entry);
				}

				space->is_tree_total--;
				delete = TRUE;
			}
			ipc_splay_traverse_finish(&move);

			/* count entries for is_tree_small */

			nosmall = 0; index = 0;
			for (tentry = ipc_splay_traverse_start(&small);
			     tentry != ITE_NULL;
			     tentry = ipc_splay_traverse_next(&small, FALSE)) {
				mach_port_index_t nindex;

				nindex = MACH_PORT_INDEX(tentry->ite_name);

				if (nindex != index) {
					nosmall++;
					index = nindex;
				}
			}
			ipc_splay_traverse_finish(&small);

			assert(nosmall <= (nsize - size));
			assert(nosmall <= space->is_tree_total);
			space->is_tree_small = nosmall;

			/* put the splay tree back together */

			ipc_splay_tree_join(&space->is_tree, &small);
			ipc_splay_tree_join(&space->is_tree, &move);
			ipc_splay_tree_join(&space->is_tree, &ignore);
		}

		/*
		 *	Add entries in the new part which still aren't used
		 *	to the free list.  Add them in reverse order,
		 *	and set the generation number to -1, so that
		 *	early allocations produce "natural" names.
		 */

		free_index = table[0].ie_next;
		for (i = size-1; i >= osize; --i) {
			ipc_entry_t entry = &table[i];

			if (entry->ie_bits == 0) {
				entry->ie_bits = IE_BITS_GEN_MASK;
				entry->ie_next = free_index;
				free_index = i;
			}
		}
		table[0].ie_next = free_index;

		/*
		 *	Now we need to free the old table.
		 *	If the space dies or grows while unlocked,
		 *	then we can quit here.
		 */

		is_write_unlock(space);
		thread_wakeup((event_t) space);
		it_entries_free(oits, otable);
		is_write_lock(space);
		if (!space->is_active || (space->is_table_next != nits))
			return KERN_SUCCESS;

		/*
		 *	We might have moved enough entries from
		 *	the splay tree into the table that
		 *	the table can be profitably grown again.
		 *
		 *	Note that if size == nsize, then
		 *	space->is_tree_small == 0.
		 */
	} while ((space->is_tree_small > 0) &&
		 (((nsize - size) * sizeof(struct ipc_entry)) <
		  (space->is_tree_small * sizeof(struct ipc_tree_entry))));

	return KERN_SUCCESS;
}


#if	MACH_KDB
#include <ddb/db_output.h>
#define	printf	kdbprintf

ipc_entry_t	db_ipc_object_by_name(
			task_t		task,
			mach_port_t	name);


ipc_entry_t
db_ipc_object_by_name(
	task_t		task,
	mach_port_t	name)
{
        ipc_space_t space = task->itk_space;
        ipc_entry_t entry;
 
 
        entry = ipc_entry_lookup(space, name);
        if(entry != IE_NULL) {
                iprintf("(task 0x%x, name 0x%x) ==> object 0x%x",
			entry->ie_object);
                return (ipc_entry_t) entry->ie_object;
        }
        return entry;
}
#endif	/* MACH_KDB */
