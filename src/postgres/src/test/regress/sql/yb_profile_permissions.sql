CREATE USER user_1;
CREATE USER user_2 SUPERUSER;
CREATE USER user_3;
CREATE USER restricted_user;

GRANT yb_db_admin TO user_3 WITH ADMIN OPTION;

CREATE PROFILE existing_profile LIMIT FAILED_LOGIN_ATTEMPTS 3;

\c yugabyte user_1

-- None of these commands should be allowed to a normal user
CREATE PROFILE test_profile_1 LIMIT FAILED_LOGIN_ATTEMPTS 3;
ALTER USER restricted_user PROFILE ATTACH test_profile_1;
ALTER USER restricted_user ACCOUNT LOCK;
ALTER USER restricted_user ACCOUNT UNLOCK;
ALTER USER restricted_user PROFILE DETACH;
DROP PROFILE existing_profile;

-- user_2 can execute these commands as it is a super user.
\c yugabyte user_2
CREATE PROFILE test_profile_2 LIMIT FAILED_LOGIN_ATTEMPTS 3;
ALTER USER restricted_user PROFILE ATTACH test_profile_2;
ALTER USER restricted_user ACCOUNT LOCK;
ALTER USER restricted_user ACCOUNT UNLOCK;
ALTER USER restricted_user PROFILE DETACH;
DROP PROFILE test_profile_2;
DROP PROFILE existing_profile;

-- Recreate profile for next test
\c yugabyte yugabyte
CREATE PROFILE existing_profile LIMIT FAILED_LOGIN_ATTEMPTS 3;

-- user_3 can execute these commands as it is a yb_superuser.

\c yugabyte user_3
CREATE PROFILE test_profile_3 LIMIT FAILED_LOGIN_ATTEMPTS 3;
ALTER USER restricted_user PROFILE ATTACH test_profile_3;
ALTER USER restricted_user ACCOUNT LOCK;
ALTER USER restricted_user ACCOUNT UNLOCK;
ALTER USER restricted_user PROFILE DETACH;
DROP PROFILE test_profile_3;
DROP PROFILE existing_profile;

\c yugabyte yugabyte
DROP USER user_1;
DROP USER user_2;
DROP USER user_3;
