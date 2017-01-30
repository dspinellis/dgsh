#!/bin/sh
#
# Patch gnulib to compile under Cygwin
# Must be executed after bootstrapping and before configuring gnulib
#
# See https://github.com/stilor/crosstool-ng/blob/f6ea9a68b26830f72f8f5242aba9b950f2e4da78/patches/gettext/0.19.7/140-Fix-Cygwin-sys-select.patch

# Exit unless running under Cygwin
uname -o | grep -q Cygwin || exit 0

# Patch the bootstrapped gnulib
patch lib/sys_select.in.h <<\EOF
diff --git a/gettext-tools/gnulib-lib/sys_select.in.h b/gettext-tools/gnulib-lib/sys_select.in.h
index d6d3f9f..7281144 100644
--- a/gettext-tools/gnulib-lib/sys_select.in.h
+++ b/gettext-tools/gnulib-lib/sys_select.in.h
@@ -81,8 +81,9 @@
    of 'struct timeval', and no definition of this type.
    Also, Mac OS X, AIX, HP-UX, IRIX, Solaris, Interix declare select()
    in <sys/time.h>.
-   But avoid namespace pollution on glibc systems.  */
-# ifndef __GLIBC__
+   But avoid namespace pollution on glibc systems and "unknown type
+   name" problems on Cygwin.  */
+# if !(defined __GLIBC__ || defined __CYGWIN__)
 #  include <sys/time.h>
 # endif
 
@@ -100,10 +101,11 @@
 #endif
 
 /* Get definition of 'sigset_t'.
-   But avoid namespace pollution on glibc systems.
+   But avoid namespace pollution on glibc systems and "unknown type
+   name" problems on Cygwin.
    Do this after the include_next (for the sake of OpenBSD 5.0) but before
    the split double-inclusion guard (for the sake of Solaris).  */
-#if !(defined __GLIBC__ && !defined __UCLIBC__)
+#if !((defined __GLIBC__ || defined __CYGWIN__) && !defined __UCLIBC__)
 # include <signal.h>
 #endif
 
EOF
