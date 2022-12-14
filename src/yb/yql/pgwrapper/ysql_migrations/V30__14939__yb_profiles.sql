BEGIN;
  CREATE TABLE IF NOT EXISTS pg_catalog.pg_yb_profile (
    prfname NAME NOT NULL,
    prffailedloginattempts INTEGER NOT NULL,
    prfpasswordlocktime INTEGER NOT NULL,
    CONSTRAINT pg_yb_profile_oid_index PRIMARY KEY(oid ASC)
        WITH (table_oid = 8052),
    CONSTRAINT pg_yb_profile_prfname_index PRIMARY KEY (prfname ASC)
        WITH (table_oid = 8057)
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

  -- CREATE INDEX IF NOT EXISTS pg_catalog.pg_yb_profile_prfname_index
  -- ON pg_yb_profile (prfname)

COMMIT;
