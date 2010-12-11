from distutils.core import setup, Extension

setup(name='python-libfov',
      version='0.1',
      description='CPython Extension for libfov',
      author='Peter Ruibal',
      author_email='ruibalp@gmail.com',
      url='https://github.com/fmoo/python-libfov',
      ext_modules = [
        Extension('fov', ['fov/fov.c'],
                  libraries=['fov'])
      ],
     )
