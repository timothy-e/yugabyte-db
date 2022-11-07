// profile.h
//	  Commands to manipulate login profiles
// src/include/commands/profile.h
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

#ifndef PROFILE_H
#define PROFILE_H

#include "catalog/objectaddress.h"
#include "lib/stringinfo.h"
#include "nodes/parsenodes.h"

extern Oid CreateProfile(CreateProfileStmt *stmt);

extern Oid	 get_profile_oid(const char *prfname, bool missing_ok);
extern char *get_profile_name(Oid prf_oid);

extern void RemoveProfileById(Oid prf_oid);

extern Oid CreateRoleProfile(Oid rolid, const char* profile);
extern Oid EnableRoleProfile(Oid roleid, bool isEnabled);

extern Oid	 get_role_profile_oid(Oid rolid, bool missing_ok);
extern HeapTuple get_role_profile_tuple(Oid rolid);

extern void RemoveRoleProfileById(Oid rolprfoid);
#endif /* PROFILE_H */
