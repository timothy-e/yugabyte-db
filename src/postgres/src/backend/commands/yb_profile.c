/*------------------------------------------------------------------------------
 *
 * yb_profile.c
 *        Commands to implement PROFILE functionality.
 *
 * Copyright (c) Yugabyte, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 * IDENTIFICATION
 *        src/backend/commands/yb_profile.c
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "access/reloptions.h"
#include "access/sysattr.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_authid.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"
#include "utils/syscache.h"

#include "catalog/pg_yb_profile.h"
#include "catalog/pg_yb_role_profile.h"
#include "commands/yb_profile.h"
#include "yb/yql/pggate/ybc_pggate.h"
#include "executor/ybcModifyTable.h"

static void
CheckProfileCatalogsExist()
{
	/*
	 * First check that the pg_yb_profile or pg_yb_role_profile catalogs
	 * actually exist.
	 */
	if (!YbLoginProfileCatalogsExist)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("Login profile system catalogs do not exist.")));
}

/*
 * Create a profile.
 */
Oid
YbCreateProfile(CreateProfileStmt *stmt)
{
	Relation  rel;
	Datum	  values[Natts_pg_yb_profile];
	bool	  nulls[Natts_pg_yb_profile];
	HeapTuple tuple;
	Oid		  prfid;

	CheckProfileCatalogsExist();

	/* Must be super user or yb_db_admin role */
	if (!superuser() && !IsYbDbAdminUser(GetUserId()))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied to create profile \"%s\"",
						stmt->prfname),
				 errhint("Must be superuser or a member of the yb_db_admin "
						 "role to create a profile.")));

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
	values[Anum_pg_yb_profile_prfmaxfailedloginattempts - 1] =
		intVal(stmt->prffailedloginattempts);
	/* Set lock time to 0 as it is not implemented yet. */
	values[Anum_pg_yb_profile_prfpasswordlocktime - 1] = 0;

	tuple = heap_form_tuple(rel->rd_att, values, nulls);

	prfid = CatalogTupleInsert(rel, tuple);

	heap_freetuple(tuple);

	/* We keep the lock on pg_yb_profile until commit */
	heap_close(rel, NoLock);

	return prfid;
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

	CheckProfileCatalogsExist();

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
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("profile \"%s\" does not exist", prfname)));

	return result;
}

HeapTuple
get_profile_tuple(Oid prfid)
{
	Relation	 rel;
	HeapScanDesc scandesc;
	HeapTuple	 tuple;
	ScanKeyData	 entry[1];

	CheckProfileCatalogsExist();

	/*
	 * Search pg_yb_role_profile.
	 */
	rel = heap_open(YbProfileRelationId, AccessShareLock);

	ScanKeyInit(&entry[0], ObjectIdAttributeNumber,
				BTEqualStrategyNumber, F_OIDEQ, prfid);
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
 * prfid: OID of the profile.
 *
 * Returns a palloc'd string.
 */
char *
get_profile_name(Oid prfid)
{
	HeapTuple tuple;

	tuple = get_profile_tuple(prfid);

	/* We assume that there can be at most one matching tuple */
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "could not find tuple for profile %u", prfid);

	return pstrdup(NameStr(((Form_pg_yb_profile) GETSTRUCT(tuple))->prfname));
}

/*
 * DROP PROFILE
 */
