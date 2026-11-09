"""Windows host tests plus AVR compile/link checks; no board flashing.

Usage: python run_tests.py --clang PATH --lld PATH --avr-bin DIRECTORY
clang must support x86_64-pc-windows-msvc; lld must support -flavor link.
Only Python's standard library is used. Outputs go into tests/build.
"""
import argparse
import ctypes
from pathlib import Path
import subprocess


def run(*args, cwd=None):
    subprocess.run([str(arg) for arg in args], check=True, cwd=cwd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang", required=True)
    parser.add_argument("--lld", required=True)
    parser.add_argument("--avr-bin", type=Path, required=True)
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    build = tests / "build"
    build.mkdir(exist_ok=True)

    for stop_on_select in (1, 0):
        obj = build / f"host_{stop_on_select}.obj"
        dll = build / f"host_{stop_on_select}.dll"
        run(args.clang, "-target", "x86_64-pc-windows-msvc", "-ffreestanding",
            "-fno-exceptions", "-fno-rtti", "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-DF_CPU=16000000UL", f"-DBUTTON_STOP_ON_SELECT={stop_on_select}",
            "-I", tests / "mock_avr", "-c", tests / "button_test.cpp", "-o", obj)
        run(args.lld, "-flavor", "link", "/dll", "/noentry", "/nodefaultlib",
            f"/out:{dll}", obj)
        lib = ctypes.CDLL(str(dll))
        failing_line = lib.run_button_tests()
        count = lib.button_test_checks()
        if failing_line:
            raise RuntimeError(f"stop_on_select={stop_on_select}: test failed at line {failing_line}")
        print(f"PASS: STOP_ON_SELECT={stop_on_select}, {count} host assertions", flush=True)

    motion_obj = build / 'host_motion.obj'
    run(args.clang, '-target', 'x86_64-pc-windows-msvc', '-ffreestanding', '-x', 'c',
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-I', tests / 'mock_avr',
        '-c', tests.parent / 'src/motion.c', '-o', motion_obj)
    for stop_on_select in (1, 0):
        obj = build / f'app_{stop_on_select}.obj'
        dll = build / f'app_{stop_on_select}.dll'
        run(args.clang, '-target', 'x86_64-pc-windows-msvc', '-ffreestanding',
            '-fno-exceptions', '-fno-rtti', '-std=c++11', '-Wall', '-Wextra', '-Werror',
            '-DF_CPU=16000000UL', f'-DBUTTON_STOP_ON_SELECT={stop_on_select}',
            '-I', tests / 'mock_avr', '-c', tests / 'app_test.cpp', '-o', obj)
        run(args.lld, '-flavor', 'link', '/dll', '/noentry', '/nodefaultlib',
            f'/out:{dll}', obj, motion_obj)
        app = ctypes.CDLL(str(dll))
        failing_line = app.run_app_tests()
        if failing_line:
            raise RuntimeError(f'app STOP_ON_SELECT={stop_on_select}: failed at line {failing_line}')
        print(f'PASS: app STOP_ON_SELECT={stop_on_select}, {app.button_test_checks()} integration assertions', flush=True)

    avr_c = args.avr_bin / "avr-gcc.exe"
    avr_cpp = args.avr_bin / "avr-g++.exe"
    flags = ["-mmcu=atmega88pa", "-DF_CPU=16000000UL", "-Os", "-Wall", "-Wextra", "-Werror"]
    # Old Windows AVR tools cannot parse Vietnamese paths: use relative paths.
    run(avr_cpp, *flags, "-std=gnu++11", "-fno-exceptions", "-fno-rtti",
        "-c", "../src/button.cpp", "-o", "build/button.o", cwd=tests)
    run(avr_c, *flags, "-std=gnu11", "-c", "../src/motor.c", "-o", "build/motor.o", cwd=tests)
    run(avr_c, *flags, "-std=gnu11", "-I", "../src",
        "-c", "avr_smoke.c", "-o", "build/avr_smoke.o", cwd=tests)
    run(avr_cpp, "-mmcu=atmega88pa", "build/avr_smoke.o", "build/button.o",
        "build/motor.o", "-o", "build/button_smoke.elf", cwd=tests)
    run(args.avr_bin / "avr-size.exe", "build/button_smoke.elf", cwd=tests)
    print("PASS: AVR ATmega88PA compile/link, including C/C++ linkage", flush=True)


if __name__ == "__main__":
    main()
