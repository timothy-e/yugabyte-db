BEGIN;
  CREATE TABLE IF NOT EXISTS pg_catalog.pg_yb_profile (
    prfname NAME NOT NULL,
    prffailedloginattempts INTEGER NOT NULL,
    prfpasswordlocktime INTEGER NOT NULL,
    CONSTRAINT pg_yb_profile_oid_index PRIMARY KEY(oid ASC)
        WITH (table_oid = 8052)
  ) WITH (
    oids = true,
    table_oid = 8051,
    row_type_oid = 8053
  ) TABLESPACE pg_global;

  CREATE TABLE IF NOT EXISTS pg_catalog.pg_yb_role_profile (
    rolid OID NOT NULL,
    prfid OID NOT NULL,
    rolisenabled BOOL NOT NULL,
    rolfailedloginattempts INTEGER NOT NULL,
    rollockedat INTEGER NOT NULL,
    CONSTRAINT pg_yb_role_profile_oid_index PRIMARY KEY(oid ASC)
        WITH (table_oid = 8055)
  ) WITH (
    oids = true,
    table_oid = 8054,
    row_type_oid = 8056
  ) TABLESPACE pg_global;
COMMIT;

BEGIN;
  SET LOCAL yb_non_ddl_txn_for_sys_tables_allowed TO true;

  INSERT INTO pg_yb_profile (oid, prfname, prffailedloginattempts, prfpasswordlocktime)
  VALUES (8057, 'default', 0, 0)
  ON CONFLICT DO NOTHING;
COMMIT;
