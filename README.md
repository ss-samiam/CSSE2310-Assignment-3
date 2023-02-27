# CSSE2310-Assignment-3

This program was developed as part of a course (CSSE2310 - Computer Systems Principles and Programming) I undertook at the University of Queensland.

The program interacts with a UNIX file system and uses multiple processes to communicate, interactively spawns new processes, run programs, sends and receive signals and outputs, and manage the processes’ lifecycle.

### sigcat.c
``sigcat`` reads one line at a time from stdin, and immediate writes and flushes that line to an output stream. The output stream by default is stdout, however the output stream can be changed at runtime between stdout and stderr by sending sigcat the SIGUSR1 and SIGUSR2 signals.

### hq.c
``hq`` reads commands from its standard input one line at a time. All of ``hq``’s output goes to stdout– and all commands are terminated by a single newline. The program allows the user to run programs, send them signals, manage their input and output streams, report on their status and so on.