void
YbDropProfile(DropProfileStmt *stmt)
{
	char	   *prfname = stmt->prfname;
	HeapScanDesc scandesc;
	Relation	rel;
	HeapTuple	tuple;
	ScanKeyData entry[1];
	Oid			prfid;
	char	   *detail;
	char	   *detail_log;

	CheckProfileCatalogsExist();

	/* A profile can only be dropped by the super user or yb_db_admin */
	if (!superuser() && !IsYbDbAdminUser(GetUserId()))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied to drop profile"),
				 errhint("Must be superuser or a member of the yb_db_admin "
						 "role to drop a profile.")));

	/*
	 * Find the target tuple
	 */
	rel = heap_open(YbProfileRelationId, RowExclusiveLock);

	/*
	 * Find the profile to delete.
	 */
	ScanKeyInit(&entry[0],
				Anum_pg_yb_profile_prfname,
				BTEqualStrategyNumber,
				F_NAMEEQ,
				CStringGetDatum(prfname));
	scandesc = heap_beginscan_catalog(rel, 1, entry);
	tuple = heap_getnext(scandesc, ForwardScanDirection);

	if (!HeapTupleIsValid(tuple))
	{
		if (!stmt->missing_ok)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("profile \"%s\" does not exist",
							prfname)));
		else
		{
			ereport(NOTICE,
					(errmsg("profile \"%s\" does not exist, skipping",
							prfname)));
			/* XXX I assume I need one or both of these next two calls */
			heap_endscan(scandesc);
			heap_close(rel, NoLock);
		}
		return;
	}

	prfid = HeapTupleGetOid(tuple);

	/*
	 * TODO(profile): disallow drop of the default profile once we introduce a
	 * default profile.
	 */

	/* Check for pg_shdepend entries depending on this profile */
	if (checkSharedDependencies(YbProfileRelationId, prfid,
								&detail, &detail_log))
	{
		if (IsYugaByteEnabled() && detail != NULL)
			detail = YBDetailSorted(detail);
		ereport(ERROR,
				(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
				 errmsg("profile \"%s\" cannot be dropped "
						"because some objects depend on it",
						prfname),
				 errdetail_internal("%s", detail),
				 errdetail_log("%s", detail_log)));
	}

	/*
	 * Check if there are snapshot schedules, disallow dropping in such cases.
	 * TODO(profile): determine if this limitation is really needed.
	 */
	if (IsYugaByteEnabled())
	{
		bool is_active;
		HandleYBStatus(YBCPgCheckIfPitrActive(&is_active));
		if (is_active)
			ereport(ERROR,
					(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
					 errmsg("profile \"%s\" cannot be dropped. "
							"Dropping profiles is not allowed on clusters "
							"with Point in Time Restore activated.",
							prfname)));
	}

	/* DROP hook for the profile being removed */
	InvokeObjectDropHook(YbProfileRelationId, prfid, 0);

	/*
	 * Remove the pg_yb_profile tuple
	 */
	CatalogTupleDelete(rel, tuple);

	heap_endscan(scandesc);

	/*
	 * Remove any comments or security labels on this profile.
	 * TODO(profile): implement this if needed.
	 */

	/*
	 * There is no owner to remove a shared dependency record for since
	 * profiles are currently owned by any superuser or yb_db_admin.
	 */

	/* We keep the lock on pg_yb_profile until commit */
	heap_close(rel, NoLock);
}

/************************
 * Role Profile Functions
 *************************/

/*
 * Create a new map between role & profile.
 */
static Oid
create_role_profile_map(Oid roleid, Oid prfid)
{
	Relation  rel;
	Datum	  values[Natts_pg_yb_role_profile];
	bool	  nulls[Natts_pg_yb_role_profile];
	HeapTuple tuple;
	Oid		  roleprfid;

	CheckProfileCatalogsExist();

	/*
	 * Insert tuple into pg_yb_role_profile.
	 */
	rel = heap_open(YbRoleProfileRelationId, RowExclusiveLock);

	MemSet(nulls, false, sizeof(nulls));

	values[Anum_pg_yb_role_profile_rolprfrole - 1] = roleid;
	values[Anum_pg_yb_role_profile_rolprfprofile - 1] = prfid;
	values[Anum_pg_yb_role_profile_rolprffailedloginattempts - 1] = 0;
	values[Anum_pg_yb_role_profile_rolprfstatus - 1] = ROLPRFSTATUS_OPEN;

	nulls[Anum_pg_yb_role_profile_rolprflockeduntil - 1] = true;

	tuple = heap_form_tuple(rel->rd_att, values, nulls);

	roleprfid = CatalogTupleInsert(rel, tuple);

	recordDependencyOnProfile(AuthIdRelationId, roleid, prfid);

	heap_freetuple(tuple);

	/* We keep the lock on pg_yb_login_profile until commit */
	heap_close(rel, NoLock);

	return roleprfid;
}

