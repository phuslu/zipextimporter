/*
  For the memimporter compiled into py2exe exe-stubs we need "Python-dynload.h".
  For the standalone .pyd we need <Python.h>
*/
#define STANDALONE

#ifdef STANDALONE
#  include <Python.h>
#  include "Python-version.h"
#else
#  include "Python-dynload.h"
#  include <stdio.h>
#endif
#include <windows.h>

static char module_doc[] =
"Importer which can load extension modules from memory";

#include "MemoryModule.h"

static void *memdup(void *ptr, Py_ssize_t size)
{
	void *p = malloc(size);
	if (p == NULL)
		return NULL;
	memcpy(p, ptr, size);
	return p;
}

/*
  Be sure to detect errors in FindLibrary - undetected errors lead to
  very strange bahaviour.
*/
static void* FindLibrary(char *name, PyObject *callback)
{
	PyObject *result;
	char *p;
	Py_ssize_t size;

	if (callback == NULL)
		return NULL;
	result = PyObject_CallFunction(callback, "s", name);
	if (result == NULL) {
		PyErr_Clear();
		return NULL;
	}
	if (-1 == PyBytes_AsStringAndSize(result, &p, &size)) {
		PyErr_Clear();
		Py_DECREF(result);
		return NULL;
	}
	p = memdup(p, size);
	Py_DECREF(result);
	return p;
}

static PyObject *set_find_proc(PyObject *self, PyObject *args)
{
	PyObject *callback = NULL;
	if (!PyArg_ParseTuple(args, "|O:set_find_proc", &callback))
		return NULL;
	Py_XDECREF((PyObject *)findproc_data);
	Py_XINCREF(callback);
	findproc_data = (void *)callback;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
import_module(PyObject *self, PyObject *args)
{
	PyObject* m = NULL;
	char *data;
	int size;
	char *initfuncname;
	char *modname;
	char *pathname;
	HMEMORYMODULE hmem;
	FARPROC do_init;

	char *oldcontext;

	/* code, initfuncname, fqmodulename, path */
	if (!PyArg_ParseTuple(args, "s#sss:import_module",
			      &data, &size,
			      &initfuncname, &modname, &pathname))
		return NULL;
	hmem = MemoryLoadLibrary(pathname, data);
	if (!hmem) {
		PyErr_Format(PyExc_ImportError,
			     "MemoryLoadLibrary failed loading %s", pathname);
		return NULL;
	}
	do_init = MemoryGetProcAddress(hmem, initfuncname);
	if (!do_init) {
		MemoryFreeLibrary(hmem);
		PyErr_Format(PyExc_ImportError,
			     "Could not find function %s", initfuncname);
		return NULL;
	}

    oldcontext = _Py_PackageContext;
	_Py_PackageContext = modname;
	m = ((PyObject*(*)(void))do_init)();
	_Py_PackageContext = oldcontext;
	if (PyErr_Occurred()) {
	    PyErr_Format(PyExc_ImportError,
	                 "initialization of %s raised unreported exception",
	                 modname);
	    return NULL;
	}
	/* Remember pointer to module init function. */
	PyModule_GetDef(m)->m_base.m_init = (PyObject*(*)(void))do_init;
	return m;
}

static PyObject *
get_verbose_flag(PyObject *self, PyObject *args)
{
	return PyLong_FromLong(Py_VerboseFlag);
}

static PyMethodDef methods[] = {
	{ "import_module", import_module, METH_VARARGS,
	  "import_module(code, initfunc, dllname[, finder]) -> module" },
	{ "get_verbose_flag", get_verbose_flag, METH_NOARGS,
	  "Return the Py_Verbose flag" },
	{ "set_find_proc", set_find_proc, METH_VARARGS },
	{ NULL, NULL },		/* Sentinel */
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "memimporter",     /* m_name */
    module_doc,  /* m_doc */
    -1,                  /* m_size */
    methods,             /* m_methods */
    NULL,                /* m_reload */
    NULL,                /* m_traverse */
    NULL,                /* m_clear */
    NULL,                /* m_free */
};

void
PyInit_memimporter(void)
{
	findproc = FindLibrary;
	PyModule_Create2(&moduledef, PYTHON_API_VERSION);
}
