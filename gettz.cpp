/*****************************************************************************

  Author:     Steve Marvin
  Program:    GetTZ.cpp
  Version:    1.1

  Created:    1.0 December 1997

  Purpose:    Parse the environment string TZ= and set the correct timezone
	      in the INI file of a "Time-Zone-Challenged" program. It's only
	      reason for existence is to eliminate the requirement of editing
	      a bunch of INI files and property pages every time the timezone
	      changes.

  LastMod:    1.1 January 1998

		 Changed setup function calculation for start and stop times
		 to use a more strict interpretation of the TZ string. When
		 the week/weekday mode is used it is now interpreted as
		 the n'th wday of the month, eg:

		     1'st Sunday, 3'rd Saturday from beginning or end.

		 Replaced borlands tzset() function with tzvalid(), tzset()
		 was not capable of correctly parsing the AAAhh:mm:ssBBB
		 format of the TZ string as specified by IBM.

  Arguments:  1. IniFileSpecification      (fully qualified path)
	      2. Application name for ini file entry
	      3. Key name for ini file entry

	      GetTz /?  prints a line explaining the syntax

  Returns:    The current time zone value (int)

  Written for Borland C++ version 2.0.

  Assumes:   1. global long variable "timezone" set by the runtime from
		the timezone number in the string using the runtime
		function tzset().

	     2. Syntax of the timezone string is as follows:

		SET TZ=IWT-2IST,3,3,5,0000,9,2,6,0000,3600

		where the various parts are:

		IST and IDT are arbitrary labels for the winter and summer
		clocks of your locality; the -2 between them means winter time
		here is two hours ahead of GMT.

		The next four numbers are respectively the month, week of the
		month, and day of the week on which the winter to summer
		transition takes place and the 24-hour time at which this
		happens. (Sunday is day 0 for this purpose.)

		The following four numbers are the same thing for the summer
		to winter transition.

		The last number is the number of seconds that the summer clock
		is advanced past the winter clock, here 3600 seconds -- one hour.

		You can omit any of the numbers that you don't need,
		e.g. if you don't need the summer-time advance, only hold
		the places with commas.

	     3. The default string here is EST5EDT,4,1,0,7200,10,-1,0,7200,3600

  Notes: This code was kept very simple, no fancy features of C used to make it
	 readable by anyone (so that you can see what is being done).

	 This is a one-time function, to implement this correctly one needs to
	 do the following:

	     1.  static struct {
		     long      dst_offset=3600;
		     long      base_zone=5*60*60;
		     long      curr_zone=5*60*60;
		     time_t    start_summer_t;
		     struct tm start_summer_blk;
		     time_t    stop_summer_t;
		     struct tm stop_summer_blk;
		     }

	      2. Init this from the environment string.
	      3. Replace all functions in the runtime that handle gmt
		 conversion and/or dst offset - in borland c these
		 occur in the following modules (TZSet, GMTime, TimeCvt
		 and UTime), eliminate any dst offset because the
		 curr_zone will always be correct.
	      4. Each call to localtime or gmtime should do the following:

		     If current time >= stop_summer_t
		     {
			if (curr_zone != base_zone)
			{
			    set curr_zone to base_zone
			    if start_summer_blk.tm_year == stop_summer_blk
			    {
			      start_summer_blk.tm_year++
			      recalc using mktime the start_summer_t;
			    }
			}
		     }
		     else if current time >= start_summer_t
		     {
			if (curr_zone == base_zone)
			{
			   set curr_zone = base_zone - dst_offset
			   if (start_summer_blk.tm_year != stop_summer_blk.tm_year)
			   {
			      stop_summer_blk.tm_year++;
			      recalc using mktime the stop_summer_t;
			   }
			}
		     }


******************************************************************************/

 // comment this out for final build, used for printf string output
 // #define DEBUG_OUT 1

 #include <ctype.h>

 #define INCL_WINSHELLDATA   /* Or use INCL_WIN or INCL_PM */
 #include <os2.h>

 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>
 #include <time.h>
 #include <mem.h>

#define  issign(c)   (((c) == '-') || ((c) == '+'))

#define  nextweekday(c) (c = (c==6) ? 0 : c+1)
#define  prevweekday(c) (c = (c==0) ? 6 : c-1)

/*  #define  _RUN_TIME_HANDLES_TZ   since the borland runtime does not
				    process the full TZ= line, this must
				    be commented out
*/

/* default to 1 hour the difference between summer and winter time           */
static int dst_offset=3600;

#ifndef _RUN_TIME_HANDLES_TZ

static char Days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
/* leap year difference in february is handled at run time below             */

