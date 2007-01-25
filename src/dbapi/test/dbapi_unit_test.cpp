/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author: Sergey Sikorskiy
 *
 * File Description: DBAPI unit-test
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbithr.hpp>

#include <ncbi_source_ver.h>
#include <dbapi/dbapi.hpp>
#include <dbapi/driver/drivers.hpp>

#include "dbapi_unit_test.hpp"
#include <dbapi/driver/dbapi_svc_mapper.hpp>

#include <test/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE

enum { max_text_size = 8000 };
#define CONN_OWNERSHIP  eTakeOwnership
static const char* msg_record_expected = "Record expected";


///////////////////////////////////////////////////////////////////////////
class CErrHandler : public CDB_UserHandler
{
public:
    // Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    virtual bool HandleIt(CDB_Exception* ex);
};


///////////////////////////////////////////////////////////////////////////
class CODBCErrHandler : public CDB_UserHandler
{
public:
    // Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    virtual bool HandleIt(CDB_Exception* ex);
};



///////////////////////////////////////////////////////////////////////////////
// Patterns to test:
//      I) Statement:
//          1) New/dedicated statement for each test
//          2) Reusable statement for all tests

///////////////////////////////////////////////////////////////////////////////
CTestTransaction::CTestTransaction(
    IConnection& conn,
    ETransBehavior tb
    )
    : m_Conn( &conn )
    , m_TransBehavior( tb )
{
    if ( m_TransBehavior != eNoTrans ) {
        auto_ptr<IStatement> stmt( m_Conn->GetStatement() );
        stmt->ExecuteUpdate( "BEGIN TRANSACTION" );
    }
}

CTestTransaction::~CTestTransaction(void)
{
    try {
        if ( m_TransBehavior == eTransCommit ) {
            auto_ptr<IStatement> stmt( m_Conn->GetStatement() );
            stmt->ExecuteUpdate( "COMMIT TRANSACTION" );
        } else if ( m_TransBehavior == eTransRollback ) {
            auto_ptr<IStatement> stmt( m_Conn->GetStatement() );
            stmt->ExecuteUpdate( "ROLLBACK TRANSACTION" );
        }
    }
    catch( ... ) {
        // Just ignore ...
    }
}

///////////////////////////////////////////////////////////////////////////////
class CIConnectionIFPolicy
{
protected:
    static void Destroy(IConnection* obj)
    {
        delete obj;
    }
    static IConnection* Init(IDataSource* ds)
    {
        return ds->CreateConnection( CONN_OWNERSHIP );
    }
};


///////////////////////////////////////////////////////////////////////////////
bool CErrHandler::HandleIt(CDB_Exception* ex)
{
    if ( !ex )
        return false;

    // Ignore errors with ErrorCode set to 0
    // (this is related mostly to the FreeTDS driver)
    if (ex->GetDBErrCode() == 0)
        return true;

    // On other errors, throw an exception (was not allowed before!)
    throw *ex;
}


///////////////////////////////////////////////////////////////////////////////
bool CODBCErrHandler::HandleIt(CDB_Exception* ex)
{
    if ( !ex )
        return false;

    // On other errors, throw an exception (was not allowed before!)
    throw *ex;
}


///////////////////////////////////////////////////////////////////////////////
// void*
// GetAppSymbol(const char* symbol)
// {
// #ifdef WIN32
//     return ::GetProcAddress(::GetModuleHandle(NULL), symbol);
// #else
//     return ::dlsym(::dlopen(NULL, RTLD_LAZY), symbol);
// #endif
// }


///////////////////////////////////////////////////////////////////////////////
CDBAPIUnitTest::CDBAPIUnitTest(const CTestArguments& args)
: m_args(args)
, m_DM( CDriverManager::GetInstance() )
, m_DS( NULL )
, m_TableName( "#dbapi_unit_table" )
{
    SetDiagFilter(eDiagFilter_All, "!/dbapi/driver/ctlib");
}

CDBAPIUnitTest::~CDBAPIUnitTest(void)
{
//     I_DriverContext* drv_context = m_DS->GetDriverContext();
//
//     drv_context->PopDefConnMsgHandler( m_ErrHandler.get() );
//     drv_context->PopCntxMsgHandler( m_ErrHandler.get() );

    m_Conn.reset(NULL);
    m_DM.DestroyDs(m_args.GetDriverName());
    m_DS = NULL;
}


///////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Connect(const auto_ptr<IConnection>& conn) const
{
    conn->Connect(
        m_args.GetUserName(),
        m_args.GetUserPassword(),
        m_args.GetServerName()
//             m_args.GetDatabaseName()
        );
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::TestInit(void)
{
    try {
//         DBLB_INSTALL_DEFAULT();

        if ( m_args.GetServerType() == CTestArguments::eMsSql ) {
            m_max_varchar_size = 8000;
        } else {
            // Sybase
            m_max_varchar_size = 1900;
        }

#ifdef USE_STATICALLY_LINKED_DRIVERS

#ifdef HAVE_LIBSYBASE
        DBAPI_RegisterDriver_CTLIB();
        DBAPI_RegisterDriver_DBLIB();
#endif
        DBAPI_RegisterDriver_FTDS();

#endif

        m_DS = m_DM.CreateDs(m_args.GetDriverName(), &m_args.GetDBParameters());

        I_DriverContext* drv_context = m_DS->GetDriverContext();

        if ( m_args.GetDriverName() == "odbc" ||
            m_args.GetDriverName() == "odbcw" ||
            m_args.GetDriverName() == "ftds64_odbc" ) {
            drv_context->PushCntxMsgHandler(new CODBCErrHandler,
                                            eTakeOwnership
                                            );
            drv_context->PushDefConnMsgHandler(new CODBCErrHandler,
                                               eTakeOwnership
                                               );
        } else {
            drv_context->PushCntxMsgHandler(new CErrHandler, eTakeOwnership);
            drv_context->PushDefConnMsgHandler(new CErrHandler, eTakeOwnership);
        }

        m_Conn.reset( m_DS->CreateConnection( CONN_OWNERSHIP ) );
        BOOST_CHECK( m_Conn.get() != NULL );

        m_Conn->SetMode(IConnection::eBulkInsert);

        Connect(m_Conn);

        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Create a test table ...
        string sql;

        sql  = " CREATE TABLE " + GetTableName() + "( \n";
        sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
        sql += "    int_field INT NULL, \n";
        sql += "    vc1000_field VARCHAR(1000) NULL, \n";
        sql += "    text_field TEXT NULL \n";
        sql += " )";

        // Create the table
        auto_stmt->ExecuteUpdate(sql);

        sql  = " CREATE UNIQUE INDEX #ind01 ON " + GetTableName() + "( id ) \n";
        // Create an index
        auto_stmt->ExecuteUpdate( sql );

        // Table for bulk insert ...
        if ( m_args.GetServerType() == CTestArguments::eMsSql ) {
            sql  = " CREATE TABLE #bulk_insert_table( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vc8000_field VARCHAR(8000) NULL, \n";
            sql += "    int_field INT NULL, \n";
            sql += "    bigint_field BIGINT NULL \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);

            sql  = " CREATE TABLE #bin_bulk_insert_table( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vb8000_field VARBINARY(8000) NULL, \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);
        } else
        {
            sql  = " CREATE TABLE #bulk_insert_table( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vc8000_field VARCHAR(1900) NULL, \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);

            sql  = " CREATE TABLE #bin_bulk_insert_table( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vb8000_field VARBINARY(1900) NULL \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);
        }

        sql  = " CREATE TABLE #test_unicode_table ( \n";
        sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
        sql += "    nvc255_field NVARCHAR(255) NULL \n";
        sql += " )";

        // Create table
        auto_stmt->ExecuteUpdate(sql);
    }
    catch(const CDB_Exception& ex) {
        BOOST_FAIL(ex.GetMsg());
    }
    catch(const CPluginManagerException& ex) {
        BOOST_FAIL(ex.GetMsg());
    }
    catch(const CException& ex) {
        BOOST_FAIL(ex.GetMsg());
    }
    catch (...) {
        BOOST_FAIL("Couldn't initialize the test-suite.");
    }
}

class CTestErrHandler : public CDB_UserHandler
{
public:
    // c-tor
    CTestErrHandler();
    // d-tor
    virtual ~CTestErrHandler();

public:
    // Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    virtual bool HandleIt(CDB_Exception* ex);

    // Get current global "last-resort" error handler.
    // If not set, then the default will be "CDB_UserHandler_Default".
    // This handler is guaranteed to be valid up to the program termination,
    // and it will call the user-defined hs_Procandler last set by SetDefault().
    // NOTE:  never pass it to SetDefault, like:  "SetDefault(&GetDefault())"!
    static CDB_UserHandler& GetDefault(void);

    // Alternate the default global "last-resort" error handler.
    // Passing NULL will mean to ignore all errors that reach it.
    // Return previously set (or default-default if not set yet) handler.
    // The returned handler should be delete'd by the caller; the last set
    // handler will be delete'd automagically on the program termination.
    static CDB_UserHandler* SetDefault(CDB_UserHandler* h);

public:
    EDB_Severity getMaxSeverity() {
        return m_max_severity;
    }
    bool GetSucceed(void) const
    {
        return m_Succeed;
    }
    void Init(void)
    {
        m_Succeed = false;
    }

private:
    EDB_Severity    m_max_severity;
    bool            m_Succeed;
};

CTestErrHandler::CTestErrHandler()
: m_max_severity(eDB_Info)
, m_Succeed(false)
{
}

CTestErrHandler::~CTestErrHandler()
{
   ;
}

bool CTestErrHandler::HandleIt(CDB_Exception* ex)
{
    m_Succeed = true;

    if (ex && ex->Severity() > m_max_severity)
    {
        m_max_severity = ex->Severity();
    }

    // return false to find the next handler on the stack.
    // There is always one default stack

    return false;
}


///////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_Unicode(void)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
    string str_rus("��� �ٽ� ���");
    string str_ger("Au�erdem k�nnen Sie einzelne Eintr�ge aus Ihrem "
                   "Suchprotokoll entfernen");

//     string str_utf8("\320\237\321\203\320\277\320\272\320\270\320\275");
    string str_utf8("\xD0\x9F\xD1\x83\xD0\xBF\xD0\xBA\xD0\xB8\xD0\xBD");
//     string str_utf8("HELLO");

    CStringUTF8 utf8_str_1252_rus(str_rus, eEncoding_Windows_1252);
    CStringUTF8 utf8_str_1252_ger(str_ger, eEncoding_Windows_1252);
    CStringUTF8 utf8_str_utf8(str_utf8, eEncoding_UTF8);

    BOOST_CHECK( str_utf8.size() > 0 );
    BOOST_CHECK_EQUAL( str_utf8, utf8_str_utf8 );

    BOOST_CHECK( utf8_str_1252_rus.IsValid() );
    BOOST_CHECK( utf8_str_1252_ger.IsValid() );
    BOOST_CHECK( utf8_str_utf8.IsValid() );

    try {
        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM #test_unicode_table" );

        // Insert data ...
        {
            sql = "INSERT INTO #test_unicode_table(nvc255_field) "
                  "VALUES(@nvc_val)";

            //
            BOOST_CHECK( utf8_str_utf8.size() > 0 );
            BOOST_CHECK_EQUAL( str_utf8, utf8_str_utf8 );
            auto_stmt->SetParam( CVariant::VarChar(utf8_str_utf8.c_str(),
                                                   utf8_str_utf8.size()),
                                 "@nvc_val" );
            auto_stmt->ExecuteUpdate(sql);

            //
            BOOST_CHECK( utf8_str_1252_rus.size() > 0 );
            auto_stmt->SetParam( CVariant::VarChar(utf8_str_1252_rus.c_str(),
                                                   utf8_str_1252_rus.size()),
                                 "@nvc_val" );
            auto_stmt->ExecuteUpdate(sql);

            //
            BOOST_CHECK( utf8_str_1252_ger.size() > 0 );
            auto_stmt->SetParam( CVariant::VarChar(utf8_str_1252_ger.c_str(),
                                                   utf8_str_1252_ger.size()),
                                 "@nvc_val" );
            auto_stmt->ExecuteUpdate(sql);
        }

        // Retrieve data ...
        {
            string nvc255_value;

            sql  = " SELECT nvc255_field FROM #test_unicode_table";
            sql += " ORDER BY id";

            auto_stmt->SendSql( sql );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            // Read utf8_str_utf8 ...
            BOOST_CHECK( rs->Next() );
            nvc255_value = rs->GetVariant(1).GetString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_utf8.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( utf8_str_utf8, nvc255_value );
            CStringUTF8 utf8_utf8(nvc255_value, eEncoding_UTF8);

            // Read utf8_str_1252_rus ...
            BOOST_CHECK( rs->Next() );
            nvc255_value = rs->GetVariant(1).GetString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_1252_rus.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( utf8_str_1252_rus, nvc255_value );
            CStringUTF8 utf8_rus(nvc255_value, eEncoding_UTF8);
            string value_rus =
                utf8_rus.AsSingleByteString(eEncoding_Windows_1252);
            BOOST_CHECK_EQUAL( str_rus, value_rus );

            // Read utf8_str_1252_ger ...
            BOOST_CHECK( rs->Next() );
            nvc255_value = rs->GetVariant(1).GetString();
            BOOST_CHECK( nvc255_value.size() > 0);
            BOOST_CHECK_EQUAL( utf8_str_1252_ger.size(), nvc255_value.size() );
            BOOST_CHECK_EQUAL( utf8_str_1252_ger, nvc255_value );
            CStringUTF8 utf8_ger(nvc255_value, eEncoding_UTF8);
            string value_ger =
                utf8_ger.AsSingleByteString(eEncoding_Windows_1252);
            BOOST_CHECK_EQUAL( str_ger, value_ger );

            DumpResults(auto_stmt.get());
        }

        // Use permanent table ...
        if (false) {
            auto_stmt->ExecuteUpdate( "DELETE FROM DBAPI_Sample.."
                                      "test_nstring_table" );

            // Insert data ...
            {
                sql = "INSERT INTO DBAPI_Sample..test_nstring_table"
                    "(first_field) VALUES(@val)";

                //
                auto_stmt->SetParam( CVariant::VarChar(utf8_str_1252_rus.c_str(),
                                                       utf8_str_1252_rus.size()),
                                     "@val" );
                auto_stmt->ExecuteUpdate(sql);

                //
                auto_stmt->SetParam( CVariant::VarChar(utf8_str_1252_ger.c_str(),
                                                       utf8_str_1252_ger.size()),
                                     "@val" );
                auto_stmt->ExecuteUpdate(sql);

                //
                auto_stmt->SetParam( CVariant::VarChar(utf8_str_utf8.c_str(),
                                                       utf8_str_utf8.size()),
                                     "@val" );
                auto_stmt->ExecuteUpdate(sql);
            }

            // Retrieve data ....
            {
                string nvc255_value;

                sql  = " SELECT first_field FROM DBAPI_Sample..test_nstring_table";
                sql += " ORDER BY id";

                auto_stmt->SendSql( sql );

                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );

                // Read utf8_str_1252_rus ...
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK_EQUAL(utf8_str_1252_rus.size(),
                                  nvc255_value.size()
                                  );
                BOOST_CHECK_EQUAL(utf8_str_1252_rus, nvc255_value);
                CStringUTF8 utf8_rus(nvc255_value, eEncoding_UTF8);
                string value_rus =
                    utf8_rus.AsSingleByteString(eEncoding_Windows_1252);
                BOOST_CHECK_EQUAL( str_rus, value_rus );

                // Read utf8_str_1252_ger ...
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK_EQUAL( utf8_str_1252_ger.size(),
                                   nvc255_value.size()
                                   );
                BOOST_CHECK_EQUAL( utf8_str_1252_ger, nvc255_value );
                CStringUTF8 utf8_ger(nvc255_value, eEncoding_UTF8);
                string value_ger =
                    utf8_ger.AsSingleByteString(eEncoding_Windows_1252);
                BOOST_CHECK_EQUAL( str_ger, value_ger );

                // Read utf8_str_utf8 ...
                BOOST_CHECK( rs->Next() );
                nvc255_value = rs->GetVariant(1).GetString();
                BOOST_CHECK_EQUAL( utf8_str_utf8.size(), nvc255_value.size() );
                BOOST_CHECK_EQUAL( utf8_str_utf8, nvc255_value );
                CStringUTF8 utf8_utf8(nvc255_value, eEncoding_UTF8);

                DumpResults(auto_stmt.get());
            }
        }
    }
    catch(CDB_Exception& ex) {
        BOOST_FAIL(ex.GetMsg());
    }
    catch(CException& ex) {
        BOOST_FAIL(ex.GetMsg());
    }
}


// Based on Soussov's API.
void CDBAPIUnitTest::Test_Iskhakov(void)
{
    string sql;

    auto_ptr<CDB_Connection> auto_conn(
        m_DS->GetDriverContext()->Connect(
            "LINK_OS",
            "anyone",
            "allowed",
            0)
        );
    BOOST_CHECK( auto_conn.get() != NULL );

    sql  = "get_iodesc_for_link pubmed_pubmed";

    auto_ptr<CDB_LangCmd> auto_stmt( auto_conn->LangCmd(sql) );
    BOOST_CHECK( auto_stmt.get() != NULL );

    bool rc = auto_stmt->Send();
    BOOST_CHECK( rc );

    auto_ptr<I_ITDescriptor> descr;

    while(auto_stmt -> HasMoreResults()) {
        auto_ptr<CDB_Result> rs(auto_stmt -> Result());

        if (rs.get() == NULL) {
            continue;
        }

        if (rs->ResultType() != eDB_RowResult) {
            continue;
        }

        while(rs->Fetch()) {
            rs->ReadItem(NULL, 0);

            descr.reset(rs->GetImageOrTextDescriptor());
        }
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Create_Destroy(void)
{
    ///////////////////////
    // CreateStatement
    ///////////////////////

    // Destroy a statement before a connection get destroyed ...
    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        auto_ptr<IStatement> stmt(local_conn->CreateStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt.get());
    }

    // Do not destroy statement, let it be destroyed ...
    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        IStatement* stmt(local_conn->CreateStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt);
    }

    ///////////////////////
    // GetStatement
    ///////////////////////

    // Destroy a statement before a connection get destroyed ...
    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        auto_ptr<IStatement> stmt(local_conn->GetStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt.get());
    }

    // Do not destroy statement, let it be destroyed ...
    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        IStatement* stmt(local_conn->GetStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt);
    }
}

