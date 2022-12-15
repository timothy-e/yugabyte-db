--
-- YB_ROLE_PROFILE Testsuite: Testing statements for connecting roles and profiles.
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
CREATE PROFILE test_profile_2 LIMIT FAILED_LOGIN_ATTEMPTS 3;
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
-- Ensure dependencies between role & pg_yb_profile is setup correctly
-- There should be one row
SELECT count(*) FROM pg_yb_role_profile;
-- One row in pg_depend for the profile -> role
SELECT count(*) FROM pg_depend dep
                JOIN pg_yb_profile prf on prf.oid = dep.refobjid
                JOIN pg_roles rol ON rol.oid = dep.objid
                WHERE dep.deptype = 'f';

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

-- Associating a role to a different profile succeeds
ALTER USER restricted_user PROFILE test_profile_2;

-- We can drop a profile that does not have any users associated
DROP PROFILE test_profile;

-- fail: Cannot drop a profile that has a role associated with it
DROP PROFILE test_profile_2;

-- Removing the user's profile should clean up the mappings and dependencies.
ALTER USER restricted_user NOPROFILE;
SELECT count(*) FROM pg_yb_role_profile;
SELECT count(*) FROM pg_depend dep
                JOIN pg_yb_profile prf on prf.oid = dep.refobjid
                JOIN pg_roles rol ON rol.oid = dep.objid
                WHERE dep.deptype = 'f';

-- Setting a new profile creates a new row in pg_yb_role_profile
ALTER USER restricted_user PROFILE test_profile_2;
SELECT count(*) FROM pg_yb_role_profile;

-- Dropping the user should clean up the mappings and dependencies.
DROP USER restricted_user;
SELECT count(*) FROM pg_yb_role_profile;
SELECT count(*) FROM pg_depend dep
                JOIN pg_yb_profile prf on prf.oid = dep.refobjid
                JOIN pg_roles rol ON rol.oid = dep.objid
                WHERE dep.deptype = 'f';

DROP PROFILE test_profile_2;
