// profile.c
//	  Commands to manipulate profiles.
//	  Profiles are used to control login behaviour such as failed attempts.
//
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
//
// The following only applies to changes made to this file as part of YugaByte
// development.
//
// Portions Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License.  You may obtain a copy
// of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations under
// the License.

#include "postgres.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/reloptions.h"
#include "access/sysattr.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_yb_profile.h"
#include "catalog/pg_yb_role_profile.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "commands/profile.h"
#include "commands/seclabel.h"
#include "commands/tablecmds.h"
#include "commands/ybccmds.h"
#include "common/file_perm.h"
#include "miscadmin.h"
#include "postmaster/bgwriter.h"
#include "storage/fd.h"
#include "storage/lmgr.h"
#include "storage/standby.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/tqual.h"
#include "utils/varlena.h"

#include "yb/yql/pggate/ybc_pggate.h"
#include "executor/ybcModifyTable.h"
#include "pg_yb_utils.h"

#define DEFAULT_PROFILE_OID 8055
/*
 * Create a profile.
 */
Oid
CreateProfile(CreateProfileStmt *stmt)
{
	Relation  rel;
	Datum	  values[Natts_pg_yb_profile];
	bool	  nulls[Natts_pg_yb_profile];
	HeapTuple tuple;
	Oid		  prfoid;

	/* If not superuser check privileges */
	if (!superuser())
	{
		AclResult aclresult;
		// Check that user has create privs on the database to allow creation
		// of a new profile.
		aclresult = pg_database_aclcheck(MyDatabaseId, GetUserId(), ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, OBJECT_DATABASE,
						   get_database_name(MyDatabaseId));
	}

	/*
	 * Check that there is no other profile by this name.
	 */
	if (OidIsValid(get_profile_oid(stmt->prfname, true)))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("profile \"%s\" already exists", stmt->prfname)));

	/*
	 * Insert tuple into pg_yb_profile.
	 */
	rel = heap_open(YbProfileRelationId, RowExclusiveLock);

	MemSet(nulls, false, sizeof(nulls));

	values[Anum_pg_yb_profile_prfname - 1] =
		DirectFunctionCall1(namein, CStringGetDatum(stmt->prfname));
	values[Anum_pg_yb_profile_prffailedloginattempts - 1] =
		intVal(stmt->prffailedloginattempts);

	tuple = heap_form_tuple(rel->rd_att, values, nulls);

	prfoid = CatalogTupleInsert(rel, tuple);

	heap_freetuple(tuple);

	/* We keep the lock on pg_yb_profile until commit */
	heap_close(rel, NoLock);

	return prfoid;
}

/*
 * get_profile_oid - given a profile name, look up the OID
 *
 * If missing_ok is false, throw an error if profile name not found.
 * If true, just return InvalidOid.
 */
Oid
get_profile_oid(const char *prfname, bool missing_ok)
{
	Oid			 result;
	Relation	 rel;
	HeapScanDesc scandesc;
	HeapTuple	 tuple;
	ScanKeyData	 entry[1];

	/*
	 * Search pg_yb_profile.
	 */
	rel = heap_open(YbProfileRelationId, AccessShareLock);

	ScanKeyInit(&entry[0], Anum_pg_yb_profile_prfname,
				BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(prfname));
	scandesc = heap_beginscan_catalog(rel, 1, entry);
	tuple = heap_getnext(scandesc, ForwardScanDirection);

	/* We assume that there can be at most one matching tuple */
	if (HeapTupleIsValid(tuple))
		result = HeapTupleGetOid(tuple);
	else
		result = InvalidOid;

	heap_endscan(scandesc);
	heap_close(rel, AccessShareLock);

	if (!OidIsValid(result) && !missing_ok)
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("profile \"%s\" does not exist", prfname)));

	return result;
}

HeapTuple
get_profile_tuple(Oid prfoid)
{
	Relation	 rel;
	HeapScanDesc scandesc;
	HeapTuple	 tuple;
	ScanKeyData	 entry[1];

	/*
	 * Search pg_yb_role_profile.
	 */
	rel = heap_open(YbProfileRelationId, AccessShareLock);

	ScanKeyInit(&entry[0], ObjectIdAttributeNumber,
				BTEqualStrategyNumber, F_OIDEQ, prfoid);
	scandesc = heap_beginscan_catalog(rel, 1, entry);
	tuple = heap_getnext(scandesc, ForwardScanDirection);

	/* Must copy tuple before releasing buffer */
	if (HeapTupleIsValid(tuple))
		tuple = heap_copytuple(tuple);

	heap_endscan(scandesc);
	heap_close(rel, AccessShareLock);

	return tuple;
}

