CREATE USER user_1;
CREATE USER user_2 SUPERUSER;
CREATE USER restricted_user;

CREATE PROFILE existing_profile FAILED ATTEMPTS 3;

\c yugabyte user_1

-- None of these commands should be allowed to a normal user
CREATE PROFILE test_profile FAILED ATTEMPTS 3;
ALTER USER restricted_user PROFILE ATTACH test_profile;
ALTER USER restricted_user PROFILE DISABLE;
ALTER USER restricted_user PROFILE ENABLE;
ALTER USER restricted_user PROFILE DETACH;
DROP PROFILE test_profile;
DROP PROFILE existing_profile;

\c yugabyte user_2
CREATE PROFILE test_profile FAILED ATTEMPTS 3;
ALTER USER restricted_user PROFILE ATTACH test_profile;
ALTER USER restricted_user PROFILE DISABLE;
ALTER USER restricted_user PROFILE ENABLE;
ALTER USER restricted_user PROFILE DETACH;
DROP PROFILE test_profile;
DROP PROFILE existing_profile;
