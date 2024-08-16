/*
 *  Python binding for the ALSA library - Universal Control Layer
 *  Copyright (c) 2007 by Jaroslav Kysela <perex@perex.cz>
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "common.h"
#include "sys/poll.h"
#include "stdlib.h"
#include "alsa/asoundlib.h"

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

static PyObject *id_to_python(snd_ctl_elem_id_t *id)
{
	PyObject *v;

	v = PyTuple_New(6);
	if (v == NULL)
		return NULL;
	PyTuple_SET_ITEM(v, 0, PyInt_FromLong(snd_ctl_elem_id_get_numid(id)));
	PyTuple_SET_ITEM(v, 1, PyInt_FromLong(snd_ctl_elem_id_get_interface(id)));
	PyTuple_SET_ITEM(v, 2, PyInt_FromLong(snd_ctl_elem_id_get_device(id)));
	PyTuple_SET_ITEM(v, 3, PyInt_FromLong(snd_ctl_elem_id_get_subdevice(id)));
	PyTuple_SET_ITEM(v, 4, PyUnicode_FromString(snd_ctl_elem_id_get_name(id)));
	PyTuple_SET_ITEM(v, 5, PyInt_FromLong(snd_ctl_elem_id_get_index(id)));
	return v;
}

static int parse_id(snd_ctl_elem_id_t *id, PyObject *o)
{
	unsigned int interface, device, subdevice, index;
	char *name;

	if (!PyTuple_Check(o) || PyTuple_Size(o) != 5) {
		PyErr_SetString(PyExc_TypeError, "id argument tuple size error");
		return -1;
	}
	if (!PyArg_ParseTuple(o, "iiisi", &interface, &device, &subdevice, &name, &index))
		return -1;
	snd_ctl_elem_id_set_interface(id, interface);
	snd_ctl_elem_id_set_device(id, device);
	snd_ctl_elem_id_set_subdevice(id, subdevice);
	snd_ctl_elem_id_set_name(id, name);
	snd_ctl_elem_id_set_index(id, index);
	return 0;
}

typedef int (*fcn_simple)(void *, void *);

static PyObject *simple_id_fcn(struct pyalsahcontrol *self, PyObject *args, void *fcn, const char *xname)
{
	snd_ctl_elem_id_t *id;
	int res;

	snd_ctl_elem_id_alloca(&id);
	if (PyTuple_Check(args) && !PyTuple_Check(PyTuple_GetItem(args, 0))) {
		if (parse_id(id, args) < 0)
			return NULL;
	} else {
		if (!PyArg_ParseTuple(args, "O", &args))
			return NULL;
		if (parse_id(id, args) < 0)
			return NULL;
	}
	res = ((fcn_simple)fcn)(snd_hctl_ctl(self->handle), id);
	if (res < 0) {
		PyErr_Format(PyExc_IOError, "element %s error: %s", xname, snd_strerror(-res));
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *
pyalsahcontrol_getcount(struct pyalsahcontrol *self, void *priv)
{
	return PyLong_FromLong(snd_hctl_get_count(self->handle));
}

static PyObject *
pyalsahcontrol_getChctl(struct pyalsahcontrol *self, void *priv)
{
	return PyInt_FromLong((long)self->handle);
}

PyDoc_STRVAR(handlevents__doc__,
"handle_events() -- Process waiting hcontrol events (and call appropriate callbacks).");

static PyObject *
pyalsahcontrol_handleevents(struct pyalsahcontrol *self, PyObject *args)
{
	int err;
	
	Py_BEGIN_ALLOW_THREADS;
	err = snd_hctl_handle_events(self->handle);
	Py_END_ALLOW_THREADS;
	if (err < 0)
		PyErr_Format(PyExc_IOError,
		     "HControl handle events error: %s", strerror(-err));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(registerpoll__doc__,
"register_poll(pollObj) -- Register poll file descriptors.");

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
	pfd = alloca(sizeof(struct pollfd) * count);
	count = snd_hctl_poll_descriptors(self->handle, pfd, count);
	if (count <= 0)
		Py_RETURN_NONE;
	
	reg = PyObject_GetAttr(pollObj, InternFromString("register"));

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

static PyObject *
pyalsahcontrol_getpollfds(struct pyalsahcontrol *self, void *priv)
{
	PyObject *l, *t;
	struct pollfd *pfds;
	int i, count;

	count = snd_hctl_poll_descriptors_count(self->handle);
	if (count < 0) {
pfds_error:
		PyErr_Format(PyExc_IOError, "poll descriptors error: %s", snd_strerror(count));
		return NULL;
	}
	pfds = alloca(sizeof(struct pollfd) * count);
	count = snd_hctl_poll_descriptors(self->handle, pfds, count);
	if (count < 0)
		goto pfds_error;

	l = PyList_New(count);
	if (!l)
		return NULL;
	for (i = 0; i < count; ++i) {
		t = PyTuple_New(2);
		if (t) {
			PyTuple_SET_ITEM(t, 0, PyInt_FromLong(pfds[i].fd));
			PyTuple_SET_ITEM(t, 1, PyInt_FromLong(pfds[i].events));
			PyList_SetItem(l, i, t);
		}
	}
	return l;
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
			v = id_to_python(id);
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

PyDoc_STRVAR(elementnew__doc__,
"element_new(element_type['INTEGER'], id, count, min, max, step)\n"
"element_new(element_type['INTEGER64'], id, count, min64, max64, step64)\n"
"element_new(element_type['BOOLEAN'], id, count)\n"
"element_new(element_type['IEC958'], id)\n"
"  -- Create a new hcontrol element.\n"
"  -- The id argument is tuple (interface, device, subdevice, name, index).\n");

static PyObject *
pyalsahcontrol_elementnew(struct pyalsahcontrol *self, PyObject *args)
{
	long type;
	PyObject *o;
	unsigned int count;
	long min, max, step;
	long long min64, max64, step64;
	snd_ctl_elem_id_t *id;
	snd_ctl_t *ctl;
	int res;

	snd_ctl_elem_id_alloca(&id);
	if (!PyTuple_Check(args) || PyTuple_Size(args) < 2) {
		PyErr_SetString(PyExc_TypeError, "wrong argument count");
		return NULL;
	}
	o = PyTuple_GetItem(args, 0);
	if (get_long1(o, &type)) {
		PyErr_SetString(PyExc_TypeError, "type argument is not integer");
		return NULL;
	}
	Py_INCREF(o);
	o = PyTuple_GetItem(args, 1);
	if (!PyTuple_Check(o)) {
		PyErr_SetString(PyExc_TypeError, "id argument is not tuple");
		return NULL;
	}
	switch (type) {
	case SND_CTL_ELEM_TYPE_INTEGER:
		if (!PyArg_ParseTuple(args, "iOilll", &type, &o, &count, &min, &max, &step))
			return NULL;
		break;
	case SND_CTL_ELEM_TYPE_INTEGER64:
		if (!PyArg_ParseTuple(args, "iO|iLLL", &type, &o, &count, &min64, &max64, &step64))
			return NULL;
		break;
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		if (!PyArg_ParseTuple(args, "iOi", &type, &o, &count))
			return NULL;
		break;
	case SND_CTL_ELEM_TYPE_IEC958:
		if (!PyArg_ParseTuple(args, "iO", &type, &o))
			return NULL;
		break;
	default:
		PyErr_Format(PyExc_TypeError, "type %li is not supported yet", type);
		return NULL;
		
	}
	if (parse_id(id, o) < 0)
		return NULL;
	ctl = snd_hctl_ctl(self->handle);
	switch (type) {
	case SND_CTL_ELEM_TYPE_INTEGER:
		res = snd_ctl_elem_add_integer(ctl, id, count, min, max, step);
		break;
	case SND_CTL_ELEM_TYPE_INTEGER64:
		res = snd_ctl_elem_add_integer64(ctl, id, count, min64, max64, step64);
		break;
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		res = snd_ctl_elem_add_boolean(ctl, id, count);
		break;
	case SND_CTL_ELEM_TYPE_IEC958:
		res = snd_ctl_elem_add_iec958(ctl, id);
		break;
	default:
		res = -EIO;
		break;
	}
	if (res < 0) {
		PyErr_Format(PyExc_IOError, "new element of type %li create error: %s", type, snd_strerror(-res));
		return NULL;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(elementremove__doc__,
"element_remove(id)\n"
"element_remove(interface, device, subdevice, name, index)\n"
"  -- Remove element.\n");

static PyObject *
pyalsahcontrol_elementremove(struct pyalsahcontrol *self, PyObject *args)
{
	return simple_id_fcn(self, args, snd_ctl_elem_remove, "remove");
}

PyDoc_STRVAR(elementlock__doc__,
"element_lock(id)\n"
"element_lock(interface, device, subdevice, name, index)\n"
" -- Lock element.\n");

static PyObject *
pyalsahcontrol_elementlock(struct pyalsahcontrol *self, PyObject *args)
{
	return simple_id_fcn(self, args, snd_ctl_elem_lock, "lock");
}

PyDoc_STRVAR(elementunlock__doc__,
"element_unlock(id)\n"
"element_unlock(interface, device, subdevice, name, index)\n"
" -- Unlock element.\n");

static PyObject *
pyalsahcontrol_elementunlock(struct pyalsahcontrol *self, PyObject *args)
{
	return simple_id_fcn(self, args, snd_ctl_elem_unlock, "unlock");
}

PyDoc_STRVAR(alsahcontrolinit__doc__,
"HControl([name='default'],[mode=0])\n"
"  -- Open an ALSA HControl device.\n");

static int
pyalsahcontrol_init(struct pyalsahcontrol *pyhctl, PyObject *args, PyObject *kwds)
{
	char *name = "default";
	int mode = 0, load = 1, err;

	static char * kwlist[] = { "name", "mode", "load", NULL };

	pyhctl->handle = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sii", kwlist, &name, &mode, &load))
		return -1;

	err = snd_hctl_open(&pyhctl->handle, name, mode);
	if (err < 0) {
		PyErr_Format(PyExc_IOError,
			     "HControl open error: %s", strerror(-err));
		return -1;
	}

	if (load) {
		err = snd_hctl_load(pyhctl->handle);
		if (err < 0) {
			snd_hctl_close(pyhctl->handle);
			pyhctl->handle = NULL;
			PyErr_Format(PyExc_IOError,
				     "HControl load error: %s", strerror(-err));
			return -1;
		}
	}

	return 0;
}

static void
pyalsahcontrol_dealloc(struct pyalsahcontrol *self)
{
	if (self->handle != NULL)
		snd_hctl_close(self->handle);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyGetSetDef pyalsahcontrol_getseters[] = {

	{"count",	(getter)pyalsahcontrol_getcount,	NULL,	"hcontrol element count",		NULL},
	{"poll_fds",	(getter)pyalsahcontrol_getpollfds,	NULL,	"list of (fd, eventbits) tuples",	NULL},
	{"get_C_hctl",	(getter)pyalsahcontrol_getChctl,	NULL,	"hcontrol C pointer (internal)",	NULL},

	{NULL}
};

static PyMethodDef pyalsahcontrol_methods[] = {

	{"list",	(PyCFunction)pyalsahcontrol_list,	METH_NOARGS,	list__doc__},
	{"element_new",	(PyCFunction)pyalsahcontrol_elementnew,	METH_VARARGS,	elementnew__doc__},
	{"element_remove",(PyCFunction)pyalsahcontrol_elementremove,	METH_VARARGS,	elementremove__doc__},
	{"element_lock",(PyCFunction)pyalsahcontrol_elementlock,	METH_VARARGS,	elementlock__doc__},
	{"element_unlock",(PyCFunction)pyalsahcontrol_elementunlock,	METH_VARARGS,	elementunlock__doc__},
	{"handle_events",(PyCFunction)pyalsahcontrol_handleevents,	METH_NOARGS,	handlevents__doc__},
	{"register_poll",(PyCFunction)pyalsahcontrol_registerpoll,	METH_VARARGS|METH_KEYWORDS,	registerpoll__doc__},
	{NULL}
};

static PyTypeObject pyalsahcontrol_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
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
	return PyUnicode_FromString(snd_hctl_elem_get_name(pyhelem->elem));
}

typedef unsigned int (*fcn1)(void *);

static PyObject *
pyalsahcontrolelement_uint(struct pyalsahcontrolelement *pyhelem, void *fcn)
{
	return PyInt_FromLong(((fcn1)fcn)(pyhelem->elem));
}

static PyObject *
pyalsahcontrolelement_getChelem(struct pyalsahcontrolelement *self, void *priv)
{
	return PyInt_FromLong((long)self->elem);
}

PyDoc_STRVAR(elock__doc__,
"lock() -- Lock this element.\n");

static PyObject *
pyalsahcontrolelement_lock(struct pyalsahcontrolelement *pyhelem, PyObject *args)
{
	snd_ctl_elem_id_t *id;
	int res;
	
	snd_ctl_elem_id_alloca(&id);
	snd_hctl_elem_get_id(pyhelem->elem, id);
	res = snd_ctl_elem_lock(snd_hctl_ctl(pyhelem->handle), id);
	if (res < 0)
		return PyErr_Format(PyExc_IOError, "element lock error: %s", snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(eunlock__doc__,
"unlock() -- Unlock this element.\n");

static PyObject *
pyalsahcontrolelement_unlock(struct pyalsahcontrolelement *pyhelem, PyObject *args)
{
	snd_ctl_elem_id_t *id;
	int res;
	
	snd_ctl_elem_id_alloca(&id);
	snd_hctl_elem_get_id(pyhelem->elem, id);
	res = snd_ctl_elem_unlock(snd_hctl_ctl(pyhelem->handle), id);
	if (res < 0)
		return PyErr_Format(PyExc_IOError, "element unlock error: %s", snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(setcallback__doc__,
"set_callback(callObj) -- Set callback object.\n"
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
	static char *kwlist1[] = { "hctl", "interface", "device", "subdevice", "name", "index", NULL };
	long helem = 0;
	float f = 0;
	snd_ctl_elem_info_t *elem_info;

	snd_ctl_elem_id_alloca(&id);
	pyhelem->pyhandle = NULL;
	pyhelem->handle = NULL;
	pyhelem->elem = NULL;

	if (!PyTuple_Check(args) || PyTuple_Size(args) < 2) {
		PyErr_SetString(PyExc_TypeError, "first argument must be alsahcontrol.HControl");
		return -1;
	}
	first = PyTuple_GetItem(args, 1);
	if (PyFloat_Check(first)) {
		if (!PyArg_ParseTuple(args, "Ofi", &hctl, &f, &helem))
			return -1;
	} else if (PyLong_Check(first)) {
		if (!PyArg_ParseTuple(args, "Oi", &hctl, &numid))
			return -1;
		snd_ctl_elem_id_set_numid(id, numid);
#if PY_MAJOR_VERSION < 3
	} else if (PyInt_Check(first)) {
		if (!PyArg_ParseTuple(args, "Oi", &hctl, &numid))
			return -1;
		snd_ctl_elem_id_set_numid(id, numid);
#endif
	} else if (PyTuple_Check(first)) {
		if (!PyArg_ParseTuple(args, "OO", &hctl, &first))
			return -1;
		if (!PyArg_ParseTuple(first, "iiis|i", &iface, &device, &subdevice, &name, &index))
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

	/* FIXME: Remove it when hctl find works ok !!! */
	snd_ctl_elem_info_alloca(&elem_info);
	snd_ctl_elem_info_set_id(elem_info, id);
	if (snd_ctl_elem_info(snd_hctl_ctl(pyhelem->handle), elem_info) < 0)
		goto elem_not_found;
	snd_ctl_elem_info_get_id(elem_info, id);

	pyhelem->elem = helem > 0 ? (snd_hctl_elem_t *)helem :
				snd_hctl_find_elem(pyhelem->handle, id);
	if (pyhelem->elem == NULL) {
elem_not_found:
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
	Py_XDECREF(self->pyhandle);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyGetSetDef pyalsahcontrolelement_getseters[] = {

	{"numid",	(getter)pyalsahcontrolelement_uint,	NULL,	"hcontrol element numid",	snd_hctl_elem_get_numid},
	{"interface",	(getter)pyalsahcontrolelement_uint,	NULL,	"hcontrol element interface",	snd_hctl_elem_get_interface},
	{"device",	(getter)pyalsahcontrolelement_uint,	NULL,	"hcontrol element device",	snd_hctl_elem_get_device},
	{"subdevice",	(getter)pyalsahcontrolelement_uint,	NULL,	"hcontrol element subdevice",	snd_hctl_elem_get_subdevice},
	{"name",	(getter)pyalsahcontrolelement_getname,	NULL,	"hcontrol element name",	NULL},
	{"index",	(getter)pyalsahcontrolelement_uint,	NULL,	"hcontrol element index",	snd_hctl_elem_get_index},
	{"get_C_helem",	(getter)pyalsahcontrolelement_getChelem, NULL,	"hcontrol element C pointer (internal)", NULL },
	
	{NULL}
};

static PyMethodDef pyalsahcontrolelement_methods[] = {

	{"lock",	(PyCFunction)pyalsahcontrolelement_lock,	METH_NOARGS,	elock__doc__},
	{"unlock",	(PyCFunction)pyalsahcontrolelement_unlock,	METH_NOARGS,	eunlock__doc__},

	{"set_callback",(PyCFunction)pyalsahcontrolelement_setcallback,	METH_VARARGS,	setcallback__doc__},

	{NULL}
};

static PyTypeObject pyalsahcontrolelement_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
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
 * hcontrol info section
 */

#define PYHCTLINFO(v) (((v) == Py_None) ? NULL : \
	((struct pyalsahcontrolinfo *)(v)))