void CDBAPIUnitTest::Test_Multiple_Close(void)
{
    ///////////////////////
    // CreateStatement
    ///////////////////////

    // Destroy a statement before a connection get destroyed ...
    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        auto_ptr<IStatement> stmt(local_conn->CreateStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt.get());

        // Close a statement first ...
        stmt->Close();
        stmt->Close();
        stmt->Close();

        local_conn->Close();
        local_conn->Close();
        local_conn->Close();
    }

    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        auto_ptr<IStatement> stmt(local_conn->CreateStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt.get());

        // Close a connection first ...
        local_conn->Close();
        local_conn->Close();
        local_conn->Close();

        stmt->Close();
        stmt->Close();
        stmt->Close();
    }

    // Do not destroy a statement, let it be destroyed ...
    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        IStatement* stmt(local_conn->CreateStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt);

        // Close a statement first ...
        stmt->Close();
        stmt->Close();
        stmt->Close();

        local_conn->Close();
        local_conn->Close();
        local_conn->Close();
    }

    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        IStatement* stmt(local_conn->CreateStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt);

        // Close a connection first ...
        local_conn->Close();
        local_conn->Close();
        local_conn->Close();

        stmt->Close();
        stmt->Close();
        stmt->Close();
    }

    ///////////////////////
    // GetStatement
    ///////////////////////

    // Destroy a statement before a connection get destroyed ...
    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        auto_ptr<IStatement> stmt(local_conn->GetStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt.get());

        // Close a statement first ...
        stmt->Close();
        stmt->Close();
        stmt->Close();

        local_conn->Close();
        local_conn->Close();
        local_conn->Close();
    }

    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        auto_ptr<IStatement> stmt(local_conn->GetStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt.get());

        // Close a connection first ...
        local_conn->Close();
        local_conn->Close();
        local_conn->Close();

        stmt->Close();
        stmt->Close();
        stmt->Close();
    }

    // Do not destroy a statement, let it be destroyed ...
    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        IStatement* stmt(local_conn->GetStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt);

        // Close a statement first ...
        stmt->Close();
        stmt->Close();
        stmt->Close();

        local_conn->Close();
        local_conn->Close();
        local_conn->Close();
    }

    {
        auto_ptr<IConnection> local_conn(
            m_DS->CreateConnection( CONN_OWNERSHIP )
            );
        Connect(local_conn);

        IStatement* stmt(local_conn->GetStatement());
        stmt->SendSql( "SELECT name FROM sysobjects" );
        DumpResults(stmt);

        // Close a connection first ...
        local_conn->Close();
        local_conn->Close();
        local_conn->Close();

        stmt->Close();
        stmt->Close();
        stmt->Close();
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_HasMoreResults(void)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // First test ...
    // This test shouldn't throw.
    // Sergey Koshelkov complained that this code throws an exception with
    // info-level severity.
    // ftds (windows):  HasMoreResults() never returns;
    // odbs (windows):  HasMoreResults() throws exception:
    try {
        auto_stmt->SendSql( "exec sp_spaceused @updateusage='true'" );

        BOOST_CHECK( auto_stmt->HasMoreResults() );
        BOOST_CHECK( auto_stmt->HasRows() );
        auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
        BOOST_CHECK( rs.get() != NULL );

        while (auto_stmt->HasMoreResults()) {
            if (auto_stmt->HasRows()) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            }
        }
    }
    catch( const CDB_Exception& ex ) {
        BOOST_FAIL( ex.GetMsg() );
    }
    catch(...) {
        BOOST_FAIL("Unknown error in Test_HasMoreResults");
    }

    // Second test ...
    {
        try {
            auto_stmt->SendSql( "exec sp_spaceused @updateusage='true'" );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            while (auto_stmt->HasMoreResults()) {
                if (auto_stmt->HasRows()) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                }
            }
        }
        catch( const CDB_Exception& ex ) {
            BOOST_FAIL( ex.GetMsg() );
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Insert(void)
{
    string sql;
    const string small_msg("%");
    const string test_msg(300, 'A');
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // Clean table ...
    auto_stmt->ExecuteUpdate( "DELETE FROM " + GetTableName() );

    // Insert data ...
    {
        // Pure SQL ...
        auto_stmt->ExecuteUpdate(
            "INSERT INTO " + GetTableName() + "(int_field, vc1000_field) "
            "VALUES(1, '" + test_msg + "')"
            );

        // Using parameters ...
        string sql = "INSERT INTO " + GetTableName() +
                     "(int_field, vc1000_field) VALUES(@id, @vc_val)";

        auto_stmt->SetParam( CVariant( Int4(2) ), "@id" );
        auto_stmt->SetParam( CVariant::LongChar(test_msg.c_str(),
                                                test_msg.size()),
                             "@vc_val"
                             );
        auto_stmt->ExecuteUpdate( sql );

        auto_stmt->SetParam( CVariant( Int4(3) ), "@id" );
        auto_stmt->SetParam( CVariant::LongChar(small_msg.c_str(),
                                                small_msg.size()),
                             "@vc_val"
                             );
        auto_stmt->ExecuteUpdate( sql );

        CVariant var_value(CVariant::LongChar(NULL, 1024));
        var_value = CVariant::LongChar(small_msg.c_str(), small_msg.size());
        auto_stmt->SetParam( CVariant( Int4(4) ), "@id" );
        auto_stmt->SetParam( CVariant::LongChar(small_msg.c_str(),
                                                small_msg.size()),
                             "@vc_val"
                             );
        auto_stmt->ExecuteUpdate( sql );
    }

    // Retrieve data ...
    {
        enum { num_of_tests = 2 };
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        sql  = " SELECT int_field, vc1000_field FROM " + GetTableName();
        sql += " ORDER BY int_field";

        auto_stmt->SendSql( sql );

        BOOST_CHECK( auto_stmt->HasMoreResults() );
        BOOST_CHECK( auto_stmt->HasRows() );
        auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
        BOOST_CHECK( rs.get() != NULL );

        for(int i = 0; i < num_of_tests; ++i ) {
            BOOST_CHECK( rs->Next() );

            string vc1000_value = rs->GetVariant(2).GetString();

            BOOST_CHECK_EQUAL( test_msg, vc1000_value );
        }

        BOOST_CHECK( rs->Next() );
        string small_value1 = rs->GetVariant(2).GetString();
        BOOST_CHECK_EQUAL( small_msg, small_value1 );

        BOOST_CHECK( rs->Next() );
        string small_value2 = rs->GetVariant(2).GetString();
        BOOST_CHECK_EQUAL( small_msg, small_value2 );
    }
};

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_DateTime(void)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
    CVariant value(eDB_DateTime);
    CTime empty_ctime;
    CVariant null_date(empty_ctime, eLong);
    CTime dt_value;

    if (true) {
        // Initialization ...
        {
            sql =
                "CREATE TABLE #test_datetime ( \n"
                "   id INT, \n"
                "   dt_field DATETIME NULL \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        {
            // Initialization ...
            {
                sql = "INSERT INTO #test_datetime(id, dt_field) "
                      "VALUES(1, GETDATE() )";

                auto_stmt->ExecuteUpdate( sql );
            }

            // Retrieve data ...
            {
                sql = "SELECT * FROM #test_datetime";

                auto_stmt->SendSql( sql );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                BOOST_CHECK( rs.get() != NULL );
                BOOST_CHECK( rs->Next() );

                value = rs->GetVariant(2);

                BOOST_CHECK( !value.IsNull() );

                dt_value = value.GetCTime();
                BOOST_CHECK( !dt_value.IsEmpty() );
            }

            // Insert data using parameters ...
            {
                auto_stmt->ExecuteUpdate( "DELETE FROM #test_datetime" );

                auto_stmt->SetParam( value, "@dt_val" );

                sql = "INSERT INTO #test_datetime(id, dt_field) "
                      "VALUES(1, @dt_val)";

                auto_stmt->ExecuteUpdate( sql );
            }

            // Retrieve data again ...
            {
                sql = "SELECT * FROM #test_datetime";

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                auto_stmt->SendSql( sql );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                BOOST_CHECK( rs.get() != NULL );
                BOOST_CHECK( rs->Next() );

                const CVariant& value2 = rs->GetVariant(2);

                BOOST_CHECK( !value2.IsNull() );

                const CTime& dt_value2 = value2.GetCTime();
                BOOST_CHECK( !dt_value2.IsEmpty() );
                CTime dt_value3 = value.GetCTime();
                BOOST_CHECK_EQUAL( dt_value2.AsString(), dt_value3.AsString() );
            }

            // Insert NULL data using parameters ...
            {
                auto_stmt->ExecuteUpdate( "DELETE FROM #test_datetime" );

                auto_stmt->SetParam( null_date, "@dt_val" );

                sql = "INSERT INTO #test_datetime(id, dt_field) "
                      "VALUES(1, @dt_val)";

                auto_stmt->ExecuteUpdate( sql );
            }


            // Retrieve data again ...
            {
                sql = "SELECT * FROM #test_datetime";

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                auto_stmt->SendSql( sql );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                BOOST_CHECK( rs.get() != NULL );
                BOOST_CHECK( rs->Next() );

                const CVariant& value2 = rs->GetVariant(2);

                BOOST_CHECK( value2.IsNull() );
            }
        }

        // Insert data using stored procedure ...
        {
            value = CTime(CTime::eCurrent);

            // Set a database ...
            {
                auto_stmt->ExecuteUpdate("use DBAPI_Sample");
            }
            // Create a stored procedure ...
            {
                bool already_exist = false;

                // auto_stmt->SendSql( "select * FROM sysobjects WHERE xtype = 'P' AND name = 'sp_test_datetime'" );
                auto_stmt->SendSql( "select * FROM sysobjects WHERE name = 'sp_test_datetime'" );
                while( auto_stmt->HasMoreResults() ) {
                    if( auto_stmt->HasRows() ) {
                        auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                        while ( rs->Next() ) {
                            already_exist = true;
                        }
                    }
                }

                if ( !already_exist ) {
                    sql =
                        " CREATE PROC sp_test_datetime(@dt_val datetime) \n"
                        " AS \n"
                        " INSERT INTO #test_datetime(id, dt_field) VALUES(1, @dt_val) \n";

                    auto_stmt->ExecuteUpdate( sql );
                }

            }

            // Insert data using parameters ...
            {
                auto_stmt->ExecuteUpdate( "DELETE FROM #test_datetime" );

                auto_ptr<ICallableStatement> call_auto_stmt(
                    m_Conn->GetCallableStatement("sp_test_datetime", 1)
                    );

                call_auto_stmt->SetParam( value, "@dt_val" );
                call_auto_stmt->Execute();
                DumpResults(call_auto_stmt.get());

                auto_stmt->ExecuteUpdate( "SELECT * FROM #test_datetime" );
                int nRows = auto_stmt->GetRowCount();
                BOOST_CHECK_EQUAL(nRows, 1);
            }

            // Retrieve data ...
            {
                sql = "SELECT * FROM #test_datetime";

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                auto_stmt->SendSql( sql );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                BOOST_CHECK( rs.get() != NULL );
                BOOST_CHECK( rs->Next() );

                const CVariant& value2 = rs->GetVariant(2);

                BOOST_CHECK( !value2.IsNull() );

                const CTime& dt_value2 = value2.GetCTime();
                BOOST_CHECK( !dt_value2.IsEmpty() );
                CTime dt_value3 = value.GetCTime();
                BOOST_CHECK_EQUAL( dt_value2.AsString(), dt_value3.AsString() );

                // Failed for some reason ...
                // BOOST_CHECK(dt_value2 == dt_value);
            }

            // Insert NULL data using parameters ...
            {
                auto_stmt->ExecuteUpdate( "DELETE FROM #test_datetime" );

                auto_ptr<ICallableStatement> call_auto_stmt(
                    m_Conn->GetCallableStatement("sp_test_datetime", 1)
                    );

                call_auto_stmt->SetParam( null_date, "@dt_val" );
                call_auto_stmt->Execute();
                DumpResults(call_auto_stmt.get());
            }

            // Retrieve data ...
            {
                sql = "SELECT * FROM #test_datetime";

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                auto_stmt->SendSql( sql );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                BOOST_CHECK( rs.get() != NULL );
                BOOST_CHECK( rs->Next() );

                const CVariant& value2 = rs->GetVariant(2);

                BOOST_CHECK( value2.IsNull() );
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_DateTimeBCP(void)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
    CVariant value(eDB_DateTime);
    CTime empty_ctime;
    CVariant null_date(empty_ctime, eLong);
    CTime dt_value;

    // Initialization ...
    {
        sql =
            "CREATE TABLE #test_bcp_datetime ( \n"
            "   id INT, \n"
            "   dt_field DATETIME NULL \n"
            ") \n";

        auto_stmt->ExecuteUpdate( sql );
    }

    // Insert data using BCP ...
    {
        value = CTime(CTime::eCurrent);

        // Insert data using parameters ...
        {
            CVariant col1(eDB_Int);
            CVariant col2(eDB_DateTime);

            auto_stmt->ExecuteUpdate( "DELETE FROM #test_bcp_datetime" );

            auto_ptr<IBulkInsert> bi(
                m_Conn->GetBulkInsert("#test_bcp_datetime", 2)
                );

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);

            col1 = 1;
            col2 = value;

            bi->AddRow();
            bi->Complete();

            auto_stmt->ExecuteUpdate( "SELECT * FROM #test_bcp_datetime" );
            int nRows = auto_stmt->GetRowCount();
            BOOST_CHECK_EQUAL(nRows, 1);
        }

        // Retrieve data ...
        {
            sql = "SELECT * FROM #test_bcp_datetime";

            // !!! Do not forget to clear a parameter list ....
            // Workaround for the ctlib driver ...
            auto_stmt->ClearParamList();

            auto_stmt->SendSql( sql );
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            const CVariant& value2 = rs->GetVariant(2);

            BOOST_CHECK( !value2.IsNull() );

            const CTime& dt_value2 = value2.GetCTime();
            BOOST_CHECK( !dt_value2.IsEmpty() );
            CTime dt_value3 = value.GetCTime();
            BOOST_CHECK_EQUAL( dt_value2.AsString(), dt_value3.AsString() );

            // Failed for some reason ...
            // BOOST_CHECK(dt_value2 == dt_value);
        }

        // Insert NULL data using parameters ...
        {
            CVariant col1(eDB_Int);
            CVariant col2(eDB_DateTime);

            auto_stmt->ExecuteUpdate( "DELETE FROM #test_bcp_datetime" );

            auto_ptr<IBulkInsert> bi(
                m_Conn->GetBulkInsert("#test_bcp_datetime", 2)
                );

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);

            col1 = 1;

            bi->AddRow();
            bi->Complete();

            auto_stmt->ExecuteUpdate( "SELECT * FROM #test_bcp_datetime" );
            int nRows = auto_stmt->GetRowCount();
            BOOST_CHECK_EQUAL(nRows, 1);
        }

        // Retrieve data ...
        {
            sql = "SELECT * FROM #test_bcp_datetime";

            // !!! Do not forget to clear a parameter list ....
            // Workaround for the ctlib driver ...
            auto_stmt->ClearParamList();

            auto_stmt->SendSql( sql );
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            const CVariant& value2 = rs->GetVariant(2);

            BOOST_CHECK( value2.IsNull() );
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_UNIQUE(void)
{
    string sql;
    CVariant value(eDB_VarBinary, 16);

    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // Initialization ...
    {
        sql =
            "CREATE TABLE #test_unique ( \n"
            "   id INT , \n"
            "   unique_field UNIQUEIDENTIFIER DEFAULT NEWID() \n"
            ") \n";

        auto_stmt->ExecuteUpdate( sql );

        sql = "INSERT INTO #test_unique(id) VALUES(1)";

        auto_stmt->ExecuteUpdate( sql );
    }

    // Retrieve data ...
    {
        sql = "SELECT * FROM #test_unique";

        auto_stmt->SendSql( sql );
        while( auto_stmt->HasMoreResults() ) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                while ( rs->Next() ) {
                    value = rs->GetVariant(2);
                    string str_value = value.GetString();
                    string str_value2 = str_value;
                }
            }
        }
    }

    // Insert retrieved data back into the table ...
    {
        auto_stmt->SetParam( value, "@guid" );

        sql = "INSERT INTO #test_unique(id, unique_field) VALUES(2, @guid)";

        auto_stmt->ExecuteUpdate( sql );
    }

    // Retrieve data again ...
    {
        sql = "SELECT * FROM #test_unique";

        auto_stmt->SendSql( sql );
        while( auto_stmt->HasMoreResults() ) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                while ( rs->Next() ) {
                    const CVariant& cur_value = rs->GetVariant(2);

                    BOOST_CHECK( !cur_value.IsNull() );
                }
            }
        }
    }

}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_LOB(void)
{
    static char clob_value[] = "1234567890";
    string sql;

    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // Prepare data ...
    {
        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );

        // Insert data ...
        sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
        sql += " VALUES(0, '')";
        auto_stmt->ExecuteUpdate( sql );

        sql  = " SELECT text_field FROM " + GetTableName();
        // sql += " FOR UPDATE OF text_field";

        auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test03", sql));

        // blobRs should be destroyed before auto_cursor ...
        auto_ptr<IResultSet> blobRs(auto_cursor->Open());
        while(blobRs->Next()) {
            ostream& out = auto_cursor->GetBlobOStream(1,
                                                       sizeof(clob_value) - 1,
                                                       eDisableLog);
            out.write(clob_value, sizeof(clob_value) - 1);
            out.flush();
        }

//         auto_stmt->SendSql( sql );
//         while( auto_stmt->HasMoreResults() ) {
//             if( auto_stmt->HasRows() ) {
//                 auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
//                 while ( rs->Next() ) {
//                     ostream& out = rs->GetBlobOStream(sizeof(clob_value) - 1, eDisableLog);
//                     out.write(clob_value, sizeof(clob_value) - 1);
//                     out.flush();
//                 }
//             }
//         }
    }

    // Retrieve data ...
    {
        sql = "SELECT text_field FROM "+ GetTableName();

        auto_stmt->SendSql( sql );
        while( auto_stmt->HasMoreResults() ) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                rs->BindBlobToVariant(true);

                while ( rs->Next() ) {
                    const CVariant& value = rs->GetVariant(1);


                    BOOST_CHECK( !value.IsNull() );

                    size_t blob_size = value.GetBlobSize();
                    BOOST_CHECK_EQUAL(sizeof(clob_value) - 1, blob_size);
                    // Int8 value = rs->GetVariant(1).GetInt8();
                }
            }
        }
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_LOB2(void)
{
    static char clob_value[] = "1234567890";
    string sql;
    enum {num_of_records = 10};

    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // Prepare data ...
    {
        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );

        // Insert data ...

        // Insert empty CLOB.
        {
            sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
            sql += " VALUES(0, '')";

            for (int i = 0; i < num_of_records; ++i) {
                auto_stmt->ExecuteUpdate( sql );
            }
        }

        // Update CLOB value.
        {
            sql  = " SELECT text_field FROM " + GetTableName();
            // sql += " FOR UPDATE OF text_field";

            auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test03", sql));

            // blobRs should be destroyed before auto_cursor ...
            auto_ptr<IResultSet> blobRs(auto_cursor->Open());
            while(blobRs->Next()) {
                ostream& out =
                    auto_cursor->GetBlobOStream(1,
                                                sizeof(clob_value) - 1,
                                                eDisableLog
                                                );
                out.write(clob_value, sizeof(clob_value) - 1);
                out.flush();
            }
        }
    }

    // Retrieve data ...
    {
        sql = "SELECT text_field FROM "+ GetTableName();

        auto_stmt->SendSql( sql );
        while( auto_stmt->HasMoreResults() ) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                rs->BindBlobToVariant(true);

                while ( rs->Next() ) {
                    const CVariant& value = rs->GetVariant(1);

                    BOOST_CHECK( !value.IsNull() );

                    size_t blob_size = value.GetBlobSize();
                    BOOST_CHECK_EQUAL(sizeof(clob_value) - 1, blob_size);
                }
            }
        }
    }
}


void
CDBAPIUnitTest::Test_BulkInsertBlob(void)
{
    string sql;
    enum { record_num = 100 };

    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // Prepare data ...
    {
        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );
    }

    // Insert data ...
    {
        auto_ptr<IBulkInsert> bi(m_Conn->CreateBulkInsert(GetTableName(), 4));
        CVariant col1(eDB_Int);
        CVariant col2(eDB_Int);
        CVariant col3(eDB_VarChar);
        CVariant col4(eDB_Text);

        bi->Bind(1, &col1);
        bi->Bind(2, &col2);
        bi->Bind(3, &col3);
        bi->Bind(4, &col4);

        for( int i = 0; i < record_num; ++i ) {
            string im = NStr::IntToString(i);
            col1 = i;
            col2 = i;
            col3 = im;
            col4.Truncate();
            col4.Append(im.c_str(), im.size());
            bi->AddRow();
        }

        bi->Complete();
    }
}

