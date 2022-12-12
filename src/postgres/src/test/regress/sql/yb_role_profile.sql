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
CREATE PROFILE test_profile FAILED ATTEMPTS 3;
CREATE USER restricted_user;

-- Can connect when no profiles are setup
\c yugabyte restricted_user
\c yugabyte yugabyte

SELECT prfname, prffailedloginattempts FROM pg_catalog.pg_yb_profile ORDER BY OID;
SELECT rolname from pg_catalog.pg_roles WHERE rolname = 'restricted_user';
ALTER USER restricted_user PROFILE ATTACH test_profile;

-- Can connect when attached to a profile with default values
\c yugabyte restricted_user
\c yugabyte yugabyte

SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

ALTER USER restricted_user PROFILE DISABLE;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

ALTER USER restricted_user PROFILE ENABLE;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

-- Can connect when attached to a profile and profile is enabled
\c yugabyte restricted_user
\c yugabyte yugabyte

-- fail: Cannot attach a role that has already been attached
ALTER USER restricted_user PROFILE ATTACH test_profile;

-- fail: Cannot drop a profile or role that has been attached
DROP PROFILE test_profile;
DROP USER restricted_user;

-- Detach a role
ALTER USER restricted_user PROFILE DETACH;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

-- fail: cannot enable/disable a role that is not attached
ALTER USER restricted_user PROFILE ENABLE;
ALTER USER restricted_user PROFILE DISABLE;

-- fail: Cannot attach to a non-existent profile
ALTER USER restricted_user PROFILE ATTACH non_existent;
DROP USER restricted_user;
DROP PROFILE test_profile;
