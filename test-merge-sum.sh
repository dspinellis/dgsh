#!/bin/bash
#
# Tests for sgsh-merge-sum
#

# Shortcut
testcase()
{
	local name="$1"
	local expect="$2"
	shift 2
	if ! diff <(perl sgsh-merge-sum.pl "$@") $expect
	then
		echo 1>&2 "Test $name failed"
		exit 1
	else
		echo 1>&2 "Test $name OK"
	fi
}

testcase merge <(cat <<RESULT
1 a
4 c
2 d
8 z
RESULT
) <(cat <<EOF
1 a
2 d
EOF
) <(cat <<EOF
4 c
8 z
EOF
)

testcase sum <(cat <<RESULT
1 a
10 b
4 c
2 d
8 z
RESULT
) <(cat <<EOF
1 a
5 b
2 d
EOF
) <(cat <<EOF
5 b
4 c
8 z
EOF
)

# Leading white space
testcase leading <(cat <<RESULT
1 a
4 c
2 d
8 z
RESULT
) <(cat <<EOF
 1 a
2 d
EOF
) <(cat <<EOF
	4 c
8 z
EOF
)

# White space embedded in values
testcase embedded <(cat <<RESULT
1 a d f
4 c
2 d
8 z
RESULT
) <(cat <<EOF
 1 a d f
2	d
EOF
) <(cat <<EOF
	4 c
8 z
EOF
)

# Single file
testcase single <(cat <<RESULT
1 a
4 c
RESULT
) <(cat <<EOF
1 a
4 c
EOF
)

# Empty files
testcase empty <(cat <<RESULT
1 a
4 c
RESULT
) /dev/null <(cat <<EOF
1 a
4 c
EOF
) /dev/null

# Three files
testcase three <(cat <<RESULT
2 a
11 b
4 c
2 d
9 z
RESULT
) <(cat <<EOF
1 a
1 b
EOF
) <(cat <<EOF
1 a
5 b
2 d
1 z
EOF
) <(cat <<EOF
5 b
4 c
8 z
EOF
)
