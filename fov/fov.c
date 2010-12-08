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
pyfov_beam(pyfov_SettingsObject *self, PyObject *args) {
  void *map, *src;
  int source_x, source_y;
  unsigned radius;
  struct map_wrapper wrap;

  if (!PyArg_ParseTuple(args, "ooiiI", &map, &src,
                        &source_x, &source_y, &radius))
    return NULL;

  // Initialize wrap to pass as map instead of *map.
  wrap.orig_map = map;
  wrap.pyfov_settings = self;

  fov_beam(self->settings, wrap, src,
           source_x, source_y, radius);

  Py_INCREF(Py_None);
  return Py_None;
}

/**
 * Wrapper for fov_circle
 */
static PyObject *
pyfov_circle(pyfov_SettingsObject *self, PyObject *args) {
  void *map, *src;
  int source_x, source_y;
  unsigned radius;
  fov_direction_type direction;
  float angle;
  struct map_wrapper wrap;

  if (!PyArg_ParseTuple(args, "ooiiIIf", &map, &src,
                        &source_x, &source_y, &radius,
                        &direction, &angle))
    return NULL;

  // Initialize wrap to pass as map instead of *map.
  wrap.orig_map = map;
  wrap.pyfov_settings = self;

  fov_circle(self->settings, wrap, src,
             source_x, source_y, radius,
             direction, angle);

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
  map_wrapper *wrap = (map_wrapper *)map;

  // TODO call wrap->settings->opacity_test_function()
  // with (wrap->orig_map, x, y) as args
  return false;
}

static void
_pyfov_apply_lighting_function(void *map, int x, int y, int dx, int dy,
                               void *src) {
  map_wrapper *wrap = (map_wrapper *)map;

  // fov_circle and fov_beam are called with python objects instead of raw
  // c pointers.  We'll cast the void * to a PyObject * before injvoking the
  // callback
  
  // TODO call wrap->settings->apply_lighting_function with
  // (wrap->orig_map, x, y, dx, dy, src) as args
  //
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