HeapTuple
get_role_profile_tuple_by_role_oid(Oid roleid)
{
	Relation	 rel;
	HeapScanDesc scandesc;
	HeapTuple	 tuple;
	ScanKeyData	 entry[1];

	CheckProfileCatalogsExist();

	/*
	 * Search pg_yb_role_profile.
	 */
	rel = heap_open(YbRoleProfileRelationId, AccessShareLock);

	ScanKeyInit(&entry[0], Anum_pg_yb_role_profile_rolprfrole,
				BTEqualStrategyNumber, F_OIDEQ, roleid);
	scandesc = heap_beginscan_catalog(rel, 1, entry);
	tuple = heap_getnext(scandesc, ForwardScanDirection);

	/* Must copy tuple before releasing buffer */
	if (HeapTupleIsValid(tuple))
		tuple = heap_copytuple(tuple);

	heap_endscan(scandesc);
	heap_close(rel, AccessShareLock);

	return tuple;
}

HeapTuple
get_role_profile_tuple_by_oid(Oid rolprfoid)
{
	Relation	 rel;
	HeapScanDesc scandesc;
	HeapTuple	 tuple;
	ScanKeyData	 entry[1];

	CheckProfileCatalogsExist();

	/*
	 * Search pg_yb_role_profile.
	 */
	rel = heap_open(YbRoleProfileRelationId, AccessShareLock);

	ScanKeyInit(&entry[0], ObjectIdAttributeNumber,
				BTEqualStrategyNumber, F_OIDEQ, rolprfoid);
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
 * update_role_profile: Utility fn. to update a row in pg_yb_role_profile
 *
 * roleid: OID of the role
 * rolename: Name of the role. Used in error messages
 * new_record: array with new values
 * new_record_nulls: bool array. element is true if column is updated to null
 * new_record_repl: bool array. element is true if column has to be updated
 * missing_ok: OK if row not found for roleid.
 */
void
update_role_profile(Oid roleid, const char *rolename, Datum *new_record,
					bool *new_record_nulls, bool *new_record_repl,
					bool missing_ok)
{
	Relation	pg_yb_role_profile_rel;
	TupleDesc	pg_yb_role_profile_dsc;
	HeapTuple	tuple, new_tuple;
	Oid 		roleprfid;

	CheckProfileCatalogsExist();

	pg_yb_role_profile_rel = heap_open(YbRoleProfileRelationId, RowExclusiveLock);
	pg_yb_role_profile_dsc = RelationGetDescr(pg_yb_role_profile_rel);

	tuple = get_role_profile_tuple_by_role_oid(roleid);

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
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("role \"%s\" is not associated with a profile",
						rolename)));
	else
		roleprfid = InvalidOid;

	/*
	 * Close pg_yb_role_login, but keep lock till commit.
	 */
	heap_close(pg_yb_role_profile_rel, NoLock);

	return;
}

/*
 * YbCreateRoleProfile - create a new mapping between role and profile.
 * roleid: OID of the role
 * rolename: Name of the role. Required for error messages
 * prfname: Name of the profile.
 */
