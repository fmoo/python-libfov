#include <Python.h>
#include "fov/fov.h"

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
} pyfov_SettingsObject;

/**
 * Ugly hack to sneak the PyObject into the C API
 * so that we can properly route the callbacks back
 * to their assigned callbacks without cheating.
 */
typedef struct {
  void *orig_map;
  pyfov_SettingsObject *settings;
} map_wrapper;

/**
 * Primary Interface Methods
 */

/**
 * Wrapper for fov_beam
 */
static PyObject *
pyfov_beam(PyObject *self, PyObject *args) {
  pyfov_SettingsObject *settings = (pyfov_SettingsObject *)self;
  void *map, *src;
  int source_x, source_y;
  unsigned radius;
  fov_direction_type direction;
  float angle;
  map_wrapper wrap;

  if (!PyArg_ParseTuple(args, "ooiiIIf", &map, &src,
                        &source_x, &source_y, &radius,
                        &direction, &angle))
    return NULL;

  // Initialize wrap to pass as map instead of *map.
  wrap.orig_map = map;
  wrap.settings = settings;

  fov_beam(&settings->settings, &wrap, src,
           source_x, source_y, radius,
           direction, angle);

  Py_INCREF(Py_None);
  return Py_None;
}

/**
 * Wrapper for fov_circle
 */
static PyObject *
pyfov_circle(PyObject *self, PyObject *args) {
  pyfov_SettingsObject *settings = (pyfov_SettingsObject *)self;
  void *map, *src;
  int source_x, source_y;
  unsigned radius;
  map_wrapper wrap;

  if (!PyArg_ParseTuple(args, "ooiiI", &map, &src,
                        &source_x, &source_y, &radius))
    return NULL;

  // Initialize wrap to pass as map instead of *map.
  wrap.orig_map = map;
  wrap.settings = settings;

  fov_circle(&settings->settings, &wrap, src,
             source_x, source_y, radius);

  Py_INCREF(Py_None);
  return Py_None;
}


/**
 * GetSet Methods
 */

/**
 * Settings Type Definition
 */
static PyTypeObject pyfov_SettingsType = {
  PyObject_HEAD_INIT(NULL)
  0,                            /* ob_size */
  "fov.Settings",               /* tp_name */
  sizeof(pyfov_SettingsObject), /* tp_basicsize */
  0,                            /* tp_itemsize */
  0,                            /* tp_dealloc */
  0,                            /* tp_print */
  0,                            /* tp_getattr */
  0,                            /* tp_setattr */
  0,                            /* tp_compare */
  0,                            /* tp_repr */
  0,                            /* tp_as_number */
  0,                            /* tp_as_sequence */
  0,                            /* tp_as_mapping */
  0,                            /* tp_hash */
  0,                            /* tp_call */
  0,                            /* tp_str */
  0,                            /* tp_getattro */
  0,                            /* tp_setattro */
  0,                            /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT,           /* tp_flags */
  "Settings objects",           /* tp_doc */  
};

/**
 * pyfov_Settings functions
 */


static bool
_pyfov_opacity_test_function(void *map, int x, int y) {
  PyObject *arglist;
  PyObject *result;
  map_wrapper *wrap = (map_wrapper *)map;
  bool test_func_result;

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
  if (result == NULL)
    return false;

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
  if (result == NULL)
    return;

  Py_DECREF(result);
}

static PyMethodDef FovModuleMethods[];

PyMODINIT_FUNC
initfov(void)
{
  PyObject *m;

  // Register module global functions
  m = Py_InitModule("fov", FovModuleMethods);
  if (m == NULL)
    return;

  // Type definition
  pyfov_SettingsType.tp_new = PyType_GenericNew;

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

static PyMethodDef FovModuleMethods[] = {
  {NULL, NULL, 0, NULL} /* Sentinel */
};