void
CDBAPIUnitTest::Test_BlobStream(void)
{
    string sql;
    enum {test_size = 10000};
    long data_len = 0;
    long write_data_len = 0;

    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // Prepare data ...
    {
        ostrstream out;

        for (int i = 0; i < test_size; ++i) {
            out << i << " ";
        }

        data_len = out.pcount();
        BOOST_CHECK(data_len > 0);

        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM "+ GetTableName() );

        // Insert data ...
        sql  = " INSERT INTO " + GetTableName() + "(int_field, text_field)";
        sql += " VALUES(0, '')";
        auto_stmt->ExecuteUpdate( sql );

        sql  = " SELECT text_field FROM " + GetTableName();

        auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test03", sql));

        // blobRs should be destroyed before auto_cursor ...
        auto_ptr<IResultSet> blobRs(auto_cursor->Open());
        while(blobRs->Next()) {
            ostream& ostrm = auto_cursor->GetBlobOStream(1,
                                                         data_len,
                                                         eDisableLog);

            ostrm.write(out.str(), data_len);
            out.freeze(false);

            BOOST_CHECK_EQUAL(ostrm.fail(), false);

            ostrm.flush();

            BOOST_CHECK_EQUAL(ostrm.fail(), false);
            BOOST_CHECK_EQUAL(ostrm.good(), true);
            // BOOST_CHECK_EQUAL(int(ostrm.tellp()), int(out.pcount()));
        }

        auto_stmt->SendSql( "SELECT datalength(text_field) FROM " +
                            GetTableName() );

        while (auto_stmt->HasMoreResults()) {
            if (auto_stmt->HasRows()) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );
                BOOST_CHECK( rs->Next() );
                write_data_len = rs->GetVariant(1).GetInt4();
                BOOST_CHECK_EQUAL( data_len, write_data_len );
                while (rs->Next()) {}
            }
        }
    }

    // Retrieve data ...
    {
        auto_stmt->ExecuteUpdate("set textsize 2000000");

        sql = "SELECT text_field FROM "+ GetTableName();

        auto_stmt->SendSql( sql );
        while( auto_stmt->HasMoreResults() ) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                try {
                    while (rs->Next()) {
                        istream& strm = rs->GetBlobIStream();
                        int j = 0;
                        for (int i = 0; i < test_size; ++i) {
                            strm >> j;

                            // if (i == 5000) {
                            //     DATABASE_DRIVER_ERROR( "Exception safety test.", 0 );
                            // }

                            BOOST_CHECK_EQUAL(strm.good(), true);
                            BOOST_CHECK_EQUAL(strm.eof(), false);
                            BOOST_CHECK_EQUAL(j, i);
                        }
                        long read_data_len = strm.tellg();
                        // Calculate a trailing space.
                        BOOST_CHECK_EQUAL(data_len, read_data_len + 1);
                    }
                }
                catch( const CDB_Exception& ) {
                    rs->Close();
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_GetTotalColumns(void)
{
    string sql;

    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // Create table ...
    {
        sql =
            "CREATE TABLE #Overlaps ( \n"
            "   pairId int NOT NULL , \n"
            "   overlapNum smallint NOT NULL , \n"
            "   start1 int NOT NULL , \n"
            "   start2 int NOT NULL , \n"
            "   stop1 int NOT NULL , \n"
            "   stop2 int NOT NULL , \n"
            "   orient char (2) NOT NULL , \n"
            "   gaps int NOT NULL , \n"
            "   mismatches int NOT NULL , \n"
            "   adjustedLen int NOT NULL , \n"
            "   length int NOT NULL , \n"
            "   contained tinyint NOT NULL , \n"
            "   seq_align text  NULL , \n"
            "   merged_sa char (1) NOT NULL , \n"
            "   PRIMARY KEY  \n"
            "   ( \n"
            "       pairId, \n"
            "       overlapNum \n"
            "   ) \n"
            ") \n";

        auto_stmt->ExecuteUpdate( sql );
    }

    // Insert data into the table ...
    {
        sql =
            "INSERT INTO #Overlaps VALUES( \n"
            "1, 1, 0, 25794, 7126, 32916, '--', 1, 21, 7124, 7127, 0, \n"
            "'Seq-align ::= { }', 'n')";

        auto_stmt->ExecuteUpdate( sql );
    }

    // Actual check ...
    {
        sql = "SELECT * FROM #Overlaps";

        auto_stmt->SendSql( sql );
        while( auto_stmt->HasMoreResults() ) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                int col_num = -2;

//                 switch ( rs->GetResultType() ) {
//                 case eDB_RowResult:
//                     col_num = rs->GetColumnNo();
//                     break;
//                 case eDB_ParamResult:
//                     col_num = rs->GetColumnNo();
//                     break;
//                 case eDB_StatusResult:
//                     col_num = rs->GetColumnNo();
//                     break;
//                 case eDB_ComputeResult:
//                 case eDB_CursorResult:
//                     col_num = rs->GetColumnNo();
//                     break;
//                 }

                col_num = rs->GetTotalColumns();
                BOOST_CHECK_EQUAL( 14, col_num );
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void
BulkAddRow(const auto_ptr<IBulkInsert>& bi, const CVariant& col)
{
    string msg(8000, 'A');
    CVariant col2(col);

    bi->Bind(2, &col2);
    col2 = msg;
    bi->AddRow();
}

void
CDBAPIUnitTest::DumpResults(IStatement* const stmt)
{
    while ( stmt->HasMoreResults() ) {
        if ( stmt->HasRows() ) {
            auto_ptr<IResultSet> rs( stmt->GetResultSet() );
        }
    }
}

void
CDBAPIUnitTest::Test_Bulk_Overflow(void)
{
    string sql;
    enum {column_size = 32, data_size = 64};

    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    // Initialize ...
    {
        sql =
            "CREATE TABLE #test_bulk_overflow ( \n"
            "   vc32_field VARCHAR(32) \n"
            ") \n";

        auto_stmt->ExecuteUpdate( sql );
    }

    // Insert data ...
    {
        bool exception_catched = false;
        auto_ptr<IBulkInsert> bi(
            m_Conn->GetBulkInsert("#test_bulk_overflow", 1)
            );

        CVariant col1(eDB_VarChar, data_size);

        bi->Bind(1, &col1);

        col1 = string(data_size, 'O');

        // Either AddRow() or Complete() should throw an exception.
        try
        {
            bi->AddRow();

        } catch(const CDB_Exception&)
        {
            exception_catched = true;
        }

        try
        {
            bi->Complete();
        } catch(const CDB_Exception&)
        {
            if ( exception_catched ) {
                throw;
            } else {
                exception_catched = true;
            }
        }

        if ( !exception_catched ) {
            BOOST_FAIL("Exception CDB_ClientEx expected.");
        }
    }

    // Retrieve data ...
//     {
//         sql = "SELECT * FROM #test_bulk_overflow";
//
//         auto_stmt->SendSql( sql );
//         BOOST_CHECK( auto_stmt->HasMoreResults() );
//         BOOST_CHECK( !auto_stmt->HasRows() );
//         auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
//
//         BOOST_CHECK( rs.get() );
//         BOOST_CHECK( rs->Next() );
//
//         const CVariant& value = rs->GetVariant(1);
//
//         BOOST_CHECK( !value.IsNull() );
//
//         string str_value = value.GetString();
//         BOOST_CHECK_EQUAL( string::size_type(column_size), str_value.size() );
//     }
}

void
CDBAPIUnitTest::Test_Bulk_Writing(void)
{
    string sql;

    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );


    // VARBINARY ...
    {
        enum { num_of_tests = 10 };
        const char char_val('2');

        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM #bin_bulk_insert_table" );

        // Insert data ...
        {
            auto_ptr<IBulkInsert> bi(
                m_Conn->GetBulkInsert("#bin_bulk_insert_table", 2)
                );

            CVariant col1(eDB_Int);
            CVariant col2(eDB_LongBinary, m_max_varchar_size);

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);

            for(int i = 0; i < num_of_tests; ++i ) {
                int int_value = m_max_varchar_size / num_of_tests * i;
                string str_val(int_value , char_val);

                col1 = int_value;
                col2 = CVariant::LongBinary(m_max_varchar_size,
                                            str_val.c_str(),
                                            str_val.size()
                                            );
                bi->AddRow();
            }
            bi->Complete();
        }

        // Retrieve data ...
        {
            auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

            sql  = " SELECT id, vb8000_field FROM #bin_bulk_insert_table";
            sql += " ORDER BY id";

            auto_stmt->SendSql( sql );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            for(int i = 0; i < num_of_tests; ++i ) {
                BOOST_CHECK( rs->Next() );

                int int_value = m_max_varchar_size / num_of_tests * i;
                Int4 id = rs->GetVariant(1).GetInt4();
                string vb8000_value = rs->GetVariant(2).GetString();

                BOOST_CHECK_EQUAL( int_value, id );
                BOOST_CHECK_EQUAL(string::size_type(int_value),
                                  vb8000_value.size()
                                  );
            }

            // Dump results ...
            DumpResults( auto_stmt.get() );
        }
    }

    // INT, BIGINT
    {

        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM #bulk_insert_table" );

        // INT collumn ...
        {
            enum { num_of_tests = 8 };

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(
                    m_Conn->GetBulkInsert("#bulk_insert_table", 3)
                    );

                CVariant col1(eDB_Int);
                CVariant col2(eDB_Int);

                bi->Bind(1, &col1);
                bi->Bind(3, &col2);

                for(int i = 0; i < num_of_tests; ++i ) {
                    col1 = i;
                    Int4 value = Int4( 1 ) << (i * 4);
                    col2 = value;
                    bi->AddRow();
                }
                bi->Complete();
            }

            // Retrieve data ...
            {
                sql  = " SELECT int_field FROM #bulk_insert_table";
                sql += " ORDER BY id";

                auto_stmt->SendSql( sql );

                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );

                for(int i = 0; i < num_of_tests; ++i ) {
                    BOOST_CHECK( rs->Next() );
                    Int4 value = rs->GetVariant(1).GetInt4();
                    Int4 expected_value = Int4( 1 ) << (i * 4);
                    BOOST_CHECK_EQUAL( expected_value, value );
                }

                // Dump results ...
                DumpResults( auto_stmt.get() );
            }
        }

        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM #bulk_insert_table" );

        // BIGINT collumn ...
        {
            // There is a problem at least with the ftds driver ...
            // enum { num_of_tests = 16 };
            enum { num_of_tests = 14 };

            // Insert data ...
            {
                auto_ptr<IBulkInsert> bi(
                    m_Conn->GetBulkInsert("#bulk_insert_table", 4)
                    );

                CVariant col1(eDB_Int);
                CVariant col2(eDB_BigInt);

                bi->Bind(1, &col1);
                bi->Bind(4, &col2);

                for(int i = 0; i < num_of_tests; ++i ) {
                    col1 = i;
                    Int8 value = Int8( 1 ) << (i * 4);
                    col2 = value;
                    bi->AddRow();
                }
                bi->Complete();
            }

            // Retrieve data ...
            {
                sql  = " SELECT bigint_field FROM #bulk_insert_table";
                sql += " ORDER BY id";

                auto_stmt->SendSql( sql );

                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );

                for(int i = 0; i < num_of_tests; ++i ) {
                    BOOST_CHECK( rs->Next() );
                    Int8 value = rs->GetVariant(1).GetInt8();
                    Int8 expected_value = Int8( 1 ) << (i * 4);
                    BOOST_CHECK_EQUAL( expected_value, value );
                }

                // Dump results ...
                DumpResults( auto_stmt.get() );
            }
        }
    }

    // Yet another BIGINT test (and more) ...
    {
        auto_ptr<IStatement> stmt( m_Conn->CreateStatement() );

        // Create table ...
        {
            stmt->ExecuteUpdate(
                // "create table #__blki_test ( name char(32) not null, value bigint null )" );
                "create table #__blki_test ( name char(32), value bigint null )"
                );
            stmt->Close();
        }

        // First test ...
        {
            auto_ptr<IBulkInsert> blki(
                m_Conn->CreateBulkInsert("#__blki_test", 2)
                );

            CVariant col1(eDB_Char,32);
            CVariant col2(eDB_BigInt);

            blki->Bind(1, &col1);
            blki->Bind(2, &col2);

            col1 = "Hello-1";
            col2 = Int8( 123 );
            blki->AddRow();

            col1 = "Hello-2";
            col2 = Int8( 1234 );
            blki->AddRow();

            col1 = "Hello-3";
            col2 = Int8( 12345 );
            blki->AddRow();

            col1 = "Hello-4";
            col2 = Int8( 123456 );
            blki->AddRow();

            blki->Complete();
            blki->Close();
        }

        // Second test ...
        // Overflow test.
        // !!! Current behavior is not defined properly and not consistent between drivers.
//         {
//             auto_ptr<IBulkInsert> blki( m_Conn->CreateBulkInsert("#__blki_test", 2) );
//
//             CVariant col1(eDB_Char,64);
//             CVariant col2(eDB_BigInt);
//
//             blki->Bind(1, &col1);
//             blki->Bind(2, &col2);
//
//             string name(8000, 'A');
//             col1 = name;
//             col2 = Int8( 123 );
//             blki->AddRow();
//         }
    }

    // VARCHAR ...
    {
        int num_of_tests;

        if ( m_args.GetServerType() == CTestArguments::eMsSql ) {
            num_of_tests = 7;
        } else {
            // Sybase
            num_of_tests = 3;
        }

        // Clean table ...
        auto_stmt->ExecuteUpdate( "DELETE FROM #bulk_insert_table" );

        // Insert data ...
        {
            auto_ptr<IBulkInsert> bi(
                m_Conn->GetBulkInsert("#bulk_insert_table", 2)
                );

            CVariant col1(eDB_Int);

            bi->Bind(1, &col1);

            for(int i = 0; i < num_of_tests; ++i ) {
                col1 = i;
                switch (i) {
                case 0:
                    BulkAddRow(bi, CVariant(eDB_VarChar));
                    break;
                case 1:
                    BulkAddRow(bi, CVariant(eDB_LongChar, m_max_varchar_size));
                    break;
                case 2:
                    BulkAddRow(bi, CVariant(eDB_LongChar, 1024));
                    break;
                case 3:
                    BulkAddRow(bi, CVariant(eDB_LongChar, 4112));
                    break;
                case 4:
                    BulkAddRow(bi, CVariant(eDB_LongChar, 4113));
                    break;
                case 5:
                    BulkAddRow(bi, CVariant(eDB_LongChar, 4138));
                    break;
                case 6:
                    BulkAddRow(bi, CVariant(eDB_LongChar, 4139));
                    break;
                };

            }
            bi->Complete();
        }

        // Retrieve data ...
        {
            sql  = " SELECT id, vc8000_field FROM #bulk_insert_table";
            sql += " ORDER BY id";

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    // Retrieve results, if any
                    while( rs->Next() ) {
                        Int4 i = rs->GetVariant(1).GetInt4();
                        string col1 = rs->GetVariant(2).GetString();

                        switch (i) {
                        case 0:
                            BOOST_CHECK_EQUAL(col1.size(),
                                              string::size_type(255)
                                              );
                            break;
                        case 1:
                            BOOST_CHECK_EQUAL(
                                col1.size(),
                                string::size_type(m_max_varchar_size)
                                );
                            break;
                        case 2:
                            BOOST_CHECK_EQUAL(col1.size(),
                                              string::size_type(1024)
                                              );
                            break;
                        case 3:
                            BOOST_CHECK_EQUAL(col1.size(),
                                              string::size_type(4112)
                                              );
                            break;
                        case 4:
                            BOOST_CHECK_EQUAL(col1.size(),
                                              string::size_type(4113)
                                              );
                            break;
                        case 5:
                            BOOST_CHECK_EQUAL(col1.size(),
                                              string::size_type(4138)
                                              );
                            break;
                        case 6:
                            BOOST_CHECK_EQUAL(col1.size(),
                                              string::size_type(4139)
                                              );
                            break;
                        };
                    }
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Variant2(void)
{
    const long rec_num = 3;
    const size_t size_step = 10;
    string sql;

    // Initialize a test table ...
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Drop all records ...
        sql  = " DELETE FROM " + GetTableName();
        auto_stmt->ExecuteUpdate(sql);

        // Insert new vc1000_field records ...
        sql  = " INSERT INTO " + GetTableName();
        sql += "(int_field, vc1000_field) VALUES(@id, @val) \n";

        string::size_type str_size(10);
        char char_val('1');
        for (long i = 0; i < rec_num; ++i) {
            string str_val(str_size, char_val);
            str_size *= size_step;
            char_val += 1;

            auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
            // !!! A line below will cause the ftds64_ctlib to crash ....
//             auto_stmt->SetParam( CVariant::LongChar(str_val.c_str(), str_val.size() ), "@val" );
            auto_stmt->SetParam( CVariant(str_val), "@val" );
            // Execute a statement with parameters ...
            auto_stmt->ExecuteUpdate( sql );
        }
    }

    // Test VarChar ...
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        sql  = "SELECT vc1000_field FROM " + GetTableName();
        sql += " ORDER BY int_field";

        string::size_type str_size(10);
        auto_stmt->SendSql( sql );
        while( auto_stmt->HasMoreResults() ) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                // Retrieve results, if any
                while( rs->Next() ) {
                    BOOST_CHECK_EQUAL(false, rs->GetVariant(1).IsNull());
                    string col1 = rs->GetVariant(1).GetString();
                    BOOST_CHECK(col1.size() == str_size || col1.size() == 255);
                    str_size *= size_step;
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_CDB_Exception(void)
{
    static const CDiagCompileInfo kBlankCompileInfo;
    const string message("Very dangerous message");
    const int msgnumber = 67890;

    {
        const string server_name("server_name");
        const string user_name("user_name");
        const int severity = 12345;

        CDB_Exception ex(
            kBlankCompileInfo,
            NULL,
            CDB_Exception::eMulti,
            message,
            eDiag_Trace,
            msgnumber);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(severity);

        BOOST_CHECK_EQUAL(server_name, ex.GetServerName());
        BOOST_CHECK_EQUAL(user_name, ex.GetUserName());
        BOOST_CHECK_EQUAL(severity, ex.GetSybaseSeverity());

        BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
        BOOST_CHECK_EQUAL(message, ex.GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Trace, ex.GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eMulti, ex.Type());

        //
        auto_ptr<CDB_Exception> auto_ex(ex.Clone());

        BOOST_CHECK_EQUAL(server_name, auto_ex->GetServerName());
        BOOST_CHECK_EQUAL(user_name, auto_ex->GetUserName());
        BOOST_CHECK_EQUAL(severity, auto_ex->GetSybaseSeverity());

        BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
        BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Trace, auto_ex->GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eMulti, auto_ex->Type());
    }

    {
        CDB_DSEx ex(
            kBlankCompileInfo,
            NULL,
            message,
            eDiag_Fatal,
            msgnumber);

        BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
        BOOST_CHECK_EQUAL(message, ex.GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Fatal, ex.GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eDS, ex.Type());

        //
        auto_ptr<CDB_Exception> auto_ex(ex.Clone());

        BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
        BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Fatal, auto_ex->GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eDS, auto_ex->Type());
    }

    {
        const string proc_name("proc_name");
        const int proc_line = 12345;

        CDB_RPCEx ex(
            kBlankCompileInfo,
            NULL,
            message,
            eDiag_Critical,
            msgnumber,
            proc_name,
            proc_line);

        BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
        BOOST_CHECK_EQUAL(message, ex.GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Critical, ex.GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eRPC, ex.Type());
        BOOST_CHECK_EQUAL(proc_name, ex.ProcName());
        BOOST_CHECK_EQUAL(proc_line, ex.ProcLine());

        //
        auto_ptr<CDB_Exception> auto_ex(ex.Clone());

        BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
        BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Critical, auto_ex->GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eRPC, auto_ex->Type());
    }

    {
        const string sql_state("sql_state");
        const int batch_line = 12345;

        CDB_SQLEx ex(
            kBlankCompileInfo,
            NULL,
            message,
            eDiag_Error,
            msgnumber,
            sql_state,
            batch_line);

        BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
        BOOST_CHECK_EQUAL(message, ex.GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Error, ex.GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eSQL, ex.GetErrCode());
        BOOST_CHECK_EQUAL(CDB_Exception::eSQL, ex.Type());
        BOOST_CHECK_EQUAL(sql_state, ex.SqlState());
        BOOST_CHECK_EQUAL(batch_line, ex.BatchLine());

        //
        auto_ptr<CDB_Exception> auto_ex(ex.Clone());

        BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
        BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Error, auto_ex->GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eSQL, auto_ex->Type());
    }

    {
        CDB_DeadlockEx ex(
            kBlankCompileInfo,
            NULL,
            message);

        BOOST_CHECK_EQUAL(message, ex.GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Error, ex.GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eDeadlock, ex.Type());

        //
        auto_ptr<CDB_Exception> auto_ex(ex.Clone());

        BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Error, auto_ex->GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eDeadlock, auto_ex->Type());
    }

    {
        CDB_TimeoutEx ex(
            kBlankCompileInfo,
            NULL,
            message,
            msgnumber);

        BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
        BOOST_CHECK_EQUAL(message, ex.GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Error, ex.GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eTimeout, ex.Type());

        //
        auto_ptr<CDB_Exception> auto_ex(ex.Clone());

        BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
        BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Error, auto_ex->GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eTimeout, auto_ex->Type());
    }

    {
        CDB_ClientEx ex(
            kBlankCompileInfo,
            NULL,
            message,
            eDiag_Warning,
            msgnumber);

        BOOST_CHECK_EQUAL(msgnumber, ex.GetDBErrCode());
        BOOST_CHECK_EQUAL(message, ex.GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Warning, ex.GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eClient, ex.Type());

        //
        auto_ptr<CDB_Exception> auto_ex(ex.Clone());

        BOOST_CHECK_EQUAL(msgnumber, auto_ex->GetDBErrCode());
        BOOST_CHECK_EQUAL(message, auto_ex->GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Warning, auto_ex->GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eClient, auto_ex->Type());
    }

    {
        CDB_MultiEx ex(
            kBlankCompileInfo,
            NULL);

        BOOST_CHECK_EQUAL(0, ex.GetDBErrCode());
        BOOST_CHECK_EQUAL(eDiag_Info, ex.GetSeverity());
        BOOST_CHECK_EQUAL(kEmptyStr, ex.GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Info, ex.GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eMulti, ex.Type());

        //
        auto_ptr<CDB_Exception> auto_ex(ex.Clone());

        BOOST_CHECK_EQUAL(0, auto_ex->GetDBErrCode());
        BOOST_CHECK_EQUAL(eDiag_Info, auto_ex->GetSeverity());
        BOOST_CHECK_EQUAL(kEmptyStr, auto_ex->GetMsg());
        BOOST_CHECK_EQUAL(eDiag_Info, auto_ex->GetSeverity());
        BOOST_CHECK_EQUAL(CDB_Exception::eMulti, auto_ex->Type());
    }

}