void
YbCreateRoleProfile(Oid roleid, const char *rolename, const char *prfname)
{
	HeapTuple tuple;
	Form_pg_yb_role_profile rolprfform;
	Oid		oldprfid;

	/* Must be super user or yb_db_admin role */
	if (!superuser() && !IsYbDbAdminUser(GetUserId()))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied to attach role \"%s\" to profile \"%s\"",
						rolename, prfname),
				 errhint("Must be superuser or a member of the yb_db_admin "
				 		 "role to attach a profile.")));

	/*
	 * Check that there is a profile by this name.
	 */
	Oid prfid = get_profile_oid(prfname, true);
	if (!OidIsValid(prfid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("profile \"%s\" does not exist", prfname)));

	tuple = get_role_profile_tuple_by_role_oid(roleid);

	// If there is no entry for the role, then create a map and return.
	if (!HeapTupleIsValid(tuple))
	{
		create_role_profile_map(roleid, prfid);
		return;
	}

	rolprfform = (Form_pg_yb_role_profile) GETSTRUCT(tuple);
	oldprfid = rolprfform->rolprfprofile;

	// Check if the role is already mapped to the same profile.
	if (oldprfid == prfid)
	{
		elog(WARNING, "role \"%s\" is already associated with profile \"%s\"",
			 rolename, prfname);
		return;
	}

	// There is an entry for the role and it has be updated to point to
	// another profile.
	Datum		new_record[Natts_pg_yb_role_profile];
	bool		new_record_nulls[Natts_pg_yb_role_profile];
	bool		new_record_repl[Natts_pg_yb_role_profile];

	MemSet(new_record, 0, sizeof(new_record));
	MemSet(new_record_nulls, false, sizeof(new_record_nulls));
	MemSet(new_record_repl, false, sizeof(new_record_repl));

	new_record[Anum_pg_yb_role_profile_rolprfprofile - 1] = prfid;
	new_record_repl[Anum_pg_yb_role_profile_rolprfprofile - 1] = true;

	update_role_profile(roleid, rolename, new_record,
						new_record_nulls,
						new_record_repl, false);

	/* Record dependency on profile */
	changeDependencyOnProfile(AuthIdRelationId, roleid, prfid);
}

/*
 * YbSetRoleProfileStatus - set rolprfstatus to given status
 *
 * roleid - the oid of the role
 * rolename - Name of the role. Used in the error message
 * status - status to set to
 */
void
YbSetRoleProfileStatus(Oid roleid, const char *rolename, char status)
{
	Datum		new_record[Natts_pg_yb_role_profile];
	bool		new_record_nulls[Natts_pg_yb_role_profile];
	bool		new_record_repl[Natts_pg_yb_role_profile];

	/*
	 * Build an updated tuple with is_enabled set to the new value
	 */
	MemSet(new_record, 0, sizeof(new_record));
	MemSet(new_record_nulls, false, sizeof(new_record_nulls));
	MemSet(new_record_repl, false, sizeof(new_record_repl));

	new_record[Anum_pg_yb_role_profile_rolprfstatus - 1] = status;
	new_record_repl[Anum_pg_yb_role_profile_rolprfstatus - 1] = true;
	new_record[Anum_pg_yb_role_profile_rolprffailedloginattempts - 1] = Int16GetDatum(0);
	new_record_repl[Anum_pg_yb_role_profile_rolprffailedloginattempts - 1] = true;

	update_role_profile(roleid, rolename, new_record, new_record_nulls,
			new_record_repl, false);
	return;
}

/*
 * YbResetFailedAttemptsIfAllowed - reset failed_attempts counter
 * This function does not check that the table exists. Since it is called
 * before the database is initialized, it expects its caller to verify that
 * the profile tables exist.
 *
 * roleid - the oid of the role
 */
void
YbResetFailedAttemptsIfAllowed(Oid roleid)
{
	HeapTuple rolprftuple;
	Form_pg_yb_role_profile rolprfform;

	rolprftuple = get_role_profile_tuple_by_role_oid(roleid);

	if (!HeapTupleIsValid(rolprftuple))
		// Role is not associated with a profile.
		return;

	rolprfform = (Form_pg_yb_role_profile) GETSTRUCT(rolprftuple);

	if (rolprfform->rolprfstatus == ROLPRFSTATUS_OPEN && rolprfform->rolprffailedloginattempts > 0)
		YBCExecuteUpdateLoginAttempts(roleid, 0, ROLPRFSTATUS_OPEN);
}

