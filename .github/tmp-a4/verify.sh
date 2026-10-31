#!/bin/bash
# Temporary. Every set of properties under every preset for main, the release
# branch and this branch, one digest expected for each set.
file="$1"
addresses="$2"
bins="$3"

registered="IpRangeStart,IpRangeEnd,RegisteredCountry,RegisteredName,RegisteredOwner"
connection="Asn,AsnName,IsHosted,ConnectionType,Mcc,IsCellular,IsBroadband,IsVPN,IsProxy,IsTor,IsPublicRouter,HumanProbability,BrowserDiversity,HardwareDiversity,PlatformDiversity"
checkout="Country,CountryCode,State,Town,Latitude,Longitude,AccuracyRadiusMin,CurrencyCode,LanguageCode,TimeZoneIana,ConnectionType"
typical="Asn,AsnName,RegisteredName,RegisteredOwner,RegisteredCountry,IpRangeStart,IpRangeEnd,Country,CountryCode,State,Town,Latitude,Longitude,TimeZoneOffset,AccuracyRadiusMin,IsHosted,IsVPN,CountryCodesGeographical,CountryCodesPopulation,Mcc"
example="IpRangeStart,IpRangeEnd,AccuracyRadiusMin,RegisteredCountry,RegisteredName,Longitude,Latitude,Areas"

run() {
  local scenario="$1" build="$2" preset="$3" list="$4" options="$5"
  local output
  output=$("$bins/$build/balanced" "$file" "$addresses" "$preset" "$list" "$options" 2>&1)
  local code=$?
  echo "### $scenario $build $preset exit=$code"
  echo "$output" | cut -c1-600
  echo "$scenario $(echo "$output" | grep -oE 'digest=[0-9a-f]+' | head -1)" >> digests.txt
}

everyPreset() {
  local name="$1" list="$2"
  for build in main release branch; do
    run "$name" "$build" balanced "$list" ""
    run "$name" "$build" high "$list" ""
    run "$name" "$build" low "$list" ""
    run "$name" "$build" memory "$list" ""
  done
  # The two choices still to be made, on this branch only.
  if [ "$name" != "all" ]; then
    run "$name" memo balanced "$list" ""
    run "$name" nopart balanced "$list" ""
  fi
}

everyPreset registered "$registered"
everyPreset connection "$connection"
everyPreset checkout "$checkout"
everyPreset typical "$typical"
everyPreset example "$example"
everyPreset all -

# Naming every property in a list of names gives the same as naming nothing.
every="$("$bins/branch/balanced" names "$file" 2>/dev/null)"
if [ -n "$every" ]; then
  run all branch balanced "$every" ""
  # All but one property.
  run allButOne branch balanced "${every%,*}" ""
  run allButOne main balanced "${every%,*}" ""
fi

# The temporary file preset copies the data file, so only on the two sets.
for build in main branch; do
  run registered "$build" temp "$registered" ""
  run all "$build" temp - ""
done

echo
echo "Distinct digests for each set of properties, which should be one each:"
sort -u digests.txt | cut -d' ' -f1 | uniq -c