///////////////////////////////////////////////////////////////////////////////
int
CDBAPIUnitTest::GetNumOfRecords(const auto_ptr<IStatement>& auto_stmt,
                                const string& table_name)
{
    DumpResults(auto_stmt.get());
    auto_stmt->ClearParamList();
    auto_stmt->SendSql( "select count(*) FROM " + table_name );
    BOOST_CHECK( auto_stmt->HasMoreResults() );
    BOOST_CHECK( auto_stmt->HasRows() );
    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
    BOOST_CHECK( rs->Next() );
    int cur_rec_num = rs->GetVariant(1).GetInt4();
    return cur_rec_num;
}

int
CDBAPIUnitTest::GetNumOfRecords(const auto_ptr<ICallableStatement>& auto_stmt)
{
    int rec_num = 0;

    BOOST_CHECK(auto_stmt->HasMoreResults());
    BOOST_CHECK(auto_stmt->HasRows());
    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

    if (rs.get() != NULL) {

        while (rs->Next()) {
            ++rec_num;
        }
    }

    DumpResults(auto_stmt.get());

    return rec_num;
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Cursor(void)
{
    const long rec_num = 2;
    string sql;

    // Initialize a test table ...
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Drop all records ...
        sql  = " DELETE FROM " + GetTableName();
        auto_stmt->ExecuteUpdate(sql);

        // Insert new LOB records ...
        sql  = " INSERT INTO " + GetTableName() +
            "(int_field, text_field) VALUES(@id, '') \n";

        // CVariant variant(eDB_Text);
        // variant.Append(" ", 1);

        for (long i = 0; i < rec_num; ++i) {
            auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
            // Execute a statement with parameters ...
            auto_stmt->ExecuteUpdate( sql );
        }
        // Check record number ...
        BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
    }

    // Test CLOB field update ...
    // It doesn't work right now.
//     {
//         const char* clob = "abc";
//
//         sql = "select text_field from " + GetTableName() + " for update of text_field \n";
//         auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test01", sql));
//
//         {
//             // blobRs should be destroyed before auto_cursor ...
//             auto_ptr<IResultSet> blobRs(auto_cursor->Open());
// //             while(blobRs->Next()) {
// //                 ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
// //                 out.write(clob, sizeof(clob) - 1);
// //                 out.flush();
// //             }
//
//             if (blobRs->Next()) {
//                 try {
//                     ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
//                 } catch(...)
//                 {
//                 }
// //                 out.write(clob, sizeof(clob) - 1);
// //                 out.flush();
//             } else {
//                 BOOST_FAIL( msg_record_expected );
//             }
//
//             if (blobRs->Next()) {
// //                 ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
// //                 out.write(clob, sizeof(clob) - 1);
// //                 out.flush();
//             } else {
//                 BOOST_FAIL( msg_record_expected );
//             }
//         }
//
//         // Check record number ...
//         {
//             auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
//             BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
//         }
//
//         // Another cursor ...
//         sql  = " select text_field from " + GetTableName();
//         sql += " where int_field = 1 for update of text_field";
//
//         auto_cursor.reset(m_Conn->GetCursor("test02", sql));
//         {
//             // blobRs should be destroyed before auto_cursor ...
//             auto_ptr<IResultSet> blobRs(auto_cursor->Open());
//             if ( !blobRs->Next() ) {
//                 BOOST_FAIL( msg_record_expected );
//             }
//             ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
//             out.write(clob, sizeof(clob) - 1);
//             out.flush();
//         }
//     }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Cursor2(void)
{
    const long rec_num = 2;
    string sql;

    // Initialize a test table ...
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Drop all records ...
        sql  = " DELETE FROM " + GetTableName();
        auto_stmt->ExecuteUpdate(sql);

        // Insert new LOB records ...
        sql  = " INSERT INTO " + GetTableName() +
            "(int_field, text_field) VALUES(@id, '') \n";

        // CVariant variant(eDB_Text);
        // variant.Append(" ", 1);

        for (long i = 0; i < rec_num; ++i) {
            auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
            // Execute a statement with parameters ...
            auto_stmt->ExecuteUpdate( sql );
        }
        // Check record number ...
        BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
    }

    // Test CLOB field update ...
    // It doesn't work right now.
//     {
//         const char* clob = "abc";
//
//         sql = "select text_field from " + GetTableName() + " for update of text_field \n";
//         auto_ptr<ICursor> auto_cursor(m_Conn->GetCursor("test01", sql));
//
//         {
//             // blobRs should be destroyed before auto_cursor ...
//             auto_ptr<IResultSet> blobRs(auto_cursor->Open());
// //             while(blobRs->Next()) {
// //                 ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
// //                 out.write(clob, sizeof(clob) - 1);
// //                 out.flush();
// //             }
//
//             if (blobRs->Next()) {
//                 try {
//                     ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
//                 } catch(...)
//                 {
//                 }
// //                 out.write(clob, sizeof(clob) - 1);
// //                 out.flush();
//             } else {
//                 BOOST_FAIL( msg_record_expected );
//             }
//
//             if (blobRs->Next()) {
// //                 ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
// //                 out.write(clob, sizeof(clob) - 1);
// //                 out.flush();
//             } else {
//                 BOOST_FAIL( msg_record_expected );
//             }
//         }
//
//         // Check record number ...
//         {
//             auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
//             BOOST_CHECK_EQUAL(rec_num, GetNumOfRecords(auto_stmt, GetTableName()));
//         }
//
//         // Another cursor ...
//         sql  = " select text_field from " + GetTableName();
//         sql += " where int_field = 1 for update of text_field";
//
//         auto_cursor.reset(m_Conn->GetCursor("test02", sql));
//         {
//             // blobRs should be destroyed before auto_cursor ...
//             auto_ptr<IResultSet> blobRs(auto_cursor->Open());
//             if ( !blobRs->Next() ) {
//                 BOOST_FAIL( msg_record_expected );
//             }
//             ostream& out = auto_cursor->GetBlobOStream(1, sizeof(clob) - 1, eDisableLog);
//             out.write(clob, sizeof(clob) - 1);
//             out.flush();
//         }
//     }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_SelectStmt(void)
{
    // Scenario:
    // 1) Select recordset with just one record
    // 2) Retrive only one record.
    // 3) Select another recordset with just one record
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
        auto_ptr<IResultSet> rs;

        // 1) Select recordset with just one record
        rs.reset( auto_stmt->ExecuteQuery( "select qq = 57 + 33" ) );
        BOOST_CHECK( rs.get() != NULL );

        // 2) Retrive a record.
        BOOST_CHECK( rs->Next() );
        BOOST_CHECK( !rs->Next() );

        // 3) Select another recordset with just one record
        rs.reset( auto_stmt->ExecuteQuery( "select qq = 57.55 + 0.0033" ) );
        BOOST_CHECK( rs.get() != NULL );
        BOOST_CHECK( rs->Next() );
        BOOST_CHECK( !rs->Next() );
    }

    // Same as before but uses two differenr connections ...
    if (false) {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
        auto_ptr<IResultSet> rs;

        // 1) Select recordset with just one record
        rs.reset( auto_stmt->ExecuteQuery( "select qq = 57 + 33" ) );
        BOOST_CHECK( rs.get() != NULL );

        // 2) Retrive only one record.
        BOOST_CHECK( rs->Next() );
        BOOST_CHECK( !rs->Next() );

        // 3) Select another recordset with just one record
        auto_ptr<IStatement> auto_stmt2( m_Conn->CreateStatement() );
        rs.reset( auto_stmt2->ExecuteQuery( "select qq = 57.55 + 0.0033" ) );
        BOOST_CHECK( rs.get() != NULL );
        BOOST_CHECK( rs->Next() );
        BOOST_CHECK( !rs->Next() );
    }

    // Check column name ...
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        auto_ptr<IResultSet> rs(
            auto_stmt->ExecuteQuery( "select @@version as oops" )
            );
        BOOST_CHECK( rs.get() != NULL );
        BOOST_CHECK( rs->Next() );
        auto_ptr<const IResultSetMetaData> col_metadata(rs->GetMetaData());
        BOOST_CHECK_EQUAL( string("oops"), col_metadata->GetName(1) );
    }

    // Check resultset ...
    {
        int num = 0;
        string sql = "select user_id(), convert(varchar(64), user_name()), "
            "convert(nvarchar(64), user_name())";

        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // 1) Select recordset with just one record
        auto_ptr<IResultSet> rs( auto_stmt->ExecuteQuery( sql ) );
        BOOST_CHECK( rs.get() != NULL );

        while (rs->Next()) {
            BOOST_CHECK(rs->GetVariant(1).GetInt4() > 0);
            BOOST_CHECK(rs->GetVariant(2).GetString().size() > 0);
            BOOST_CHECK(rs->GetVariant(3).GetString().size() > 0);
            ++num;
        }

        BOOST_CHECK_EQUAL(num, 1);

        DumpResults(auto_stmt.get());
    }

//     // TMP
//     {
//         auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
//         BOOST_CHECK( conn.get() != NULL );
//
//         conn->SetMode(IConnection::eBulkInsert);
//
//         conn->Connect(
//             "anyone",
//             "allowed",
//             "MSSQL10",
//             "gMapDB"
//             );
//
//         auto_ptr<IStatement> auto_stmt( conn->GetStatement() );
//
//         string sql;
//
//         sql  = "SELECT len(seq_loc) l, seq_loc FROM protein where ProtGi =56964519";
//
//         auto_stmt->SendSql( sql );
//         while( auto_stmt->HasMoreResults() ) {
//             if( auto_stmt->HasRows() ) {
//                 auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
//
//                 // Retrieve results, if any
//                 while( rs->Next() ) {
//                     string col2 = rs->GetVariant(2).GetString();
//                     BOOST_CHECK_EQUAL(col2.size(), 355);
//                 }
//             }
//         }
//     }