/*
 * YbMaybeIncFailedAttemptsAndDisableProfile - increment failed_attempts
 * counter and disable if it exceeds limit. The counter will not exceed the
 * limit + 1.
 *
 * roleid - the oid of the role
 *
 * Returns true if the role ends in a locked state.
 * Returns false if the profile does not exist or does exist and is not locked.
 */
bool
YbMaybeIncFailedAttemptsAndDisableProfile(Oid roleid)
{
	HeapTuple 				rolprftuple;
	Form_pg_yb_role_profile rolprfform;
	Form_pg_yb_profile 		prfform;
	HeapTuple 				prftuple;
	int 					failed_attempts_limit;
	int						current_failed_attempts;
	int 					new_failed_attempts;
	char 					rolprfstatus;

	rolprftuple = get_role_profile_tuple_by_role_oid(roleid);

	if (!HeapTupleIsValid(rolprftuple))
		// Role is not associated with a profile.
		return false;

	rolprfform = (Form_pg_yb_role_profile) GETSTRUCT(rolprftuple);

	prftuple = get_profile_tuple(DatumGetObjectId(rolprfform->rolprfprofile));
	if (!HeapTupleIsValid(prftuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("profile \"%d\" not found!", roleid)));

	prfform = (Form_pg_yb_profile) GETSTRUCT(prftuple);

	current_failed_attempts = DatumGetInt16(rolprfform->rolprffailedloginattempts);
	failed_attempts_limit = DatumGetInt16(prfform->prfmaxfailedloginattempts);

	new_failed_attempts = current_failed_attempts < failed_attempts_limit
						? current_failed_attempts + 1
						: failed_attempts_limit + 1;

	/* Keep role enabled IFF role is enabled AND failed attempts < limit */
	rolprfstatus = rolprfform->rolprfstatus == ROLPRFSTATUS_OPEN &&
						(new_failed_attempts <= failed_attempts_limit) ?
						ROLPRFSTATUS_OPEN :
						ROLPRFSTATUS_LOCKED;

	/* Do not write unless the values have changed */
	if (rolprfstatus != rolprfform->rolprfstatus
		|| new_failed_attempts != current_failed_attempts)
		YBCExecuteUpdateLoginAttempts(roleid, new_failed_attempts, rolprfstatus);

	return rolprfstatus != ROLPRFSTATUS_OPEN;
}

void
YbRemoveRoleProfileForRole(Oid roleid)
{
	Relation	 rel;
	HeapScanDesc scandesc;
	ScanKeyData	 skey[1];
	HeapTuple	 tuple;

	CheckProfileCatalogsExist();

	rel = heap_open(YbRoleProfileRelationId, RowExclusiveLock);
	HeapTuple rolprftuple = get_role_profile_tuple_by_role_oid(roleid);

	/* We assume that there can be at most one matching tuple */
	if (!HeapTupleIsValid(rolprftuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("role \"%d\" is not associated with a profile",
						roleid)));
	Oid roleprfid = HeapTupleGetOid(rolprftuple);

	/*
	 * Find the profile to delete.
	 */
	ScanKeyInit(&skey[0], ObjectIdAttributeNumber, BTEqualStrategyNumber,
				F_OIDEQ, ObjectIdGetDatum(roleprfid));
	scandesc = heap_beginscan_catalog(rel, 1, skey);
	tuple = heap_getnext(scandesc, ForwardScanDirection);

	/* If the profile exists, then remove it, otherwise raise an error. */
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("role profile %d does not exist", roleprfid)));

	/*
	 * Remove the pg_yb_role_profile tuple
	 */
	CatalogTupleDelete(rel, tuple);

	heap_endscan(scandesc);
	heap_close(rel, NoLock);

	/*
	 * TODO(profile): check that this deletes only role->profile pg_shdepend
	 * record.  Assumption right now is that that is the only record that
	 * exists where objid=role.  To be safe, it is probably best to make (or
	 * find) a function to delete the specific role->profile dependency record.
	 */
	deleteSharedDependencyRecordsFor(AuthIdRelationId, roleid, 0);
}
