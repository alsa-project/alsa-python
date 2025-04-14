/*
 *  Python binding for the ALSA library - mixer
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

static int element_callback(snd_mixer_elem_t *elem, unsigned int mask);

static PyObject *module;
#if 0
static PyObject *buildin;
#endif
static PyInterpreterState *main_interpreter;

/*
 *
 */

#define PYALSAMIXER(v) (((v) == Py_None) ? NULL : \
	((struct pyalsamixer *)(v)))

struct pyalsamixer {
	PyObject_HEAD
	snd_mixer_t *handle;
};

static PyObject *
pyalsamixer_getcount(struct pyalsamixer *self, void *priv)
{
	return PyLong_FromLong(snd_mixer_get_count(self->handle));
}

PyDoc_STRVAR(attach__doc__,
"attach(card=None, [, abstract=]) -- Attach mixer to a sound card.");

static PyObject *
pyalsamixer_attach(struct pyalsamixer *self, PyObject *args, PyObject *kwds)
{
	char *card = "default";
	int abstract = -1, res;
	static char * kwlist[] = { "card", "abstract", NULL };
	struct snd_mixer_selem_regopt *options = NULL, _options;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|si", kwlist, &card, &abstract))
		Py_RETURN_NONE;

	if (abstract < 0) {
		res = snd_mixer_attach(self->handle, card);
		if (res < 0)
			return PyErr_Format(PyExc_RuntimeError, "Cannot attach card '%s': %s", card, snd_strerror(-res));
		abstract = -1;
	}
	if (abstract >= 0) {
		memset(&_options, 0, sizeof(_options));
		_options.ver = 1;
		_options.abstract = abstract;
		_options.device = card;
		options = &_options;
	}
	res = snd_mixer_selem_register(self->handle, options, NULL);
	if (res < 0)
		return PyErr_Format(PyExc_RuntimeError, "Cannot register simple mixer (abstract %i): %s", abstract, snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(load__doc__,
"load() -- Load mixer elements.");

static PyObject *
pyalsamixer_load(struct pyalsamixer *self, PyObject *args)
{
	int res = snd_mixer_load(self->handle);
	if (res < 0)
		return PyErr_Format(PyExc_RuntimeError, "Cannot load mixer elements: %s", snd_strerror(-res));
	return Py_BuildValue("i", res);
}

PyDoc_STRVAR(free__doc__,
"free() -- Free mixer elements and all related resources.");

static PyObject *
pyalsamixer_free(struct pyalsamixer *self, PyObject *args)
{
	snd_mixer_free(self->handle);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(handlevents__doc__,
"handle_events() -- Process waiting mixer events (and call appropriate callbacks).");

static PyObject *
pyalsamixer_handleevents(struct pyalsamixer *self, PyObject *args)
{
	int err;

	Py_BEGIN_ALLOW_THREADS;
	err = snd_mixer_handle_events(self->handle);
	Py_END_ALLOW_THREADS;
	if (err < 0)
		PyErr_Format(PyExc_IOError,
		     "Alsamixer handle events error: %s", strerror(-err));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(registerpoll__doc__,
"register_poll(pollObj) -- Register poll file descriptors.");

static PyObject *
pyalsamixer_registerpoll(struct pyalsamixer *self, PyObject *args)
{
	PyObject *pollObj, *reg, *t;
	struct pollfd *pfd;
	int i, count;

	if (!PyArg_ParseTuple(args, "O", &pollObj))
		return NULL;

	count = snd_mixer_poll_descriptors_count(self->handle);
	if (count <= 0)
		Py_RETURN_NONE;
	pfd = alloca(sizeof(struct pollfd) * count);
	count = snd_mixer_poll_descriptors(self->handle, pfd, count);
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
pyalsamixer_getpollfds(struct pyalsamixer *self, void *priv)
{
	PyObject *l, *t;
	struct pollfd *pfds;
	int i, count;

	count = snd_mixer_poll_descriptors_count(self->handle);
	if (count < 0) {
pfds_error:
		PyErr_Format(PyExc_IOError, "poll descriptors error: %s", snd_strerror(count));
		return NULL;
	}
	pfds = alloca(sizeof(struct pollfd) * count);
	count = snd_mixer_poll_descriptors(self->handle, pfds, count);
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
"list() -- Return a list (tuple) of element IDs in (name,index) tuple.");

static PyObject *
pyalsamixer_list(struct pyalsamixer *self, PyObject *args)
{
	PyObject *t, *v;
	int i, count;
	snd_mixer_elem_t *elem;

	count = snd_mixer_get_count(self->handle);
	t = PyTuple_New(count);
	if (count == 0)
		return t;
	elem = snd_mixer_first_elem(self->handle);
	for (i = 0; i < count; i++) {
		v = NULL;
		if (elem) {
			v = PyTuple_New(2);
			PyTuple_SET_ITEM(v, 0, PyUnicode_FromString(snd_mixer_selem_get_name(elem)));
			PyTuple_SET_ITEM(v, 1, PyInt_FromLong(snd_mixer_selem_get_index(elem)));
		}
		if (v == NULL || elem == NULL) {
			v = Py_None;
			Py_INCREF(v);
		}
		PyTuple_SET_ITEM(t, i, v);
		elem = snd_mixer_elem_next(elem);
	}
	return t;
}

PyDoc_STRVAR(alsamixerinit__doc__,
"Mixer([mode=0])\n"
"  -- Open an ALSA mixer device.\n");

static int
pyalsamixer_init(struct pyalsamixer *pymix, PyObject *args, PyObject *kwds)
{
	int mode = 0, err;

	static char * kwlist[] = { "mode", NULL };

	pymix->handle = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist, &mode))
		return -1;

	err = snd_mixer_open(&pymix->handle, mode);
	if (err < 0) {
		PyErr_Format(PyExc_IOError,
			     "Alsamixer open error: %s", strerror(-err));
		return -1;
	}

	return 0;
}

static void
pyalsamixer_dealloc(struct pyalsamixer *self)
{
	if (self->handle != NULL)
		snd_mixer_close(self->handle);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyGetSetDef pyalsamixer_getseters[] = {

	{"count",	(getter)pyalsamixer_getcount,   NULL,	"mixer element count",		NULL},
	{"poll_fds",    (getter)pyalsamixer_getpollfds,     NULL,	"list of (fd, eventbits) tuples",	NULL},

	{NULL}
};

static PyMethodDef pyalsamixer_methods[] = {

	{"attach",	(PyCFunction)pyalsamixer_attach, METH_VARARGS|METH_KEYWORDS,	attach__doc__},
	{"load",	(PyCFunction)pyalsamixer_load,	METH_NOARGS,	load__doc__},
	{"free",	(PyCFunction)pyalsamixer_free,	METH_NOARGS,	free__doc__},
	{"list",	(PyCFunction)pyalsamixer_list,	METH_NOARGS,	list__doc__},
	{"handle_events",(PyCFunction)pyalsamixer_handleevents,	METH_NOARGS,	handlevents__doc__},
	{"register_poll",(PyCFunction)pyalsamixer_registerpoll,	METH_VARARGS,	registerpoll__doc__},
	{NULL}
};

static PyTypeObject pyalsamixer_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	tp_name:	"alsamixer.Mixer",
	tp_basicsize:	sizeof(struct pyalsamixer),
	tp_dealloc:	(destructor)pyalsamixer_dealloc,
	tp_flags:	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	tp_doc:		alsamixerinit__doc__,
	tp_getset:	pyalsamixer_getseters,
	tp_init:	(initproc)pyalsamixer_init,
	tp_alloc:	PyType_GenericAlloc,
	tp_new:		PyType_GenericNew,
	tp_free:	PyObject_Del,
	tp_methods:	pyalsamixer_methods,
};

/*
 * mixer element section
 */

#define PYALSAMIXERELEMENT(v) (((v) == Py_None) ? NULL : \
	((struct pyalsamixerelement *)(v)))

struct pyalsamixerelement {
	PyObject_HEAD
	PyObject *pyhandle;
	PyObject *callback;
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
};

static PyObject *
pyalsamixerelement_getname(struct pyalsamixerelement *pyelem, void *priv)
{
	return PyUnicode_FromString(snd_mixer_selem_get_name(pyelem->elem));
}

static PyObject *
pyalsamixerelement_getindex(struct pyalsamixerelement *pyelem, void *priv)
{
	return PyInt_FromLong(snd_mixer_selem_get_index(pyelem->elem));
}

typedef unsigned int (*fcn1)(void *);

static PyObject *
pyalsamixerelement_bool(struct pyalsamixerelement *pyelem, void *fcn)
{
	int res = ((fcn1)fcn)(pyelem->elem);
	if (res > 0)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static PyObject *
pyalsamixerelement_getcapgroup(struct pyalsamixerelement *pyelem, void *priv)
{
	return PyInt_FromLong(snd_mixer_selem_get_capture_group(pyelem->elem));
}

PyDoc_STRVAR(ismono__doc__,
"is_mono([capture=False]]) -- Return if this control is mono.");

static PyObject *
pyalsamixerelement_ismono(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0;

	if (!PyArg_ParseTuple(args, "|i", &dir))
		return NULL;

	if (dir == 0)
		res = snd_mixer_selem_is_playback_mono(pyelem->elem);
	else
		res = snd_mixer_selem_is_capture_mono(pyelem->elem);
	if (res > 0)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

PyDoc_STRVAR(haschannel__doc__,
"has_channel([channel=channel_id['MONO'], [capture=False]]) -- Return if channel exists.");

static PyObject *
pyalsamixerelement_haschannel(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, chn = SND_MIXER_SCHN_MONO;

	if (!PyArg_ParseTuple(args, "|ii", &chn, &dir))
		return NULL;

	if (dir == 0)
		res = snd_mixer_selem_has_playback_channel(pyelem->elem, chn);
	else
		res = snd_mixer_selem_has_capture_channel(pyelem->elem, chn);
	if (res > 0)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

PyDoc_STRVAR(hasvolume__doc__,
"has_volume([capture=False]]) -- Return if volume exists.\n"
"Note: String 'Joined' is returned when volume is joined.");

static PyObject *
pyalsamixerelement_hasvolume(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0;

	if (!PyArg_ParseTuple(args, "|i", &dir))
		return NULL;

	if (dir == 0)
		res = snd_mixer_selem_has_playback_volume(pyelem->elem);
	else
		res = snd_mixer_selem_has_capture_volume(pyelem->elem);
	if (res > 0) {
		if (dir == 0)
			res = snd_mixer_selem_has_playback_volume_joined(pyelem->elem);
		else
			res = snd_mixer_selem_has_capture_volume_joined(pyelem->elem);
		if (res > 0)
			return Py_BuildValue("s", "Joined");
		Py_RETURN_TRUE;
	} else {
		Py_RETURN_FALSE;
	}
}

PyDoc_STRVAR(hasswitch__doc__,
"has_switch([capture=False]) -- Return if switch exists."
"Note: String 'Joined' is returned when switch is joined.");

static PyObject *
pyalsamixerelement_hasswitch(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0;

	if (!PyArg_ParseTuple(args, "|i", &dir))
		return NULL;

	if (dir == 0)
		res = snd_mixer_selem_has_playback_switch(pyelem->elem);
	else
		res = snd_mixer_selem_has_capture_switch(pyelem->elem);
	if (res > 0) {
		if (dir == 0)
			res = snd_mixer_selem_has_playback_switch_joined(pyelem->elem);
		else
			res = snd_mixer_selem_has_capture_switch_joined(pyelem->elem);
		if (res > 0)
			return Py_BuildValue("s", "Joined");
		Py_RETURN_TRUE;
	} else {
		Py_RETURN_FALSE;
	}
}

PyDoc_STRVAR(askvoldB__doc__,
"ask_volume_dB(value, [capture=False]]) -- Convert integer volume to dB value.");

static PyObject *
pyalsamixerelement_askvoldB(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0;
	long volume, val;

	if (!PyArg_ParseTuple(args, "|li", &volume, &dir))
		return NULL;

	if (dir == 0)
		res = snd_mixer_selem_ask_playback_vol_dB(pyelem->elem, volume, &val);
	else
		res = snd_mixer_selem_ask_capture_vol_dB(pyelem->elem, volume, &val);
	if (res < 0) {
		 PyErr_Format(PyExc_RuntimeError, "Cannot convert mixer volume (capture=%s, value=%li): %s", dir ? "True" : "False", volume, snd_strerror(-res));
		 Py_RETURN_NONE;
	}
	return PyInt_FromLong(val);
}

PyDoc_STRVAR(askdBvol__doc__,
"ask_dB_volume(dBvalue, [[direction=-1], [capture=False]]) -- Convert integer volume to dB value.");

static PyObject *
pyalsamixerelement_askdBvol(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, xdir = -1, dir = 0;
	long dBvolume, val;

	if (!PyArg_ParseTuple(args, "|lii", &dBvolume, &xdir, &dir))
		return NULL;

	if (dir == 0)
		res = snd_mixer_selem_ask_playback_dB_vol(pyelem->elem, dBvolume, xdir, &val);
	else
		res = snd_mixer_selem_ask_capture_dB_vol(pyelem->elem, dBvolume, xdir, &val);
	if (res < 0) {
		 PyErr_Format(PyExc_RuntimeError, "Cannot convert mixer volume (capture=%s, dBvalue=%li, direction=%i): %s", dir ? "True" : "False", dBvolume, xdir, snd_strerror(-res));
		 Py_RETURN_NONE;
	}
	return PyInt_FromLong(val);
}

PyDoc_STRVAR(getvolume__doc__,
"get_volume([channel=ChannelId['MONO'], [capture=False]]) -- Get volume.");

static PyObject *
pyalsamixerelement_getvolume(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, chn = SND_MIXER_SCHN_MONO;
	long val;

	if (!PyArg_ParseTuple(args, "|ii", &chn, &dir))
		return NULL;

	if (dir == 0)
		res = snd_mixer_selem_get_playback_volume(pyelem->elem, chn, &val);
	else
		res = snd_mixer_selem_get_capture_volume(pyelem->elem, chn, &val);
	if (res < 0) {
		 PyErr_Format(PyExc_RuntimeError, "Cannot get mixer volume (capture=%s, channel=%i): %s", dir ? "True" : "False", chn, snd_strerror(-res));
		 Py_RETURN_NONE;
	}
	return PyInt_FromLong(val);
}

PyDoc_STRVAR(getvolumetuple__doc__,
"get_volume_tuple([capture=False]]) -- Get volume and store result to tuple.");

static PyObject *
pyalsamixerelement_getvolumetuple(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, i, last;
	long val;
	PyObject *t;

	if (!PyArg_ParseTuple(args, "|i", &dir))
		return NULL;

	if (dir == 0) {
		if (snd_mixer_selem_is_playback_mono(pyelem->elem)) {
			t = PyTuple_New(1);
			if (!t)
				return NULL;
 			res = snd_mixer_selem_get_playback_volume(pyelem->elem, SND_MIXER_SCHN_MONO, &val);
 			if (res >= 0)
 				PyTuple_SET_ITEM(t, 0, PyInt_FromLong(val));
		} else {
			t = PyTuple_New(SND_MIXER_SCHN_LAST+1);
			if (!t)
				return NULL;
			for (i = last = 0; i <= SND_MIXER_SCHN_LAST; i++) {
				res = -1;
				if (snd_mixer_selem_has_playback_channel(pyelem->elem, i)) {
					res = snd_mixer_selem_get_playback_volume(pyelem->elem, i, &val);
					if (res >= 0) {
						while (last < i) {
							Py_INCREF(Py_None);
							PyTuple_SET_ITEM(t, last, Py_None);
							last++;
						}
						PyTuple_SET_ITEM(t, i, PyInt_FromLong(val));
						last++;
					}
				}
			}
			_PyTuple_Resize(&t, last);
		}
	} else {
		if (snd_mixer_selem_is_capture_mono(pyelem->elem)) {
			t = PyTuple_New(1);
			if (!t)
				return NULL;
 			res = snd_mixer_selem_get_capture_volume(pyelem->elem, SND_MIXER_SCHN_MONO, &val);
 			if (res >= 0)
 				PyTuple_SET_ITEM(t, 0, PyInt_FromLong(val));
		} else {
			t = PyTuple_New(SND_MIXER_SCHN_LAST+1);
			if (!t)
				return NULL;
			for (i = last = 0; i <= SND_MIXER_SCHN_LAST; i++) {
				res = -1;
				if (snd_mixer_selem_has_capture_channel(pyelem->elem, i)) {
					res = snd_mixer_selem_get_capture_volume(pyelem->elem, i, &val);
					if (res >= 0) {
						while (last < i) {
							Py_INCREF(Py_None);
							PyTuple_SET_ITEM(t, last, Py_None);
							last++;
						}
						PyTuple_SET_ITEM(t, i, PyInt_FromLong(val));
					}
				}
			}
			_PyTuple_Resize(&t, last);
		}
	}
	return t;
}

PyDoc_STRVAR(getvolumearray__doc__,
"get_volume_array([capture=False]]) -- Get volume and store result to array.");

static PyObject *
pyalsamixerelement_getvolumearray(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, i, last;
	long val;
	PyObject *t, *l;

	if (!PyArg_ParseTuple(args, "|i", &dir))
		return NULL;

	if (dir == 0) {
		if (snd_mixer_selem_is_playback_mono(pyelem->elem)) {
			t = PyList_New(1);
			if (!t)
				return NULL;
 			res = snd_mixer_selem_get_playback_volume(pyelem->elem, SND_MIXER_SCHN_MONO, &val);
 			if (res >= 0)
				PyList_SetItem(t, 0, PyInt_FromLong(val));
		} else {
			t = PyList_New(SND_MIXER_SCHN_LAST+1);
			if (!t)
				return NULL;
			for (i = last = 0; i <= SND_MIXER_SCHN_LAST; i++) {
				res = -1;
				if (snd_mixer_selem_has_playback_channel(pyelem->elem, i)) {
					res = snd_mixer_selem_get_playback_volume(pyelem->elem, i, &val);
					if (res >= 0) {
						while (last < i) {
							Py_INCREF(Py_None);
							PyList_SetItem(t, last, Py_None);
							last++;
						}
						PyList_SetItem(t, i, PyInt_FromLong(val));
						last++;
					}
				}
			}
			l = PyList_GetSlice(t, 0, last);
			Py_DECREF(t);
			t = l;
		}
	} else {
		if (snd_mixer_selem_is_capture_mono(pyelem->elem)) {
			t = PyList_New(1);
			if (!t)
				return NULL;
 			res = snd_mixer_selem_get_capture_volume(pyelem->elem, SND_MIXER_SCHN_MONO, &val);
 			if (res >= 0)
				PyList_SET_ITEM(t, 0, PyInt_FromLong(val));
		} else {
			t = PyList_New(SND_MIXER_SCHN_LAST+1);
			if (!t)
				return NULL;
			for (i = last = 0; i <= SND_MIXER_SCHN_LAST; i++) {
				res = -1;
				if (snd_mixer_selem_has_capture_channel(pyelem->elem, i)) {
					res = snd_mixer_selem_get_capture_volume(pyelem->elem, i, &val);
					if (res >= 0) {
						while (last < i) {
							Py_INCREF(Py_None);
							PyList_SetItem(t, last, Py_None);
							last++;
						}
						PyList_SetItem(t, i, PyInt_FromLong(val));
					}
				}
			}
			l = PyList_GetSlice(t, 0, last);
			Py_DECREF(t);
			t = l;
		}
	}
	return t;
}

PyDoc_STRVAR(setvolume__doc__,
"set_volume(value, [channel=ChannelId['MONO'], [capture=False]]) -- Set volume.");

static PyObject *
pyalsamixerelement_setvolume(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, chn = SND_MIXER_SCHN_MONO;
	long val;

	if (!PyArg_ParseTuple(args, "l|ii", &val, &chn, &dir))
		return NULL;
	if (dir == 0)
		res = snd_mixer_selem_set_playback_volume(pyelem->elem, chn, val);
	else
		res = snd_mixer_selem_set_capture_volume(pyelem->elem, chn, val);
	if (res < 0)
		 PyErr_Format(PyExc_RuntimeError, "Cannot set mixer volume (capture=%s, channel=%i, value=%li): %s", dir ? "True" : "False", chn, val, snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(setvolumetuple__doc__,
"set_volume_tuple(value, [capture=False]]) -- Set volume level from tuple.");

PyDoc_STRVAR(setvolumearray__doc__,
"set_volume_array(value, [capture=False]]) -- Set volume level from array.");

static PyObject *
pyalsamixerelement_setvolumetuple(struct pyalsamixerelement *pyelem, PyObject *args)
{
	PyObject *t, *o;
	int i, res, dir = 0;
	long val;

	if (!PyArg_ParseTuple(args, "O|i", &t, &dir))
		return NULL;
	if (!PyTuple_Check(t) && !PyList_Check(t))
		return PyErr_Format(PyExc_RuntimeError, "Volume values in tuple are expected!");
	if (PyTuple_Check(t)) {
		for (i = 0; i < PyTuple_Size(t); i++) {
			o = PyTuple_GetItem(t, i);
			if (o == Py_None)
				continue;
			if (get_long(o, &val))
				break;
			if (dir == 0)
				res = snd_mixer_selem_set_playback_volume(pyelem->elem, i, val);
			else
				res = snd_mixer_selem_set_capture_volume(pyelem->elem, i, val);
			if (res < 0)
				 PyErr_Format(PyExc_RuntimeError, "Cannot set mixer volume (capture=%s, channel=%i, value=%li): %s", dir ? "True" : "False", i, val, snd_strerror(-res));
		}
	} else {
		for (i = 0; i < PyList_Size(t); i++) {
			o = PyList_GetItem(t, i);
			if (o == Py_None)
				continue;
			if (get_long(o, &val))
				break;
			if (dir == 0)
				res = snd_mixer_selem_set_playback_volume(pyelem->elem, i, val);
			else
				res = snd_mixer_selem_set_capture_volume(pyelem->elem, i, val);
			if (res < 0)
				 PyErr_Format(PyExc_RuntimeError, "Cannot set mixer volume (capture=%s, channel=%i, value=%li): %s", dir ? "True" : "False", i, val, snd_strerror(-res));
		}
	}
	Py_DECREF(t);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(setvolumeall__doc__,
"set_volume_all(value, [capture=False]]) -- Set volume for all channels.");

static PyObject *
pyalsamixerelement_setvolumeall(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0;
	long val;

	if (!PyArg_ParseTuple(args, "l|i", &val, &dir))
		return NULL;
	if (dir == 0)
		res = snd_mixer_selem_set_playback_volume_all(pyelem->elem, val);
	else
		res = snd_mixer_selem_set_capture_volume_all(pyelem->elem, val);
	if (res < 0)
		 PyErr_Format(PyExc_RuntimeError, "Cannot set mixer volume (capture=%s, value=%li): %s", dir ? "True" : "False", val, snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(getrange__doc__,
"get_volume_range([capture=False]]) -- Get volume range in (min,max) tuple.");

static PyObject *
pyalsamixerelement_getrange(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int dir = 0, res;
	long min, max;
	PyObject *t;

	if (!PyArg_ParseTuple(args, "|i", &dir))
		return NULL;
	if (dir == 0)
		res = snd_mixer_selem_get_playback_volume_range(pyelem->elem, &min, &max);
	else
		res = snd_mixer_selem_get_capture_volume_range(pyelem->elem, &min, &max);
	if (res < 0)
		 return PyErr_Format(PyExc_RuntimeError, "Cannot get mixer volume range (capture=%s): %s", dir ? "True" : "False", snd_strerror(-res));
       	t = PyTuple_New(2);
	if (!t)
		Py_RETURN_NONE;
	PyTuple_SET_ITEM(t, 0, PyInt_FromLong(min));
	PyTuple_SET_ITEM(t, 1, PyInt_FromLong(max));
	return t;
}

PyDoc_STRVAR(setrange__doc__,
"set_volume_range(min, max, [capture=False]]) -- Set volume range limits.");

static PyObject *
pyalsamixerelement_setrange(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int dir = 0, res;
	long min, max;

	if (!PyArg_ParseTuple(args, "ll|i", &min, &max, &dir))
		return NULL;
	if (dir == 0)
		res = snd_mixer_selem_set_playback_volume_range(pyelem->elem, min, max);
	else
		res = snd_mixer_selem_set_capture_volume_range(pyelem->elem, min, max);
	if (res < 0)
		 return PyErr_Format(PyExc_RuntimeError, "Cannot set mixer volume range (min=%li,max=%li,capture=%s): %s", min, max, dir ? "True" : "False", snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(getswitch__doc__,
"get_switch([channel=ChannelId['MONO'], [capture=False]]) -- Get switch state.");

static PyObject *
pyalsamixerelement_getswitch(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, chn = SND_MIXER_SCHN_MONO, val;

	if (!PyArg_ParseTuple(args, "|ii", &chn, &dir))
		return NULL;

	if (dir == 0)
		res = snd_mixer_selem_get_playback_switch(pyelem->elem, chn, &val);
	else
		res = snd_mixer_selem_get_capture_switch(pyelem->elem, chn, &val);
	if (res < 0) {
		 PyErr_Format(PyExc_RuntimeError, "Cannot get mixer volume (capture=%s, channel=%i): %s", dir ? "True" : "False", chn, snd_strerror(-res));
		 Py_RETURN_NONE;
	}
	if (val) {
		Py_RETURN_TRUE;
	} else {
		Py_RETURN_FALSE;
	}
}

PyDoc_STRVAR(getswitchtuple__doc__,
"get_switch_tuple([capture=False]]) -- Get switch state and store result to tuple.");

static PyObject *
pyalsamixerelement_getswitchtuple(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, i, last, val;
	PyObject *t;

	if (!PyArg_ParseTuple(args, "|i", &dir))
		return NULL;

	if (dir == 0) {
		if (snd_mixer_selem_is_playback_mono(pyelem->elem)) {
			t = PyTuple_New(1);
			if (!t)
				return NULL;
 			res = snd_mixer_selem_get_playback_switch(pyelem->elem, SND_MIXER_SCHN_MONO, &val);
 			if (res >= 0)
 				PyTuple_SET_ITEM(t, 0, get_bool(val));
		} else {
			t = PyTuple_New(SND_MIXER_SCHN_LAST+1);
			if (!t)
				return NULL;
			for (i = last = 0; i <= SND_MIXER_SCHN_LAST; i++) {
				res = -1;
				if (snd_mixer_selem_has_playback_channel(pyelem->elem, i)) {
					res = snd_mixer_selem_get_playback_switch(pyelem->elem, i, &val);
					if (res >= 0) {
						while (last < i) {
							Py_INCREF(Py_None);
							PyTuple_SET_ITEM(t, last, Py_None);
							last++;
						}
						PyTuple_SET_ITEM(t, i, get_bool(val));
						last++;
					}
				}
			}
			_PyTuple_Resize(&t, last);
		}
	} else {
		if (snd_mixer_selem_is_capture_mono(pyelem->elem)) {
			t = PyTuple_New(1);
			if (!t)
				return NULL;
 			res = snd_mixer_selem_get_capture_switch(pyelem->elem, SND_MIXER_SCHN_MONO, &val);
 			if (res >= 0)
 				PyTuple_SET_ITEM(t, 0, get_bool(val));
		} else {
			t = PyTuple_New(SND_MIXER_SCHN_LAST+1);
			if (!t)
				return NULL;
			for (i = last = 0; i <= SND_MIXER_SCHN_LAST; i++) {
				res = -1;
				if (snd_mixer_selem_has_capture_channel(pyelem->elem, i)) {
					res = snd_mixer_selem_get_capture_switch(pyelem->elem, i, &val);
					if (res >= 0) {
						while (last < i) {
							Py_INCREF(Py_None);
							PyTuple_SET_ITEM(t, last, Py_None);
							last++;
						}
						PyTuple_SET_ITEM(t, i, get_bool(val));
					}
				}
			}
			_PyTuple_Resize(&t, last);
		}
	}
	return t;
}

PyDoc_STRVAR(setswitch__doc__,
"set_switch(value, [channel=ChannelId['MONO'], [capture=False]]) -- Set switch state.");

static PyObject *
pyalsamixerelement_setswitch(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, chn = SND_MIXER_SCHN_MONO, val;

	if (!PyArg_ParseTuple(args, "i|ii", &val, &chn, &dir))
		return NULL;
	if (dir == 0)
		res = snd_mixer_selem_set_playback_switch(pyelem->elem, chn, val);
	else
		res = snd_mixer_selem_set_capture_switch(pyelem->elem, chn, val);
	if (res < 0)
		 PyErr_Format(PyExc_RuntimeError, "Cannot set mixer switch (capture=%s, channel=%i, value=%i): %s", dir ? "True" : "False", chn, val, snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(setswitchtuple__doc__,
"set_switch_tuple(value, [capture=False]]) -- Set switch state from tuple.");

static PyObject *
pyalsamixerelement_setswitchtuple(struct pyalsamixerelement *pyelem, PyObject *args)
{
	PyObject *t, *o;
	int i, res, dir = 0, val;

	if (!PyArg_ParseTuple(args, "O|i", &t, &dir))
		return NULL;
	if (!PyTuple_Check(t))
		return PyErr_Format(PyExc_RuntimeError, "Switch state values in tuple are expected!");
	for (i = 0; i < PyTuple_Size(t); i++) {
		o = PyTuple_GetItem(t, i);
		if (o == Py_None)
			continue;
		val = PyObject_IsTrue(o);
		if (dir == 0)
			res = snd_mixer_selem_set_playback_switch(pyelem->elem, i, val);
		else
			res = snd_mixer_selem_set_capture_switch(pyelem->elem, i, val);
		if (res < 0)
			 PyErr_Format(PyExc_RuntimeError, "Cannot set mixer switch (capture=%s, channel=%i, value=%i): %s", dir ? "True" : "False", i, val, snd_strerror(-res));
	}
	Py_DECREF(t);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(setswitchall__doc__,
"set_switch_all(value, [capture=False]]) -- Set switch for all channels.");

static PyObject *
pyalsamixerelement_setswitchall(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, val;

	if (!PyArg_ParseTuple(args, "i|i", &val, &dir))
		return NULL;
	if (dir == 0)
		res = snd_mixer_selem_set_playback_switch_all(pyelem->elem, val);
	else
		res = snd_mixer_selem_set_capture_switch_all(pyelem->elem, val);
	if (res < 0)
		 PyErr_Format(PyExc_RuntimeError, "Cannot set mixer switch state (capture=%s, value=%i): %s", dir ? "True" : "False", val, snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(getvolumedb__doc__,
"get_volume_dB([channel=ChannelId['MONO'], [capture=False]]) -- Get volume in dB.");

static PyObject *
pyalsamixerelement_getvolumedb(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, chn = SND_MIXER_SCHN_MONO;
	long val;

	if (!PyArg_ParseTuple(args, "|ii", &chn, &dir))
		return NULL;

	if (dir == 0)
		res = snd_mixer_selem_get_playback_dB(pyelem->elem, chn, &val);
	else
		res = snd_mixer_selem_get_capture_dB(pyelem->elem, chn, &val);
	if (res < 0) {
		 PyErr_Format(PyExc_RuntimeError, "Cannot get mixer volume in dB (capture=%s, channel=%i): %s", dir ? "True" : "False", chn, snd_strerror(-res));
		 Py_RETURN_NONE;
	}
	return Py_BuildValue("i", val);
}

PyDoc_STRVAR(setvolumedb__doc__,
"set_volume_dB(value, [channel=ChannelId['MONO'], [capture=False], [dir=0]]) -- Set volume in dB.");

static PyObject *
pyalsamixerelement_setvolumedb(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, dir1 = 0, chn = SND_MIXER_SCHN_MONO;
	long val;

	if (!PyArg_ParseTuple(args, "l|iii", &val, &chn, &dir, &dir1))
		return NULL;
	if (dir == 0)
		res = snd_mixer_selem_set_playback_dB(pyelem->elem, chn, val, dir1);
	else
		res = snd_mixer_selem_set_capture_dB(pyelem->elem, chn, val, dir1);
	if (res < 0)
		 PyErr_Format(PyExc_RuntimeError, "Cannot set mixer volume in dB (capture=%s, channel=%i, value=%li): %s", dir ? "True" : "False", chn, val, snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(setvolumealldb__doc__,
"set_volume_all_dB(value, [[capture=False], [dir=0]]) -- Set volume for all channels in dB.");

static PyObject *
pyalsamixerelement_setvolumealldb(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int res, dir = 0, dir1 = 0;
	long val;

	if (!PyArg_ParseTuple(args, "l|ii", &val, &dir, &dir1))
		return NULL;
	if (dir == 0)
		res = snd_mixer_selem_set_playback_dB_all(pyelem->elem, val, dir1);
	else
		res = snd_mixer_selem_set_capture_dB_all(pyelem->elem, val, dir1);
	if (res < 0)
		 PyErr_Format(PyExc_RuntimeError, "Cannot set mixer volume in dB (capture=%s, value=%li): %s", dir ? "True" : "False", val, snd_strerror(-res));
	Py_RETURN_NONE;
}

PyDoc_STRVAR(getrangedb__doc__,
"get_volume_range_dB([capture=False]]) -- Get volume range in dB in (min,max) tuple.");

static PyObject *
pyalsamixerelement_getrangedb(struct pyalsamixerelement *pyelem, PyObject *args)
{
	int dir = 0, res;
	long min, max;
	PyObject *t;

	if (!PyArg_ParseTuple(args, "|i", &dir))
		return NULL;
	if (dir == 0)
		res = snd_mixer_selem_get_playback_dB_range(pyelem->elem, &min, &max);
	else
		res = snd_mixer_selem_get_capture_dB_range(pyelem->elem, &min, &max);
	if (res < 0)
		 return PyErr_Format(PyExc_RuntimeError, "Cannot get mixer volume range in dB (capture=%s): %s", dir ? "True" : "False", snd_strerror(-res));
       	t = PyTuple_New(2);
	if (!t)
		Py_RETURN_NONE;
	PyTuple_SET_ITEM(t, 0, PyInt_FromLong(min));
	PyTuple_SET_ITEM(t, 1, PyInt_FromLong(max));
	return t;
}

PyDoc_STRVAR(setcallback__doc__,
"set_callback(callObj) -- Set callback object.\n"
"Note: callObj might have callObj.callback attribute.\n");

static PyObject *
pyalsamixerelement_setcallback(struct pyalsamixerelement *pyelem, PyObject *args)
{
	PyObject *o;

	if (!PyArg_ParseTuple(args, "O", &o))
		return NULL;
	if (o == Py_None) {
		Py_XDECREF(pyelem->callback);
		pyelem->callback = NULL;
		snd_mixer_elem_set_callback(pyelem->elem, NULL);
	} else {
		Py_INCREF(o);
		pyelem->callback = o;
		snd_mixer_elem_set_callback_private(pyelem->elem, pyelem);
		snd_mixer_elem_set_callback(pyelem->elem, element_callback);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(elementinit__doc__,
"Element(mixer, name[, index=0])\n"
"  -- Create a mixer element object.\n"
"\n"
"You may specify simple name and index like:\n"
"element(mixer, \"PCM\", 0)\n");

static int
pyalsamixerelement_init(struct pyalsamixerelement *pyelem, PyObject *args, PyObject *kwds)
{
	PyObject *mixer;
	char *name;
	int index = 0;
	snd_mixer_selem_id_t *id;
	static char * kwlist[] = { "mixer", "name", "index", NULL };

	snd_mixer_selem_id_alloca(&id);
	pyelem->pyhandle = NULL;
	pyelem->handle = NULL;
	pyelem->elem = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Os|i",
					 kwlist, &mixer, &name, &index))
		return -1;

	if (mixer->ob_type != &pyalsamixer_type) {
		PyErr_SetString(PyExc_TypeError, "bad type for mixer argument");
		return -1;
	}

	pyelem->pyhandle = mixer;
	Py_INCREF(mixer);
	pyelem->handle = PYALSAMIXER(mixer)->handle;

	snd_mixer_selem_id_set_name(id, name);
	snd_mixer_selem_id_set_index(id, index);

	pyelem->elem = snd_mixer_find_selem(pyelem->handle, id);
	if (pyelem->elem == NULL) {
		PyErr_Format(PyExc_IOError, "cannot find mixer element '%s',%i", name, index);
		return -1;
	}
	
	return 0;
}

static void
pyalsamixerelement_dealloc(struct pyalsamixerelement *self)
{
	if (self->elem) {
		Py_XDECREF(self->callback);
		snd_mixer_elem_set_callback(self->elem, NULL);
	}
	Py_XDECREF(self->pyhandle);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyGetSetDef pyalsamixerelement_getseters[] = {

	{"name",	(getter)pyalsamixerelement_getname,	NULL,	"mixer element name",		NULL},
	{"index",	(getter)pyalsamixerelement_getindex,	NULL,	"mixer element index",		NULL},

	{"is_active",	(getter)pyalsamixerelement_bool,	NULL,	"element is in active status",	snd_mixer_selem_is_active},
	{"is_enumerated",(getter)pyalsamixerelement_bool,	NULL,	"element is enumerated type",	snd_mixer_selem_is_enumerated},
	{"has_common_volume",(getter)pyalsamixerelement_bool,	NULL,	"element has common volume control",	snd_mixer_selem_has_common_volume},
	{"has_common_switch",(getter)pyalsamixerelement_bool,	NULL,	"element has common switch control",	snd_mixer_selem_has_common_switch},
	{"has_capture_switch_exclusive", (getter)pyalsamixerelement_bool, NULL, "element has exclusive capture switch",	snd_mixer_selem_has_capture_switch_exclusive},

	{"get_capture_group",(getter)pyalsamixerelement_getcapgroup,	NULL,	"element get capture group",	NULL},
	
	{NULL}
};

static PyMethodDef pyalsamixerelement_methods[] = {

	{"ask_volume_dB",(PyCFunction)pyalsamixerelement_askvoldB,	METH_VARARGS,	askvoldB__doc__},
	{"ask_dB_volume",(PyCFunction)pyalsamixerelement_askdBvol,	METH_VARARGS,	askdBvol__doc__},
	{"get_volume",	(PyCFunction)pyalsamixerelement_getvolume,	METH_VARARGS,	getvolume__doc__},
	{"get_volume",	(PyCFunction)pyalsamixerelement_getvolume,	METH_VARARGS,	getvolume__doc__},
	{"get_volume_tuple", (PyCFunction)pyalsamixerelement_getvolumetuple, METH_VARARGS,getvolumetuple__doc__},
	{"get_volume_array", (PyCFunction)pyalsamixerelement_getvolumearray, METH_VARARGS,getvolumearray__doc__},
	{"set_volume",	(PyCFunction)pyalsamixerelement_setvolume,	METH_VARARGS,	setvolume__doc__},
	{"set_volume_tuple", (PyCFunction)pyalsamixerelement_setvolumetuple, METH_VARARGS,setvolumetuple__doc__},
	{"set_volume_array", (PyCFunction)pyalsamixerelement_setvolumetuple, METH_VARARGS,setvolumearray__doc__},
	{"set_volume_all",(PyCFunction)pyalsamixerelement_setvolumeall,	METH_VARARGS,	setvolumeall__doc__},
	{"get_volume_range", (PyCFunction)pyalsamixerelement_getrange,	METH_VARARGS,	getrange__doc__},
	{"set_volume_range", (PyCFunction)pyalsamixerelement_setrange,	METH_VARARGS,	setrange__doc__},

	{"get_volume_dB",(PyCFunction)pyalsamixerelement_getvolumedb,	METH_VARARGS,	getvolumedb__doc__},
	{"set_volume_dB",(PyCFunction)pyalsamixerelement_setvolumedb,	METH_VARARGS,	setvolumedb__doc__},
	{"set_volume_all_dB",(PyCFunction)pyalsamixerelement_setvolumealldb,METH_VARARGS,	setvolumealldb__doc__},
	{"get_volume_range_dB", (PyCFunction)pyalsamixerelement_getrangedb,	METH_VARARGS,	getrangedb__doc__},

	{"get_switch",	(PyCFunction)pyalsamixerelement_getswitch,	METH_VARARGS,	getswitch__doc__},
	{"get_switch_tuple", (PyCFunction)pyalsamixerelement_getswitchtuple, METH_VARARGS,getswitchtuple__doc__},
	{"set_switch",	(PyCFunction)pyalsamixerelement_setswitch,	METH_VARARGS,	setswitch__doc__},
	{"set_switch_tuple", (PyCFunction)pyalsamixerelement_setswitchtuple, METH_VARARGS,setswitchtuple__doc__},
	{"set_switch_all",(PyCFunction)pyalsamixerelement_setswitchall,	METH_VARARGS,	setswitchall__doc__},

	{"is_mono",	(PyCFunction)pyalsamixerelement_ismono,		METH_VARARGS,	ismono__doc__},
	{"has_channel",	(PyCFunction)pyalsamixerelement_haschannel,	METH_VARARGS,	haschannel__doc__},
	{"has_volume",	(PyCFunction)pyalsamixerelement_hasvolume,	METH_VARARGS,	hasvolume__doc__},
	{"has_switch",	(PyCFunction)pyalsamixerelement_hasswitch,	METH_VARARGS,	hasswitch__doc__},

	{"set_callback",(PyCFunction)pyalsamixerelement_setcallback,	METH_VARARGS,	setcallback__doc__},

	{NULL}
};

static PyTypeObject pyalsamixerelement_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	tp_name:	"alsamixer.Element",
	tp_basicsize:	sizeof(struct pyalsamixerelement),
	tp_dealloc:	(destructor)pyalsamixerelement_dealloc,
	tp_flags:	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	tp_doc:		elementinit__doc__,
	tp_getset:	pyalsamixerelement_getseters,
	tp_init:	(initproc)pyalsamixerelement_init,
	tp_alloc:	PyType_GenericAlloc,
	tp_new:		PyType_GenericNew,
	tp_free:	PyObject_Del,
	tp_methods:	pyalsamixerelement_methods,
};

/*
 *
 */

static PyMethodDef pyalsamixerparse_methods[] = {
	{NULL}
};

MOD_INIT(alsamixer)
{
	PyObject *d, *d1, *l1, *o;
	int i;

	if (PyType_Ready(&pyalsamixer_type) < 0)
		return MOD_ERROR_VAL;
	if (PyType_Ready(&pyalsamixerelement_type) < 0)
		return MOD_ERROR_VAL;

	MOD_DEF(module, "alsamixer", "libasound mixer wrapper", pyalsamixerparse_methods);
	if (module == NULL)
		return MOD_ERROR_VAL;

#if 0
	buildin = PyImport_AddModule("__buildin__");
	if (buildin == NULL)
		return MOD_ERROR_VAL;
	if (PyObject_SetAttrString(module, "__buildins__", buildin) < 0)
		return MOD_ERROR_VAL;
#endif

	Py_INCREF(&pyalsamixer_type);
	PyModule_AddObject(module, "Mixer", (PyObject *)&pyalsamixer_type);

	Py_INCREF(&pyalsamixerelement_type);
	PyModule_AddObject(module, "Element", (PyObject *)&pyalsamixerelement_type);

	d = PyModule_GetDict(module);

	/* ---- */

	d1 = PyDict_New();

#define add_space1(pname, name) { \
	o = PyInt_FromLong(SND_MIXER_SCHN_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space1("UNKNOWN", UNKNOWN);
	add_space1("FRONT_LEFT", FRONT_LEFT);
	add_space1("FRONT_RIGHT", FRONT_RIGHT);
	add_space1("REAR_LEFT", REAR_LEFT);
	add_space1("REAR_RIGHT", REAR_RIGHT);
	add_space1("FRONT_CENTER", FRONT_CENTER);
	add_space1("WOOFER", WOOFER);
	add_space1("SIDE_LEFT", SIDE_LEFT);
	add_space1("SIDE_RIGHT", SIDE_RIGHT);
	add_space1("REAR_CENTER", REAR_CENTER);
	add_space1("LAST", LAST);
	add_space1("MONO", MONO);

	PyDict_SetItemString(d, "channel_id", d1);
	Py_DECREF(d1);

	/* ---- */

	l1 = PyList_New(0);

	for (i = 0; i <= SND_MIXER_SCHN_LAST; i++) {
		o = PyUnicode_FromString(snd_mixer_selem_channel_name(i));
		PyList_Append(l1, o);
		Py_DECREF(o);
	}

	PyDict_SetItemString(d, "channel_name", l1);
	Py_DECREF(l1);

	/* ---- */

	d1 = PyDict_New();
	
#define add_space2(pname, name) { \
	o = PyInt_FromLong(SND_MIXER_SABSTRACT_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space2("NONE", NONE);
	add_space2("BASIC", BASIC);

	PyDict_SetItemString(d, "regopt_abstract", d1);
	Py_DECREF(d1);
	
	/* ---- */

	d1 = PyDict_New();
	
#define add_space3(pname, name) { \
	o = PyInt_FromLong(SND_CTL_EVENT_MASK_##name); \
	PyDict_SetItemString(d1, pname, o); \
	Py_DECREF(o); }
	
	add_space3("VALUE", VALUE);
	add_space3("INFO", INFO);
	add_space3("ADD", ADD);
	add_space3("TLV", TLV);

	PyDict_SetItemString(d, "event_mask", d1);
	Py_DECREF(d1);

	o = PyInt_FromLong(SND_CTL_EVENT_MASK_REMOVE);
	PyDict_SetItemString(d, "event_mask_remove", o);
	Py_DECREF(o);
	
	/* ---- */

	main_interpreter = PyThreadState_Get()->interp;

	if (PyErr_Occurred())
		Py_FatalError("Cannot initialize module alsamixer");

	return MOD_SUCCESS_VAL(module);
}

/*
 *  element event callback
 */

static int element_callback(snd_mixer_elem_t *elem, unsigned int mask)
{
	struct pyalsamixerelement *pyelem;
	PyObject *o, *t, *r;
	int res = 0, inside = 1;
	CALLBACK_VARIABLES;

	if (elem == NULL)
		return -EINVAL;
	pyelem = snd_mixer_elem_get_callback_private(elem);
	if (pyelem == NULL || pyelem->callback == NULL)
		return -EINVAL;

	CALLBACK_INIT;

	o = PyObject_GetAttr(pyelem->callback, InternFromString("callback"));
	if (!o) {
		PyErr_Clear();
		o = pyelem->callback;
		inside = 0;
	}

	t = PyTuple_New(2);
	if (t) {
		PyTuple_SET_ITEM(t, 0, (PyObject *)pyelem);
		Py_INCREF(pyelem);
		PyTuple_SET_ITEM(t, 1, PyInt_FromLong(mask));
		r = PyObject_CallObject(o, t);
		Py_DECREF(t);
			
		if (r) {
			if (PyLong_Check(r)) {
				res = PyLong_AsLong(r);
			}
#if PY_MAJOR_VERSION < 3
			else if (PyInt_Check(r)) {
				res = PyInt_AsLong(r);
			}
#endif
			 else if (r == Py_None) {
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
