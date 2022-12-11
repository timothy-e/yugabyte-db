CREATE PROFILE ind_profile FAILED ATTEMPTS 2;
CREATE USER ind_user;

SELECT prfname, prffailedloginattempts FROM pg_catalog.pg_yb_profile ORDER BY OID;
SELECT rolname from pg_catalog.pg_roles WHERE rolname = 'ind_user';

ALTER USER ind_user PROFILE ATTACH ind_profile;

SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

-- Simulate a failed attempt that increments the counter.
ALTER USER ind_user PROFILE ATTEMPTS FAILED;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

 -- Simulate one more failure. This failure is allowed and does not disable the role.
ALTER USER ind_user PROFILE ATTEMPTS FAILED;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

 -- Simulate one more failure. This attempt disables the role.
ALTER USER ind_user PROFILE ATTEMPTS FAILED;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

-- Enable should reset the counter
ALTER USER ind_user PROFILE ENABLE;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

ALTER USER ind_user PROFILE DETACH;
DROP USER ind_user;
DROP PROFILE ind_profile;
