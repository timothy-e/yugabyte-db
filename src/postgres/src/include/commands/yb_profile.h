/*--------------------------------------------------------------------------------------------------
 *
 * yb_profile.h
 *	  prototypes for yb_profile.c
 *
 * Copyright (c) YugaByte, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied.  See the License for the specific language governing permissions and limitations
 * under the License.
 *
 * src/include/commands/yb_profile.h
 *
 *--------------------------------------------------------------------------------------------------
 */

#ifndef PROFILE_H
#define PROFILE_H

#include "catalog/objectaddress.h"
#include "lib/stringinfo.h"
#include "nodes/parsenodes.h"

extern Oid CreateProfile(CreateProfileStmt* stmt);

extern Oid	 get_profile_oid(const char* prfname, bool missing_ok);
extern char* get_profile_name(Oid prfid);

extern void RemoveProfileById(Oid prfid);

extern void CreateRoleProfile(Oid roleid, const char* rolename,
		const char* profile);
extern void EnableRoleProfile(Oid roleid, const char* rolename, bool is_enabled);
extern bool YBCIncFailedAttemptsAndMaybeDisableProfile(Oid roleid);
extern void YBCResetFailedAttemptsIfAllowed(Oid roleid);

extern Oid	 get_role_profile_oid(Oid roleid, const char* rolname,
		bool missing_ok);
extern Oid	 get_role_oid_from_role_profile(Oid roleprfid);
extern HeapTuple get_role_profile_tuple_by_role_oid(Oid roleid);
extern HeapTuple get_role_profile_tuple_by_oid(Oid rolprfid);

extern void RemoveRoleProfileForRole(Oid roleid, const char* rolename);
extern void RemoveRoleProfileById(Oid roleprfid);
#endif /* PROFILE_H */
