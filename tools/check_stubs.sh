#!/bin/sh
# Check the host stubs against the real SDK headers.
#
# `make check` compiles src/ against tests/stubs/, so a stub that declares
# a function the SDK puts in a different header - or does not have at all -
# makes the type-check agree with nothing and the failure only shows up
# when the Vita toolchain compiles for real. That happened with
# sceIoMkdir, which the SDK declares in psp2/io/stat.h while the stub had
# it in psp2/io/dirent.h.
#
# So: for every stub header, every function it declares must be declared
# in the SDK header sitting at the same relative path.
#
# Needs $VITASDK. Run from the project root:  sh tools/check_stubs.sh
set -eu

if [ -z "${VITASDK:-}" ]; then
    echo "check_stubs needs the VitaSDK: set VITASDK to compare against its headers"
    exit 1
fi
INC="$VITASDK/arm-vita-eabi/include"
fail=0
checked=0

for stub in $(find tests/stubs -name '*.h' | sort); do
    rel=${stub#tests/stubs/}

    sdk="$INC/$rel"
    if [ ! -f "$sdk" ]; then
        # vitaGL and friends may install somewhere else under $VITASDK
        sdk=$(find "$VITASDK" -path "*/$rel" -type f 2>/dev/null | head -n 1 || true)
    fi
    if [ -z "$sdk" ] || [ ! -f "$sdk" ]; then
        echo "MISSING  no SDK header at $rel (stub: $stub)"
        fail=1
        continue
    fi

    # function-looking identifiers the stub declares
    syms=$(grep -ohE '\b(sce[A-Za-z0-9_]*|vgl[A-Za-z0-9_]*|gl[A-Z][A-Za-z0-9_]*)[[:space:]]*\(' \
           "$stub" | tr -d '( \t' | sort -u)

    for s in $syms; do
        checked=$((checked + 1))
        if ! grep -qE "\b$s\b" "$sdk"; then
            echo "WRONG    $s is declared in $rel but not in $sdk"
            fail=1
        fi
    done
done

if [ "$fail" = "0" ]; then
    echo "all $checked stub declarations match the SDK header of the same path"
else
    echo "stub headers disagree with the SDK - make check cannot be trusted"
fi
exit $fail
