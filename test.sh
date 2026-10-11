#!/bin/sh
BIN="${BIN:-./shab}"
PASS=0
FAIL=0
FAILED=""

run_case() {
    name="$1"
    src="tests/${name}.ir"
    exp="tests/${name}.expected"
    [ -f "$src" ] || return
    [ -f "$exp" ] || return
    actual="$("$BIN" "$src" 2>&1 | grep -vE "^(Compile-time architecture:|Kernel system:|Kernel machine:|Execution complete:)")"
    expected="$(cat "$exp")"
    if [ "$expected" = "ERROR" ]; then
        case "$actual" in
            *[Ee]rror*|*expected*|*Line*|*FAIL*)
                PASS=$((PASS+1))
                printf "  PASS  %s\n" "$name"
                ;;
            *)
                FAIL=$((FAIL+1))
                FAILED="$FAILED $name"
                printf "  FAIL  %s (expected ERROR)\n" "$name"
                printf "        got: %s\n" "$actual"
                ;;
        esac
        return
    fi
    if [ "$actual" = "$expected" ]; then
        PASS=$((PASS+1))
        printf "  PASS  %s\n" "$name"
    else
        FAIL=$((FAIL+1))
        FAILED="$FAILED $name"
        printf "  FAIL  %s\n" "$name"
        printf "        expected: %s\n" "$expected"
        printf "        got:      %s\n" "$actual"
    fi
}

echo "shab-polyglot test suite"
echo "========================"
for f in tests/*.ir; do
    [ -e "$f" ] || { echo "No tests found"; exit 1; }
    run_case "$(basename "${f%.ir}")"
done

echo ""
echo "$PASS passed, $FAIL failed"
[ -n "$FAILED" ] && echo "Failed:$FAILED"
[ "$FAIL" -eq 0 ]
