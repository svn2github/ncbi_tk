#! /bin/sh
# $Id$

. ./ncbi_test_data

n="`ls -m $NCBI_TEST_DATA/proxy 2>/dev/null | wc -w`"
n="`expr ${n:-0} + 1`"
n="`expr $$ '%' $n`"

if [ -r $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n ]; then
  .     $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n
  proxy="$n"
fi

if [ -r $NCBI_TEST_DATA/x.509/test_ncbi_http_get ]; then
  .     $NCBI_TEST_DATA/x.509/test_ncbi_http_get
fi

if [ -z "$CONN_HTTP11" -a "`expr $$ '%' 2`" = "1" ]; then
  CONN_HTTP11=1
  export CONN_HTTP11
fi

if [ "`echo $FEATURES | grep -vic '[-]GNUTLS'`" = "1" ]; then
  # for netstat
  PATH=${PATH}:/sbin:/usr/sbin
  : ${CONN_USESSL:=1}
  : ${CONN_TLS_LOGLEVEL:=2}
  export PATH CONN_USESSL CONN_TLS_LOGLEVEL
  if [ -z "$proxy" -a "`netstat -a -n | grep -w 5556 | grep -c ':5556'`" != "0" ]; then
    url='https://localhost:5556'
  else
    url='https://www.ncbi.nlm.nih.gov/Service/index.html'
  fi
else
  url='http://intranet.ncbi.nlm.nih.gov/Service/index.html'
fi

: ${CONN_DEBUG_PRINTOUT:=SOME};  export CONN_DEBUG_PRINTOUT

$CHECK_EXEC test_ncbi_http_get "$url"
