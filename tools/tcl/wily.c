/*	$Header: /u/cvs/wtcl/wily.c,v 1.2 1996/11/12 23:33:58 cvs Exp $	*/

#include	<tcl.h>
#include	<u.h>
#include	<libc.h>
#include	<msg.h>
#include	<string.h>

/*
 *	dispatcher within Wily() routine
 */
typedef struct WCmd WCmd ;
struct WCmd {
	char	*name ;
	int		(*proc)(Tcl_Interp*, int, char**) ;
} ;

/*
 *	the handle to wily.  should be 0 if not connected
 */
static Handle	*h ;

/*
 *	the last event fetched by rpc_event() used
 *	by bounce -- we can only bounce if we haveevent
 */
static Bool	haveevent = false ;
static Msg	m ;
	
static Bool
iscon(
	void
)
{
	if(h == 0 || rpc_isconnected(h) == false) {
		h = 0 ;
		return false ;
	}
	return true ;
}

/*
 *	check args etc for Wily() sub command.
 *	check that argc == argswanted.
 *	if id != 0 then check that we have an id
 *	and return it as *id.
 *	check we are connected to wily.
 *	if anything fails then set the result string
 */
static int
chk(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv,
	int			argswanted,
	char		*usage,
	Id			*id
)
{
	int	n ;
	
	if(argc != argswanted) {
		Tcl_SetResult(interp, usage, TCL_STATIC) ;
		return TCL_ERROR ;
	}

	if(iscon() == false) {
		Tcl_SetResult(interp, "not connected", TCL_STATIC) ;
		return TCL_ERROR ;
	}

	if(id != 0) {
		if(Tcl_GetInt(interp, argv[1], &n) != TCL_OK) {
			Tcl_SetResult(interp, "arg doesn't look like id", TCL_STATIC) ;
			return TCL_ERROR ;
		}
		*id = n ;
	}
	return TCL_OK ;
}

static int
wrpc_init(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	int		fd ;
	
	if(argc != 1) {
		Tcl_SetResult(interp, "init requires no arguments", TCL_STATIC) ;
		return TCL_ERROR ;
	}

	fd = client_connect() ;
	if(fd < 0) {
		Tcl_SetResult(interp, "can't connect", TCL_STATIC) ;
		return TCL_ERROR ;
	}
	h = rpc_init(fd) ;
	if(h == 0) {
		Tcl_SetResult(interp, "can't init", TCL_STATIC) ;
		return TCL_ERROR ;
	}
	return TCL_OK ;
}

static int
wrpc_isconnected(
	Tcl_Interp	*interp,
	int			argc, 
	char		**argv
)
{
	if(argc != 1) {
		Tcl_AppendResult(interp, "isconnected requires no arguments", TCL_STATIC) ;
		return TCL_ERROR ;
	}
	if(iscon() == false) {
		Tcl_SetResult(interp, "0", TCL_STATIC) ;
		h = 0 ;
	} else
		Tcl_SetResult(interp, "1", TCL_STATIC) ;
	return TCL_OK ;
}

static int
wrpc_list(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	char	*p ;
	char	*emsg ;
	char	*t ;
	char	*s ;

	if(chk(interp, argc, argv, 1, "list requires no arguments", 0) != TCL_OK)
		return TCL_ERROR ;
		
	emsg = rpc_list(h, &p) ;
	if(emsg != 0) {
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}

	/*
	 *	we trust p to be in a reasonable condition
	 */
	t = strtok(p, "\n") ;
	while(t != 0 && (s = strchr(t, '\t')) != 0) {
		s++ ;
		Tcl_AppendElement(interp, s) ;
		t = strtok(0, "\n") ;
	}
	free(p) ;
	return TCL_OK ;
}

static int
wrpc_name(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	char	*p ;
	char	*emsg ;
	char	*t ;
	char	*s ;

	if(chk(interp, argc, argv, 2, "name requires id", 0) != TCL_OK)
		return TCL_ERROR ;
		
	emsg = rpc_list(h, &p) ;
	if(emsg != 0) {
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}

	/*
	 *	we trust p to be in a reasonable condition
	 */
	t = strtok(p, "\n") ;
	while(t != 0 && (s = strchr(t, '\t')) != 0) {
		*s++ = '\0' ;

		if(strcmp(argv[1], s) == 0) {
			Tcl_SetResult(interp, t, TCL_VOLATILE) ;
			free(p) ;
			return TCL_OK ;
		}
		t = strtok(0, "\n") ;
	}
	
	Tcl_SetResult(interp, "id not found", TCL_STATIC) ;
	free(p) ;
	return TCL_ERROR ;
}