/*****************************************************************************

  Get the next token from the environment string (delimited by a comma) and
  verify that it IS numeric.

******************************************************************************/
int next_numeric_token (int* pRet)
{
   int     vRet=FALSE, gotsign=FALSE;
   char    *pb;

   pb = strtok(NULL, ",");
   if (pb != NULL)
   {
       for (int i=0; i<strlen(pb); i++)
       {
	  if (!isdigit(pb[i]))
	  {
	     if (gotsign)
	     {
		return vRet;
	     }
	     else if (issign(pb[i]))
	     {
		gotsign = TRUE;
	     }
	     else
	     {
		return vRet;
	     }
	  }
       }
       *pRet = atoi(pb);
       vRet = TRUE;
   }

   return vRet;
}

/*****************************************************************************

  Extract one set of time information from the environment string in the
  format MONTH,WEEK,DAY,SECS.

     MONTH         Month number 1 to 11, January is 1
     WEEK          Week of the month to use
		      1,2,3,4    First, Second, Third, Fourth week
		      -1..-4     Last to First week
		      0          DAY is the number of day in month
     DAY           If WEEK is 0 this is the day number (1..31) to use
		   otherwise it is 0..6 with 0=Sunday and 6=Saturday
     SECS          Number of seconds since midnight.

  Args      year        Current year MOD 100 (eg 97 for 1997)
	    fday        Day (0..6) of first day of year (0=Sunday)
	    month       Default month
	    week        Default week
	    day         Default day
	    secs        Default seconds

  Returns   -1 or a valid time

******************************************************************************/
time_t check_one_set (int year, int fday, int month, int week, int day, int secs)
{
   time_t     retV=-1;
   struct tm  tblock;
   int        i, n;

   memset (&tblock, 0, sizeof(tblock));
   tblock.tm_year = year;

   if ( (next_numeric_token (&n)) && (n > 0) && (n < 13) )
   {
      month = n;
   }
   if ( (next_numeric_token (&n)) && (n > -5) && (n < 5) )
   {
      week = n;
   }
   if ( (next_numeric_token (&n)) && (n >= 0) && (n < 7) )
   {
      day = n;
   }
   if (next_numeric_token (&n))
   {
      secs = n;
   }

   tblock.tm_mon = month-1;

   #ifdef DEBUG_OUT
      printf ("check_one_set year=%d, month=%d, week=%d, day=%d, secs=%d\n", year, month, week, day, secs);
   #endif

   /* now find first DAY of this month */
   n = 0;
   for (i=0; i<tblock.tm_mon; i++)
   {
      n += Days[i];
   }
   n = n % 7;
   while (n != 0)
   {
      nextweekday(fday);
      n--;
   }

   #ifdef DEBUG_OUT
      printf ("              first day number=%d\n", fday);
   #endif

   if (week == 0)
   {
      /* use day as the absolute day number                        */
      tblock.tm_mday = day;
   }
   else
   {

/* original method found 'week' week and then 'day' day number
       if (week > 0)
       {
	  // 1..4 first second ... week in month
	  tblock.tm_mday = ((week-1)*7)+1;
       }
       else
       {
	  // -1..-4 first second ... week from end
	  week = -week;
	  tblock.tm_mday = ((4-week)*7)+1;
       }

       // tblock.tm_mday is now the first day of the week
       // use day to get the exact day number
       while (fday != day)
       {
	  tblock.tm_mday++;
	  nextweekday(fday);
       }
*/
// new method at version 1.1
       if (week > 0)
       {
	  // 1..4 first second ... week in month
	  // start at first day of month == 1
	  tblock.tm_mday = 1;

	  // Now move forward till the week'th day number
	  while ( (fday != day) && (week != 0) )
	  {
	     tblock.tm_mday++;
	     nextweekday(fday);
	     if (fday == day)
	     {
		week--;
	     }
	  }
       }
       else
       {
	  // -1..-4 first second ... week from end
	  // start at last day of the month
	  tblock.tm_mday = Days[tblock.tm_mon];

	  // get day of week of last day of month
	  n = tblock.tm_mday-1; // zero based not 1..31
	  while (n != 0)
	  {
	     nextweekday(fday);
	     n--;
	  }
   #ifdef DEBUG_OUT
      printf ("New fday=%d, tm_mday=%d\n", fday, tblock.tm_mday);
   #endif
	  // Now move backward till the week'th day number
	  while ( (fday != day) && (week != 0) )
	  {
	     tblock.tm_mday--;
	     prevweekday(fday);
	     if (fday == day)
	     {
		week++;
	     }
	  }
       }
   } // end else week != 0

   while (secs >= 60*60)
   {
      tblock.tm_hour++;
      secs -= (60*60);
   }
   while (secs >= 60)
   {
      tblock.tm_min++;
      secs -= 60;
   }
   tblock.tm_sec = secs;

   tblock.tm_isdst = -1;

   /* mktime fills in the remainder of the fields and returns the system time */
   retV = mktime(&tblock);

   #ifdef DEBUG_OUT
      printf ("       tm_*   mon=%d, mday=%d, wday=%d, yday=%d, hour=%d, min=%d, sec=%d, isdst?=%s\n",
	      tblock.tm_mon, tblock.tm_mday, tblock.tm_wday, tblock.tm_yday,
	      tblock.tm_hour, tblock.tm_min, tblock.tm_sec,
	      (tblock.tm_isdst)?"Yes":"No");
   #endif
   return retV;
}