struct pyalsahcontrolinfo {
	PyObject_HEAD
	PyObject *pyelem;
	snd_hctl_elem_t *elem;
	snd_ctl_elem_info_t *info;
};

static PyObject *
pyalsahcontrolinfo_element(struct pyalsahcontrolinfo *pyinfo, void *priv)
{
	Py_INCREF(pyinfo->pyelem);
	return pyinfo->pyelem;
}

typedef int (*fcn2)(void *);

static PyObject *
pyalsahcontrolinfo_bool(struct pyalsahcontrolinfo *pyinfo, void *fcn)
{
	return get_bool(((fcn2)fcn)(pyinfo->info));
}

static PyObject *
pyalsahcontrolinfo_getowner(struct pyalsahcontrolinfo *pyinfo, void *priv)
{
	return PyInt_FromLong(snd_ctl_elem_info_get_owner(pyinfo->info));
}

static PyObject *
pyalsahcontrolinfo_getitems(struct pyalsahcontrolinfo *pyinfo, void *priv)
{
	if (snd_ctl_elem_info_get_type(pyinfo->info) != SND_CTL_ELEM_TYPE_ENUMERATED) {
		PyErr_SetString(PyExc_TypeError, "element is not enumerated");
		return NULL;
	}
	return PyInt_FromLong(snd_ctl_elem_info_get_items(pyinfo->info));
}

