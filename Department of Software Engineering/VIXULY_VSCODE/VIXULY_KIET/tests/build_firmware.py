"""Build every src_goc/src source and Arduino AVR core, without uploading."""
import argparse
from pathlib import Path
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--avr-bin', type=Path, required=True)
    p.add_argument('--arduino-avr', type=Path, required=True)
    args = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = Path('tests/build/firmware')
    (root / build).mkdir(parents=True, exist_ok=True)
    core = args.arduino_avr / 'cores/arduino'
    variant = args.arduino_avr / 'variants/standard'

    def run(*command):
        subprocess.run([str(x) for x in command], cwd=root, check=True)

    common = ['-mmcu=atmega88pa', '-DF_CPU=16000000UL', '-DARDUINO=10819',
              '-Os', '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra',
              '-I', core, '-I', variant]
    app_objects = []
    for source in sorted((root / 'src').iterdir()):
        if source.suffix not in ('.c', '.cpp'):
            continue
        cpp = source.suffix == '.cpp'
        obj = build / (source.name + '.o')
        run(args.avr_bin / ('avr-g++.exe' if cpp else 'avr-gcc.exe'), *common,
            '-std=gnu++11' if cpp else '-std=gnu11', '-Werror',
            '-c', source.relative_to(root), '-o', obj)
        app_objects.append(obj)

    core_objects = []
    for source in sorted(core.iterdir()):
        if source.suffix not in ('.c', '.cpp', '.S'):
            continue
        cpp = source.suffix == '.cpp'
        obj = build / ('core_' + source.name + '.o')
        language = ['-std=gnu++11', '-fno-exceptions'] if cpp else (['-std=gnu11'] if source.suffix == '.c' else [])
        run(args.avr_bin / ('avr-g++.exe' if cpp else 'avr-gcc.exe'), *common,
            *language, '-c', source, '-o', obj)
        core_objects.append(obj)
    archive = build / 'core.a'
    run(args.avr_bin / 'avr-ar.exe', 'rcs', archive, *core_objects)
    run(args.avr_bin / 'avr-g++.exe', '-mmcu=atmega88pa', '-Wl,--gc-sections',
        f'-Wl,-Map,{build / "firmware.map"}', *app_objects, archive, '-lm',
        '-o', build / 'firmware.elf')
    run(args.avr_bin / 'avr-size.exe', build / 'firmware.elf')
    run(args.avr_bin / 'avr-objcopy.exe', '-O', 'ihex', '-R', '.eeprom',
        build / 'firmware.elf', build / 'firmware.hex')
    print('PASS: all application sources compiled; complete firmware linked.')


if __name__ == '__main__':
    main()