/*****************************************************************************

  This function substitutes for the tzset() in the runtime, because borland
  does not correctly parse the TZ string missing the +/-hh:mm:ss option

  Returns   TRUE if the AAAnnBBB portion of the environment string is
	    valid (also sets timezone variable).

******************************************************************************/
int tzvalid(char* env){
   int   iret=0, n;
   char  c, str[_MAX_PATH], *pb;

   timezone=0;

   if ( (strlen(env) > 3) && isalpha(env[0]) && isalpha(env[1]) && isalpha(env[2]) )
   {
      /* leading part of string is valid three character standard time abbreviation */
      strcpy (str, env);
      str[2]=',';

      pb=&str[3];
      while ((c=pb[0]) != 0)
      {
	 if (isalpha(c))
	 {

	     pb[0] = 0;
	     pb++;
	     if ( (strlen(pb) < 2) || (!isalpha(pb[0])) || (!isalpha(pb[1])) )
	     {
		 #ifdef DEBUG_OUT
		    printf ("Trailing ascii grouping missing\n");
		 #endif
		 return iret;
	     }
	     break;
	 }
	 if (c == ':')
	 {
	    pb[0] = ',';
	 }
	 else if ( !isdigit(c) && !issign(c) )
	 {
	    #ifdef DEBUG_OUT
		printf ("Time zone number is not numeric\n");
	     #endif
	     return iret;	 }
	 pb++;
      }
      strtok (str, ",");

      /* string tokens now in format
	    nn
	    +nn
	    -nn
	    +/-hh,mm,ss
      */

     if (next_numeric_token(&n))
     {
	 timezone = (long)n*60L*60L;          /* hours converted to seconds   */
	 if (next_numeric_token(&n))
	 {
	    timezone += (long)n*60L;          /* minutes converted to seconds */
	    if (next_numeric_token(&n))
	    {
	       timezone += (long)n;           /* seconds                      */
	    }
	 }
	 iret = 1;
      }
      #ifdef DEBUG_OUT
	else
	{
	   printf ("Time zone number is not numeric\n");
	}
      #endif

   }
   #ifdef DEBUG_OUT
     else
     {
	printf ("First three character ascii grouping missing\n");
     }
     if (iret==1)
       printf ("TimeZone variable set to %ld\n", timezone);
   #endif

   return iret;
}


/*****************************************************************************

  This function substitutes for the _IsDst() in the runtime, because borland
  does not parse the entire TZ string and uses hard coded values.

  Returns   TRUE if the current local time is within the summer time
	    block, FALSE if it is outside (before or after).

******************************************************************************/
int is_it_dst (){
   time_t      currtime;
   struct tm   *ptm, tblock;
   char        str[_MAX_PATH], *pb, *env;
   int         n, fday;
   time_t      start, stop;

   /* first disable dst checking so that mktime calls work
      correctly - runtime was resetting 02:00 to 01:00 when
      on the dst boundary
   */
   daylight = FALSE;

   /* gets time of day                                    */
   currtime = time(NULL);

   /* converts date/time to a structure                   */
   ptm = localtime(&currtime);

   /* I have to parse the part of the string past EST5EDT */
   env = getenv("TZ");
   #ifdef DEBUG_OUT
       printf ("Raw TZ string=%s\n", env);
   #endif

   // parse check string and set timezone variable in time.h
   if (tzvalid(env))
   {
       strcpy (str, env);
   }
   else
   {
       /* default is first Sunday in April at 0200 start  */
       /* and last Sunday in October to end, 1hr offset   */
       strcpy (str,"EST5EDT,4,1,0,7200,10,-1,0,7200,3600");
       #ifdef DEBUG_OUT
	  printf ("Environment TZ incorrect, using default=%s\n", str);
       #endif
       timezone = 5*60L*60L;
   }   /* discard the first token "EST5EDT,"                  */
   strtok (str, ",");

   /* if ptm->tm_year is a leap year adjust Days[1] to 29 */
   n = ptm->tm_year + 1900;
   if ( ( ((ptm->tm_year % 4) == 0) && ((ptm->tm_year % 100) != 0) )
	|| ((ptm->tm_year % 400) == 0) )
   {
      Days[1]++;
   }

   /* find what day the first day of this year is                             */

   fday = ptm->tm_wday;
   n = ptm->tm_yday % 7;
   while (n != 0)
   {
      prevweekday(fday);
      n--;
   }
   #ifdef DEBUG_OUT
      printf ("Day number of first day of this year is %d\n", fday);
   #endif
   start = check_one_set (ptm->tm_year, fday, 4, 1, 0, 7200);
   if (start != -1)
   {
      /* valid dst start time, now parse and check stop  */
      stop = check_one_set(ptm->tm_year, fday, 10, -1, 0, 7200);
      if (stop != -1)
      {
	 /* start and stop are valid, get the dst offset */
	 /* it is already preset to the default 3600     */
	 if (next_numeric_token(&n))
	 {
	    dst_offset = n;
	 }

	 #ifdef DEBUG_OUT
	    printf ("Time zone offset in seconds=%lu\n", dst_offset);
	    printf ("Start time    %lu\n", start);
	    printf ("Current time  %lu\n", currtime);
	    printf ("Stop time     %lu\n", stop);
	    if ( (currtime >= start) && (currtime <= stop) )
	       printf ("In DST\n");
	    else
	       printf ("Not in DST\n");
	 #endif
	 return ( (currtime >= start) && (currtime <= stop) );
      }
   }

   return ptm->tm_isdst;
}

