#include <gtest/gtest.h>
#include "odbc_connection.h"
#include "odbc_execution.h"
#include "odbc_result.h"
#include <stdlib.h>
#include <string>
#include <vector>
#include <cstring>
#include <iostream>
#include <dlg_specific.h>
#include "../common/common.h"

extern "C" {
#include "statement.h"
#include "connection.h"
}

using namespace std;

class OdbcapiTest: public ::testing::Test {
public:
    static SQLHENV hEnv;
    static SQLHDBC hDbc;
    SQLHSTMT hStmt;

protected:
    SQLHDBC killhDbc = SQL_NULL_HDBC;

    static void SetUpTestSuite()
    {
        SQLRETURN retcode;
        string connectionString = "DSN=ODBC_DT_A;UseServerSidePrepare=0";
        retcode = ConnectToDatabase(hEnv, hDbc, connectionString);
        ASSERT_TRUE(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO);
    }

    static void TearDownTestSuite()
    {
        SQLRETURN retcode;
        retcode = DisconnectFromDatabase(hDbc, hEnv);
        ASSERT_TRUE(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO);
    }

    void SetUp() override
    {
        SQLRETURN retcode;
        retcode = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        ASSERT_TRUE(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO);
        auto *unit_test = testing::UnitTest::GetInstance();
        auto *test_info = unit_test->current_test_info();
        cout << "====== " << test_info->test_suite_name() << "." << test_info->name() << ":Start "
             << "======" << endl;
    }

    void TearDown() override
    {
        auto *unit_test = testing::UnitTest::GetInstance();
        auto *test_info = unit_test->current_test_info();
        cout << "====== " << test_info->test_suite_name() << "." << test_info->name() << ":End "
             << "======" << endl;
        if (hStmt != SQL_NULL_HSTMT) {
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            hStmt = SQL_NULL_HSTMT;
        }
    }
};

SQLHENV OdbcapiTest::hEnv = SQL_NULL_HENV;
SQLHDBC OdbcapiTest::hDbc = SQL_NULL_HDBC;

static void FetchString(SQLHSTMT hstmt, SQLUSMALLINT col, string &out)
{
    char buffer[4096] = {0};
    SQLLEN ind = 0;
    SQLRETURN rc = SQLGetData(hstmt, col, SQL_C_CHAR, buffer, sizeof(buffer), &ind);
    ASSERT_TRUE(rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO);
    ASSERT_NE(SQL_NULL_DATA, ind);
    out.assign(buffer);
}

TEST_F(OdbcapiTest, ConvertToPgbinaryHexPathShouldRoundTripBinary_L0)
{
    SQLRETURN retcode;
    unsigned char input[] = {0x00, 0x01, 0x27, 0x5c, 0x80, 0xff};
    SQLLEN inputLen = sizeof(input);

    retcode = SQLPrepare(hStmt,
        (SQLCHAR *)"SELECT encode(?::bytea, 'hex')",
        SQL_NTS);
    CHECK_STMT_RESULT(retcode, "SQLPrepare", hStmt);

    retcode = SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT,
        SQL_C_BINARY, SQL_VARBINARY,
        sizeof(input), 0, input, sizeof(input), &inputLen);
    CHECK_STMT_RESULT(retcode, "SQLBindParameter", hStmt);

    retcode = SQLExecute(hStmt);
    CHECK_STMT_RESULT(retcode, "SQLExecute", hStmt);
    ASSERT_EQ(SQL_SUCCESS, SQLFetch(hStmt));

    string hex;
    FetchString(hStmt, 1, hex);
    EXPECT_EQ("0001275c80ff", hex);
}

