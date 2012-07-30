/*
    websuemf.c -- GoAhead Micro Embedded Management Framework

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include	"ejIntrn.h"
#include	"wsIntrn.h"

/*********************************** Defines **********************************/

/*
 *	This structure stores scheduled events.
 */
typedef struct {
	void	(*routine)(void *arg, int id);
	void	*arg;
	time_t	at;
	int		schedid;
} sched_t;

/*********************************** Locals ***********************************/

static sched_t		**sched;
static int			schedMax;

/************************************* Code ***********************************/
/*
  	Evaluate a script
 */

int scriptEval(int engine, char_t *cmd, char_t **result, void* chan)
{
	int		ejid;

	if (engine == EMF_SCRIPT_EJSCRIPT) {
		ejid = (int) chan;
		if (ejEval(ejid, cmd, result) ) {
			return 0;
		} else {
			return -1;
		}
	}
	return -1;
}

/*
    Compare strings, ignoring case:  normal strcmp return codes.
  
    WARNING: It is not good form to increment or decrement pointers inside a "call" to tolower et al. These can be
    MACROS, and have undesired side effects.
 */
int strcmpci(char_t *s1, char_t *s2)
{
	int		rc;

	a_assert(s1 && s2);
	if (s1 == NULL || s2 == NULL) {
		return 0;
	}

	if (s1 == s2) {
		return 0;
	}

	do {
		rc = gtolower(*s1) - gtolower(*s2);
		if (*s1 == '\0') {
			break;
		}
		s1++;
		s2++;
	} while (rc == 0);
	return rc;
}


/*
  	This function is called when a scheduled process time has come.
    MOB - why caps?  Static?
 */
void TimerProc(int schedid)
{
	sched_t	*s;

	a_assert(0 <= schedid && schedid < schedMax);
	s = sched[schedid];
	a_assert(s);

	(s->routine)(s->arg, s->schedid);
}


/*
  	Schedule an event in delay milliseconds time. We will use 1 second granularity for webServer.
 */
int emfSchedCallback(int delay, emfSchedProc *proc, void *arg)
{
	sched_t	*s;
	int		schedid;

	if ((schedid = hAllocEntry((void***) &sched, &schedMax,
		sizeof(sched_t))) < 0) {
		return -1;
	}
	s = sched[schedid];
	s->routine = proc;
	s->arg = arg;
	s->schedid = schedid;

    /*
      	Round the delay up to seconds.
     */
	s->at = ((delay + 500) / 1000) + time(0);
	return schedid;
}


/*
  	Reschedule to a new delay.
 */
void emfReschedCallback(int schedid, int delay)
{
	sched_t	*s;

	if (sched == NULL || schedid == -1 || schedid >= schedMax || (s = sched[schedid]) == NULL) {
		return;
	}
	s->at = ((delay + 500) / 1000) + time(0);
}


void emfUnschedCallback(int schedid)
{
	sched_t	*s;

	if (sched == NULL || schedid == -1 || schedid >= schedMax || (s = sched[schedid]) == NULL) {
		return;
	}
	bfree(B_L, s);
	schedMax = hFree((void***) &sched, schedid);
}


/*
  	Take tasks off the queue in a round robin fashion.
 */
void emfSchedProcess()
{
	sched_t		*s;
	int			schedid;
	static int	next = 0;	

    /*
        If schedMax is 0, there are no tasks scheduled, so just return.
     */
	if (schedMax <= 0) {
		return;
	}

    /*
      	If next >= schedMax, the schedule queue was reduced in our absence
      	so reset next to 0 to start from the begining of the queue again.
     */
	if (next >= schedMax) {
		next = 0;
	}

	schedid = next;
	for (;;) {
		if ((s = sched[schedid]) != NULL &&	(int)s->at <= (int)time(0)) {
			TimerProc(schedid);
			next = schedid + 1;
			return;
		}
		if (++schedid >= schedMax) {
			schedid = 0;
		}
		if (schedid == next) {
            /*
                We've gone all the way through the queue without finding anything to do so just return.
             */
			return;
		}
	}
}

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis GoAhead open source license or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