#else
/*****************************************************************************

  If the runtime correctly handles the entire TZ= string, this code is
  enough to tell if we are within summer time.

  Returns   TRUE if the current local time is within the summer time
	    block, FALSE if it is outside (before or after).

******************************************************************************/
int is_it_dst ()
{
   time_t      currtime;
   struct tm   *ptm;

   /* Set up timezone from environment string TZ=         */
   tzset();

   /* gets time of day                                    */
   currtime = time(NULL);

   /* converts date/time to a structure                   */
   ptm = localtime(&currtime);

   return ptm->tm_isdst;
}
#endif // _RUN_TIME_HANDLES_TZ


int main(int argc, char* argv[])
{
   long        lt;
   char	       str[_MAX_PATH], inifile[_MAX_PATH], *pb;
   int         s, isdst=FALSE;
   HAB         hab;         /* Anchor-block handle        */
   HINI        hini;        /* Initialization-file handle */
   FILE*       fp;


   /* adjust timezone if in daylight savings time         */
   if (is_it_dst())
   {
     isdst = TRUE;
     lt = dst_offset - timezone;
   }
   else
   {
      lt = -timezone;
   }

   /* convert it from seconds to hours                    */
   lt /= (60L*60L);

   switch (argc)
   {
      case 0 :
      case 1 :
	printf ("SET TZ=%s\n", getenv("TZ"));
	printf ("TimeZone is %d, ", (int)lt);
	printf ("%s Time\n", (isdst) ? "Summer(Daylight Savings)" : "Winter(Standard)");
	printf ("GetTZ /h for more help\n");

	break;

      case 2 :
	 pb = argv[1];
	 if ( (pb[0] == '/') || (pb[0] == '-') )
	 {
	     pb++;
	 }
	 if ( (pb[0] == '?') || (toupper(pb[0]) == 'H') )
	 {
	     printf ("GetTZ version 1.1\n");
	     printf ("  Syntax:  'GetTZ IniFileSpec Application KeyName'\n");
	     printf ("  Example: 'GetTZ C:\\SOUTHSIDE\\PMMAIL\\PMMAIL.Ini GLOBAL TZOFFSET'\n");
	 }
	 break;

      // this was erroneously case 3 : in 1.0 preventing any ini file from being updated!
      default :
	 /* argv[1] is ini filespecification */
	 strcpy (inifile, argv[1]);
	 if ( ((s=strlen(inifile)) > 4) && (memicmp(&inifile[s-4], ".INI", 4)) )
	 {
	    if (inifile[strlen(inifile)-1] != '.')
	      strcat (inifile, ".");
	    strcat (inifile, "Ini");
	 }
	 if ((fp = fopen(inifile, "r")) == NULL)
	 {
	    sprintf (str, "File '%s' ", inifile);
	    perror(str);
	    break;
	 }
	 else
	 {
	    fclose(fp);
	 }
	 hini = PrfOpenProfile(hab, inifile);

	 if (hini != NULLHANDLE)
	 {
	     /* argv[2] is the application and argv[3] is the key */
	     PrfWriteProfileData(hini, argv[2], argv[3], &lt, sizeof(long));
	     PrfCloseProfile(hini);
	 }
	 break;
   }

   #ifdef DEBUG_OUT
      printf ("Time zone = %ld\n", lt);
   #endif

   return (int)lt;

}