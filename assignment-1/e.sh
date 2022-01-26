_V=$1
log() {
    if [ "$_V" = "-v" ]; then
        echo "$@"
    fi
}
export REQ_HEADERS="Accept,Host,X-Cloud-Trace-Context"
log "*** Required Headers : "$REQ_HEADERS
log "******"
log "*** GET from https://example.com ***"
curl https://example.com > example.html
log "*** CURL VERBOSE OUTPUT : http://ip.jsontest.com ***"
curl $_V http://ip.jsontest.com | jq .ip;curl --head http://ip.jsontest.com
log "******"
log "*** CURL VERBOSE OUTPUT : http://header.jsontest.com ***"
curl $_V http://header.jsontest.com | jq .\"$(echo $REQ_HEADERS | sed 's/,/\",.\"/g')\"
log "******"
> valid.txt
> invalid.txt
for file in JSONData/*.json; do
    log '*** RESPONSE FROM http://validate.jsontest.com FOR '$file' - '$(curl -sd "json=$(cat $file)" -X POST http://validate.jsontest.com|jq)
    var="invalid"
    [ "$(curl -sd "json=$(cat $file)" -X POST http://validate.jsontest.com|jq '.validate')" = "true" ] && var="valid"
    echo ${file##*/} >> $var.txt
done