typedef long (*fcn3_0)(void *);

static PyObject *
pyalsahcontrolinfo_long(struct pyalsahcontrolinfo *pyinfo, void *fcn)
{
	if (snd_ctl_elem_info_get_type(pyinfo->info) != SND_CTL_ELEM_TYPE_INTEGER) {
		PyErr_SetString(PyExc_TypeError, "element is not integer");
		return NULL;
	}
	return PyLong_FromLong(((fcn3_0)fcn)(pyinfo->info));
}

typedef long long (*fcn3)(void *);

static PyObject *
pyalsahcontrolinfo_longlong(struct pyalsahcontrolinfo *pyinfo, void *fcn)
{
	if (snd_ctl_elem_info_get_type(pyinfo->info) != SND_CTL_ELEM_TYPE_INTEGER64) {
		PyErr_SetString(PyExc_TypeError, "element is not integer64");
		return NULL;
	}
	return PyLong_FromLongLong(((fcn3)fcn)(pyinfo->info));
}

typedef unsigned int (*fcn4)(void *);

static PyObject *
pyalsahcontrolinfo_uint(struct pyalsahcontrolinfo *pyinfo, void *fcn)
{
	return PyLong_FromLong(((fcn4)fcn)(pyinfo->info));
}

typedef const char * (*fcn5)(void *);

