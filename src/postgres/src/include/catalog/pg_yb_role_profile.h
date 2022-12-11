/*-------------------------------------------------------------------------
 *
 * pg_yb_role_profile.h
 *	  definition of the "role_profile" system catalog (pg_yb_role_profile)
 *
 *
 * Copyright (c) YugaByte, Inc.
 *
 * src/include/catalog/pg_yb_role_profile.h
 *
 * NOTES
 *	  The Catalog.pm module reads this file and derives schema
 *	  information.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_YB_ROLE_PROFILE_H
#define PG_YB_ROLE_PROFILE_H

#include "catalog/genbki.h"
#include "catalog/pg_yb_role_profile_d.h"

/* ----------------
 *		pg_yb_role_profile definition.  cpp turns this into
 *		typedef struct FormData_pg_yb_role_profile
 * ----------------
 */
CATALOG(pg_yb_role_profile,8054,YbRoleProfileRelationId) BKI_SHARED_RELATION BKI_ROWTYPE_OID(8056,YbRoleProfileRelation_Rowtype_Id) BKI_SCHEMA_MACRO
{
	bool		rolisenabled;	/* Is the Role enabled? */
	Oid			rolid;		/* OID of the role */
	Oid			prfid;		/* OID of the profile */
	int32		rolfailedloginattempts;  /* Number of failed attempts */
	int64    	rollockedat;  /* Timestamp at which role was locked */
} FormData_pg_yb_role_profile;

/* ----------------
 *		Form_pg_yb_role_profile corresponds to a pointer to a tuple with
 *		the format of pg_yb_role_profile relation.
 * ----------------
 */
typedef FormData_pg_yb_role_profile *Form_pg_yb_role_profile;

#endif							/* PG_YB_ROLE_PROFILE_H */
