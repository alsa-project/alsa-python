/*
 *  Python binding for the ALSA library - card management
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

static PyObject *module;

/*
 *
 */

PyDoc_STRVAR(asoundlib_version__doc__,
"asoundlib_version() -- Return asoundlib version string.");

static PyObject *
asoundlib_version(PyObject *self, PyObject *args)
{
	return Py_BuildValue("s", snd_asoundlib_version());
}

PyDoc_STRVAR(card_load__doc__,
"card_load([card]) -- Load driver for given card.");

static PyObject *
card_load(PyObject *self, PyObject *args, PyObject *kwds)
{
	int card = 0;
	static char * kwlist[] = { "card", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist, &card))
		Py_RETURN_NONE;

	return Py_BuildValue("i", snd_card_load(card));
}

PyDoc_STRVAR(card_list__doc__,
"card_list() -- Get a card number list in tuple.");

static PyObject *
card_list(PyObject *self, PyObject *args)
{
	PyObject *t;
	int size = 0, i = -1, res;
	
	t = PyTuple_New(0);
	if (!t)
		return NULL;
	while (1) {
		res = snd_card_next(&i);
		if (res) {
			Py_DECREF(t);
			return PyErr_Format(PyExc_IOError, "Cannot get next card: %s", snd_strerror(res));
		}
		if (i < 0)
			break;
		size++;
		if (_PyTuple_Resize(&t, size))
			return NULL;
		PyTuple_SET_ITEM(t, size - 1, PyInt_FromLong(i));
	}
	return t;
}

PyDoc_STRVAR(card_get_index__doc__,
"card_get_index(name) -- Get card index using ID string.");

static PyObject *
card_get_index(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *card;
	static char * kwlist[] = { "name", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &card))
		Py_RETURN_NONE;

	return Py_BuildValue("i", snd_card_get_index(card));
}

PyDoc_STRVAR(card_get_name__doc__,
"card_get_name(card) -- Get card name string using card index.");

static PyObject *
card_get_name(PyObject *self, PyObject *args, PyObject *kwds)
{
	int card, res;
	char *str;
	static char * kwlist[] = { "card", NULL };
	PyObject *t;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &card))
		Py_RETURN_NONE;

	res = snd_card_get_name(card, &str);
	if (res)
		return PyErr_Format(PyExc_IOError, "Cannot get card name: %s", snd_strerror(res));
	t = Py_BuildValue("s", str);
	free(str);
	return t;
}

PyDoc_STRVAR(card_get_longname__doc__,
"card_get_longname(card) -- Get long card name string using card index.");

static PyObject *
card_get_longname(PyObject *self, PyObject *args, PyObject *kwds)
{
	int card, res;
	char *str;
	static char * kwlist[] = { "card", NULL };
	PyObject *t;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &card))
		Py_RETURN_NONE;

	res = snd_card_get_longname(card, &str);
	if (res)
		return PyErr_Format(PyExc_IOError, "Cannot get card longname: %s", snd_strerror(res));
	t = Py_BuildValue("s", str);
	free(str);
	return t;
}

PyDoc_STRVAR(device_name_hint__doc__,
"device_name_hint(card, interface) -- Get device name hints.");

static PyObject *
device_name_hint(PyObject *self, PyObject *args, PyObject *kwds)
{
	int card, res;
	char *iface, **hint;
	void **hints;
	static char * kwlist[] = { "card", "interface", NULL };
	static char * ids[] = { "NAME", "DESC", "IOID", NULL };
	PyObject *l, *d, *v;
	char **id, *str;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "is", kwlist, &card, &iface))
		Py_RETURN_NONE;

	res = snd_device_name_hint(card, iface, &hints);
	if (res)
		return PyErr_Format(PyExc_IOError, "Cannot get card longname: %s", snd_strerror(res));

	l = PyList_New(0);

	hint = (char **)hints;
	while (*hint) {
		d = PyDict_New();
		if (d == NULL)
			goto err1;
		id = ids;
		while (*id) {
			str = snd_device_name_get_hint(*hint, *id);
			if (str == NULL)
				break;
			v = PyString_FromString(str);
			free(str);
			if (v == NULL)
				goto err1;
			if (PyDict_SetItemString(d, *id, v))
				goto err1;
			id++;
		}
		if (PyList_Append(l, d))
			goto err1;
		hint++;
	}
	
	snd_device_name_free_hint(hints);
	return l;

      err1:
      	Py_XDECREF(d);
      	Py_XDECREF(l);
	snd_device_name_free_hint(hints);
	return NULL;
}

/*
 *
 */

static PyMethodDef pyalsacardparse_methods[] = {
	{"asoundlib_version", (PyCFunction)asoundlib_version, METH_NOARGS, asoundlib_version__doc__},
	{"card_load", (PyCFunction)card_load, METH_VARARGS|METH_KEYWORDS, card_load__doc__},
	{"card_list", (PyCFunction)card_list, METH_NOARGS, card_list__doc__},
	{"card_get_index", (PyCFunction)card_get_index, METH_VARARGS|METH_KEYWORDS, card_get_index__doc__},
	{"card_get_name", (PyCFunction)card_get_name, METH_VARARGS|METH_KEYWORDS, card_get_name__doc__},
	{"card_get_longname", (PyCFunction)card_get_longname, METH_VARARGS|METH_KEYWORDS, card_get_longname__doc__},
	{"device_name_hint", (PyCFunction)device_name_hint, METH_VARARGS|METH_KEYWORDS, device_name_hint__doc__},
	{NULL}
};

PyMODINIT_FUNC
initalsacard(void)
{
	module = Py_InitModule3("alsacard", pyalsacardparse_methods, "libasound alsacard wrapper");
	if (module == NULL)
		return;

	if (PyErr_Occurred())
		Py_FatalError("Cannot initialize module alsacard");
}