static PyObject *
pyalsahcontrolinfo_str(struct pyalsahcontrolinfo *pyinfo, void *fcn)
{
	return PyUnicode_FromString(((fcn5)fcn)(pyinfo->info));
}

static PyObject *
pyalsahcontrolinfo_id(struct pyalsahcontrolinfo *pyinfo, void *priv)
{
	snd_ctl_elem_id_t *id;
	
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_info_get_id(pyinfo->info, id);
	return id_to_python(id);
}

static PyObject *
pyalsahcontrolinfo_dimensions(struct pyalsahcontrolinfo *pyinfo, void *priv)
{
	int dims = snd_ctl_elem_info_get_dimensions(pyinfo->info);
	int i;
	PyObject *t;
	
	if (dims <= 0)
		Py_RETURN_NONE;
	t = PyTuple_New(dims);
	if (t == NULL)
		return NULL;
	for (i = 0; i < dims; i++) {
		PyTuple_SET_ITEM(t, i, PyInt_FromLong(snd_ctl_elem_info_get_dimension(pyinfo->info, i)));
	}
	return t;
}

static PyObject *
pyalsahcontrolinfo_itemnames(struct pyalsahcontrolinfo *pyinfo, void *priv)
{
	int items;
	int res;
	int i;
	PyObject *t;
	
	if (snd_ctl_elem_info_get_type(pyinfo->info) != SND_CTL_ELEM_TYPE_ENUMERATED) {
		PyErr_SetString(PyExc_TypeError, "element is not enumerated");
		return NULL;
	}
	items = snd_ctl_elem_info_get_items(pyinfo->info);
	if (items <= 0)
		Py_RETURN_NONE;
	t = PyTuple_New(items);
	if (t == NULL)
		return NULL;
	for (i = 0; i < items; i++) {
		snd_ctl_elem_info_set_item(pyinfo->info, i);
		res = snd_hctl_elem_info(pyinfo->elem, pyinfo->info);
		if (res < 0) {
			Py_INCREF(Py_None);
			PyTuple_SET_ITEM(t, i, Py_None);
		} else {
			PyTuple_SET_ITEM(t, i, PyUnicode_FromString(snd_ctl_elem_info_get_item_name(pyinfo->info)));
		}
	}
	return t;
}

PyDoc_STRVAR(infoinit__doc__,
"Info(elem)\n"
"  -- Create a hcontrol element info object.\n");

static int
pyalsahcontrolinfo_init(struct pyalsahcontrolinfo *pyinfo, PyObject *args, PyObject *kwds)
{
	PyObject *elem;
	int res;

	pyinfo->pyelem = NULL;
	pyinfo->elem = NULL;
	pyinfo->info = NULL;

	if (!PyArg_ParseTuple(args, "O", &elem))
		return -1;

	if (elem->ob_type != &pyalsahcontrolelement_type) {
		PyErr_SetString(PyExc_TypeError, "bad type for element argument");
		return -1;
	}

	if (snd_ctl_elem_info_malloc(&pyinfo->info)) {
		PyErr_SetString(PyExc_TypeError, "malloc problem");
		return -1;
	}

	pyinfo->pyelem = elem;
	Py_INCREF(elem);
	pyinfo->elem = PYHCTLELEMENT(elem)->elem;

	res = snd_hctl_elem_info(pyinfo->elem, pyinfo->info);
	if (res < 0) {
		PyErr_Format(PyExc_IOError, "hcontrol element info problem: %s", snd_strerror(-res));
		return -1;
	}	

	return 0;
}

