// Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//

package org.yb.pgsql;

import static org.yb.AssertionWrappers.assertEquals;
import static org.yb.AssertionWrappers.assertFalse;

import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.Statement;
import java.util.Map;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.yb.YBTestRunner;

import com.yugabyte.util.PSQLException;
@RunWith(value=YBTestRunner.class)
public class TestYbRoleProfile extends BasePgSQLTest {

  private static final Logger LOG = LoggerFactory.getLogger(TestPgSequences.class);
  private static final String USERNAME = "profile_user";
  private static final String PASSWORD = "profile_password";
  private static final String PROFILE_1_NAME = "prf1";
  private static final String PROFILE_2_NAME = "prf2";
  private static final int PRF_1_FAILED_ATTEMPTS = 3;
  private static final int PRF_2_FAILED_ATTEMPTS = 2;

  @Override
  protected Map<String, String> getTServerFlags() {
    Map<String, String> flagMap = super.getTServerFlags();
    flagMap.put("ysql_enable_auth", "true");
    flagMap.put("ysql_enable_profile", "true");
    return flagMap;
  }

  private void attemptLogin(String username, String password) throws Exception {
    try {
      getConnectionBuilder()
        .withTServer(0)
        .withUser(username)
        .withPassword(password)
        .connect();
    } catch (PSQLException e) {
      assertEquals(String.format("FATAL: password authentication failed for user \"%s\"",
          username), e.getMessage());
    }
  }

  private void login(String username, String password) throws Exception {
    Connection connection = getConnectionBuilder()
      .withTServer(0)
      .withUser(username)
      .withPassword(password)
      .connect();

    connection.close();
  }

  private void assertProfileStateForUser(String username,
      int expectedFailedLogins,
      boolean expectedEnabled) throws Exception {
    try (Statement stmt = connection.createStatement()) {
      ResultSet result = stmt.executeQuery(
          String.format("SELECT rolisenabled, rolfailedloginattempts " +
              "FROM pg_yb_role_profile rp " +
              "JOIN pg_roles rol ON rp.rolid = rol.oid " +
              "WHERE rol.rolname = '%s'",
              username));
      while (result.next()) {
        assertEquals(expectedFailedLogins, Integer.parseInt(
            result.getString("rolfailedloginattempts")));
        assertEquals(expectedEnabled,
            result.getString("rolisenabled").equals("t"));
        assertFalse(result.next());
      }
    }
  }

  private String getProfileName(String username) throws Exception {
    try (Statement stmt = connection.createStatement()) {
      ResultSet result = stmt.executeQuery(String.format(
          "SELECT prfname " +
              "FROM pg_yb_role_profile rp " +
              "JOIN pg_roles rol ON rp.rolid = rol.oid " +
              "JOIN pg_yb_profile lp ON rp.prfid = lp.oid " +
              "WHERE rol.rolname = '%s'",
          username));
      while (result.next()) {
        return result.getString("prfname");
      }
    }
    return null;
  }

  private void enableUserProfile(String username) throws Exception {
    try (Statement stmt = connection.createStatement()) {
      stmt.execute(String.format("ALTER USER %s PROFILE ENABLE", username));
    }
  }

  private void disableUserProfile(String username) throws Exception {
    try (Statement stmt = connection.createStatement()) {
      stmt.execute(String.format("ALTER USER %s PROFILE DISABLE", username));
    }
  }

  private void detachUserProfile(String username) throws Exception {
    try (Statement stmt = connection.createStatement()) {
      stmt.execute(String.format("ALTER USER %s PROFILE DETACH", username));
    }
  }

  private void attachUserProfile(String username, String profilename) throws Exception {
    try (Statement stmt = connection.createStatement()) {
      stmt.execute(String.format("ALTER USER %s PROFILE ATTACH %s", username, profilename));
    }
  }

  @After
  public void cleanup() throws Exception {
    try (Statement stmt = connection.createStatement()) {
      stmt.execute(String.format("ALTER USER %s PROFILE DETACH", USERNAME));
      stmt.execute(String.format("DROP PROFILE %s", PROFILE_1_NAME));
      stmt.execute(String.format("DROP PROFILE %s", PROFILE_2_NAME));
      stmt.execute(String.format("DROP USER %s", USERNAME));
    }
  }