//     // TMP
//     {
//         auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
//         BOOST_CHECK( conn.get() != NULL );
//
//         conn->SetMode(IConnection::eBulkInsert);
//
//         conn->Connect(
//             "mvread",
//             "daervm",
//             "MSSQL29",
//             "MapViewHum36"
//             );
//
//         auto_ptr<IStatement> auto_stmt( conn->GetStatement() );
//
//         string sql;
//
//         sql  = "MM_GetData \'model\',\'1\',8335044,8800111,9606";
//
//         auto_stmt->SendSql( sql );
//         while( auto_stmt->HasMoreResults() ) {
//             if( auto_stmt->HasRows() ) {
//                 auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
//
//                 // Retrieve results, if any
//                 while( rs->Next() ) {
//                 }
//             }
//         }
//     }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_SelectStmtXML(void)
{
    // SQL + XML
    {
        string sql;
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
        auto_ptr<IResultSet> rs;

        sql = "select 1 as Tag, null as Parent, 1 as [x!1!id] for xml explicit";
        rs.reset( auto_stmt->ExecuteQuery( sql ) );
        BOOST_CHECK( rs.get() != NULL );

        if ( !rs->Next() ) {
            BOOST_FAIL( msg_record_expected );
        }

        // Same but call Execute instead of ExecuteQuery.
        auto_stmt->SendSql( sql );
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_UserErrorHandler(void)
{
    // Set up an user-defined error handler ..

    // Push message handler into a context ...
    I_DriverContext* drv_context = m_DS->GetDriverContext();

    // Check PushCntxMsgHandler ...
    // PushCntxMsgHandler - Add message handler "h" to process 'context-wide' (not bound
    // to any particular connection) error messages.
    {
        auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());

        auto_ptr<IConnection> local_conn( m_DS->CreateConnection() );
        Connect(local_conn);

        drv_context->PushCntxMsgHandler( drv_err_handler.get() );

        // Connection process should be affected ...
        {
            // Create a new connection ...
            auto_ptr<IConnection> conn( m_DS->CreateConnection() );

            try {
                conn->Connect(
                    "unknown",
                    "invalid",
                    m_args.GetServerName()
                    );
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( drv_err_handler->GetSucceed() );
            }
            catch( const CException& ) {
                BOOST_FAIL( "CException was catched instead of CDB_Exception." );
            }
            catch( ... ) {
                BOOST_FAIL( "... was catched instead of CDB_Exception." );
            }

            drv_err_handler->Init();
        }

        // Current connection should not be affected ...
        {
            try {
                Test_ES_01(*local_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( !drv_err_handler->GetSucceed() );
            }
        }

        // New connection should not be affected ...
        {
            // Create a new connection ...
            auto_ptr<IConnection> conn( m_DS->CreateConnection() );
            Connect(conn);

            // Reinit the errot handler because it can be affected during connection.
            drv_err_handler->Init();

            try {
                Test_ES_01(*conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( !drv_err_handler->GetSucceed() );
            }
        }

        // Push a message handler into a connection ...
        {
            auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

            local_conn->GetCDB_Connection()->PushMsgHandler(msg_handler.get());

            try {
                Test_ES_01(*local_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( msg_handler->GetSucceed() );
            }

            // Remove handler ...
            local_conn->GetCDB_Connection()->PopMsgHandler( msg_handler.get() );
        }

        // New connection should not be affected
        // after pushing a message handler into another connection
        {
            // Create a new connection ...
            auto_ptr<IConnection> conn( m_DS->CreateConnection() );
            Connect(conn);

            // Reinit the errot handler because it can be affected during connection.
            drv_err_handler->Init();

            try {
                Test_ES_01(*conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( !drv_err_handler->GetSucceed() );
            }
        }

        // Remove all inserted handlers ...
        drv_context->PopCntxMsgHandler( drv_err_handler.get() );
    }


    // Check PushDefConnMsgHandler ...
    // PushDefConnMsgHandler - Add `per-connection' err.message handler "h" to the stack of default
    // handlers which are inherited by all newly created connections.
    {
        auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());


        auto_ptr<IConnection> local_conn( m_DS->CreateConnection() );
        Connect(local_conn);

        drv_context->PushDefConnMsgHandler( drv_err_handler.get() );

        // Current connection should not be affected ...
        {
            try {
                Test_ES_01(*local_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( !drv_err_handler->GetSucceed() );
            }
        }

        // Push a message handler into a connection ...
        // This is supposed to be okay.
        {
            auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

            local_conn->GetCDB_Connection()->PushMsgHandler(msg_handler.get());

            try {
                Test_ES_01(*local_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( msg_handler->GetSucceed() );
            }

            // Remove handler ...
            local_conn->GetCDB_Connection()->PopMsgHandler(msg_handler.get());
        }


        ////////////////////////////////////////////////////////////////////////
        // Create a new connection.
        auto_ptr<IConnection> new_conn( m_DS->CreateConnection() );
        Connect(new_conn);

        // New connection should be affected ...
        {
            try {
                Test_ES_01(*new_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( drv_err_handler->GetSucceed() );
            }
        }

        // Push a message handler into a connection ...
        // This is supposed to be okay.
        {
            auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

            new_conn->GetCDB_Connection()->PushMsgHandler( msg_handler.get() );

            try {
                Test_ES_01(*new_conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( msg_handler->GetSucceed() );
            }

            // Remove handler ...
            new_conn->GetCDB_Connection()->PopMsgHandler( msg_handler.get() );
        }

        // New connection should be affected
        // after pushing a message handler into another connection
        {
            // Create a new connection ...
            auto_ptr<IConnection> conn( m_DS->CreateConnection() );
            Connect(conn);

            try {
                Test_ES_01(*conn);
            }
            catch( const CDB_Exception& ) {
                // Ignore it
                BOOST_CHECK( drv_err_handler->GetSucceed() );
            }
        }

        // Remove handlers ...
        drv_context->PopDefConnMsgHandler( drv_err_handler.get() );
    }

    // SetLogStream ...
    {
        {
            IConnection* conn = NULL;

            // Enable multiexception ...
            m_DS->SetLogStream(0);

            try {
                // Create a new connection ...
                conn = m_DS->CreateConnection();
            } catch(...)
            {
                delete conn;
            }

            m_DS->SetLogStream(&cerr);
        }

        {
            IConnection* conn = NULL;

            // Enable multiexception ...
            m_DS->SetLogStream(0);

            try {
                // Create a new connection ...
                conn = m_DS->CreateConnection();

                m_DS->SetLogStream(&cerr);
            } catch(...)
            {
                delete conn;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Procedure(void)
{
    // Test a regular IStatement with "exec"
    // Parameters are not allowed with this construction.
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Execute it first time ...
        // auto_stmt->SendSql( "exec sp_databases" );
        auto_stmt->SendSql( "SELECT name FROM sysobjects" );
        while( auto_stmt->HasMoreResults() ) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                switch ( rs->GetResultType() ) {
                case eDB_RowResult:
                    while( rs->Next() ) {
                        // int col1 = rs->GetVariant(1).GetInt4();
                    }
                    break;
                case eDB_ParamResult:
                    while( rs->Next() ) {
                        // int col1 = rs->GetVariant(1).GetInt4();
                    }
                    break;
                case eDB_StatusResult:
                    while( rs->Next() ) {
                        int status = rs->GetVariant(1).GetInt4();
                        status = status;
                    }
                    break;
                case eDB_ComputeResult:
                case eDB_CursorResult:
                    break;
                }
            }
        }

        // Execute it second time ...
        // auto_stmt->SendSql( "exec sp_databases" );
        auto_stmt->SendSql( "SELECT name FROM sysobjects" );
        while( auto_stmt->HasMoreResults() ) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                switch ( rs->GetResultType() ) {
                case eDB_RowResult:
                    while( rs->Next() ) {
                        // int col1 = rs->GetVariant(1).GetInt4();
                    }
                    break;
                case eDB_ParamResult:
                    while( rs->Next() ) {
                        // int col1 = rs->GetVariant(1).GetInt4();
                    }
                    break;
                case eDB_StatusResult:
                    while( rs->Next() ) {
                        int status = rs->GetVariant(1).GetInt4();
                        status = status;
                    }
                    break;
                case eDB_ComputeResult:
                case eDB_CursorResult:
                    break;
                }
            }
        }

        // Same as before but do not retrieve data ...
        auto_stmt->SendSql( "exec sp_databases" );
        auto_stmt->SendSql( "exec sp_databases" );
    }

    // Test ICallableStatement
    // No parameters at this time.
    {
        // Execute it first time ...
        auto_ptr<ICallableStatement> auto_stmt(
            m_Conn->GetCallableStatement("sp_databases")
            );
        auto_stmt->Execute();
        while(auto_stmt->HasMoreResults()) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                switch( rs->GetResultType() ) {
                case eDB_RowResult:
                    while(rs->Next()) {
                        // retrieve row results
                    }
                    break;
                case eDB_ParamResult:
                    while(rs->Next()) {
                        // Retrieve parameter row
                    }
                    break;
                default:
                    break;
                }
            }
        }
        // Get status
        int status = auto_stmt->GetReturnStatus();


        // Execute it second time ...
        auto_stmt.reset( m_Conn->GetCallableStatement("sp_databases") );
        auto_stmt->Execute();
        while(auto_stmt->HasMoreResults()) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                switch( rs->GetResultType() ) {
                case eDB_RowResult:
                    while(rs->Next()) {
                        // retrieve row results
                    }
                    break;
                case eDB_ParamResult:
                    while(rs->Next()) {
                        // Retrieve parameter row
                    }
                    break;
                default:
                    break;
                }
            }
        }
        // Get status
        status = auto_stmt->GetReturnStatus();


        // Same as before but do not retrieve data ...
        auto_stmt.reset( m_Conn->GetCallableStatement("sp_databases") );
        auto_stmt->Execute();
        auto_stmt.reset( m_Conn->GetCallableStatement("sp_databases") );
        auto_stmt->Execute();
    }

    // Temporary test ...
    // !!! This is a bug ...
    if (false) {
        auto_ptr<IConnection> conn( m_DS->CreateConnection( CONN_OWNERSHIP ) );
        BOOST_CHECK( conn.get() != NULL );

        conn->Connect(
            "anyone",
            "allowed",
            "PUBSEQ_OS_LXA",
            ""
            );

        auto_ptr<ICallableStatement> auto_stmt(
            conn->GetCallableStatement("id_seqid4gi")
            );
        auto_stmt->SetParam( CVariant(1), "@gi" );
        auto_stmt->Execute();
        while(auto_stmt->HasMoreResults()) {
            if( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                switch( rs->GetResultType() ) {
                case eDB_RowResult:
                    while(rs->Next()) {
                        // retrieve row results
                    }
                    break;
                case eDB_ParamResult:
                    _ASSERT(false);
                    while(rs->Next()) {
                        // Retrieve parameter row
                    }
                    break;
                default:
                    break;
                }
            }
        }
        // Get status
        int status = auto_stmt->GetReturnStatus();
        status = status; // Get rid of warnings.
    }


    // Test returned recordset ...
    {
        {
            int num = 0;
            // Execute it first time ...
            auto_ptr<ICallableStatement> auto_stmt(
                m_Conn->GetCallableStatement("sp_databases", 3)
                );

            auto_stmt->Execute();

            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);

            while (rs->Next()) {
                BOOST_CHECK(rs->GetVariant(1).GetString().size() > 0);
                BOOST_CHECK(rs->GetVariant(2).GetInt4() > 0);
                BOOST_CHECK_EQUAL(rs->GetVariant(3).IsNull(), true);
                ++num;
            }

            BOOST_CHECK(num > 0);

            DumpResults(auto_stmt.get());
        }

        {
            int num = 0;
            auto_ptr<ICallableStatement> auto_stmt(
                m_Conn->GetCallableStatement("sp_server_info", 3)
                );

            auto_stmt->Execute();

            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);

            while (rs->Next()) {
                BOOST_CHECK(rs->GetVariant(1).GetInt4() > 0);
                BOOST_CHECK(rs->GetVariant(2).GetString().size() > 0);
                BOOST_CHECK(rs->GetVariant(3).GetString().size() > 0);
                ++num;
            }

            BOOST_CHECK(num > 0);

            DumpResults(auto_stmt.get());
        }
    }

    // Test output parameters ...
    if (false) {
        CVariant param(eDB_Int);

        auto_ptr<ICallableStatement> auto_stmt(
            m_Conn->GetCallableStatement("DBAPI_Sample..TestProc4", 1)
            );
//         auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );
        auto_stmt->SetParam( param, "@test_out" );

        auto_stmt->Execute();
//         auto_stmt->SendSql( "exec DBAPI_Sample..TestProc4 @test_out output" );

         while(auto_stmt->HasMoreResults()) {
             if( auto_stmt->HasRows() ) {
                 auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                 switch( rs->GetResultType() ) {
                 case eDB_RowResult:
                     while(rs->Next()) {
                         // retrieve row results
                     }
                     break;
                 case eDB_ParamResult:
                     while(rs->Next()) {
                         // Retrieve parameter row
                     }
                     break;
                 default:
                     break;
                 }
             }
         }

//         BOOST_CHECK(auto_stmt->HasMoreResults());
//         BOOST_CHECK(auto_stmt->HasRows());
//         auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
//         BOOST_CHECK(rs.get() != NULL);
//
//         while (rs->Next()) {
//             BOOST_CHECK(rs->GetVariant(1).GetString().size() > 0);
//             BOOST_CHECK(rs->GetVariant(2).GetInt4() > 0);
//             BOOST_CHECK_EQUAL(rs->GetVariant(3).IsNull(), true);
//             ++num;
//         }
//
//         BOOST_CHECK(num > 0);

        DumpResults(auto_stmt.get());
    }
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Exception_Safety(void)
{
    // Very first test ...
    // Try to catch a base class ...
    BOOST_CHECK_THROW( Test_ES_01(*m_Conn), CDB_Exception );
}

///////////////////////////////////////////////////////////////////////////////
// Throw CDB_Exception ...
void
CDBAPIUnitTest::Test_ES_01(IConnection& conn)
{
    auto_ptr<IStatement> auto_stmt( conn.GetStatement() );

    auto_stmt->SendSql( "select name from wrong table" );
    while( auto_stmt->HasMoreResults() ) {
        if( auto_stmt->HasRows() ) {
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

            while( rs->Next() ) {
                // int col1 = rs->GetVariant(1).GetInt4();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_StatementParameters(void)
{
    // Very first test ...
    {
        string sql;
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        {
            sql  = " INSERT INTO " + GetTableName() +
                "(int_field) VALUES( @value ) \n";

            auto_stmt->SetParam( CVariant(0), "@value" );
            // Execute a statement with parameters ...
            auto_stmt->ExecuteUpdate( sql );

            auto_stmt->SetParam( CVariant(1), "@value" );
            // Execute a statement with parameters ...
            auto_stmt->ExecuteUpdate( sql );

            // !!! Do not forget to clear a parameter list ....
            // Workaround for the ctlib driver ...
            auto_stmt->ClearParamList();
        }


        {
            sql  = " SELECT int_field FROM " + GetTableName() +
                " ORDER BY int_field";
            // Execute a statement without parameters ...
            auto_stmt->ExecuteUpdate( sql );
        }

        // Get number of records ...
        {
            auto_stmt->SendSql( "SELECT COUNT(*) FROM " + GetTableName() );
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 2 );
            DumpResults(auto_stmt.get());
        }

        // Read first inserted value back ...
        {
            auto_stmt->SetParam( CVariant(0), "@value" );
            auto_stmt->SendSql( " SELECT int_field FROM " + GetTableName() +
                                " WHERE int_field = @value");
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 0 );
            DumpResults(auto_stmt.get());
            // !!! Do not forget to clear a parameter list ....
            // Workaround for the ctlib driver ...
            auto_stmt->ClearParamList();
        }

        // Read second inserted value back ...
        {
            auto_stmt->SetParam( CVariant(1), "@value" );
            auto_stmt->SendSql( " SELECT int_field FROM " + GetTableName() +
                                " WHERE int_field = @value");
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 1 );
            DumpResults(auto_stmt.get());
            // !!! Do not forget to clear a parameter list ....
            // Workaround for the ctlib driver ...
            auto_stmt->ClearParamList();
        }

        // Clean previously inserted data ...
        {
            auto_stmt->ExecuteUpdate( "DELETE FROM " + GetTableName() );
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_NULL(void)
{
    enum {rec_num = 10};
    string sql;

    // Initialize data (strings are NOT empty) ...
    {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        {
            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, vc1000_field) "
                "VALUES(@int_field, @vc1000_field) \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            // Insert data ...
            for (long ind = 0; ind < rec_num; ++ind) {
                if (ind % 2 == 0) {
                    auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                    auto_stmt->SetParam(CVariant(eDB_VarChar), "@vc1000_field");
                } else {
                    auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                    auto_stmt->SetParam( CVariant(NStr::IntToString(ind)),
                                         "@vc1000_field"
                                         );
                }

                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();
            }

            // Check record number ...
            BOOST_CHECK_EQUAL(int(rec_num),
                              GetNumOfRecords(auto_stmt, GetTableName())
                              );
        }

        {
            // Drop all records ...
            sql  = " DELETE FROM #test_unicode_table";
            auto_stmt->ExecuteUpdate(sql);

            sql  = " INSERT INTO #test_unicode_table"
                "(nvc255_field) VALUES(@nvc255_field)";

            // Insert data ...
            for (long ind = 0; ind < rec_num; ++ind) {
                if (ind % 2 == 0) {
                    auto_stmt->SetParam(CVariant(eDB_VarChar), "@nvc255_field");
                } else {
                    auto_stmt->SetParam( CVariant(NStr::IntToString(ind)),
                                         "@nvc255_field"
                                         );
                }

                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();
            }

            // Check record number ...
            BOOST_CHECK_EQUAL(int(rec_num),
                              GetNumOfRecords(auto_stmt, GetTableName()));
        }
    }

    if (false) {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Drop all records ...
        sql  = " DELETE FROM DBAPI_Sample..dbapi_unit_test";
        auto_stmt->ExecuteUpdate(sql);

        sql  = " INSERT INTO DBAPI_Sample..dbapi_unit_test"
            "(int_field, vc1000_field) "
            "VALUES(@int_field, @vc1000_field) \n";

        // CVariant variant(eDB_Text);
        // variant.Append(" ", 1);

        // Insert data ...
        for (long ind = 0; ind < rec_num; ++ind) {
            if (ind % 2 == 0) {
                auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                auto_stmt->SetParam( CVariant(eDB_VarChar), "@vc1000_field" );
            } else {
                auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                auto_stmt->SetParam( CVariant(NStr::IntToString(ind)),
                                     "@vc1000_field"
                                     );
            }

            // Execute a statement with parameters ...
            auto_stmt->ExecuteUpdate( sql );

            // !!! Do not forget to clear a parameter list ....
            // Workaround for the ctlib driver ...
            auto_stmt->ClearParamList();
        }

        // Check record number ...
        BOOST_CHECK_EQUAL(int(rec_num),
                          GetNumOfRecords(auto_stmt, GetTableName())
                          );
    }

    // Check ...
    if (true) {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        {
            sql = "SELECT int_field, vc1000_field FROM " + GetTableName() +
                " ORDER BY id";

            auto_stmt->SendSql( sql );
            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);

            for (long ind = 0; ind < rec_num; ++ind) {
                BOOST_CHECK(rs->Next());

                const CVariant& int_field = rs->GetVariant(1);
                const CVariant& vc1000_field = rs->GetVariant(2);

                if (ind % 2 == 0) {
                    BOOST_CHECK( !int_field.IsNull() );
                    BOOST_CHECK_EQUAL( int_field.GetInt4(), ind );

                    BOOST_CHECK( vc1000_field.IsNull() );
                } else {
                    BOOST_CHECK( int_field.IsNull() );

                    BOOST_CHECK( !vc1000_field.IsNull() );
                    BOOST_CHECK_EQUAL( vc1000_field.GetString(),
                                       NStr::IntToString(ind) );
                }
            }

            DumpResults(auto_stmt.get());
        }

        {
            sql = "SELECT nvc255_field FROM #test_unicode_table ORDER BY id";

            auto_stmt->SendSql( sql );
            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);

            for (long ind = 0; ind < rec_num; ++ind) {
                BOOST_CHECK(rs->Next());

                const CVariant& nvc255_field = rs->GetVariant(1);

                if (ind % 2 == 0) {
                    BOOST_CHECK( nvc255_field.IsNull() );
                } else {
                    BOOST_CHECK( !nvc255_field.IsNull() );
                    BOOST_CHECK_EQUAL( nvc255_field.GetString(),
                                       NStr::IntToString(ind) );
                }
            }

            DumpResults(auto_stmt.get());
        }
    }

    // Check NULL with stored procedures ...
    {
        {
            auto_ptr<ICallableStatement> auto_stmt(
                m_Conn->GetCallableStatement("sp_server_info", 1)
                );

            // Set parameter to NULL ...
            auto_stmt->SetParam( CVariant(eDB_Int), "@attribute_id" );
            auto_stmt->Execute();

            if (m_args.GetServerType() == CTestArguments::eSybase) {
                BOOST_CHECK_EQUAL( 30, GetNumOfRecords(auto_stmt) );
            } else {
                BOOST_CHECK_EQUAL( 29, GetNumOfRecords(auto_stmt) );
            }

            // !!! Do not forget to clear a parameter list ....
            // Workaround for the ctlib driver ...
            auto_stmt->ClearParamList();

            // Set parameter to 1 ...
            auto_stmt->SetParam( CVariant( Int4(1) ), "@attribute_id" );
            auto_stmt->Execute();

            BOOST_CHECK_EQUAL( 1, GetNumOfRecords(auto_stmt) );
        }

        {
        }
    }


    // Special case: empty strings.
    if (false) {
        auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

        // Initialize data (strings are EMPTY) ...
        {
            // Drop all records ...
            sql  = " DELETE FROM " + GetTableName();
            auto_stmt->ExecuteUpdate(sql);

            sql  = " INSERT INTO " + GetTableName() +
                "(int_field, vc1000_field) "
                "VALUES(@int_field, @vc1000_field) \n";

            // CVariant variant(eDB_Text);
            // variant.Append(" ", 1);

            // Insert data ...
            for (long ind = 0; ind < rec_num; ++ind) {
                if (ind % 2 == 0) {
                    auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                    auto_stmt->SetParam(CVariant(eDB_VarChar), "@vc1000_field");
                } else {
                    auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                    auto_stmt->SetParam( CVariant(string()), "@vc1000_field" );
                }

                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();
            }

            // Check record number ...
            BOOST_CHECK_EQUAL(int(rec_num), GetNumOfRecords(auto_stmt,
                                                            GetTableName()));
        }

        // Check ...
        {
            sql = "SELECT int_field, vc1000_field FROM " + GetTableName() +
                " ORDER BY id";

            auto_stmt->SendSql( sql );
            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);

            for (long ind = 0; ind < rec_num; ++ind) {
                BOOST_CHECK(rs->Next());

                const CVariant& int_field = rs->GetVariant(1);
                const CVariant& vc1000_field = rs->GetVariant(2);

                if (ind % 2 == 0) {
                    BOOST_CHECK( !int_field.IsNull() );
                    BOOST_CHECK_EQUAL( int_field.GetInt4(), ind );

                    BOOST_CHECK( vc1000_field.IsNull() );
                } else {
                    BOOST_CHECK( int_field.IsNull() );

                    BOOST_CHECK( !vc1000_field.IsNull() );
                    BOOST_CHECK_EQUAL( vc1000_field.GetString(), string() );
                }
            }

            DumpResults(auto_stmt.get());
        }
    }

}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_GetRowCount()
{
    enum {repeat_num = 5};
    for ( int i = 0; i < repeat_num; ++i ) {
        // Shared/Reusable statement
        {
            auto_ptr<IStatement> stmt( m_Conn->GetStatement() );

            CheckGetRowCount( i, eNoTrans, stmt.get() );
            CheckGetRowCount( i, eTransCommit, stmt.get() );
            CheckGetRowCount( i, eTransRollback, stmt.get() );
        }
        // Dedicated statement
        {
            CheckGetRowCount( i, eNoTrans );
            CheckGetRowCount( i, eTransCommit );
            CheckGetRowCount( i, eTransRollback );
        }
    }

    for ( int i = 0; i < repeat_num; ++i ) {
        // Shared/Reusable statement
        {
            auto_ptr<IStatement> stmt( m_Conn->GetStatement() );

            CheckGetRowCount2( i, eNoTrans, stmt.get() );
            CheckGetRowCount2( i, eTransCommit, stmt.get() );
            CheckGetRowCount2( i, eTransRollback, stmt.get() );
        }
        // Dedicated statement
        {
            CheckGetRowCount2( i, eNoTrans );
            CheckGetRowCount2( i, eTransCommit );
            CheckGetRowCount2( i, eTransRollback );
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::CheckGetRowCount(
    int row_count,
    ETransBehavior tb,
    IStatement* stmt
    )
{
    // Transaction ...
    CTestTransaction transaction(*m_Conn, tb);
    string sql;
    sql  = " INSERT INTO " + GetTableName() + "(int_field) VALUES( 1 ) \n";

    // Insert row_count records into the table ...
    for ( int i = 0; i < row_count; ++i ) {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->ExecuteUpdate(sql);
        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows, 1 );
    }

    // Check a SELECT statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check an UPDATE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " UPDATE " + GetTableName() + " SET int_field = 0 ";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName() + " WHERE int_field = 0";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a DELETE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again and again ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( 0, nRows );
    }

}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::CheckGetRowCount2(
    int row_count,
    ETransBehavior tb,
    IStatement* stmt
    )
{
    // Transaction ...
    CTestTransaction transaction(*m_Conn, tb);
    // auto_ptr<IStatement> stmt( m_Conn->CreateStatement() );
    // _ASSERT(curr_stmt->get());
    string sql;
    sql  = " INSERT INTO " + GetTableName() + "(int_field) VALUES( @value ) \n";

    // Insert row_count records into the table ...
    for ( Int4 i = 0; i < row_count; ++i ) {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->SetParam(CVariant(i), "@value");
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows, 1 );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows2, 1 );
    }

    // Workaround for the CTLIB driver ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->ClearParamList();
    }

    // Check a SELECT statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field FROM " + GetTableName() +
            " ORDER BY int_field";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check an UPDATE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " UPDATE " + GetTableName() + " SET int_field = 0 ";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a SELECT statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field FROM " + GetTableName() +
            " WHERE int_field = 0";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a DELETE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a DELETE statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows2 );
    }

    // Check a SELECT statement again and again ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( m_Conn->GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field FROM " + GetTableName() + " ORDER BY int_field";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows2 );
    }

}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Variant(void)
{
    const Uint1 value_Uint1 = 1;
    const Int2 value_Int2 = -2;
    const Int4 value_Int4 = -4;
    const Int8 value_Int8 = -8;
    const float value_float = float(0.4);
    const double value_double = 0.8;
    const bool value_bool = false;
    const string value_string = "test string 0987654321";
    const char* value_char = "test char* 1234567890";
    const char* value_binary = "test binary 1234567890 binary";
    const CTime value_CTime( CTime::eCurrent );

    // Check constructors
    {
        const CVariant variant_Int8( value_Int8 );
        BOOST_CHECK( !variant_Int8.IsNull() );
        BOOST_CHECK( variant_Int8.GetInt8() == value_Int8 );

        const CVariant variant_Int4( value_Int4 );
        BOOST_CHECK( !variant_Int4.IsNull() );
        BOOST_CHECK( variant_Int4.GetInt4() == value_Int4 );

        const CVariant variant_Int2( value_Int2 );
        BOOST_CHECK( !variant_Int2.IsNull() );
        BOOST_CHECK( variant_Int2.GetInt2() == value_Int2 );

        const CVariant variant_Uint1( value_Uint1 );
        BOOST_CHECK( !variant_Uint1.IsNull() );
        BOOST_CHECK( variant_Uint1.GetByte() == value_Uint1 );

        const CVariant variant_float( value_float );
        BOOST_CHECK( !variant_float.IsNull() );
        BOOST_CHECK( variant_float.GetFloat() == value_float );

        const CVariant variant_double( value_double );
        BOOST_CHECK( !variant_double.IsNull() );
        BOOST_CHECK( variant_double.GetDouble() == value_double );

        const CVariant variant_bool( value_bool );
        BOOST_CHECK( !variant_bool.IsNull() );
        BOOST_CHECK( variant_bool.GetBit() == value_bool );

        const CVariant variant_string( value_string );
        BOOST_CHECK( !variant_string.IsNull() );
        BOOST_CHECK( variant_string.GetString() == value_string );

        const CVariant variant_char( value_char );
        BOOST_CHECK( !variant_char.IsNull() );
        BOOST_CHECK( variant_char.GetString() == value_char );

        const CVariant variant_CTimeShort( value_CTime, eShort );
        BOOST_CHECK( !variant_CTimeShort.IsNull() );
        BOOST_CHECK( variant_CTimeShort.GetCTime() == value_CTime );

        const CVariant variant_CTimeLong( value_CTime, eLong );
        BOOST_CHECK( !variant_CTimeLong.IsNull() );
        BOOST_CHECK( variant_CTimeLong.GetCTime() == value_CTime );

//        explicit CVariant(CDB_Object* obj);

    }

    // Check the CVariant(EDB_Type type, size_t size = 0) constructor
    {
//        enum EDB_Type {
//            eDB_Int,
//            eDB_SmallInt,
//            eDB_TinyInt,
//            eDB_BigInt,
//            eDB_VarChar,
//            eDB_Char,
//            eDB_VarBinary,
//            eDB_Binary,
//            eDB_Float,
//            eDB_Double,
//            eDB_DateTime,
//            eDB_SmallDateTime,
//            eDB_Text,
//            eDB_Image,
//            eDB_Bit,
//            eDB_Numeric,

//            eDB_UnsupportedType,
//            eDB_LongChar,
//            eDB_LongBinary
//        };

        {
            CVariant value_variant( eDB_Int );

            BOOST_CHECK_EQUAL( eDB_Int, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_SmallInt );

            BOOST_CHECK_EQUAL( eDB_SmallInt, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_TinyInt );

            BOOST_CHECK_EQUAL( eDB_TinyInt, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_BigInt );

            BOOST_CHECK_EQUAL( eDB_BigInt, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_VarChar );

            BOOST_CHECK_EQUAL( eDB_VarChar, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Char, max_text_size );

            BOOST_CHECK_EQUAL( eDB_Char, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_VarBinary );

            BOOST_CHECK_EQUAL( eDB_VarBinary, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Binary, max_text_size );

            BOOST_CHECK_EQUAL( eDB_Binary, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Float );

            BOOST_CHECK_EQUAL( eDB_Float, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Double );

            BOOST_CHECK_EQUAL( eDB_Double, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_DateTime );

            BOOST_CHECK_EQUAL( eDB_DateTime, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_SmallDateTime );

            BOOST_CHECK_EQUAL( eDB_SmallDateTime, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Text );

            BOOST_CHECK_EQUAL( eDB_Text, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Image );

            BOOST_CHECK_EQUAL( eDB_Image, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Bit );

            BOOST_CHECK_EQUAL( eDB_Bit, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_Numeric );

            BOOST_CHECK_EQUAL( eDB_Numeric, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_LongChar, max_text_size );

            BOOST_CHECK_EQUAL( eDB_LongChar, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        {
            CVariant value_variant( eDB_LongBinary, max_text_size );

            BOOST_CHECK_EQUAL( eDB_LongBinary, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
        if (false) {
            CVariant value_variant( eDB_UnsupportedType );

            BOOST_CHECK_EQUAL( eDB_UnsupportedType, value_variant.GetType() );
            BOOST_CHECK( value_variant.IsNull() );
        }
    }

    // Check the copy-constructor CVariant(const CVariant& v)
    {
        const CVariant variant_Int8 = CVariant(value_Int8);
        BOOST_CHECK( !variant_Int8.IsNull() );
        BOOST_CHECK( variant_Int8.GetInt8() == value_Int8 );

        const CVariant variant_Int4 = CVariant(value_Int4);
        BOOST_CHECK( !variant_Int4.IsNull() );
        BOOST_CHECK( variant_Int4.GetInt4() == value_Int4 );

        const CVariant variant_Int2 = CVariant(value_Int2);
        BOOST_CHECK( !variant_Int2.IsNull() );
        BOOST_CHECK( variant_Int2.GetInt2() == value_Int2 );

        const CVariant variant_Uint1 = CVariant(value_Uint1);
        BOOST_CHECK( !variant_Uint1.IsNull() );
        BOOST_CHECK( variant_Uint1.GetByte() == value_Uint1 );

        const CVariant variant_float = CVariant(value_float);
        BOOST_CHECK( !variant_float.IsNull() );
        BOOST_CHECK( variant_float.GetFloat() == value_float );

        const CVariant variant_double = CVariant(value_double);
        BOOST_CHECK( !variant_double.IsNull() );
        BOOST_CHECK( variant_double.GetDouble() == value_double );

        const CVariant variant_bool = CVariant(value_bool);
        BOOST_CHECK( !variant_bool.IsNull() );
        BOOST_CHECK( variant_bool.GetBit() == value_bool );

        const CVariant variant_string = CVariant(value_string);
        BOOST_CHECK( !variant_string.IsNull() );
        BOOST_CHECK( variant_string.GetString() == value_string );

        const CVariant variant_char = CVariant(value_char);
        BOOST_CHECK( !variant_char.IsNull() );
        BOOST_CHECK( variant_char.GetString() == value_char );

        const CVariant variant_CTimeShort = CVariant( value_CTime, eShort );
        BOOST_CHECK( !variant_CTimeShort.IsNull() );
        BOOST_CHECK( variant_CTimeShort.GetCTime() == value_CTime );

        const CVariant variant_CTimeLong = CVariant( value_CTime, eLong );
        BOOST_CHECK( !variant_CTimeLong.IsNull() );
        BOOST_CHECK( variant_CTimeLong.GetCTime() == value_CTime );
    }

    // Call Factories for different types
    {
        const CVariant variant_Int8 = CVariant::BigInt( const_cast<Int8*>(&value_Int8) );
        BOOST_CHECK( !variant_Int8.IsNull() );
        BOOST_CHECK( variant_Int8.GetInt8() == value_Int8 );

        const CVariant variant_Int4 = CVariant::Int( const_cast<Int4*>(&value_Int4) );
        BOOST_CHECK( !variant_Int4.IsNull() );
        BOOST_CHECK( variant_Int4.GetInt4() == value_Int4 );

        const CVariant variant_Int2 = CVariant::SmallInt( const_cast<Int2*>(&value_Int2) );
        BOOST_CHECK( !variant_Int2.IsNull() );
        BOOST_CHECK( variant_Int2.GetInt2() == value_Int2 );

        const CVariant variant_Uint1 = CVariant::TinyInt( const_cast<Uint1*>(&value_Uint1) );
        BOOST_CHECK( !variant_Uint1.IsNull() );
        BOOST_CHECK( variant_Uint1.GetByte() == value_Uint1 );

        const CVariant variant_float = CVariant::Float( const_cast<float*>(&value_float) );
        BOOST_CHECK( !variant_float.IsNull() );
        BOOST_CHECK( variant_float.GetFloat() == value_float );

        const CVariant variant_double = CVariant::Double( const_cast<double*>(&value_double) );
        BOOST_CHECK( !variant_double.IsNull() );
        BOOST_CHECK( variant_double.GetDouble() == value_double );

        const CVariant variant_bool = CVariant::Bit( const_cast<bool*>(&value_bool) );
        BOOST_CHECK( !variant_bool.IsNull() );
        BOOST_CHECK( variant_bool.GetBit() == value_bool );

        const CVariant variant_LongChar = CVariant::LongChar( value_char, strlen(value_char) );
        BOOST_CHECK( !variant_LongChar.IsNull() );
        BOOST_CHECK( variant_LongChar.GetString() == value_char );

        const CVariant variant_VarChar = CVariant::VarChar( value_char, strlen(value_char) );
        BOOST_CHECK( !variant_VarChar.IsNull() );
        BOOST_CHECK( variant_VarChar.GetString() == value_char );

        const CVariant variant_Char = CVariant::Char( strlen(value_char), const_cast<char*>(value_char) );
        BOOST_CHECK( !variant_Char.IsNull() );
        BOOST_CHECK( variant_Char.GetString() == value_char );

        const CVariant variant_LongBinary = CVariant::LongBinary( strlen(value_binary), value_binary, strlen(value_binary)) ;
        BOOST_CHECK( !variant_LongBinary.IsNull() );

        const CVariant variant_VarBinary = CVariant::VarBinary( value_binary, strlen(value_binary) );
        BOOST_CHECK( !variant_VarBinary.IsNull() );

        const CVariant variant_Binary = CVariant::Binary( strlen(value_binary), value_binary, strlen(value_binary) );
        BOOST_CHECK( !variant_Binary.IsNull() );

        const CVariant variant_SmallDateTime = CVariant::SmallDateTime( const_cast<CTime*>(&value_CTime) );
        BOOST_CHECK( !variant_SmallDateTime.IsNull() );
        BOOST_CHECK( variant_SmallDateTime.GetCTime() == value_CTime );

        const CVariant variant_DateTime = CVariant::DateTime( const_cast<CTime*>(&value_CTime) );
        BOOST_CHECK( !variant_DateTime.IsNull() );
        BOOST_CHECK( variant_DateTime.GetCTime() == value_CTime );

//        CVariant variant_Numeric = CVariant::Numeric  (unsigned int precision, unsigned int scale, const char* p);
    }

    // Call Get method for different types
    {
        const Uint1 value_Uint1_tmp = CVariant::TinyInt( const_cast<Uint1*>(&value_Uint1) ).GetByte();
        BOOST_CHECK_EQUAL( value_Uint1, value_Uint1_tmp );

        const Int2 value_Int2_tmp = CVariant::SmallInt( const_cast<Int2*>(&value_Int2) ).GetInt2();
        BOOST_CHECK_EQUAL( value_Int2, value_Int2_tmp );

        const Int4 value_Int4_tmp = CVariant::Int( const_cast<Int4*>(&value_Int4) ).GetInt4();
        BOOST_CHECK_EQUAL( value_Int4, value_Int4_tmp );

        const Int8 value_Int8_tmp = CVariant::BigInt( const_cast<Int8*>(&value_Int8) ).GetInt8();
        BOOST_CHECK_EQUAL( value_Int8, value_Int8_tmp );

        const float value_float_tmp = CVariant::Float( const_cast<float*>(&value_float) ).GetFloat();
        BOOST_CHECK_EQUAL( value_float, value_float_tmp );

        const double value_double_tmp = CVariant::Double( const_cast<double*>(&value_double) ).GetDouble();
        BOOST_CHECK_EQUAL( value_double, value_double_tmp );

        const bool value_bool_tmp = CVariant::Bit( const_cast<bool*>(&value_bool) ).GetBit();
        BOOST_CHECK_EQUAL( value_bool, value_bool_tmp );

        const CTime value_CTime_tmp = CVariant::DateTime( const_cast<CTime*>(&value_CTime) ).GetCTime();
        BOOST_CHECK( value_CTime == value_CTime_tmp );

        // GetNumeric() ????
    }

    // Call operator= for different types
    //!!! It *fails* !!!
    if (false) {
        CVariant value_variant(0);
        value_variant.SetNull();

        value_variant = CVariant(0);
        BOOST_CHECK( CVariant(0) == value_variant );

        value_variant = value_Int8;
        BOOST_CHECK( CVariant( value_Int8 ) == value_variant );

        value_variant = value_Int4;
        BOOST_CHECK( CVariant( value_Int4 ) == value_variant );

        value_variant = value_Int2;
        BOOST_CHECK( CVariant( value_Int2 ) == value_variant );

        value_variant = value_Uint1;
        BOOST_CHECK( CVariant( value_Uint1 ) == value_variant );

        value_variant = value_float;
        BOOST_CHECK( CVariant( value_float ) == value_variant );

        value_variant = value_double;
        BOOST_CHECK( CVariant( value_double ) == value_variant );

        value_variant = value_string;
        BOOST_CHECK( CVariant( value_string ) == value_variant );

        value_variant = value_char;
        BOOST_CHECK( CVariant( value_char ) == value_variant );

        value_variant = value_bool;
        BOOST_CHECK( CVariant( value_bool ) == value_variant );

        value_variant = value_CTime;
        BOOST_CHECK( CVariant( value_CTime ) == value_variant );

    }

    // Check assigning of values of same type ...
    {
        CVariant variant_Int8( value_Int8 );
        BOOST_CHECK( !variant_Int8.IsNull() );
        variant_Int8 = CVariant( value_Int8 );
        BOOST_CHECK( variant_Int8.GetInt8() == value_Int8 );
        variant_Int8 = value_Int8;
        BOOST_CHECK( variant_Int8.GetInt8() == value_Int8 );

        CVariant variant_Int4( value_Int4 );
        BOOST_CHECK( !variant_Int4.IsNull() );
        variant_Int4 = CVariant( value_Int4 );
        BOOST_CHECK( variant_Int4.GetInt4() == value_Int4 );
        variant_Int4 = value_Int4;
        BOOST_CHECK( variant_Int4.GetInt4() == value_Int4 );

        CVariant variant_Int2( value_Int2 );
        BOOST_CHECK( !variant_Int2.IsNull() );
        variant_Int2 = CVariant( value_Int2 );
        BOOST_CHECK( variant_Int2.GetInt2() == value_Int2 );
        variant_Int2 = value_Int2;
        BOOST_CHECK( variant_Int2.GetInt2() == value_Int2 );

        CVariant variant_Uint1( value_Uint1 );
        BOOST_CHECK( !variant_Uint1.IsNull() );
        variant_Uint1 = CVariant( value_Uint1 );
        BOOST_CHECK( variant_Uint1.GetByte() == value_Uint1 );
        variant_Uint1 = value_Uint1;
        BOOST_CHECK( variant_Uint1.GetByte() == value_Uint1 );

        CVariant variant_float( value_float );
        BOOST_CHECK( !variant_float.IsNull() );
        variant_float = CVariant( value_float );
        BOOST_CHECK( variant_float.GetFloat() == value_float );
        variant_float = value_float;
        BOOST_CHECK( variant_float.GetFloat() == value_float );

        CVariant variant_double( value_double );
        BOOST_CHECK( !variant_double.IsNull() );
        variant_double = CVariant( value_double );
        BOOST_CHECK( variant_double.GetDouble() == value_double );
        variant_double = value_double;
        BOOST_CHECK( variant_double.GetDouble() == value_double );

        CVariant variant_bool( value_bool );
        BOOST_CHECK( !variant_bool.IsNull() );
        variant_bool = CVariant( value_bool );
        BOOST_CHECK( variant_bool.GetBit() == value_bool );
        variant_bool = value_bool;
        BOOST_CHECK( variant_bool.GetBit() == value_bool );

        CVariant variant_string( value_string );
        BOOST_CHECK( !variant_string.IsNull() );
        variant_string = CVariant( value_string );
        BOOST_CHECK( variant_string.GetString() == value_string );
        variant_string = value_string;
        BOOST_CHECK( variant_string.GetString() == value_string );

        CVariant variant_char( value_char );
        BOOST_CHECK( !variant_char.IsNull() );
        variant_char = CVariant( value_char );
        BOOST_CHECK( variant_char.GetString() == value_char );
        variant_char = value_char;
        BOOST_CHECK( variant_char.GetString() == value_char );

        CVariant variant_CTimeShort( value_CTime, eShort );
        BOOST_CHECK( !variant_CTimeShort.IsNull() );
        variant_CTimeShort = CVariant( value_CTime, eShort );
        BOOST_CHECK( variant_CTimeShort.GetCTime() == value_CTime );

        CVariant variant_CTimeLong( value_CTime, eLong );
        BOOST_CHECK( !variant_CTimeLong.IsNull() );
        variant_CTimeLong = CVariant( value_CTime, eLong );
        BOOST_CHECK( variant_CTimeLong.GetCTime() == value_CTime );

//        explicit CVariant(CDB_Object* obj);

    }

    // Call operator= for different Variant types
    {
        // Assign to CVariant(NULL)
        if (false) {
            {
                CVariant value_variant(0);
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant(0);
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_Uint1 )
        if (false) {
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant(value_Uint1);
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_Int2 )
        if (false) {
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_Int2 );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_Int4 )
        if (false) {
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_Int4 );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_Int8 )
        if (false) {
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_Int8 );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_float )
        if (false) {
            {
                CVariant value_variant( value_float );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_float );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_double )
        if (false) {
            {
                CVariant value_variant( value_double );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_double );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_bool )
        if (false) {
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_bool );
                value_variant = CVariant( value_CTime );
            }
        }

        // Assign to CVariant( value_CTime )
        if (false) {
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant(0);
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_Int8 );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_Int4 );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_Int2 );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_Uint1 );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_float );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_double );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_bool );
            }
            {
                CVariant value_variant( value_CTime );
                value_variant = CVariant( value_CTime );
            }
        }
    }

    // Test Null cases ...
    {
        CVariant value_variant(0);

        BOOST_CHECK( !value_variant.IsNull() );

        value_variant.SetNull();
        BOOST_CHECK( value_variant.IsNull() );

        value_variant.SetNull();
        BOOST_CHECK( value_variant.IsNull() );

        value_variant.SetNull();
        BOOST_CHECK( value_variant.IsNull() );
    }

    // Check operator==
    {
        // Check values of same type ...
        BOOST_CHECK( CVariant( true ) == CVariant( true ) );
        BOOST_CHECK( CVariant( false ) == CVariant( false ) );
        BOOST_CHECK( CVariant( Uint1(1) ) == CVariant( Uint1(1) ) );
        BOOST_CHECK( CVariant( Int2(1) ) == CVariant( Int2(1) ) );
        BOOST_CHECK( CVariant( Int4(1) ) == CVariant( Int4(1) ) );
        BOOST_CHECK( CVariant( Int8(1) ) == CVariant( Int8(1) ) );
        BOOST_CHECK( CVariant( float(1) ) == CVariant( float(1) ) );
        BOOST_CHECK( CVariant( double(1) ) == CVariant( double(1) ) );
        BOOST_CHECK( CVariant( string("abcd") ) == CVariant( string("abcd") ) );
        BOOST_CHECK( CVariant( "abcd" ) == CVariant( "abcd" ) );
        BOOST_CHECK( CVariant( value_CTime, eShort ) ==
                     CVariant( value_CTime, eShort )
                     );
        BOOST_CHECK( CVariant( value_CTime, eLong ) ==
                     CVariant( value_CTime, eLong )
                     );
    }

    // Check operator<
    {
        // Check values of same type ...
        {
            //  Type not supported
            // BOOST_CHECK( CVariant( false ) < CVariant( true ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Uint1(1) ) );
            BOOST_CHECK( CVariant( Int2(-1) ) < CVariant( Int2(1) ) );
            BOOST_CHECK( CVariant( Int4(-1) ) < CVariant( Int4(1) ) );
            BOOST_CHECK( CVariant( Int8(-1) ) < CVariant( Int8(1) ) );
            BOOST_CHECK( CVariant( float(-1) ) < CVariant( float(1) ) );
            BOOST_CHECK( CVariant( double(-1) ) < CVariant( double(1) ) );
            BOOST_CHECK( CVariant(string("abcd")) < CVariant( string("bcde")));
            BOOST_CHECK( CVariant( "abcd" ) < CVariant( "bcde" ) );
            CTime new_time = value_CTime;
            new_time += 1;
            BOOST_CHECK( CVariant( value_CTime, eShort ) <
                         CVariant( new_time, eShort )
                         );
            BOOST_CHECK( CVariant( value_CTime, eLong ) <
                         CVariant( new_time, eLong )
                         );
        }

        // Check comparasion wit Uint1(0) ...
        //!!! It *fails* !!!
        if (false) {
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Uint1(1) ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Int2(1) ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Int4(1) ) );
            // !!! Does not work ...
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( Int8(1) ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( float(1) ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( double(1) ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( string("bcde") ) );
            BOOST_CHECK( CVariant( Uint1(0) ) < CVariant( "bcde" ) );
        }
    }

    // Check GetType
    {
        const CVariant variant_Int8( value_Int8 );
        BOOST_CHECK_EQUAL( eDB_BigInt, variant_Int8.GetType() );

        const CVariant variant_Int4( value_Int4 );
        BOOST_CHECK_EQUAL( eDB_Int, variant_Int4.GetType() );

        const CVariant variant_Int2( value_Int2 );
        BOOST_CHECK_EQUAL( eDB_SmallInt, variant_Int2.GetType() );

        const CVariant variant_Uint1( value_Uint1 );
        BOOST_CHECK_EQUAL( eDB_TinyInt, variant_Uint1.GetType() );

        const CVariant variant_float( value_float );
        BOOST_CHECK_EQUAL( eDB_Float, variant_float.GetType() );

        const CVariant variant_double( value_double );
        BOOST_CHECK_EQUAL( eDB_Double, variant_double.GetType() );

        const CVariant variant_bool( value_bool );
        BOOST_CHECK_EQUAL( eDB_Bit, variant_bool.GetType() );

        const CVariant variant_string( value_string );
        BOOST_CHECK_EQUAL( eDB_VarChar, variant_string.GetType() );

        const CVariant variant_char( value_char );
        BOOST_CHECK_EQUAL( eDB_VarChar, variant_char.GetType() );
    }

    // Test BLOB ...
    {
    }

    // Check AsNotNullString
    {
    }

    // Assignment of bound values ...
    {
    }
}