/*
 * get_profile_name - given a profile OID, look up the name
 *
 * Returns a palloc'd string, or NULL if no such profile.
 * TODO: Profiles are not part of cache. Code has to iterate the heap.
 */
char *
get_profile_name(Oid prfoid)
{
	char	 *result;
	HeapTuple tuple;

	tuple = get_profile_tuple(prfoid);

	/* We assume that there can be at most one matching tuple */
	if (HeapTupleIsValid(tuple))
	{
		result = pstrdup(
			NameStr(((Form_pg_yb_profile) GETSTRUCT(tuple))->prfname));
	}
	else
		result = NULL;

	return result;
}

/*
 * RemoveProfileById - remove a profile by its OID.
 * If a profile does not exist with the provided oid, then an error is
 * raised.
 *
 * grp_oid - the oid of the profile.
 */
void
RemoveProfileById(Oid prf_oid)
{
	Relation	 pg_profile_rel;
	HeapScanDesc scandesc;
	ScanKeyData	 skey[1];
	HeapTuple	 tuple;

	if (prf_oid == DEFAULT_PROFILE_OID)
	{
		ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					errmsg("profile \"default\" cannot be deleted")));
	}

	pg_profile_rel = heap_open(YbProfileRelationId, RowExclusiveLock);

	/*
	 * Find the profile to delete.
	 */
	ScanKeyInit(&skey[0], ObjectIdAttributeNumber, BTEqualStrategyNumber,
				F_OIDEQ, ObjectIdGetDatum(prf_oid));
	scandesc = heap_beginscan_catalog(pg_profile_rel, 1, skey);
	tuple = heap_getnext(scandesc, ForwardScanDirection);

	/* If the profile exists, then remove it, otherwise raise an error. */
	if (!HeapTupleIsValid(tuple))
	{
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("profile "
																  "with oid %u "
																  "does not "
																  "exist",
																  prf_oid)));
	}

	/* DROP hook for the profile being removed */
	InvokeObjectDropHook(YbProfileRelationId, prf_oid, 0);

	/*
	 * Remove the pg_yb_profile tuple
	 */
	CatalogTupleDelete(pg_profile_rel, tuple);

	heap_endscan(scandesc);

	/* We keep the lock on pg_yb_profile until commit */
	heap_close(pg_profile_rel, NoLock);
}

/************************
 * Role Profile Functions
 *************************/

/*
 * Create a role profile.
 */
Oid
CreateRoleProfile(Oid rolid, const char* prfname)
{
	Relation  rel;
	Datum	  values[Natts_pg_yb_role_profile];
	bool	  nulls[Natts_pg_yb_role_profile];
	HeapTuple tuple;
	Oid		  rolprfoid;

	/* If not superuser check privileges */
	if (!superuser())
	{
		AclResult aclresult;
		// Check that user has create privs on the database to allow creation
		// of a new profile.
		aclresult = pg_database_aclcheck(MyDatabaseId, GetUserId(), ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, OBJECT_PROFILE,
						   get_database_name(MyDatabaseId));
	}

	/*
	 * Check that there is a profile by this name.
	 */
	Oid prfoid = get_profile_oid(prfname, true);
	if (!OidIsValid(prfoid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("profile \"%s\" does not exist", prfname)));

	/*
	 * Check that there isnt another entry for role profile
	 */
	if (OidIsValid(get_role_profile_oid(rolid, true)))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("role \"%d\" is associated with a profile \"%s\"",
					 rolid, prfname)));


	/*
	 * Insert tuple into pg_yb_role_profile.
	 */
	rel = heap_open(YbRoleProfileRelationId, RowExclusiveLock);

	MemSet(nulls, false, sizeof(nulls));

	values[Anum_pg_yb_role_profile_rolid - 1] = rolid;
	values[Anum_pg_yb_role_profile_prfid - 1] = prfoid;
	values[Anum_pg_yb_role_profile_rolfailedloginattempts - 1] = 0;
	values[Anum_pg_yb_role_profile_rollockedat - 1] = 0;
	values[Anum_pg_yb_role_profile_rolisenabled - 1] = true;

	tuple = heap_form_tuple(rel->rd_att, values, nulls);

	rolprfoid = CatalogTupleInsert(rel, tuple);

	heap_freetuple(tuple);

	/* We keep the lock on pg_yb_login_profile until commit */
	heap_close(rel, NoLock);

	return rolprfoid;
}

