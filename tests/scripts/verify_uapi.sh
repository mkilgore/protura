#!/bin/bash
#
# Verifies the correctness of the uapi set of headers by verifying all of their dependencies exist in the uapi.
#
# Argument 1: gcc binary to use
# Argument 2: test results directory
# Argument n: Header directory

# IE. If a uapi header pulls in <protura/types.h>, then there must be a
# coresponding <uapi/protura/types.h> header or this test will fail, because if
# userspace attempted to include that header they would get an error that
# <protura/types.h> does not exist.

# The test is pretty simple, as `gcc` can do almost all of the heavy lifting
# for us. We pass it a header and it will attempt to parse it. Doing this while
# passing the correct flags forces it to check if all the dependency header
# files exist.

GCC=$1
TEST_PREFIX=$2
TOTAL_RESULT=0

PREFIX="uapi"

shift
shift

header_files=()
gcc_args="-nostdinc -fsyntax-only"

# Collect all of the header files in the provided directories
# Also create the gcc argument list that allows including headers from any of
# the provided directories
for var in "$@"
do
    readarray -d '' tmparray < <(find $var -type f -name "*.h" -print0)

    header_files+=( "${tmparray[@]}" )
    gcc_args+=" -isystem $var"
done

# Run each header through gcc, and verify it reports success
for f in "${header_files[@]}"
do
    TESTCASE=$f

    name=${f#./}
    name=$(echo "$name" | sed "s/\//_/g")
    GCC_LOG=$TEST_PREFIX/$name.log
    GCC_ERR_LOG=$TEST_PREFIX/$name.err.log

    gcc $gcc_args $f > $GCC_LOG 2> $GCC_ERR_LOG
    assert_success "Review gcc error below:" cat $GCC_ERR_LOG
done