////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_NCBI_LS(void)
{
    if (true) {
        static time_t s_tTimeOut = 30; // in seconds
        CDB_Connection* pDbLink = NULL;

        string sDbDriver(m_args.GetDriverName());
//         string sDbServer("MSSQL57");
        string sDbServer("mssql57.nac.ncbi.nlm.nih.gov");
        string sDbName("NCBI_LS");
        string sDbUser("anyone");
        string sDbPasswd("allowed");
        string sErr;

        try {
            I_DriverContext* drv_context = m_DS->GetDriverContext();

        	if( drv_context == NULL ) {
                BOOST_FAIL("FATAL: Unable to load context for dbdriver " +
                           sDbDriver);
        	}
        	drv_context->SetMaxTextImageSize( 0x7fffffff );
        	drv_context->SetLoginTimeout( s_tTimeOut );
        	drv_context->SetTimeout( s_tTimeOut );

        	pDbLink = drv_context->Connect( sDbServer, sDbUser, sDbPasswd,
        				    0, true, "Service" );

        	string sUseDb = "use "+sDbName;
        	auto_ptr<CDB_LangCmd> pUseCmd( pDbLink->LangCmd( sUseDb.c_str() ) );
        	if( pUseCmd->Send() ) {
        	    pUseCmd->DumpResults();
        	} else {
                BOOST_FAIL("Unable to send sql command "+ sUseDb);
        	}
        } catch( CDB_Exception& dbe ) {
        	sErr = "CDB Exception from "+dbe.OriginatedFrom()+":"+
        	    dbe.SeverityString(dbe.Severity())+": "+
        	    dbe.Message();

            BOOST_FAIL(sErr);
        }


        CDB_Connection* m_Connect = pDbLink;

        // id(7777777) is going to expire soon ...
//         int uid = 0;
//         bool need_userInfo = true;
//
//         try {
//             CDB_RPCCmd* r_cmd= m_Connect->RPC("getAcc4Sid", 2);
//             CDB_Int id(7777777);
//             CDB_TinyInt info(need_userInfo? 1 : 0);
//
//             r_cmd->SetParam("@sid", &id);
//             r_cmd->SetParam("@need_info", &info);
//
//             r_cmd->Send();
//
//             CDB_Result* r;
//             const char* res_name;
//             while(r_cmd->HasMoreResults()) {
//                 r= r_cmd->Result();
//                 if(r == 0) continue;
//                 if(r->ResultType() == eDB_RowResult) {
//                     res_name= r->ItemName(0);
//                     if(strstr(res_name, "uId") == res_name) {
//                         while(r->Fetch()) {
//                             r->GetItem(&id);
//                         }
//                         uid = id.Value();
//                     }
//                     else if((strstr(res_name, "login_name") == res_name)) {
//                         CDB_LongChar name;
//                         while(r->Fetch()) {
//                             r->GetItem(&name); // login name
//                             r->GetItem(&name); // user name
//                             r->GetItem(&name); // first name
//                             r->GetItem(&name); // last name
//                             r->GetItem(&name); // affiliation
//                             r->GetItem(&name); // question
//                             r->GetItem(&info); // status
//                             r->GetItem(&info); // role
//                         }
//                     }
//                     else if((strstr(res_name, "gID") == res_name)) {
//                         // int gID;
//                         // int bID= 0;
//                         CDB_VarChar label;
//                         CDB_LongChar val;
//
//                         while(r->Fetch()) {
//                             r->GetItem(&id); // gID
//                             r->GetItem(&id); // bID
//
//                             r->GetItem(&info); // flag
//                             r->GetItem(&label);
//                             r->GetItem(&val);
//
//                         }
//                     }
//                     else if((strstr(res_name, "addr") == res_name)) {
//                         CDB_LongChar addr;
//                         CDB_LongChar comment(2048);
//                         while(r->Fetch()) {
//                             r->GetItem(&addr);
//                             r->GetItem(&info);
//                             r->GetItem(&comment);
//                         }
//                     }
//                     else if((strstr(res_name, "num") == res_name)) {
//                         CDB_LongChar num;
//                         CDB_LongChar comment(2048);
//                         while(r->Fetch()) {
//                             r->GetItem(&num);
//                             r->GetItem(&info);
//                             r->GetItem(&comment);
//                         }
//                     }
//                 }
//                 delete r;
//             }
//             delete r_cmd;
//         }
//         catch (CDB_Exception& e) {
//             CDB_UserHandler::GetDefault().HandleIt(&e);
//         }

        // Find accounts
        int acc_num   = 0;
        CDB_Int       status;
        // Request
        CDB_VarChar   db_login( NULL, 64 );
        CDB_VarChar   db_title( NULL, 64 );
        CDB_VarChar   db_first( NULL, 127 );
        CDB_VarChar   db_last( NULL, 127 );
        CDB_VarChar   db_affil( NULL, 127 );
        // grp ids
        CDB_Int       db_gid1;
        CDB_LongChar  db_val1;
        CDB_VarChar   db_label1( NULL, 32 );
        CDB_Int       db_gid2;
        CDB_LongChar  db_val2;
        CDB_VarChar   db_label2( NULL, 32 );
        CDB_Int       db_gid3;
        CDB_LongChar  db_val3;
        CDB_VarChar   db_label3( NULL, 32 );
        // emails
        CDB_VarChar   db_email1( NULL, 255 );
        CDB_VarChar   db_email2( NULL, 255 );
        CDB_VarChar   db_email3( NULL, 255 );
        // phones
        CDB_VarChar   db_phone1( NULL, 20 );
        CDB_VarChar   db_phone2( NULL, 20 );
        CDB_VarChar   db_phone3( NULL, 20 );
        // Reply
        CDB_Int       db_uid;
        CDB_Int       db_score;

        auto_ptr<CDB_RPCCmd> pRpc( NULL );

//         pRpc.reset( m_Connect->RPC("FindAccount",20) );
        pRpc.reset( m_Connect->RPC("FindAccountMatchAll",20) );

        db_login.SetValue( "lsroot" );
        // db_last.SetValue( "%" );

        pRpc->SetParam( "@login",  &db_login );
        pRpc->SetParam( "@title",   &db_title );
        pRpc->SetParam( "@first", &db_first );
        pRpc->SetParam( "@last", &db_last );
        pRpc->SetParam( "@affil", &db_affil );
        pRpc->SetParam( "@gid1", &db_gid1 );
        pRpc->SetParam( "@val1", &db_val1 );
        pRpc->SetParam( "@label1", &db_label1 );
        pRpc->SetParam( "@gid2", &db_gid2 );
        pRpc->SetParam( "@val2", &db_val2 );
        pRpc->SetParam( "@label2", &db_label2 );
        pRpc->SetParam( "@gid3",  &db_gid3 );
        pRpc->SetParam( "@val3", &db_val3 );
        pRpc->SetParam( "@label3", &db_label3 );
        pRpc->SetParam( "@email1", &db_email1 );
        pRpc->SetParam( "@email2", &db_email2 );
        pRpc->SetParam( "@email3", &db_email3 );
        pRpc->SetParam( "@phone1", &db_phone1 );
        pRpc->SetParam( "@phone2", &db_phone2 );
        pRpc->SetParam( "@phone3", &db_phone3 );
        bool bFetchErr = false;
        string sError;

        try {
        	if( pRpc->Send() ) {
        	    // Save results
        	    while ( !bFetchErr && pRpc->HasMoreResults() ) {
            		auto_ptr<CDB_Result> pRes( pRpc->Result() );
            		if( !pRes.get() ) {
            		    continue;
            		}
            		if( pRes->ResultType() == eDB_RowResult ) {
            		    while( pRes->Fetch() ) {
                            ++acc_num;
                			pRes->GetItem( &db_uid );
                                    // cout << "uid=" << db_uid.Value();
                			pRes->GetItem( &db_score );
                                    // cout << " score=" << db_score.Value() << endl;
            		    }
            		} else if( pRes->ResultType() == eDB_StatusResult ) {
            		    while( pRes->Fetch() ) {
                			pRes->GetItem( &status );
                			if( status.Value() < 0 ) {
                			    sError = "Bad status value " +
                                    NStr::IntToString(status.Value())
                				+ " from RPC 'FindAccount'";
                			    bFetchErr = true;
                			    break;
                			}
            		    }
            		}
        	    }
        	} else { // Error sending rpc
        	    sError = "Error sending rpc 'FindAccount' to db server";
        	    bFetchErr = true;
        	}
        }  catch( CDB_Exception& dbe ) {
        	sError = "CDB Exception from "+dbe.OriginatedFrom()+":"+
        	    dbe.SeverityString(dbe.Severity())+": "+
        	    dbe.Message();
        	bFetchErr = true;
        }

        BOOST_CHECK( acc_num > 0 );
        BOOST_CHECK_EQUAL( bFetchErr, false );
    }

    auto_ptr<IConnection> auto_conn( m_DS->CreateConnection() );
    BOOST_CHECK( auto_conn.get() != NULL );

    auto_conn->Connect(
        "anyone",
        "allowed",
//         "MSSQL57",
        "mssql57.nac.ncbi.nlm.nih.gov",
        "NCBI_LS"
        );

    // Does not work with ftds64_odbc ...
//     {
//         int sid = 0;
//
//         {
//             auto_ptr<IStatement> auto_stmt( auto_conn->GetStatement() );
//
//             auto_ptr<IResultSet> rs( auto_stmt->ExecuteQuery( "SELECT sID FROM Session" ) );
//             BOOST_CHECK( rs.get() != NULL );
//             BOOST_CHECK( rs->Next() );
//             sid = rs->GetVariant(1).GetInt4();
//             BOOST_CHECK( sid > 0 );
//         }
//
//         {
//             int num = 0;
//             auto_ptr<ICallableStatement> auto_stmt( auto_conn->GetCallableStatement("getAcc4Sid", 2) );
//
//             auto_stmt->SetParam( CVariant(Int4(sid)), "@sid" );
//             auto_stmt->SetParam( CVariant(Int2(1)), "@need_info" );
//
//             auto_stmt->Execute();
//
//             BOOST_CHECK(auto_stmt->HasMoreResults());
//             BOOST_CHECK(auto_stmt->HasMoreResults());
//             BOOST_CHECK(auto_stmt->HasMoreResults());
//             BOOST_CHECK(auto_stmt->HasRows());
//             auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
//             BOOST_CHECK(rs.get() != NULL);
//
//             while (rs->Next()) {
//                 BOOST_CHECK(rs->GetVariant(1).GetInt4() > 0);
//                 BOOST_CHECK(rs->GetVariant(2).GetInt4() > 0);
//                 BOOST_CHECK_EQUAL(rs->GetVariant(3).IsNull(), false);
//                 BOOST_CHECK(rs->GetVariant(4).GetString().size() > 0);
//                 // BOOST_CHECK(rs->GetVariant(5).GetString().size() > 0); // Stoped to work for some reason ...
//                 ++num;
//             }
//
//             BOOST_CHECK(num > 0);
//
// //             DumpResults(auto_stmt.get());
//         }
//     }

    //
    {
        auto_ptr<ICallableStatement> auto_stmt(
            auto_conn->GetCallableStatement("FindAccount", 20)
            );

        auto_stmt->SetParam( CVariant(eDB_VarChar), "@login" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@title" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@first" );
        auto_stmt->SetParam( CVariant("a%"), "@last" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@affil" );
        auto_stmt->SetParam( CVariant(eDB_Int), "@gid1" );
        auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val1" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@label1" );
        auto_stmt->SetParam( CVariant(eDB_Int), "@gid2" );
        auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val2" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@label2" );
        auto_stmt->SetParam( CVariant(eDB_Int), "@gid3" );
        auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val3" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@label3" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@email1" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@email2" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@email3" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone1" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone2" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone3" );

        auto_stmt->Execute();

        BOOST_CHECK( GetNumOfRecords(auto_stmt) > 1 );

        auto_stmt->Execute();

        BOOST_CHECK( GetNumOfRecords(auto_stmt) > 1 );
    }

    //
    {
        auto_ptr<ICallableStatement> auto_stmt(
            auto_conn->GetCallableStatement("FindAccountMatchAll", 20)
            );

        auto_stmt->SetParam( CVariant(eDB_VarChar), "@login" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@title" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@first" );
        auto_stmt->SetParam( CVariant("a%"), "@last" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@affil" );
        auto_stmt->SetParam( CVariant(eDB_Int), "@gid1" );
        auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val1" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@label1" );
        auto_stmt->SetParam( CVariant(eDB_Int), "@gid2" );
        auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val2" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@label2" );
        auto_stmt->SetParam( CVariant(eDB_Int), "@gid3" );
        auto_stmt->SetParam( CVariant(eDB_LongChar, 3600), "@val3" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@label3" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@email1" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@email2" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@email3" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone1" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone2" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@phone3" );

        auto_stmt->Execute();

        BOOST_CHECK( GetNumOfRecords(auto_stmt) > 1 );

        auto_stmt->Execute();

        BOOST_CHECK( GetNumOfRecords(auto_stmt) > 1 );

        // Another set of values ....
        auto_stmt->SetParam( CVariant("lsroot"), "@login" );
        auto_stmt->SetParam( CVariant(eDB_VarChar), "@last" );

        auto_stmt->Execute();

        BOOST_CHECK_EQUAL( GetNumOfRecords(auto_stmt), 1 );
    }
}


