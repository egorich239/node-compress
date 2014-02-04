import Options
from os import unlink, symlink, popen
from os.path import exists 
from shutil import copy2 as copy

srcdir = "."
blddir = "build"
VERSION = "0.1.10"

TARGET = 'compress-bindings'
TARGET_FILE = '%s.node' % TARGET
built = 'build/default/%s' % TARGET_FILE
dest = 'lib/compress/%s' % TARGET_FILE


def set_options(opt):
  opt.tool_options("compiler_cxx")

  opt.add_option('--debug', dest='debug', action='store_true', default=False)
  opt.add_option('--with-gzip', dest='gzip', action='store_true', default=True)
  opt.add_option('--no-gzip', dest='gzip', action='store_false')
  opt.add_option('--with-bzip', dest='bzip', action='store_true', default=False)
  opt.add_option('--no-bzip', dest='bzip', action='store_false')

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("node_addon")

  conf.env.DEFINES = []
  conf.env.USELIB = []

  if Options.options.gzip:
    conf.check_cxx(lib='z',
                   uselib_store='ZLIB',
                   mandatory=True)
    conf.env.DEFINES += [ 'WITH_GZIP' ]
    conf.env.USELIB += [ 'ZLIB' ]

  if Options.options.bzip:
    conf.check_cxx(lib='bz2',
                   uselib_store='BZLIB',
                   mandatory=True)
    conf.env.DEFINES += [ 'WITH_BZIP' ]
    conf.env.USELIB += [ 'BZLIB' ]

  if Options.options.debug:
    conf.env.DEFINES += [ 'DEBUG' ]
    conf.env.CXXFLAGS = [ '-O0', '-g3' ]
  else:
    conf.env.CXXFLAGS = [ '-O2' ]


def build(bld):
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.cxxflags = ["-D_FILE_OFFSET_BITS=64", "-D_LARGEFILE_SOURCE", "-Wall"]
  obj.target = TARGET
  obj.source = "src/compress.cc"
  obj.defines = bld.env.DEFINES
  obj.uselib = bld.env.USELIB


def shutdown():
  if Options.commands['clean']:
      if exists(dest):
          unlink(dest)
  else:
      if exists(built):
          copy(built, dest)
