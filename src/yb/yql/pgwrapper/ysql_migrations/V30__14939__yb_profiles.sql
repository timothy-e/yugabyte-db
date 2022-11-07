BEGIN;
  CREATE TABLE IF NOT EXISTS pg_catalog.pg_yb_profile (
    prfname NAME NOT NULL,
    prffailedloginattempts SMALLINT NOT NULL,
    prfpasswordlocktime INTEGER NOT NULL DEFAULT 0,
    CONSTRAINT pg_yb_profile_oid_index PRIMARY KEY(oid ASC)
        WITH (table_oid = 8052)
  ) WITH (
    oids = true,
    table_oid = 8051,
    row_type_oid = 8053
  );
COMMIT;

BEGIN;
  CREATE TABLE IF NOT EXISTS pg_catalog.pg_yb_role_profile (
    rolid OID NOT NULL,
    prfid OID NOT NULL,
    rolisenabled BOOL NOT NULL,
    rolfailedloginattempts SMALLINT NOT NULL,
    rollockts TIMESTAMPTZ NOT NULL,
    CONSTRAINT pg_yb_role_profile_oid_index PRIMARY KEY(oid ASC)
        WITH (table_oid = 8055)
  ) WITH (
    oids = true,
    table_oid = 8054,
    row_type_oid = 8056
  );
COMMIT;
