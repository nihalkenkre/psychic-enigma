import argparse
import os
from pathlib import Path
import subprocess
import shutil


def delete_spv(path):
    print('Deleting spv from ' + str(path))

    for spv in path.glob('*.spv'):
        os.remove(spv)


def build_glsl(shader_path, build_type):
    print('Building GLSL...')

    for glsl in shader_path.glob('*.glsl'):
        if not glsl.match('utils.glsl'):
            spv_name = str(glsl) + '.spv'

            cmd = 'glslang ' + str(glsl)
            if (build_type == 'Debug'):
                cmd += ' -gVS -Od '
            elif (build_type == 'MinSizeRel'):
                cmd += ' -g0 -Os '
            cmd += ' --target-env vulkan1.4 -o ' + str(spv_name)

            subprocess.call(cmd)


def copy_spv(src, dst):
    print('Copy spv to ' + str(dst))

    if not dst.exists():
        os.mkdir(dst)

    for src_glsl in src.glob('*.spv'):
        shutil.copy(src_glsl, dst)


def main(args):
    local_shader_path = Path('.').resolve().parent / 'shaders' / 'glsl'
    delete_spv(local_shader_path)
    build_glsl(local_shader_path, args.build_type)

    remote_shader_path = Path(args.output_dir) / 'shaders' / 'glsl'
    delete_spv(remote_shader_path)
    copy_spv(local_shader_path, remote_shader_path)
    delete_spv(local_shader_path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument('source_dir')
    parser.add_argument('build_type')
    parser.add_argument('output_dir')

    main(parser.parse_args())
