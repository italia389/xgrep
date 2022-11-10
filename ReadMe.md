Introduction
============
Xgrep is a fast file-searching utility that uses advanced regular expression patterns by default (similar to GNU egrep, but with
several enhancements) and automatically switches to a fast Boyer-Moore search if a pattern is plain text.  It accepts all the
standard grep switches and also uses fast I/O routines which provide for quick searches on large files.

Installation
============
Installation is very straightforward.  Change to the root of the source directory and do one of the following:

    On Linux:
        $ sudo ./xgrep-1.0.0.sh

    On macOS:
        $ open xgrep-1.0.0.pkg

This will install the xgrep binary and man pages into `/usr/local`.

Features
========
XRE has many features, some of which are not available in other implementations.  A partial list follows.

Advanced Regular Expressions
----------------------------
Xgrep uses the regular expression engine from the XRE library in enhanced mode for regular expression pattern matching by
default, which includes all syntax supported by GNU egrep plus enhanced features such as `\b` for a word boundary, `\1`, `\2`,
etc. for back references, `\t` for a tab character, `\u` for an upper-case character, and the  `?` modifier for non-greedy
repetitions, among others.

Fast Plain Text Searching
-------------------------
Xgrep automatically uses the Boyer-Moore search engine from the CXL library for any pattern that is plain text (which is the
fast Apostolico-Giancarlo variant).  A `-lit` switch is also provided to force pattern(s) to be interpreted as plain text.

Fast I/O
--------
In addition to fast matching algorithms, the fast I/O routines from the CXL library are also used so that very large files (for
example, a multi-gigabyte log file) can be searched very quickly.

Flexibility
-----------
Any filename argument can be specified as `-` to read data from standard input.  Additionally, a `-0` switch is available which,
when used in concert with the `print0` operator of the find(1) utility,  provides the ability to read a list of filenames from
standard input instead of using command-line arguments.

Conformance to Standards
------------------------
Xgrep supports all the standard grep switches, such as `-c`, `-h`, `-i`, `-l`, `-n`, and `-v`.

Runs On macOS and Linux
-----------------------
Xgrep can be used on macOS and all Linux platforms.  It contains precompiled libraries for both.  It should also be portable to
other Unix platforms as well with little or no modifications.

Free
----
Xgrep is released under the GNU General Public License (GPLv3).  See the file `License.txt` for details.

Distribution
============
The current distribution of xgrep may be obtained at `https://github.com/italia389/xgrep.git`.

Contact and Feedback
====================
User feedback is welcomed and encouraged.  If you have the time and interest, please contact Rick Marinelli
<italian389@yahoo.com> with your questions, comments, bug reports, likes, dislikes, or whatever you feel is worth mentioning.
Questions and feature requests are welcomed as well.  You may also report a bug by opening an issue on the GitHub page.

Notes
=====
This distribution of xgrep is version 1.0.0.  Installer packages containing 64-bit binaries are included for Linux platforms and
macOS ver. 10.12 and later.  The sources can be compiled instead if desired; however, the build process has not been tested on
other Unix platforms and there may be some (hopefully minor) issues which will need to be resolved.  If you are compiling the
sources and encounter any problems, please contact the author with the details.

Credits
=======
Xgrep (c) Copyright 2022 Richard W. Marinelli was created by Rick Marinelli <italian389@yahoo.com>.