static void
pyalsahcontrolinfo_dealloc(struct pyalsahcontrolinfo *self)
{
	if (self->info)
		snd_ctl_elem_info_free(self->info);
	Py_XDECREF(self->pyelem);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyGetSetDef pyalsahcontrolinfo_getseters[] = {

	{"element",	(getter)pyalsahcontrolinfo_element,	NULL,	"hcontrol element for this info",	NULL},

	{"id",		(getter)pyalsahcontrolinfo_id,		NULL,	"hcontrol element full id",	snd_ctl_elem_info_get_id},

	{"numid",	(getter)pyalsahcontrolinfo_uint,	NULL,	"hcontrol element numid",	snd_ctl_elem_info_get_numid},
	{"interface",	(getter)pyalsahcontrolinfo_uint,	NULL,	"hcontrol element interface",	snd_ctl_elem_info_get_interface},
	{"device",	(getter)pyalsahcontrolinfo_uint,	NULL,	"hcontrol element device",	snd_ctl_elem_info_get_device},
	{"subdevice",	(getter)pyalsahcontrolinfo_uint,	NULL,	"hcontrol element subdevice",	snd_ctl_elem_info_get_subdevice},
	{"name",	(getter)pyalsahcontrolinfo_str,		NULL,	"hcontrol element name",	snd_ctl_elem_info_get_name},
	{"index",	(getter)pyalsahcontrolinfo_uint,	NULL,	"hcontrol element index",	snd_ctl_elem_info_get_index},

	{"type",	(getter)pyalsahcontrolinfo_uint,	NULL,	"hcontrol element index",	snd_ctl_elem_info_get_type},

	{"is_readable",	(getter)pyalsahcontrolinfo_bool,	NULL,	"hcontrol element is readable",	snd_ctl_elem_info_is_readable},
	{"is_writable",	(getter)pyalsahcontrolinfo_bool,	NULL,	"hcontrol element is writable",snd_ctl_elem_info_is_writable},
	{"is_volatile",	(getter)pyalsahcontrolinfo_bool,	NULL,	"hcontrol element is volatile",snd_ctl_elem_info_is_volatile},
	{"is_inactive",	(getter)pyalsahcontrolinfo_bool,	NULL,	"hcontrol element is inactive",snd_ctl_elem_info_is_inactive},
	{"is_locked",	(getter)pyalsahcontrolinfo_bool,	NULL,	"hcontrol element is locked",snd_ctl_elem_info_is_locked},
	{"is_tlv_readable",(getter)pyalsahcontrolinfo_bool,	NULL,	"hcontrol element is TLV readable",snd_ctl_elem_info_is_tlv_readable},
	{"is_tlv_Writable",(getter)pyalsahcontrolinfo_bool,	NULL,	"hcontrol element is TLV writable",snd_ctl_elem_info_is_tlv_writable},
	{"is_tlv_Commandable",(getter)pyalsahcontrolinfo_bool,	NULL,	"hcontrol element is TLV commandable",snd_ctl_elem_info_is_tlv_commandable},
	{"is_owner",	(getter)pyalsahcontrolinfo_bool,	NULL,	"this process is owner of this hcontrol element",snd_ctl_elem_info_is_owner},
	{"is_user",	(getter)pyalsahcontrolinfo_bool,	NULL,	"hcontrol element is user element",snd_ctl_elem_info_is_user},

	{"owner",	(getter)pyalsahcontrolinfo_getowner,	NULL,	"get owner pid for this hcontrol element",	NULL},
	{"count",	(getter)pyalsahcontrolinfo_uint,	NULL,	"get count of values",			snd_ctl_elem_info_get_count},
	{"min",		(getter)pyalsahcontrolinfo_long,	NULL,	"get minimum limit value",		snd_ctl_elem_info_get_min},
	{"max",		(getter)pyalsahcontrolinfo_long,	NULL,	"get maximum limit value",		snd_ctl_elem_info_get_max},
	{"step",	(getter)pyalsahcontrolinfo_long,	NULL,	"get step value",			snd_ctl_elem_info_get_step},
	{"min64",	(getter)pyalsahcontrolinfo_longlong,	NULL,	"get 64-bit minimum limit value",	snd_ctl_elem_info_get_min64},
	{"max64",	(getter)pyalsahcontrolinfo_longlong,	NULL,	"get 64-bit maximum limit value",	snd_ctl_elem_info_get_max64},
	{"step64",	(getter)pyalsahcontrolinfo_longlong,	NULL,	"get 64-bit step value",		snd_ctl_elem_info_get_step64},
	{"items",	(getter)pyalsahcontrolinfo_getitems,	NULL,	"get count of enumerated items",	NULL},

	{"dimensions",	(getter)pyalsahcontrolinfo_dimensions,	NULL,	"get hcontrol element dimensions (in tuple)",	NULL},
	{"item_names",	(getter)pyalsahcontrolinfo_itemnames,	NULL,	"get enumerated item names (in tuple)",		NULL},
	
	{NULL}
};

static PyMethodDef pyalsahcontrolinfo_methods[] = {
	{NULL}
};

static PyTypeObject pyalsahcontrolinfo_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	tp_name:	"alsahcontrol.Info",
	tp_basicsize:	sizeof(struct pyalsahcontrolinfo),
	tp_dealloc:	(destructor)pyalsahcontrolinfo_dealloc,
	tp_flags:	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	tp_doc:		infoinit__doc__,
	tp_getset:	pyalsahcontrolinfo_getseters,
	tp_init:	(initproc)pyalsahcontrolinfo_init,
	tp_alloc:	PyType_GenericAlloc,
	tp_new:		PyType_GenericNew,
	tp_free:	PyObject_Del,
	tp_methods:	pyalsahcontrolinfo_methods,
};

/*
 * hcontrol value section
 */

#define PYHCTLVALUE(v) (((v) == Py_None) ? NULL : \
	((struct pyalsahcontrolvalue *)(v)))

struct pyalsahcontrolvalue {
	PyObject_HEAD
	PyObject *pyelem;
	snd_hctl_elem_t *elem;
	snd_ctl_elem_value_t *value;
};

static PyObject *
pyalsahcontrolvalue_element(struct pyalsahcontrolvalue *pyvalue, void *priv)
{
	Py_INCREF(pyvalue->pyelem);
	return pyvalue->pyelem;
}

typedef unsigned int (*fcn10)(void *);

static PyObject *
pyalsahcontrolvalue_uint(struct pyalsahcontrolvalue *pyvalue, void *fcn)
{
	return PyLong_FromLong(((fcn10)fcn)(pyvalue->value));
}

typedef const char * (*fcn11)(void *);

static PyObject *
pyalsahcontrolvalue_str(struct pyalsahcontrolvalue *pyvalue, void *fcn)
{
	return PyUnicode_FromString(((fcn5)fcn)(pyvalue->value));
}

static PyObject *
pyalsahcontrolvalue_id(struct pyalsahcontrolvalue *pyvalue, void *priv)
{
	snd_ctl_elem_id_t *id;
	
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_value_get_id(pyvalue->value, id);
	return id_to_python(id);
}

PyDoc_STRVAR(gettuple__doc__,
"get_tuple(type, count) -- Get hcontrol element values in tuple.");

PyDoc_STRVAR(getarray__doc__,
"get_array(type, count) -- Get hcontrol element values in array.");

