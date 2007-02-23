/*
 *  Python binding for the ALSA library - Universal Control Layer
 *  Copyright (c) 2007 by Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "Python.h"
#include "structmember.h"
#include "frameobject.h"
#ifndef PY_LONG_LONG
  #define PY_LONG_LONG LONG_LONG
#endif
#include "sys/poll.h"
#include "stdlib.h"
#include "alsa/asoundlib.h"

#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#endif
#ifndef Py_RETURN_TRUE
#define Py_RETURN_TRUE return Py_INCREF(Py_True), Py_True
#endif
#ifndef Py_RETURN_FALSE
#define Py_RETURN_FALSE return Py_INCREF(Py_False), Py_False
#endif

static int element_callback(snd_hctl_elem_t *elem, unsigned int mask);

static PyObject *module;
#if 0
static PyObject *buildin;
#endif
static PyInterpreterState *main_interpreter;

/*
 *
 */

#define PYHCTL(v) (((v) == Py_None) ? NULL : \
	((struct pyalsahcontrol *)(v)))

struct pyalsahcontrol {
	PyObject_HEAD
	snd_hctl_t *handle;
};

static inline PyObject *get_bool(int val)
{
	if (val) {
		Py_INCREF(Py_True);
		return Py_True;
	} else {
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject *
pyalsahcontrol_getcount(struct pyalsahcontrol *self, void *priv)
{
	return PyLong_FromLong(snd_hctl_get_count(self->handle));
}

PyDoc_STRVAR(handlevents__doc__,
"handleEvents() -- Process waiting hcontrol events (and call appropriate callbacks).");

static PyObject *
pyalsahcontrol_handleevents(struct pyalsahcontrol *self, PyObject *args)
{
	int err = snd_hctl_handle_events(self->handle);
	if (err < 0)
		PyErr_Format(PyExc_IOError,
		     "HControl handle events error: %s", strerror(-err));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(registerpoll__doc__,
"registerPoll(pollObj) -- Register poll file descriptors.");

static PyObject *
pyalsahcontrol_registerpoll(struct pyalsahcontrol *self, PyObject *args)
{
	PyObject *pollObj, *reg, *t;
	struct pollfd *pfd;
	int i, count;

	if (!PyArg_ParseTuple(args, "O", &pollObj))
		return NULL;

	count = snd_hctl_poll_descriptors_count(self->handle);
	if (count <= 0)
		Py_RETURN_NONE;
	pfd = malloc(sizeof(struct pollfd) * count);
	if (pfd == NULL)
		Py_RETURN_NONE;
	count = snd_hctl_poll_descriptors(self->handle, pfd, count);
	if (count <= 0)
		Py_RETURN_NONE;
	
	reg = PyObject_GetAttr(pollObj, PyString_InternFromString("register"));

	for (i = 0; i < count; i++) {
		t = PyTuple_New(2);
		if (t) {
			PyTuple_SET_ITEM(t, 0, PyInt_FromLong(pfd[i].fd));
			PyTuple_SET_ITEM(t, 1, PyInt_FromLong(pfd[i].events));
			Py_XDECREF(PyObject_CallObject(reg, t));
			Py_DECREF(t);
		}
	}	

	Py_XDECREF(reg);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(list__doc__,
"list() -- Return a list (tuple) of element IDs in (numid,interface,device,subdevice,name,index) tuple.");

static PyObject *
pyalsahcontrol_list(struct pyalsahcontrol *self, PyObject *args)
{
	PyObject *t, *v;
	int i, count;
	snd_hctl_elem_t *elem;
	snd_ctl_elem_id_t *id;
	
	snd_ctl_elem_id_alloca(&id);
	count = snd_hctl_get_count(self->handle);
	t = PyTuple_New(count);
	if (count == 0)
		return t;
	elem = snd_hctl_first_elem(self->handle);
	for (i = 0; i < count; i++) {
		v = NULL;
		if (elem) {
			snd_hctl_elem_get_id(elem, id);
			v = PyTuple_New(6);
			PyTuple_SET_ITEM(v, 0, PyInt_FromLong(snd_ctl_elem_id_get_numid(id)));
			PyTuple_SET_ITEM(v, 1, PyInt_FromLong(snd_ctl_elem_id_get_interface(id)));
			PyTuple_SET_ITEM(v, 2, PyInt_FromLong(snd_ctl_elem_id_get_device(id)));
			PyTuple_SET_ITEM(v, 3, PyInt_FromLong(snd_ctl_elem_id_get_subdevice(id)));
			PyTuple_SET_ITEM(v, 4, PyString_FromString(snd_ctl_elem_id_get_name(id)));
			PyTuple_SET_ITEM(v, 5, PyInt_FromLong(snd_ctl_elem_id_get_index(id)));
		}
		if (v == NULL || elem == NULL) {
			v = Py_None;
			Py_INCREF(v);
		}
		PyTuple_SET_ITEM(t, i, v);
		elem = snd_hctl_elem_next(elem);
	}
	return t;
}

PyDoc_STRVAR(alsahcontrolinit__doc__,
"HControl([name='default'],[mode=0])\n"
"  -- Open an ALSA HControl device.\n");

static int
pyalsahcontrol_init(struct pyalsahcontrol *pyhctl, PyObject *args, PyObject *kwds)
{
	char *name = "default";
	int mode = 0, err;

	static char * kwlist[] = { "name", "mode", NULL };

	pyhctl->handle = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|si", kwlist, &name, &mode))
		return -1;

	err = snd_hctl_open(&pyhctl->handle, name, mode);
	if (err < 0) {
		PyErr_Format(PyExc_IOError,
			     "HControl open error: %s", strerror(-err));
		return -1;
	}

	err = snd_hctl_load(pyhctl->handle);
	if (err < 0) {
		snd_hctl_close(pyhctl->handle);
		pyhctl->handle = NULL;
		PyErr_Format(PyExc_IOError,
			     "HControl load error: %s", strerror(-err));
		return -1;
	}

	return 0;
}

static void
pyalsahcontrol_dealloc(struct pyalsahcontrol *self)
{
	if (self->handle != NULL)
		snd_hctl_close(self->handle);

	self->ob_type->tp_free(self);
}

static PyGetSetDef pyalsahcontrol_getseters[] = {

	{"count",	(getter)pyalsahcontrol_getcount,	NULL,	"hcontrol element count",		NULL},

	{NULL}
};

static PyMethodDef pyalsahcontrol_methods[] = {

	{"list",	(PyCFunction)pyalsahcontrol_list,	METH_NOARGS,	list__doc__},
	{"handleEvents",(PyCFunction)pyalsahcontrol_handleevents,	METH_NOARGS,	handlevents__doc__},
	{"registerPoll",(PyCFunction)pyalsahcontrol_registerpoll,	METH_VARARGS|METH_KEYWORDS,	registerpoll__doc__},
	{NULL}
};

static PyTypeObject pyalsahcontrol_type = {
	PyObject_HEAD_INIT(0)
	tp_name:	"alsahcontrol.HControl",
	tp_basicsize:	sizeof(struct pyalsahcontrol),
	tp_dealloc:	(destructor)pyalsahcontrol_dealloc,
	tp_flags:	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	tp_doc:		alsahcontrolinit__doc__,
	tp_getset:	pyalsahcontrol_getseters,
	tp_init:	(initproc)pyalsahcontrol_init,
	tp_alloc:	PyType_GenericAlloc,
	tp_new:		PyType_GenericNew,
	tp_free:	PyObject_Del,
	tp_methods:	pyalsahcontrol_methods,
};

/*
 * hcontrol element section
 */

#define PYHCTLELEMENT(v) (((v) == Py_None) ? NULL : \
	((struct pyalsahcontrolelement *)(v)))

struct pyalsahcontrolelement {
	PyObject_HEAD
	PyObject *pyhandle;
	PyObject *callback;
	snd_hctl_t *handle;
	snd_hctl_elem_t *elem;
};

static PyObject *
pyalsahcontrolelement_getname(struct pyalsahcontrolelement *pyhelem, void *priv)
{
	return PyString_FromString(snd_hctl_elem_get_name(pyhelem->elem));
}

typedef unsigned int (*fcn1)(void *);

static PyObject *
pyalsahcontrolelement_uint(struct pyalsahcontrolelement *pyhelem, void *fcn)
{
	return PyInt_FromLong(((fcn1)fcn)(pyhelem->elem));
}

PyDoc_STRVAR(setcallback__doc__,
"setCallback(callObj) -- Set callback object.\n"
"Note: callObj might have callObj.callback attribute.\n");

static PyObject *
pyalsahcontrolelement_setcallback(struct pyalsahcontrolelement *pyhelem, PyObject *args)
{
	PyObject *o;

	if (!PyArg_ParseTuple(args, "O", &o))
		return NULL;
	if (o == Py_None) {
		Py_XDECREF(pyhelem->callback);
		pyhelem->callback = NULL;
		snd_hctl_elem_set_callback(pyhelem->elem, NULL);
	} else {
		Py_INCREF(o);
		pyhelem->callback = o;
		snd_hctl_elem_set_callback_private(pyhelem->elem, pyhelem);
		snd_hctl_elem_set_callback(pyhelem->elem, element_callback);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(elementinit__doc__,
"Element(hctl, numid)\n"
"Element(hctl, interface, device, subdevice, name, index)\n"
"Element(hctl, (interface, device, subdevice, name, index))\n"
"  -- Create a hcontrol element object.\n");

static int
pyalsahcontrolelement_init(struct pyalsahcontrolelement *pyhelem, PyObject *args, PyObject *kwds)
{
	PyObject *hctl, *first;
	char *name = "Default";
	int numid = 0, iface = 0, device = 0, subdevice = 0, index = 0;
	snd_ctl_elem_id_t *id;
	static char *kwlist1[] = { "hctl", "interface", "device", "subdevice", "name", "index" };

	snd_ctl_elem_id_alloca(&id);
	pyhelem->pyhandle = NULL;
	pyhelem->handle = NULL;
	pyhelem->elem = NULL;

	if (!PyTuple_Check(args) || PyTuple_Size(args) < 2) {
		PyErr_SetString(PyExc_TypeError, "first argument must be alsahcontrol.HControl");
		return -1;
	}
	first = PyTuple_GetItem(args, 1);
	if (PyInt_Check(first)) {
		if (!PyArg_ParseTuple(args, "Oi", &hctl, &numid))
			return -1;
		snd_ctl_elem_id_set_numid(id, numid);
	} else if (PyTuple_Check(first)) {
		if (!PyArg_ParseTuple(args, "OO", &hctl, &first))
			return -1;
		if (!PyArg_ParseTupleAndKeywords(first, kwds, "|iiisi", kwlist1 + 1, &iface, &device, &subdevice, &name, &index))
			return -1;
		goto parse1;
	} else {
		if (!PyArg_ParseTupleAndKeywords(first, kwds, "O|iiisi", kwlist1, &hctl, &iface, &device, &subdevice, &name, &index))
			return -1;
	      parse1:
		snd_ctl_elem_id_set_interface(id, iface);
		snd_ctl_elem_id_set_device(id, device);
		snd_ctl_elem_id_set_subdevice(id, subdevice);
		snd_ctl_elem_id_set_name(id, name);
		snd_ctl_elem_id_set_index(id, index);
	}

	if (hctl->ob_type != &pyalsahcontrol_type) {
		PyErr_SetString(PyExc_TypeError, "bad type for hctl argument");
		return -1;
	}

	pyhelem->pyhandle = hctl;
	Py_INCREF(hctl);
	pyhelem->handle = PYHCTL(hctl)->handle;

	pyhelem->elem = snd_hctl_find_elem(pyhelem->handle, id);
	if (pyhelem->elem == NULL) {
		if (numid == 0)
			PyErr_Format(PyExc_IOError, "cannot find hcontrol element %i,%i,%i,'%s',%i", iface, device, subdevice, name, index);
		else
			PyErr_Format(PyExc_IOError, "cannot find hcontrol element numid=%i", numid);
		return -1;
	}
	
	return 0;
}

static void
pyalsahcontrolelement_dealloc(struct pyalsahcontrolelement *self)
{
	if (self->elem) {
		Py_XDECREF(self->callback);
		snd_hctl_elem_set_callback(self->elem, NULL);
	}
	if (self->pyhandle) {
		Py_XDECREF(self->pyhandle);
	}

	self->ob_type->tp_free(self);
}

static PyGetSetDef pyalsahcontrolelement_getseters[] = {

	{"numid",	(getter)pyalsahcontrolelement_uint,	NULL,	"hcontrol element numid",	snd_hctl_elem_get_numid},
	{"interface",	(getter)pyalsahcontrolelement_uint,	NULL,	"hcontrol element interface",	snd_hctl_elem_get_interface},
	{"device",	(getter)pyalsahcontrolelement_uint,	NULL,	"hcontrol element device",	snd_hctl_elem_get_device},
	{"subdevice",	(getter)pyalsahcontrolelement_uint,	NULL,	"hcontrol element subdevice",	snd_hctl_elem_get_subdevice},
	{"name",	(getter)pyalsahcontrolelement_getname,	NULL,	"hcontrol element name",	NULL},
	{"index",	(getter)pyalsahcontrolelement_uint,	NULL,	"hcontrol element index",	snd_hctl_elem_get_index},
	
	{NULL}
};

static PyMethodDef pyalsahcontrolelement_methods[] = {

	{"setCallback",	(PyCFunction)pyalsahcontrolelement_setcallback,	METH_VARARGS,	setcallback__doc__},

	{NULL}
};

static PyTypeObject pyalsahcontrolelement_type = {
	PyObject_HEAD_INIT(0)
	tp_name:	"alsahcontrol.Element",
	tp_basicsize:	sizeof(struct pyalsahcontrolelement),
	tp_dealloc:	(destructor)pyalsahcontrolelement_dealloc,
	tp_flags:	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	tp_doc:		elementinit__doc__,
	tp_getset:	pyalsahcontrolelement_getseters,
	tp_init:	(initproc)pyalsahcontrolelement_init,
	tp_alloc:	PyType_GenericAlloc,
	tp_new:		PyType_GenericNew,
	tp_free:	PyObject_Del,
	tp_methods:	pyalsahcontrolelement_methods,
};

/*
 *
 */

static PyMethodDef pyalsahcontrolparse_methods[] = {
	{NULL}
};

PyMODINIT_FUNC
initalsahcontrol(void)
{
	PyObject *d, *d1, *l1, *o;
	int i;

	if (PyType_Ready(&pyalsahcontrol_type) < 0)
		return;
	if (PyType_Ready(&pyalsahcontrolelement_type) < 0)
		return;

	module = Py_InitModule3("alsahcontrol", pyalsahcontrolparse_methods, "libasound hcontrol wrapper");
	if (module == NULL)
		return;

#if 0
	buildin = PyImport_AddModule("__buildin__");
	if (buildin == NULL)
		return;
	if (PyObject_SetAttrString(module, "__buildins__", buildin) < 0)
		return;
#endif

	Py_INCREF(&pyalsahcontrol_type);
	PyModule_AddObject(module, "HControl", (PyObject *)&pyalsahcontrol_type);

	Py_INCREF(&pyalsahcontrolelement_type);
	PyModule_AddObject(module, "Element", (PyObject *)&pyalsahcontrolelement_type);

	d = PyModule_GetDict(module);

	/* ---- */

	d1 = PyDict_New();

#define add_space1(pname, name) { \
	o = PyInt_FromLong(SND_CTL_ELEM_IFACE_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space1("Card", CARD);
	add_space1("HwDep", HWDEP);
	add_space1("Mixer", MIXER);
	add_space1("PCM", PCM);
	add_space1("RawMidi", RAWMIDI);
	add_space1("Timer", TIMER);
	add_space1("Sequencer", SEQUENCER);
	add_space1("Last", LAST);

	PyDict_SetItemString(d, "InterfaceId", d1);
	Py_DECREF(d1);

	/* ---- */

	l1 = PyList_New(0);

	for (i = 0; i <= SND_CTL_ELEM_IFACE_LAST; i++) {
		o = PyString_FromString(snd_ctl_elem_iface_name(i));
		PyList_Append(l1, o);
		Py_DECREF(o);
	}

	PyDict_SetItemString(d, "InterfaceName", l1);
	Py_DECREF(l1);

	/* ---- */

	d1 = PyDict_New();
	
#define add_space2(pname, name) { \
	o = PyInt_FromLong(SND_CTL_ELEM_TYPE_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space2("None", NONE);
	add_space2("Boolean", BOOLEAN);
	add_space2("Integer", INTEGER);
	add_space2("Enumeraed", ENUMERATED);
	add_space2("Bytes", BYTES);
	add_space2("IEC958", IEC958);
	add_space2("Integer64", INTEGER64);
	add_space2("Last", LAST);

	PyDict_SetItemString(d, "ElementType", d1);
	Py_DECREF(d1);
	
	/* ---- */

	l1 = PyList_New(0);

	for (i = 0; i <= SND_CTL_ELEM_TYPE_LAST; i++) {
		o = PyString_FromString(snd_ctl_elem_type_name(i));
		PyList_Append(l1, o);
		Py_DECREF(o);
	}

	PyDict_SetItemString(d, "ElementTypeName", l1);
	Py_DECREF(l1);

	/* ---- */

	d1 = PyDict_New();
	
#define add_space3(pname, name) { \
	o = PyInt_FromLong(SND_CTL_EVENT_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space3("Element", ELEM);
	add_space3("Last", LAST);

	PyDict_SetItemString(d, "EventClass", d1);
	Py_DECREF(d1);

	/* ---- */

	d1 = PyDict_New();
	
#define add_space4(pname, name) { \
	o = PyInt_FromLong(SND_CTL_EVENT_MASK_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space4("Value", VALUE);
	add_space4("Info", INFO);
	add_space4("Add", ADD);
	add_space4("TLV", TLV);

	PyDict_SetItemString(d, "EventMask", d1);
	Py_DECREF(d1);

	o = PyInt_FromLong(SND_CTL_EVENT_MASK_REMOVE);
	PyDict_SetItemString(d, "EventMaskRemove", o);
	Py_DECREF(o);

	/* ---- */

	d1 = PyDict_New();
	
#define add_space5(pname, name) { \
	o = PyInt_FromLong(SND_CTL_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space5("NonBlock", NONBLOCK);
	add_space5("Async", ASYNC);
	add_space5("ReadOnly", READONLY);

	PyDict_SetItemString(d, "OpenMode", d1);
	Py_DECREF(d1);

	/* ---- */

	main_interpreter = PyThreadState_Get()->interp;

	if (PyErr_Occurred())
		Py_FatalError("Cannot initialize module alsahcontrol");
}

/*
 *  element event callback
 */

static int element_callback(snd_hctl_elem_t *elem, unsigned int mask)
{
	PyThreadState *tstate, *origstate;
	struct pyalsahcontrolelement *pyhelem;
	PyObject *o, *t, *r;
	int res = 0, inside = 1;

	if (elem == NULL)
		return -EINVAL;
	pyhelem = snd_hctl_elem_get_callback_private(elem);
	if (pyhelem == NULL || pyhelem->callback == NULL)
		return -EINVAL;

	tstate = PyThreadState_New(main_interpreter);
	origstate = PyThreadState_Swap(tstate);

	o = PyObject_GetAttr(pyhelem->callback, PyString_InternFromString("callback"));
	if (!o) {
		PyErr_Clear();
		o = pyhelem->callback;
		inside = 0;
	}

	t = PyTuple_New(2);
	if (t) {
		if (PyTuple_SET_ITEM(t, 0, (PyObject *)pyhelem))
			Py_INCREF(pyhelem);
		PyTuple_SET_ITEM(t, 1, PyInt_FromLong(mask));
		r = PyObject_CallObject(o, t);
		Py_DECREF(t);
			
		if (r) {
			if (PyInt_Check(r)) {
				res = PyInt_AsLong(o);
			} else if (r == Py_None) {
				res = 0;
			}
			Py_DECREF(r);
		} else {
			PyErr_Print();
			PyErr_Clear();
			res = -EIO;
		}
	}
	if (inside) {
		Py_DECREF(o);
	}

	PyThreadState_Swap(origstate);
	PyThreadState_Delete(tstate);

	return res;
}
