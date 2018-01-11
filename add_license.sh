add_lic() {
LICENSE='license_stub.txt'
START="START OF LICENSE STUB"
END="END OF LICENSE STUB"
for FILENAME in "$@"
do
    x=`grep "$START" $FILENAME`
    if [[ $x == "" ]];
    then
        cat $LICENSE $FILENAME > ${FILENAME}_with_license && mv ${FILENAME}_with_license $FILENAME
    else
        cat $LICENSE > ${FILENAME}_with_license && \
            awk "BEGIN {ENDLIC=0}/$END/{ENDLIC=1;next}/*\//{ flag=1;if (ENDLIC) {ENDLIC=0; next}; }flag" $FILENAME >> ${FILENAME}_with_license && \
            mv ${FILENAME}_with_license ${FILENAME}
    fi
done
}

add_lic src/runtime/*.[ch] src/global_controller/*.[ch] src/common/*.[ch] src/msus/*.[ch] src/msus/webserver/*.[ch] 
