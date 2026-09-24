#!/bin/sh
# Check the host stubs against the real SDK headers.
#
# `make check` compiles src/ against tests/stubs/, so a stub that declares
# a function the SDK does not put behind the same include makes the
# type-check agree with nothing, and the failure only shows up when the
# Vita toolchain compiles for real. That happened with sceIoMkdir: the
# stub had it in psp2/io/dirent.h, the SDK declares it in psp2/io/stat.h,
# and dirent.h does not pull stat.h in.
#
# The rule, then, is the one the compiler applies: for every stub header,
# including the SDK header at the same relative path must make each
# function the stub declares visible - directly or through that header's
# own includes.
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
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# every header reachable from $1 by #include, resolved inside the SDK
reachable() {
    printf '%s\n' "$1" > "$work/set"
    depth=0
    while [ "$depth" -lt 6 ]; do
        while IFS= read -r f; do
            [ -f "$f" ] || continue
            grep -hoE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^>"]+[>"]' "$f" 2>/dev/null |
                sed -E 's/.*[<"]([^>"]+)[>"].*/\1/'
        done < "$work/set" | while IFS= read -r inc; do
            [ -f "$INC/$inc" ] && printf '%s\n' "$INC/$inc"
        done > "$work/found" || true
        cat "$work/set" "$work/found" | sort -u > "$work/merged"
        if cmp -s "$work/set" "$work/merged"; then break; fi
        mv "$work/merged" "$work/set"
        depth=$((depth + 1))
    done
    cat "$work/set"
}

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

    reachable "$sdk" > "$work/headers"

    # function-looking identifiers the stub declares
    syms=$(grep -ohE '\b(sce[A-Za-z0-9_]*|vgl[A-Za-z0-9_]*|gl[A-Z][A-Za-z0-9_]*)[[:space:]]*\(' \
           "$stub" | tr -d '( \t' | sort -u)

    for s in $syms; do
        checked=$((checked + 1))
        if ! grep -qE "\b$s\b" $(cat "$work/headers" | tr '\n' ' '); then
            echo "WRONG    $rel declares $s, but including it in the SDK does not"
            echo "         make $s visible - check which header really has it"
            fail=1
        fi
    done
done

if [ "$fail" = "0" ]; then
    echo "all $checked stub declarations are visible through the matching SDK header"
else
    echo "stub headers disagree with the SDK - make check cannot be trusted"
fi
exit $fail