static int
wrpc_new(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	char	*emsg ;
	Id		id ;
	
	if(chk(interp, argc, argv, 2, "new needs filename", 0) != TCL_OK)
		return TCL_ERROR ;
	
	emsg = rpc_new(h, argv[1], &id) ;
	if(emsg != 0) {
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}
	sprintf(interp->result, "%ld", (long)id) ;
	return TCL_OK ;
}

static int
wrpc_attach(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	Id		id ;
	char	*emsg ;
	
	if(chk(interp, argc, argv, 2, "attach needs id", &id) != TCL_OK)
		return TCL_ERROR ;

	emsg = rpc_attach(h, id, WEexec | WEgoto | WEdestroy | WEreplace) ;
	if(emsg != 0) {
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}

	return TCL_OK ;
}

static int
wrpc_setname(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	Id		id ;
	char	*emsg ;

	if(chk(interp, argc, argv, 3, "setname needs id and new name", &id) != TCL_OK)
		return TCL_ERROR ;
		
	emsg = rpc_setname(h, id, argv[2]) ;
	if(emsg != 0) {
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}
	return TCL_OK ;
}

static int
wrpc_settools(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	Id		id ;
	char	*emsg ;
		
	if(chk(interp, argc, argv, 3, "settools needs id and new text", &id) != TCL_OK)
		return TCL_ERROR ;

	emsg = rpc_settools(h, id, argv[2]) ;
	if(emsg != 0) {
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}
	return TCL_OK ;
}

static int
wrpc_read(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	Id		id ;
	char	*emsg ;
	Range	r ;
	char	*p ;
	int		n ;
	
	if(chk(interp, argc, argv, 4, "read needs id, begin and end", &id) != TCL_OK)
		return TCL_ERROR ;

	r.p0 = Tcl_GetInt(interp, argv[2], &n) != TCL_OK ? -1 : n ;
	r.p1 = Tcl_GetInt(interp, argv[3], &n) != TCL_OK ? -1 : n ;

	if(r.p0 < 0 || r.p1 < 0 || r.p0 > r.p1) {
		Tcl_SetResult(interp, "strange numbers", TCL_STATIC) ;
		return TCL_ERROR ;
	}

	p = malloc(UTFmax * RLEN(r)) ;
	if(p == 0) {
		Tcl_SetResult(interp, "malloc fails", TCL_STATIC) ;
		return TCL_ERROR ;
	}
	
	emsg = rpc_read(h, id, r, p) ;	
	if(emsg != 0) {
		free(p) ;
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}
	
	/*
	 *	let tcl reclaim the string
	 */
	Tcl_SetResult(interp, p, TCL_DYNAMIC) ;
	return TCL_OK ;
}

static int
wrpc_replace(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	Id		id ;
	char	*emsg ;
	Range	r ;
	int		n ;
		
	if(chk(interp, argc, argv, 5, "replace needs id, begin, end and new text", &id) != TCL_OK)
		return TCL_ERROR ;

	r.p0 = Tcl_GetInt(interp, argv[2], &n) != TCL_OK ? -1 : n ;
	r.p1 = Tcl_GetInt(interp, argv[3], &n) != TCL_OK ? -1 : n ;


	if(r.p0 < 0 || r.p1 < 0 || r.p0 > r.p1) {
		Tcl_SetResult(interp, "strange numbers", TCL_STATIC) ;
		return TCL_ERROR ;
	}

	emsg = rpc_replace(h, id, r, argv[4]) ;	
	if(emsg != 0) {
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}
	return TCL_OK ;
}

static int
wrpc_exec(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	Id		id ;
	char	*emsg ;
	
	if(chk(interp, argc, argv, 3, "exec needs id and command", &id) != TCL_OK)
		return TCL_ERROR ;

	emsg = rpc_exec(h, id, argv[2]) ;
	if(emsg != 0) {
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}
	return TCL_OK ;
}

static int
wrpc_goto(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	Id		id ;
	char	*emsg ;
	Range	r ;
	int		b ;

	if(chk(interp, argc, argv, 4, "goto needs id, string and flag", &id) != TCL_OK)
		return TCL_ERROR ;

	if(Tcl_GetBoolean(interp, argv[3], &b) != TCL_OK)
		b = 0 ;
	
	emsg = rpc_goto(h, &id, &r, argv[2], b != 0 ? true : false) ;
	if(emsg != 0) {
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}
	
	sprintf(interp->result, "%ld %ld %ld", (long)id, (long)r.p0, (long)r.p1) ;
	return TCL_OK ;
}