HeapTuple
get_role_profile_tuple(Oid rolid)
{
	Relation	 rel;
	HeapScanDesc scandesc;
	HeapTuple	 tuple;
	ScanKeyData	 entry[1];

	/*
	 * Search pg_yb_role_profile.
	 */
	rel = heap_open(YbRoleProfileRelationId, AccessShareLock);

	ScanKeyInit(&entry[0], Anum_pg_yb_role_profile_rolid,
				BTEqualStrategyNumber, F_OIDEQ, rolid);
	scandesc = heap_beginscan_catalog(rel, 1, entry);
	tuple = heap_getnext(scandesc, ForwardScanDirection);

	/* Must copy tuple before releasing buffer */
	if (HeapTupleIsValid(tuple))
		tuple = heap_copytuple(tuple);

	heap_endscan(scandesc);
	heap_close(rel, AccessShareLock);

	return tuple;
}


/*
 * get_role_profile_oid - given a profile name, look up the OID
 *
 * If missing_ok is false, throw an error if profile name not found.
 * If true, just return InvalidOid.
 */
Oid
get_role_profile_oid(Oid rolid, bool missing_ok)
{
	Oid			 result;
	HeapTuple	 tuple;

	tuple = get_role_profile_tuple(rolid);

	/* We assume that there can be at most one matching tuple */
	if (HeapTupleIsValid(tuple))
		result = HeapTupleGetOid(tuple);
	else
		result = InvalidOid;

	if (!OidIsValid(result) && !missing_ok)
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("role \"%d\" is not associated with a profile",
							rolid)));

	return result;
}

/*
 * EnableRoleProfile - set rolisenabled flag
 *
 * roleid - the oid of the role
 * isenabled - bool value
 */
Oid
update_role_profile(Oid roleid, Datum *new_record,
		bool* new_record_nulls, bool* new_record_repl,
		bool missing_ok)
{
	Relation	pg_yb_role_profile_rel;
	TupleDesc	pg_yb_role_profile_dsc;
	HeapTuple	tuple, new_tuple;
	Oid 		roleprfid;

	pg_yb_role_profile_rel = heap_open(YbRoleProfileRelationId, RowExclusiveLock);
	pg_yb_role_profile_dsc = RelationGetDescr(pg_yb_role_profile_rel);

	tuple = get_role_profile_tuple(roleid);

	/* We assume that there can be at most one matching tuple */
	if (HeapTupleIsValid(tuple))
	{
		roleprfid = HeapTupleGetOid(tuple);
		new_tuple = heap_modify_tuple(tuple, pg_yb_role_profile_dsc, new_record,
								  new_record_nulls, new_record_repl);
		CatalogTupleUpdate(pg_yb_role_profile_rel, &tuple->t_self, new_tuple);

		InvokeObjectPostAlterHook(YbRoleProfileRelationId, roleprfid, 0);

		heap_freetuple(new_tuple);
	}
	else if (!missing_ok)
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("role \"%d\" is not associated with a profile",
							roleid)));
	else
		roleprfid = InvalidOid;

	/*
	 * Close pg_yb_role_login, but keep lock till commit.
	 */
	heap_close(pg_yb_role_profile_rel, NoLock);

	return roleprfid;
}

/*
 * EnableRoleProfile - set rolisenabled flag
 *
 * roleid - the oid of the role
 * isenabled - bool value
 */
