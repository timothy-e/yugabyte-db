--
-- YB_ROLE_LOGIN_PROFILE Testsuite: Testing Statments for ALTER USER ...PROFILE.
--

--
-- pg_catalog alterations. Validate columns of pg_yb_role_profile and oids.
--
\d pg_yb_role_profile
SELECT oid, relname, reltype, relnatts FROM pg_class WHERE relname IN ('pg_yb_role_profile', 'pg_yb_role_profile_oid_index');
SELECT oid, typname, typrelid FROM pg_type WHERE typname LIKE 'pg_yb_role_profile';

--
-- CREATE PROFILE
--
CREATE PROFILE test_profile LIMIT FAILED_LOGIN_ATTEMPTS 3;
CREATE USER restricted_user;

-- fail: cannot lock/unlock a role that is not attached
ALTER USER restricted_user ACCOUNT UNLOCK;
ALTER USER restricted_user ACCOUNT LOCK;

-- Can connect when no profiles are setup
\c yugabyte restricted_user
\c yugabyte yugabyte

-- fail: Cannot attach to a non-existent profile
ALTER USER restricted_user PROFILE non_existent;

-- Attach role to a profile
SELECT prfname, prfmaxfailedloginattempts FROM pg_catalog.pg_yb_profile ORDER BY OID;
SELECT rolname from pg_catalog.pg_roles WHERE rolname = 'restricted_user';
ALTER USER restricted_user PROFILE test_profile;
--
-- Ensure dependencies betwee pg_yb_role_profile & pg_yb_profile
-- and pg_yb_role_profile & pg_roles is setup correctly
-- There should be one row
SELECT count(*) FROM pg_yb_role_profile;
-- One row in pg_shdepend for the role profile
SELECT count(*) FROM pg_shdepend shdep
                    JOIN pg_yb_role_profile rpf on rpf.oid = shdep.objid
                WHERE shdep.deptype = 'f';
-- One row in pg_shdepend for the role
SELECT count(*) FROM pg_shdepend shdep
                    JOIN pg_roles rol on rol.oid = shdep.refobjid
                WHERE shdep.deptype = 'f' and rol.rolname = 'restricted_user';
-- One row in pg_depend for the role profile
SELECT count(*) FROM pg_depend dep
                    JOIN pg_yb_role_profile rpf on rpf.oid = dep.objid;
-- One row in pg_depend for the profile
SELECT count(*) FROM pg_depend dep
                    JOIN pg_yb_profile prf on prf.oid = dep.refobjid
                    WHERE prf.prfname = 'test_profile';

-- Can connect when attached to a profile
\c yugabyte restricted_user
\c yugabyte yugabyte

SELECT rolprfstatus, rolprffailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolprfrole = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.rolprfprofile = lp.oid;

ALTER USER restricted_user ACCOUNT LOCK;
SELECT rolprfstatus, rolprffailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolprfrole = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.rolprfprofile = lp.oid;

ALTER USER restricted_user ACCOUNT UNLOCK;
SELECT rolprfstatus, rolprffailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolprfrole = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.rolprfprofile = lp.oid;

-- Associating a role to the same profile is a no-op
ALTER USER restricted_user PROFILE test_profile;

-- fail: Cannot drop a profile that has a role associated with it
DROP PROFILE test_profile;

-- A role/user cannot be dropped because there is mapping to profile.
DROP USER restricted_user;

-- Remove the association of a role to a profile
ALTER USER restricted_user NOPROFILE;
SELECT rolprfstatus, rolprffailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolprfrole = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.rolprfprofile = lp.oid;
--
-- Ensure dependencies betwee pg_yb_role_profile & pg_yb_profile
-- and pg_yb_role_profile & pg_roles is setup correctly
-- There should be zero rows
SELECT count(*) FROM pg_yb_role_profile;
-- pg_shdepend for the role profile
SELECT count(*) FROM pg_shdepend shdep
                    JOIN pg_yb_role_profile rpf on rpf.oid = shdep.objid
                WHERE shdep.deptype = 'f';
-- pg_shdepend for the role
SELECT count(*) FROM pg_shdepend shdep
                    JOIN pg_roles rol on rol.oid = shdep.refobjid
                WHERE shdep.deptype = 'f' and rol.rolname = 'restricted_user';
-- pg_depend for the role profile
SELECT count(*) FROM pg_depend dep
                    JOIN pg_yb_role_profile rpf on rpf.oid = dep.objid;
-- pg_depend for the profile
SELECT count(*) FROM pg_depend dep
                    JOIN pg_yb_profile prf on prf.oid = dep.refobjid
                    WHERE prf.prfname = 'test_profile';


-- Can drop user as it is not attached to a profile. After dropping the user there should be 0 rows
DROP USER restricted_user;
select count(*) from pg_yb_role_profile;

DROP PROFILE test_profile;
