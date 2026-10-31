#!/bin/bash
# Temporary. Answers the two choices that need measuring before the code is
# written: where profile offsets go in the Balanced preset, and whether the
# profile should be kept for the length of a lookup.
file="$1"
addresses="$2"
plain="$3"
memo="$4"

registered="IpRangeStart,IpRangeEnd,RegisteredCountry,RegisteredName,RegisteredOwner"
connection="Asn,AsnName,IsHosted,ConnectionType,Mcc,IsCellular,IsBroadband,IsVPN,IsProxy,IsTor,IsPublicRouter,HumanProbability,BrowserDiversity,HardwareDiversity,PlatformDiversity"
checkout="Country,CountryCode,State,Town,Latitude,Longitude,AccuracyRadiusMin,CurrencyCode,LanguageCode,TimeZoneIana,ConnectionType"
typical="Asn,AsnName,RegisteredName,RegisteredOwner,RegisteredCountry,IpRangeStart,IpRangeEnd,Country,CountryCode,State,Town,Latitude,Longitude,TimeZoneOffset,AccuracyRadiusMin,IsHosted,IsVPN,CountryCodesGeographical,CountryCodesPopulation,Mcc"
example="IpRangeStart,IpRangeEnd,AccuracyRadiusMin,RegisteredCountry,RegisteredName,Longitude,Latitude,Areas"

run() {
  local scenario="$1" option="$2" binary="$3" list="$4" options="$5"
  local output
  output=$("$binary" "$file" "$addresses" balanced "$list" "$options" 2>&1)
  local code=$?
  echo "### $scenario $option exit=$code"
  echo "$output" | cut -c1-500
  echo "$scenario $(echo "$output" | grep -oE 'digest=[0-9a-f]+' | head -1)" >> digests.txt
}

scenario() {
  local name="$1" list="$2"
  run "$name" today "$plain" "$list" ""
  run "$name" both "$plain" "$list" "types"
  run "$name" literal "$plain" "$list" "literal"
  run "$name" memo "$memo" "$list" ""
  run "$name" memo+both "$memo" "$list" "types"
}

if [ "$RUNNER_OS" = "Linux" ]; then
  for pair in "registered:$registered" "connection:$connection" "checkout:$checkout" "typical:$typical" "example:$example"; do
    echo "### gaps ${pair%%:*}"
    "$plain" gaps "$file" "${pair#*:}" 2>&1 | cut -c1-200
  done
fi

scenario all -
scenario typical "$typical"
scenario checkout "$checkout"
scenario registered "$registered"
scenario connection "$connection"

# The profile cache at three sizes, with the values held and both collections
# the preset fault touches in memory.
for capacity in 10000 100000 1000000; do
  run typical "cache$capacity" "$plain" "$typical" "types,heldvalues,cache=$capacity"
  run checkout "cache$capacity" "$plain" "$checkout" "types,heldvalues,cache=$capacity"
done

echo
echo "Distinct digests for each set of properties, which should be one each:"
sort -u digests.txt | cut -d' ' -f1 | uniq -c
