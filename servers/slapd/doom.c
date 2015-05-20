/*
    Copyright (c) 2015 Leonid Yuriev <leo@yuriev.ru>.
    Copyright (c) 2015 Peter-Service R&D LLC.

    This file is part of ReOpenLDAP.

    ReOpenLDAP is free software; you can redistribute it and/or modify it under
    the terms of the GNU Affero General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ReOpenLDAP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "portable.h"

#include "slap.h"
#include "proto-slap.h"
#include "lber_hipagut.h"

#if SLAPD_MDB == SLAPD_MOD_STATIC
#	include "../../libraries/liblmdb/lmdb.h"
#endif /* SLAPD_MDB */

int reopenldap_flags
#if (LDAP_MEMORY_DEBUG > 1) || (LDAP_DEBUG > 1)
		= REOPENLDAP_FLAG_IDKFA
#endif
		;

void __attribute__((constructor)) reopenldap_flags_init() {
	int flags = reopenldap_flags;
	if (getenv("REOPENLDAP_FORCE_IDKFA"))
		flags |= REOPENLDAP_FLAG_IDKFA;
	if (getenv("REOPENLDAP_FORCE_IDDQD"))
		flags |= REOPENLDAP_FLAG_IDDQD;
	reopenldap_flags_setup(flags);
}

void reopenldap_flags_setup(int flags) {
	reopenldap_flags = flags & (REOPENLDAP_FLAG_IDDQD | REOPENLDAP_FLAG_IDKFA);

#if LDAP_MEMORY_DEBUG > 0
	if (reopenldap_mode_idkfa()) {
		lber_hug_nasty_disabled = 0;
#ifdef LDAP_MEMORY_TRACE
		lber_hug_memchk_trace_disabled = 0;
#endif /* LDAP_MEMORY_TRACE */
		lber_hug_memchk_poison_alloc = 0xCC;
		lber_hug_memchk_poison_free = 0xDD;
	} else {
		lber_hug_nasty_disabled = LBER_HUG_DISABLED;
		lber_hug_memchk_trace_disabled = LBER_HUG_DISABLED;
		lber_hug_memchk_poison_alloc = 0;
		lber_hug_memchk_poison_free = 0;
	}
#endif /* LDAP_MEMORY_DEBUG */

#if SLAPD_MDB == SLAPD_MOD_STATIC
	flags = mdb_setup_debug(MDB_DBG_DNT, (MDB_debug_func*) MDB_DBG_DNT, MDB_DBG_DNT);
	flags &= ~(MDB_DBG_TRACE | MDB_DBG_EXTRA | MDB_DBG_ASSERT);
	if (reopenldap_mode_idkfa())
		flags |=
#	if LDAP_DEBUG > 2
				MDB_DBG_TRACE | MDB_DBG_EXTRA |
#	endif /* LDAP_DEBUG > 2 */
				MDB_DBG_ASSERT;

	mdb_setup_debug(flags, (MDB_debug_func*) MDB_DBG_DNT, MDB_DBG_DNT);
#endif /* SLAPD_MDB */
}