  @Before
  public void setup() throws Exception {
    try (Statement stmt = connection.createStatement()) {
      stmt.execute(String.format("CREATE USER %s PASSWORD '%s'", USERNAME, PASSWORD));
      stmt.execute(String.format("CREATE PROFILE %s LIMIT FAILED_LOGIN_ATTEMPTS %d",
                                  PROFILE_1_NAME, PRF_1_FAILED_ATTEMPTS));
      stmt.execute(String.format("CREATE PROFILE %s LIMIT FAILED_LOGIN_ATTEMPTS %d",
                                  PROFILE_2_NAME, PRF_2_FAILED_ATTEMPTS));
      stmt.execute(String.format("ALTER USER %s PROFILE ATTACH %s",
                                  USERNAME, PROFILE_1_NAME));
    }
  }

  @Test
  public void testAdminCanChangeUserProfile() throws Exception {
    assertEquals(PROFILE_1_NAME, getProfileName(USERNAME));

    /* Now exceed the failed attempts limit */
    for (int i = 0; i < PRF_1_FAILED_ATTEMPTS + 1; i++) {
      assertProfileStateForUser(USERNAME, i, true);
      attemptLogin(USERNAME, "wrong");
    }
    assertProfileStateForUser(USERNAME, PRF_1_FAILED_ATTEMPTS + 1, false);

    /* When the profile is removed, the user can log in again */
    detachUserProfile(USERNAME);
    assertEquals(null, getProfileName(USERNAME));
    login(USERNAME, PASSWORD);

    /* Then, if we change the profile, the user has only that many failed attempts */
    attachUserProfile(USERNAME, PROFILE_2_NAME);
    assertEquals(PROFILE_2_NAME, getProfileName(USERNAME));

    /* Now exceed the failed attempts limit */
    for (int i = 0; i < PRF_2_FAILED_ATTEMPTS + 1; i++) {
      assertProfileStateForUser(USERNAME, i, true);
      attemptLogin(USERNAME, "wrong");
    }
    assertProfileStateForUser(USERNAME, PRF_2_FAILED_ATTEMPTS + 1, false);
  }

  @Test
  public void testRegularUserCanFailLoginManyTimes() throws Exception {
    for (int i = 0; i < 10; i++) {
      attemptLogin(TEST_PG_USER, "wrong");
    }
    attemptLogin(TEST_PG_USER, TEST_PG_PASS);
  }

  @Test
  public void testAdminCanEnable() throws Exception {
    /* Exceed the failed attempts limit */
    for (int i = 0; i < PRF_1_FAILED_ATTEMPTS + 1; i++) {
      attemptLogin(USERNAME, "wrong");
    }
    assertProfileStateForUser(USERNAME, PRF_1_FAILED_ATTEMPTS + 1, false);

    /* Now the user cannot login */
    attemptLogin(USERNAME, PASSWORD);

    /* After an admin resets, the user can login again */
    enableUserProfile(USERNAME);
    assertProfileStateForUser(USERNAME, 0, true);
    login(USERNAME, PASSWORD);
  }

  @Test
  public void testAdminCanDisableAndEnable() throws Exception {
    disableUserProfile(USERNAME);

    /* With a disabled profile, the user cannot login */
    assertProfileStateForUser(USERNAME, 0, false);
    attemptLogin(USERNAME, PASSWORD);

    /* After an admin enables, the user can login again */
    enableUserProfile(USERNAME);
    assertProfileStateForUser(USERNAME, 0, true);
    login(USERNAME, PASSWORD);
  }

  @Test
  public void testLogin() throws Exception {
    /* The initial state allows logins */
    assertProfileStateForUser(USERNAME, 0, true);
    login(USERNAME, PASSWORD);

    /* Use up all allowed failed attempts */
    for (int i = 0; i < PRF_1_FAILED_ATTEMPTS; i++) {
      assertProfileStateForUser(USERNAME, i, true);
      attemptLogin(USERNAME, "wrong");
    }
    assertProfileStateForUser(USERNAME, PRF_1_FAILED_ATTEMPTS, true);

    /* A successful login wipes the slate clean */
    login(USERNAME, PASSWORD);
    assertProfileStateForUser(USERNAME, 0, true);

    /* Now exceed the failed attempts limit */
    for (int i = 0; i < PRF_1_FAILED_ATTEMPTS + 1; i++) {
      assertProfileStateForUser(USERNAME, i, true);
      attemptLogin(USERNAME, "wrong");
    }
    assertProfileStateForUser(USERNAME, PRF_1_FAILED_ATTEMPTS + 1, false);

    /* Now even the correct password will not let us in */
    attemptLogin(USERNAME, PASSWORD);
    attemptLogin(USERNAME, "wrong");
    assertProfileStateForUser(USERNAME, PRF_1_FAILED_ATTEMPTS + 3, false);
  }
}
