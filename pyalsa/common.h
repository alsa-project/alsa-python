/*
 *  Python binding for the ALSA library - Common macros
 *  Copyright (c) 2018 by Jaroslav Kysela <perex@perex.cz>
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

#ifndef __PYALSA_COMMON_H
#define __PYALSA_COMMON_H

#include "Python.h"
#include "structmember.h"
#include "frameobject.h"
#ifndef PY_LONG_LONG
  #define PY_LONG_LONG LONG_LONG
#endif

#ifndef Py_RETURN_NONE
  #define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#endif
#ifndef Py_RETURN_TRUE
  #define Py_RETURN_TRUE return Py_INCREF(Py_True), Py_True
#endif
#ifndef Py_RETURN_FALSE
  #define Py_RETURN_FALSE return Py_INCREF(Py_False), Py_False
#endif

#if PY_MAJOR_VERSION >= 3
  #define PyInt_FromLong PyLong_FromLong
#endif

/*
 *
 */

#if PY_MAJOR_VERSION >= 3
  #define MOD_ERROR_VAL NULL
  #define MOD_SUCCESS_VAL(val) val
  #define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
  #define MOD_DEF(ob, name, doc, methods) \
          static struct PyModuleDef moduledef = { \
            PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
          ob = PyModule_Create(&moduledef);
#else
  #define MOD_ERROR_VAL
  #define MOD_SUCCESS_VAL(val)
  #define MOD_INIT(name) void init##name(void)
  #define MOD_DEF(ob, name, doc, methods) \
          ob = Py_InitModule3(name, methods, doc);
#endif

/*
 *
 */

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

static inline int get_long1(PyObject *o, long *val)
{
#if PY_MAJOR_VERSION < 3
	if (PyInt_Check(o)) {
		*val = PyInt_AsLong(o);
		return 0;
	}
#endif
	if (PyLong_Check(o)) {
		*val = PyLong_AsLong(o);
		return 0;
	}
	return 1;
}

static inline int get_long(PyObject *o, long *val)
{
	if (!get_long1(o, val))
		return 0;
#if PY_MAJOR_VERSION < 3
	PyErr_Format(PyExc_TypeError, "Only None, Int or Long types are expected!");
#else
	PyErr_Format(PyExc_TypeError, "Only None or Long types are expected!");
#endif
	return 1;
}

static inline int get_utf8_string(PyObject *o, char **str)
{
	PyObject *e = PyUnicode_AsEncodedString(o, "utf-8", "strict");
	char *s = e ? PyBytes_AsString(e) : NULL;
	if (s)
		s = strdup(s);
	*str = s;
	Py_XDECREF(e);
	return s == NULL;
}

static inline PyObject *InternFromString(const char *name)
{
#if PY_MAJOR_VERSION < 3
	return PyString_InternFromString("register");
#else
	return PyUnicode_InternFromString("register");
#endif
}

#endif /* __PYALSA_COMMON_H */
