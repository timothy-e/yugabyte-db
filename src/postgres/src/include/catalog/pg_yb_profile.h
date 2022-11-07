/*-------------------------------------------------------------------------
 *
 * pg_yb_profile.h
 *	  definition of the "profile" system catalog (pg_yb_profile)
 *
 *
 * Copyright (c) YugaByte, Inc.
 *
 * src/include/catalog/pg_yb_profile.h
 *
 * NOTES
 *	  The Catalog.pm module reads this file and derives schema
 *	  information.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_YB_LOGIN_PROFILE_H
#define PG_YB_LOGIN_PROFILE_H

#include "catalog/genbki.h"
#include "catalog/pg_yb_profile_d.h"

/* ----------------
 *		pg_yb_profile definition.  cpp turns this into
 *		typedef struct FormData_pg_yb_profile
 * ----------------
 */
CATALOG(pg_yb_profile,8049,YbProfileRelationId) BKI_ROWTYPE_OID(8051,YbProfileRelation_Rowtype_Id) BKI_SCHEMA_MACRO
{
	NameData	prfname;		/* profile name */
	int16		prffailedloginattempts;		/* no. of attempts allowed */
	int64		prfpasswordlocktime;  /* secs to lock out an account */
} FormData_pg_yb_profile;

/* ----------------
 *		Form_pg_yb_profile corresponds to a pointer to a tuple with
 *		the format of pg_yb_profile relation.
 * ----------------
 */
typedef FormData_pg_yb_profile *Form_pg_yb_profile;

#endif							/* PG_YB_LOGIN_PROFILE_H */
