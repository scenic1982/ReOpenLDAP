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

/* Based on mtest2.c - memory-mapped database tester/toy */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "lmdb.h"

#define E(expr) CHECK((rc = (expr)) == MDB_SUCCESS, #expr)
#define RES(err, expr) ((rc = expr) == (err) || (CHECK(!rc, #expr), 0))
#define CHECK(test, msg) ((test) ? (void)0 : ((void)fprintf(stderr, \
	"%s:%d: %s: %s\n", __FILE__, __LINE__, msg, mdb_strerror(rc)), abort()))

int main(int argc,char * argv[])
{
	int i = 0, j = 0, rc;
	MDB_env *env;
	MDB_dbi dbi;
	MDB_val key, data;
	MDB_txn *txn;
	MDB_stat mst;
	MDB_cursor *cursor;
	int count;
	int *values;
	char sval[32] = "";

	srand(time(NULL));

	count = (rand()%384) + 64;
	values = (int *)malloc(count*sizeof(int));

	for(i = 0;i<count;i++) {
		values[i] = rand()%1024;
	}

	E(mdb_env_create(&env));

	E(mdb_env_set_maxreaders(env, 1));
	E(mdb_env_set_mapsize(env, 10485760));
	E(mdb_env_set_maxdbs(env, 4));
	E(mdb_env_open(env, "./testdb", MDB_FIXEDMAP|MDB_NOSYNC, 0664));

	E(mdb_txn_begin(env, NULL, 0, &txn));
	if (mdb_dbi_open(txn, "id1", MDB_CREATE, &dbi) == MDB_SUCCESS)
		E(mdb_drop(txn, dbi, 1));
	E(mdb_dbi_open(txn, "id1", MDB_CREATE, &dbi));

	key.mv_size = sizeof(int);
	key.mv_data = sval;
	data.mv_size = sizeof(sval);
	data.mv_data = sval;

	printf("Adding %d values\n", count);
	for (i=0;i<count;i++) {
		sprintf(sval, "%03x %d foo bar", values[i], values[i]);
		if (RES(MDB_KEYEXIST, mdb_put(txn, dbi, &key, &data, MDB_NOOVERWRITE)))
			j++;
	}
	if (j) printf("%d duplicates skipped\n", j);
	E(mdb_txn_commit(txn));
	E(mdb_env_stat(env, &mst));

	printf("check-preset-a\n");
	E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
	E(mdb_cursor_open(txn, dbi, &cursor));
	int present_a = 0;
	while ((rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
		printf("key: %p %.*s, data: %p %.*s\n",
			key.mv_data,  (int) key.mv_size,  (char *) key.mv_data,
			data.mv_data, (int) data.mv_size, (char *) data.mv_data);
		++present_a;
	}
	CHECK(rc == MDB_NOTFOUND, "mdb_cursor_get");
	CHECK(present_a == count - j, "mismatch");
	mdb_cursor_close(cursor);
	mdb_txn_abort(txn);
	mdb_env_sync(env, 1);

	j=0;
	key.mv_data = sval;
	for (i= count - 1; i > -1; i-= (rand()%5)) {
		j++;
		txn=NULL;
		E(mdb_txn_begin(env, NULL, 0, &txn));
		sprintf(sval, "%03x ", values[i]);
		if (RES(MDB_NOTFOUND, mdb_del(txn, dbi, &key, NULL))) {
			j--;
			mdb_txn_abort(txn);
		} else {
			E(mdb_txn_commit(txn));
		}
	}
	free(values);
	printf("Deleted %d values\n", j);

	printf("check-preset-b.cursor-next\n");
	E(mdb_env_stat(env, &mst));
	E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
	E(mdb_cursor_open(txn, dbi, &cursor));
	int present_b = 0;
	while ((rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
		printf("key: %.*s, data: %.*s\n",
			(int) key.mv_size,  (char *) key.mv_data,
			(int) data.mv_size, (char *) data.mv_data);
		++present_b;
	}
	CHECK(rc == MDB_NOTFOUND, "mdb_cursor_get");
	CHECK(present_b == present_a - j, "mismatch");

	printf("check-preset-b.cursor-prev\n");
	j = 1;
	while ((rc = mdb_cursor_get(cursor, &key, &data, MDB_PREV)) == 0) {
		printf("key: %.*s, data: %.*s\n",
			(int) key.mv_size,  (char *) key.mv_data,
			(int) data.mv_size, (char *) data.mv_data);
		++j;
	}
	CHECK(rc == MDB_NOTFOUND, "mdb_cursor_get");
	CHECK(present_b == j, "mismatch");
	mdb_cursor_close(cursor);
	mdb_txn_abort(txn);

	mdb_dbi_close(env, dbi);
	/********************* LY: kept DB dirty ****************/
	mdb_env_close_ex(env, 1);
	E(mdb_env_create(&env));
	E(mdb_env_set_maxdbs(env, 4));
	E(mdb_env_open(env, "./testdb", MDB_FIXEDMAP|MDB_NOSYNC, 0664));

	printf("check-preset-c.cursor-next\n");
	E(mdb_env_stat(env, &mst));
	E(mdb_txn_begin(env, NULL, MDB_RDONLY, &txn));
	E(mdb_dbi_open(txn, "id1", 0, &dbi));
	E(mdb_cursor_open(txn, dbi, &cursor));
	int present_c = 0;
	while ((rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
		printf("key: %.*s, data: %.*s\n",
			(int) key.mv_size,  (char *) key.mv_data,
			(int) data.mv_size, (char *) data.mv_data);
		++present_c;
	}
	CHECK(rc == MDB_NOTFOUND, "mdb_cursor_get");
	CHECK(present_c == present_a, "mismatch");

	printf("check-preset-d.cursor-prev\n");
	j = 1;
	while ((rc = mdb_cursor_get(cursor, &key, &data, MDB_PREV)) == 0) {
		printf("key: %.*s, data: %.*s\n",
			(int) key.mv_size,  (char *) key.mv_data,
			(int) data.mv_size, (char *) data.mv_data);
		++j;
	}
	CHECK(rc == MDB_NOTFOUND, "mdb_cursor_get");
	CHECK(present_c == j, "mismatch");
	mdb_cursor_close(cursor);
	mdb_txn_abort(txn);

	mdb_dbi_close(env, dbi);
	mdb_env_close_ex(env, 0);

	return 0;
}
