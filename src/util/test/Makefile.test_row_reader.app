# $Id$

APP = test_row_reader
SRC = test_row_reader

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost xncbi

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = test_row_reader.txt

WATCHERS = satskyse
