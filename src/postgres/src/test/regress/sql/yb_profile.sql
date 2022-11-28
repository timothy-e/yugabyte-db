--
-- YB_LOGIN_PROFILE Testsuite: Testing Statments for LOGIN PROFILES.
--
\h CREATE PROFILE
\h DROP PROFILE

--
-- pg_catalog alterations. Validate columns of pg_yb_profile and oids.
--
\d pg_yb_profile
SELECT oid, relname, reltype, relnatts FROM pg_class WHERE relname IN ('pg_yb_profile', 'pg_yb_profile_oid_index');
SELECT oid, typname, typrelid FROM pg_type WHERE typname LIKE 'pg_yb_profile';
SELECT pg_describe_object('pg_yb_profile'::regclass::oid, oid, 0) FROM pg_yb_profile;

--
-- CREATE PROFILE
--
SELECT oid, prfname, prffailedloginattempts FROM pg_catalog.pg_yb_profile ORDER BY oid;

CREATE PROFILE test_profile FAILED ATTEMPTS 3;

SELECT prfname, prffailedloginattempts FROM pg_catalog.pg_yb_profile ORDER BY OID;

--Fail because it is a duplicate name

CREATE PROFILE test_profile FAILED ATTEMPTS 4;

--
-- DROP PROFILE
--

DROP PROFILE test_profile;

-- fail: does not exist
DROP PROFILE test_profile;

-- fail: cannot delete default profile
DROP PROFILE "default";
