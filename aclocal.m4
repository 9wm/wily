dnl AC_CHECK_HAVE_TYPE(TYPE)
AC_DEFUN(AC_CHECK_HAVE_TYPE,
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_type_$1,
[
AC_EGREP_CPP($1 *;,
[#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#endif], ac_cv_type_$1=yes, ac_cv_type_$1=no)])dnl
AC_MSG_RESULT($ac_cv_type_$1)
if test $ac_cv_type_$1 = yes; then
changequote(<,>)dnl
  ac_type=HAVE_`echo $1 | tr '[a-z]' '[A-Z]'`
changequote([,])dnl
  AC_DEFINE_UNQUOTED($ac_type)
fi
])