TEST_F(OdbcapiTest, ConvertToPgbinaryShouldNotCorruptFollowingParameters_L0)
{
    SQLRETURN retcode;
    unsigned char input[] = {0x00, 0x27, 0x5c, 0x7f, 0x80, 0xff};
    SQLLEN inputLen1 = sizeof(input);
    SQLLEN inputLen2 = sizeof(input);
    SQLINTEGER marker = 149;
    SQLLEN markerLen = 0;

    retcode = SQLPrepare(hStmt,
        (SQLCHAR *)"SELECT length(?::bytea), encode(?::bytea, 'hex'), ?::int",
        SQL_NTS);
    CHECK_STMT_RESULT(retcode, "SQLPrepare", hStmt);

    retcode = SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT,
        SQL_C_BINARY, SQL_VARBINARY,
        sizeof(input), 0, input, sizeof(input), &inputLen1);
    CHECK_STMT_RESULT(retcode, "SQLBindParameter 1", hStmt);

    retcode = SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT,
        SQL_C_BINARY, SQL_VARBINARY,
        sizeof(input), 0, input, sizeof(input), &inputLen2);
    CHECK_STMT_RESULT(retcode, "SQLBindParameter 2", hStmt);

    retcode = SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT,
        SQL_C_SLONG, SQL_INTEGER,
        0, 0, &marker, 0, &markerLen);
    CHECK_STMT_RESULT(retcode, "SQLBindParameter 3", hStmt);

    retcode = SQLExecute(hStmt);
    CHECK_STMT_RESULT(retcode, "SQLExecute", hStmt);
    ASSERT_EQ(SQL_SUCCESS, SQLFetch(hStmt));

    SQLINTEGER byteLen = 0;
    SQLINTEGER returnedMarker = 0;
    SQLLEN ind = 0;
    retcode = SQLGetData(hStmt, 1, SQL_C_SLONG, &byteLen, sizeof(byteLen), &ind);
    ASSERT_EQ(SQL_SUCCESS, retcode);

    string hex;
    FetchString(hStmt, 2, hex);

    retcode = SQLGetData(hStmt, 3, SQL_C_SLONG, &returnedMarker, sizeof(returnedMarker), &ind);
    ASSERT_EQ(SQL_SUCCESS, retcode);

    EXPECT_EQ(6, byteLen);
    EXPECT_EQ("00275c7f80ff", hex);
    EXPECT_EQ(149, returnedMarker);
}

TEST_F(OdbcapiTest, ConvertToPgbinaryShouldHandleLargeBinaryAndExpandQueryBuffer_L0)
{
    SQLRETURN retcode;
    vector<unsigned char> input(1024);
    for (size_t i = 0; i < input.size(); i++) {
        input[i] = static_cast<unsigned char>(i & 0xff);
    }
    SQLLEN inputLen = static_cast<SQLLEN>(input.size());

    retcode = SQLPrepare(hStmt,
        (SQLCHAR *)"SELECT length(?::bytea), substr(encode(?::bytea, 'hex'), 1, 16)",
        SQL_NTS);
    CHECK_STMT_RESULT(retcode, "SQLPrepare", hStmt);

    retcode = SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT,
        SQL_C_BINARY, SQL_VARBINARY,
        input.size(), 0, input.data(), input.size(), &inputLen);
    CHECK_STMT_RESULT(retcode, "SQLBindParameter 1", hStmt);

    retcode = SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT,
        SQL_C_BINARY, SQL_VARBINARY,
        input.size(), 0, input.data(), input.size(), &inputLen);
    CHECK_STMT_RESULT(retcode, "SQLBindParameter 2", hStmt);

    retcode = SQLExecute(hStmt);
    CHECK_STMT_RESULT(retcode, "SQLExecute", hStmt);
    ASSERT_EQ(SQL_SUCCESS, SQLFetch(hStmt));

    SQLINTEGER byteLen = 0;
    SQLLEN ind = 0;
    retcode = SQLGetData(hStmt, 1, SQL_C_SLONG, &byteLen, sizeof(byteLen), &ind);
    ASSERT_EQ(SQL_SUCCESS, retcode);

    string prefix;
    FetchString(hStmt, 2, prefix);

    EXPECT_EQ(1024, byteLen);
    EXPECT_EQ("0001020304050607", prefix);
}
