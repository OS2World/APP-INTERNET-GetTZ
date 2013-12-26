
/*
** GetTZ 1.1 freeware 1/1998
*/

Greetings.

GetTZ is a simple command line utility written to solve an annoying
problem. Every time change (twice a year), I have to change the time
zone in several applications, select settings, find the right page
and make the change. This is particularly annoying in mail and news
applications because the time stamp on the post/message includes
the timezone.

GetTZ parses the environment TZ string (Config.sys 'SET TZ=') for
the timezone, summer time offset and the start and stop times
for summer time (daylight savings time for USA). It then sets
the current time zone into the appropriate value in the Ini file.

For PMMail and PMINews, I set up a Rexx Exit that calls GetTZ.EXE
with the correct parameters each time the program is started.

Note that if you have an application running during the time change
it will probably not pick up the altered value from the ini file until
restarted.

To Use:

  1. Copy the GetTZ.EXE file to any directory in your PATH statement
     in config.sys (I use \UTILS\BIN\ for all my small utilities).

  2. Execute the program with no parameters, 'GetTZ' and observe the
     output on the screen.

        First line:  'SET TZ=MMMnMMM.....' is the environment string
                     found in your config.sys file, make sure it is
                     correct.
        Second line: 'TimeZone is nn, Winter(Standard) Time' or
                     'TimeZone is nn, Summer(Daylight) Time'
                     Make sure this is correct, to fix your
                     config.sys file setting you can use TZCalc.Exe
                     distributed free with TIME868.Zip.
        Third line:  'GetTz /h for more help'

  2A. Execute the program again with the correct parameters
      for either PMMail or PMINews (see paragraph below).
      If you see nothing on the screen, the ini file has
      been updated correctly. Otherwise there may be a
      file error message displayed, this means it could
      not find the ini file.
                     
  3. PMMAIL:

        Copy the SetIniTZ.cmd to your PMMail directory, then
        edit and correct the GetTZ line correcting the path:

        'GetTZ C:\SOUTHSDE\PMMAIL\PMMAIL.INI GLOBAL TZOFFSET'

        In PMMail select PMMAIL->Settings->Rexx and set the
        'program open exit' to:

            Enabled -      checked
            Execute in Foreground - unchecked (this will run in the background)
            Script - C:\SOUTHSDE\PMMAIL\SetIniTZ.cmd

  4. PMINEWS:

        Copy the SetIniTZ.cmd to your PMINEWS directory, then
        edit and correct the GetTZ line correcting the path:

        'GetTZ C:\SOUTHSDE\PMINEWS\PMINEWS.INI GLOBAL TZOFFSET'

        In PMINEWS select PMINEWS->Settings->Rexx Exits and set the
        'program open exit' to:

            Enabled -      checked
            Execute in Foreground - unchecked (this will run in the background)
            Script - C:\SOUTHSDE\PMINEWS\SetIniTZ.cmd

 Format of the SET TZ= line in your config.sys uses the IBM standard
 which is not exactly the same as the unix standard. Support for the
 unix standard using the environment string EMXTZ= may be added later.

            Default is EST5EDT,4,1,0,7200,10,-1,0,7200,3600

         	 where the various parts are:

	 EST and EDT are arbitrary labels for the winter and summer
	 clocks of your locality; the 5 between them means winter time
	 here is 5 hours behind GMT.

         NOTE : The number 5 can actually be in the format hh:mm:ss
                if your offset from GMT is not an even hour. Version
                1.1 handles this correctly.

	 The next four numbers are respectively the month, week of the
 	 month, and day of the week on which the winter to summer
	 transition takes place and the 24-hour time at which this
	 happens. (Sunday is day 0 for this purpose). The default shown
           here is the first Sunday in April at 02:00 hours.

	The following four numbers are the same thing for the summer
	to winter transition. The default shown here is the last Sunday
           in October at 02:00 hours.

	The last number is the number of seconds that the summer clock
	is advanced past the winter clock, here 3600 seconds -- one hour.

If you don't know the settings for your area, get the TZCALC.EXE that
comes with the freeware TIME868.EXE and use it.

Now every time you start PMMail and PMINews the timezone used by the
program to stamp messages will be correct whether you are in summer
or winter time or even if you move to another timezone (provided you
correct your Config.Sys entry<g>.

NOTE: PMMail and PMINews only read the ini file on startup, if you run
      this utility while they are running OR the timezone changes
      (for example at the DST switch time) while they are running,
      they will both be using the wrong timezone until they restart!
       
The program source is included gratis, you are free to do whatever you
want with it.           

History:

1.1 Fixes a bug that prevented setting the changes in the .ini file
    even though the string was parsed correctly and arguments were
    correct.

    Changes slightly the interpretation of the start and stop times in the
    SET TZ= string to more closely conform to the IBM standard. See the .cpp
    for details.

    Added support for hh:mm:ss format of the timezone offset number that
    is in-between the two three-character ascii strings.

