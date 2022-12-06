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

import static org.junit.Assert.assertEquals;
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
  private static final String PROFILE_USERNAME = "profile_user";
  private static final String PROFILE_USER_PASS = "profile_password";
  private static final String PROFILE_NAME = "prf";
  private static final String AUTHENTICATION_FAILED_MESSAGE = String.format("FATAL: password authentication failed for user \"%s\"", PROFILE_USERNAME);;
  private static final String CANNOT_LOGIN_MESSAGE = String.format("FATAL: role \"%s\" is not permitted to log in", PROFILE_USERNAME);
  private static final int FAILED_ATTEMPTS = 3;

  @Override
  protected Map<String, String> getTServerFlags() {
    Map<String, String> flagMap = super.getTServerFlags();
    flagMap.put("ysql_enable_auth", "true");
    return flagMap;
  }

  private void attemptLogin(String username, String password, String expectedError) throws Exception {
    try {
      getConnectionBuilder()
        .withTServer(0)
        .withUser(username)
        .withPassword(password)
        .connect();
    } catch (PSQLException e) {
      if (expectedError == null)
        throw e;

      assertEquals(expectedError, e.getMessage());
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

  private void assertProfileStateForUser(String username, int expectedFailedLogins, boolean expectedEnabled) throws Exception {
    try (Statement stmt = connection.createStatement()) {
      ResultSet result = stmt.executeQuery(String.format("SELECT rolisenabled, rolfailedloginattempts " +
                                                         "FROM pg_yb_role_profile rp " +
                                                         "JOIN pg_roles rol ON rp.rolid = rol.oid " +
                                                         "WHERE rol.rolname = '%s'",
                                                         username));
      while (result.next()) {
        assertEquals(expectedFailedLogins, Integer.parseInt(result.getString("rolfailedloginattempts")));
        assertEquals(expectedEnabled, result.getString("rolisenabled").equals("t"));
        assertFalse(result.next());
      }
    }
  }

  private void unlockUser(String username) throws Exception {
    try (Statement stmt = connection.createStatement()) {
      stmt.execute(String.format("ALTER USER %s PROFILE ENABLE", PROFILE_USERNAME));
    }
  }

  private void lockUser(String username) throws Exception {
    try (Statement stmt = connection.createStatement()) {
      stmt.execute(String.format("ALTER USER %s PROFILE DISABLE", PROFILE_USERNAME));
    }
  }

  @After
  public void cleanup() throws Exception {
    try (Statement stmt = connection.createStatement()) {
      stmt.execute(String.format("DROP USER %s", PROFILE_USERNAME));
      stmt.execute(String.format("DROP PROFILE %s", PROFILE_NAME));
    }
  }

  @Before
  public void setup() throws Exception {
    try (Statement stmt = connection.createStatement()) {
      stmt.execute(String.format("CREATE USER %s PASSWORD '%s'", PROFILE_USERNAME, PROFILE_USER_PASS));
      stmt.execute(String.format("CREATE PROFILE %s FAILED ATTEMPTS %d", PROFILE_NAME, FAILED_ATTEMPTS));
      stmt.execute(String.format("ALTER USER %s PROFILE ATTACH %s", PROFILE_USERNAME, PROFILE_NAME));
    }
  }

  @Test
  public void testAdminCanUnlock() throws Exception {
    /* Exceed the failed attempts limit */
    for (int i = 0; i < FAILED_ATTEMPTS + 1; i++) {
      attemptLogin(PROFILE_USERNAME, "wrong", AUTHENTICATION_FAILED_MESSAGE);
    }
    assertProfileStateForUser(PROFILE_USERNAME, FAILED_ATTEMPTS + 1, false);

    /* Now the user cannot login */
    attemptLogin(PROFILE_USERNAME, PROFILE_USER_PASS, CANNOT_LOGIN_MESSAGE);

    /* After an admin resets, the user can login again */
    unlockUser(PROFILE_USERNAME);
    assertProfileStateForUser(PROFILE_USERNAME, 0, true);
    login(PROFILE_USERNAME, PROFILE_USER_PASS);
  }

  @Test
  public void testAdminCanLockAndUnlock() throws Exception {
    /* Exceed the failed attempts limit */
    lockUser(PROFILE_USERNAME);
    assertProfileStateForUser(PROFILE_USERNAME, 0, false);

    /* Now the user cannot login */
    attemptLogin(PROFILE_USERNAME, PROFILE_USER_PASS, CANNOT_LOGIN_MESSAGE);

    /* After an admin resets, the user can login again */
    unlockUser(PROFILE_USERNAME);
    assertProfileStateForUser(PROFILE_USERNAME, 0, true);
    login(PROFILE_USERNAME, PROFILE_USER_PASS);
  }

  @Test
  public void testLogin() throws Exception {
    assertProfileStateForUser(PROFILE_USERNAME, 0, true);

    login(PROFILE_USERNAME, PROFILE_USER_PASS);
    assertProfileStateForUser(PROFILE_USERNAME, 0, true);

    /* Use up all allowed failed attempts */
    for (int i = 0; i < FAILED_ATTEMPTS; i++) {
      attemptLogin(PROFILE_USERNAME, "wrong", AUTHENTICATION_FAILED_MESSAGE);
      assertProfileStateForUser(PROFILE_USERNAME, i + 1, true);
    }

    /* A successful login wipes the slate clean */
    login(PROFILE_USERNAME, PROFILE_USER_PASS);
    assertProfileStateForUser(PROFILE_USERNAME, 0, true);

    /* Now exceed the failed attempts limit */
    for (int i = 0; i < FAILED_ATTEMPTS + 1; i++) {
      assertProfileStateForUser(PROFILE_USERNAME, i, true);
      attemptLogin(PROFILE_USERNAME, "wrong", AUTHENTICATION_FAILED_MESSAGE);
    }
    assertProfileStateForUser(PROFILE_USERNAME, FAILED_ATTEMPTS + 1, false);

    /* A correct password gives a different error message and does not increment the failed counts*/
    attemptLogin(PROFILE_USERNAME, PROFILE_USER_PASS, CANNOT_LOGIN_MESSAGE);
    assertProfileStateForUser(PROFILE_USERNAME, FAILED_ATTEMPTS + 1, false);

    attemptLogin(PROFILE_USERNAME, "wrong", AUTHENTICATION_FAILED_MESSAGE);
    assertProfileStateForUser(PROFILE_USERNAME, FAILED_ATTEMPTS + 2, false);
  }
}
