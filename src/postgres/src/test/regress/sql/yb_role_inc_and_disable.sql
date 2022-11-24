CREATE PROFILE ind_profile FAILED ATTEMPTS 2;
CREATE USER ind_user;

SELECT prfname, prffailedloginattempts FROM pg_catalog.pg_yb_profile ORDER BY OID;
SELECT rolname from pg_catalog.pg_roles WHERE rolname = 'ind_user';

ALTER USER ind_user PROFILE ATTACH ind_profile;

SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

-- A failed attempt increments the counter
ALTER USER ind_user PROFILE ATTEMPTS FAILED;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

 -- One more is allowed
ALTER USER ind_user PROFILE ATTEMPTS FAILED;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

 -- Increment and disable
ALTER USER ind_user PROFILE ATTEMPTS FAILED;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

--Enable should reset the counter
ALTER USER ind_user PROFILE ENABLE;
SELECT rolisenabled, rolfailedloginattempts, rolname, prfname FROM
    pg_catalog.pg_yb_role_profile rp JOIN pg_catalog.pg_roles rol ON rp.rolid = rol.oid
    JOIN pg_catalog.pg_yb_profile lp ON rp.prfid = lp.oid;

DROP USER ind_user;
DROP PROFILE ind_profile;