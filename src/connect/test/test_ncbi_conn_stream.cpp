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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Standard test for the CONN-based streams
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.19  2002/03/30 03:37:21  lavr
 * Added test for memory leak in unused connector
 *
 * Revision 6.18  2002/03/24 16:25:25  lavr
 * Changed "ray" -> "ray.nlm.nih.gov"
 *
 * Revision 6.17  2002/03/22 19:46:37  lavr
 * Test_assert.h made last among the include files
 *
 * Revision 6.16  2002/02/05 21:45:55  lavr
 * Included header files rearranged
 *
 * Revision 6.15  2002/01/28 20:28:28  lavr
 * Changed io_bounce.cgi -> bounce.cgi
 *
 * Revision 6.14  2002/01/16 21:23:15  vakatov
 * Utilize header "test_assert.h" to switch on ASSERTs in the Release mode too
 *
 * Revision 6.13  2001/12/30 19:45:50  lavr
 * 'static' added to every file-scope identifiers
 *
 * Revision 6.12  2001/07/24 18:00:30  lavr
 * New function introduced s_Read (instead of direct use of istream::read)
 * This function is compiler-dependent, and specially-featured for GCC
 *
 * Revision 6.11  2001/04/23 18:03:06  vakatov
 * Artificial cast to get rid of warning in the 64-bit compilation mode
 *
 * Revision 6.10  2001/03/27 23:39:16  lavr
 * Explicit cast to (char) added in buffer filling
 *
 * Revision 6.9  2001/03/24 00:50:06  lavr
 * Log typo correction
 *
 * Revision 6.8  2001/03/24 00:35:21  lavr
 * Two more tests added: randomly sized read test, and binary (-1) read test
 *
 * Revision 6.7  2001/03/22 19:19:17  lavr
 * Buffer size extended; random number generator seeded with current time
 *
 * Revision 6.6  2001/03/02 20:03:17  lavr
 * "../ncbi_priv.h" explained
 *
 * Revision 6.5  2001/01/25 17:12:01  lavr
 * Added: buffers and LOG freed upon program exit
 *
 * Revision 6.4  2001/01/23 23:21:03  lavr
 * Added proper logging
 *
 * Revision 6.3  2001/01/13 00:01:26  lavr
 * Changes in REG_cxx2c() prototype -> appropriate changes in the test
 * Explicit registry at the end
 *
 * Revision 6.2  2001/01/12 05:49:31  vakatov
 * Get rid of unused "argc", "argv" in main()
 *
 * Revision 6.1  2001/01/11 23:09:36  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* This header must go last */
#include "test_assert.h"


BEGIN_NCBI_SCOPE


static const size_t kBufferSize = 1024*1024;


static CNcbiRegistry* s_CreateRegistry(void)
{
    CNcbiRegistry* reg = new CNcbiRegistry;

    // Compose a test registry
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "TRUE");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_HOST,           "ray.nlm.nih.gov");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_PATH,       "/Service/bounce.cgi");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_ARGS,           "arg1+arg2+arg3");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_REQ_METHOD,     "POST");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_TIMEOUT,        "5.0");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "TRUE");
    return reg;
}


static size_t s_Read(iostream& ios, char* buffer, size_t size)
{
#ifdef NCBI_COMPILER_GCC
    size_t read = 0;
    for (;;) {
        ios.read(buffer, size);
        size_t count = ios.gcount();
        read += count;
        if (count == 0 && ios.eof())
            return read;
        buffer += count;
        size   -= count;
        if (!size)
            break;
        ios.clear();
    }
    return read;
#else
    ios.read(buffer, size);
    return ios.gcount();
#endif
}


END_NCBI_SCOPE


int main(void)
{
    USING_NCBI_SCOPE;
    size_t i, j, k, l;

    srand((unsigned int) time(0));

    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(s_CreateRegistry(), true));
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);

    LOG_POST(Info << "Checking error log setup"); // short explanatory mesg
    ERR_POST(Info << "Test log message using C++ Toolkit posting");
    CORE_LOG(eLOG_Note, "Another test message using C Toolkit posting");

    {{
        // Test for memory leaks in unused stream
        CConn_IOStream* s  =
            new CConn_ServiceStream("ID1", fSERV_Any);
        delete s;
    }}

    LOG_POST("Test 1 of 3: Big buffer bounce");
    CConn_HttpStream ios(0, "User-Header: My header\r\n",
                         fHCC_UrlEncodeArgs | fHCC_AutoReconnect);

    char *buf1 = new char[kBufferSize + 1];
    char *buf2 = new char[kBufferSize + 2];

    for (j = 0; j < kBufferSize/1024; j++) {
        for (i = 0; i < 1024 - 1; i++)
            buf1[j*1024 + i] = "0123456789"[rand() % 10];
        buf1[j*1024 + 1024 - 1] = '\n';
    }
    buf1[kBufferSize] = '\0';

    if (!(ios << buf1)) {
        ERR_POST("Error sending data");
        return 1;
    }

    size_t buflen = s_Read(ios, buf2, kBufferSize + 1);

    if (!ios.good() && !ios.eof()) {
        ERR_POST("Error receiving data");
        return 2;
    }

    LOG_POST(Info << buflen << " bytes obtained" <<
             (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0; i < kBufferSize; i++) {
        if (!buf2[i])
            break;
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
    else if (buflen > kBufferSize)
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
    else
        LOG_POST(Info << "Test 1 passed");

    // Clear EOF condition
    ios.clear();

    LOG_POST("Test 2 of 3: Random bounce");
    
    if (!(ios << buf1)) {
        ERR_POST("Error sending data");
        return 1;
    }

    j = 0;
    buflen = 0;
    for (i = 0, l = 0; i < kBufferSize; i += j, l++) {
        k = rand()%15 + 1;

        if (i + k > kBufferSize + 1)
            k = kBufferSize + 1 - i;
        j = s_Read(ios, &buf2[i], k);
        if (!ios.good() && !ios.eof()) {
            ERR_POST("Error receiving data");
            return 2;
        }
        if (j != k)
            LOG_POST("Bytes requested: " << k << ", received: " << j);
        buflen += j;
        l++;
    }

    LOG_POST(Info << buflen << " bytes obtained in " << l << " iteration(s)" <<
             (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0; i < kBufferSize; i++) {
        if (!buf2[i])
            break;
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
    else if (buflen > kBufferSize)
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
    else
        LOG_POST(Info << "Test 2 passed");

    // Clear EOF condition
    ios.clear();

    LOG_POST("Test 3 of 3: Truly binary bounce");

    for (i = 0; i < kBufferSize; i++)
        buf1[i] = (char)(255/*rand()%256*/);

    ios.write(buf1, kBufferSize);
    
    if (!ios.good()) {
        ERR_POST("Error sending data");
        return 1;
    }
    
    buflen = s_Read(ios, buf2, kBufferSize + 1);

    if (!ios.good() && !ios.eof()) {
        ERR_POST("Error receiving data");
        return 2;
    }

    LOG_POST(Info << buflen << " bytes obtained" <<
             (ios.eof() ? " (EOF)" : ""));

    for (i = 0; i < kBufferSize; i++) {
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
    else if (buflen > kBufferSize)
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
    else
        LOG_POST(Info << "Test 3 passed");
    CORE_SetREG(0);
    CORE_SetLOG(0);

    delete[] buf1;
    delete[] buf2;

    return 0/*okay*/;
}
