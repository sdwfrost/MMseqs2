#!/bin/sh -e
fail() {
    echo "Error: $1"
    exit 1
}

notExists() {
	[ ! -f "$1" ]
}

# check amount of input variables
[ "$#" -ne 4 ] && echo "Please provide <queryDB> <targetDB> <outDB> <tmp>" && exit 1;
# check if files exists
[ ! -f "$1" ] &&  echo "$1 not found!" && exit 1;
[ ! -f "$2" ] &&  echo "$2 not found!" && exit 1;
[   -f "$3.dbtype" ] &&  echo "$3 exists already!" && exit 1;
[ ! -d "$4" ] &&  echo "tmp directory $4 not found!" && mkdir -p "$4";

INPUT="$1"
RESULTS="$3"
TMP_PATH="$4"

# call prefilter module
if notExists "${TMP_PATH}/pref.dbtype"; then
     # shellcheck disable=SC2086
    $RUNNER "$MMSEQS" prefilter "${INPUT}" "${2}" "${TMP_PATH}/pref" ${PREFILTER_PAR} \
        || fail "Prefilter died"
fi

if notExists "${TMP_PATH}/pref_swapped.dbtype"; then
     # shellcheck disable=SC2086
    "$MMSEQS" swapresults "${INPUT}" "${2}" "${TMP_PATH}/pref" "${TMP_PATH}/pref_swapped" ${SWAP_PAR} \
        || fail "Swapresults pref died"
fi

# call alignment module
if notExists "$TMP_PATH/aln_swapped.dbtype"; then
    # shellcheck disable=SC2086
    $RUNNER "$MMSEQS" "${ALIGN_MODULE}" "${2}" "${INPUT}" "${TMP_PATH}/pref_swapped" "${TMP_PATH}/aln_swapped" ${ALIGNMENT_PAR} \
        || fail "Alignment died"
fi

if notExists "${RESULTS}.dbtype"; then
    # shellcheck disable=SC2086
    "$MMSEQS" swapresults "${2}" "${INPUT}" "${TMP_PATH}/aln_swapped"  "${RESULTS}" ${SWAP_PAR} \
        || fail "Swapresults aln died"
fi


if [ -n "${REMOVE_TMP}" ]; then
    echo "Remove temporary files"
    "$MMSEQS" rmdb "${TMP_PATH}/pref"
    "$MMSEQS" rmdb "${TMP_PATH}/pref_swapped"
    "$MMSEQS" rmdb "${TMP_PATH}/aln_swapped"
    rm -f "${TMP_PATH}/searchtargetprofile.sh"
fi