void
CDBAPIUnitTest::Test_Authentication(void)
{
    // MSSQL10 has SSL certificate.
    // There is no MSSQL10 any more ...
//     {
//         auto_ptr<IConnection> auto_conn( m_DS->CreateConnection() );
//
//         auto_conn->Connect(
//             "NCBI_NT\\anyone",
//             "Perm1tted",
//             "MSSQL10"
//             );
//
//         auto_ptr<IStatement> auto_stmt( auto_conn->GetStatement() );
//
//         auto_ptr<IResultSet> rs( auto_stmt->ExecuteQuery( "select @@version" ) );
//         BOOST_CHECK( rs.get() != NULL );
//         BOOST_CHECK( rs->Next() );
//     }

    // No SSL certificate.
    if (true) {
        auto_ptr<IConnection> auto_conn( m_DS->CreateConnection() );

        auto_conn->Connect(
            "NAC\\anyone",
            "permitted",
            "MS_DEV2"
            );

        auto_ptr<IStatement> auto_stmt( auto_conn->GetStatement() );

        auto_ptr<IResultSet> rs(auto_stmt->ExecuteQuery("select @@version"));
        BOOST_CHECK( rs.get() != NULL );
        BOOST_CHECK( rs->Next() );
    }
}


////////////////////////////////////////////////////////////////////////////////
class CContextThread : public CThread
{
public:
    CContextThread(CDriverManager& dm, const CTestArguments& args);

protected:
    virtual ~CContextThread(void);
    virtual void* Main(void);
    virtual void  OnExit(void);

private:
    const CTestArguments    m_Args;
    CDriverManager&         m_DM;
    IDataSource*            m_DS;
};


CContextThread::CContextThread(CDriverManager& dm,
                               const CTestArguments& args) :
    m_Args(args),
    m_DM(dm),
    m_DS(NULL)
{
}


CContextThread::~CContextThread(void)
{
}


void*
CContextThread::Main(void)
{
    try {
        m_DS = m_DM.CreateDs(m_Args.GetDriverName(), &m_Args.GetDBParameters());
        return m_DS;
    } catch (const CDB_ClientEx&) {
        // Ignore it ...
    } catch (...) {
        _ASSERT(false);
    }

    return NULL;
}


void  CContextThread::OnExit(void)
{
    if (m_DS) {
        m_DM.DestroyDs(m_Args.GetDriverName());
    }
}


void
CDBAPIUnitTest::Test_DriverContext_One(void)
{
    enum {eNumThreadsMax = 5};
    void* ok;
    int succeeded_num = 0;
    CRef<CContextThread> thr[eNumThreadsMax];

    // Spawn more threads
    for (int i = 0; i < eNumThreadsMax; ++i) {
        thr[i] = new CContextThread(m_DM, m_args);
        // Allow threads to run even in single thread environment
        thr[i]->Run(CThread::fRunAllowST);
    }

    // Wait for all threads
    for (unsigned int i = 0; i < eNumThreadsMax; ++i) {
        thr[i]->Join(&ok);
        if (ok != NULL) {
            ++succeeded_num;
        }
    }

    BOOST_CHECK(succeeded_num >= 1);
}


void
CDBAPIUnitTest::Test_DriverContext_Many(void)
{
    enum {eNumThreadsMax = 5};
    void* ok;
    int succeeded_num = 0;
    CRef<CContextThread> thr[eNumThreadsMax];

    // Spawn more threads
    for (int i = 0; i < eNumThreadsMax; ++i) {
        thr[i] = new CContextThread(m_DM, m_args);
        // Allow threads to run even in single thread environment
        thr[i]->Run(CThread::fRunAllowST);
    }

    // Wait for all threads
    for (unsigned int i = 0; i < eNumThreadsMax; ++i) {
        thr[i]->Join(&ok);
        if (ok != NULL) {
            ++succeeded_num;
        }
    }

    BOOST_CHECK_EQUAL(succeeded_num, int(eNumThreadsMax));
}


void CDBAPIUnitTest::Test_Decimal(void)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( m_Conn->GetStatement() );

    sql = "declare @num decimal(6,2) set @num = 1.5 select @num as NumCol";
    auto_stmt->SendSql(sql);

    BOOST_CHECK( auto_stmt->HasMoreResults() );
    BOOST_CHECK( auto_stmt->HasRows() );
    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
    BOOST_CHECK( rs.get() != NULL );
    BOOST_CHECK( rs->Next() );

    DumpResults(auto_stmt.get());
}


void
CDBAPIUnitTest::Test_Query_Cancelation(void)
{
    string sql;

    // Cancel without a previously thrown exception.
    {
        // IStatement
        {
            auto_ptr<IStatement> auto_stmt(m_Conn->GetStatement());

            // 1
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            auto_stmt->Cancel();
            auto_stmt->Cancel();

            // 2
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );
            }
            auto_stmt->Cancel();
            auto_stmt->Cancel();

            // 3
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );
                BOOST_CHECK( rs->Next() );
            }
            auto_stmt->Cancel();
            auto_stmt->Cancel();

            // 4
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            {
                int i = 0;
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);
                while (rs->Next()) {
                    ++i;
                }
                BOOST_CHECK(i > 0);
            }
            auto_stmt->Cancel();
            auto_stmt->Cancel();
            auto_stmt->Cancel();
        }

        // IColableStatement
        {
            auto_ptr<ICallableStatement> auto_stmt;

            // 1
            auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
            auto_stmt->Execute();
            DumpResults(auto_stmt.get());
            auto_stmt->Execute();
            DumpResults(auto_stmt.get());

            // 2
            auto_stmt->Execute();
            auto_stmt->Cancel();
            auto_stmt->Execute();
            auto_stmt->Cancel();
            auto_stmt->Cancel();

            // 3
            auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
            auto_stmt->Execute();
            auto_stmt->Cancel();
            auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
            auto_stmt->Execute();
            auto_stmt->Cancel();
            auto_stmt->Cancel();

            // 4
            auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
            auto_stmt->Execute();
            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK(rs.get() != NULL);
            }
            auto_stmt->Cancel();
            auto_stmt->Cancel();

            // 5
            auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
            auto_stmt->Execute();
            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK(rs.get() != NULL);
                BOOST_CHECK(rs->Next());
            }
            auto_stmt->Cancel();
            auto_stmt->Cancel();

            // 6
            auto_stmt.reset(m_Conn->GetCallableStatement("sp_databases"));
            auto_stmt->Execute();
            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            {
                int i = 0;
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);
                while (rs->Next()) {
                    ++i;
                }
                BOOST_CHECK(i > 0);
            }
            auto_stmt->Cancel();
            auto_stmt->Cancel();
            auto_stmt->Cancel();
        }

        // BCP
        {
        }
    }

    // Cancel with a previously thrown exception.
    {
        // IStatement
        {
            auto_ptr<IStatement> auto_stmt(m_Conn->GetStatement());

            // 1
            try {
                auto_stmt->SendSql("SELECT oops FROM sysobjects");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
            } catch(const CDB_Exception&)
            {
                auto_stmt->Cancel();
            }
            auto_stmt->Cancel();

            // Check that everything is fine ...
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            DumpResults(auto_stmt.get());

            // 2
            try {
                sql = "SELECT name FROM sysobjects WHERE name = 'oops'";
                auto_stmt->SendSql(sql);
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);
                rs->Next();
            } catch(const CDB_Exception&)
            {
                auto_stmt->Cancel();
            }
            auto_stmt->Cancel();

            // Check that everything is fine ...
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            DumpResults(auto_stmt.get());

            // 3
            try {
                int i = 0;
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);
                while (rs->Next()) {
                    ++i;
                }
                BOOST_CHECK(i > 0);
                rs->Next();
            } catch(const CDB_Exception&)
            {
                auto_stmt->Cancel();
            }
            auto_stmt->Cancel();

            // Check that everything is fine ...
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            DumpResults(auto_stmt.get());
        }


        // IColableStatement
        {
            auto_ptr<ICallableStatement> auto_stmt;

            // 1
            try {
                auto_stmt.reset(m_Conn->GetCallableStatement("sp_wrong"));
                auto_stmt->Execute();
            } catch(const CDB_Exception&)
            {
                auto_stmt->Cancel();
            }

            try {
                auto_stmt->Execute();
            } catch(const CDB_Exception&)
            {
                auto_stmt->Cancel();
            }
        }
    }
}


void CDBAPIUnitTest::Test_Timeout(void)
{
    bool timeout_was_reported = false;
    auto_ptr<IConnection> auto_conn;
    I_DriverContext* dc = m_DS->GetDriverContext();
    unsigned int timeout = dc->GetTimeout();

    dc->SetTimeout(1);

    auto_conn.reset(m_DS->CreateConnection());
    BOOST_CHECK(auto_conn.get() != NULL);
    auto_conn->Connect(
        m_args.GetUserName(),
        m_args.GetUserPassword(),
        m_args.GetServerName()
        );

    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());

    try {
        auto_stmt->SendSql("waitfor delay '0:00:03'");
        BOOST_CHECK(auto_stmt->HasMoreResults());
    // } catch(const CDB_TimeoutEx&) {
    } catch(const CDB_Exception&) {
        timeout_was_reported = true;
        auto_stmt->Cancel();
    }

    // Check if connection is alive ...
    if (false) {
        auto_stmt->SendSql("SELECT name FROM sysobjects");
        BOOST_CHECK( auto_stmt->HasMoreResults() );
        BOOST_CHECK( auto_stmt->HasRows() );
        auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
        BOOST_CHECK( rs.get() != NULL );
    }

    dc->SetTimeout(timeout);

    BOOST_CHECK(timeout_was_reported);
}


