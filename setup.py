from distutils.core import setup, Extension

setup(name='python-libfov',
      version='0.1',
      ext_modules = [
        Extension('fov', ['fov/fov.c'],
                  libraries=['fov'])
      ],
     )
