#!/bin/bash
#
# Verifies the correctness of the uapi set of headers by verifying all of their dependencies exist in the uapi.
#
# Argument 1: gcc binary to use
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
    if ! gcc $gcc_args $f > /dev/null; then
        echo "UAPI check FAILURE on header $f! Review gcc error above!"
        exit 1
    fi
done

