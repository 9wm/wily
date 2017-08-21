#include "Python.h"
#ifndef NDEBUG
#define NDEBUG
#endif

#include <u.h>
#include <libc.h>
#include <msg.h>

static PyObject *ErrorObject;

/* Declarations for objects of type Connection */

typedef struct {
	PyObject_HEAD
	/* XXXX Add your own stuff here */
	Handle	*h;
} conobject;

staticforward PyTypeObject Contype;

static char con_list__doc__[] = 
"List currently open windows

list()
Returns list of (name, id) tuples representing currently open windows"
;

static PyObject *
con_list(conobject *self, PyObject *args)
{
	char	*err;
	char	*buf;

	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	if((err = rpc_list(self->h, &buf))) {
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	return Py_BuildValue("s", buf);
}


static char con_win__doc__[] = 
"open a window

win(name:string, isBackedUp:integer)
Returns an integer window identifier, to be used for later operations on the window
"
;

static PyObject *
con_win(conobject *self, PyObject *args)
{
	char	*s;
	Id	id;
	char	*err;
	int	isbackedup;

	if (!PyArg_ParseTuple(args, "si", &s, &isbackedup)) {
		return NULL;
	}	
	if((err = rpc_new(self->h, s, &id, (ushort)isbackedup))) {
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	return Py_BuildValue("i", id);
}


static char con_event__doc__[] = 
"event()

returns an event tuple (w, t, r0, r1, s), whose fields are:
w: window identifier
t: event type
r0, r1: affected range
s: string

The meaning (if any) of the values of r0, r1 and s depend
on the event type 't'
"
;

static PyObject *
con_event(conobject *self, PyObject *args)
{
	Msg	m;
	char	*err;
	PyObject *o;

	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	if(rpc_event(self->h, &m)){
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	o = Py_BuildValue("(iills)", m.w, m.t, m.r.p0, m.r.p1, m.s);
	free(m.s);
	return o;
}


static char con_eventwouldblock__doc__[] = 
"eventwouldblock()

If eventwouldblock() returns true, calling event() might have to wait.
If eventwouldblock() returns false, calling event() would return immediately
because an event is already queued up and waiting";


static PyObject *
con_eventwouldblock(conobject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	return Py_BuildValue("b", rpc_wouldblock(self->h));
}

static char con_bounce__doc__[] = 
"bounce(tuple)

Called with an event tuple as returned by event().  Returns None"
;

static PyObject *
con_bounce(conobject *self, PyObject *args)
{
	Msg	m;
	char	*err;

	if (!PyArg_ParseTuple(args, "(iills)", 
		&m.w, &m.t, &m.r.p0, &m.r.p1, &m.s))
		return NULL;
	if(rpc_bounce(self->h, &m)){
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static char con_attach__doc__[] = 
"attach(w:integer, mask:integer)

'w' is a window identifier as obtained by new() or list().
'mask' is a bitmask of event types.

Sets the mask of events to be sent to us.
";

static PyObject *
con_attach(conobject *self, PyObject *args)
{
	Id	id;
	int	mask;
	char	*err;

	if (!PyArg_ParseTuple(args, "ii",&id, &mask))
		return NULL;
	if((err = rpc_attach(self->h, id, (ushort) mask))) {
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static char con_setname__doc__[] = 
"setname(w:integer, s:string)

Set w's name to 's'";

static PyObject *
con_setname(conobject *self, PyObject *args)
{
	char	*err;
	char	*name;
	Id	id;

	if (!PyArg_ParseTuple(args, "is", &id, &name))
		return NULL;
	if ((err = rpc_setname(self->h, id, name))) {
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}


static char con_settools__doc__[] = 
"settools(w:integer, s:string)

Set w's tools to 's'
";

static PyObject *
con_settools(conobject *self, PyObject *args)
{
	char	*err;
	char	*name;
	Id	id;

	if (!PyArg_ParseTuple(args, "is", &id, &name))
		return NULL;
	if ((err = rpc_settools(self->h, id, name))) {
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static char con_gettools__doc__[] = 
"gettools(w:integer) : string

Return the tools currently visible in w's tag";

static PyObject *
con_gettools(conobject *self, PyObject *args)
{
	char	*err;
	char	*tools;
	Id	id;

	if (!PyArg_ParseTuple(args, "i", &id))
		return NULL;
	if ((err = rpc_gettools(self->h, id, &tools))) {
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	return Py_BuildValue("s", tools);
}

static char con_getname__doc__[] = 
"getname(w:integer) : string

Return the name currently visible in w's tag";

static PyObject *
con_getname(conobject *self, PyObject *args)
{
	char	*err;
	char	*name;
	Id	id;

	if (!PyArg_ParseTuple(args, "i", &id))
		return NULL;
	if ((err = rpc_getname(self->h, id, &name))) {
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	return Py_BuildValue("s", name);
}

static char con_read__doc__[] = 
"read(w:integer, from:integer, to:integer)

returns a (UTF) string
";

static PyObject *
con_read(conobject *self, PyObject *args)
{
	int	from,to;
	Id	id;
	Range	r;
	char	*buf,*err;
	PyObject	*retval;

	if (!PyArg_ParseTuple(args, "iii", &id, &from, &to))
		return NULL;
	r.p0 = from;
	r.p1 = to;
	buf = malloc(UTFmax *(r.p1 -r.p0));
	if(!buf) {
		PyErr_NoMemory();
		return NULL;
	}
	err = rpc_read(self->h, id, r, buf);

	if(err) {
		PyErr_SetString(ErrorObject, err);
		retval= NULL;
	} else {
		retval=  Py_BuildValue("s", buf);
	}
	free(buf);
	return retval;
}


static char con_replace__doc__[] = 
"replace(w:integer, from:integer, to:integer, s:string)

replace the text in 'w' from 'from' to 'to' with 's'
";

static PyObject *
con_replace(conobject *self, PyObject *args)
{
	char	*err;
	int	p0, p1;
	char	*replace;
	Id	id;
	Range	r;

	if (!PyArg_ParseTuple(args, "iiis",&id, &p0, &p1,&replace))
		return NULL;
	if (p0>p1 || p1<0 || p0<0) {
		PyErr_SetString(ErrorObject, "range out of bounds");
		return NULL;
	}
	r.p0 = p0;
	r.p1 = p1;

	if((err = rpc_replace(self->h, id, r, replace))) {
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}


static char con_run__doc__[] = 
"run(w:integer, s:string)

has the same effect as sweeping 's' with B2  in window 'w'
";

static PyObject *
con_run(conobject *self, PyObject *args)
{
	char	*err;
	char	*name;
	Id	id;

	if (!PyArg_ParseTuple(args, "is",&id, &name))
		return NULL;
	if((err = rpc_exec(self->h, id, name))) {
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static char con_goto__doc__[] = 
"goto(w:integer, from:long, to:long, s:string, flag:integer)

has the same effect as sweeping 's' with B3  in window 'w',
and starting any search at range(from,to), except that we only
warp and select text if 'flag' is set.

Returns a tuple (w:integer, from:long, to:long),
which represents the window and selection that was
opened.
";

static PyObject *
con_goto(conobject *self, PyObject *args)
{
	char	*err;
	char	*name;
	Id	id;
	Range	r;
	long	from,to;
	int	flag;

	if (!PyArg_ParseTuple(args, "illsi",&id, &from, &to, &name,&flag))
		return NULL;
	r.p0 = from;
	r.p1 = to;
	if((err = rpc_goto(self->h, &id, &r, name, flag))) {
		PyErr_SetString(ErrorObject, err);
		return NULL;
	}
	return Py_BuildValue("(ill)", id, r.p0, r.p1);
}

static struct PyMethodDef con_methods[] = {
 {"list",	con_list,	1,	con_list__doc__},
 {"win",	con_win,	1,	con_win__doc__},
 {"event",	con_event,	1,	con_event__doc__},
 {"eventwouldblock",	con_eventwouldblock,	1,	con_eventwouldblock__doc__},
 {"bounce",	con_bounce,	1,	con_bounce__doc__},
 {"attach",	con_attach,	1,	con_attach__doc__},
 {"setname",	con_setname,	1,	con_setname__doc__},
 {"getname",	con_getname,	1,	con_getname__doc__},
 {"settools",	con_settools,	1,	con_settools__doc__},
 {"gettools",	con_gettools,	1,	con_gettools__doc__},
 {"read",	con_read,	1,	con_read__doc__},
 {"replace",	con_replace,	1,	con_replace__doc__},
 {"run",	con_run,	1,	con_run__doc__},
 {"goto",	con_goto,	1,	con_goto__doc__},
 
	{NULL,		NULL}		/* sentinel */
};

/* ---------- */


static conobject *
newconobject()
{
	conobject *self;
	int	fd;
	
	self = PyObject_NEW(conobject, &Contype);
	if (self == NULL)
		return NULL;
	/* XXXX Add your own initializers here */
	/* todo - handle errors */
	fd = client_connect();
	if(fd<0) {
		PyErr_SetString(ErrorObject, "client_connect");
		return NULL;
	}
	self->h = rpc_init(fd);
	if (!self->h) {
		PyErr_SetString(ErrorObject, "rpc_init");
		return NULL;
	}
	
	return self;
}


static void
con_dealloc(self)
	conobject *self;
{
	/* XXXX Add your own cleanup code here */
	PyMem_DEL(self);
}

static PyObject *
con_getattr(self, name)
	conobject *self;
	char *name;
{
	/* XXXX Add your own getattr code here */
	return Py_FindMethod(con_methods, (PyObject *)self, name);
}

static char Contype__doc__[] = 
"Wily connection

Holds the state for a connection to Wily
"
;

static PyTypeObject Contype = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/*ob_size*/
	"Connection",			/*tp_name*/
	sizeof(conobject),		/*tp_basicsize*/
	0,				/*tp_itemsize*/
	/* methods */
	(destructor)con_dealloc,	/*tp_dealloc*/
	(printfunc)0,		/*tp_print*/
	(getattrfunc)con_getattr,	/*tp_getattr*/
	(setattrfunc)0,	/*tp_setattr*/
	(cmpfunc)0,		/*tp_compare*/
	(reprfunc)0,		/*tp_repr*/
	0,			/*tp_as_number*/
	0,		/*tp_as_sequence*/
	0,		/*tp_as_mapping*/
	(hashfunc)0,		/*tp_hash*/
	(binaryfunc)0,		/*tp_call*/
	(reprfunc)0,		/*tp_str*/

	/* Space for future expansion */
	0L,0L,0L,0L,
	Contype__doc__ /* Documentation string */
};

/* End of code for Connection objects */
/* -------------------------------------------------------- */


static char wily_Connection__doc__[] =
"Connection()

Return a connection object.
Uses $WILYFIFO if set"
;

static PyObject *
wily_Connection(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	return (PyObject*) newconobject();
}

/* List of methods defined in the module */

static struct PyMethodDef wily_methods[] = {
	{"Connection",	wily_Connection,	1,	wily_Connection__doc__},
 
	{NULL,		NULL}		/* sentinel */
};


/* Initialization function for the module (*must* be called initwily) */

static char wily_module_documentation[] = 
"Open, manipulate, monitor Wily windows

Everything is accessed through a Connection object.
Windows are represented by an integer.
GOTO, EXEC, DESTROY and REPLACE are event bitmask values.";

void
initwily()
{
	PyObject *m, *d;

	/* Create the module and add the functions */
	m = Py_InitModule4("wily", wily_methods,
		wily_module_documentation,
		(PyObject*)NULL,PYTHON_API_VERSION);

	/* Add some symbolic constants to the module */
	d = PyModule_GetDict(m);
	ErrorObject = PyString_FromString("wily.error");
	PyDict_SetItemString(d, "error", ErrorObject);

	/* XXXX Add constants here */
	PyDict_SetItemString(d, "GOTO",	Py_BuildValue("i", WEgoto));
	PyDict_SetItemString(d, "EXEC", 	Py_BuildValue("i", WEexec));
	PyDict_SetItemString(d, "DESTROY", Py_BuildValue("i", WEdestroy));
	PyDict_SetItemString(d, "REPLACE", Py_BuildValue("i", WEreplace));

	/* Check for errors */
	if (PyErr_Occurred())
		Py_FatalError("can't initialize module wily");
}

