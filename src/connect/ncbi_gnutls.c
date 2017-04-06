/* $Id$
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
 *   GNUTLS support for SSL in connection library
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_connssl.h"
#include "ncbi_priv.h"
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_gnutls.h>
#include <stdlib.h>

#ifdef HAVE_LIBGNUTLS

#  include <gnutls/gnutls.h>

#  if   defined(ENOTSUP)
#    define NCBI_NOTSUPPORTED  ENOTSUP
#  elif defined(ENOSYS)
#    define NCBI_NOTSUPPORTED  ENOSYS
#  else
#    define NCBI_NOTSUPPORTED  EINVAL
#  endif

#  if GNUTLS_VERSION_NUMBER < 0x020C00
#    ifdef HAVE_LIBGCRYPT

#      include <gcrypt.h>

#      if   defined(NCBI_POSIX_THREADS)

#        include <pthread.h>
#        ifdef __cplusplus
extern "C" {
#        endif /*__cplusplus*/
    GCRY_THREAD_OPTION_PTHREAD_IMPL;
#        ifdef __cplusplus
} /* extern "C" */
#        endif /*__cplusplus*/

#      elif defined(NCBI_THREADS)

#        ifdef __cplusplus
extern "C" {
#        endif /*__cplusplus*/
static int gcry_user_mutex_init(void** lock)
{
    return !(*lock = MT_LOCK_AddRef(CORE_GetLOCK())) ? NCBI_NOTSUPPORTED : 0;
}
static int gcry_user_mutex_destroy(void** lock)
{
    MT_LOCK_Delete(*((MT_LOCK*) lock));
    *lock = 0;
    return 0;
}
static int gcry_user_mutex_lock(void** lock)
{
    return MT_LOCK_Do((MT_LOCK)(*lock), eMT_Lock) > 0 ? 0 : NCBI_NOTSUPPORTED;
}
static int gcry_user_mutex_unlock(void** lock)
{
    return MT_LOCK_Do((MT_LOCK)(*lock), eMT_Unlock) ? 0 : NCBI_NOTSUPPORTED;
}
static struct gcry_thread_cbs gcry_threads_user = {
    GCRY_THREAD_OPTION_USER, NULL/*gcry_user_init*/,
    gcry_user_mutex_init, gcry_user_mutex_destroy,
    gcry_user_mutex_lock, gcry_user_mutex_unlock,
    NULL/*all other fields NULL-inited*/
};
#        ifdef __cplusplus
} /* extern "C" */
#        endif /*__cplusplus*/

#      endif /*NCBI_..._THREADS*/

#    endif /*HAVE_LIBGCRYPT*/
#  elif defined(NCBI_THREADS)
#    ifdef __cplusplus
extern "C" {
#    endif /*__cplusplus*/
static int gtls_user_mutex_init(void** lock)
{
    return !(*lock = MT_LOCK_AddRef(CORE_GetLOCK())) ? NCBI_NOTSUPPORTED : 0;
}
static int gtls_user_mutex_deinit(void** lock)
{
    MT_LOCK_Delete((MT_LOCK)(*lock));
    *lock = 0;
    return 0;
}
static int gtls_user_mutex_lock(void** lock)
{
    return MT_LOCK_Do((MT_LOCK)(*lock), eMT_Lock) > 0 ? 0 : NCBI_NOTSUPPORTED;
}
static int gtls_user_mutex_unlock(void** lock)
{
    return MT_LOCK_Do((MT_LOCK)(*lock), eMT_Unlock) ? 0 : NCBI_NOTSUPPORTED;
}
#    ifdef __cplusplus
}
#    endif /*__cplusplus*/
#  endif


#  ifdef __cplusplus
extern "C" {
#  endif /*__cplusplus*/

static EIO_Status  s_GnuTlsInit  (FSSLPull pull, FSSLPush push);
static void*       s_GnuTlsCreate(ESOCK_Side side, SOCK sock, const char* host,
                                  NCBI_CRED cred, int* error);
static EIO_Status  s_GnuTlsOpen  (void* session, int* error, char** desc);
static EIO_Status  s_GnuTlsRead  (void* session,       void* buf,  size_t size,
                                  size_t* done, int* error);
static EIO_Status  s_GnuTlsWrite (void* session, const void* data, size_t size,
                                  size_t* done, int* error);
static EIO_Status  s_GnuTlsClose (void* session, int how, int* error);
static void        s_GnuTlsDelete(void* session);
static void        s_GnuTlsExit  (void);
static const char* s_GnuTlsError (void* session, int error,
                                  char* buf, size_t size);

static void        x_GnuTlsLogger(int level, const char* message);
static ssize_t     x_GnuTlsPull  (gnutls_transport_ptr_t,       void*, size_t);
static ssize_t     x_GnuTlsPush  (gnutls_transport_ptr_t, const void*, size_t);

#  ifdef __cplusplus
}
#  endif /*__cplusplus*/


