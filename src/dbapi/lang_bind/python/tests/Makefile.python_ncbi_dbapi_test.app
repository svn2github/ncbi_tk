# $Id$

APP = python_ncbi_dbapi_test
SRC = python_ncbi_dbapi_test

REQUIRES = PYTHON Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(PYTHON_INCLUDE) $(BOOST_INCLUDE)

LIB  = dbapi_driver$(STATIC) xconnect xutil test_boost xncbi
LIBS = $(PYTHON_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = unix DLL_BUILD in-house-resources
CHECK_COPY = python_ncbi_dbapi_test.ini
CHECK_TIMEOUT = 300

CHECK_CMD = python_ncbi_dbapi_test -dr ctlib -S Sybase
CHECK_CMD = python_ncbi_dbapi_test -dr ctlib -S DBAPI_SYB160_TEST
CHECK_CMD = python_ncbi_dbapi_test -dr dblib -S Sybase
CHECK_CMD = python_ncbi_dbapi_test -dr dblib -S DBAPI_SYB160_TEST
CHECK_CMD = python_ncbi_dbapi_test -dr ftds64 -S Sybase
CHECK_CMD = python_ncbi_dbapi_test -dr ftds64 -S DBAPI_SYB160_TEST
CHECK_CMD = python_ncbi_dbapi_test -dr ftds64 -S MsSql
# CHECK_CMD = python_ncbi_dbapi_test -dr ftds64 -S DBAPI_MS2008_TEST
CHECK_CMD = python_ncbi_dbapi_test -dr ftds95 -S Sybase
CHECK_CMD = python_ncbi_dbapi_test -dr ftds95 -S DBAPI_SYB160_TEST
CHECK_CMD = python_ncbi_dbapi_test -dr ftds95 -S MsSql
CHECK_CMD = python_ncbi_dbapi_test -dr ftds95 -S MsSql -V 7.3
# CHECK_CMD = python_ncbi_dbapi_test -dr ftds95 -S DBAPI_MS2008_TEST
# CHECK_CMD = python_ncbi_dbapi_test -dr ftds95 -S DBAPI_MS2008_TEST -V 7.3
CHECK_CMD = python_ncbi_dbapi_test -dr odbc  -S MsSql
# CHECK_CMD = python_ncbi_dbapi_test -dr odbc  -S DBAPI_MS2008_TEST


WATCHERS = ucko
