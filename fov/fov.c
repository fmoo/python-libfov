#include <Python.h>
#include "fov/fov.h"

#define SET_INCREF(A, B) \
  Py_INCREF(B); \
  A = B;

#define ASSIGN_REFS(A, B) \
  Py_DECREF(A); \
  Py_INCREF(B); \
  A = B;

/**
 * pyfov provides an object oriented wrapper for libfov.
 *
 * Example Usage:
 *
 * import fov
 * s = fov.settings()
 * s.opaque_apply = fov.OPAQUE_NOAPPLY
 * s.opacity_test_function = lambda map, x, y: \
 *  x < 0 or x > 5 or y < 0 or y > 5
 * s.circle(None, None, 4, 4, 3)
 */

/**
 * Define the wrapper around the core C settings,
 * since our python callbacks won't match the signatures
 * of the underlying C library.
 */
typedef struct {
  PyObject_HEAD

  /**
   * Raw C settings object.  This will get passed to all the setters fo
   * libfov.
   */
  fov_settings_type settings;

  /**
   * Python callback for testing opacity
   */
  PyObject *opacity_test_function;

  /**
   * Python callback for applying lighting 
   */
  PyObject *apply_lighting_function;
} pyfov_Settings;

/**
 * Ugly hack to sneak the PyObject into the C API
 * so that we can properly route the callbacks back
 * to their assigned callbacks without cheating.
 */
typedef struct {
  void *orig_map;
  pyfov_Settings *settings;
  bool threw_exception;
} map_wrapper;

// Global pyfov callbacks for all calls to fov_beam, etc
static bool _pyfov_opacity_test_function(void *map, int x, int y);
static void _pyfov_apply_lighting_function(void *map, int x, int y,
                                           int dx, int dy, void *src);

/**
 * Primary Interface Methods
 */
static int
pyfov_Settings_init(pyfov_Settings *self, PyObject *args, PyObject *kwargs) {
  static char *kwlist[] = {NULL, };

  // Our __init__ function doesn't take any arguments...
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|", kwlist)) {
    return -1;
  }

  SET_INCREF(self->opacity_test_function, Py_None);
  SET_INCREF(self->apply_lighting_function, Py_None);

  // Init the underlying settings datastructure
  fov_settings_init(&self->settings);

  // Set the pyfov handlers internally for all instances.
  // these are ALWAYS handles by these functions
  fov_settings_set_opacity_test_function(&self->settings,
    _pyfov_opacity_test_function);
  fov_settings_set_apply_lighting_function(&self->settings,
    _pyfov_apply_lighting_function);

  return 0;
}

static void
pyfov_Settings_dealloc(pyfov_Settings *self)
{
  // Free the underlying implementation
  fov_settings_free(&self->settings);
  self->ob_type->tp_free(self);
}

/**
 * Property implementations
 */
static PyObject *
pyfov_Settings_get_opacity_test_function(pyfov_Settings *self, void *data) {
  Py_INCREF(self->opacity_test_function);
  return self->opacity_test_function;
}

static int
pyfov_Settings_set_opacity_test_function(pyfov_Settings *self, PyObject *cb,
                                         void *data) {
  ASSIGN_REFS(self->opacity_test_function, cb);
  return 0;
}

static PyGetSetDef pyfov_Settings_properties[] = {
  {"opacity_test_function",
   (getter)pyfov_Settings_get_opacity_test_function,
   (setter)pyfov_Settings_set_opacity_test_function,
   "", NULL},
  /* Sentinel */
  {NULL, NULL, NULL, NULL, NULL},
};

/**
 * Wrapper for fov_beam
 */
static PyObject *
pyfov_Settings_beam(pyfov_Settings *self, PyObject *args) {
  void *map, *src;
  int source_x, source_y;
  unsigned radius;
  fov_direction_type direction;
  float angle;
  map_wrapper wrap;

  if (!PyArg_ParseTuple(args, "OOiiIIf", &map, &src,
                        &source_x, &source_y, &radius,
                        &direction, &angle))
    return NULL;

  // Initialize wrap to pass as map instead of *map.
  wrap.orig_map = map;
  wrap.settings = self;
  wrap.threw_exception = false;

  fov_beam(&self->settings, &wrap, src,
           source_x, source_y, radius,
           direction, angle);

  if (wrap.threw_exception)
    return NULL;

  Py_INCREF(Py_None);
  return Py_None;
}

/**
 * Wrapper for fov_circle
 */
static PyObject *
pyfov_Settings_circle(pyfov_Settings *self, PyObject *args) {
  void *map, *src;
  int source_x, source_y;
  unsigned radius;
  map_wrapper wrap;

  if (!PyArg_ParseTuple(args, "OOiiI", &map, &src,
                        &source_x, &source_y, &radius))
    return NULL;

  // Initialize wrap to pass as map instead of *map.
  wrap.orig_map = map;
  wrap.settings = self;
  wrap.threw_exception = false;

  fov_circle(&self->settings, &wrap, src,
             source_x, source_y, radius);

  if (wrap.threw_exception)
    return NULL;

  Py_INCREF(Py_None);
  return Py_None;
}


/**
 * Stub for SettingsType
 */
static PyTypeObject pyfov_SettingsType = {
  PyObject_HEAD_INIT(NULL)
};


