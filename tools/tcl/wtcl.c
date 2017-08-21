/*	$Header: /u/cvs/wtcl/wtcl.c,v 1.1.1.1 1996/11/12 21:28:09 cvs Exp $	*/

#include	<tcl.h>
#include	"wtcl.h"

extern int
Tcl_AppInit(
	Tcl_Interp	*interp
)
{
	if(Tcl_Init(interp) == TCL_ERROR)
		return TCL_ERROR ;

	if(Wily_Init(interp) == TCL_ERROR)
		return TCL_ERROR ;

	Tcl_SetVar(interp, "tcl_rcFileName", "~/.wtcl", TCL_GLOBAL_ONLY) ;

	return TCL_OK ;
}

extern int
main(
	int		argc,
	char	**argv
)
{
	Tcl_Main(argc, argv, Tcl_AppInit) ;
	return 0 ;
}