static PyObject *
pyalsahcontrolvalue_get1(struct pyalsahcontrolvalue *self, PyObject *args, int list)
{
	int type;
	long i, count;
	snd_aes_iec958_t *iec958;
	PyObject *t, *o;

	if (!PyArg_ParseTuple(args, "il", &type, &count))
		return NULL;

	if (count <= 0)
		Py_RETURN_NONE;

	if (type == SND_CTL_ELEM_TYPE_IEC958) {
		if (count != 1)
			Py_RETURN_NONE;
		count = 3;
	}
	t = !list ? PyTuple_New(count) : PyList_New(count);
	if (t == NULL)
		return NULL;

	switch (type) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		for (i = 0; i < count; i++) {
 			o = get_bool(snd_ctl_elem_value_get_boolean(self->value, i));
			if (!list)
				PyTuple_SET_ITEM(t, i, o);
			else
				PyList_SetItem(t, i, o);
		}
		break;
	case SND_CTL_ELEM_TYPE_INTEGER:
		for (i = 0; i < count; i++) {
			o = PyInt_FromLong(snd_ctl_elem_value_get_integer(self->value, i));
			if (!list)
				PyTuple_SET_ITEM(t, i, o);
			else
				PyList_SetItem(t, i, o);
		}
		break;
	case SND_CTL_ELEM_TYPE_INTEGER64:
		for (i = 0; i < count; i++) {
			o = PyLong_FromLongLong(snd_ctl_elem_value_get_integer64(self->value, i));
			if (!list)
				PyTuple_SET_ITEM(t, i, o);
			else
				PyList_SetItem(t, i, o);
		}
		break;
	case SND_CTL_ELEM_TYPE_ENUMERATED:
		for (i = 0; i < count; i++) {
			o = PyInt_FromLong(snd_ctl_elem_value_get_enumerated(self->value, i));
			if (!list) {
				PyTuple_SET_ITEM(t, i, o);
			} else {
				PyList_SetItem(t, i, o);
			}
		}
		break;
	case SND_CTL_ELEM_TYPE_BYTES:
		for (i = 0; i < count; i++) {
			o = PyInt_FromLong(snd_ctl_elem_value_get_byte(self->value, i));
			if (!list) {
				PyTuple_SET_ITEM(t, i, o);
			} else {
				PyList_SetItem(t, i, o);
			}
		}
		break;
	case SND_CTL_ELEM_TYPE_IEC958:
		iec958 = malloc(sizeof(*iec958));
		if (iec958 == NULL) {
			Py_DECREF(t);
			Py_RETURN_NONE;
		}
		snd_ctl_elem_value_get_iec958(self->value, iec958);
		if (!list) {
			PyTuple_SET_ITEM(t, 0, PyBytes_FromStringAndSize((char *)iec958->status, sizeof(iec958->status)));
			PyTuple_SET_ITEM(t, 1, PyBytes_FromStringAndSize((char *)iec958->subcode, sizeof(iec958->subcode)));
			PyTuple_SET_ITEM(t, 2, PyBytes_FromStringAndSize((char *)iec958->dig_subframe, sizeof(iec958->dig_subframe)));
		} else {
			PyList_SetItem(t, 0, PyBytes_FromStringAndSize((char *)iec958->status, sizeof(iec958->status)));
			PyList_SetItem(t, 1, PyBytes_FromStringAndSize((char *)iec958->subcode, sizeof(iec958->subcode)));
			PyList_SetItem(t, 2, PyBytes_FromStringAndSize((char *)iec958->dig_subframe, sizeof(iec958->dig_subframe)));
		}
		free(iec958);
		break;
	default:
		Py_DECREF(t); t = NULL;
		PyErr_Format(PyExc_TypeError, "Unknown hcontrol element type %i", type);
		break;
	}

	return t;
}

static PyObject *
pyalsahcontrolvalue_gettuple(struct pyalsahcontrolvalue *self, PyObject *args)
{
	return pyalsahcontrolvalue_get1(self, args, 0);
}


static PyObject *
pyalsahcontrolvalue_getarray(struct pyalsahcontrolvalue *self, PyObject *args)
{
	return pyalsahcontrolvalue_get1(self, args, 1);
}


PyDoc_STRVAR(settuple__doc__,
"set_tuple(type, val) -- Set hcontrol element values from tuple.");

PyDoc_STRVAR(setarray__doc__,
"set_array(type, val) -- Set hcontrol element values from array.");