Oid
EnableRoleProfile(Oid roleid, bool isEnabled)
{
	Datum		new_record[Natts_pg_yb_role_profile];
	bool		new_record_nulls[Natts_pg_yb_role_profile];
	bool		new_record_repl[Natts_pg_yb_role_profile];

	/*
	 * Build an updated tuple with isEnabled set to the new value
	 */
	MemSet(new_record, 0, sizeof(new_record));
	MemSet(new_record_nulls, false, sizeof(new_record_nulls));
	MemSet(new_record_repl, false, sizeof(new_record_repl));

	new_record[Anum_pg_yb_role_profile_rolisenabled - 1] = BoolGetDatum(isEnabled);
	new_record_repl[Anum_pg_yb_role_profile_rolisenabled - 1] = true;
	new_record[Anum_pg_yb_role_profile_rolfailedloginattempts - 1] = Int16GetDatum(0);
	new_record_repl[Anum_pg_yb_role_profile_rolfailedloginattempts - 1] = true;

	return update_role_profile(roleid, new_record, new_record_nulls,
			new_record_repl, false);
}

/*
 * ResetProfileFailedAttempts - reset failed_attempts counter
 *
 * roleid - the oid of the role
 */
void
ResetProfileFailedAttempts(Oid roleid)
{
	HeapTuple rolprftuple = get_role_profile_tuple(roleid);

	if (!HeapTupleIsValid(rolprftuple))
		// Role is not associated with a profile.
		return;

	YBCExecuteUpdateLoginAttempts(roleid, 0, true);
}

/*
 * IncFailedAttemptsAndMaybeDisableProfile - increment failed_attempts counter and disable if it
 *                             exceeds limit
 *
 * roleid - the oid of the role
 */
void
IncFailedAttemptsAndMaybeDisableProfile(Oid roleid)
{
	HeapTuple rolprftuple = get_role_profile_tuple(roleid);

	if (!HeapTupleIsValid(rolprftuple))
		// Role is not associated with a profile.
		return;

	Form_pg_yb_role_profile rolprfform = (Form_pg_yb_role_profile)
									GETSTRUCT(rolprftuple);

	HeapTuple prftuple = get_profile_tuple(DatumGetObjectId(rolprfform->prfid));
	if (!HeapTupleIsValid(prftuple))
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("Profile \"%d\" not found!",
							roleid)));

	Form_pg_yb_profile prfform = (Form_pg_yb_profile) GETSTRUCT(prftuple);

	int failed_attempts = DatumGetInt16(rolprfform->rolfailedloginattempts) + 1;
	int failed_attempts_limit = DatumGetInt16(prfform->prffailedloginattempts);

	// Keep role enabled IFF role is enabled AND failed attempts < limit
	bool rolisenabled = rolprfform->rolisenabled &&
						(failed_attempts <= failed_attempts_limit);

	YBCExecuteUpdateLoginAttempts(roleid, failed_attempts, rolisenabled);
	CommitTransactionCommand();
}

/*
 * RemoveRoleProfileById - detach a role from profile.
 *
 * rolprfoid - the oid of the role_profile entry.
 */
void
RemoveRoleProfileById(Oid rolprfoid)
{
	Relation	 pg_role_profile_rel;
	HeapScanDesc scandesc;
	ScanKeyData	 skey[1];
	HeapTuple	 tuple;

	pg_role_profile_rel = heap_open(YbRoleProfileRelationId, RowExclusiveLock);

	/*
	 * Find the profile to delete.
	 */
	ScanKeyInit(&skey[0], ObjectIdAttributeNumber, BTEqualStrategyNumber,
				F_OIDEQ, ObjectIdGetDatum(rolprfoid));
	scandesc = heap_beginscan_catalog(pg_role_profile_rel, 1, skey);
	tuple = heap_getnext(scandesc, ForwardScanDirection);

	/* If the profile exists, then remove it, otherwise raise an error. */
	if (!HeapTupleIsValid(tuple))
	{
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role %d is not"
																  " attached "
																  "to a "
																  "profile",
																  rolprfoid)));
	}

	/* DROP hook for the profile being removed */
	InvokeObjectDropHook(YbRoleProfileRelationId, rolprfoid, 0);

	/*
	 * Remove the pg_yb_role_profile tuple
	 */
	CatalogTupleDelete(pg_role_profile_rel, tuple);

	heap_endscan(scandesc);

	/* We keep the lock on pg_yb_login_profile until commit */
	heap_close(pg_role_profile_rel, NoLock);
}