#  if LIBGNUTLS_VERSION_NUMBER < 0x030306
static const int kGnuTlsCertPrio[] = {
    GNUTLS_CRT_X509,
    /*GNUTLS_CRT_OPENPGP,*/
    0
};
static const int kGnuTlsCompPrio[] = {
    GNUTLS_COMP_ZLIB,
    GNUTLS_COMP_NULL,
    0
};
#  endif /*LIBGNUTLS_VERSION_NUMBER<3.3.6*/


static int                              s_GnuTlsLogLevel;
static gnutls_anon_client_credentials_t s_GnuTlsCredAnon;
static gnutls_certificate_credentials_t s_GnuTlsCredCert;
static FSSLPull                         s_Pull;
static FSSLPush                         s_Push;


static void x_GnuTlsLogger(int level, const char* message)
{
    /* do some basic filtering and EOL cut-offs */
    size_t len = message ? strlen(message) : 0;
    if (!len  ||  *message == '\n')
        return;
    if (strncasecmp(message, "ASSERT: ", 8) == 0)
        return;
    if (message[len - 1] == '\n')
        --len;
    CORE_LOGF(eLOG_Note, ("GNUTLS%d: %.*s", level, (int) len, message));
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static EIO_Status x_RetryStatus(SOCK sock, EIO_Event direction)
{
    EIO_Status status;
    if (direction == eIO_Open) {
        EIO_Status r_status = SOCK_Status(sock, eIO_Read);
        EIO_Status w_status = SOCK_Status(sock, eIO_Write);
        status = r_status > w_status ? r_status : w_status;
    } else
        status = SOCK_Status(sock, direction);
    return status == eIO_Success ? eIO_Timeout : status;
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static EIO_Status x_ErrorToStatus(int* error, gnutls_session_t session,
                                  EIO_Event direction)
{
    SOCK       sock;
    EIO_Status status;

    assert(error  &&  *error <= 0/*GNUTLS_E_SUCCESS*/);

    if (!*error)
        return eIO_Success;
    sock = (SOCK) gnutls_transport_get_ptr(session);
    if      (*error == GNUTLS_E_AGAIN)
        status = x_RetryStatus(sock, direction);
    else if (*error == GNUTLS_E_INTERRUPTED)
        status = eIO_Interrupt;
    else if (*error == GNUTLS_E_WARNING_ALERT_RECEIVED) {
        status = eIO_Unknown;
        *error = GNUTLS_E_APPLICATION_ERROR_MAX - gnutls_alert_get(session);
    }
    else if (*error == GNUTLS_E_FATAL_ALERT_RECEIVED) {
        status = eIO_Closed;
        *error = GNUTLS_E_APPLICATION_ERROR_MAX - gnutls_alert_get(session);
    }
    else if (*error == GNUTLS_E_PULL_ERROR
             &&  sock->r_status != eIO_Success
             &&  sock->r_status != eIO_Unknown) {
        status = sock->r_status;
    }
    else if (*error == GNUTLS_E_PUSH_ERROR
             &&  sock->w_status != eIO_Success
             &&  sock->w_status != eIO_Unknown) {
        status = sock->w_status;
    }
    else if (gnutls_error_is_fatal(*error))
        status = eIO_Closed;
    else
        status = eIO_Unknown;

    CORE_LOGF(eLOG_Trace, ("GNUTLS error %d -> CONNECT status %s",
                           *error, IO_StatusStr(status)));

    return status;
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static int x_StatusToError(EIO_Status status, SOCK sock, EIO_Event direction)
{
    int error;

    assert(status != eIO_Success);
    assert(direction == eIO_Read  ||  direction == eIO_Write);

    switch (status) {
    case eIO_Timeout:
        error = EAGAIN;
        break;
    case eIO_Closed:
        error = SOCK_ENOTCONN;
        break;
    case eIO_Interrupt:
        error = SOCK_EINTR;
        break;
    case eIO_NotSupported:
        error = NCBI_NOTSUPPORTED;
        break;
    case eIO_Unknown:
        error = 0/*keep*/;
        break;
    default:
        /*NB:eIO_InvalidArg*/
        error = EINVAL;
        break;
    }

    {{
        const char* x_what = error ? "error" : "errno";
        int        x_error = error ?  error  :  errno;
        CORE_LOGF(eLOG_Trace, ("CONNECT GNUTLS status %s -> %s %d",
                               IO_StatusStr(status), x_what, x_error));
        if (!error)
            errno = x_error; /* restore errno that may be clobbered by log */
    }}

    return error;
}


static void* s_GnuTlsCreate(ESOCK_Side side, SOCK sock, const char* host,
                            NCBI_CRED cred, int* error)
{
    gnutls_transport_ptr_t  ptr = (gnutls_transport_ptr_t) sock;
    gnutls_connection_end_t end = (side == eSOCK_Client
                                   ? GNUTLS_CLIENT
                                   : GNUTLS_SERVER);
    gnutls_certificate_credentials_t xcred;
    gnutls_anon_client_credentials_t acred;
    gnutls_session_t session;
    char val[128];
    size_t len;
    int err;

    if (end == GNUTLS_SERVER) {
        CORE_LOG(eLOG_Error, "Server-side SSL not yet supported with GNUTLS");
        *error = 0;
        return 0;
    }

    CORE_LOCK_READ;
    xcred = s_GnuTlsCredCert;
    acred = s_GnuTlsCredAnon;
    CORE_UNLOCK;

    if (!acred
        ||  (cred  &&  (cred->type != eNcbiCred_GnuTls  ||  !cred->data))) {
        CORE_LOGF(eLOG_Error, ("Cannot %s GNUTLS credentials: %s",
                               acred ? "use"            : "set",
                               acred ? "Invalid format" : "Not initialized"));
        /*FIXME: there's a NULL(data)-terminated array of credentials */
        *error = 0;
        return 0;
    }

    if ((*error = gnutls_init(&session, end)) != GNUTLS_E_SUCCESS/*0*/)
        return 0;

    ConnNetInfo_GetValue(0, "GNUTLS_PRIORITY", val, sizeof(val), 0);

    len = host ? strlen(host) : 0;

    if ((err = gnutls_set_default_priority(session))                   != 0  ||
#  if LIBGNUTLS_VERSION_NUMBER >= 0x020200
        ( *val  &&
         (err = gnutls_priority_set_direct(session, val, 0))           != 0) ||
#  endif /*LIBGNUTLS_VERSION_NUMBER>=2.2.0*/
#  if LIBGNUTLS_VERSION_NUMBER < 0x030306
        (!*val  &&
         (err = gnutls_compression_set_priority(session,
                                                kGnuTlsCompPrio))      != 0) ||
        (!*val  &&
         (err = gnutls_certificate_type_set_priority(session,
                                                     kGnuTlsCertPrio)) != 0) ||
#  endif /*LIBGNUTLS_VERSION_NUMBER<3.3.6*/
        (err = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
                                      cred ? cred->data : xcred))      != 0  ||
        (err = gnutls_credentials_set(session, GNUTLS_CRD_ANON, acred))!= 0  ||
        (len  &&  (err = gnutls_server_name_set(session, GNUTLS_NAME_DNS,
                                                host, len))            != 0)) {
        gnutls_deinit(session);
        *error = err;
        return 0;
    }

    gnutls_transport_set_pull_function(session, x_GnuTlsPull);
    gnutls_transport_set_push_function(session, x_GnuTlsPush);
    gnutls_transport_set_ptr(session, ptr);
    gnutls_session_set_ptr(session, sock);

#  if LIBGNUTLS_VERSION_NUMBER >= 0x030000
    gnutls_handshake_set_timeout(session, 0);
#  endif /*LIBGNUTLS_VERSION_NUMBER>=3.0.0*/

    return session;
}


static EIO_Status s_GnuTlsOpen(void* session, int* error, char** desc)
{
    EIO_Status status;
    int x_error;

    *desc = 0;

    do {
        x_error = gnutls_handshake((gnutls_session_t) session);
    } while (x_error  &&  x_error == GNUTLS_E_REHANDSHAKE);

    if (x_error < 0) {
        status = x_ErrorToStatus(&x_error, (gnutls_session_t) session,
                                 eIO_Open);
        assert(status != eIO_Success);
        *error = x_error;
    } else {
#  if LIBGNUTLS_VERSION_NUMBER >= 0x030110
        char* temp = gnutls_session_get_desc(session);
        if (temp) {
            *desc = strdup(temp);
            gnutls_free(temp);
        }
#  endif /*LIBGNUTLS_VERSION_NUMBER>=3.1.10*/
        status = eIO_Success;
    }

    return status;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_IfToLog(void)
{
    return 7 < s_GnuTlsLogLevel  &&  s_GnuTlsLogLevel <= 10 ? 1/*T*/ : 0/*F*/;
}


/*ARGSUSED*/
#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static void x_set_errno(gnutls_session_t session, int error)
{
    assert(session);
#  if LIBGNUTLS_VERSION_NUMBER >= 0x010504
    gnutls_transport_set_errno(session, error);
#  else
    if (error)
        errno = error;
#  endif /*LIBGNUTLS_VERSION>=1.5.4*/
}


static ssize_t x_GnuTlsPull(gnutls_transport_ptr_t ptr,
                            void* buf, size_t size)
{
    int x_error;
    EIO_Status status;
    SOCK sock = (SOCK) ptr;
    FSSLPull pull = s_Pull;

    if (pull) {
        size_t x_read = 0;
        status = pull(sock, buf, size, &x_read, x_IfToLog());
        if (x_read > 0  ||  status == eIO_Success/*&& x_read==0*/) {
            assert(status == eIO_Success);
            assert(x_read <= size);
            x_set_errno((gnutls_session_t) sock->session, 0);
            return x_read;
        }
    } else
        status = eIO_NotSupported;

    x_error = x_StatusToError(status, sock, eIO_Read);
    if (x_error)
        x_set_errno((gnutls_session_t) sock->session, x_error);
    return -1;
}


static ssize_t x_GnuTlsPush(gnutls_transport_ptr_t ptr,
                            const void* data, size_t size)
{
    int x_error;
    EIO_Status status;
    SOCK sock = (SOCK) ptr;
    FSSLPush push = s_Push;

    if (push) {
        ssize_t n_written = 0;
        do {
            size_t x_written = 0;
            status = push(sock, data, size, &x_written, x_IfToLog());
            if (!x_written) {
                assert(!size  ||  status != eIO_Success);
                if (size  ||  status != eIO_Success)
                    goto out;
            } else {
                assert(status == eIO_Success);
                assert(x_written <= size);
                n_written += x_written;
                size      -= x_written;
                data       = (const char*) data + x_written;
            }
        } while (size);
        x_set_errno((gnutls_session_t) sock->session, 0);
        return n_written;
    } else
        status = eIO_NotSupported;

 out:
    x_error = x_StatusToError(status, sock, eIO_Write);
    if (x_error)
        x_set_errno((gnutls_session_t) sock->session, x_error);
    return -1;
}


static EIO_Status x_InitLocking(void)
{
    EIO_Status status;

#  if GNUTLS_VERSION_NUMBER < 0x020C00
#    ifdef HAVE_LIBGCRYPT
#      if   defined(NCBI_POSIX_THREADS)
    status = gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread) == 0
        ? eIO_Success
        : eIO_NotSupported;
#      elif defined(NCBI_THREADS)
    MT_LOCK lk = CORE_GetLOCK();
    if (MT_LOCK_Do(lk, eMT_Lock) != -1) {
        status = gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_user) == 0
            ? eIO_Success
            : eIO_NotSupported;
        MT_LOCK_Do(lk, eMT_Unlock);
    } else
        status = lk ? eIO_Success : eIO_NotSupported;
#      elif !defined(NCBI_NO_THREADS)  &&  defined(_MT)
    CORE_LOG(eLOG_Critical,"LIBGCRYPT uninitialized: Unknown threading model");
    status = eIO_NotSupported;
#      endif /*NCBI_POSIX_THREADS*/
#    endif /*HAVE_LIBGCRYPT*/
#  elif defined(NCBI_THREADS)
    MT_LOCK lk = CORE_GetLOCK();
    if (MT_LOCK_Do(lk, eMT_Lock) != -1) {
        /* Note that this call causes global_init/global_deinit to be called */
        gnutls_global_set_mutex(gtls_user_mutex_init, gtls_user_mutex_deinit,
                                gtls_user_mutex_lock, gtls_user_mutex_unlock);
        MT_LOCK_Do(lk, eMT_Unlock);
        status = eIO_Success;
    } else
        status = lk ? eIO_Success : eIO_NotSupported;
#  elif !defined(NCBI_NO_THREADS)  &&  defined(_MT)
    CORE_LOG(eLOG_Critical,"GNUTLS locking uninited: Unknown threading model");
    status = eIO_NotSupported;
#  else
    status = eIO_Success;
#  endif

    return status;
}


static EIO_Status s_GnuTlsRead(void* session, void* buf, size_t n_todo,
                               size_t* n_done, int* error)
{
    EIO_Status status;
    ssize_t    x_read;

    assert(session);

    x_read = gnutls_record_recv((gnutls_session_t) session, buf, n_todo);
    assert(x_read < 0  ||  (size_t) x_read <= n_todo);

    if (x_read <= 0) {
        int x_error = (int) x_read;
        status = x_ErrorToStatus(&x_error, (gnutls_session_t) session,
                                 eIO_Read);
        *error = x_error;
        x_read = 0;
    } else
        status = eIO_Success;

    *n_done = x_read;
    return status;
}


static EIO_Status x_GnuTlsWrite(void* session, const void* data, size_t n_todo,
                                size_t* n_done, int* error)
{
    EIO_Status status;
    ssize_t    x_written;

    assert(session);

    x_written = gnutls_record_send((gnutls_session_t) session, data, n_todo);
    assert(x_written < 0  ||  (size_t) x_written <= n_todo);

    if (x_written <= 0) {
        int x_error = (int) x_written;
        status = x_ErrorToStatus(&x_error, (gnutls_session_t) session,
                                 eIO_Write);
        *error = x_error;
        x_written = 0;
    } else
        status = eIO_Success;

    *n_done = x_written;
    return status;
}


static EIO_Status s_GnuTlsWrite(void* session, const void* data, size_t n_todo,
                                size_t* n_done, int* error)
{
    size_t max_size = gnutls_record_get_max_size((gnutls_session_t) session);
    EIO_Status status;

    *n_done = 0;

    do {
        size_t x_todo = n_todo > max_size ? max_size : n_todo;
        size_t x_done;
        status = x_GnuTlsWrite(session, data, x_todo, &x_done, error);
        assert((status == eIO_Success) == (x_done > 0));
        assert(status == eIO_Success  ||  *error);
        assert(x_done <= x_todo);
        if (status != eIO_Success)
            break;
        *n_done += x_done;
        if (x_todo != x_done)
            break;
        n_todo  -= x_done;
        data     = (const char*) data + x_done;
    } while (n_todo);

    return *n_done ? eIO_Success : status;
}


static EIO_Status s_GnuTlsClose(void* session, int how, int* error)
{
    int x_error;

    assert(session);

    x_error = gnutls_bye((gnutls_session_t) session,
                         how == SOCK_SHUTDOWN_RDWR
                         ? GNUTLS_SHUT_RDWR
                         : GNUTLS_SHUT_WR);
    if (x_error != GNUTLS_E_SUCCESS) {
        *error = x_error;
        return eIO_Unknown;
    }

    return eIO_Success;
}


static void s_GnuTlsDelete(void* session)
{
    assert(session);

    gnutls_deinit((gnutls_session_t) session);
}


/* NB: Called under a lock */
static EIO_Status s_GnuTlsInit(FSSLPull pull, FSSLPush push)
{
    gnutls_anon_client_credentials_t acred;
    gnutls_certificate_credentials_t xcred;
    const char* version;
    EIO_Status status;
    const char* val;
    char buf[32];

    assert(!s_GnuTlsCredAnon);

    version = gnutls_check_version(0);
    if (strcasecmp(GNUTLS_VERSION, version) != 0) {
        CORE_LOGF(eLOG_Critical,
                  ("GNUTLS version mismatch: %s headers vs. %s runtime",
                   GNUTLS_VERSION, version));
        assert(0);
    }

    if (!pull  ||  !push)
        return eIO_InvalidArg;

    val = ConnNetInfo_GetValue(0, "TLS_LOGLEVEL", buf, sizeof(buf), 0);
    CORE_LOCK_READ;
    if (!val  ||  !*val)
        val = getenv("GNUTLS_DEBUG_LEVEL"); /* GNUTLS proprietary setting */
    if (val  &&  *val) {
        ELOG_Level level;
        s_GnuTlsLogLevel = atoi(val);
        CORE_UNLOCK;
        if (s_GnuTlsLogLevel) {
            gnutls_global_set_log_function(x_GnuTlsLogger);
            if (val == buf)
                gnutls_global_set_log_level(s_GnuTlsLogLevel);
            level = eLOG_Note;
		} else
            level = eLOG_Trace;
        CORE_LOGF(level, ("GNUTLS V%s (LogLevel=%d)",
                          version, s_GnuTlsLogLevel));
    } else
        CORE_UNLOCK;

    if ((status = x_InitLocking()) != eIO_Success)
        goto out;

    if (!gnutls_check_version(LIBGNUTLS_VERSION)  ||
        gnutls_global_init() != GNUTLS_E_SUCCESS/*0*/) {
        status = eIO_NotSupported;
        goto out;
    }
    if (gnutls_anon_allocate_client_credentials(&acred) != 0) {
        gnutls_global_deinit();
        status = eIO_Unknown;
        goto out;
    }
    if (gnutls_certificate_allocate_credentials(&xcred) != 0) {
        gnutls_anon_free_client_credentials(acred);
        gnutls_global_deinit();
        status = eIO_Unknown;
        goto out;
    }

    s_GnuTlsCredAnon = acred;
    s_GnuTlsCredCert = xcred;
    s_Pull           = pull;
    s_Push           = push;

    return eIO_Success;

 out:
    gnutls_global_set_log_level(s_GnuTlsLogLevel = 0);
    gnutls_global_set_log_function(0);
    return status;
}


/* NB: Called under a lock */
static void s_GnuTlsExit(void)
{
    gnutls_anon_client_credentials_t acred = s_GnuTlsCredAnon;
    gnutls_certificate_credentials_t xcred = s_GnuTlsCredCert;

    assert(acred);

    s_Push           = 0;
    s_Pull           = 0;
    s_GnuTlsCredCert = 0;
    s_GnuTlsCredAnon = 0;

    gnutls_certificate_free_credentials(xcred);
    gnutls_anon_free_client_credentials(acred);
    gnutls_global_deinit();

    gnutls_global_set_log_level(s_GnuTlsLogLevel = 0);
    gnutls_global_set_log_function(0);

    /* If GNUTLS is loaded as DLL, it still has init count 1, so make sure
     * cleanup worked completely (MSVC2015 ReleaseDLL build breaks) */
    gnutls_global_deinit();
}

 
static const char* s_GnuTlsError(void* session/*unused*/, int error,
                                 char* buf/*unused*/, size_t size/*unused*/)
{
    /* GNUTLS defines only negative error codes */
    return error >= 0 ? 0 : error < GNUTLS_E_APPLICATION_ERROR_MAX
        ? gnutls_alert_get_name(GNUTLS_E_APPLICATION_ERROR_MAX - error)
        : gnutls_strerror(error);
}


#else


/*ARGSUSED*/
static EIO_Status s_GnuTlsInit(FSSLPull unused_pull, FSSLPush unused_push)
{
    CORE_LOG(eLOG_Critical, "Unavailable feature GNUTLS");
    return eIO_NotSupported;
}


#endif /*HAVE_LIBGNUTLS*/


extern SOCKSSL NcbiSetupGnuTls(void)
{
    static const struct SOCKSSL_struct kGnuTlsOps = {
        s_GnuTlsInit
#ifdef HAVE_LIBGNUTLS
        , s_GnuTlsCreate
        , s_GnuTlsOpen
        , s_GnuTlsRead
        , s_GnuTlsWrite
        , s_GnuTlsClose
        , s_GnuTlsDelete
        , s_GnuTlsExit
        , s_GnuTlsError
#endif /*HAVE_LIBGNUTLS*/
    };
#ifndef HAVE_LIBGNUTLS
    CORE_LOG(eLOG_Warning, "Unavailable feature GNUTLS");
#endif /*!HAVE_LIBGNUTLS*/
    return &kGnuTlsOps;
}


extern NCBI_CRED NcbiCredGnuTls(void* xcred)
{
    struct SNcbiCred* cred = (NCBI_CRED) calloc(xcred ? 2 : 1, sizeof(*cred));
    if (cred  &&  xcred) {
        cred->type = eNcbiCred_GnuTls;
        cred->data = xcred;
    }
    return cred;
}