static PyObject *
pyalsahcontrolvalue_settuple(struct pyalsahcontrolvalue *self, PyObject *args)
{
	int type, list;
	Py_ssize_t len;
	long i, count, lval;
	snd_aes_iec958_t *iec958;
	PyObject *t, *v;
	char *str;

	if (!PyArg_ParseTuple(args, "iO", &type, &t))
		return NULL;

	list = PyList_Check(t);
	if (!PyTuple_Check(t) && !list) {
		PyErr_SetString(PyExc_TypeError, "Tuple expected as val argument!");
		return NULL;
	}

	count = !list ? PyTuple_Size(t) : PyList_Size(t);

	switch (type) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		for (i = 0; i < count; i++) {
			v = !list ? PyTuple_GetItem(t, i) : PyList_GetItem(t, i);
			if (v == Py_None)
				continue;
			Py_INCREF(v);
			if (get_long(v, &lval))
				break;
			snd_ctl_elem_value_set_boolean(self->value, i, lval);
		}
		break;
	case SND_CTL_ELEM_TYPE_INTEGER:
		for (i = 0; i < count; i++) {
			v = !list ? PyTuple_GetItem(t, i) : PyList_GetItem(t, i);
			if (v == Py_None)
				continue;
			Py_INCREF(v);
			if (get_long(v, &lval))
				break;
			snd_ctl_elem_value_set_integer(self->value, i, lval);
		}
		break;
	case SND_CTL_ELEM_TYPE_INTEGER64:
		for (i = 0; i < count; i++) {
			v = !list ? PyTuple_GetItem(t, i) : PyList_GetItem(t, i);
			if (v == Py_None)
				continue;
			Py_INCREF(v);
			snd_ctl_elem_value_set_integer64(self->value, i, PyLong_AsLongLong(v));
		}
		break;
	case SND_CTL_ELEM_TYPE_ENUMERATED:
		for (i = 0; i < count; i++) {
			v = !list ? PyTuple_GetItem(t, i) : PyList_GetItem(t, i);
			if (v == Py_None)
				continue;
			Py_INCREF(v);
			if (get_long(v, &lval))
				break;
			snd_ctl_elem_value_set_enumerated(self->value, i, lval);
		}
		break;
	case SND_CTL_ELEM_TYPE_BYTES:
		for (i = 0; i < count; i++) {
			v = !list ? PyTuple_GetItem(t, i) : PyList_GetItem(t, i);
			if (v == Py_None)
				continue;
			Py_INCREF(v);
			if (get_long(v, &lval))
				break;
			snd_ctl_elem_value_set_byte(self->value, i, lval);
		}
		break;
	case SND_CTL_ELEM_TYPE_IEC958:
		if (count != 3) {
			PyErr_SetString(PyExc_TypeError, "Tuple with len == 3 expected for IEC958 type!");
			return NULL;
		}
		iec958 = calloc(1, sizeof(*iec958));
		if (iec958 == NULL) {
			Py_DECREF(t);
			Py_RETURN_NONE;
		}
		v = !list ? PyTuple_GET_ITEM(t, 0) : PyList_GetItem(t, 0);
		Py_INCREF(v);
		if (!PyBytes_Check(v))
			goto err1;
		len = PyBytes_Size(v);
		if (len > (Py_ssize_t)sizeof(iec958->status))
			len = (Py_ssize_t)sizeof(iec958->status);
		str = PyBytes_AsString(v);
		memcpy(iec958->status, str, (size_t)len);
		v = !list ? PyTuple_GET_ITEM(t, 1) : PyList_GetItem(t, 1);
		Py_INCREF(v);
		if (!PyBytes_Check(v))
			goto err1;
		len = PyBytes_Size(v);
		if (len > (Py_ssize_t)sizeof(iec958->subcode))
			len = (Py_ssize_t)sizeof(iec958->subcode);
		str = PyBytes_AsString(v);
		memcpy(iec958->subcode, str, (size_t)len);
		v = !list ? PyTuple_GET_ITEM(t, 2) : PyList_GetItem(t, 2);
		Py_INCREF(v);
		if (!PyBytes_Check(v))
			goto err1;
		len = PyBytes_Size(v);
		if (len > (Py_ssize_t)sizeof(iec958->dig_subframe))
			len = (Py_ssize_t)sizeof(iec958->dig_subframe);
		str = PyBytes_AsString(v);
		memcpy(iec958->dig_subframe, str, (size_t)len);
		snd_ctl_elem_value_set_iec958(self->value, iec958);
		free(iec958);
		break;
	      err1:
		PyErr_SetString(PyExc_TypeError, "Invalid tuple IEC958 type!");
	       	free(iec958);
	       	break;
	default:
		PyErr_Format(PyExc_TypeError, "Unknown hcontrol element type %i", type);
		break;
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(read__doc__,
"read() -- Read value state.\n");

static PyObject *
pyalsahcontrolvalue_read(struct pyalsahcontrolvalue *self, PyObject *args)
{
	int res;

	res = snd_hctl_elem_read(self->elem, self->value);
	if (res < 0)
		return PyErr_Format(PyExc_IOError, "hcontrol element read error: %s", snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(write__doc__,
"write() -- Write new value state.\n");

static PyObject *
pyalsahcontrolvalue_write(struct pyalsahcontrolvalue *self, PyObject *args)
{
	int res;

	res = snd_hctl_elem_write(self->elem, self->value);
	if (res < 0)
		return PyErr_Format(PyExc_IOError, "hcontrol element write error: %s", snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(valueinit__doc__,
"Value(elem)\n"
"  -- Create a hcontrol element value object.\n"
"Note: Value state is read from device!\n");

static int
pyalsahcontrolvalue_init(struct pyalsahcontrolvalue *pyvalue, PyObject *args, PyObject *kwds)
{
	PyObject *elem;
	int res;

	pyvalue->pyelem = NULL;
	pyvalue->elem = NULL;
	pyvalue->value = NULL;

	if (!PyArg_ParseTuple(args, "O", &elem))
		return -1;

	if (elem->ob_type != &pyalsahcontrolelement_type) {
		PyErr_SetString(PyExc_TypeError, "bad type for element argument");
		return -1;
	}

	if (snd_ctl_elem_value_malloc(&pyvalue->value)) {
		PyErr_SetString(PyExc_TypeError, "malloc problem");
		return -1;
	}

	pyvalue->pyelem = elem;
	Py_INCREF(elem);
	pyvalue->elem = PYHCTLELEMENT(elem)->elem;

	res = snd_hctl_elem_read(pyvalue->elem, pyvalue->value);
	if (res < 0) {
		PyErr_Format(PyExc_IOError, "hcontrol element value read problem: %s", snd_strerror(-res));
		return -1;
	}	

	return 0;
}

static void
pyalsahcontrolvalue_dealloc(struct pyalsahcontrolvalue *self)
{
	if (self->value)
		snd_ctl_elem_value_free(self->value);
	Py_XDECREF(self->pyelem);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyGetSetDef pyalsahcontrolvalue_getseters[] = {

	{"element",	(getter)pyalsahcontrolvalue_element,	NULL,	"hcontrol element for this value",	NULL},

	{"id",		(getter)pyalsahcontrolvalue_id,		NULL,	"hcontrol element full id",	snd_ctl_elem_value_get_id},

	{"numid",	(getter)pyalsahcontrolvalue_uint,	NULL,	"hcontrol element numid",	snd_ctl_elem_value_get_numid},
	{"interface",	(getter)pyalsahcontrolvalue_uint,	NULL,	"hcontrol element interface",	snd_ctl_elem_value_get_interface},
	{"device",	(getter)pyalsahcontrolvalue_uint,	NULL,	"hcontrol element device",	snd_ctl_elem_value_get_device},
	{"subdevice",	(getter)pyalsahcontrolvalue_uint,	NULL,	"hcontrol element subdevice",	snd_ctl_elem_value_get_subdevice},
	{"name",	(getter)pyalsahcontrolvalue_str,	NULL,	"hcontrol element name",	snd_ctl_elem_value_get_name},
	{"index",	(getter)pyalsahcontrolvalue_uint,	NULL,	"hcontrol element index",	snd_ctl_elem_value_get_index},
	
	{NULL}
};

static PyMethodDef pyalsahcontrolvalue_methods[] = {

	{"get_tuple",	(PyCFunction)pyalsahcontrolvalue_gettuple,	METH_VARARGS,	gettuple__doc__},
	{"get_array",	(PyCFunction)pyalsahcontrolvalue_getarray,	METH_VARARGS,	getarray__doc__},
	{"set_tuple",	(PyCFunction)pyalsahcontrolvalue_settuple,	METH_VARARGS,	settuple__doc__},
	{"set_array",	(PyCFunction)pyalsahcontrolvalue_settuple,	METH_VARARGS,	setarray__doc__},

	{"read",	(PyCFunction)pyalsahcontrolvalue_read,		METH_NOARGS,	read__doc__},
	{"write",	(PyCFunction)pyalsahcontrolvalue_write,		METH_NOARGS,	write__doc__},

	{NULL}
};

static PyTypeObject pyalsahcontrolvalue_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	tp_name:	"alsahcontrol.Value",
	tp_basicsize:	sizeof(struct pyalsahcontrolvalue),
	tp_dealloc:	(destructor)pyalsahcontrolvalue_dealloc,
	tp_flags:	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	tp_doc:		valueinit__doc__,
	tp_getset:	pyalsahcontrolvalue_getseters,
	tp_init:	(initproc)pyalsahcontrolvalue_init,
	tp_alloc:	PyType_GenericAlloc,
	tp_new:		PyType_GenericNew,
	tp_free:	PyObject_Del,
	tp_methods:	pyalsahcontrolvalue_methods,
};

/*
 *
 */

static PyMethodDef pyalsahcontrolparse_methods[] = {
	{NULL}
};

MOD_INIT(alsahcontrol)
{
	PyObject *d, *d1, *l1, *o;
	int i;

	if (PyType_Ready(&pyalsahcontrol_type) < 0)
		return MOD_ERROR_VAL;
	if (PyType_Ready(&pyalsahcontrolelement_type) < 0)
		return MOD_ERROR_VAL;
	if (PyType_Ready(&pyalsahcontrolinfo_type) < 0)
		return MOD_ERROR_VAL;
	if (PyType_Ready(&pyalsahcontrolvalue_type) < 0)
		return MOD_ERROR_VAL;

	MOD_DEF(module, "alsahcontrol", "libasound hcontrol wrapper", pyalsahcontrolparse_methods);
	if (module == NULL)
		return MOD_ERROR_VAL;

#if 0
	buildin = PyImport_AddModule("__buildin__");
	if (buildin == NULL)
		return MOD_ERROR_VAL;
	if (PyObject_SetAttrString(module, "__buildins__", buildin) < 0)
		return MOD_ERROR_VAL;
#endif

	Py_INCREF(&pyalsahcontrol_type);
	PyModule_AddObject(module, "HControl", (PyObject *)&pyalsahcontrol_type);

	Py_INCREF(&pyalsahcontrolelement_type);
	PyModule_AddObject(module, "Element", (PyObject *)&pyalsahcontrolelement_type);

	Py_INCREF(&pyalsahcontrolinfo_type);
	PyModule_AddObject(module, "Info", (PyObject *)&pyalsahcontrolinfo_type);

	Py_INCREF(&pyalsahcontrolvalue_type);
	PyModule_AddObject(module, "Value", (PyObject *)&pyalsahcontrolvalue_type);

	d = PyModule_GetDict(module);

	/* ---- */

	d1 = PyDict_New();

#define add_space1(pname, name) { \
	o = PyInt_FromLong(SND_CTL_ELEM_IFACE_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space1("CARD", CARD);
	add_space1("HWDEP", HWDEP);
	add_space1("MIXER", MIXER);
	add_space1("PCM", PCM);
	add_space1("RAWMIDI", RAWMIDI);
	add_space1("TIMER", TIMER);
	add_space1("SEQUENCER", SEQUENCER);
	add_space1("LAST", LAST);

	PyDict_SetItemString(d, "interface_id", d1);
	Py_DECREF(d1);

	/* ---- */

	l1 = PyList_New(0);

	for (i = 0; i <= SND_CTL_ELEM_IFACE_LAST; i++) {
		o = PyUnicode_FromString(snd_ctl_elem_iface_name(i));
		PyList_Append(l1, o);
		Py_DECREF(o);
	}

	PyDict_SetItemString(d, "interface_name", l1);
	Py_DECREF(l1);

	/* ---- */

	d1 = PyDict_New();
	
#define add_space2(pname, name) { \
	o = PyInt_FromLong(SND_CTL_ELEM_TYPE_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space2("NONE", NONE);
	add_space2("BOOLEAN", BOOLEAN);
	add_space2("INTEGER", INTEGER);
	add_space2("ENUMERATED", ENUMERATED);
	add_space2("BYTES", BYTES);
	add_space2("IEC958", IEC958);
	add_space2("INTEGER64", INTEGER64);
	add_space2("LAST", LAST);

	PyDict_SetItemString(d, "element_type", d1);
	Py_DECREF(d1);
	
	/* ---- */

	l1 = PyList_New(0);

	for (i = 0; i <= SND_CTL_ELEM_TYPE_LAST; i++) {
		o = PyUnicode_FromString(snd_ctl_elem_type_name(i));
		PyList_Append(l1, o);
		Py_DECREF(o);
	}

	PyDict_SetItemString(d, "element_type_name", l1);
	Py_DECREF(l1);

	/* ---- */

	d1 = PyDict_New();
	
#define add_space3(pname, name) { \
	o = PyInt_FromLong(SND_CTL_EVENT_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space3("ELEMENT", ELEM);
	add_space3("LAST", LAST);

	PyDict_SetItemString(d, "event_class", d1);
	Py_DECREF(d1);

	/* ---- */

	d1 = PyDict_New();
	
#define add_space4(pname, name) { \
	o = PyInt_FromLong(SND_CTL_EVENT_MASK_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space4("VALUE", VALUE);
	add_space4("INFO", INFO);
	add_space4("ADD", ADD);
	add_space4("TLV", TLV);

	PyDict_SetItemString(d, "event_mask", d1);
	Py_DECREF(d1);

	o = PyInt_FromLong(SND_CTL_EVENT_MASK_REMOVE);
	PyDict_SetItemString(d, "event_mask_remove", o);
	Py_DECREF(o);

	/* ---- */

	d1 = PyDict_New();
	
#define add_space5(pname, name) { \
	o = PyInt_FromLong(SND_CTL_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space5("NONBLOCK", NONBLOCK);
	add_space5("ASYNC", ASYNC);
	add_space5("READONLY", READONLY);

	PyDict_SetItemString(d, "open_mode", d1);
	Py_DECREF(d1);

	/* ---- */

	main_interpreter = PyThreadState_Get()->interp;

	if (PyErr_Occurred())
		Py_FatalError("Cannot initialize module alsahcontrol");

	return MOD_SUCCESS_VAL(module);
}

/*
 *  element event callback
 */

static int element_callback(snd_hctl_elem_t *elem, unsigned int mask)
{
	struct pyalsahcontrolelement *pyhelem;
	PyObject *o, *t, *r;
	int res = 0, inside = 1;
	CALLBACK_VARIABLES;

	if (elem == NULL)
		return -EINVAL;
	pyhelem = snd_hctl_elem_get_callback_private(elem);
	if (pyhelem == NULL || pyhelem->callback == NULL)
		return -EINVAL;

	CALLBACK_INIT;

	o = PyObject_GetAttr(pyhelem->callback, InternFromString("callback"));
	if (!o) {
		PyErr_Clear();
		o = pyhelem->callback;
		inside = 0;
	}

	t = PyTuple_New(2);
	if (t) {
		PyTuple_SET_ITEM(t, 0, (PyObject *)pyhelem);
		Py_INCREF(pyhelem);
		PyTuple_SET_ITEM(t, 1, PyInt_FromLong(mask));
		r = PyObject_CallObject(o, t);
		Py_DECREF(t);
			
		if (r) {
			if (PyLong_Check(r)) {
				res = PyLong_AsLong(r);
#if PY_MAJOR_VERSION < 3
			} else if (PyInt_Check(r)) {
				res = PyInt_AsLong(r);
#endif
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

	CALLBACK_DONE;

	return res;
}