////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_SetLogStream(void)
{
    CNcbiOfstream logfile("dbapi_unit_test.log");

    m_DS->SetLogStream(&logfile);
    m_DS->SetLogStream(&logfile);

    // Test block ...
    {
        // No errors ...
        {
            auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            Connect(auto_conn);

            auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            BOOST_CHECK(auto_stmt->HasMoreResults());
            DumpResults(auto_stmt.get());
        }

        m_DS->SetLogStream(&logfile);

        // Force errors ...
        {
            auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            Connect(auto_conn);

            auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
            try {
                auto_stmt->SendSql("SELECT oops FROM sysobjects");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
            } catch(const CDB_Exception&) {
                auto_stmt->Cancel();
            }
        }

        m_DS->SetLogStream(&logfile);
    }

    // Install user-defined error handler (eTakeOwnership)
    {
        I_DriverContext* drv_context = m_DS->GetDriverContext();

        if (m_args.GetDriverName() == "odbc" ||
            m_args.GetDriverName() == "odbcw" ||
            m_args.GetDriverName() == "ftds64_odbc"
            ) {
            drv_context->PushCntxMsgHandler(new CODBCErrHandler,
                                            eTakeOwnership
                                            );
            drv_context->PushDefConnMsgHandler(new CODBCErrHandler,
                                               eTakeOwnership
                                               );
        } else {
            CRef<CDB_UserHandler> hx(new CErrHandler);
            CDB_UserHandler::SetDefault(hx);
            drv_context->PushCntxMsgHandler(new CErrHandler, eTakeOwnership);
            drv_context->PushDefConnMsgHandler(new CErrHandler, eTakeOwnership);
        }
    }

    // Test block ...
    {
        // No errors ...
        {
            auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            Connect(auto_conn);

            auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            BOOST_CHECK(auto_stmt->HasMoreResults());
            DumpResults(auto_stmt.get());
        }

        m_DS->SetLogStream(&logfile);

        // Force errors ...
        {
            auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            Connect(auto_conn);

            auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
            try {
                auto_stmt->SendSql("SELECT oops FROM sysobjects");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
            } catch(const CDB_Exception&) {
                auto_stmt->Cancel();
            }
        }

        m_DS->SetLogStream(&logfile);
    }

    // Install user-defined error handler (eNoOwnership)
    {
        I_DriverContext* drv_context = m_DS->GetDriverContext();

        if (m_args.GetDriverName() == "odbc" ||
            m_args.GetDriverName() == "odbcw" ||
            m_args.GetDriverName() == "ftds64_odbc"
            ) {
            drv_context->PushCntxMsgHandler(new CODBCErrHandler,
                                            eNoOwnership
                                            );
            drv_context->PushDefConnMsgHandler(new CODBCErrHandler,
                                               eNoOwnership
                                               );
        } else {
            drv_context->PushCntxMsgHandler(new CErrHandler, eNoOwnership);
            drv_context->PushDefConnMsgHandler(new CErrHandler, eNoOwnership);
        }
    }

    // Test block ...
    {
        // No errors ...
        {
            auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            Connect(auto_conn);

            auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
            auto_stmt->SendSql("SELECT name FROM sysobjects");
            BOOST_CHECK(auto_stmt->HasMoreResults());
            DumpResults(auto_stmt.get());
        }

        m_DS->SetLogStream(&logfile);

        // Force errors ...
        {
            auto_ptr<IConnection> auto_conn(m_DS->CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            Connect(auto_conn);

            auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
            try {
                auto_stmt->SendSql("SELECT oops FROM sysobjects");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
            } catch(const CDB_Exception&) {
                auto_stmt->Cancel();
            }
        }

        m_DS->SetLogStream(&logfile);
    }
}

////////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_Bind(void)
{
}

void
CDBAPIUnitTest::Test_Execute(void)
{
}

void
CDBAPIUnitTest::Test_Exception(void)
{
}

void
CDBAPIUnitTest::Repeated_Usage(void)
{
}

void
CDBAPIUnitTest::Single_Value_Writing(void)
{
}

void
CDBAPIUnitTest::Single_Value_Reading(void)
{
}

void
CDBAPIUnitTest::Bulk_Reading(void)
{
}

void
CDBAPIUnitTest::Multiple_Resultset(void)
{
}

void
CDBAPIUnitTest::Error_Conditions(void)
{
}

void
CDBAPIUnitTest::Transactional_Behavior(void)
{
}


////////////////////////////////////////////////////////////////////////////////
CDBAPITestSuite::CDBAPITestSuite(const CTestArguments& args)
    : test_suite("DBAPI Test Suite")
{
    bool SolarisWorkshop = false;

#if defined(NCBI_OS_SOLARIS) && defined(NCBI_COMPILER_WORKSHOP)
    SolarisWorkshop = true;
#endif

    // add member function test cases to a test suite
    boost::shared_ptr<CDBAPIUnitTest> DBAPIInstance(new CDBAPIUnitTest(args));
    boost::unit_test::test_case* tc = NULL;

    // Test DriverContext
    if (args.GetDriverName() == "ftds" ||
        args.GetDriverName() == "ftds63" ||
        args.GetDriverName() == "ftds64_dblib" ||
        // args.GetDriverName() == "dblib" ||
        args.GetDriverName() == "msdblib"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DriverContext_One,
                                   DBAPIInstance);
        add(tc);
    }

    // We cannot run this test till we have an appropriate method in DBAPI
    // to destroy a context.
//     if (args.GetDriverName() == "ctlib" ||
//         args.GetDriverName() == "ftds64_ctlib" ||
//         args.GetDriverName() == "odbc" ||
//         args.GetDriverName() == "ftds64_odbc"
//         ) {
//         tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DriverContext_Many,
//                                    DBAPIInstance);
//         add(tc);
//     }

    boost::unit_test::test_case* tc_init =
        BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::TestInit, DBAPIInstance);

    add(tc_init);


    // It looks like ftds on WorkShop55_550-DebugMT64 doesn't work ...
    if ((args.GetDriverName() == "ftds" && !SolarisWorkshop)
        || args.GetDriverName() == "dblib"
        || args.GetDriverName() == "msdblib"
//         || args.GetDriverName() == "ctlib"
//         || args.GetDriverName() == "ftds64_ctlib"
//         || args.GetDriverName() == "ftds64_odbc"
//         || args.GetDriverName() == "odbc"
//         || args.GetDriverName() == "odbcw"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Timeout,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }


    // There is a problem with ftds driver
    // on GCC_340-ReleaseMT--i686-pc-linux2.4.23
    // There is a problem with ftds64_ctlib driver
    // on orkShop55_550-ReleaseMT
    if (args.GetDriverName() != "ftds" &&
        args.GetDriverName() != "msdblib") {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Query_Cancelation,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }


    if (args.GetServerType() == CTestArguments::eMsSql &&
        (args.GetDriverName() == "ftds64_odbc"
         || args.GetDriverName() == "ftds64_ctlib")
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Authentication,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    //
    add(BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Variant, DBAPIInstance));
    add(BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_CDB_Exception,
                              DBAPIInstance));

    if (!(args.GetDriverName() == "ftds64_ctlib"
          && args.GetServerType() == CTestArguments::eSybase) // Something
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Create_Destroy,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    //
    if (!(args.GetDriverName() == "ftds64_ctlib"
          && args.GetServerType() == CTestArguments::eSybase) // Something
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Multiple_Close,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    //
    boost::unit_test::test_case* tc_parameters =
        BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_StatementParameters,
                              DBAPIInstance);
    tc_parameters->depends_on(tc_init);
    add(tc_parameters);

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_GetRowCount,
                               DBAPIInstance);
    tc->depends_on(tc_init);
    tc->depends_on(tc_parameters);
    add(tc);

    // Doesn't work at the moment ...

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_NULL, DBAPIInstance);
    tc->depends_on(tc_init);
    tc->depends_on(tc_parameters);
    add(tc);

    {
        // Cursors work either with ftds + MSSQL or with ctlib at the moment ...
        // !!! It does not work in case of a new FTDS driver.
        if ((args.GetDriverName() == "ftds" &&
            args.GetServerType() == CTestArguments::eMsSql) ||
            args.GetDriverName() == "ctlib" ||
            // args.GetDriverName() == "dblib" || // Code will hang up with dblib for some reason ...
            args.GetDriverName() == "ftds63" ||
            args.GetDriverName() == "odbc" ||
            args.GetDriverName() == "odbcw" ||
            // args.GetDriverName() == "msdblib" || // doesn't work ...
            args.GetDriverName() == "ftds64_odbc" ||
            args.GetDriverName() == "ftds64_ctlib" ||
            args.GetDriverName() == "ftds64_dblib" ) {
            //
            boost::unit_test::test_case* tc_cursor =
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Cursor,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            tc->depends_on(tc_parameters);
            add(tc);

            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Cursor2,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            tc->depends_on(tc_parameters);
            add(tc);

            // Does not work with all databases and drivers currently ...
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            tc->depends_on(tc_cursor);
            add(tc);

//             tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_LOB2,
//             DBAPIInstance);
//             tc->depends_on(tc_init);
//             tc->depends_on(tc_cursor);
//             add(tc);

            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BlobStream,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            tc->depends_on(tc_cursor);
            add(tc);
        }
    }

    // !!! Need to be fixed !!!
    if (args.GetDriverName() != "ftds64_ctlib" // Doesn't work for some reason ...
        && args.GetDriverName() != "ftds64_odbc" // No BCP at the moment ...
        && args.GetDriverName() != "odbcw" // No BCP at the moment ...
        && args.GetDriverName() != "ctlib" // Doesn't work for some reason ...
        && args.GetDriverName() != "msdblib" // Doesn't work for some reason ...
        && !(args.GetDriverName() == "ftds" &&
             args.GetServerType() == CTestArguments::eSybase)
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_BulkInsertBlob,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    {
        boost::unit_test::test_case* except_safety_tc =
            BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Exception_Safety,
                                  DBAPIInstance);
        except_safety_tc->depends_on(tc_init);
        add(except_safety_tc);

        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_UserErrorHandler,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        tc->depends_on(except_safety_tc);
        add(tc);
    }

    {
        boost::unit_test::test_case* select_stmt_tc =
            BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SelectStmt,
                                  DBAPIInstance);
        select_stmt_tc->depends_on(tc_init);
        add(select_stmt_tc);

        if ( args.GetServerType() == CTestArguments::eMsSql &&
             (args.GetDriverName() != "msdblib" &&
              args.GetDriverName() != "dblib")
             ) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SelectStmtXML,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            tc->depends_on(select_stmt_tc);
            add(tc);
        }

        // ctlib/ftds64_ctlib will work in case of protocol version 12.5 only
        // ftds + Sybaseand dblib won't work because of the early protocol versions.
        if (((args.GetDriverName() == "ftds" ||
              args.GetDriverName() == "ftds63" ||
              args.GetDriverName() == "odbc" ||
              args.GetDriverName() == "odbcw" ||
              args.GetDriverName() == "ftds64_odbc" ||
              // args.GetDriverName() == "ftds64_ctlib" || // !!! This driver won't work because it supports CS_VERSION_110 only. !!!
              args.GetDriverName() == "ftds64_dblib" ) &&
             args.GetServerType() == CTestArguments::eMsSql)
             // args.GetDriverName() == "ctlib" ||
            ) {
            tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Insert,
                                       DBAPIInstance);
            tc->depends_on(tc_init);
            add(tc);
        }
    }

    // FTDS driver doesn't work with GCC_401-DebugMT, GCC_340-ReleaseMT on
    // linux for some reason.
    if (args.GetDriverName() != "ftds") {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Procedure,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Variant2, DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_GetTotalColumns,
                               DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

    if ( (args.GetDriverName() == "ftds"
          || args.GetDriverName() == "ftds63"
          || args.GetDriverName() == "ftds64_dblib"
          // || args.GetDriverName() == "ftds64_ctlib" // Need to fix this some day ...
          // || args.GetDriverName() == "ftds64_odbc"  // This is a big problem ....
          ) &&
         args.GetServerType() == CTestArguments::eMsSql ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_UNIQUE, DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    if ( args.IsBCPAvailable()
         && args.GetDriverName() != "ftds64_ctlib"  // Something is broken in ftds64_ctlib ...
         && args.GetDriverName() != "msdblib"     // Just does'nt work for some reason
         ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DateTimeBCP,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    // !!! There are still problems ...
    if ( args.GetDriverName() == "dblib"
         || args.GetDriverName() == "ftds"
         || args.GetDriverName() == "ftds63"
         || args.GetDriverName() == "ftds64_dblib"
         || args.GetDriverName() == "ftds64_ctlib"
         // || args.GetDriverName() == "ftds64_odbc" // No BCP at the moment ...
         ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Overflow,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    if (args.GetDriverName() == "odbcw"
//         args.GetDriverName() == "ftds63" ||
//         args.GetDriverName() == "ftds64_odbc" ||
//         || args.GetDriverName() == "ftds64_ctlib"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Unicode,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        tc->depends_on(tc_parameters);
        add(tc);
    }

    // The only problem with this test is that the LINK_OS server is not
    // available from time to time.
    if (args.GetServerType() == CTestArguments::eSybase &&
        args.GetDriverName() != "ftds"
        ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Iskhakov,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }


    if ( args.GetDriverName() != "ftds64_odbc"  // Strange ....
         ) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_DateTime,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    // Valid for all drivers ...
    {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Decimal,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_SetLogStream,
                               DBAPIInstance);
    tc->depends_on(tc_init);
    add(tc);

//     if (args.GetServerType() == CTestArguments::eMsSql
//         && args.GetDriverName() != "odbc" // Doesn't work ...
//         && args.GetDriverName() != "odbcw" // Doesn't work ...
//         // && args.GetDriverName() != "ftds64_odbc"
//         && args.GetDriverName() != "msdblib"
//         ) {
//         tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_NCBI_LS, DBAPIInstance);
//         add(tc);
//     }

    // development ....
    if (false) {
        tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_HasMoreResults,
                                   DBAPIInstance);
        tc->depends_on(tc_init);
        add(tc);
    }

    // !!! ctlib/dblib do not work at the moment.
    // !!! ftds works with MS SQL Server only at the moment.
//     if ( (args.GetDriverName() == "ftds" || args.GetDriverName() == "ftds63" ) &&
//          args.GetServerType() == CTestArguments::eMsSql ) {
//         tc = BOOST_CLASS_TEST_CASE(&CDBAPIUnitTest::Test_Bulk_Writing, DBAPIInstance);
//         tc->depends_on(tc_init);
//         add(tc);
//     }
}

CDBAPITestSuite::~CDBAPITestSuite(void)
{
}


////////////////////////////////////////////////////////////////////////////////
CTestArguments::CTestArguments(int argc, char * argv[]) :
    m_Arguments(argc, argv)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

    // Specify USAGE context
    arg_desc->SetUsageContext(m_Arguments.GetProgramBasename(),
                              "dbapi_unit_test");

    // Describe the expected command-line arguments
#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MS_DEV1"
#define DEF_DRIVER    "ftds"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "ftds63", "msdblib", "odbc", \
                      "gateway", "ftds64_dblib", "ftds64_odbc", "ftds64_ctlib", \
                      "odbcw"
#elif defined(HAVE_LIBSYBASE)
#define DEF_SERVER    "OBERON"
#define DEF_DRIVER    "ctlib"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "ftds63", "gateway", \
                      "ftds64_dblib", "ftds64_odbc", "ftds64_ctlib"
#else
#define DEF_SERVER    "MS_DEV1"
#define DEF_DRIVER    "ftds"
#define ALL_DRIVERS   "ftds", "ftds63", "gateway", \
                      "ftds64_dblib", "ftds64_odbc", "ftds64_ctlib"
#endif

    arg_desc->AddDefaultKey("S", "server",
                            "Name of the SQL server to connect to",
                            CArgDescriptions::eString, DEF_SERVER);

    arg_desc->AddDefaultKey("d", "driver",
                            "Name of the DBAPI driver to use",
                            CArgDescriptions::eString,
                            DEF_DRIVER);
    arg_desc->SetConstraint("d", &(*new CArgAllow_Strings, ALL_DRIVERS));

    arg_desc->AddDefaultKey("U", "username",
                            "User name",
                            CArgDescriptions::eString, "anyone");

    arg_desc->AddDefaultKey("P", "password",
                            "Password",
                            CArgDescriptions::eString, "allowed");
    arg_desc->AddDefaultKey("D", "database",
                            "Name of the database to connect",
                            CArgDescriptions::eString,
                            "DBAPI_Sample");

    arg_desc->AddOptionalKey("v", "version",
                            "TDS protocol version",
                            CArgDescriptions::eInteger);

    auto_ptr<CArgs> args_ptr(arg_desc->CreateArgs(m_Arguments));
    const CArgs& args = *args_ptr;


    // Get command-line arguments ...
    m_DriverName    = args["d"].AsString();
    m_ServerName    = args["S"].AsString();
    m_UserName      = args["U"].AsString();
    m_UserPassword  = args["P"].AsString();
    m_DatabaseName  = args["D"].AsString();
    if ( args["v"].HasValue() ) {
        m_TDSVersion = args["v"].AsString();
    } else {
        m_TDSVersion.clear();
    }

    SetDatabaseParameters();
}


CTestArguments::EServerType
CTestArguments::GetServerType(void) const
{
    if ( NStr::CompareNocase(GetServerName(), "STRAUSS") == 0
         || NStr::CompareNocase(GetServerName(), "MOZART") == 0
         || NStr::CompareNocase(GetServerName(), "OBERON") == 0
         || NStr::CompareNocase(GetServerName(), "SCHUMANN") == 0
         || NStr::CompareNocase(GetServerName(), "BARTOK") == 0
         ) {
        return eSybase;
    } else if ( NStr::CompareNocase(GetServerName(), 0, 6, "MS_DEV") == 0
                || NStr::CompareNocase(GetServerName(), 0, 5, "MSSQL") == 0
                || NStr::CompareNocase(GetServerName(), 0, 7, "OAMSDEV") == 0
                || NStr::CompareNocase(GetServerName(), 0, 6, "QMSSQL") == 0
                ) {
        return eMsSql;
    }

    return eUnknown;
}


string
CTestArguments::GetProgramBasename(void) const
{
    return m_Arguments.GetProgramBasename();
}


bool
CTestArguments::IsBCPAvailable(void) const
{
#if defined(NCBI_OS_SOLARIS)
    const bool os_solaris = true;
#else
    const bool os_solaris = false;
#endif

    if (os_solaris && HOST_CPU[0] == 'i' &&
        (GetDriverName() == "dblib" || GetDriverName() == "ctlib")
        ) {
        // Solaris Intel native Sybase drivers ...
        // There is no apropriate client
        return false;
    } else if ( GetDriverName() == "ftds64_odbc"
         || GetDriverName() != "odbcw"
         ) {
        return false;
    }

    return true;
}


void
CTestArguments::SetDatabaseParameters(void)
{
    if ( m_TDSVersion.empty() ) {
        if ( GetDriverName() == "ctlib" ) {
            // m_DatabaseParameters["version"] = "125";
        } else if ( GetDriverName() == "dblib"  &&
                    GetServerType() == eSybase ) {
            // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
            // BcpIn to it using protocol version other than "100".
            m_DatabaseParameters["version"] = "100";
        } else if ( (GetDriverName() == "ftds") &&
                    GetServerType() == eSybase ) {
            m_DatabaseParameters["version"] = "42";
        } else if ( (GetDriverName() == "ftds63" ||
                     GetDriverName() == "ftds64_dblib") &&
                    GetServerType() == eSybase ) {
            // ftds work with Sybase databases using protocol v42 only ...
            m_DatabaseParameters["version"] = "100";
        } else if (GetDriverName() == "ftds64_odbc") {
            if (GetServerType() == eSybase) {
                m_DatabaseParameters["version"] = "50";
            }
        } else if (GetDriverName() == "ftds64_ctlib"  &&
                   GetServerType() == eSybase) {
            m_DatabaseParameters["version"] = "125";
        }
    } else {
        m_DatabaseParameters["version"] = m_TDSVersion;
    }

    if ( (GetDriverName() == "ftds" ||
          GetDriverName() == "ftds63" ||
//           GetDriverName() == "ftds64_ctlib" ||
//           GetDriverName() == "ftds64_odbc" ||
          GetDriverName() == "ftds64_dblib") &&
        GetServerType() == eMsSql) {
        m_DatabaseParameters["client_charset"] = "UTF-8";
    }
}


END_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////////
test_suite*
init_unit_test_suite( int argc, char * argv[] )
{
    // Configure UTF ...
    // boost::unit_test_framework::unit_test_log::instance().set_log_format( "XML" );
    // boost::unit_test_framework::unit_test_result::set_report_format( "XML" );

    test_suite* test = BOOST_TEST_SUITE("DBAPI Unit Test.");

    test->add(new ncbi::CDBAPITestSuite(ncbi::CTestArguments(argc, argv)));

    return test;
}


