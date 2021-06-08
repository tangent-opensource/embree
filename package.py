# -*- coding: utf-8 -*-

name = 'embree'

version = '3.8.0-ta.1.2.1'

authors = [
    'alex.fuller',
    'benjamin.skinner',
]

requires = [
    'tbb-2019'
]

@early()
def private_build_requires():
    import sys
    if 'win' in str(sys.platform):
        return ['visual_studio']
    else:
        return ['gcc-6']

variants = [
    ['platform-windows', 'arch-x64', 'os-windows-10'],
    ['platform-linux', 'arch-x86_64', 'os-centos-7'],
]

build_system = "cmake"

def commands():

    # Split and store version and package version
    split_versions = str(version).split('-')
    env.EMBREE_VERSION.set(split_versions[0])
    env.EMBREE_PACKAGE_VERSION.set(split_versions[1])

    env.EMBREE_ROOT.set("{root}")
    env.EMBREE_ROOT_DIR.set("{root}")
    env.EMBREE_INCLUDE_DIR.set("{root}/include")
    env.EMBREE_LIBRARY_DIR.set("{root}/lib")

    import sys
    if 'win' not in str(sys.platform):
        env.EMBREE_LIBRARY_DIR.set("{root}/lib64")
        env.LD_LIBRARY_PATH.append("{root}/lib64")

    env.EMBREE_BINARY_DIR.set("{root}/bin")

    env.PATH.append( str(env.EMBREE_BINARY_DIR) )
