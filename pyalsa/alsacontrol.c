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
#include "stdlib.h"
#include "alsa/asoundlib.h"

/*
 *
 */

static PyObject *module;
#if 0
static PyObject *buildin;
#endif

/*
 *
 */

#define PYCTL(v) \
	(((v) == Py_None) ? NULL : ((struct pyalsacontrol *)(v)))

struct pyalsacontrol {
	PyObject_HEAD
	snd_ctl_t *handle;
};

PyDoc_STRVAR(cardinfo__doc__,
"card_info() -- Return a dictionary with card specific information.");

static PyObject *
pyalsacontrol_cardinfo(struct pyalsacontrol *self, PyObject *args)
{
	snd_ctl_card_info_t *info;
	PyObject *d;

	snd_ctl_card_info_alloca(&info);
	int err = snd_ctl_card_info(self->handle, info);
	if (err < 0) {
		PyErr_Format(PyExc_IOError,
		     "Control card info error: %s", strerror(-err));
		return NULL;
	}
	d = PyDict_New();
	if (d) {
		PyDict_SetItem(d, PyUnicode_FromString("card"), PyInt_FromLong(snd_ctl_card_info_get_card(info)));
		PyDict_SetItem(d, PyUnicode_FromString("id"), PyUnicode_FromString(snd_ctl_card_info_get_id(info)));
		PyDict_SetItem(d, PyUnicode_FromString("driver"), PyUnicode_FromString(snd_ctl_card_info_get_driver(info)));
		PyDict_SetItem(d, PyUnicode_FromString("name"), PyUnicode_FromString(snd_ctl_card_info_get_driver(info)));
		PyDict_SetItem(d, PyUnicode_FromString("longname"), PyUnicode_FromString(snd_ctl_card_info_get_longname(info)));
		PyDict_SetItem(d, PyUnicode_FromString("mixername"), PyUnicode_FromString(snd_ctl_card_info_get_mixername(info)));
		PyDict_SetItem(d, PyUnicode_FromString("components"), PyUnicode_FromString(snd_ctl_card_info_get_components(info)));
	}
	return d;
}

static PyObject *
devices(struct pyalsacontrol *self, PyObject *args,
			   int (*fcn)(snd_ctl_t *, int *))
{
	int dev, err, size = 0;
	PyObject *t;

	dev = -1;
	t = PyTuple_New(size);
	do {
		err = fcn(self->handle, &dev);
		if (err < 0) {
			PyErr_Format(PyExc_IOError,
			     "Control hwdep_next_device error: %s", strerror(-err));
			Py_DECREF(t);
			return NULL;
		}
		if (dev >= 0) {
			_PyTuple_Resize(&t, ++size);
			if (t)
				PyTuple_SetItem(t, size-1, PyInt_FromLong(dev));
		}
	} while (dev >= 0);
	return t;
}

PyDoc_STRVAR(hwdepdevices__doc__,
"hwdep_devices() -- Return a tuple with available hwdep devices.");

static PyObject *
pyalsacontrol_hwdepdevices(struct pyalsacontrol *self, PyObject *args)
{
	return devices(self, args, snd_ctl_hwdep_next_device);
}

PyDoc_STRVAR(pcmdevices__doc__,
"pcm_devices() -- Return a tuple with available PCM devices.");

static PyObject *
pyalsacontrol_pcmdevices(struct pyalsacontrol *self, PyObject *args)
{
	return devices(self, args, snd_ctl_pcm_next_device);
}

PyDoc_STRVAR(rawmididevices__doc__,
"rawmidi_devices() -- Return a tuple with available RawMidi devices.");

static PyObject *
pyalsacontrol_rawmididevices(struct pyalsacontrol *self, PyObject *args)
{
	return devices(self, args, snd_ctl_rawmidi_next_device);
}

PyDoc_STRVAR(alsacontrolinit__doc__,
"Control([name='default'],[mode=0])\n"
"  -- Open an ALSA Control device.\n");

static int
pyalsacontrol_init(struct pyalsacontrol *pyctl, PyObject *args, PyObject *kwds)
{
	char *name = "default";
	int mode = 0, err;

	static char * kwlist[] = { "name", "mode", NULL };

	pyctl->handle = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|si", kwlist, &name, &mode))
		return -1;

	err = snd_ctl_open(&pyctl->handle, name, mode);
	if (err < 0) {
		PyErr_Format(PyExc_IOError,
			     "Control open error: %s", strerror(-err));
		return -1;
	}

	return 0;
}

static void
pyalsacontrol_dealloc(struct pyalsacontrol *self)
{
	if (self->handle != NULL)
		snd_ctl_close(self->handle);
}

static PyGetSetDef pyalsacontrol_getseters[] = {

	{NULL}
};

static PyMethodDef pyalsacontrol_methods[] = {

	{"card_info",	(PyCFunction)pyalsacontrol_cardinfo,	METH_NOARGS,	cardinfo__doc__},
	{"hwdep_devices",(PyCFunction)pyalsacontrol_hwdepdevices,	METH_NOARGS,	hwdepdevices__doc__},
	{"pcm_devices",	(PyCFunction)pyalsacontrol_pcmdevices,	METH_NOARGS,	pcmdevices__doc__},
	{"rawmidi_devices",(PyCFunction)pyalsacontrol_rawmididevices,	METH_NOARGS,	rawmididevices__doc__},
	{NULL}
};

static PyTypeObject pyalsacontrol_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	tp_name:	"alsacontrol.Control",
	tp_basicsize:	sizeof(struct pyalsacontrol),
	tp_dealloc:	(destructor)pyalsacontrol_dealloc,
	tp_flags:	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	tp_doc:		alsacontrolinit__doc__,
	tp_getset:	pyalsacontrol_getseters,
	tp_init:	(initproc)pyalsacontrol_init,
	tp_alloc:	PyType_GenericAlloc,
	tp_new:		PyType_GenericNew,
	tp_free:	PyObject_Del,
	tp_methods:	pyalsacontrol_methods,
};

/*
 *
 */

static PyMethodDef pyalsacontrolparse_methods[] = {
	{NULL}
};

MOD_INIT(alsacontrol)
{
	PyObject *d, *d1, *l1, *o;
	int i;

	pyalsacontrol_type.tp_free = PyObject_GC_Del;

	if (PyType_Ready(&pyalsacontrol_type) < 0)
		return MOD_ERROR_VAL;

	MOD_DEF(module, "alsacontrol", "libasound control wrapper", pyalsacontrolparse_methods);
	if (module == NULL)
		return MOD_ERROR_VAL;

#if 0
	buildin = PyImport_AddModule("__buildin__");
	if (buildin == NULL)
		return MOD_ERROR_VAL;
	if (PyObject_SetAttrString(module, "__buildins__", buildin) < 0)
		return MOD_ERROR_VAL;
#endif

	Py_INCREF(&pyalsacontrol_type);
	PyModule_AddObject(module, "Control", (PyObject *)&pyalsacontrol_type);

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

	if (PyErr_Occurred())
		Py_FatalError("Cannot initialize module alsacontrol");

	return MOD_SUCCESS_VAL(module);
}