static int
wrpc_length(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	Id		id ;
	char	*emsg ;
	Range	r ;

	if(chk(interp, argc, argv, 2, "length needs id", &id) != TCL_OK)
		return TCL_ERROR ;

	emsg = rpc_goto(h, &id, &r, ":$", false) ;
	if(emsg != 0) {
		Tcl_SetResult(interp, emsg, TCL_VOLATILE) ;
		return TCL_ERROR ;
	}
	
	sprintf(interp->result, "%ld", (long)r.p0) ;
	return TCL_OK ;
}

static int
wrpc_wouldblock(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	if(chk(interp, argc, argv, 1, "wouldblock needs no args", 0) != TCL_OK)
		return TCL_ERROR ;

	Tcl_SetResult(interp, rpc_wouldblock(h) == false ? "1" : "0", TCL_STATIC) ;
	return TCL_OK ;
}

static int
wrpc_event(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	if(chk(interp, argc, argv, 1, "event needs no args", 0) != TCL_OK)
		return TCL_ERROR ;
		
	if(rpc_event(h, &m) == -1) {
		Tcl_SetResult(interp, "event fails", TCL_STATIC) ;
		return TCL_ERROR ;
	}
	haveevent = true ;
	
	switch(m.t) {
	case WEexec:
		sprintf(interp->result, "WMexec\t%ld\t%s", (long)m.w, m.s) ;
		break ;
	case WEgoto:
		sprintf(interp->result, "WMgoto\t%ld\t%ld\t%ld\t%s", (long)m.w, m.r.p0, m.r.p1, m.s) ;
		break ;
	case WEdestroy:
		sprintf(interp->result, "WMdestory\t%ld", (long)m.w) ;
		break ;
	case WEreplace:
		sprintf(interp->result, "WMreplace\t%ld\t%ld\t%ld\t%s", (long)m.w, m.r.p0, m.r.p1, m.s) ;
		break ;
	default:
		Tcl_SetResult(interp, "unknown message type", TCL_STATIC) ;
		return TCL_ERROR ;
	}
	return TCL_OK ;
}

static int
wrpc_bounce(
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	if(chk(interp, argc, argv, 1, "bounce needs no args", 0) != TCL_OK)
		return TCL_ERROR ;
		
	if(haveevent != true) {
		Tcl_SetResult(interp, "nothing to bounce", TCL_STATIC) ;
		return TCL_ERROR ;
	}

	if(rpc_bounce(h, &m) != 0) {
		Tcl_SetResult(interp, "rpc_bounce fails", TCL_STATIC) ;
		return TCL_ERROR ;
	}
	return TCL_OK ;
}

static WCmd		cmds[] = {
	"init",			wrpc_init,
	"isconnected",	wrpc_isconnected,
	"list",			wrpc_list,
	"new",			wrpc_new,
	"attach",		wrpc_attach,
	"setname",		wrpc_setname,
	"name",			wrpc_name,
	"settools",		wrpc_settools,
	"read",			wrpc_read,
	"replace",		wrpc_replace,
	"exec",			wrpc_exec,
	"goto",			wrpc_goto,
	"length",		wrpc_length,
	"wouldblock",	wrpc_wouldblock,
	"event",		wrpc_event,
	"bounce",		wrpc_bounce,
} ;

static int
Wily(
	ClientData	clientdata,
	Tcl_Interp	*interp,
	int			argc,
	char		**argv
)
{
	WCmd	*p ;
	
	if(argc > 1)
		for(p = cmds ; p < cmds + sizeof cmds / sizeof cmds[0] ; p++)
			if(strcmp(argv[1], p->name) == 0)
				return (*p->proc)(interp, argc - 1, argv + 1) ;

	Tcl_AppendResult(interp, "usage: ", argv[0], ": ", (char*)0) ;
	
	for(p = cmds ; p < cmds + sizeof cmds / sizeof cmds[0] ; p++)
		Tcl_AppendResult(interp, "[", p->name, "] ", (char*)0) ;

	return TCL_ERROR ;
}

extern int
Wily_Init(
	Tcl_Interp	*interp
)
{
	Tcl_CreateCommand(interp, "wily", Wily, (ClientData)0, (Tcl_CmdDeleteProc*)0) ;
	return TCL_OK ;
}