static bool
_pyfov_opacity_test_function(void *map, int x, int y) {
  PyObject *arglist;
  PyObject *result;
  map_wrapper *wrap = (map_wrapper *)map;
  bool test_func_result;

  // Early out if no user-callback was set
  if (wrap->settings->opacity_test_function == Py_None)
    return false;

  // Pack up the C return values to python objects
  arglist = Py_BuildValue("(Oii)", (PyObject *)wrap->orig_map, x, y);
  result = PyObject_CallObject(wrap->settings->opacity_test_function,
                               arglist);

  // decref the O arguments since py_BuildValue() increments refs on these
  Py_DECREF((PyObject *)wrap->orig_map);
  Py_DECREF(arglist);

  // If the callback threw an exception, we trace back through the C code...
  // We should really stop the execution of additional callbacks, but I'm not
  // sure if this can be done without doing something super gnarly with the
  // raw C settings object.
  if (result == NULL) {
    wrap->threw_exception = true;
    return false;
  }

  test_func_result = (bool)PyInt_AsLong(result);
  Py_DECREF(result);

  return test_func_result;
}

static void
_pyfov_apply_lighting_function(void *map, int x, int y, int dx, int dy,
                               void *src) {
  PyObject *arglist;
  PyObject *result;
  map_wrapper *wrap = (map_wrapper *)map;

  // Early out if no user-callback was set
  if (wrap->settings->apply_lighting_function == Py_None)
    return;

  // Pack up the C return values to python objects
  arglist = Py_BuildValue("(OiiiiO)", (PyObject *)wrap->orig_map, x, y,
                          dx, dy, (PyObject *)src);
  result = PyObject_CallObject(wrap->settings->apply_lighting_function,
                               arglist);
  
  // decref the O arguments since py_BuildValue() increments refs on these
  Py_DECREF((PyObject *)wrap->orig_map);
  Py_DECREF((PyObject *)src);
  Py_DECREF(arglist);

  // If the callback threw an exception, we trace back through the C code...
  // We should really stop the execution of additional callbacks, but I'm not
  // sure if this can be done without doing something super gnarly with the
  // raw C settings object.
  if (result == NULL) {
    wrap->threw_exception = true;
    return;
  }

  Py_DECREF(result);
}

static PyMethodDef pyfov_methods[];

static void init_fov_settings_type(PyTypeObject *t);

PyMODINIT_FUNC
initfov(void)
{
  PyObject *m;

  // Register module global functions
  m = Py_InitModule("fov", pyfov_methods);
  if (m == NULL)
    return;

  // Type definition
  init_fov_settings_type(&pyfov_SettingsType);

  if (PyType_Ready(&pyfov_SettingsType) < 0)
    return;

  // Add the custom type to this module
  PyModule_AddObject(m, "Settings", (PyObject *)&pyfov_SettingsType);

  // Add consts from fov.h to python module

  // fov_direction_type
  PyModule_AddIntConstant(m, "EAST", FOV_EAST);
  PyModule_AddIntConstant(m, "NORTHEAST", FOV_NORTHEAST);
  PyModule_AddIntConstant(m, "NORTH", FOV_NORTH);
  PyModule_AddIntConstant(m, "NORTHWEST", FOV_NORTHWEST);
  PyModule_AddIntConstant(m, "WEST", FOV_WEST);
  PyModule_AddIntConstant(m, "SOUTHWEST", FOV_SOUTHWEST);
  PyModule_AddIntConstant(m, "SOUTH", FOV_SOUTH);
  PyModule_AddIntConstant(m, "SOUTHEAST", FOV_SOUTHEAST);


  // These should be consts on some subobject/submodule

  // fov_shape_type
  PyModule_AddIntConstant(m, "SHAPE_CIRCLE_PRECALCULATE",
                          FOV_SHAPE_CIRCLE_PRECALCULATE);
  PyModule_AddIntConstant(m, "SHAPE_SQUARE",
                          FOV_SHAPE_SQUARE);
  PyModule_AddIntConstant(m, "SHAPE_CIRCLE",
                          FOV_SHAPE_CIRCLE);
  PyModule_AddIntConstant(m, "SHAPE_OCTAGON",
                          FOV_SHAPE_OCTAGON);

  // fov_corner_peek_type
  PyModule_AddIntConstant(m, "CORNER_NOPEEK", FOV_CORNER_NOPEEK);
  PyModule_AddIntConstant(m, "CORNER_PEEK", FOV_CORNER_PEEK);

  // fov_opaque_apply_type
  PyModule_AddIntConstant(m, "OPAQUE_APPLY", FOV_OPAQUE_APPLY);
  PyModule_AddIntConstant(m, "OPAQUE_NOAPPLY", FOV_OPAQUE_NOAPPLY);

}

static PyMethodDef pyfov_methods[] = {
  {NULL, NULL, 0, NULL} /* Sentinel */
};

static PyMethodDef pyfov_Settings_methods[] = {
  // We set METH_VARARGS to require a sane calling convention, even
  // though we require all the args.  PyArg_ParseTuple does some
  // awesome error handling.
  {"beam", (PyCFunction)pyfov_Settings_beam, METH_VARARGS, NULL},
  {"circle", (PyCFunction)pyfov_Settings_circle, METH_VARARGS, NULL},
  {NULL, NULL, 0, NULL} /* Sentinel */
};

static void
init_fov_settings_type(PyTypeObject *t) {
  t->tp_name = "fov.Settings";
  t->tp_basicsize = sizeof(pyfov_Settings);
  t->tp_flags = Py_TPFLAGS_DEFAULT;
  t->tp_doc = "FOV Settings Object";

  // We cast these, since their real signatures pass pyfov_Settings *
  // instead of PyObject *, and we'd like the compiler to implicitly cast
  // the PyObject * to the type we care about.
  t->tp_init = (initproc)pyfov_Settings_init;
  t->tp_dealloc = (destructor)pyfov_Settings_dealloc;

  // Use a generic new method (inits members to 0/NULL)
  t->tp_new = PyType_GenericNew;
  t->tp_methods = pyfov_Settings_methods;
  t->tp_getset = pyfov_Settings_properties;
}
